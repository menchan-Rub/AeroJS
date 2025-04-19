/**
 * @file dead_code_elimination.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 不要なコード（デッドコード）を除去するトランスフォーマーの実装
 */

#include "dead_code_elimination.h"
#include "core/ast/node.h"
#include "core/ast/visitor.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace aero {
namespace transformers {

DeadCodeEliminationTransformer::DeadCodeEliminationTransformer()
    : Transformer("DeadCodeElimination", "不要なコード（デッドコード）を除去するトランスフォーマー")
    , m_unreachableCodeDetected(false)
{
}

void DeadCodeEliminationTransformer::visitBlockStatement(ast::BlockStatement* node) {
    // 既にブロック内に到達不能コードが含まれているか確認
    bool hasUnreachableCode = removeUnreachableCode(node->body);
    if (hasUnreachableCode) {
        m_changed = true;
    }
    
    // 空のブロックステートメントを削除（または最適化）する
    if (node->body.empty()) {
        // 空のブロックは変更なしとマークする（必要に応じてサブクラスでカスタマイズ可能）
        return;
    }
    
    // 再帰的に子ノードを処理
    bool localChanged = false;
    for (size_t i = 0; i < node->body.size(); ++i) {
        auto result = transformNode(node->body[i]);
        if (result.changed) {
            localChanged = true;
            node->body[i] = std::move(result.node);
        }
    }
    
    // 最適化の結果、ブロックが空になった場合
    if (node->body.empty()) {
        // 空のブロックになった場合は変更ありとマーク
        m_changed = true;
    } else {
        m_changed = localChanged;
    }
}

void DeadCodeEliminationTransformer::visitIfStatement(ast::IfStatement* node) {
    // 条件式を変換
    auto testResult = transformNode(node->test);
    if (testResult.changed) {
        m_changed = true;
        node->test = std::move(testResult.node);
    }
    
    // 条件式が静的に評価可能かチェック
    bool conditionValue;
    bool canDetermine = evaluatesToTruthy(node->test, conditionValue);
    
    if (canDetermine) {
        if (conditionValue) {
            // 条件が常にtrueの場合、then節だけを保持
            auto consequentResult = transformNode(node->consequent);
            if (consequentResult.changed) {
                m_changed = true;
                node->consequent = std::move(consequentResult.node);
            }
            
            // ifステートメントをthen節で置き換え
            m_result = node->consequent;
            m_changed = true;
            return;
        } else {
            // 条件が常にfalseの場合、else節があればそれを保持、なければifステートメントを削除
            if (node->alternate) {
                auto alternateResult = transformNode(node->alternate);
                if (alternateResult.changed) {
                    m_changed = true;
                    node->alternate = std::move(alternateResult.node);
                }
                
                // ifステートメントをelse節で置き換え
                m_result = node->alternate;
                m_changed = true;
                return;
            } else {
                // else節がない場合は空のブロックに置き換え
                m_result = std::make_shared<ast::BlockStatement>();
                m_changed = true;
                return;
            }
        }
    }
    
    // 条件が静的に決定できない場合は通常処理を続行
    
    // then節を変換
    auto consequentResult = transformNode(node->consequent);
    if (consequentResult.changed) {
        m_changed = true;
        node->consequent = std::move(consequentResult.node);
    }
    
    // else節を変換
    if (node->alternate) {
        auto alternateResult = transformNode(node->alternate);
        if (alternateResult.changed) {
            m_changed = true;
            node->alternate = std::move(alternateResult.node);
        }
    }
}

void DeadCodeEliminationTransformer::visitSwitchStatement(ast::SwitchStatement* node) {
    // 判別式を変換
    auto discriminantResult = transformNode(node->discriminant);
    if (discriminantResult.changed) {
        m_changed = true;
        node->discriminant = std::move(discriminantResult.node);
    }
    
    // ケース節を変換
    bool localChanged = false;
    for (size_t i = 0; i < node->cases.size(); ++i) {
        auto caseResult = transformNode(node->cases[i]);
        if (caseResult.changed) {
            localChanged = true;
            node->cases[i] = std::move(caseResult.node);
        }
    }
    
    // 空のケース節を削除
    node->cases.erase(
        std::remove_if(node->cases.begin(), node->cases.end(),
            [](const ast::NodePtr& caseNode) {
                auto* caseClause = static_cast<ast::CaseClause*>(caseNode.get());
                return caseClause->consequent.empty();
            }),
        node->cases.end()
    );
    
    // ケース節がすべて削除された場合
    if (node->cases.empty()) {
        // 空のブロックに置き換え
        m_result = std::make_shared<ast::BlockStatement>();
        m_changed = true;
        return;
    }
    
    m_changed = localChanged || (node->cases.size() != node->cases.size());
}

void DeadCodeEliminationTransformer::visitForStatement(ast::ForStatement* node) {
    // 初期化式を変換
    if (node->init) {
        auto initResult = transformNode(node->init);
        if (initResult.changed) {
            m_changed = true;
            node->init = std::move(initResult.node);
        }
    }
    
    // 条件式を変換
    if (node->test) {
        auto testResult = transformNode(node->test);
        if (testResult.changed) {
            m_changed = true;
            node->test = std::move(testResult.node);
        }
        
        // 条件式が静的に評価可能かチェック
        bool conditionValue;
        bool canDetermine = evaluatesToTruthy(node->test, conditionValue);
        
        if (canDetermine && !conditionValue) {
            // 条件が常にfalseの場合、ループは一度も実行されない
            if (node->init && !hasSideEffects(node->init)) {
                // 初期化式が副作用を持たない場合は完全に削除
                m_result = std::make_shared<ast::BlockStatement>();
                m_changed = true;
                return;
            } else {
                // 初期化式が副作用を持つ場合は初期化式だけを実行
                auto block = std::make_shared<ast::BlockStatement>();
                if (node->init) {
                    // 初期化式を式ステートメントとして追加
                    auto exprStmt = std::make_shared<ast::ExpressionStatement>();
                    exprStmt->expression = node->init;
                    block->body.push_back(exprStmt);
                }
                m_result = block;
                m_changed = true;
                return;
            }
        }
    }
    
    // 更新式を変換
    if (node->update) {
        auto updateResult = transformNode(node->update);
        if (updateResult.changed) {
            m_changed = true;
            node->update = std::move(updateResult.node);
        }
    }
    
    // 本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        m_changed = true;
        node->body = std::move(bodyResult.node);
    }
}

void DeadCodeEliminationTransformer::visitReturnStatement(ast::ReturnStatement* node) {
    // return文以降は到達不能コードになるため、そのことを記録
    m_unreachableCodeDetected = true;
    
    // 戻り値があれば変換
    if (node->argument) {
        auto argumentResult = transformNode(node->argument);
        if (argumentResult.changed) {
            m_changed = true;
            node->argument = std::move(argumentResult.node);
        }
    }
}

void DeadCodeEliminationTransformer::visitExpressionStatement(ast::ExpressionStatement* node) {
    // 式を変換
    auto expressionResult = transformNode(node->expression);
    if (expressionResult.changed) {
        m_changed = true;
        node->expression = std::move(expressionResult.node);
    }
    
    // 副作用のない式ステートメントは削除
    if (!hasSideEffects(node->expression)) {
        // 空のブロックに置き換え
        m_result = std::make_shared<ast::BlockStatement>();
        m_changed = true;
    }
}

void DeadCodeEliminationTransformer::visitVariableDeclaration(ast::VariableDeclaration* node) {
    // 変数宣言子を変換
    bool localChanged = false;
    
    // 使用されない宣言を削除するための一時リスト
    std::vector<std::shared_ptr<ast::VariableDeclarator>> usedDeclarations;
    usedDeclarations.reserve(node->declarations.size());
    
    for (size_t i = 0; i < node->declarations.size(); ++i) {
        auto& declaration = node->declarations[i];
        
        // 初期化式があれば変換
        if (declaration->init) {
            auto initResult = transformNode(declaration->init);
            if (initResult.changed) {
                localChanged = true;
                declaration->init = std::move(initResult.node);
            }
            
            // 初期化式がある場合は副作用がある可能性があるため保持
            usedDeclarations.push_back(declaration);
            
            // 変数使用状況を追跡
            if (auto* id = dynamic_cast<ast::Identifier*>(declaration->id.get())) {
                m_currentFunctionVariables[id->name] = VariableInfo{
                    .initialized = true,
                    .used = false,
                    .constValue = evaluateConstantExpression(declaration->init.get())
                };
            }
        } else {
            // 初期化式がない場合でも、constやletの場合は保持
            if (node->kind != ast::VariableDeclarationKind::Var || 
                isVariableUsed(declaration->id.get())) {
                usedDeclarations.push_back(declaration);
            } else {
                // 使用されていないvarで初期化もない変数は削除
                localChanged = true;
            }
            
            // 変数使用状況を追跡
            if (auto* id = dynamic_cast<ast::Identifier*>(declaration->id.get())) {
                m_currentFunctionVariables[id->name] = VariableInfo{
                    .initialized = false,
                    .used = false,
                    .constValue = std::nullopt
                };
            }
        }
    }
    
    // 使用されない宣言が削除された場合
    if (usedDeclarations.size() != node->declarations.size()) {
        localChanged = true;
        
        // すべての宣言が削除された場合は空のブロックに置き換え
        if (usedDeclarations.empty()) {
            m_result = std::make_shared<ast::BlockStatement>();
        } else {
            // 残った宣言で更新
            node->declarations = std::move(usedDeclarations);
        }
    }
    
    // 変数宣言の種類に基づいて最適化
    if (node->kind == ast::VariableDeclarationKind::Const) {
        // 定数伝播のための情報を収集
        for (const auto& declaration : node->declarations) {
            if (auto* id = dynamic_cast<ast::Identifier*>(declaration->id.get())) {
                if (declaration->init) {
                    auto constValue = evaluateConstantExpression(declaration->init.get());
                    if (constValue) {
                        m_constantValues[id->name] = *constValue;
                    }
                }
            }
        }
    }
    
    m_changed |= localChanged;
}

void DeadCodeEliminationTransformer::visitFunctionDeclaration(ast::FunctionDeclaration* node) {
    // 現在の関数スコープの変数を保存
    auto oldFunctionVariables = m_currentFunctionVariables;
    m_currentFunctionVariables.clear();
    
    // 関数名を変換
    auto idResult = transformNode(node->id);
    if (idResult.changed) {
        m_changed = true;
        node->id = std::move(idResult.node);
    }
    
    // パラメータを変換
    bool localChanged = false;
    for (size_t i = 0; i < node->params.size(); ++i) {
        auto paramResult = transformNode(node->params[i]);
        if (paramResult.changed) {
            localChanged = true;
            node->params[i] = std::move(paramResult.node);
        }
    }
    
    // 本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        localChanged = true;
        node->body = std::move(bodyResult.node);
    }
    
    // 関数スコープの変数を復元
    m_currentFunctionVariables = oldFunctionVariables;
    
    m_changed = localChanged;
}

void DeadCodeEliminationTransformer::visitFunctionExpression(ast::FunctionExpression* node) {
    // 現在の関数スコープの変数を保存
    auto oldFunctionVariables = m_currentFunctionVariables;
    m_currentFunctionVariables.clear();
    
    // 関数名があれば変換
    bool localChanged = false;
    if (node->id) {
        auto idResult = transformNode(node->id);
        if (idResult.changed) {
            localChanged = true;
            node->id = std::move(idResult.node);
        }
    }
    
    // パラメータを変換
    for (size_t i = 0; i < node->params.size(); ++i) {
        auto paramResult = transformNode(node->params[i]);
        if (paramResult.changed) {
            localChanged = true;
            node->params[i] = std::move(paramResult.node);
        }
    }
    
    // 本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        localChanged = true;
        node->body = std::move(bodyResult.node);
    }
    
    // 関数スコープの変数を復元
    m_currentFunctionVariables = oldFunctionVariables;
    
    m_changed = localChanged;
}

void DeadCodeEliminationTransformer::visitArrowFunctionExpression(ast::ArrowFunctionExpression* node) {
    // 現在の関数スコープの変数を保存
    auto oldFunctionVariables = m_currentFunctionVariables;
    m_currentFunctionVariables.clear();
    
    // パラメータを変換
    bool localChanged = false;
    for (size_t i = 0; i < node->params.size(); ++i) {
        auto paramResult = transformNode(node->params[i]);
        if (paramResult.changed) {
            localChanged = true;
            node->params[i] = std::move(paramResult.node);
        }
    }
    
    // 本体を変換
    auto bodyResult = transformNode(node->body);
    if (bodyResult.changed) {
        localChanged = true;
        node->body = std::move(bodyResult.node);
    }
    
    // 関数スコープの変数を復元
    m_currentFunctionVariables = oldFunctionVariables;
    
    m_changed = localChanged;
}

void DeadCodeEliminationTransformer::visitLiteral(ast::Literal* node) {
    // リテラルに対する最適化は特にない
}

void DeadCodeEliminationTransformer::visitBinaryExpression(ast::BinaryExpression* node) {
    // 左辺を変換
    auto leftResult = transformNode(node->left);
    if (leftResult.changed) {
        m_changed = true;
        node->left = std::move(leftResult.node);
    }
    
    // 右辺を変換
    auto rightResult = transformNode(node->right);
    if (rightResult.changed) {
        m_changed = true;
        node->right = std::move(rightResult.node);
    }
    
    // 静的に評価可能な二項演算を計算
    ast::LiteralPtr result;
    if (evaluateBinaryExpression(node, result)) {
        m_result = result;
        m_changed = true;
    }
}

void DeadCodeEliminationTransformer::visitLogicalExpression(ast::LogicalExpression* node) {
    // 左辺を変換
    auto leftResult = transformNode(node->left);
    if (leftResult.changed) {
        m_changed = true;
        node->left = std::move(leftResult.node);
    }
    
    // 左辺が静的に評価可能かチェック
    bool leftValue;
    bool canDetermineLeft = evaluatesToTruthy(node->left, leftValue);
    
    if (canDetermineLeft) {
        if (node->operator_ == "&&") {
            if (!leftValue) {
                // left && right で左辺がfalseなら、常にfalse
                m_result = std::make_shared<ast::Literal>(false);
                m_changed = true;
                return;
            } else {
                // left && right で左辺がtrueなら、右辺の値に等しい
                auto rightResult = transformNode(node->right);
                if (rightResult.changed) {
                    m_changed = true;
                    node->right = std::move(rightResult.node);
                }
                m_result = node->right;
                m_changed = true;
                return;
            }
        } else if (node->operator_ == "||") {
            if (leftValue) {
                // left || right で左辺がtrueなら、常にtrue
                m_result = std::make_shared<ast::Literal>(true);
                m_changed = true;
                return;
            } else {
                // left || right で左辺がfalseなら、右辺の値に等しい
                auto rightResult = transformNode(node->right);
                if (rightResult.changed) {
                    m_changed = true;
                    node->right = std::move(rightResult.node);
                }
                m_result = node->right;
                m_changed = true;
                return;
            }
        }
    }
    
    // 右辺を変換
    auto rightResult = transformNode(node->right);
    if (rightResult.changed) {
        m_changed = true;
        node->right = std::move(rightResult.node);
    }
}

void DeadCodeEliminationTransformer::visitUnaryExpression(ast::UnaryExpression* node) {
    // 引数を変換
    auto argumentResult = transformNode(node->argument);
    if (argumentResult.changed) {
        m_changed = true;
        node->argument = std::move(argumentResult.node);
    }
    
    // 静的に評価可能な単項演算を計算
    if (node->operator_ == "!" && node->argument->type == ast::NodeType::Literal) {
        auto* literal = static_cast<ast::Literal*>(node->argument.get());
        if (literal->valueType == ast::LiteralType::Boolean) {
            // !true -> false, !false -> true
            m_result = std::make_shared<ast::Literal>(!literal->booleanValue);
            m_changed = true;
            return;
        } else if (literal->valueType == ast::LiteralType::Null) {
            // !null -> true
            m_result = std::make_shared<ast::Literal>(true);
            m_changed = true;
            return;
        } else if (literal->valueType == ast::LiteralType::Number) {
            // !0 -> true, !非0 -> false
            m_result = std::make_shared<ast::Literal>(literal->numberValue == 0.0);
            m_changed = true;
            return;
        } else if (literal->valueType == ast::LiteralType::String) {
            // !"" -> true, !非"" -> false
            m_result = std::make_shared<ast::Literal>(literal->stringValue.empty());
            m_changed = true;
            return;
        }
    } else if (node->operator_ == "-" && node->argument->type == ast::NodeType::Literal) {
        auto* literal = static_cast<ast::Literal*>(node->argument.get());
        if (literal->valueType == ast::LiteralType::Number) {
            // -数値 -> 符号を反転した数値
            m_result = std::make_shared<ast::Literal>(-literal->numberValue);
            m_changed = true;
            return;
        }
    } else if (node->operator_ == "+" && node->argument->type == ast::NodeType::Literal) {
        auto* literal = static_cast<ast::Literal*>(node->argument.get());
        if (literal->valueType == ast::LiteralType::Number) {
            // +数値 -> そのまま
            m_result = node->argument;
            m_changed = true;
            return;
        } else if (literal->valueType == ast::LiteralType::String) {
            // +文字列 -> 文字列を数値に変換
            try {
                double value = std::stod(literal->stringValue);
                m_result = std::make_shared<ast::Literal>(value);
                m_changed = true;
                return;
            } catch (...) {
                // 変換できない場合はNaN
                m_result = std::make_shared<ast::Literal>(std::numeric_limits<double>::quiet_NaN());
                m_changed = true;
                return;
            }
        } else if (literal->valueType == ast::LiteralType::Boolean) {
            // +true -> 1, +false -> 0
            m_result = std::make_shared<ast::Literal>(literal->booleanValue ? 1.0 : 0.0);
            m_changed = true;
            return;
        } else if (literal->valueType == ast::LiteralType::Null) {
            // +null -> 0
            m_result = std::make_shared<ast::Literal>(0.0);
            m_changed = true;
            return;
        }
    }
}

void DeadCodeEliminationTransformer::visitConditionalExpression(ast::ConditionalExpression* node) {
    // 条件式を変換
    auto testResult = transformNode(node->test);
    if (testResult.changed) {
        m_changed = true;
        node->test = std::move(testResult.node);
    }
    
    // 条件式が静的に評価可能かチェック
    bool conditionValue;
    bool canDetermine = evaluatesToTruthy(node->test, conditionValue);
    
    if (canDetermine) {
        if (conditionValue) {
            // 条件がtrueの場合、consequent部分だけを変換して結果とする
            auto consequentResult = transformNode(node->consequent);
            if (consequentResult.changed) {
                m_changed = true;
                node->consequent = std::move(consequentResult.node);
            }
            m_result = node->consequent;
            m_changed = true;
            return;
        } else {
            // 条件がfalseの場合、alternate部分だけを変換して結果とする
            auto alternateResult = transformNode(node->alternate);
            if (alternateResult.changed) {
                m_changed = true;
                node->alternate = std::move(alternateResult.node);
            }
            m_result = node->alternate;
            m_changed = true;
            return;
        }
    }
    
    // 条件式が静的に評価できない場合は通常の処理を続行
    
    // then部分を変換
    auto consequentResult = transformNode(node->consequent);
    if (consequentResult.changed) {
        m_changed = true;
        node->consequent = std::move(consequentResult.node);
    }
    
    // else部分を変換
    auto alternateResult = transformNode(node->alternate);
    if (alternateResult.changed) {
        m_changed = true;
        node->alternate = std::move(alternateResult.node);
    }
}

bool DeadCodeEliminationTransformer::hasSideEffects(ast::NodePtr node) {
    if (!node) {
        return false;
    }
    
    switch (node->type) {
        case ast::NodeType::CallExpression:
        case ast::NodeType::NewExpression:
        case ast::NodeType::AssignmentExpression:
        case ast::NodeType::UpdateExpression:
        case ast::NodeType::AwaitExpression:
        case ast::NodeType::YieldExpression:
        case ast::NodeType::ThrowStatement:
            // これらの式は副作用を持つ
            return true;
            
        case ast::NodeType::UnaryExpression: {
            auto* unary = static_cast<ast::UnaryExpression*>(node.get());
            // delete演算子は副作用を持つ
            if (unary->operator_ == "delete") {
                return true;
            }
            // その他の単項演算子は引数に依存
            return hasSideEffects(unary->argument);
        }
        
        case ast::NodeType::BinaryExpression: {
            auto* binary = static_cast<ast::BinaryExpression*>(node.get());
            // 左辺または右辺に副作用があるかチェック
            return hasSideEffects(binary->left) || hasSideEffects(binary->right);
        }
        
        case ast::NodeType::LogicalExpression: {
            auto* logical = static_cast<ast::LogicalExpression*>(node.get());
            // 左辺または右辺に副作用があるかチェック
            return hasSideEffects(logical->left) || hasSideEffects(logical->right);
        }
        
        case ast::NodeType::ConditionalExpression: {
            auto* conditional = static_cast<ast::ConditionalExpression*>(node.get());
            // 条件式、then式、else式のいずれかに副作用があるかチェック
            return hasSideEffects(conditional->test) ||
                   hasSideEffects(conditional->consequent) ||
                   hasSideEffects(conditional->alternate);
        }
        
        case ast::NodeType::SequenceExpression: {
            auto* sequence = static_cast<ast::SequenceExpression*>(node.get());
            // 式のいずれかに副作用があるかチェック
            for (const auto& expr : sequence->expressions) {
                if (hasSideEffects(expr)) {
                    return true;
                }
            }
            return false;
        }
        
        default:
            // その他のノードタイプは副作用がないと判断
            return false;
    }
}

bool DeadCodeEliminationTransformer::evaluatesToTruthy(ast::NodePtr node, bool& result) {
    if (!node) {
        return false;
    }
    
    if (node->type == ast::NodeType::Literal) {
        auto* literal = static_cast<ast::Literal*>(node.get());
        switch (literal->valueType) {
            case ast::LiteralType::Boolean:
                result = literal->booleanValue;
                return true;
                
            case ast::LiteralType::Number:
                // 0と非0で真偽値が決まる
                result = literal->numberValue != 0.0 && !std::isnan(literal->numberValue);
                return true;
                
            case ast::LiteralType::String:
                // 空文字と非空文字で真偽値が決まる
                result = !literal->stringValue.empty();
                return true;
                
            case ast::LiteralType::Null:
                // nullはfalseと評価される
                result = false;
                return true;
                
            default:
                return false;
        }
    } else if (node->type == ast::NodeType::UnaryExpression) {
        auto* unary = static_cast<ast::UnaryExpression*>(node.get());
        if (unary->operator_ == "!") {
            // !式の場合、式を評価して結果を反転
            bool innerResult;
            if (evaluatesToTruthy(unary->argument, innerResult)) {
                result = !innerResult;
                return true;
            }
        }
    } else if (node->type == ast::NodeType::BinaryExpression) {
        // 二項演算の結果を評価
        ast::LiteralPtr literalResult;
        if (evaluateBinaryExpression(static_cast<ast::BinaryExpression*>(node.get()), literalResult)) {
            return evaluatesToTruthy(literalResult, result);
        }
    }
    
    // 静的に評価できない
    return false;
}
void DeadCodeEliminationTransformer::collectUsedVariables(ast::NodePtr node, std::unordered_set<std::string>& usedVariables) {
    if (!node) {
        return;
    }
    
    switch (node->type) {
        case ast::NodeType::Identifier: {
            auto* identifier = static_cast<ast::Identifier*>(node.get());
            usedVariables.insert(identifier->name);
            break;
        }
        case ast::NodeType::MemberExpression: {
            auto* memberExpr = static_cast<ast::MemberExpression*>(node.get());
            // オブジェクトの変数を収集
            collectUsedVariables(memberExpr->object, usedVariables);
            // 計算されたプロパティの場合、そのプロパティ式内の変数も収集
            if (memberExpr->computed) {
                collectUsedVariables(memberExpr->property, usedVariables);
            }
            break;
        }
        case ast::NodeType::CallExpression: {
            auto* callExpr = static_cast<ast::CallExpression*>(node.get());
            // 関数自体の変数を収集
            collectUsedVariables(callExpr->callee, usedVariables);
            // 引数内の変数を収集
            for (const auto& arg : callExpr->arguments) {
                collectUsedVariables(arg, usedVariables);
            }
            break;
        }
        case ast::NodeType::BinaryExpression: {
            auto* binaryExpr = static_cast<ast::BinaryExpression*>(node.get());
            collectUsedVariables(binaryExpr->left, usedVariables);
            collectUsedVariables(binaryExpr->right, usedVariables);
            break;
        }
        case ast::NodeType::UnaryExpression: {
            auto* unaryExpr = static_cast<ast::UnaryExpression*>(node.get());
            collectUsedVariables(unaryExpr->argument, usedVariables);
            break;
        }
        case ast::NodeType::ConditionalExpression: {
            auto* condExpr = static_cast<ast::ConditionalExpression*>(node.get());
            collectUsedVariables(condExpr->test, usedVariables);
            collectUsedVariables(condExpr->consequent, usedVariables);
            collectUsedVariables(condExpr->alternate, usedVariables);
            break;
        }
        case ast::NodeType::AssignmentExpression: {
            auto* assignExpr = static_cast<ast::AssignmentExpression*>(node.get());
            // 右辺の変数を収集
            collectUsedVariables(assignExpr->right, usedVariables);
            // 左辺が識別子でない場合（例：obj.prop = value）、その中の変数も収集
            if (assignExpr->left->type != ast::NodeType::Identifier) {
                collectUsedVariables(assignExpr->left, usedVariables);
            }
            break;
        }
        case ast::NodeType::ArrayExpression: {
            auto* arrayExpr = static_cast<ast::ArrayExpression*>(node.get());
            for (const auto& element : arrayExpr->elements) {
                if (element) { // スパース配列の場合、nullの要素がある可能性がある
                    collectUsedVariables(element, usedVariables);
                }
            }
            break;
        }
        case ast::NodeType::ObjectExpression: {
            auto* objExpr = static_cast<ast::ObjectExpression*>(node.get());
            for (const auto& prop : objExpr->properties) {
                // 計算されたプロパティキーの場合
                if (prop->computed) {
                    collectUsedVariables(prop->key, usedVariables);
                }
                // プロパティ値内の変数を収集
                collectUsedVariables(prop->value, usedVariables);
            }
            break;
        }
        case ast::NodeType::SequenceExpression: {
            auto* seqExpr = static_cast<ast::SequenceExpression*>(node.get());
            for (const auto& expr : seqExpr->expressions) {
                collectUsedVariables(expr, usedVariables);
            }
            break;
        }
        case ast::NodeType::UpdateExpression: {
            auto* updateExpr = static_cast<ast::UpdateExpression*>(node.get());
            collectUsedVariables(updateExpr->argument, usedVariables);
            break;
        }
        case ast::NodeType::LogicalExpression: {
            auto* logicalExpr = static_cast<ast::LogicalExpression*>(node.get());
            collectUsedVariables(logicalExpr->left, usedVariables);
            collectUsedVariables(logicalExpr->right, usedVariables);
            break;
        }
        case ast::NodeType::TemplateLiteral: {
            auto* templateLit = static_cast<ast::TemplateLiteral*>(node.get());
            for (const auto& expr : templateLit->expressions) {
                collectUsedVariables(expr, usedVariables);
            }
            break;
        }
        case ast::NodeType::TaggedTemplateExpression: {
            auto* taggedExpr = static_cast<ast::TaggedTemplateExpression*>(node.get());
            collectUsedVariables(taggedExpr->tag, usedVariables);
            collectUsedVariables(taggedExpr->quasi, usedVariables);
            break;
        }
        case ast::NodeType::ArrowFunctionExpression:
        case ast::NodeType::FunctionExpression: {
            // 関数式内のスコープは別途処理するため、ここでは収集しない
            // ただし、クロージャとして外部変数を参照する場合は別途解析が必要
            break;
        }
        // リテラルや定数などの場合は変数参照がないのでスキップ
        case ast::NodeType::Literal:
        case ast::NodeType::ThisExpression:
        case ast::NodeType::Super:
            break;
        // その他のノードタイプに対する処理
        default:
            // 子ノードがある可能性のあるその他のノードタイプを処理
            if (auto* block = node->asBlock()) {
                for (const auto& stmt : block->body) {
                    collectUsedVariables(stmt, usedVariables);
                }
            } else if (auto* program = node->asProgram()) {
                for (const auto& stmt : program->body) {
                    collectUsedVariables(stmt, usedVariables);
                }
            } else if (auto* ifStmt = node->asIfStatement()) {
                collectUsedVariables(ifStmt->test, usedVariables);
                collectUsedVariables(ifStmt->consequent, usedVariables);
                if (ifStmt->alternate) {
                    collectUsedVariables(ifStmt->alternate, usedVariables);
                }
            } else if (auto* loopStmt = node->asLoopStatement()) {
                // 各種ループ文の条件式と本体を処理
                if (loopStmt->test) {
                    collectUsedVariables(loopStmt->test, usedVariables);
                }
                if (loopStmt->body) {
                    collectUsedVariables(loopStmt->body, usedVariables);
                }
                if (auto* forStmt = node->asForStatement()) {
                    if (forStmt->init) {
                        collectUsedVariables(forStmt->init, usedVariables);
                    }
                    if (forStmt->update) {
                        collectUsedVariables(forStmt->update, usedVariables);
                    }
                }
            } else if (auto* switchStmt = node->asSwitchStatement()) {
                collectUsedVariables(switchStmt->discriminant, usedVariables);
                for (const auto& caseClause : switchStmt->cases) {
                    if (caseClause->test) {
                        collectUsedVariables(caseClause->test, usedVariables);
                    }
                    for (const auto& stmt : caseClause->consequent) {
                        collectUsedVariables(stmt, usedVariables);
                    }
                }
            } else if (auto* tryStmt = node->asTryStatement()) {
                collectUsedVariables(tryStmt->block, usedVariables);
                if (tryStmt->handler) {
                    collectUsedVariables(tryStmt->handler->body, usedVariables);
                }
                if (tryStmt->finalizer) {
                    collectUsedVariables(tryStmt->finalizer, usedVariables);
                }
            }
            break;
    }
}

bool DeadCodeEliminationTransformer::evaluateBinaryExpression(ast::BinaryExpression* node, ast::LiteralPtr& result) {
    // リテラル同士の演算のみを静的に評価
    if (node->left->type != ast::NodeType::Literal || node->right->type != ast::NodeType::Literal) {
        return false;
    }
    
    auto* left = static_cast<ast::Literal*>(node->left.get());
    auto* right = static_cast<ast::Literal*>(node->right.get());
    
    // 数値演算
    if (left->valueType == ast::LiteralType::Number && right->valueType == ast::LiteralType::Number) {
        double leftValue = left->numberValue;
        double rightValue = right->numberValue;
        
        if (node->operator_ == "+") {
            result = std::make_shared<ast::Literal>(leftValue + rightValue);
            return true;
        } else if (node->operator_ == "-") {
            result = std::make_shared<ast::Literal>(leftValue - rightValue);
            return true;
        } else if (node->operator_ == "*") {
            result = std::make_shared<ast::Literal>(leftValue * rightValue);
            return true;
        } else if (node->operator_ == "/") {
            if (rightValue == 0.0) {
                // ゼロ除算の場合は静的評価しない
                return false;
            }
            result = std::make_shared<ast::Literal>(leftValue / rightValue);
            return true;
        } else if (node->operator_ == "%") {
            if (rightValue == 0.0) {
                // ゼロ除算の場合は静的評価しない
                return false;
            }
            result = std::make_shared<ast::Literal>(std::fmod(leftValue, rightValue));
            return true;
        } else if (node->operator_ == "**") {
            result = std::make_shared<ast::Literal>(std::pow(leftValue, rightValue));
            return true;
        } else if (node->operator_ == "<") {
            result = std::make_shared<ast::Literal>(leftValue < rightValue);
            return true;
        } else if (node->operator_ == ">") {
            result = std::make_shared<ast::Literal>(leftValue > rightValue);
            return true;
        } else if (node->operator_ == "<=") {
            result = std::make_shared<ast::Literal>(leftValue <= rightValue);
            return true;
        } else if (node->operator_ == ">=") {
            result = std::make_shared<ast::Literal>(leftValue >= rightValue);
            return true;
        } else if (node->operator_ == "==") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!=") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        } else if (node->operator_ == "===") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!==") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        }
    }
    
    // 文字列演算
    if (left->valueType == ast::LiteralType::String && right->valueType == ast::LiteralType::String) {
        const std::string& leftValue = left->stringValue;
        const std::string& rightValue = right->stringValue;
        
        if (node->operator_ == "+") {
            result = std::make_shared<ast::Literal>(leftValue + rightValue);
            return true;
        } else if (node->operator_ == "==") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!=") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        } else if (node->operator_ == "===") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!==") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        }
    }
    
    // ブール演算
    if (left->valueType == ast::LiteralType::Boolean && right->valueType == ast::LiteralType::Boolean) {
        bool leftValue = left->booleanValue;
        bool rightValue = right->booleanValue;
        
        if (node->operator_ == "==") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!=") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        } else if (node->operator_ == "===") {
            result = std::make_shared<ast::Literal>(leftValue == rightValue);
            return true;
        } else if (node->operator_ == "!==") {
            result = std::make_shared<ast::Literal>(leftValue != rightValue);
            return true;
        }
    }
    
    // null演算
    if (left->valueType == ast::LiteralType::Null || right->valueType == ast::LiteralType::Null) {
        if (node->operator_ == "==") {
            bool result_val = (left->valueType == ast::LiteralType::Null && right->valueType == ast::LiteralType::Null);
            result = std::make_shared<ast::Literal>(result_val);
            return true;
        } else if (node->operator_ == "!=") {
            bool result_val = !(left->valueType == ast::LiteralType::Null && right->valueType == ast::LiteralType::Null);
            result = std::make_shared<ast::Literal>(result_val);
            return true;
        } else if (node->operator_ == "===") {
            bool result_val = (left->valueType == ast::LiteralType::Null && right->valueType == ast::LiteralType::Null);
            result = std::make_shared<ast::Literal>(result_val);
            return true;
        } else if (node->operator_ == "!==") {
            bool result_val = !(left->valueType == ast::LiteralType::Null && right->valueType == ast::LiteralType::Null);
            result = std::make_shared<ast::Literal>(result_val);
            return true;
        }
    }
    
    // 静的に評価できない
    return false;
}

bool DeadCodeEliminationTransformer::removeUnreachableCode(std::vector<ast::NodePtr>& statements) {
    // 到達不能コードを検出・削除する
    bool unreachableFound = false;
    bool changed = false;
    
    for (auto it = statements.begin(); it != statements.end(); ) {
        if (unreachableFound) {
            // 既に到達不能コードが見つかっている場合、それ以降のコードを削除
            it = statements.erase(it);
            changed = true;
        } else {
            // return, throw, breakなどの制御フロー終了ステートメントをチェック
            ast::NodePtr& stmt = *it;
            
            // リセット
            m_unreachableCodeDetected = false;
            
            // ステートメントを変換
            auto result = transformNode(stmt);
            if (result.changed) {
                stmt = std::move(result.node);
                changed = true;
            }
            
            // 到達不能コードが検出されたかチェック
            unreachableFound = m_unreachableCodeDetected;
            
            // 次のステートメントへ
            ++it;
        }
    }
    
    return changed;
}

} // namespace transformers
} // namespace aero 