/**
 * @file try_statement.h
 * @brief AeroJS AST の try...catch...finally 文関連ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの `try...catch...finally` 文 (`TryStatement`)
 * および `catch` 節 (`CatchClause`) を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_TRY_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_TRY_STATEMENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../patterns/pattern_node.h"  // catch 節のパラメータ用
#include "statement_node.h"            // 基底クラス
#include "statements.h"                // BlockStatement 用

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class CatchClause
 * @brief `try` 文の `catch` 節を表すノード。
 *
 * @details
 * オプションでエラーオブジェクトを受け取るパラメータ (通常は Identifier または Pattern) を持ち、
 * 実行される本体 (BlockStatement) を持ちます。
 * パラメータが省略される場合もあります (`catch { ... }`)。
 */
class CatchClause final : public Node {  // StatementNode ではない
 public:
  /**
   * @brief CatchClauseノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param param catch するエラーオブジェクトのパラメータ (Identifier or Pattern or null)。ムーブされる。
   * @param body catch 節の本体 (BlockStatement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit CatchClause(const SourceLocation& location,
                       NodePtr&& param,  // null の場合あり
                       NodePtr&& body,
                       Node* parent = nullptr);

  ~CatchClause() override = default;

  CatchClause(const CatchClause&) = delete;
  CatchClause& operator=(const CatchClause&) = delete;
  CatchClause(CatchClause&&) noexcept = default;
  CatchClause& operator=(CatchClause&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::CatchClause;
  }

  /**
   * @brief パラメータを取得します (非const版)。
   * @return パラメータノード (Identifier/Pattern/null) への参照 (`NodePtr&`)。
   */
  NodePtr& getParam() noexcept;

  /**
   * @brief パラメータを取得します (const版)。
   * @return パラメータノード (Identifier/Pattern/null) へのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getParam() const noexcept;

  /**
   * @brief catch 節の本体を取得します (非const版)。
   * @return 本体 (BlockStatement) ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief catch 節の本体を取得します (const版)。
   * @return 本体 (BlockStatement) ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_param;  ///< エラーオブジェクトのパラメータ (Identifier or Pattern or null)
  NodePtr m_body;   ///< catch 節の本体 (BlockStatement)
};

/**
 * @class TryStatement
 * @brief `try...catch...finally` 文を表すノード。
 *
 * @details
 * `try` ブロック (`block`) は必須です。
 * `catch` ハンドラ (`handler`) または `finally` ブロック (`finalizer`) の
 * 少なくとも一方が必要です。
 */
class TryStatement final : public StatementNode {
 public:
  /**
   * @brief TryStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param block try ブロック (BlockStatement)。ムーブされる。
   * @param handler catch 節 (CatchClause or null)。ムーブされる。
   * @param finalizer finally ブロック (BlockStatement or null)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit TryStatement(const SourceLocation& location,
                        NodePtr&& block,
                        NodePtr&& handler,    // オプション
                        NodePtr&& finalizer,  // オプション
                        Node* parent = nullptr);

  ~TryStatement() override = default;

  TryStatement(const TryStatement&) = delete;
  TryStatement& operator=(const TryStatement&) = delete;
  TryStatement(TryStatement&&) noexcept = default;
  TryStatement& operator=(TryStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::TryStatement;
  }

  /**
   * @brief try ブロックを取得します (非const版)。
   * @return try ブロック (BlockStatement) ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBlock() noexcept;

  /**
   * @brief try ブロックを取得します (const版)。
   * @return try ブロック (BlockStatement) ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBlock() const noexcept;

  /**
   * @brief catch ハンドラを取得します (非const版)。
   * @return catch 節 (CatchClause) ノードへの参照 (`NodePtr&`)。catch 節がない場合はnullを指すunique_ptr。
   */
  NodePtr& getHandler() noexcept;

  /**
   * @brief catch ハンドラを取得します (const版)。
   * @return catch 節 (CatchClause) ノードへのconst参照 (`const NodePtr&`)。catch 節がない場合はnullを指すunique_ptr。
   */
  const NodePtr& getHandler() const noexcept;

  /**
   * @brief finally ブロックを取得します (非const版)。
   * @return finally ブロック (BlockStatement) ノードへの参照 (`NodePtr&`)。finally 節がない場合はnullを指すunique_ptr。
   */
  NodePtr& getFinalizer() noexcept;

  /**
   * @brief finally ブロックを取得します (const版)。
   * @return finally ブロック (BlockStatement) ノードへのconst参照 (`const NodePtr&`)。finally 節がない場合はnullを指すunique_ptr。
   */
  const NodePtr& getFinalizer() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_block;      ///< try ブロック (BlockStatement)
  NodePtr m_handler;    ///< catch 節 (CatchClause or null)
  NodePtr m_finalizer;  ///< finally ブロック (BlockStatement or null)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_TRY_STATEMENT_H