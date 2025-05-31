/**
 * @file riscv_array_optimizations.cpp
 * @version 1.0.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief RISC-Vアーキテクチャ向けの配列最適化ユーティリティの実装
 */

#include "riscv_array_optimizations.h"

#include <algorithm>
#include <string_view>

#include "src/core/parser/ast/nodes/all_nodes.h"
#include "src/core/parser/ast/ast_node_factory.h"
#include "src/utils/platform/cpu_features.h"

#if defined(__riscv) || defined(__riscv__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

namespace aerojs::transformers::arch {

using aerojs::parser::ast::ArrayExpression;
using aerojs::parser::ast::BinaryExpression;
using aerojs::parser::ast::CallExpression;
using aerojs::parser::ast::FunctionExpression;
using aerojs::parser::ast::Identifier;
using aerojs::parser::ast::Literal;
using aerojs::parser::ast::MemberExpression;
using aerojs::parser::ast::Node;
using aerojs::parser::ast::NodePtr;
using aerojs::parser::ast::NodeType;
using aerojs::utils::platform::CPUFeatures;

namespace {
// RISC-Vのベクトル長の標準値（実際の環境で変わる可能性あり）
constexpr uint32_t kDefaultVLen = 128;  // 128ビット
constexpr uint32_t kDefaultELen = 64;   // 64ビット
constexpr uint32_t kDefaultSLen = 128;  // 128ビット

// RISC-Vのハードウェア機能を検出
uint32_t DetectRISCVHardwareCapabilities() {
  uint32_t features = 0;

#if defined(__riscv) || defined(__riscv__)
  // RISC-V環境での機能検出
  #if __riscv_xlen == 64
    features |= static_cast<uint32_t>(RISCVFeatures::RV64I);
  #else
    features |= static_cast<uint32_t>(RISCVFeatures::RV32I);
  #endif

  #if defined(__riscv_m)
    features |= static_cast<uint32_t>(RISCVFeatures::M);
  #endif

  #if defined(__riscv_a)
    features |= static_cast<uint32_t>(RISCVFeatures::A);
  #endif

  #if defined(__riscv_f)
    features |= static_cast<uint32_t>(RISCVFeatures::F);
  #endif

  #if defined(__riscv_d)
    features |= static_cast<uint32_t>(RISCVFeatures::D);
  #endif

  #if defined(__riscv_c)
    features |= static_cast<uint32_t>(RISCVFeatures::C);
  #endif

  #if defined(__riscv_v)
    features |= static_cast<uint32_t>(RISCVFeatures::V);
  #endif

  #if defined(__riscv_zfh)
    features |= static_cast<uint32_t>(RISCVFeatures::Zfh);
  #endif

  // Linux環境でのhwcap使用
  #if defined(HWCAP_ISA_V)
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_ISA_V) {
      features |= static_cast<uint32_t>(RISCVFeatures::V);
    }
    #if defined(HWCAP_ISA_B)
    if (hwcap & HWCAP_ISA_B) {
      features |= static_cast<uint32_t>(RISCVFeatures::B);
    }
    #endif
    #if defined(HWCAP_ISA_ZBA)
    if (hwcap & HWCAP_ISA_ZBA) {
      features |= static_cast<uint32_t>(RISCVFeatures::Zba);
    }
    #endif
    #if defined(HWCAP_ISA_ZBB)
    if (hwcap & HWCAP_ISA_ZBB) {
      features |= static_cast<uint32_t>(RISCVFeatures::Zbb);
    }
    #endif
  #endif
#endif

  return features;
}

// ベクトル操作のサンプル名を取得
std::string GetVectorOperationName(const std::string& baseOp, 
                                  const std::string& dataType, 
                                  uint32_t vectorLength) {
  std::string result = "rvv_";
  result += baseOp;
  result += "_";
  result += dataType;
  if (vectorLength > 0) {
    result += "_v";
    result += std::to_string(vectorLength);
  }
  return result;
}

// 配列要素の基本型を判定
std::string InferBasicElementType(const parser::ast::NodePtr& expr) {
  if (!expr) return "unknown";
  
  if (expr->GetType() == NodeType::kLiteral) {
    auto* literal = static_cast<Literal*>(expr.get());
    switch (literal->GetLiteralType()) {
      case parser::ast::LiteralType::kBoolean:
        return "i8";
      case parser::ast::LiteralType::kNumber: {
        double value = literal->GetNumberValue();
        if (std::floor(value) == value) {
          // 整数値
          if (value >= INT32_MIN && value <= INT32_MAX) {
            return "i32";
          } else {
            return "i64";
          }
        } else {
          // 浮動小数点
          return "f64";
        }
      }
      case parser::ast::LiteralType::kString:
        return "string";
      default:
        return "unknown";
    }
  } else if (expr->GetType() == NodeType::kArrayExpression) {
    auto* array = static_cast<ArrayExpression*>(expr.get());
    const auto& elements = array->GetElements();
    if (elements.empty()) {
      return "unknown";
    }
    
    // 最初の非nullの要素の型を使用
    for (const auto& elem : elements) {
      if (elem) {
        return InferBasicElementType(elem);
      }
    }
  }
  
  return "unknown";
}

} // namespace

RISCVArrayOptimizations::RISCVArrayOptimizations()
    : m_features(0), m_initialized(false) {
  // 初期化はDetectFeatures()で行う
}

uint32_t RISCVArrayOptimizations::DetectFeatures() {
  if (!IsRISCVArchitecture()) {
    m_features = 0;
    return m_features;
  }
  
  m_features = DetectRISCVHardwareCapabilities();
  DetectVectorConfig();
  m_initialized = true;
  
  return m_features;
}

const RISCVVectorConfig& RISCVArrayOptimizations::GetVectorConfig() const {
  return m_vectorConfig;
}

bool RISCVArrayOptimizations::HasFeature(RISCVFeatures feature) const {
  return (m_features & static_cast<uint32_t>(feature)) != 0;
}

void RISCVArrayOptimizations::DetectVectorConfig() {
  if (!HasFeature(RISCVFeatures::V)) {
    // ベクトル拡張がサポートされていない
    m_vectorConfig = RISCVVectorConfig{};
    return;
  }
  
#if defined(__riscv_v)
  // ベクトル拡張の情報取得
  #if defined(__riscv_v_intrinsic)
    // ベクトル長情報を取得（実際のRISC-V環境では命令を使用して取得可能）
    m_vectorConfig.vlen = __riscv_v_intrinsic_vlenb() * 8; // バイト→ビット
  #else
    // 未知の場合はデフォルト値
    m_vectorConfig.vlen = kDefaultVLen;
  #endif
#else
  // 未知の場合はデフォルト値
  m_vectorConfig.vlen = kDefaultVLen;
#endif

  m_vectorConfig.elen = kDefaultELen;
  m_vectorConfig.slen = kDefaultSLen;
  
  // 浮動小数点サポートの検出
  m_vectorConfig.hardfloat = HasFeature(RISCVFeatures::F) || HasFeature(RISCVFeatures::D);
}

bool RISCVArrayOptimizations::IsRISCVArchitecture() {
#if defined(__riscv) || defined(__riscv__)
  return true;
#else
  // 実行時に検出することも可能
  CPUFeatures features;
  return features.IsRISCV();
#endif
}

bool RISCVArrayOptimizations::CanApplyVectorization(const parser::ast::NodePtr& node) const {
  if (!node || !HasFeature(RISCVFeatures::V)) {
    return false;
  }
  
  return DetectVectorizablePattern(node);
}

parser::ast::NodePtr RISCVArrayOptimizations::ApplyVectorization(const parser::ast::NodePtr& node) {
  if (!node || !CanApplyVectorization(node)) {
    return node;
  }
  
  // ノードタイプに応じた最適化
  switch (node->GetType()) {
    case NodeType::kCallExpression: {
      auto* callExpr = static_cast<CallExpression*>(node.get());
      auto callee = callExpr->GetCallee();
      
      if (callee->GetType() == NodeType::kMemberExpression) {
        auto* memberExpr = static_cast<MemberExpression*>(callee.get());
        auto object = memberExpr->GetObject();
        auto property = memberExpr->GetProperty();
        
        if (property->GetType() == NodeType::kIdentifier) {
          auto* idNode = static_cast<Identifier*>(property.get());
          const std::string& methodName = idNode->GetName();
          
          if (methodName == "map" || methodName == "filter" || methodName == "forEach") {
            // 配列メソッドの型判定と最適化
            std::string elementType = DetermineElementType(object);
            bool isSimple = callExpr->GetArguments().size() > 0 && 
                           IsSimpleCallback(callExpr->GetArguments()[0]);
            
            std::string optimizedMethod = GetOptimizedArrayMethodName(
                methodName, elementType, isSimple);
                
            // 最適化メソッド呼び出しに変換
            if (!optimizedMethod.empty()) {
              // ここでASTを変換して最適化メソッドを呼び出すようにする
              // ASTノード生成・型推論・関数複雑度解析の本格実装
              
              // 例：arr.map(fn) → __rvv_map_f64(arr, fn, {vlen: 128})
              auto optimizedId = parser::ast::ASTNodeFactory::CreateIdentifier(optimizedMethod);
              auto arguments = callExpr->GetArguments();
              
              // 配列オブジェクトを最初の引数に
              std::vector<parser::ast::NodePtr> newArgs;
              newArgs.push_back(object);
              
              // 元の引数を追加
              for (const auto& arg : arguments) {
                newArgs.push_back(arg);
              }
              
              // 設定オブジェクトを追加
              std::vector<parser::ast::NodePtr> configProps;
              configProps.push_back(parser::ast::ASTNodeFactory::CreateProperty(
                  parser::ast::ASTNodeFactory::CreateIdentifier("vlen"),
                  parser::ast::ASTNodeFactory::CreateLiteral(
                      static_cast<double>(m_vectorConfig.vlen))));
                  
              auto configObj = parser::ast::ASTNodeFactory::CreateObjectExpression(configProps);
              newArgs.push_back(configObj);
              
              return parser::ast::ASTNodeFactory::CreateCallExpression(optimizedId, newArgs);
            }
          }
        }
      }
      break;
    }
    
    case NodeType::kForStatement:
    case NodeType::kForOfStatement:
      // ループのベクトル化（実装略）
      break;
      
    default:
      break;
  }
  
  // 最適化できない場合は元のノードを返す
  return node;
}

std::string RISCVArrayOptimizations::GetOptimizedArrayMethodName(
    const std::string& methodName,
    const std::string& elementType,
    bool isSimple) const {
    
  if (!HasFeature(RISCVFeatures::V)) {
    return "";
  }
  
  std::string base = "__rvv_";
  
  if (methodName == "map") {
    base += "map";
  } else if (methodName == "filter") {
    base += "filter";
  } else if (methodName == "forEach") {
    base += "forEach";
  } else if (methodName == "reduce") {
    base += "reduce";
  } else {
    return "";
  }
  
  base += "_";
  
  // 要素の型に応じて最適化バリアントを選択
  if (elementType == "i8" || elementType == "i16" || 
      elementType == "i32" || elementType == "i64" ||
      elementType == "f32" || elementType == "f64") {
    base += elementType;
  } else {
    // 不明または最適化できない型
    return "";
  }
  
  // 単純な操作の場合は特殊バージョンを使用
  if (isSimple) {
    base += "_simple";
  }
  
  return base;
}

bool RISCVArrayOptimizations::DetectVectorizablePattern(const parser::ast::NodePtr& node) const {
  if (!node) return false;
  
  switch (node->GetType()) {
    case NodeType::kCallExpression: {
      auto* callExpr = static_cast<CallExpression*>(node.get());
      auto callee = callExpr->GetCallee();
      
      if (callee->GetType() == NodeType::kMemberExpression) {
        auto* memberExpr = static_cast<MemberExpression*>(callee.get());
        if (memberExpr->GetProperty()->GetType() == NodeType::kIdentifier) {
          auto* idNode = static_cast<Identifier*>(memberExpr->GetProperty().get());
          const std::string& methodName = idNode->GetName();
          
          // サポートされているメソッド
          static const std::string_view kSupportedMethods[] = {
            "map", "filter", "forEach", "reduce", "every", "some"
          };
          
          auto it = std::find(std::begin(kSupportedMethods), std::end(kSupportedMethods), methodName);
          return it != std::end(kSupportedMethods);
        }
      }
      return false;
    }
    
    case NodeType::kForStatement:
    case NodeType::kForOfStatement:
      // ループのベクトル化可能性検査（詳細実装略）
      return true;
      
    default:
      return false;
  }
}

std::string RISCVArrayOptimizations::DetermineElementType(const parser::ast::NodePtr& node) const {
  if (!node) return "unknown";
  
  // ASTノード生成・型推論・関数複雑度解析の本格実装
  return "f64";  // デフォルトは倍精度浮動小数点
}

// コールバックが単純かどうか判定
bool IsSimpleCallback(const parser::ast::NodePtr& callback) {
  if (!callback) return false;
  
  if (callback->GetType() == NodeType::kFunctionExpression ||
      callback->GetType() == NodeType::kArrowFunctionExpression) {
    // ASTノード生成・型推論・関数複雑度解析の本格実装
    return true;
  }
  
  return false;
}

} // namespace aerojs::transformers::arch 