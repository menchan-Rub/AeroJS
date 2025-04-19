/**
 * @file constant_folding.cpp
 * @version 0.1
 * @copyright 2023 AeroJS
 * @brief ConstantFoldingTransformerの実装
 *
 * @details このファイルは、JavaScriptのASTに対する定数畳み込み最適化を実装しています。
 * 定数畳み込みは、コンパイル時に計算可能な式を事前に評価することで、実行時のパフォーマンスを向上させます。
 *
 * @license
 * Copyright (c) 2023 AeroJS プロジェクト
 * このソースコードはMITライセンスの下で提供されています。
 */

#include "constant_folding.h"
#include "../ast/ast_node_factory.h"
#include "../ast/visitors/ast_visitor.h"
#include "../common/logger.h"
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>  // std::max_element, std::min_element用

namespace aero {
namespace transformers {

using namespace ast;

// キャッシュを使用して同じ演算の再計算を避ける
// 数値演算のキャッシュ
struct BinaryOperationCacheKey {
    BinaryOperator op;
    double left;
    double right;
    
    bool operator==(const BinaryOperationCacheKey& other) const {
        return op == other.op && left == other.left && right == other.right;
    }
};

// ハッシュ関数
struct BinaryOperationCacheKeyHash {
    std::size_t operator()(const BinaryOperationCacheKey& key) const {
        std::size_t h1 = std::hash<int>{}(static_cast<int>(key.op));
        std::size_t h2 = std::hash<double>{}(key.left);
        std::size_t h3 = std::hash<double>{}(key.right);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// 単項演算のキャッシュキー
struct UnaryOperationCacheKey {
    UnaryOperator op;
    LiteralType type;
    union {
        double numValue;
        bool boolValue;
    };
    std::string strValue;
    
    bool operator==(const UnaryOperationCacheKey& other) const {
        if (op != other.op || type != other.type) return false;
        
        switch (type) {
            case LiteralType::Number:
                return numValue == other.numValue;
            case LiteralType::Boolean:
                return boolValue == other.boolValue;
            case LiteralType::String:
                return strValue == other.strValue;
            default:
                return true;
        }
    }
};

// ハッシュ関数
struct UnaryOperationCacheKeyHash {
    std::size_t operator()(const UnaryOperationCacheKey& key) const {
        std::size_t h1 = std::hash<int>{}(static_cast<int>(key.op));
        std::size_t h2 = std::hash<int>{}(static_cast<int>(key.type));
        std::size_t h3;
        
        switch (key.type) {
            case LiteralType::Number:
                h3 = std::hash<double>{}(key.numValue);
                break;
            case LiteralType::Boolean:
                h3 = std::hash<bool>{}(key.boolValue);
                break;
            case LiteralType::String:
                h3 = std::hash<std::string>{}(key.strValue);
                break;
            default:
                h3 = 0;
                break;
        }
        
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// キャッシュ
class ConstantFoldingCache {
public:
    // 二項演算のキャッシュ
    std::unordered_map<BinaryOperationCacheKey, NodePtr, BinaryOperationCacheKeyHash> binaryCache;
    
    // 単項演算のキャッシュ
    std::unordered_map<UnaryOperationCacheKey, NodePtr, UnaryOperationCacheKeyHash> unaryCache;
    
    void clear() {
        binaryCache.clear();
        unaryCache.clear();
    }
};

// シングルトンインスタンス
static ConstantFoldingCache& getCache() {
    static ConstantFoldingCache instance;
    return instance;
}

// 定数として扱える組み込み関数のセット
static const std::unordered_set<std::string> s_pureFunctions = {
    "Math.abs", "Math.acos", "Math.acosh", "Math.asin", "Math.asinh", "Math.atan", "Math.atanh",
    "Math.atan2", "Math.cbrt", "Math.ceil", "Math.clz32", "Math.cos", "Math.cosh", "Math.exp",
    "Math.expm1", "Math.floor", "Math.fround", "Math.hypot", "Math.imul", "Math.log", "Math.log1p",
    "Math.log10", "Math.log2", "Math.max", "Math.min", "Math.pow", "Math.round", "Math.sign",
    "Math.sin", "Math.sinh", "Math.sqrt", "Math.tan", "Math.tanh", "Math.trunc",
    "Number.isFinite", "Number.isInteger", "Number.isNaN", "Number.isSafeInteger",
    "String.fromCharCode", "String.fromCodePoint"
};

ConstantFoldingTransformer::ConstantFoldingTransformer()
    : m_statisticsEnabled(false)
    , m_foldedExpressions(0)
    , m_visitedNodes(0) {
    // キャッシュをクリア
    getCache().clear();
}

ConstantFoldingTransformer::~ConstantFoldingTransformer() {
}

void ConstantFoldingTransformer::reset() {
    m_foldedExpressions = 0;
    m_visitedNodes = 0;
    // キャッシュをクリア
    getCache().clear();
}

void ConstantFoldingTransformer::enableStatistics(bool enable) {
    m_statisticsEnabled = enable;
}

size_t ConstantFoldingTransformer::getFoldedExpressions() const {
    return m_foldedExpressions;
}

size_t ConstantFoldingTransformer::getVisitedNodes() const {
    return m_visitedNodes;
}

NodePtr ConstantFoldingTransformer::transform(NodePtr node) {
    if (!node) {
        return nullptr;
    }

    if (m_statisticsEnabled) {
        m_visitedNodes++;
    }

    // ノードタイプに基づいて適切な変換を適用
    switch (node->getType()) {
        case NodeType::BinaryExpression:
            return foldBinaryExpression(std::static_pointer_cast<BinaryExpression>(node));
        case NodeType::UnaryExpression:
            return foldUnaryExpression(std::static_pointer_cast<UnaryExpression>(node));
        case NodeType::LogicalExpression:
            return foldLogicalExpression(std::static_pointer_cast<LogicalExpression>(node));
        case NodeType::ConditionalExpression:
            return foldConditionalExpression(std::static_pointer_cast<ConditionalExpression>(node));
        case NodeType::ArrayExpression:
            return foldArrayExpression(std::static_pointer_cast<ArrayExpression>(node));
        case NodeType::ObjectExpression:
            return foldObjectExpression(std::static_pointer_cast<ObjectExpression>(node));
        case NodeType::CallExpression:
            return foldCallExpression(std::static_pointer_cast<CallExpression>(node));
        case NodeType::MemberExpression:
            return foldMemberExpression(std::static_pointer_cast<MemberExpression>(node));
        default:
            // 子ノードを再帰的に処理
            return traverseChildren(node);
    }
}

NodePtr ConstantFoldingTransformer::traverseChildren(NodePtr node) {
    // 各ノードタイプに応じた子ノードの処理
    switch (node->getType()) {
        case NodeType::Program: {
            auto program = std::static_pointer_cast<Program>(node);
            auto& body = program->getBody();
            for (size_t i = 0; i < body.size(); ++i) {
                body[i] = transform(body[i]);
            }
            return program;
        }
        case NodeType::BlockStatement: {
            auto block = std::static_pointer_cast<BlockStatement>(node);
            auto& body = block->getBody();
            for (size_t i = 0; i < body.size(); ++i) {
                body[i] = transform(body[i]);
            }
            return block;
        }
        case NodeType::ExpressionStatement: {
            auto expr = std::static_pointer_cast<ExpressionStatement>(node);
            expr->setExpression(transform(expr->getExpression()));
            return expr;
        }
        case NodeType::IfStatement: {
            auto ifStmt = std::static_pointer_cast<IfStatement>(node);
            ifStmt->setTest(transform(ifStmt->getTest()));
            ifStmt->setConsequent(transform(ifStmt->getConsequent()));
            if (ifStmt->getAlternate()) {
                ifStmt->setAlternate(transform(ifStmt->getAlternate()));
            }
            return ifStmt;
        }
        case NodeType::WhileStatement: {
            auto whileStmt = std::static_pointer_cast<WhileStatement>(node);
            whileStmt->setTest(transform(whileStmt->getTest()));
            whileStmt->setBody(transform(whileStmt->getBody()));
            return whileStmt;
        }
        case NodeType::DoWhileStatement: {
            auto doWhileStmt = std::static_pointer_cast<DoWhileStatement>(node);
            doWhileStmt->setBody(transform(doWhileStmt->getBody()));
            doWhileStmt->setTest(transform(doWhileStmt->getTest()));
            return doWhileStmt;
        }
        case NodeType::ForStatement: {
            auto forStmt = std::static_pointer_cast<ForStatement>(node);
            if (forStmt->getInit()) {
                forStmt->setInit(transform(forStmt->getInit()));
            }
            if (forStmt->getTest()) {
                forStmt->setTest(transform(forStmt->getTest()));
            }
            if (forStmt->getUpdate()) {
                forStmt->setUpdate(transform(forStmt->getUpdate()));
            }
            forStmt->setBody(transform(forStmt->getBody()));
            return forStmt;
        }
        case NodeType::ForInStatement: {
            auto forInStmt = std::static_pointer_cast<ForInStatement>(node);
            forInStmt->setLeft(transform(forInStmt->getLeft()));
            forInStmt->setRight(transform(forInStmt->getRight()));
            forInStmt->setBody(transform(forInStmt->getBody()));
            return forInStmt;
        }
        case NodeType::ForOfStatement: {
            auto forOfStmt = std::static_pointer_cast<ForOfStatement>(node);
            forOfStmt->setLeft(transform(forOfStmt->getLeft()));
            forOfStmt->setRight(transform(forOfStmt->getRight()));
            forOfStmt->setBody(transform(forOfStmt->getBody()));
            return forOfStmt;
        }
        case NodeType::SwitchStatement: {
            auto switchStmt = std::static_pointer_cast<SwitchStatement>(node);
            switchStmt->setDiscriminant(transform(switchStmt->getDiscriminant()));
            auto& cases = switchStmt->getCases();
            for (auto& caseClause : cases) {
                if (caseClause->getTest()) {
                    caseClause->setTest(transform(caseClause->getTest()));
                }
                auto& caseBody = caseClause->getConsequent();
                for (size_t i = 0; i < caseBody.size(); ++i) {
                    caseBody[i] = transform(caseBody[i]);
                }
            }
            return switchStmt;
        }
        case NodeType::ReturnStatement: {
            auto returnStmt = std::static_pointer_cast<ReturnStatement>(node);
            if (returnStmt->getArgument()) {
                returnStmt->setArgument(transform(returnStmt->getArgument()));
            }
            return returnStmt;
        }
        case NodeType::ThrowStatement: {
            auto throwStmt = std::static_pointer_cast<ThrowStatement>(node);
            throwStmt->setArgument(transform(throwStmt->getArgument()));
            return throwStmt;
        }
        case NodeType::TryStatement: {
            auto tryStmt = std::static_pointer_cast<TryStatement>(node);
            tryStmt->setBlock(transform(tryStmt->getBlock()));
            if (tryStmt->getHandler()) {
                auto handler = tryStmt->getHandler();
                if (handler->getParam()) {
                    handler->setParam(transform(handler->getParam()));
                }
                handler->setBody(transform(handler->getBody()));
            }
            if (tryStmt->getFinalizer()) {
                tryStmt->setFinalizer(transform(tryStmt->getFinalizer()));
            }
            return tryStmt;
        }
        case NodeType::VariableDeclaration: {
            auto varDecl = std::static_pointer_cast<VariableDeclaration>(node);
            auto& declarations = varDecl->getDeclarations();
            for (auto& decl : declarations) {
                if (decl->getInit()) {
                    decl->setInit(transform(decl->getInit()));
                }
            }
            return varDecl;
        }
        case NodeType::FunctionDeclaration: {
            auto funcDecl = std::static_pointer_cast<FunctionDeclaration>(node);
            funcDecl->setBody(transform(funcDecl->getBody()));
            return funcDecl;
        }
        case NodeType::FunctionExpression: {
            auto funcExpr = std::static_pointer_cast<FunctionExpression>(node);
            funcExpr->setBody(transform(funcExpr->getBody()));
            return funcExpr;
        }
        case NodeType::ArrowFunctionExpression: {
            auto arrowFunc = std::static_pointer_cast<ArrowFunctionExpression>(node);
            arrowFunc->setBody(transform(arrowFunc->getBody()));
            return arrowFunc;
        }
        case NodeType::ClassDeclaration: {
            auto classDecl = std::static_pointer_cast<ClassDeclaration>(node);
            if (classDecl->getSuperClass()) {
                classDecl->setSuperClass(transform(classDecl->getSuperClass()));
            }
            auto& body = classDecl->getBody();
            for (auto& method : body) {
                method->setValue(transform(method->getValue()));
            }
            return classDecl;
        }
        case NodeType::ClassExpression: {
            auto classExpr = std::static_pointer_cast<ClassExpression>(node);
            if (classExpr->getSuperClass()) {
                classExpr->setSuperClass(transform(classExpr->getSuperClass()));
            }
            auto& body = classExpr->getBody();
            for (auto& method : body) {
                method->setValue(transform(method->getValue()));
            }
            return classExpr;
        }
        case NodeType::SequenceExpression: {
            auto seqExpr = std::static_pointer_cast<SequenceExpression>(node);
            auto& expressions = seqExpr->getExpressions();
            for (size_t i = 0; i < expressions.size(); ++i) {
                expressions[i] = transform(expressions[i]);
            }
            return seqExpr;
        }
        default:
            return node;
    }
}

NodePtr ConstantFoldingTransformer::foldBinaryExpression(std::shared_ptr<BinaryExpression> expr) {
    // 左右の子ノードを再帰的に処理
    expr->setLeft(transform(expr->getLeft()));
    expr->setRight(transform(expr->getRight()));

    // 両方の子が定数かどうかチェック
    if (!isConstant(expr->getLeft()) || !isConstant(expr->getRight())) {
        return expr;
    }

    // 定数式を評価
    auto result = evaluateBinaryOperation(
        expr->getOperator(),
        std::static_pointer_cast<Literal>(expr->getLeft()),
        std::static_pointer_cast<Literal>(expr->getRight())
    );

    if (result) {
        if (m_statisticsEnabled) {
            m_foldedExpressions++;
        }
        return result;
    }
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldUnaryExpression(std::shared_ptr<UnaryExpression> expr) {
    // 子ノードを再帰的に処理
    expr->setArgument(transform(expr->getArgument()));

    // 子が定数かどうかチェック
    if (!isConstant(expr->getArgument())) {
        return expr;
    }

    // 定数式を評価
    auto result = evaluateUnaryOperation(
        expr->getOperator(),
        std::static_pointer_cast<Literal>(expr->getArgument())
    );

    if (result) {
        if (m_statisticsEnabled) {
            m_foldedExpressions++;
        }
        return result;
    }
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldLogicalExpression(std::shared_ptr<LogicalExpression> expr) {
    // 左右の子ノードを再帰的に処理
    expr->setLeft(transform(expr->getLeft()));
    expr->setRight(transform(expr->getRight()));

    // 両方の子が定数かどうかチェック
    if (!isConstant(expr->getLeft()) || !isConstant(expr->getRight())) {
        return expr;
    }

    // 定数式を評価
    auto result = evaluateLogicalOperation(
        expr->getOperator(),
        std::static_pointer_cast<Literal>(expr->getLeft()),
        std::static_pointer_cast<Literal>(expr->getRight())
    );

    if (result) {
        if (m_statisticsEnabled) {
            m_foldedExpressions++;
        }
        return result;
    }
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldConditionalExpression(std::shared_ptr<ConditionalExpression> expr) {
    // 条件、trueパス、falseパスを再帰的に処理
    expr->setTest(transform(expr->getTest()));
    expr->setConsequent(transform(expr->getConsequent()));
    expr->setAlternate(transform(expr->getAlternate()));

    // 条件が定数かどうかチェック
    if (!isConstant(expr->getTest())) {
        return expr;
    }

    auto testLiteral = std::static_pointer_cast<Literal>(expr->getTest());
    bool condition = isTruthy(testLiteral);

    // 条件に基づいて結果を選択
    if (condition) {
        if (m_statisticsEnabled) {
            m_foldedExpressions++;
        }
        return expr->getConsequent();
    } else {
        if (m_statisticsEnabled) {
            m_foldedExpressions++;
        }
        return expr->getAlternate();
    }
}

NodePtr ConstantFoldingTransformer::foldArrayExpression(std::shared_ptr<ArrayExpression> expr) {
    // 配列の要素を再帰的に処理
    auto& elements = expr->getElements();
    bool allConstant = true;
    
    for (size_t i = 0; i < elements.size(); ++i) {
        if (elements[i]) {  // nullでない要素のみ変換
            elements[i] = transform(elements[i]);
            if (allConstant && !isConstant(elements[i])) {
                allConstant = false;
            }
        }
    }
    
    // 現在の実装では配列リテラルは定数として畳み込まない
    // 将来的に必要に応じて拡張可能
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldObjectExpression(std::shared_ptr<ObjectExpression> expr) {
    // オブジェクトのプロパティを再帰的に処理
    auto& properties = expr->getProperties();
    bool allConstant = true;
    
    for (auto& prop : properties) {
        if (prop->getValue()) {
            prop->setValue(transform(prop->getValue()));
            if (allConstant && !isConstant(prop->getValue())) {
                allConstant = false;
            }
        }
        if (prop->getKey() && prop->isComputed()) {
            prop->setKey(transform(prop->getKey()));
            if (allConstant && !isConstant(prop->getKey())) {
                allConstant = false;
            }
        }
    }
    
    // 現在の実装ではオブジェクトリテラルは定数として畳み込まない
    // 将来的に必要に応じて拡張可能
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldCallExpression(std::shared_ptr<CallExpression> expr) {
    // 関数と引数を再帰的に処理
    expr->setCallee(transform(expr->getCallee()));
    auto& args = expr->getArguments();
    
    for (size_t i = 0; i < args.size(); ++i) {
        args[i] = transform(args[i]);
    }
    
    // 定数の引数を持つMath関数呼び出しを評価
    if (expr->getCallee()->getType() == NodeType::MemberExpression) {
        auto memberExpr = std::static_pointer_cast<MemberExpression>(expr->getCallee());
        
        // Math.Xの形式をチェック
        if (memberExpr->getObject()->getType() == NodeType::Identifier &&
            !memberExpr->isComputed()) {
            
            auto objIdent = std::static_pointer_cast<Identifier>(memberExpr->getObject());
            if (objIdent->getName() == "Math" && 
                memberExpr->getProperty()->getType() == NodeType::Identifier) {
                
                auto propIdent = std::static_pointer_cast<Identifier>(memberExpr->getProperty());
                std::string funcName = "Math." + propIdent->getName();
                
                // s_pureFunctionsにあるか確認
                if (s_pureFunctions.find(funcName) != s_pureFunctions.end()) {
                    
                    // すべての引数が数値定数かチェック
                    bool allConstantNumbers = true;
                    std::vector<double> numArgs;
                    
                    for (const auto& arg : args) {
                        if (!isConstant(arg) || 
                            std::static_pointer_cast<Literal>(arg)->getLiteralType() != LiteralType::Number) {
                            allConstantNumbers = false;
                            break;
                        }
                        
                        numArgs.push_back(std::static_pointer_cast<Literal>(arg)->getNumberValue());
                    }
                    
                    if (allConstantNumbers) {
                        double result = 0.0;
                        if (evaluateBuiltInMathFunction(funcName, numArgs, result)) {
                            if (m_statisticsEnabled) {
                                m_foldedExpressions++;
                            }
                            
                            ASTNodeFactory factory;
                            return factory.createLiteral(result);
                        }
                    }
                }
            }
        }
    }
    
    return expr;
}

NodePtr ConstantFoldingTransformer::foldMemberExpression(std::shared_ptr<MemberExpression> expr) {
    // オブジェクトとプロパティを再帰的に処理
    expr->setObject(transform(expr->getObject()));
    if (expr->isComputed()) {
        expr->setProperty(transform(expr->getProperty()));
    }
    
    // 現在の実装ではメンバーアクセスは評価しない
    // 将来的に定数オブジェクトのプロパティアクセスを評価可能
    
    return expr;
}

bool ConstantFoldingTransformer::isConstant(NodePtr node) const {
    if (!node) {
        return false;
    }
    
    NodeType type = node->getType();
    
    // リテラルは定数
    if (type == NodeType::Literal) {
        return true;
    }
    
    // 特定の識別子（true, false, null, undefined）も定数として扱う
    if (type == NodeType::Identifier) {
        auto ident = std::static_pointer_cast<Identifier>(node);
        const std::string& name = ident->getName();
        return name == "true" || name == "false" || name == "null" || name == "undefined" || 
               name == "NaN" || name == "Infinity";
    }
    
    return false;
}

bool ConstantFoldingTransformer::isTruthy(std::shared_ptr<Literal> literal) const {
    LiteralType type = literal->getLiteralType();
    
    switch (type) {
        case LiteralType::Boolean:
            return literal->getBooleanValue();
        case LiteralType::Number:
            return literal->getNumberValue() != 0.0 && 
                  !std::isnan(literal->getNumberValue());
        case LiteralType::String:
            return !literal->getStringValue().empty();
        case LiteralType::Null:
        case LiteralType::Undefined:
            return false;
        default:
            return true;  // Object, Regexp など
    }
}

NodePtr ConstantFoldingTransformer::evaluateBinaryOperation(
    BinaryOperator op,
    std::shared_ptr<Literal> left,
    std::shared_ptr<Literal> right) {
    
    // 数値演算の場合、キャッシュをチェック
    if (left->getLiteralType() == LiteralType::Number && 
        right->getLiteralType() == LiteralType::Number) {
        
        BinaryOperationCacheKey key = {
            op,
            left->getNumberValue(),
            right->getNumberValue()
        };
        
        auto& cache = getCache();
        auto it = cache.binaryCache.find(key);
        if (it != cache.binaryCache.end()) {
            return it->second;
        }
    }
    
    ast::ASTNodeFactory factory;
    
    // 数値演算
    if (left->getLiteralType() == LiteralType::Number && 
        right->getLiteralType() == LiteralType::Number) {
        
        double leftVal = left->getNumberValue();
        double rightVal = right->getNumberValue();
        double result = 0.0;
        
        switch (op) {
            case BinaryOperator::Plus:
                result = leftVal + rightVal;
                break;
            case BinaryOperator::Minus:
                result = leftVal - rightVal;
                break;
            case BinaryOperator::Multiply:
                result = leftVal * rightVal;
                break;
            case BinaryOperator::Divide:
                // ゼロ除算のチェック
                if (rightVal == 0.0) {
                    if (leftVal == 0.0) {
                        return factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
                    } else if (leftVal > 0.0) {
                        return factory.createLiteral(std::numeric_limits<double>::infinity());
                    } else {
                        return factory.createLiteral(-std::numeric_limits<double>::infinity());
                    }
                }
                result = leftVal / rightVal;
                break;
            case BinaryOperator::Modulo:
                // ゼロ除算のチェック
                if (rightVal == 0.0) {
                    return factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
                }
                result = std::fmod(leftVal, rightVal);
                break;
            case BinaryOperator::Exponentiation:
                result = std::pow(leftVal, rightVal);
                break;
            case BinaryOperator::BitwiseAnd:
                result = static_cast<double>(static_cast<int32_t>(leftVal) & static_cast<int32_t>(rightVal));
                break;
            case BinaryOperator::BitwiseOr:
                result = static_cast<double>(static_cast<int32_t>(leftVal) | static_cast<int32_t>(rightVal));
                break;
            case BinaryOperator::BitwiseXor:
                result = static_cast<double>(static_cast<int32_t>(leftVal) ^ static_cast<int32_t>(rightVal));
                break;
            case BinaryOperator::LeftShift:
                result = static_cast<double>(static_cast<int32_t>(leftVal) << (static_cast<int32_t>(rightVal) & 0x1F));
                break;
            case BinaryOperator::RightShift:
                result = static_cast<double>(static_cast<int32_t>(leftVal) >> (static_cast<int32_t>(rightVal) & 0x1F));
                break;
            case BinaryOperator::UnsignedRightShift: {
                uint32_t uLeftVal = static_cast<uint32_t>(static_cast<int32_t>(leftVal));
                result = static_cast<double>(uLeftVal >> (static_cast<int32_t>(rightVal) & 0x1F));
                break;
            }
            case BinaryOperator::Equal:
                return factory.createLiteral(leftVal == rightVal);
            case BinaryOperator::NotEqual:
                return factory.createLiteral(leftVal != rightVal);
            case BinaryOperator::StrictEqual:
                return factory.createLiteral(leftVal == rightVal);
            case BinaryOperator::StrictNotEqual:
                return factory.createLiteral(leftVal != rightVal);
            case BinaryOperator::LessThan:
                return factory.createLiteral(leftVal < rightVal);
            case BinaryOperator::LessThanOrEqual:
                return factory.createLiteral(leftVal <= rightVal);
            case BinaryOperator::GreaterThan:
                return factory.createLiteral(leftVal > rightVal);
            case BinaryOperator::GreaterThanOrEqual:
                return factory.createLiteral(leftVal >= rightVal);
            default:
                return nullptr;
        }
        
        // キャッシュに結果を保存（数値演算の場合）
        if (left->getLiteralType() == LiteralType::Number && 
            right->getLiteralType() == LiteralType::Number) {
            
            BinaryOperationCacheKey key = {
                op,
                left->getNumberValue(),
                right->getNumberValue()
            };
            
            auto& cache = getCache();
            cache.binaryCache[key] = factory.createLiteral(result);
        }
        
        return factory.createLiteral(result);
    }
    
    // 文字列の連結（+演算子）
    if (op == BinaryOperator::Plus) {
        std::string leftStr = literalToString(left);
        std::string rightStr = literalToString(right);
        return factory.createLiteral(leftStr + rightStr);
    }
    
    // 比較演算子
    if (op == BinaryOperator::Equal || op == BinaryOperator::NotEqual ||
        op == BinaryOperator::StrictEqual || op == BinaryOperator::StrictNotEqual) {
        
        // 型が異なる場合の厳密等価演算子
        if ((op == BinaryOperator::StrictEqual || op == BinaryOperator::StrictNotEqual) &&
            left->getLiteralType() != right->getLiteralType()) {
            return factory.createLiteral(op == BinaryOperator::StrictNotEqual);
        }
        
        // 値の比較
        bool result = false;
        
        switch (left->getLiteralType()) {
            case LiteralType::Boolean:
                result = left->getBooleanValue() == (right->getLiteralType() == LiteralType::Boolean ? 
                         right->getBooleanValue() : isTruthy(right));
                break;
            case LiteralType::Number:
                if (right->getLiteralType() == LiteralType::Number) {
                    result = left->getNumberValue() == right->getNumberValue();
                } else if (right->getLiteralType() == LiteralType::String) {
                    // 文字列を数値に変換して比較
                    try {
                        double rightVal = std::stod(right->getStringValue());
                        result = left->getNumberValue() == rightVal;
                    } catch (...) {
                        result = false;
                    }
                } else if (right->getLiteralType() == LiteralType::Boolean) {
                    result = left->getNumberValue() == (right->getBooleanValue() ? 1.0 : 0.0);
                } else if (right->getLiteralType() == LiteralType::Null) {
                    result = false;  // 数値とnullは常に等しくない
                } else {
                    result = false;
                }
                break;
            case LiteralType::String:
                if (right->getLiteralType() == LiteralType::String) {
                    result = left->getStringValue() == right->getStringValue();
                } else if (right->getLiteralType() == LiteralType::Number) {
                    // 文字列を数値に変換して比較
                    try {
                        double leftVal = std::stod(left->getStringValue());
                        result = leftVal == right->getNumberValue();
                    } catch (...) {
                        result = false;
                    }
                } else {
                    result = false;
                }
                break;
            case LiteralType::Null:
            case LiteralType::Undefined:
                result = right->getLiteralType() == LiteralType::Null || 
                         right->getLiteralType() == LiteralType::Undefined;
                break;
            default:
                return nullptr;  // 複雑なケースは評価しない
        }
        
        if (op == BinaryOperator::NotEqual || op == BinaryOperator::StrictNotEqual) {
            result = !result;
        }
        
        return factory.createLiteral(result);
    }
    
    // より複雑な比較演算
    if (op == BinaryOperator::LessThan || op == BinaryOperator::LessThanOrEqual ||
        op == BinaryOperator::GreaterThan || op == BinaryOperator::GreaterThanOrEqual) {
        
        double leftVal = 0.0;
        double rightVal = 0.0;
        
        // 左辺の値を数値に変換
        if (left->getLiteralType() == LiteralType::Number) {
            leftVal = left->getNumberValue();
        } else if (left->getLiteralType() == LiteralType::String) {
            try {
                leftVal = std::stod(left->getStringValue());
            } catch (...) {
                return nullptr;  // 変換できない場合は評価しない
            }
        } else if (left->getLiteralType() == LiteralType::Boolean) {
            leftVal = left->getBooleanValue() ? 1.0 : 0.0;
        } else {
            return nullptr;  // 他の型は評価しない
        }
        
        // 右辺の値を数値に変換
        if (right->getLiteralType() == LiteralType::Number) {
            rightVal = right->getNumberValue();
        } else if (right->getLiteralType() == LiteralType::String) {
            try {
                rightVal = std::stod(right->getStringValue());
            } catch (...) {
                return nullptr;  // 変換できない場合は評価しない
            }
        } else if (right->getLiteralType() == LiteralType::Boolean) {
            rightVal = right->getBooleanValue() ? 1.0 : 0.0;
        } else {
            return nullptr;  // 他の型は評価しない
        }
        
        // 比較演算子による評価
        bool result = false;
        switch (op) {
            case BinaryOperator::LessThan:
                result = leftVal < rightVal;
                break;
            case BinaryOperator::LessThanOrEqual:
                result = leftVal <= rightVal;
                break;
            case BinaryOperator::GreaterThan:
                result = leftVal > rightVal;
                break;
            case BinaryOperator::GreaterThanOrEqual:
                result = leftVal >= rightVal;
                break;
            default:
                return nullptr;
        }
        
        return factory.createLiteral(result);
    }
    
    // その他の演算は現在サポートしていない
    return nullptr;
}

NodePtr ConstantFoldingTransformer::evaluateUnaryOperation(
    UnaryOperator op,
    std::shared_ptr<Literal> arg) {
    
    // キャッシュをチェック
    UnaryOperationCacheKey key;
    key.op = op;
    key.type = arg->getLiteralType();
    
    switch (key.type) {
        case LiteralType::Number:
            key.numValue = arg->getNumberValue();
            break;
        case LiteralType::Boolean:
            key.boolValue = arg->getBooleanValue();
            break;
        case LiteralType::String:
            key.strValue = arg->getStringValue();
            break;
        default:
            break;
    }
    
    auto& cache = getCache();
    auto it = cache.unaryCache.find(key);
    if (it != cache.unaryCache.end()) {
        return it->second;
    }
    
    ast::ASTNodeFactory factory;
    
    NodePtr result = nullptr;
    
    switch (op) {
        case UnaryOperator::Plus: {
            // 数値への変換
            if (arg->getLiteralType() == LiteralType::Number) {
                result = arg;  // 数値はそのまま
            } else if (arg->getLiteralType() == LiteralType::String) {
                try {
                    double val = std::stod(arg->getStringValue());
                    result = factory.createLiteral(val);
                } catch (...) {
                    result = factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
                }
            } else if (arg->getLiteralType() == LiteralType::Boolean) {
                result = factory.createLiteral(arg->getBooleanValue() ? 1.0 : 0.0);
            } else if (arg->getLiteralType() == LiteralType::Null) {
                result = factory.createLiteral(0.0);
            } else if (arg->getLiteralType() == LiteralType::Undefined) {
                result = factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
            }
            break;
        }
        case UnaryOperator::Minus: {
            // 数値の符号反転
            if (arg->getLiteralType() == LiteralType::Number) {
                result = factory.createLiteral(-arg->getNumberValue());
            } else if (arg->getLiteralType() == LiteralType::String) {
                try {
                    double val = std::stod(arg->getStringValue());
                    result = factory.createLiteral(-val);
                } catch (...) {
                    result = factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
                }
            } else if (arg->getLiteralType() == LiteralType::Boolean) {
                result = factory.createLiteral(arg->getBooleanValue() ? -1.0 : -0.0);
            } else if (arg->getLiteralType() == LiteralType::Null) {
                result = factory.createLiteral(-0.0);
            } else if (arg->getLiteralType() == LiteralType::Undefined) {
                result = factory.createLiteral(std::numeric_limits<double>::quiet_NaN());
            }
            break;
        }
        case UnaryOperator::LogicalNot:
            // 論理否定
            result = factory.createLiteral(!isTruthy(arg));
            break;
        case UnaryOperator::BitwiseNot: {
            // ビット否定
            if (arg->getLiteralType() == LiteralType::Number) {
                result = factory.createLiteral(static_cast<double>(~static_cast<int32_t>(arg->getNumberValue())));
            } else if (arg->getLiteralType() == LiteralType::String) {
                try {
                    double val = std::stod(arg->getStringValue());
                    result = factory.createLiteral(static_cast<double>(~static_cast<int32_t>(val)));
                } catch (...) {
                    result = factory.createLiteral(static_cast<double>(~0));
                }
            } else if (arg->getLiteralType() == LiteralType::Boolean) {
                result = factory.createLiteral(static_cast<double>(~(arg->getBooleanValue() ? 1 : 0)));
            } else if (arg->getLiteralType() == LiteralType::Null) {
                result = factory.createLiteral(static_cast<double>(~0));
            } else if (arg->getLiteralType() == LiteralType::Undefined) {
                result = factory.createLiteral(static_cast<double>(~0));
            }
            break;
        }
        case UnaryOperator::Typeof: {
            // typeof演算子
            switch (arg->getLiteralType()) {
                case LiteralType::Boolean:
                    result = factory.createLiteral("boolean");
                    break;
                case LiteralType::Number:
                    result = factory.createLiteral("number");
                    break;
                case LiteralType::String:
                    result = factory.createLiteral("string");
                    break;
                case LiteralType::Null:
                    result = factory.createLiteral("object");
                    break;
                case LiteralType::Undefined:
                    result = factory.createLiteral("undefined");
                    break;
                case LiteralType::Object:
                    result = factory.createLiteral("object");
                    break;
                case LiteralType::RegExp:
                    result = factory.createLiteral("object");
                    break;
                default:
                    result = nullptr;
            }
            break;
        }
        case UnaryOperator::Void:
            // void演算子は常にundefinedを返す
            result = factory.createLiteral(LiteralType::Undefined);
            break;
        case UnaryOperator::Delete:
            // リテラルに対するdeleteは常にtrueを返す
            result = factory.createLiteral(true);
            break;
        default:
            result = nullptr;
            break;
    }
    
    // キャッシュに結果を保存
    if (result) {
        cache.unaryCache[key] = result;
    }
    
    return result;
}

NodePtr ConstantFoldingTransformer::evaluateLogicalOperation(
    LogicalOperator op,
    std::shared_ptr<Literal> left,
    std::shared_ptr<Literal> right) {
    
    // 論理演算子の短絡評価
    bool leftTruthy = isTruthy(left);
    
    switch (op) {
        case LogicalOperator::And:
            // leftがfalseの場合、leftを返す
            if (!leftTruthy) {
                return left;
            }
            // leftがtrueの場合、rightを返す
            return right;
        
        case LogicalOperator::Or:
            // leftがtrueの場合、leftを返す
            if (leftTruthy) {
                return left;
            }
            // leftがfalseの場合、rightを返す
            return right;
        
        case LogicalOperator::NullishCoalescing:
            // leftがnullまたはundefinedの場合、rightを返す
            if (left->getLiteralType() == LiteralType::Null || 
                left->getLiteralType() == LiteralType::Undefined) {
                return right;
            }
            // それ以外の場合、leftを返す
            return left;
        
        default:
            return nullptr;
    }
}

std::string ConstantFoldingTransformer::literalToString(std::shared_ptr<Literal> literal) const {
    switch (literal->getLiteralType()) {
        case LiteralType::String:
            return literal->getStringValue();
        case LiteralType::Number:
            return std::to_string(literal->getNumberValue());
        case LiteralType::Boolean:
            return literal->getBooleanValue() ? "true" : "false";
        case LiteralType::Null:
            return "null";
        case LiteralType::Undefined:
            return "undefined";
        default:
            return "[object Object]";  // デフォルトのオブジェクト文字列表現
    }
}

/**
 * @brief 組み込みのMath関数を評価します
 * 
 * @param name 関数名（"Math.sin"など）
 * @param args 数値引数のリスト
 * @param result 計算結果（出力パラメータ）
 * @return 計算に成功した場合はtrue、失敗した場合はfalse
 */
bool ConstantFoldingTransformer::evaluateBuiltInMathFunction(
    const std::string& name,
    const std::vector<double>& args,
    double& result) {
    
    // 引数の数をチェック
    auto checkArgsCount = [&args](size_t expected) -> bool {
        return args.size() == expected;
    };
    
    // NaN引数のチェック
    auto hasNaN = [&args]() -> bool {
        for (const auto& arg : args) {
            if (std::isnan(arg)) {
                return true;
            }
        }
        return false;
    };
    
    // Math.abs（絶対値）
    if (name == "Math.abs") {
        if (!checkArgsCount(1)) return false;
        result = std::abs(args[0]);
        return true;
    }
    
    // Math.acos（アークコサイン）
    if (name == "Math.acos") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < -1.0 || args[0] > 1.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::acos(args[0]);
        }
        return true;
    }
    
    // Math.acosh（双曲線アークコサイン）
    if (name == "Math.acosh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < 1.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::acosh(args[0]);
        }
        return true;
    }
    
    // Math.asin（アークサイン）
    if (name == "Math.asin") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < -1.0 || args[0] > 1.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::asin(args[0]);
        }
        return true;
    }
    
    // Math.asinh（双曲線アークサイン）
    if (name == "Math.asinh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::asinh(args[0]);
        }
        return true;
    }
    
    // Math.atan（アークタンジェント）
    if (name == "Math.atan") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::atan(args[0]);
        }
        return true;
    }
    
    // Math.atanh（双曲線アークタンジェント）
    if (name == "Math.atanh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] <= -1.0 || args[0] >= 1.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::atanh(args[0]);
        }
        return true;
    }
    
    // Math.atan2（2つの引数を取るアークタンジェント）
    if (name == "Math.atan2") {
        if (!checkArgsCount(2)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::atan2(args[0], args[1]);
        }
        return true;
    }
    
    // Math.cbrt（立方根）
    if (name == "Math.cbrt") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::cbrt(args[0]);
        }
        return true;
    }
    
    // Math.ceil（天井関数）
    if (name == "Math.ceil") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::ceil(args[0]);
        }
        return true;
    }
    
    // Math.clz32（32ビット整数のリーディングゼロの数）
    if (name == "Math.clz32") {
        if (!checkArgsCount(1)) return false;
        
        uint32_t x = static_cast<uint32_t>(args[0]);
        if (x == 0) {
            result = 32.0;
        } else {
            int count = 0;
            uint32_t mask = 0x80000000;
            while ((x & mask) == 0) {
                count++;
                mask >>= 1;
            }
            result = static_cast<double>(count);
        }
        return true;
    }
    
    // Math.cos（コサイン）
    if (name == "Math.cos") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::cos(args[0]);
        }
        return true;
    }
    
    // Math.cosh（双曲線コサイン）
    if (name == "Math.cosh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::cosh(args[0]);
        }
        return true;
    }
    
    // Math.exp（eの累乗）
    if (name == "Math.exp") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::exp(args[0]);
        }
        return true;
    }
    
    // Math.expm1（e^x - 1）
    if (name == "Math.expm1") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::expm1(args[0]);
        }
        return true;
    }
    
    // Math.floor（床関数）
    if (name == "Math.floor") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::floor(args[0]);
        }
        return true;
    }
    
    // Math.fround（32ビット浮動小数点に丸める）
    if (name == "Math.fround") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            // 32ビット浮動小数点に変換してから戻す
            float f = static_cast<float>(args[0]);
            result = static_cast<double>(f);
        }
        return true;
    }
    
    // Math.hypot（引数の二乗和の平方根）
    if (name == "Math.hypot") {
        if (args.empty()) {
            result = 0.0;
            return true;
        }
        
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        
        // 引数が1つの場合は絶対値を返す
        if (args.size() == 1) {
            result = std::abs(args[0]);
            return true;
        }
        
        // 引数が複数の場合はstd::hypotを使用
        if (args.size() == 2) {
            result = std::hypot(args[0], args[1]);
        } else {
            // 3つ以上の引数の場合は自分で計算
            double sum = 0.0;
            for (const auto& arg : args) {
                sum += arg * arg;
            }
            result = std::sqrt(sum);
        }
        return true;
    }
    
    // Math.imul（32ビット整数の乗算）
    if (name == "Math.imul") {
        if (!checkArgsCount(2)) return false;
        
        int32_t a = static_cast<int32_t>(args[0]);
        int32_t b = static_cast<int32_t>(args[1]);
        result = static_cast<double>(a * b);
        return true;
    }
    
    // Math.log（自然対数）
    if (name == "Math.log") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < 0.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] == 0.0) {
            result = -std::numeric_limits<double>::infinity();
        } else {
            result = std::log(args[0]);
        }
        return true;
    }
    
    // Math.log1p（ln(1 + x)）
    if (name == "Math.log1p") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < -1.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] == -1.0) {
            result = -std::numeric_limits<double>::infinity();
        } else {
            result = std::log1p(args[0]);
        }
        return true;
    }
    
    // Math.log10（常用対数）
    if (name == "Math.log10") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < 0.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] == 0.0) {
            result = -std::numeric_limits<double>::infinity();
        } else {
            result = std::log10(args[0]);
        }
        return true;
    }
    
    // Math.log2（2を底とする対数）
    if (name == "Math.log2") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < 0.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] == 0.0) {
            result = -std::numeric_limits<double>::infinity();
        } else {
            result = std::log2(args[0]);
        }
        return true;
    }
    
    // Math.max（最大値）
    if (name == "Math.max") {
        if (args.empty()) {
            result = -std::numeric_limits<double>::infinity();
            return true;
        }
        
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        
        result = *std::max_element(args.begin(), args.end());
        return true;
    }
    
    // Math.min（最小値）
    if (name == "Math.min") {
        if (args.empty()) {
            result = std::numeric_limits<double>::infinity();
            return true;
        }
        
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
            return true;
        }
        
        result = *std::min_element(args.begin(), args.end());
        return true;
    }
    
    // Math.pow（累乗）
    if (name == "Math.pow") {
        if (!checkArgsCount(2)) return false;
        
        // 特殊なケースの処理
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] == 1.0) {
            result = 1.0;  // 1^(任意の値) = 1
        } else if (args[0] == -1.0 && std::isinf(args[1])) {
            result = 1.0;  // (-1)^(無限大) = 1
        } else if (args[0] == 0.0 && args[1] < 0.0) {
            if (std::signbit(args[0])) {
                // -0^(負の数) = 符号付き無限大（奇数の場合は負、偶数の場合は正）
                if (std::fmod(std::abs(args[1]), 2.0) == 1.0) {
                    result = -std::numeric_limits<double>::infinity();
                } else {
                    result = std::numeric_limits<double>::infinity();
                }
            } else {
                // +0^(負の数) = 無限大
                result = std::numeric_limits<double>::infinity();
            }
        } else if (args[0] == 0.0 && args[1] > 0.0) {
            // 0^(正の数) = 0
            result = 0.0;
        } else if (std::abs(args[0]) > 1.0 && std::isinf(args[1]) && args[1] > 0.0) {
            // |x| > 1, x^(+無限大) = +無限大
            result = std::numeric_limits<double>::infinity();
        } else if (std::abs(args[0]) > 1.0 && std::isinf(args[1]) && args[1] < 0.0) {
            // |x| > 1, x^(-無限大) = 0
            result = 0.0;
        } else if (std::abs(args[0]) < 1.0 && std::isinf(args[1]) && args[1] > 0.0) {
            // |x| < 1, x^(+無限大) = 0
            result = 0.0;
        } else if (std::abs(args[0]) < 1.0 && std::isinf(args[1]) && args[1] < 0.0) {
            // |x| < 1, x^(-無限大) = +無限大
            result = std::numeric_limits<double>::infinity();
        } else if (std::isinf(args[0]) && args[1] > 0.0) {
            // x = 無限大, x^(正の数) = +無限大
            if (args[0] < 0.0 && std::fmod(args[1], 2.0) == 1.0) {
                result = -std::numeric_limits<double>::infinity();
            } else {
                result = std::numeric_limits<double>::infinity();
            }
        } else if (std::isinf(args[0]) && args[1] < 0.0) {
            // x = 無限大, x^(負の数) = 0
            result = 0.0;
        } else {
            // 通常の計算
            result = std::pow(args[0], args[1]);
        }
        
        return true;
    }
    
    // Math.round（四捨五入）
    if (name == "Math.round") {
        if (!checkArgsCount(1)) return false;
        
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            // JavaScriptの丸め処理（0.5は切り上げ）
            result = std::floor(args[0] + 0.5);
            
            // 特殊なケース: -0.5は-0.0に丸める
            if (args[0] == -0.5) {
                result = -0.0;
            }
        }
        
        return true;
    }
    
    // Math.sign（符号関数）
    if (name == "Math.sign") {
        if (!checkArgsCount(1)) return false;
        
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else if (args[0] > 0.0) {
            result = 1.0;
        } else if (args[0] < 0.0) {
            result = -1.0;
        } else {
            // 0または-0の場合
            result = args[0];  // 符号を保存
        }
        
        return true;
    }
    
    // Math.sin（サイン）
    if (name == "Math.sin") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::sin(args[0]);
        }
        return true;
    }
    
    // Math.sinh（双曲線サイン）
    if (name == "Math.sinh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::sinh(args[0]);
        }
        return true;
    }
    
    // Math.sqrt（平方根）
    if (name == "Math.sqrt") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN() || args[0] < 0.0) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::sqrt(args[0]);
        }
        return true;
    }
    
    // Math.tan（タンジェント）
    if (name == "Math.tan") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::tan(args[0]);
        }
        return true;
    }
    
    // Math.tanh（双曲線タンジェント）
    if (name == "Math.tanh") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            result = std::tanh(args[0]);
        }
        return true;
    }
    
    // Math.trunc（整数部分）
    if (name == "Math.trunc") {
        if (!checkArgsCount(1)) return false;
        if (hasNaN()) {
            result = std::numeric_limits<double>::quiet_NaN();
        } else {
            // 整数部分のみを取得（小数点以下を切り捨て）
            result = args[0] < 0.0 ? std::ceil(args[0]) : std::floor(args[0]);
        }
        return true;
    }
    
    // Number関連の関数
    if (name == "Number.isFinite") {
        if (!checkArgsCount(1)) return false;
        result = std::isfinite(args[0]) ? 1.0 : 0.0;
        return true;
    }
    
    if (name == "Number.isInteger") {
        if (!checkArgsCount(1)) return false;
        result = (std::isfinite(args[0]) && args[0] == std::floor(args[0])) ? 1.0 : 0.0;
        return true;
    }
    
    if (name == "Number.isNaN") {
        if (!checkArgsCount(1)) return false;
        result = std::isnan(args[0]) ? 1.0 : 0.0;
        return true;
    }
    
    if (name == "Number.isSafeInteger") {
        if (!checkArgsCount(1)) return false;
        const double MAX_SAFE_INTEGER = 9007199254740991.0;  // 2^53 - 1
        bool isSafe = std::isfinite(args[0]) && 
                      args[0] == std::floor(args[0]) && 
                      std::abs(args[0]) <= MAX_SAFE_INTEGER;
        result = isSafe ? 1.0 : 0.0;
        return true;
    }
    
    // String関連の関数
    if (name == "String.fromCharCode" || name == "String.fromCodePoint") {
        // これらの関数は文字列を返すため、定数畳み込みでは数値結果として評価できない
        return false;
    }
    
    // サポートされていない関数
    return false;
}

} // namespace transformers
} // namespace aero 