/**
 * @file type_inference.cpp
 * @brief 型推論トランスフォーマーの実装
 * @version 0.1
 * @date 2024-01-11
 * 
 * @copyright Copyright (c) 2024 AeroJS Project
 * 
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "type_inference.h"
#include "../ast/ast.h"
#include "../common/debug.h"
#include "../common/util.h"
#include <unordered_set>
#include <algorithm>
#include <sstream>

namespace aero {
namespace transformers {

TypeInferenceTransformer::TypeInferenceTransformer()
    : m_enableStatistics(false)
    , m_inferredTypes(0)
    , m_inferredVariables(0)
    , m_typeMismatchWarnings(0) {
}

TypeInferenceTransformer::~TypeInferenceTransformer() {
}

void TypeInferenceTransformer::enableStatistics(bool enable) {
    m_enableStatistics = enable;
}

int TypeInferenceTransformer::getInferredTypeCount() const {
    return m_inferredTypes;
}

int TypeInferenceTransformer::getInferredVariableCount() const {
    return m_inferredVariables;
}

int TypeInferenceTransformer::getTypeMismatchWarningCount() const {
    return m_typeMismatchWarnings;
}

void TypeInferenceTransformer::resetCounters() {
    m_inferredTypes = 0;
    m_inferredVariables = 0;
    m_typeMismatchWarnings = 0;
    m_typeCache.clear();
    m_scopeStack.clear();
}

ast::NodePtr TypeInferenceTransformer::transform(const ast::NodePtr& node) {
    if (!node) {
        return nullptr;
    }

    // スコープスタックをクリア
    m_scopeStack.clear();
    
    // ルートスコープを作成
    enterScope();
    
    // ノードを訪問して型推論を実行
    ast::NodePtr result = visit(node);
    
    // スコープから出る
    exitScope();
    
    return result;
}

ast::NodePtr TypeInferenceTransformer::visitProgram(const ast::NodePtr& node) {
    auto program = std::static_pointer_cast<ast::Program>(node);
    
    // プログラム内のすべての宣言をスキャンして初期型情報を収集
    scanDeclarations(program);
    
    // 各ステートメントを訪問
    for (auto& stmt : program->body) {
        stmt = visit(stmt);
    }
    
    return program;
}

ast::NodePtr TypeInferenceTransformer::visitBlockStatement(const ast::NodePtr& node) {
    auto block = std::static_pointer_cast<ast::BlockStatement>(node);
    
    // 新しいスコープに入る
    enterScope();
    
    // ブロック内のすべてのステートメントを訪問
    for (auto& stmt : block->body) {
        stmt = visit(stmt);
    }
    
    // スコープから出る
    exitScope();
    
    return block;
}

ast::NodePtr TypeInferenceTransformer::visitExpressionStatement(const ast::NodePtr& node) {
    auto expr = std::static_pointer_cast<ast::ExpressionStatement>(node);
    expr->expression = visit(expr->expression);
    return expr;
}

ast::NodePtr TypeInferenceTransformer::visitIfStatement(const ast::NodePtr& node) {
    auto ifStmt = std::static_pointer_cast<ast::IfStatement>(node);
    
    // 条件式を訪問
    ifStmt->test = visit(ifStmt->test);
    
    // テスト式が定数の場合、Dead Code Eliminationに任せるため変更せず返す
    if (isConstant(ifStmt->test)) {
        return ifStmt;
    }
    
    // thenブロックを訪問
    ifStmt->consequent = visit(ifStmt->consequent);
    
    // elseブロックがある場合は訪問
    if (ifStmt->alternate) {
        ifStmt->alternate = visit(ifStmt->alternate);
    }
    
    return ifStmt;
}

ast::NodePtr TypeInferenceTransformer::visitReturnStatement(const ast::NodePtr& node) {
    auto returnStmt = std::static_pointer_cast<ast::ReturnStatement>(node);
    
    if (returnStmt->argument) {
        returnStmt->argument = visit(returnStmt->argument);
        
        // 現在の関数のreturn型を更新
        if (!m_currentFunctionTypeInfo.empty()) {
            TypeInfo argType = inferType(returnStmt->argument);
            std::string funcId = m_currentFunctionTypeInfo.back();
            
            auto it = m_typeCache.find(funcId);
            if (it != m_typeCache.end()) {
                TypeInfo& funcType = it->second;
                
                // 関数の戻り値型を更新
                if (funcType.returnType.kind == TypeKind::Unknown) {
                    funcType.returnType = argType;
                } else {
                    // 複数のreturn文がある場合は型を統合
                    funcType.returnType = mergeTypes(funcType.returnType, argType);
                }
            }
        }
    }
    
    return returnStmt;
}

ast::NodePtr TypeInferenceTransformer::visitFunctionDeclaration(const ast::NodePtr& node) {
    auto func = std::static_pointer_cast<ast::FunctionDeclaration>(node);
    
    // 関数の一意なIDを生成
    std::string funcId = "func:" + func->id->name;
    
    // 新しい型情報を作成
    TypeInfo funcTypeInfo;
    funcTypeInfo.kind = TypeKind::Function;
    funcTypeInfo.returnType.kind = TypeKind::Unknown;
    
    // パラメータの型情報を初期化
    for (const auto& param : func->params) {
        if (param->type == ast::NodeType::Identifier) {
            auto identifier = std::static_pointer_cast<ast::Identifier>(param);
            funcTypeInfo.paramTypes[identifier->name].kind = TypeKind::Unknown;
        }
    }
    
    // 関数の型情報をキャッシュに追加
    m_typeCache[funcId] = funcTypeInfo;
    
    // 現在の関数コンテキストを保存
    m_currentFunctionTypeInfo.push_back(funcId);
    
    // 新しいスコープに入る
    enterScope();
    
    // パラメータをスコープに追加
    for (const auto& param : func->params) {
        if (param->type == ast::NodeType::Identifier) {
            auto identifier = std::static_pointer_cast<ast::Identifier>(param);
            m_scopeStack.back()[identifier->name] = TypeInfo{TypeKind::Unknown};
            
            if (m_enableStatistics) {
                m_inferredVariables++;
            }
        }
    }
    
    // 関数本体を訪問
    if (func->body && func->body->type == ast::NodeType::BlockStatement) {
        func->body = visit(func->body);
    }
    
    // スコープから出る
    exitScope();
    
    // 関数コンテキストから出る
    m_currentFunctionTypeInfo.pop_back();
    
    return func;
}

ast::NodePtr TypeInferenceTransformer::visitVariableDeclaration(const ast::NodePtr& node) {
    auto varDecl = std::static_pointer_cast<ast::VariableDeclaration>(node);
    
    for (auto& declarator : varDecl->declarations) {
        if (declarator->type == ast::NodeType::VariableDeclarator) {
            auto decl = std::static_pointer_cast<ast::VariableDeclarator>(declarator);
            
            // 初期化式がある場合は訪問
            if (decl->init) {
                decl->init = visit(decl->init);
                
                // 識別子の場合、型情報を追加
                if (decl->id->type == ast::NodeType::Identifier) {
                    auto id = std::static_pointer_cast<ast::Identifier>(decl->id);
                    TypeInfo typeInfo = inferType(decl->init);
                    
                    // スコープに型情報を追加
                    if (!m_scopeStack.empty()) {
                        m_scopeStack.back()[id->name] = typeInfo;
                        
                        if (m_enableStatistics) {
                            m_inferredVariables++;
                            if (typeInfo.kind != TypeKind::Unknown) {
                                m_inferredTypes++;
                            }
                        }
                    }
                }
            } else {
                // 初期化式がない場合、未知の型として登録
                if (decl->id->type == ast::NodeType::Identifier) {
                    auto id = std::static_pointer_cast<ast::Identifier>(decl->id);
                    
                    // スコープに型情報を追加
                    if (!m_scopeStack.empty()) {
                        m_scopeStack.back()[id->name] = TypeInfo{TypeKind::Unknown};
                        
                        if (m_enableStatistics) {
                            m_inferredVariables++;
                        }
                    }
                }
            }
        }
    }
    
    return varDecl;
}

ast::NodePtr TypeInferenceTransformer::visitAssignmentExpression(const ast::NodePtr& node) {
    auto assign = std::static_pointer_cast<ast::AssignmentExpression>(node);
    
    // 右辺を訪問
    assign->right = visit(assign->right);
    
    // 左辺が識別子の場合、型情報を更新
    if (assign->left->type == ast::NodeType::Identifier) {
        auto id = std::static_pointer_cast<ast::Identifier>(assign->left);
        TypeInfo typeInfo = inferType(assign->right);
        
        // 変数の型情報を更新
        updateVariableType(id->name, typeInfo);
    } else {
        // それ以外の左辺（MemberExpressionなど）は訪問
        assign->left = visit(assign->left);
    }
    
    return assign;
}

ast::NodePtr TypeInferenceTransformer::visitBinaryExpression(const ast::NodePtr& node) {
    auto binary = std::static_pointer_cast<ast::BinaryExpression>(node);
    
    // 左辺と右辺を訪問
    binary->left = visit(binary->left);
    binary->right = visit(binary->right);
    
    // 型情報に基づく最適化
    TypeInfo leftType = inferType(binary->left);
    TypeInfo rightType = inferType(binary->right);
    
    // 数値同士の演算の場合
    if (leftType.kind == TypeKind::Number && rightType.kind == TypeKind::Number) {
        // 型情報をキャッシュに追加
        cacheTypeForNode(binary, TypeInfo{TypeKind::Number});
    } 
    // 文字列連結の場合
    else if (binary->operator_ == "+" && 
            (leftType.kind == TypeKind::String || rightType.kind == TypeKind::String)) {
        cacheTypeForNode(binary, TypeInfo{TypeKind::String});
    }
    // 比較演算子の場合
    else if (binary->operator_ == "==" || binary->operator_ == "!=" || 
             binary->operator_ == "===" || binary->operator_ == "!==" ||
             binary->operator_ == "<" || binary->operator_ == "<=" || 
             binary->operator_ == ">" || binary->operator_ == ">=") {
        cacheTypeForNode(binary, TypeInfo{TypeKind::Boolean});
    }
    
    return binary;
}

ast::NodePtr TypeInferenceTransformer::visitUnaryExpression(const ast::NodePtr& node) {
    auto unary = std::static_pointer_cast<ast::UnaryExpression>(node);
    
    // 引数を訪問
    unary->argument = visit(unary->argument);
    
    // 型情報に基づく最適化
    TypeInfo argType = inferType(unary->argument);
    
    // 一項演算子による型の変換
    if (unary->operator_ == "!") {
        cacheTypeForNode(unary, TypeInfo{TypeKind::Boolean});
    } else if (unary->operator_ == "+" || unary->operator_ == "-") {
        // 数値へのキャスト試行
        cacheTypeForNode(unary, TypeInfo{TypeKind::Number});
    } else if (unary->operator_ == "typeof") {
        cacheTypeForNode(unary, TypeInfo{TypeKind::String});
    }
    
    return unary;
}

ast::NodePtr TypeInferenceTransformer::visitCallExpression(const ast::NodePtr& node) {
    auto call = std::static_pointer_cast<ast::CallExpression>(node);
    
    // 呼び出し対象を訪問
    call->callee = visit(call->callee);
    
    // 引数を訪問
    for (auto& arg : call->arguments) {
        arg = visit(arg);
    }
    
    // 特定の組み込み関数の戻り値型を推論
    if (call->callee->type == ast::NodeType::MemberExpression) {
        auto member = std::static_pointer_cast<ast::MemberExpression>(call->callee);
        
        // Math.系関数の場合
        if (member->object->type == ast::NodeType::Identifier) {
            auto obj = std::static_pointer_cast<ast::Identifier>(member->object);
            
            if (obj->name == "Math" && member->property->type == ast::NodeType::Identifier) {
                auto prop = std::static_pointer_cast<ast::Identifier>(member->property);
                
                // 数値を返すMath関数
                std::unordered_set<std::string> numericFunctions = {
                    "abs", "acos", "acosh", "asin", "asinh", "atan", "atanh", "cbrt",
                    "ceil", "cos", "cosh", "exp", "floor", "log", "log10", "log2",
                    "max", "min", "pow", "random", "round", "sign", "sin", "sinh",
                    "sqrt", "tan", "tanh", "trunc"
                };
                
                if (numericFunctions.find(prop->name) != numericFunctions.end()) {
                    cacheTypeForNode(call, TypeInfo{TypeKind::Number});
                }
            }
        }
    } else if (call->callee->type == ast::NodeType::Identifier) {
        auto id = std::static_pointer_cast<ast::Identifier>(call->callee);
        
        // parseInt, parseFloat関数の場合
        if (id->name == "parseInt" || id->name == "parseFloat") {
            cacheTypeForNode(call, TypeInfo{TypeKind::Number});
        }
        // String, Number, Boolean関数の場合
        else if (id->name == "String") {
            cacheTypeForNode(call, TypeInfo{TypeKind::String});
        }
        else if (id->name == "Number") {
            cacheTypeForNode(call, TypeInfo{TypeKind::Number});
        }
        else if (id->name == "Boolean") {
            cacheTypeForNode(call, TypeInfo{TypeKind::Boolean});
        }
        // カスタム関数呼び出しの場合、戻り値型を検索
        else {
            TypeInfo funcType = lookupFunctionType(id->name);
            if (funcType.kind == TypeKind::Function) {
                cacheTypeForNode(call, funcType.returnType);
            }
        }
    }
    
    return call;
}

ast::NodePtr TypeInferenceTransformer::visitObjectExpression(const ast::NodePtr& node) {
    auto obj = std::static_pointer_cast<ast::ObjectExpression>(node);
    
    // 各プロパティを訪問
    for (auto& prop : obj->properties) {
        if (prop->type == ast::NodeType::Property) {
            auto property = std::static_pointer_cast<ast::Property>(prop);
            property->value = visit(property->value);
        }
    }
    
    // オブジェクト型を登録
    TypeInfo objType;
    objType.kind = TypeKind::Object;
    
    // プロパティの型情報を収集
    for (const auto& prop : obj->properties) {
        if (prop->type == ast::NodeType::Property) {
            auto property = std::static_pointer_cast<ast::Property>(prop);
            
            // プロパティ名を取得
            std::string propName;
            
            if (property->key->type == ast::NodeType::Identifier) {
                auto id = std::static_pointer_cast<ast::Identifier>(property->key);
                propName = id->name;
            } else if (property->key->type == ast::NodeType::Literal) {
                auto lit = std::static_pointer_cast<ast::Literal>(property->key);
                if (lit->literalType == ast::LiteralType::String) {
                    propName = lit->stringValue;
                }
            }
            
            // プロパティの型を推論して登録
            if (!propName.empty()) {
                objType.memberTypes[propName] = inferType(property->value);
            }
        }
    }
    
    cacheTypeForNode(obj, objType);
    
    return obj;
}

ast::NodePtr TypeInferenceTransformer::visitArrayExpression(const ast::NodePtr& node) {
    auto array = std::static_pointer_cast<ast::ArrayExpression>(node);
    
    // 各要素を訪問
    for (auto& elem : array->elements) {
        if (elem) {
            elem = visit(elem);
        }
    }
    
    // 配列型を登録
    TypeInfo arrayType;
    arrayType.kind = TypeKind::Array;
    
    // 均一な型を持つ場合、要素の型を登録
    if (!array->elements.empty()) {
        bool uniform = true;
        TypeInfo firstType;
        
        // 最初の非nullish要素を見つける
        for (const auto& elem : array->elements) {
            if (elem) {
                firstType = inferType(elem);
                break;
            }
        }
        
        // すべての要素が同じ型かチェック
        for (const auto& elem : array->elements) {
            if (elem) {
                TypeInfo elemType = inferType(elem);
                if (!areTypesSame(elemType, firstType)) {
                    uniform = false;
                    break;
                }
            }
        }
        
        if (uniform) {
            arrayType.elementType = firstType;
        }
    }
    
    cacheTypeForNode(array, arrayType);
    
    return array;
}

ast::NodePtr TypeInferenceTransformer::visitMemberExpression(const ast::NodePtr& node) {
    auto member = std::static_pointer_cast<ast::MemberExpression>(node);
    
    // オブジェクトを訪問
    member->object = visit(member->object);
    
    // 計算されたプロパティの場合、プロパティも訪問
    if (member->computed) {
        member->property = visit(member->property);
    }
    
    // オブジェクトの型情報に基づいてプロパティの型を推論
    TypeInfo objType = inferType(member->object);
    
    if (objType.kind == TypeKind::Object && !member->computed) {
        // プロパティ名を取得
        std::string propName;
        
        if (member->property->type == ast::NodeType::Identifier) {
            auto id = std::static_pointer_cast<ast::Identifier>(member->property);
            propName = id->name;
        }
        
        // オブジェクトのプロパティ型情報を検索
        if (!propName.empty() && objType.memberTypes.find(propName) != objType.memberTypes.end()) {
            TypeInfo propType = objType.memberTypes[propName];
            cacheTypeForNode(member, propType);
        }
    } else if (objType.kind == TypeKind::Array && member->computed) {
        // 配列の要素アクセスの場合は要素の型を使用
        cacheTypeForNode(member, objType.elementType);
    }
    
    return member;
}

ast::NodePtr TypeInferenceTransformer::visitIdentifier(const ast::NodePtr& node) {
    auto id = std::static_pointer_cast<ast::Identifier>(node);
    
    // 識別子の型を検索
    TypeInfo typeInfo = lookupVariableType(id->name);
    
    // キャッシュに追加
    if (typeInfo.kind != TypeKind::Unknown) {
        cacheTypeForNode(id, typeInfo);
    }
    
    return id;
}

ast::NodePtr TypeInferenceTransformer::visitLiteral(const ast::NodePtr& node) {
    auto literal = std::static_pointer_cast<ast::Literal>(node);
    
    // リテラルの型を登録
    TypeInfo typeInfo;
    
    switch (literal->literalType) {
        case ast::LiteralType::Number:
            typeInfo.kind = TypeKind::Number;
            break;
        case ast::LiteralType::String:
            typeInfo.kind = TypeKind::String;
            break;
        case ast::LiteralType::Boolean:
            typeInfo.kind = TypeKind::Boolean;
            break;
        case ast::LiteralType::Null:
            typeInfo.kind = TypeKind::Null;
            break;
        case ast::LiteralType::Undefined:
            typeInfo.kind = TypeKind::Undefined;
            break;
        case ast::LiteralType::RegExp:
            typeInfo.kind = TypeKind::Object; // RegExpはオブジェクト
            break;
        default:
            typeInfo.kind = TypeKind::Unknown;
            break;
    }
    
    cacheTypeForNode(literal, typeInfo);
    
    return literal;
}

void TypeInferenceTransformer::enterScope() {
    m_scopeStack.push_back(std::unordered_map<std::string, TypeInfo>());
}

void TypeInferenceTransformer::exitScope() {
    if (!m_scopeStack.empty()) {
        m_scopeStack.pop_back();
    }
}

TypeInfo TypeInferenceTransformer::inferType(const ast::NodePtr& node) {
    if (!node) {
        return TypeInfo{TypeKind::Unknown};
    }
    
    // ノードのキャッシュされた型を確認
    std::stringstream ss;
    ss << node->type << ":" << node.get();
    std::string nodeId = ss.str();
    
    auto it = m_typeCache.find(nodeId);
    if (it != m_typeCache.end()) {
        return it->second;
    }
    
    // 型を推論
    TypeInfo typeInfo;
    
    switch (node->type) {
        case ast::NodeType::Literal: {
            auto literal = std::static_pointer_cast<ast::Literal>(node);
            
            switch (literal->literalType) {
                case ast::LiteralType::Number:
                    typeInfo.kind = TypeKind::Number;
                    break;
                case ast::LiteralType::String:
                    typeInfo.kind = TypeKind::String;
                    break;
                case ast::LiteralType::Boolean:
                    typeInfo.kind = TypeKind::Boolean;
                    break;
                case ast::LiteralType::Null:
                    typeInfo.kind = TypeKind::Null;
                    break;
                case ast::LiteralType::Undefined:
                    typeInfo.kind = TypeKind::Undefined;
                    break;
                case ast::LiteralType::RegExp:
                    typeInfo.kind = TypeKind::Object;
                    break;
                default:
                    typeInfo.kind = TypeKind::Unknown;
                    break;
            }
            break;
        }
        case ast::NodeType::Identifier: {
            auto id = std::static_pointer_cast<ast::Identifier>(node);
            typeInfo = lookupVariableType(id->name);
            break;
        }
        case ast::NodeType::BinaryExpression: {
            auto binary = std::static_pointer_cast<ast::BinaryExpression>(node);
            
            TypeInfo leftType = inferType(binary->left);
            TypeInfo rightType = inferType(binary->right);
            
            // 演算子に基づく型推論
            if (binary->operator_ == "+" && 
                (leftType.kind == TypeKind::String || rightType.kind == TypeKind::String)) {
                typeInfo.kind = TypeKind::String;
            } else if (binary->operator_ == "+" || binary->operator_ == "-" || 
                       binary->operator_ == "*" || binary->operator_ == "/" || 
                       binary->operator_ == "%" || binary->operator_ == "**") {
                typeInfo.kind = TypeKind::Number;
            } else if (binary->operator_ == "==" || binary->operator_ == "!=" || 
                       binary->operator_ == "===" || binary->operator_ == "!==" ||
                       binary->operator_ == "<" || binary->operator_ == "<=" || 
                       binary->operator_ == ">" || binary->operator_ == ">=" ||
                       binary->operator_ == "&&" || binary->operator_ == "||") {
                typeInfo.kind = TypeKind::Boolean;
            } else {
                typeInfo.kind = TypeKind::Unknown;
            }
            break;
        }
        case ast::NodeType::UnaryExpression: {
            auto unary = std::static_pointer_cast<ast::UnaryExpression>(node);
            
            if (unary->operator_ == "!") {
                typeInfo.kind = TypeKind::Boolean;
            } else if (unary->operator_ == "+" || unary->operator_ == "-" || 
                       unary->operator_ == "~") {
                typeInfo.kind = TypeKind::Number;
            } else if (unary->operator_ == "typeof") {
                typeInfo.kind = TypeKind::String;
            } else {
                typeInfo.kind = TypeKind::Unknown;
            }
            break;
        }
        case ast::NodeType::CallExpression: {
            auto call = std::static_pointer_cast<ast::CallExpression>(node);
            
            // 特定の組み込み関数の戻り値型を推論
            if (call->callee->type == ast::NodeType::Identifier) {
                auto id = std::static_pointer_cast<ast::Identifier>(call->callee);
                
                if (id->name == "parseInt" || id->name == "parseFloat") {
                    typeInfo.kind = TypeKind::Number;
                } else if (id->name == "String") {
                    typeInfo.kind = TypeKind::String;
                } else if (id->name == "Number") {
                    typeInfo.kind = TypeKind::Number;
                } else if (id->name == "Boolean") {
                    typeInfo.kind = TypeKind::Boolean;
                } else {
                    // カスタム関数の場合は戻り値型を検索
                    TypeInfo funcType = lookupFunctionType(id->name);
                    if (funcType.kind == TypeKind::Function) {
                        typeInfo = funcType.returnType;
                    } else {
                        typeInfo.kind = TypeKind::Unknown;
                    }
                }
            } else if (call->callee->type == ast::NodeType::MemberExpression) {
                auto member = std::static_pointer_cast<ast::MemberExpression>(call->callee);
                
                // Math.関数の場合
                if (member->object->type == ast::NodeType::Identifier) {
                    auto obj = std::static_pointer_cast<ast::Identifier>(member->object);
                    
                    if (obj->name == "Math" && member->property->type == ast::NodeType::Identifier) {
                        typeInfo.kind = TypeKind::Number;
                    } else {
                        typeInfo.kind = TypeKind::Unknown;
                    }
                } else {
                    typeInfo.kind = TypeKind::Unknown;
                }
            } else {
                typeInfo.kind = TypeKind::Unknown;
            }
            break;
        }
        case ast::NodeType::ObjectExpression:
            typeInfo.kind = TypeKind::Object;
            break;
        case ast::NodeType::ArrayExpression:
            typeInfo.kind = TypeKind::Array;
            break;
        case ast::NodeType::FunctionExpression:
        case ast::NodeType::ArrowFunctionExpression:
            typeInfo.kind = TypeKind::Function;
            break;
        default:
            typeInfo.kind = TypeKind::Unknown;
            break;
    }
    
    // 結果をキャッシュ
    cacheTypeForNode(node, typeInfo);
    
    if (m_enableStatistics && typeInfo.kind != TypeKind::Unknown) {
        m_inferredTypes++;
    }
    
    return typeInfo;
}

void TypeInferenceTransformer::cacheTypeForNode(const ast::NodePtr& node, const TypeInfo& type) {
    std::stringstream ss;
    ss << node->type << ":" << node.get();
    std::string nodeId = ss.str();
    
    m_typeCache[nodeId] = type;
}

TypeInfo TypeInferenceTransformer::lookupVariableType(const std::string& name) {
    // スコープスタックを逆順に検索
    for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            return varIt->second;
        }
    }
    
    return TypeInfo{TypeKind::Unknown};
}

TypeInfo TypeInferenceTransformer::lookupFunctionType(const std::string& name) {
    std::string funcId = "func:" + name;
    
    auto it = m_typeCache.find(funcId);
    if (it != m_typeCache.end()) {
        return it->second;
    }
    
    return TypeInfo{TypeKind::Unknown};
}

void TypeInferenceTransformer::updateVariableType(const std::string& name, const TypeInfo& type) {
    // 変数がどのスコープにあるか検索
    for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
        auto varIt = it->find(name);
        if (varIt != it->end()) {
            // 既存の型と新しい型を比較
            if (varIt->second.kind == TypeKind::Unknown) {
                // 未知の型の場合は単純に更新
                varIt->second = type;
                
                if (m_enableStatistics && type.kind != TypeKind::Unknown) {
                    m_inferredTypes++;
                }
            } else if (!areTypesSame(varIt->second, type) && type.kind != TypeKind::Unknown) {
                // 型の不一致を検出
                if (m_enableStatistics) {
                    m_typeMismatchWarnings++;
                }
                
                // 型を統合
                varIt->second = mergeTypes(varIt->second, type);
            }
            
            return;
        }
    }
    
    // 見つからない場合は最もローカルなスコープに追加
    if (!m_scopeStack.empty()) {
        m_scopeStack.back()[name] = type;
        
        if (m_enableStatistics) {
            m_inferredVariables++;
            if (type.kind != TypeKind::Unknown) {
                m_inferredTypes++;
            }
        }
    }
}

bool TypeInferenceTransformer::areTypesSame(const TypeInfo& a, const TypeInfo& b) {
    if (a.kind == TypeKind::Unknown || b.kind == TypeKind::Unknown) {
        return true; // 未知の型は他の型と互換性がある
    }
    
    return a.kind == b.kind;
}

TypeInfo TypeInferenceTransformer::mergeTypes(const TypeInfo& a, const TypeInfo& b) {
    // 片方が未知の場合は他方を採用
    if (a.kind == TypeKind::Unknown) {
        return b;
    }
    if (b.kind == TypeKind::Unknown) {
        return a;
    }
    
    // 同じ型の場合はそのまま返す
    if (a.kind == b.kind) {
        return a;
    }
    
    // 異なる型の場合はユニオン型として扱う
    TypeInfo result;
    result.kind = TypeKind::Union;
    result.unionTypes.push_back(a);
    result.unionTypes.push_back(b);
    
    return result;
}

void TypeInferenceTransformer::scanDeclarations(const ast::NodePtr& program) {
    if (program->type != ast::NodeType::Program) {
        return;
    }
    
    auto prog = std::static_pointer_cast<ast::Program>(program);
    
    // プログラム内のすべての関数宣言を最初にスキャン
    for (const auto& stmt : prog->body) {
        if (stmt->type == ast::NodeType::FunctionDeclaration) {
            auto func = std::static_pointer_cast<ast::FunctionDeclaration>(stmt);
            
            // 関数の一意なIDを生成
            std::string funcId = "func:" + func->id->name;
            
            // 新しい型情報を作成
            TypeInfo funcTypeInfo;
            funcTypeInfo.kind = TypeKind::Function;
            funcTypeInfo.returnType.kind = TypeKind::Unknown;
            
            // パラメータの型情報を初期化
            for (const auto& param : func->params) {
                if (param->type == ast::NodeType::Identifier) {
                    auto identifier = std::static_pointer_cast<ast::Identifier>(param);
                    funcTypeInfo.paramTypes[identifier->name].kind = TypeKind::Unknown;
                }
            }
            
            // 関数の型情報をキャッシュに追加
            m_typeCache[funcId] = funcTypeInfo;
            
            // 現在のスコープに関数を登録
            if (!m_scopeStack.empty()) {
                m_scopeStack.back()[func->id->name].kind = TypeKind::Function;
                
                if (m_enableStatistics) {
                    m_inferredVariables++;
                    m_inferredTypes++;
                }
            }
        }
    }
}

bool TypeInferenceTransformer::isConstant(const ast::NodePtr& node) {
    if (!node) {
        return false;
    }
    
    // リテラルは定数
    if (node->type == ast::NodeType::Literal) {
        return true;
    }
    
    return false;
}

std::string TypeInferenceTransformer::typeKindToString(TypeKind kind) {
    switch (kind) {
        case TypeKind::Unknown:    return "unknown";
        case TypeKind::Null:       return "null";
        case TypeKind::Undefined:  return "undefined";
        case TypeKind::Boolean:    return "boolean";
        case TypeKind::Number:     return "number";
        case TypeKind::String:     return "string";
        case TypeKind::Object:     return "object";
        case TypeKind::Array:      return "array";
        case TypeKind::Function:   return "function";
        case TypeKind::Union:      return "union";
        default:                   return "unknown";
    }
}

} // namespace transformers
} // namespace aero
 