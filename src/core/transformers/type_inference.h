/**
 * @file type_inference.h
 * @brief 型推論トランスフォーマーの定義
 * @version 0.1
 * @date 2024-01-11
 * 
 * @copyright Copyright (c) 2024 AeroJS Project
 * 
 * @license
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef AERO_TYPE_INFERENCE_H
#define AERO_TYPE_INFERENCE_H

#include "../ast/ast.h"
#include "transformer.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aero {
namespace transformers {

/**
 * @brief 型の種類を表す列挙型
 */
enum class TypeKind {
    Unknown,    // 未知の型
    Null,       // null値
    Undefined,  // undefined値
    Boolean,    // ブール値
    Number,     // 数値
    String,     // 文字列
    Object,     // オブジェクト
    Array,      // 配列
    Function,   // 関数
    Union       // 複数の型の組み合わせ
};

/**
 * @brief 型情報を表す構造体
 */
struct TypeInfo {
    TypeKind kind = TypeKind::Unknown;
    
    // 関数の戻り値と引数の型
    TypeInfo returnType;
    std::unordered_map<std::string, TypeInfo> paramTypes;
    
    // オブジェクトのメンバーの型
    std::unordered_map<std::string, TypeInfo> memberTypes;
    
    // 配列の要素の型
    TypeInfo elementType;
    
    // ユニオン型の場合の複数の型
    std::vector<TypeInfo> unionTypes;
};

/**
 * @brief JavaScriptコード内の変数や式の型を推論するトランスフォーマークラス
 * 
 * このクラスはASTを解析して各ノードの型を推論し、最適化や型チェックのための情報を提供します。
 * 主な機能は以下の通りです：
 * 1. 変数宣言や代入からの型推論
 * 2. リテラル値、演算子、関数呼び出しなどからの型推論
 * 3. 型情報の追跡と型互換性のチェック
 * 4. 型情報に基づく警告の生成
 */
class TypeInferenceTransformer : public Transformer {
public:
    /**
     * @brief コンストラクタ
     */
    TypeInferenceTransformer();
    
    /**
     * @brief デストラクタ
     */
    ~TypeInferenceTransformer();
    
    /**
     * @brief 統計情報の収集を有効または無効にする
     * @param enable 収集を有効にするかどうか
     */
    void enableStatistics(bool enable);
    
    /**
     * @brief 推論された型の総数を取得
     * @return 推論された型の数
     */
    int getInferredTypeCount() const;
    
    /**
     * @brief 型推論された変数の総数を取得
     * @return 型推論された変数の数
     */
    int getInferredVariableCount() const;
    
    /**
     * @brief 型の不一致警告の総数を取得
     * @return 型の不一致警告の数
     */
    int getTypeMismatchWarningCount() const;
    
    /**
     * @brief すべてのカウンターとキャッシュをリセット
     */
    void resetCounters();
    
    /**
     * @brief 型推論トランスフォームを実行
     * @param node 対象のASTノード
     * @return 変換されたノード
     */
    ast::NodePtr transform(const ast::NodePtr& node) override;

private:
    // トランスフォーマーの状態
    bool m_enableStatistics;
    int m_inferredTypes;
    int m_inferredVariables;
    int m_typeMismatchWarnings;
    
    // 型情報のキャッシュ（ノードIDまたは変数名 -> 型情報）
    std::unordered_map<std::string, TypeInfo> m_typeCache;
    
    // スコープスタック（変数名 -> 型情報のマップのスタック）
    std::vector<std::unordered_map<std::string, TypeInfo>> m_scopeStack;
    
    // 現在処理中の関数のID
    std::vector<std::string> m_currentFunctionTypeInfo;
    
    // ノード訪問メソッド
    ast::NodePtr visit(const ast::NodePtr& node) {
        if (!node) {
            return nullptr;
        }
        
        switch (node->type) {
            case ast::NodeType::Program:
                return visitProgram(node);
            case ast::NodeType::BlockStatement:
                return visitBlockStatement(node);
            case ast::NodeType::ExpressionStatement:
                return visitExpressionStatement(node);
            case ast::NodeType::IfStatement:
                return visitIfStatement(node);
            case ast::NodeType::ReturnStatement:
                return visitReturnStatement(node);
            case ast::NodeType::FunctionDeclaration:
                return visitFunctionDeclaration(node);
            case ast::NodeType::VariableDeclaration:
                return visitVariableDeclaration(node);
            case ast::NodeType::AssignmentExpression:
                return visitAssignmentExpression(node);
            case ast::NodeType::BinaryExpression:
                return visitBinaryExpression(node);
            case ast::NodeType::UnaryExpression:
                return visitUnaryExpression(node);
            case ast::NodeType::CallExpression:
                return visitCallExpression(node);
            case ast::NodeType::ObjectExpression:
                return visitObjectExpression(node);
            case ast::NodeType::ArrayExpression:
                return visitArrayExpression(node);
            case ast::NodeType::MemberExpression:
                return visitMemberExpression(node);
            case ast::NodeType::Identifier:
                return visitIdentifier(node);
            case ast::NodeType::Literal:
                return visitLiteral(node);
            default:
                // その他のノードタイプはそのまま返す
                return node;
        }
    }
    
    // 各ノードタイプ用の訪問関数
    ast::NodePtr visitProgram(const ast::NodePtr& node);
    ast::NodePtr visitBlockStatement(const ast::NodePtr& node);
    ast::NodePtr visitExpressionStatement(const ast::NodePtr& node);
    ast::NodePtr visitIfStatement(const ast::NodePtr& node);
    ast::NodePtr visitReturnStatement(const ast::NodePtr& node);
    ast::NodePtr visitFunctionDeclaration(const ast::NodePtr& node);
    ast::NodePtr visitVariableDeclaration(const ast::NodePtr& node);
    ast::NodePtr visitAssignmentExpression(const ast::NodePtr& node);
    ast::NodePtr visitBinaryExpression(const ast::NodePtr& node);
    ast::NodePtr visitUnaryExpression(const ast::NodePtr& node);
    ast::NodePtr visitCallExpression(const ast::NodePtr& node);
    ast::NodePtr visitObjectExpression(const ast::NodePtr& node);
    ast::NodePtr visitArrayExpression(const ast::NodePtr& node);
    ast::NodePtr visitMemberExpression(const ast::NodePtr& node);
    ast::NodePtr visitIdentifier(const ast::NodePtr& node);
    ast::NodePtr visitLiteral(const ast::NodePtr& node);
    
    // スコープ管理ヘルパーメソッド
    void enterScope();
    void exitScope();
    
    // 型推論ヘルパーメソッド
    TypeInfo inferType(const ast::NodePtr& node);
    void cacheTypeForNode(const ast::NodePtr& node, const TypeInfo& type);
    TypeInfo lookupVariableType(const std::string& name);
    TypeInfo lookupFunctionType(const std::string& name);
    void updateVariableType(const std::string& name, const TypeInfo& type);
    bool areTypesSame(const TypeInfo& a, const TypeInfo& b);
    TypeInfo mergeTypes(const TypeInfo& a, const TypeInfo& b);
    
    // 宣言スキャナー
    void scanDeclarations(const ast::NodePtr& program);
    
    // 定数判定
    bool isConstant(const ast::NodePtr& node);
    
    // 型種類を文字列に変換
    std::string typeKindToString(TypeKind kind);
};

} // namespace transformers
} // namespace aero

#endif // AERO_TYPE_INFERENCE_H 