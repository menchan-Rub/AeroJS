/*******************************************************************************
 * @file src/core/transformers/constant_folding.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2024 AeroJS Project
 * @brief ConstantFoldingTransformer の実装ファイル。
 *
 * @details このファイルは、JavaScript の AST に対する定数畳み込み最適化を実装しています。
 *          定数畳み込みは、コンパイル時に計算可能な式を事前に評価することで、
 *          実行時のパフォーマンスを向上させます。
 *
 * @license
 * Copyright (c) 2024 AeroJS Project
 * このソースコードは MIT ライセンスの下で提供されています。
 ******************************************************************************/

#include "src/core/transformers/constant_folding.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "src/core/parser/ast/ast_node_factory.h"
#include "src/core/parser/ast/ast_node_types.h"
#include "src/core/parser/ast/nodes/all_nodes.h"
#include "src/core/common/logger.h"

namespace aerojs::core::transformers {

namespace {

using aerojs::parser::ast::BinaryOperator;
using aerojs::parser::ast::LiteralType;
using aerojs::parser::ast::NodePtr;
using aerojs::parser::ast::UnaryOperator;

struct BinaryOperationCacheKey {
  BinaryOperator m_op;
  double m_left;
  double m_right;

  bool operator==(const BinaryOperationCacheKey& other) const {
    return m_op == other.m_op && m_left == other.m_left && m_right == other.m_right;
  }
};

struct BinaryOperationCacheKeyHash {
  std::size_t operator()(const BinaryOperationCacheKey& key) const {
    std::size_t h1 = std::hash<int>{}(static_cast<int>(key.m_op));
    std::size_t h2 = std::hash<double>{}(key.m_left);
    std::size_t h3 = std::hash<double>{}(key.m_right);
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

struct UnaryOperationCacheKey {
  UnaryOperator m_op;
  std::variant<double, bool, std::string> m_operandValue;

  bool operator==(const UnaryOperationCacheKey& other) const {
    return m_op == other.m_op && m_operandValue == other.m_operandValue;
  }
};

struct UnaryOperationCacheKeyHash {
  std::size_t operator()(const UnaryOperationCacheKey& key) const {
    std::size_t h1 = std::hash<int>{}(static_cast<int>(key.m_op));
    std::size_t h2 = std::hash<std::variant<double, bool, std::string>>{}(key.m_operandValue);
    return h1 ^ (h2 << 1);
  }
};

class ConstantFoldingCache {
 public:
  std::unordered_map<BinaryOperationCacheKey, NodePtr, BinaryOperationCacheKeyHash> m_binaryCache;
  std::unordered_map<UnaryOperationCacheKey, NodePtr, UnaryOperationCacheKeyHash> m_unaryCache;

  void Clear() {
    m_binaryCache.clear();
    m_unaryCache.clear();
  }
};

ConstantFoldingCache& GetCacheInstance() {
  thread_local ConstantFoldingCache instance;
  return instance;
}

const std::unordered_set<std::string> S_PURE_MATH_FUNCTIONS = {
    "abs", "acos", "acosh", "asin", "asinh", "atan", "atanh",
    "atan2", "cbrt", "ceil", "clz32", "cos", "cosh", "exp",
    "expm1", "floor", "fround", "hypot", "imul", "log", "log1p",
    "log10", "log2", "max", "min", "pow", "round", "sign",
    "sin", "sinh", "sqrt", "tan", "tanh", "trunc"};

const std::unordered_set<std::string> S_PURE_NUMBER_FUNCTIONS = {
    "isFinite", "isInteger", "isNaN", "isSafeInteger"
};

const std::unordered_set<std::string> S_PURE_STRING_FUNCTIONS = {
    "fromCharCode", "fromCodePoint"};

const std::unordered_map<std::string, double> S_CONSTANT_PROPERTIES = {
    {"Math.E", M_E},
    {"Math.LN10", M_LN10},
    {"Math.LN2", M_LN2},
    {"Math.LOG10E", M_LOG10E},
    {"Math.LOG2E", M_LOG2E},
    {"Math.PI", M_PI},
    {"Math.SQRT1_2", M_SQRT1_2},
    {"Math.SQRT2", M_SQRT2},
    {"Number.EPSILON", std::numeric_limits<double>::epsilon()},
    {"Number.MAX_SAFE_INTEGER", static_cast<double>(9007199254740991LL)},
    {"Number.MAX_VALUE", std::numeric_limits<double>::max()},
    {"Number.MIN_SAFE_INTEGER", static_cast<double>(-9007199254740991LL)},
    {"Number.MIN_VALUE", std::numeric_limits<double>::min()},
    {"Number.NaN", std::numeric_limits<double>::quiet_NaN()},
    {"Number.NEGATIVE_INFINITY", -std::numeric_limits<double>::infinity()},
    {"Number.POSITIVE_INFINITY", std::numeric_limits<double>::infinity()}};

// 値が真偽値として「真」と評価されるかを判定する
bool IsTruthy(const parser::ast::Literal& literal) {
  switch (literal.GetLiteralType()) {
    case LiteralType::BOOLEAN:
      return literal.GetBooleanValue();
    case LiteralType::NUMBER:
      return literal.GetNumberValue() != 0.0 && !std::isnan(literal.GetNumberValue());
    case LiteralType::STRING:
      return !literal.GetStringValue().empty();
    case LiteralType::NULL_TYPE:
      return false;
    case LiteralType::UNDEFINED:
      return false;
    default:
      return true;
  }
}

// ノードが定数かどうかを判定する
bool IsConstant(const parser::ast::NodePtr& node) {
  if (!node) return false;
  
  return node->GetType() == parser::ast::NodeType::LITERAL;
}

// 二項演算の評価を行う
parser::ast::NodePtr EvaluateBinaryOperation(
    BinaryOperator op,
    const parser::ast::Literal& left,
    const parser::ast::Literal& right) {
  
  // 数値演算の場合
  if (left.GetLiteralType() == LiteralType::NUMBER && 
      right.GetLiteralType() == LiteralType::NUMBER) {
    
    double leftVal = left.GetNumberValue();
    double rightVal = right.GetNumberValue();
    
    // キャッシュをチェック
    BinaryOperationCacheKey key{op, leftVal, rightVal};
    auto& cache = GetCacheInstance().m_binaryCache;
    auto it = cache.find(key);
    if (it != cache.end()) {
      return it->second;
    }
    
    double result = 0.0;
    bool validOperation = true;
    
    switch (op) {
      case BinaryOperator::ADDITION:
        result = leftVal + rightVal;
        break;
      case BinaryOperator::SUBTRACTION:
        result = leftVal - rightVal;
        break;
      case BinaryOperator::MULTIPLICATION:
        result = leftVal * rightVal;
        break;
      case BinaryOperator::DIVISION:
        if (rightVal == 0.0) {
          result = leftVal > 0 ? std::numeric_limits<double>::infinity() :
                  leftVal < 0 ? -std::numeric_limits<double>::infinity() :
                  std::numeric_limits<double>::quiet_NaN();
        } else {
          result = leftVal / rightVal;
        }
        break;
      case BinaryOperator::REMAINDER:
        if (rightVal == 0.0) {
          result = std::numeric_limits<double>::quiet_NaN();
        } else {
          result = std::fmod(leftVal, rightVal);
        }
        break;
      case BinaryOperator::EXPONENTIATION:
        result = std::pow(leftVal, rightVal);
        break;
      case BinaryOperator::BITWISE_AND:
        result = static_cast<double>(static_cast<int32_t>(leftVal) & static_cast<int32_t>(rightVal));
        break;
      case BinaryOperator::BITWISE_OR:
        result = static_cast<double>(static_cast<int32_t>(leftVal) | static_cast<int32_t>(rightVal));
        break;
      case BinaryOperator::BITWISE_XOR:
        result = static_cast<double>(static_cast<int32_t>(leftVal) ^ static_cast<int32_t>(rightVal));
        break;
      case BinaryOperator::LEFT_SHIFT:
        result = static_cast<double>(static_cast<int32_t>(leftVal) << (static_cast<int32_t>(rightVal) & 0x1F));
        break;
      case BinaryOperator::RIGHT_SHIFT:
        result = static_cast<double>(static_cast<int32_t>(leftVal) >> (static_cast<int32_t>(rightVal) & 0x1F));
        break;
      case BinaryOperator::UNSIGNED_RIGHT_SHIFT:
        result = static_cast<double>(static_cast<uint32_t>(static_cast<int32_t>(leftVal)) >> (static_cast<int32_t>(rightVal) & 0x1F));
        break;
      case BinaryOperator::EQUAL:
        result = leftVal == rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::NOT_EQUAL:
        result = leftVal != rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::STRICT_EQUAL:
        result = leftVal == rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::STRICT_NOT_EQUAL:
        result = leftVal != rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::LESS_THAN:
        result = leftVal < rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::LESS_THAN_OR_EQUAL:
        result = leftVal <= rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::GREATER_THAN:
        result = leftVal > rightVal ? 1.0 : 0.0;
        break;
      case BinaryOperator::GREATER_THAN_OR_EQUAL:
        result = leftVal >= rightVal ? 1.0 : 0.0;
        break;
      default:
        validOperation = false;
        break;
    }
    
    if (validOperation) {
      auto resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(result);
      cache[key] = resultNode;
      return resultNode;
    }
  }
  
  // 文字列連結の場合
  if (op == BinaryOperator::ADDITION &&
      (left.GetLiteralType() == LiteralType::STRING || right.GetLiteralType() == LiteralType::STRING)) {
    
    std::string leftStr = left.GetLiteralType() == LiteralType::STRING ? 
                          left.GetStringValue() : 
                          left.ToString();
    
    std::string rightStr = right.GetLiteralType() == LiteralType::STRING ? 
                           right.GetStringValue() : 
                           right.ToString();
    
    return parser::ast::ASTNodeFactory::CreateStringLiteral(leftStr + rightStr);
  }
  
  // 比較演算子の場合
  if ((op == BinaryOperator::EQUAL || op == BinaryOperator::NOT_EQUAL ||
       op == BinaryOperator::STRICT_EQUAL || op == BinaryOperator::STRICT_NOT_EQUAL) &&
      (left.GetLiteralType() != LiteralType::NUMBER || right.GetLiteralType() != LiteralType::NUMBER)) {
    
    bool result = false;
    
    if (op == BinaryOperator::STRICT_EQUAL || op == BinaryOperator::STRICT_NOT_EQUAL) {
      // 厳密等価の場合は型も一致する必要がある
      if (left.GetLiteralType() != right.GetLiteralType()) {
        result = false;
      } else {
        switch (left.GetLiteralType()) {
          case LiteralType::BOOLEAN:
            result = left.GetBooleanValue() == right.GetBooleanValue();
            break;
          case LiteralType::STRING:
            result = left.GetStringValue() == right.GetStringValue();
            break;
          case LiteralType::NULL_TYPE:
          case LiteralType::UNDEFINED:
            result = true; // 同じ型なので等しい
            break;
          default:
            return nullptr; // 評価できない
        }
      }
      
      if (op == BinaryOperator::STRICT_NOT_EQUAL) {
        result = !result;
      }
    } else {
      // 非厳密等価の場合
      // JavaScript の型変換ルールに従って比較
      if (left.GetLiteralType() == LiteralType::NULL_TYPE && right.GetLiteralType() == LiteralType::UNDEFINED ||
          left.GetLiteralType() == LiteralType::UNDEFINED && right.GetLiteralType() == LiteralType::NULL_TYPE) {
        result = true;
      } else if (left.GetLiteralType() == LiteralType::BOOLEAN || right.GetLiteralType() == LiteralType::BOOLEAN) {
        // 真偽値を数値に変換して比較
        double leftNum = left.GetLiteralType() == LiteralType::BOOLEAN ? 
                         (left.GetBooleanValue() ? 1.0 : 0.0) : 
                         left.GetNumberValue();
        
        double rightNum = right.GetLiteralType() == LiteralType::BOOLEAN ? 
                          (right.GetBooleanValue() ? 1.0 : 0.0) : 
                          right.GetNumberValue();
        
        result = leftNum == rightNum;
      } else if (left.GetLiteralType() == LiteralType::STRING && right.GetLiteralType() == LiteralType::STRING) {
        result = left.GetStringValue() == right.GetStringValue();
      } else {
        return nullptr; // 評価できない
      }
      
      if (op == BinaryOperator::NOT_EQUAL) {
        result = !result;
      }
    }
    
    return parser::ast::ASTNodeFactory::CreateBooleanLiteral(result);
  }
  
  return nullptr; // 評価できない場合
}

// 単項演算の評価を行う
parser::ast::NodePtr EvaluateUnaryOperation(
    UnaryOperator op,
    const parser::ast::Literal& arg) {
  
  // キャッシュキーを作成
  UnaryOperationCacheKey key;
  key.m_op = op;
  
  // オペランドの値をキャッシュキーに設定
  switch (arg.GetLiteralType()) {
    case LiteralType::NUMBER:
      key.m_operandValue = arg.GetNumberValue();
      break;
    case LiteralType::BOOLEAN:
      key.m_operandValue = arg.GetBooleanValue();
      break;
    case LiteralType::STRING:
      key.m_operandValue = arg.GetStringValue();
      break;
    default:
      return nullptr; // キャッシュ対象外
  }
  
  // キャッシュをチェック
  auto& cache = GetCacheInstance().m_unaryCache;
  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }
  
  parser::ast::NodePtr resultNode = nullptr;
  
  switch (op) {
    case UnaryOperator::UNARY_PLUS:
      if (arg.GetLiteralType() == LiteralType::NUMBER) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(arg.GetNumberValue());
      } else if (arg.GetLiteralType() == LiteralType::BOOLEAN) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(arg.GetBooleanValue() ? 1.0 : 0.0);
      } else if (arg.GetLiteralType() == LiteralType::STRING) {
        try {
          double value = std::stod(arg.GetStringValue());
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(value);
        } catch (...) {
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(std::numeric_limits<double>::quiet_NaN());
        }
      }
      break;
      
    case UnaryOperator::UNARY_NEGATION:
      if (arg.GetLiteralType() == LiteralType::NUMBER) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(-arg.GetNumberValue());
      } else if (arg.GetLiteralType() == LiteralType::BOOLEAN) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(arg.GetBooleanValue() ? -1.0 : -0.0);
      } else if (arg.GetLiteralType() == LiteralType::STRING) {
        try {
          double value = std::stod(arg.GetStringValue());
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(-value);
        } catch (...) {
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(std::numeric_limits<double>::quiet_NaN());
        }
      }
      break;
      
    case UnaryOperator::LOGICAL_NOT:
      resultNode = parser::ast::ASTNodeFactory::CreateBooleanLiteral(!IsTruthy(arg));
      break;
      
    case UnaryOperator::BITWISE_NOT:
      if (arg.GetLiteralType() == LiteralType::NUMBER) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(
            static_cast<double>(~static_cast<int32_t>(arg.GetNumberValue())));
      } else if (arg.GetLiteralType() == LiteralType::BOOLEAN) {
        resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(
            static_cast<double>(~static_cast<int32_t>(arg.GetBooleanValue() ? 1 : 0)));
      } else if (arg.GetLiteralType() == LiteralType::STRING) {
        try {
          double value = std::stod(arg.GetStringValue());
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(
              static_cast<double>(~static_cast<int32_t>(value)));
        } catch (...) {
          resultNode = parser::ast::ASTNodeFactory::CreateNumberLiteral(
              static_cast<double>(~static_cast<int32_t>(0)));
        }
      }
      break;
      
    case UnaryOperator::TYPEOF:
      switch (arg.GetLiteralType()) {
        case LiteralType::NUMBER:
          resultNode = parser::ast::ASTNodeFactory::CreateStringLiteral("number");
          break;
        case LiteralType::BOOLEAN:
          resultNode = parser::ast::ASTNodeFactory::CreateStringLiteral("boolean");
          break;
        case LiteralType::STRING:
          resultNode = parser::ast::ASTNodeFactory::CreateStringLiteral("string");
          break;
        case LiteralType::NULL_TYPE:
          resultNode = parser::ast::ASTNodeFactory::CreateStringLiteral("object");
          break;
        case LiteralType::UNDEFINED:
          resultNode = parser::ast::ASTNodeFactory::CreateStringLiteral("undefined");
          break;
        default:
          break;
      }
      break;
      
    default:
      break;
  }
  
  if (resultNode) {
    cache[key] = resultNode;
  }
  
  return resultNode;
}

// 論理演算の評価を行う
parser::ast::NodePtr EvaluateLogicalOperation(
    parser::ast::LogicalOperator op,
    const parser::ast::Literal& left,
    const parser::ast::Literal& right) {
  
  bool leftTruthy = IsTruthy(left);
  
  switch (op) {
    case parser::ast::LogicalOperator::LOGICAL_AND:
      return leftTruthy ? parser::ast::ASTNodeFactory::CreateLiteralCopy(right) : 
                          parser::ast::ASTNodeFactory::CreateLiteralCopy(left);
      
    case parser::ast::LogicalOperator::LOGICAL_OR:
      return leftTruthy ? parser::ast::ASTNodeFactory::CreateLiteralCopy(left) : 
                          parser::ast::ASTNodeFactory::CreateLiteralCopy(right);
      
    case parser::ast::LogicalOperator::NULLISH_COALESCING:
      return (left.GetLiteralType() == LiteralType::NULL_TYPE || 
              left.GetLiteralType() == LiteralType::UNDEFINED) ? 
              parser::ast::ASTNodeFactory::CreateLiteralCopy(right) : 
              parser::ast::ASTNodeFactory::CreateLiteralCopy(left);
      
    default:
      return nullptr;
  }
}

// 条件式の評価を行う
parser::ast::NodePtr EvaluateConditionalExpression(
    const parser::ast::Literal& test,
    const parser::ast::NodePtr& consequent,
    const parser::ast::NodePtr& alternate) {
  
  bool testTruthy = IsTruthy(test);
  
  if (testTruthy) {
    return consequent;
  } else {
    return alternate;
  }
}

// Math関数の評価を行う
parser::ast::NodePtr EvaluateMathFunction(
    const std::string& functionName,
    const std::vector<parser::ast::NodePtr>& args) {
  
  // 引数がすべて数値リテラルであることを確認
  std::vector<double> numArgs;
  for (const auto& arg : args) {
    auto literal = std::dynamic_pointer_cast<parser::ast::Literal>(arg);
    if (!literal || literal->GetLiteralType() != LiteralType::NUMBER) {
      return nullptr;
    }
    numArgs.push_back(literal->GetNumberValue());
  }
  
  double result = 0.0;
  bool validOperation = true;
  
  if (functionName == "abs") {
    if (numArgs.size() == 1) result = std::abs(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "acos") {
    if (numArgs.size() == 1) result = std::acos(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "acosh") {
    if (numArgs.size() == 1) result = std::acosh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "asin") {
    if (numArgs.size() == 1) result = std::asin(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "asinh") {
    if (numArgs.size() == 1) result = std::asinh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "atan") {
    if (numArgs.size() == 1) result = std::atan(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "atanh") {
    if (numArgs.size() == 1) result = std::atanh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "atan2") {
    if (numArgs.size() == 2) result = std::atan2(numArgs[0], numArgs[1]);
    else validOperation = false;
  } else if (functionName == "cbrt") {
    if (numArgs.size() == 1) result = std::cbrt(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "ceil") {
    if (numArgs.size() == 1) result = std::ceil(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "cos") {
    if (numArgs.size() == 1) result = std::cos(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "cosh") {
    if (numArgs.size() == 1) result = std::cosh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "exp") {
    if (numArgs.size() == 1) result = std::exp(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "expm1") {
    if (numArgs.size() == 1) result = std::expm1(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "floor") {
    if (numArgs.size() == 1) result = std::floor(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "hypot") {
    if (!numArgs.empty()) {
      result = 0.0;
      for (double arg : numArgs) {
        result += arg * arg;
      }
      result = std::sqrt(result);
    } else {
      validOperation = false;
    }
  } else if (functionName == "log") {
    if (numArgs.size() == 1) result = std::log(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "log10") {
    if (numArgs.size() == 1) result = std::log10(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "log1p") {
    if (numArgs.size() == 1) result = std::log1p(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "log2") {
    if (numArgs.size() == 1) result = std::log2(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "max") {
    if (!numArgs.empty()) {
      result = *std::max_element(numArgs.begin(), numArgs.end());
    } else {
      result = -std::numeric_limits<double>::infinity();
    }
  } else if (functionName == "min") {
    if (!numArgs.empty()) {
      result = *std::min_element(numArgs.begin(), numArgs.end());
    } else {
      result = std::numeric_limits<double>::infinity();
    }
  } else if (functionName == "pow") {
    if (numArgs.size() == 2) result = std::pow(numArgs[0], numArgs[1]);
    else validOperation = false;
  } else if (functionName == "round") {
    if (numArgs.size() == 1) result = std::round(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "sign") {
    if (numArgs.size() == 1) {
      if (numArgs[0] > 0) result = 1;
      else if (numArgs[0] < 0) result = -1;
      else result = numArgs[0]; // 保存 +0, -0, NaN
    } else {
      validOperation = false;
    }
  } else if (functionName == "sin") {
    if (numArgs.size() == 1) result = std::sin(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "sinh") {
    if (numArgs.size() == 1) result = std::sinh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "sqrt") {
    if (numArgs.size() == 1) result = std::sqrt(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "tan") {
    if (numArgs.size() == 1) result = std::tan(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "tanh") {
    if (numArgs.size() == 1) result = std::tanh(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "trunc") {
    if (numArgs.size() == 1) result = std::trunc(numArgs[0]);
    else validOperation = false;
  } else if (functionName == "clz32") {
    if (numArgs.size() == 1) {
      uint32_t n = static_cast<uint32_t>(numArgs[0]);
      if (n == 0) {
        result = 32;
      } else {
        result = __builtin_clz(n);
      }
    } else {
      validOperation = false;
    }
  } else if (functionName == "imul") {
    if (numArgs.size() == 2) {
size_t ConstantFoldingTransformer::GetFoldedExpressionsCount() const noexcept {
  return m_foldedExpressions;
}

size_t ConstantFoldingTransformer::GetVisitedNodesCount() const noexcept {
  return m_visitedNodes;
}
parser::ast::NodePtr ConstantFoldingTransformer::Transform(parser::ast::NodePtr node) {
  // VisitNodeを呼び出して変換処理を実行
  return VisitNode(std::move(node));
}

parser::ast::NodePtr ConstantFoldingTransformer::VisitNode(parser::ast::NodePtr node) {
  if (!node) {
    return nullptr;
  }

  // 統計情報の収集
  if (m_statisticsEnabled) {
    m_visitedNodes++;
  }

  // 子ノードを再帰的に処理
  if (auto binaryExpr = std::dynamic_pointer_cast<parser::ast::BinaryExpression>(node)) {
    binaryExpr->SetLeft(VisitNode(binaryExpr->GetLeft()));
    binaryExpr->SetRight(VisitNode(binaryExpr->GetRight()));
  } else if (auto unaryExpr = std::dynamic_pointer_cast<parser::ast::UnaryExpression>(node)) {
    unaryExpr->SetArgument(VisitNode(unaryExpr->GetArgument()));
  } else if (auto logicalExpr = std::dynamic_pointer_cast<parser::ast::LogicalExpression>(node)) {
    logicalExpr->SetLeft(VisitNode(logicalExpr->GetLeft()));
    logicalExpr->SetRight(VisitNode(logicalExpr->GetRight()));
  } else if (auto condExpr = std::dynamic_pointer_cast<parser::ast::ConditionalExpression>(node)) {
    condExpr->SetTest(VisitNode(condExpr->GetTest()));
    condExpr->SetConsequent(VisitNode(condExpr->GetConsequent()));
    condExpr->SetAlternate(VisitNode(condExpr->GetAlternate()));
  } else if (auto callExpr = std::dynamic_pointer_cast<parser::ast::CallExpression>(node)) {
    callExpr->SetCallee(VisitNode(callExpr->GetCallee()));
    auto& args = callExpr->GetArguments();
    for (size_t i = 0; i < args.size(); ++i) {
      args[i] = VisitNode(args[i]);
    }
  } else if (auto memberExpr = std::dynamic_pointer_cast<parser::ast::MemberExpression>(node)) {
    memberExpr->SetObject(VisitNode(memberExpr->GetObject()));
    if (!memberExpr->IsComputed()) {
      memberExpr->SetProperty(VisitNode(memberExpr->GetProperty()));
    }
  } else if (auto arrayExpr = std::dynamic_pointer_cast<parser::ast::ArrayExpression>(node)) {
    auto& elements = arrayExpr->GetElements();
    for (size_t i = 0; i < elements.size(); ++i) {
      if (elements[i]) {  // スパース配列の場合、nullの要素がある
        elements[i] = VisitNode(elements[i]);
      }
    }
  } else if (auto objExpr = std::dynamic_pointer_cast<parser::ast::ObjectExpression>(node)) {
    auto& properties = objExpr->GetProperties();
    for (auto& prop : properties) {
      if (prop->GetType() == parser::ast::NodeType::PROPERTY) {
        auto property = std::static_pointer_cast<parser::ast::Property>(prop);
        property->SetValue(VisitNode(property->GetValue()));
        if (property->IsComputed()) {
          property->SetKey(VisitNode(property->GetKey()));
        }
      }
    }
  } else if (auto blockStmt = std::dynamic_pointer_cast<parser::ast::BlockStatement>(node)) {
    auto& body = blockStmt->GetBody();
    for (size_t i = 0; i < body.size(); ++i) {
      body[i] = VisitNode(body[i]);
    }
  } else if (auto ifStmt = std::dynamic_pointer_cast<parser::ast::IfStatement>(node)) {
    ifStmt->SetTest(VisitNode(ifStmt->GetTest()));
    ifStmt->SetConsequent(VisitNode(ifStmt->GetConsequent()));
    if (ifStmt->GetAlternate()) {
      ifStmt->SetAlternate(VisitNode(ifStmt->GetAlternate()));
    }
  } else if (auto program = std::dynamic_pointer_cast<parser::ast::Program>(node)) {
    auto& body = program->GetBody();
    for (size_t i = 0; i < body.size(); ++i) {
      body[i] = VisitNode(body[i]);
    }
  }

  // 現在のノードに対する畳み込み処理
  parser::ast::NodePtr foldedNode = nullptr;

  switch (node->GetType()) {
    case parser::ast::NodeType::BINARY_EXPRESSION:
      foldedNode = FoldBinaryExpression(std::static_pointer_cast<parser::ast::BinaryExpression>(node));
      break;
    case parser::ast::NodeType::UNARY_EXPRESSION:
      foldedNode = FoldUnaryExpression(std::static_pointer_cast<parser::ast::UnaryExpression>(node));
      break;
    case parser::ast::NodeType::LOGICAL_EXPRESSION:
      foldedNode = FoldLogicalExpression(std::static_pointer_cast<parser::ast::LogicalExpression>(node));
      break;
    case parser::ast::NodeType::CONDITIONAL_EXPRESSION:
      foldedNode = FoldConditionalExpression(std::static_pointer_cast<parser::ast::ConditionalExpression>(node));
      break;
    case parser::ast::NodeType::CALL_EXPRESSION:
      foldedNode = FoldCallExpression(std::static_pointer_cast<parser::ast::CallExpression>(node));
      break;
    case parser::ast::NodeType::MEMBER_EXPRESSION:
      foldedNode = FoldMemberExpression(std::static_pointer_cast<parser::ast::MemberExpression>(node));
      break;
    default:
      break;
  }

  // 畳み込みが行われたかチェック
  if (foldedNode && foldedNode != node) {
    if (m_statisticsEnabled) {
      m_foldedExpressions++;
    }
    return foldedNode;
  } else {
    return node;
  }
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldBinaryExpression(const std::shared_ptr<parser::ast::BinaryExpression>& expr) {
  // オペランドが定数かチェック
  if (!IsConstant(expr->GetLeft()) || !IsConstant(expr->GetRight())) {
    return expr;
  }

  // 定数リテラルを取得
  auto leftLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(expr->GetLeft());
  auto rightLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(expr->GetRight());

  if (!leftLiteral || !rightLiteral) {
    return expr;
  }

  // 演算を評価
  parser::ast::NodePtr resultNode = EvaluateBinaryOperation(expr->GetOperator(), *leftLiteral, *rightLiteral);

  return resultNode ? resultNode : expr;
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldUnaryExpression(const std::shared_ptr<parser::ast::UnaryExpression>& expr) {
  // オペランドが定数かチェック
  if (!IsConstant(expr->GetArgument())) {
    return expr;
  }

  auto argLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(expr->GetArgument());
  if (!argLiteral) {
    return expr;
  }

  // 演算を評価
  parser::ast::NodePtr resultNode = EvaluateUnaryOperation(expr->GetOperator(), *argLiteral);

  return resultNode ? resultNode : expr;
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldLogicalExpression(const std::shared_ptr<parser::ast::LogicalExpression>& expr) {
  auto leftOperand = expr->GetLeft();
  auto op = expr->GetOperator();

  // 左オペランドが定数かチェック
  if (IsConstant(leftOperand)) {
    auto leftLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(leftOperand);
    if (!leftLiteral) {
      return expr;
    }

    bool leftIsTruthy = IsTruthy(*leftLiteral);

    // ショートサーキット評価
    if (op == parser::ast::LogicalOperator::LOGICAL_AND && !leftIsTruthy) {
      if (m_statisticsEnabled) {
        m_foldedExpressions++;
      }
      return leftOperand;
    }
    if (op == parser::ast::LogicalOperator::LOGICAL_OR && leftIsTruthy) {
      if (m_statisticsEnabled) {
        m_foldedExpressions++;
      }
      return leftOperand;
    }

    // 右オペランドの評価
    auto rightOperand = expr->GetRight();
    if (IsConstant(rightOperand)) {
      auto rightLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(rightOperand);
      if (!rightLiteral) {
        return expr;
      }
      parser::ast::NodePtr resultNode = EvaluateLogicalOperation(op, *leftLiteral, *rightLiteral);
      return resultNode ? resultNode : expr;
    } else {
      if ((op == parser::ast::LogicalOperator::LOGICAL_AND && leftIsTruthy) ||
          (op == parser::ast::LogicalOperator::LOGICAL_OR && !leftIsTruthy)) {
        if (m_statisticsEnabled) {
          m_foldedExpressions++;
        }
        return rightOperand;
      }
    }
  }

  return expr;
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldConditionalExpression(const std::shared_ptr<parser::ast::ConditionalExpression>& expr) {
  auto testExpr = expr->GetTest();
  
  // 条件式が定数かチェック
  if (IsConstant(testExpr)) {
    auto testLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(testExpr);
    if (!testLiteral) {
      return expr;
    }
    
    bool testIsTruthy = IsTruthy(*testLiteral);
    
    // 条件に基づいて結果を選択
    if (testIsTruthy) {
      if (m_statisticsEnabled) {
        m_foldedExpressions++;
      }
      return expr->GetConsequent();
    } else {
      if (m_statisticsEnabled) {
        m_foldedExpressions++;
      }
      return expr->GetAlternate();
    }
  }
  
  return expr;
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldCallExpression(const std::shared_ptr<parser::ast::CallExpression>& expr) {
  auto callee = expr->GetCallee();
  
  // Math関数の定数畳み込み
  if (auto memberExpr = std::dynamic_pointer_cast<parser::ast::MemberExpression>(callee)) {
    auto object = memberExpr->GetObject();
    auto property = memberExpr->GetProperty();
    
    // Math.関数の呼び出しかチェック
    if (auto identifier = std::dynamic_pointer_cast<parser::ast::Identifier>(object)) {
      if (identifier->GetName() == "Math") {
        if (auto propIdentifier = std::dynamic_pointer_cast<parser::ast::Identifier>(property)) {
          // 引数が全て定数かチェック
          const auto& args = expr->GetArguments();
          bool allArgsConstant = true;
          std::vector<double> numArgs;
          
          for (const auto& arg : args) {
            if (!IsConstant(arg)) {
              allArgsConstant = false;
              break;
            }
            
            auto literal = std::dynamic_pointer_cast<parser::ast::Literal>(arg);
            if (!literal || !IsNumber(*literal)) {
              allArgsConstant = false;
              break;
            }
            
            numArgs.push_back(GetNumberValue(*literal));
          }
          
          if (allArgsConstant) {
            parser::ast::NodePtr resultNode = EvaluateMathFunction(propIdentifier->GetName(), numArgs);
            if (resultNode) {
              if (m_statisticsEnabled) {
                m_foldedExpressions++;
              }
              return resultNode;
            }
          }
        }
      }
    }
  }
  
  return expr;
}

parser::ast::NodePtr ConstantFoldingTransformer::FoldMemberExpression(const std::shared_ptr<parser::ast::MemberExpression>& expr) {
  auto object = expr->GetObject();
  
  // 配列やオブジェクトリテラルのプロパティアクセスを畳み込む
  if (expr->IsComputed() && IsConstant(expr->GetProperty())) {
    if (auto arrayExpr = std::dynamic_pointer_cast<parser::ast::ArrayExpression>(object)) {
      auto property = std::dynamic_pointer_cast<parser::ast::Literal>(expr->GetProperty());
      if (property && IsNumber(*property)) {
        int index = static_cast<int>(GetNumberValue(*property));
        const auto& elements = arrayExpr->GetElements();
        
        if (index >= 0 && index < static_cast<int>(elements.size()) && elements[index] && IsConstant(elements[index])) {
          if (m_statisticsEnabled) {
            m_foldedExpressions++;
          }
          return elements[index];
        }
      }
    } else if (auto objExpr = std::dynamic_pointer_cast<parser::ast::ObjectExpression>(object)) {
      auto property = std::dynamic_pointer_cast<parser::ast::Literal>(expr->GetProperty());
      if (property) {
        std::string key;
        if (IsString(*property)) {
          key = GetStringValue(*property);
        } else if (IsNumber(*property)) {
          key = std::to_string(static_cast<int>(GetNumberValue(*property)));
        }
        
        if (!key.empty()) {
          const auto& properties = objExpr->GetProperties();
          for (const auto& prop : properties) {
            if (auto objProp = std::dynamic_pointer_cast<parser::ast::Property>(prop)) {
              if (!objProp->IsComputed()) {
                if (auto keyIdentifier = std::dynamic_pointer_cast<parser::ast::Identifier>(objProp->GetKey())) {
                  if (keyIdentifier->GetName() == key && IsConstant(objProp->GetValue())) {
                    if (m_statisticsEnabled) {
                      m_foldedExpressions++;
                    }
                    return objProp->GetValue();
                  }
                } else if (auto keyLiteral = std::dynamic_pointer_cast<parser::ast::Literal>(objProp->GetKey())) {
                  std::string propKey;
                  if (IsString(*keyLiteral)) {
                    propKey = GetStringValue(*keyLiteral);
                  } else if (IsNumber(*keyLiteral)) {
                    propKey = std::to_string(static_cast<int>(GetNumberValue(*keyLiteral)));
                  }
                  
                  if (propKey == key && IsConstant(objProp->GetValue())) {
                    if (m_statisticsEnabled) {
                      m_foldedExpressions++;
                    }
                    return objProp->GetValue();
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  
  return expr;
}

}  // namespace aerojs::core::transformers