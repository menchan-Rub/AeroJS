/**
 * @file transformer.h
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief AST変換を行うトランスフォーマーの基本インターフェース
 *
 * このファイルは、AeroJS JavaScriptエンジンのためのASTトランスフォーマーシステムを定義します。
 * トランスフォーマーは、JavaScriptのASTを変換して最適化や解析を行うコンポーネントです。
 */

#ifndef AERO_TRANSFORMER_H
#define AERO_TRANSFORMER_H

#include "core/ast/node.h"
#include "core/ast/visitor.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace aero {
namespace transformers {

/**
 * @brief 変換の結果を保持する構造体
 */
struct TransformResult {
    ast::NodePtr node;             ///< 変換後のノード
    bool changed;                  ///< 変換によって変更があったか
    bool stop_transformation;      ///< 変換処理を停止するか
    
    /**
     * @brief 変更があったが変換を続行する結果を作成
     * @param n 変換後のノード
     * @return 変換結果
     */
    static TransformResult changed(ast::NodePtr n) {
        return {std::move(n), true, false};
    }
    
    /**
     * @brief 変更がなく変換を続行する結果を作成
     * @param n 変換後のノード
     * @return 変換結果
     */
    static TransformResult unchanged(ast::NodePtr n) {
        return {std::move(n), false, false};
    }
    
    /**
     * @brief 変換を停止する結果を作成
     * @param n 変換後のノード
     * @return 変換結果
     */
    static TransformResult stop(ast::NodePtr n) {
        return {std::move(n), true, true};
    }
};

/**
 * @brief トランスフォーマーの基本インターフェース
 *
 * ASTノードを変換するための基本インターフェースを定義します。
 * サブクラスは変換ロジックを実装する必要があります。
 */
class ITransformer {
public:
    /**
     * @brief 仮想デストラクタ
     */
    virtual ~ITransformer() = default;
    
    /**
     * @brief ノードを変換します
     * @param node 変換するノード
     * @return 変換結果
     */
    virtual TransformResult transform(ast::NodePtr node) = 0;
    
    /**
     * @brief トランスフォーマーの名前を取得します
     * @return トランスフォーマーの名前
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief トランスフォーマーの説明を取得します
     * @return トランスフォーマーの説明
     */
    virtual std::string getDescription() const = 0;
};

/**
 * @brief スマートポインタによるトランスフォーマー型
 */
using TransformerPtr = std::shared_ptr<ITransformer>;

/**
 * @brief 基本的なASTトランスフォーマー実装
 *
 * 再帰的にASTを訪問し、各ノードタイプに対する変換メソッドを呼び出します。
 */
class Transformer : public ITransformer, public ast::Visitor {
public:
    /**
     * @brief コンストラクタ
     * @param name トランスフォーマーの名前
     * @param description トランスフォーマーの説明
     */
    Transformer(std::string name, std::string description);
    
    /**
     * @brief デストラクタ
     */
    ~Transformer() override = default;
    
    /**
     * @brief ノードを変換します
     * @param node 変換するノード
     * @return 変換結果
     */
    TransformResult transform(ast::NodePtr node) override;
    
    /**
     * @brief トランスフォーマーの名前を取得します
     * @return トランスフォーマーの名前
     */
    std::string getName() const override;
    
    /**
     * @brief トランスフォーマーの説明を取得します
     * @return トランスフォーマーの説明
     */
    std::string getDescription() const override;
    
protected:
    /**
     * @brief ノードを再帰的に変換します
     * @param node 変換するノード
     * @return 変換結果
     */
    virtual TransformResult transformNode(ast::NodePtr node);

    // AST Visitor メソッドのオーバーライド
    void visitProgram(ast::Program* node) override;
    void visitExpressionStatement(ast::ExpressionStatement* node) override;
    void visitBlockStatement(ast::BlockStatement* node) override;
    void visitEmptyStatement(ast::EmptyStatement* node) override;
    void visitIfStatement(ast::IfStatement* node) override;
    void visitSwitchStatement(ast::SwitchStatement* node) override;
    void visitCaseClause(ast::CaseClause* node) override;
    void visitWhileStatement(ast::WhileStatement* node) override;
    void visitDoWhileStatement(ast::DoWhileStatement* node) override;
    void visitForStatement(ast::ForStatement* node) override;
    void visitForInStatement(ast::ForInStatement* node) override;
    void visitForOfStatement(ast::ForOfStatement* node) override;
    void visitContinueStatement(ast::ContinueStatement* node) override;
    void visitBreakStatement(ast::BreakStatement* node) override;
    void visitReturnStatement(ast::ReturnStatement* node) override;
    void visitWithStatement(ast::WithStatement* node) override;
    void visitLabeledStatement(ast::LabeledStatement* node) override;
    void visitTryStatement(ast::TryStatement* node) override;
    void visitCatchClause(ast::CatchClause* node) override;
    void visitThrowStatement(ast::ThrowStatement* node) override;
    void visitDebuggerStatement(ast::DebuggerStatement* node) override;
    void visitVariableDeclaration(ast::VariableDeclaration* node) override;
    void visitVariableDeclarator(ast::VariableDeclarator* node) override;
    void visitIdentifier(ast::Identifier* node) override;
    void visitLiteral(ast::Literal* node) override;
    void visitFunctionDeclaration(ast::FunctionDeclaration* node) override;
    void visitFunctionExpression(ast::FunctionExpression* node) override;
    void visitArrowFunctionExpression(ast::ArrowFunctionExpression* node) override;
    void visitClassDeclaration(ast::ClassDeclaration* node) override;
    void visitClassExpression(ast::ClassExpression* node) override;
    void visitMethodDefinition(ast::MethodDefinition* node) override;
    void visitClassProperty(ast::ClassProperty* node) override;
    void visitImportDeclaration(ast::ImportDeclaration* node) override;
    void visitExportNamedDeclaration(ast::ExportNamedDeclaration* node) override;
    void visitExportDefaultDeclaration(ast::ExportDefaultDeclaration* node) override;
    void visitExportAllDeclaration(ast::ExportAllDeclaration* node) override;
    void visitImportSpecifier(ast::ImportSpecifier* node) override;
    void visitExportSpecifier(ast::ExportSpecifier* node) override;
    void visitObjectPattern(ast::ObjectPattern* node) override;
    void visitArrayPattern(ast::ArrayPattern* node) override;
    void visitAssignmentPattern(ast::AssignmentPattern* node) override;
    void visitRestElement(ast::RestElement* node) override;
    void visitSpreadElement(ast::SpreadElement* node) override;
    void visitTemplateElement(ast::TemplateElement* node) override;
    void visitTemplateLiteral(ast::TemplateLiteral* node) override;
    void visitTaggedTemplateExpression(ast::TaggedTemplateExpression* node) override;
    void visitObjectExpression(ast::ObjectExpression* node) override;
    void visitProperty(ast::Property* node) override;
    void visitArrayExpression(ast::ArrayExpression* node) override;
    void visitUnaryExpression(ast::UnaryExpression* node) override;
    void visitUpdateExpression(ast::UpdateExpression* node) override;
    void visitBinaryExpression(ast::BinaryExpression* node) override;
    void visitAssignmentExpression(ast::AssignmentExpression* node) override;
    void visitLogicalExpression(ast::LogicalExpression* node) override;
    void visitMemberExpression(ast::MemberExpression* node) override;
    void visitConditionalExpression(ast::ConditionalExpression* node) override;
    void visitCallExpression(ast::CallExpression* node) override;
    void visitNewExpression(ast::NewExpression* node) override;
    void visitSequenceExpression(ast::SequenceExpression* node) override;
    void visitAwaitExpression(ast::AwaitExpression* node) override;
    void visitYieldExpression(ast::YieldExpression* node) override;

private:
    std::string m_name;           ///< トランスフォーマーの名前
    std::string m_description;    ///< トランスフォーマーの説明
    ast::NodePtr m_result;        ///< 変換結果のノード
    bool m_changed;               ///< 変換によって変更があったか
};

} // namespace transformers
} // namespace aero

#endif // AERO_TRANSFORMER_H 