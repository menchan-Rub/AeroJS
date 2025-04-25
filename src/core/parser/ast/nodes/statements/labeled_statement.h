/**
 * @file labeled_statement.h
 * @brief AeroJS AST のラベル付き文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptのラベル付き文 (`label: statement`)
 * を表すASTノード `LabeledStatement` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_LABELED_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_LABELED_STATEMENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../expressions/identifier.h"  // ラベル名用
#include "statement_node.h"             // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class LabeledStatement
 * @brief ラベル付き文 (`label: statement`) を表すノード。
 */
class LabeledStatement final : public StatementNode {
 public:
  /**
   * @brief LabeledStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param label ラベル名 (Identifier)。ムーブされる。
   * @param body ラベル付けされた文 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit LabeledStatement(const SourceLocation& location,
                            NodePtr&& label,
                            NodePtr&& body,
                            Node* parent = nullptr);

  ~LabeledStatement() override = default;

  LabeledStatement(const LabeledStatement&) = delete;
  LabeledStatement& operator=(const LabeledStatement&) = delete;
  LabeledStatement(LabeledStatement&&) noexcept = default;
  LabeledStatement& operator=(LabeledStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::LabeledStatement;
  }

  /**
   * @brief ラベル名を取得します (非const版)。
   * @return ラベル名 (Identifier) ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getLabel() noexcept;

  /**
   * @brief ラベル名を取得します (const版)。
   * @return ラベル名 (Identifier) ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getLabel() const noexcept;

  /**
   * @brief ラベル付けされた文を取得します (非const版)。
   * @return 文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ラベル付けされた文を取得します (const版)。
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
  NodePtr m_label;  ///< ラベル名 (Identifier)
  NodePtr m_body;   ///< ラベル付けされた文 (Statement)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_LABELED_STATEMENT_H