/**
 * @file with_statement.h
 * @brief AeroJS AST の with 文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの `with` 文 (`with (object) statement`)
 * を表すASTノード `WithStatement` を定義します。
 * 注意: `with` 文は非推奨であり、strict mode では構文エラーとなります。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_WITH_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_WITH_STATEMENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../expressions/expression_node.h"  // スコープオブジェクト用
#include "statement_node.h"                  // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class WithStatement
 * @brief `with` 文 (`with (object) statement`) を表すノード。
 *
 * @warning `with` 文は非推奨であり、パフォーマンスやコードの可読性に悪影響を与えます。
 *          Strict mode では使用できません。
 */
class WithStatement final : public StatementNode {
 public:
  /**
   * @brief WithStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param object スコープ拡張の対象となるオブジェクト (Expression)。ムーブされる。
   * @param body `with` スコープ内で実行される文 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit WithStatement(const SourceLocation& location,
                         NodePtr&& object,
                         NodePtr&& body,
                         Node* parent = nullptr);

  ~WithStatement() override = default;

  WithStatement(const WithStatement&) = delete;
  WithStatement& operator=(const WithStatement&) = delete;
  WithStatement(WithStatement&&) noexcept = default;
  WithStatement& operator=(WithStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::WithStatement;
  }

  /**
   * @brief スコープ拡張の対象オブジェクトを取得します (非const版)。
   * @return オブジェクト式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getObject() noexcept;

  /**
   * @brief スコープ拡張の対象オブジェクトを取得します (const版)。
   * @return オブジェクト式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getObject() const noexcept;

  /**
   * @brief `with` スコープ内の文を取得します (非const版)。
   * @return 文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief `with` スコープ内の文を取得します (const版)。
   * @return 文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_object;  ///< スコープ拡張の対象オブジェクト (Expression)
  NodePtr m_body;    ///< `with` スコープ内の文 (Statement)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_WITH_STATEMENT_H