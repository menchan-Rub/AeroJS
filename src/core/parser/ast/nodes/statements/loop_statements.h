/**
 * @file loop_statements.h
 * @brief AeroJS AST のループ文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptのループ文 (`while`, `do-while`, `for`)
 * を表すASTノードを定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_LOOP_STATEMENTS_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_LOOP_STATEMENTS_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../declarations/variable_declaration.h"  // for 文の初期化用
#include "../expressions/expression_node.h"        // 条件式、更新式
#include "statement_node.h"                        // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class WhileStatement
 * @brief `while` 文 (`while (test) body`) を表すノード。
 */
class WhileStatement final : public StatementNode {
 public:
  /**
   * @brief WhileStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param test ループ継続条件 (Expression)。ムーブされる。
   * @param body ループ本体 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit WhileStatement(const SourceLocation& location,
                          NodePtr&& test,
                          NodePtr&& body,
                          Node* parent = nullptr);

  ~WhileStatement() override = default;

  WhileStatement(const WhileStatement&) = delete;
  WhileStatement& operator=(const WhileStatement&) = delete;
  WhileStatement(WhileStatement&&) noexcept = default;
  WhileStatement& operator=(WhileStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::WhileStatement;
  }

  /**
   * @brief ループ継続条件を取得します (非const版)。
   * @return 条件式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getTest() noexcept;

  /**
   * @brief ループ継続条件を取得します (const版)。
   * @return 条件式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getTest() const noexcept;

  /**
   * @brief ループ本体を取得します (非const版)。
   * @return ループ本体の文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ループ本体を取得します (const版)。
   * @return ループ本体の文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_test;  ///< ループ継続条件 (Expression)
  NodePtr m_body;  ///< ループ本体 (Statement)
};

/**
 * @class DoWhileStatement
 * @brief `do-while` 文 (`do body while (test)`) を表すノード。
 */
class DoWhileStatement final : public StatementNode {
 public:
  /**
   * @brief DoWhileStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param body ループ本体 (Statement)。ムーブされる。
   * @param test ループ継続条件 (Expression)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit DoWhileStatement(const SourceLocation& location,
                            NodePtr&& body,
                            NodePtr&& test,
                            Node* parent = nullptr);

  ~DoWhileStatement() override = default;

  DoWhileStatement(const DoWhileStatement&) = delete;
  DoWhileStatement& operator=(const DoWhileStatement&) = delete;
  DoWhileStatement(DoWhileStatement&&) noexcept = default;
  DoWhileStatement& operator=(DoWhileStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::DoWhileStatement;
  }

  /**
   * @brief ループ本体を取得します (非const版)。
   * @return ループ本体の文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ループ本体を取得します (const版)。
   * @return ループ本体の文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  /**
   * @brief ループ継続条件を取得します (非const版)。
   * @return 条件式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getTest() noexcept;

  /**
   * @brief ループ継続条件を取得します (const版)。
   * @return 条件式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getTest() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_body;  ///< ループ本体 (Statement)
  NodePtr m_test;  ///< ループ継続条件 (Expression)
};

/**
 * @class ForStatement
 * @brief `for` 文 (`for (init; test; update) body`) を表すノード。
 *
 * @details
 * `init`, `test`, `update` はそれぞれ省略可能です (null)。
 * `init` は `VariableDeclaration` または `Expression` です。
 */
class ForStatement final : public StatementNode {
 public:
  /**
   * @brief ForStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param init 初期化部 (VariableDeclaration or Expression or null)。ムーブされる。
   * @param test 条件部 (Expression or null)。ムーブされる。
   * @param update 更新部 (Expression or null)。ムーブされる。
   * @param body ループ本体 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ForStatement(const SourceLocation& location,
                        NodePtr&& init,    // オプション
                        NodePtr&& test,    // オプション
                        NodePtr&& update,  // オプション
                        NodePtr&& body,
                        Node* parent = nullptr);

  ~ForStatement() override = default;

  ForStatement(const ForStatement&) = delete;
  ForStatement& operator=(const ForStatement&) = delete;
  ForStatement(ForStatement&&) noexcept = default;
  ForStatement& operator=(ForStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ForStatement;
  }

  /**
   * @brief 初期化部を取得します (非const版)。
   * @return 初期化ノード (VariableDeclaration/Expression/null) への参照 (`NodePtr&`)。
   */
  NodePtr& getInit() noexcept;

  /**
   * @brief 初期化部を取得します (const版)。
   * @return 初期化ノード (VariableDeclaration/Expression/null) へのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getInit() const noexcept;

  /**
   * @brief 条件部を取得します (非const版)。
   * @return 条件式ノード (Expression/null) への参照 (`NodePtr&`)。
   */
  NodePtr& getTest() noexcept;

  /**
   * @brief 条件部を取得します (const版)。
   * @return 条件式ノード (Expression/null) へのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getTest() const noexcept;

  /**
   * @brief 更新部を取得します (非const版)。
   * @return 更新式ノード (Expression/null) への参照 (`NodePtr&`)。
   */
  NodePtr& getUpdate() noexcept;

  /**
   * @brief 更新部を取得します (const版)。
   * @return 更新式ノード (Expression/null) へのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getUpdate() const noexcept;

  /**
   * @brief ループ本体を取得します (非const版)。
   * @return ループ本体の文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ループ本体を取得します (const版)。
   * @return ループ本体の文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_init;    ///< 初期化部 (VariableDeclaration or Expression or null)
  NodePtr m_test;    ///< 条件部 (Expression or null)
  NodePtr m_update;  ///< 更新部 (Expression or null)
  NodePtr m_body;    ///< ループ本体 (Statement)
};

/**
 * @class ForInStatement
 * @brief `for...in` 文 (`for (left in right) body`) を表すノード。
 *
 * @details
 * `left` は通常、変数宣言 (`VariableDeclaration`) またはパターン (`Pattern`) です。
 * `right` はオブジェクト式 (`Expression`) です。
 */
class ForInStatement final : public StatementNode {
 public:
  /**
   * @brief ForInStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param left ループ変数/パターン (VariableDeclaration or Pattern)。ムーブされる。
   * @param right 反復対象のオブジェクト (Expression)。ムーブされる。
   * @param body ループ本体 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ForInStatement(const SourceLocation& location,
                          NodePtr&& left,
                          NodePtr&& right,
                          NodePtr&& body,
                          Node* parent = nullptr);

  ~ForInStatement() override = default;

  ForInStatement(const ForInStatement&) = delete;
  ForInStatement& operator=(const ForInStatement&) = delete;
  ForInStatement(ForInStatement&&) noexcept = default;
  ForInStatement& operator=(ForInStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ForInStatement;
  }

  /**
   * @brief 左辺 (ループ変数/パターン) を取得します (非const版)。
   * @return 左辺ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getLeft() noexcept;

  /**
   * @brief 左辺 (ループ変数/パターン) を取得します (const版)。
   * @return 左辺ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getLeft() const noexcept;

  /**
   * @brief 右辺 (反復対象オブジェクト) を取得します (非const版)。
   * @return 右辺式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getRight() noexcept;

  /**
   * @brief 右辺 (反復対象オブジェクト) を取得します (const版)。
   * @return 右辺式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getRight() const noexcept;

  /**
   * @brief ループ本体を取得します (非const版)。
   * @return ループ本体の文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ループ本体を取得します (const版)。
   * @return ループ本体の文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_left;   ///< ループ変数/パターン (VariableDeclaration or Pattern)
  NodePtr m_right;  ///< 反復対象のオブジェクト (Expression)
  NodePtr m_body;   ///< ループ本体 (Statement)
};

/**
 * @class ForOfStatement
 * @brief `for...of` 文 (`for (left of right) body`) および `for await...of` 文を表すノード。
 *
 * @details
 * `left` は通常、変数宣言 (`VariableDeclaration`) またはパターン (`Pattern`) です。
 * `right` は反復可能なオブジェクト式 (`Expression`) です。
 * `await` フラグは `for await...of` 構文の場合に true になります。
 */
class ForOfStatement final : public StatementNode {
 public:
  /**
   * @brief ForOfStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param isAwait `for await...of` 構文かどうか。
   * @param left ループ変数/パターン (VariableDeclaration or Pattern)。ムーブされる。
   * @param right 反復対象のオブジェクト (Expression)。ムーブされる。
   * @param body ループ本体 (Statement)。ムーブされる。
   * @param parent 親ノード (オプション)。
   */
  explicit ForOfStatement(const SourceLocation& location,
                          bool isAwait,
                          NodePtr&& left,
                          NodePtr&& right,
                          NodePtr&& body,
                          Node* parent = nullptr);

  ~ForOfStatement() override = default;

  ForOfStatement(const ForOfStatement&) = delete;
  ForOfStatement& operator=(const ForOfStatement&) = delete;
  ForOfStatement(ForOfStatement&&) noexcept = default;
  ForOfStatement& operator=(ForOfStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::ForOfStatement;
  }

  /**
   * @brief `await` キーワードが使用されているか (`for await...of`) を示します。
   * @return await が使用されている場合は true、それ以外は false。
   */
  bool isAwait() const noexcept;

  /**
   * @brief 左辺 (ループ変数/パターン) を取得します (非const版)。
   * @return 左辺ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getLeft() noexcept;

  /**
   * @brief 左辺 (ループ変数/パターン) を取得します (const版)。
   * @return 左辺ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getLeft() const noexcept;

  /**
   * @brief 右辺 (反復対象オブジェクト) を取得します (非const版)。
   * @return 右辺式ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getRight() noexcept;

  /**
   * @brief 右辺 (反復対象オブジェクト) を取得します (const版)。
   * @return 右辺式ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getRight() const noexcept;

  /**
   * @brief ループ本体を取得します (非const版)。
   * @return ループ本体の文ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief ループ本体を取得します (const版)。
   * @return ループ本体の文ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  bool m_isAwait;   ///< `for await...of` かどうか
  NodePtr m_left;   ///< ループ変数/パターン (VariableDeclaration or Pattern)
  NodePtr m_right;  ///< 反復対象のオブジェクト (Expression)
  NodePtr m_body;   ///< ループ本体 (Statement)
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_LOOP_STATEMENTS_H