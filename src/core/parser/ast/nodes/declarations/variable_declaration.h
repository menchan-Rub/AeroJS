/**
 * @file variable_declaration.h
 * @brief AeroJS AST の変数宣言関連ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの変数宣言 (`var`, `let`, `const`)
 * に関連するASTノード (`VariableDeclarator`, `VariableDeclaration`) を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_DECLARATIONS_VARIABLE_DECLARATION_H
#define AEROJS_PARSER_AST_NODES_DECLARATIONS_VARIABLE_DECLARATION_H

#include <vector>
#include <memory>
#include <string>
#include <optional>

#include "../node.h"
#include "../../visitors/ast_visitor.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class VariableDeclarator
 * @brief 変数宣言子を表すノード。
 *
 * @details
 * `VariableDeclaration` 内の個々の宣言を表します (例: `x = 1`)。
 * 宣言される変数名 (またはパターン) と、オプションの初期化式を持ちます。
 */
class VariableDeclarator final : public Node {
public:
    /**
     * @brief VariableDeclaratorノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param id 宣言される変数名またはパターン (Identifier または Pattern)。ムーブされる。
     * @param init 初期化式 (オプション)。ムーブされる。
     * @param parent 親ノード (オプション)。
     */
    explicit VariableDeclarator(const SourceLocation& location,
                                NodePtr&& id,
                                NodePtr&& init, // Optional, can be nullptr
                                Node* parent = nullptr);

    ~VariableDeclarator() override = default;

    VariableDeclarator(const VariableDeclarator&) = delete;
    VariableDeclarator& operator=(const VariableDeclarator&) = delete;
    VariableDeclarator(VariableDeclarator&&) noexcept = default;
    VariableDeclarator& operator=(VariableDeclarator&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::VariableDeclarator; }

    /**
     * @brief 宣言される変数/パターンを取得します (非const版)。
     * @return 識別子またはパターンノードへの参照 (`NodePtr&`)。
     */
    NodePtr& getId() noexcept;

    /**
     * @brief 宣言される変数/パターンを取得します (const版)。
     * @return 識別子またはパターンノードへのconst参照 (`const NodePtr&`)。
     */
    const NodePtr& getId() const noexcept;

    /**
     * @brief 初期化式を取得します (非const版)。
     * @return 初期化式ノードへの参照 (`NodePtr&`)。初期化子がない場合はnullを指すunique_ptr。
     */
    NodePtr& getInit() noexcept;

    /**
     * @brief 初期化式を取得します (const版)。
     * @return 初期化式ノードへのconst参照 (`const NodePtr&`)。初期化子がない場合はnullを指すunique_ptr。
     */
    const NodePtr& getInit() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    NodePtr m_id;   ///< 宣言される変数/パターン (Identifier or Pattern)
    NodePtr m_init; ///< 初期化式 (Expression or null)
};

/**
 * @enum VariableDeclarationKind
 * @brief 変数宣言の種類 (`var`, `let`, `const`)。
 */
enum class VariableDeclarationKind {
    Var,
    Let,
    Const
};

/**
 * @brief VariableDeclarationKind を文字列に変換します。
 */
const char* variableDeclarationKindToString(VariableDeclarationKind kind);

/**
 * @class VariableDeclaration
 * @brief 変数宣言文 (`var`, `let`, `const`) を表すノード。
 *
 * @details
 * 1つ以上の `VariableDeclarator` を含みます。
 */
class VariableDeclaration final : public Node { // Note: Inherits from Node, not Statement directly
public:
    /**
     * @brief VariableDeclarationノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param declarations この宣言に含まれる宣言子のリスト (ムーブされる)。
     * @param kind 宣言の種類 (`var`, `let`, `const`)。
     * @param parent 親ノード (オプション)。
     */
    explicit VariableDeclaration(const SourceLocation& location,
                               std::vector<NodePtr>&& declarations,
                               VariableDeclarationKind kind,
                               Node* parent = nullptr);

    ~VariableDeclaration() override = default;

    VariableDeclaration(const VariableDeclaration&) = delete;
    VariableDeclaration& operator=(const VariableDeclaration&) = delete;
    VariableDeclaration(VariableDeclaration&&) noexcept = default;
    VariableDeclaration& operator=(VariableDeclaration&&) noexcept = default;

    NodeType getType() const noexcept override final { return NodeType::VariableDeclaration; }

    /**
     * @brief 宣言子のリストを取得します (非const版)。
     * @return 宣言子ノードのリストへの参照 (`std::vector<NodePtr>&`)。
     */
    std::vector<NodePtr>& getDeclarations() noexcept;

    /**
     * @brief 宣言子のリストを取得します (const版)。
     * @return 宣言子ノードのリストへのconst参照 (`const std::vector<NodePtr>&`)。
     */
    const std::vector<NodePtr>& getDeclarations() const noexcept;

    /**
     * @brief 宣言の種類 (`var`, `let`, `const`) を取得します。
     * @return 宣言の種類 (`VariableDeclarationKind`)。
     */
    VariableDeclarationKind getKind() const noexcept;

    void accept(AstVisitor* visitor) override final;
    void accept(ConstAstVisitor* visitor) const override final;

    std::vector<Node*> getChildren() override final;
    std::vector<const Node*> getChildren() const override final;

    nlohmann::json toJson(bool pretty = false) const override final;
    std::string toString() const override final;

private:
    std::vector<NodePtr> m_declarations; ///< 宣言子のリスト
    VariableDeclarationKind m_kind;      ///< 宣言の種類 (var, let, const)
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_DECLARATIONS_VARIABLE_DECLARATION_H 