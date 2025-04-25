/**
 * @file function_expression.h
 * @brief AeroJS AST の関数式ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの関数式 (`function name? (...) { ... }`)
 * を表すASTノード `FunctionExpression` を定義します。
 * 関数宣言 (`FunctionDeclaration`) やアロー関数 (`ArrowFunctionExpression`)
 * とは区別されます。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H

#include <memory>
#include <optional>  // 関数の ID がない場合があるため
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "../node.h"
#include "../patterns/pattern_node.h"  // パラメータ用
#include "../statements/statements.h"  // 関数本体 (BlockStatement)
#include "expression_node.h"           // 基底クラス
#include "identifier.h"                // 関数名用

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class FunctionExpression
 * @brief 関数式 (`function name? (...) { ... }`) を表すノード。
 *
 * @details
 * 関数宣言 (`FunctionDeclaration`) と似ていますが、これは式として評価されます。
 * オプションの関数名 (`id`)、パラメータリスト (`params`)、関数本体 (`body`)、
 * および async/generator フラグを持ちます。
 */
class FunctionExpression final : public ExpressionNode {
 public:
  /**
   * @brief FunctionExpressionノードのコンストラクタ。
   * @param location ソースコード内のこのノードの位置情報。
   * @param id 関数名 (Identifier、オプション)。ムーブされる。
   * @param params パラメータリスト (Identifier または Pattern のリスト)。ムーブされる。
   * @param body 関数本体 (BlockStatement)。ムーブされる。
   * @param isAsync 非同期関数 (`async function`) かどうか。
   * @param isGenerator ジェネレータ関数 (`function*`) かどうか。
   * @param parent 親ノード (オプション)。
   */
  explicit FunctionExpression(const SourceLocation& location,
                              NodePtr&& id,  // オプション、nullptr の場合あり
                              std::vector<NodePtr>&& params,
                              NodePtr&& body,
                              bool isAsync,
                              bool isGenerator,
                              Node* parent = nullptr);

  ~FunctionExpression() override = default;

  FunctionExpression(const FunctionExpression&) = delete;
  FunctionExpression& operator=(const FunctionExpression&) = delete;
  FunctionExpression(FunctionExpression&&) noexcept = default;
  FunctionExpression& operator=(FunctionExpression&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::FunctionExpression;
  }

  /**
   * @brief 関数名を取得します (非const版)。
   * @return 関数名ノードへの参照 (`NodePtr&`)。関数名がない場合はnullを指すunique_ptr。
   */
  NodePtr& getId() noexcept;

  /**
   * @brief 関数名を取得します (const版)。
   * @return 関数名ノードへのconst参照 (`const NodePtr&`)。関数名がない場合はnullを指すunique_ptr。
   */
  const NodePtr& getId() const noexcept;

  /**
   * @brief パラメータリストを取得します (非const版)。
   * @return パラメータノードのリストへの参照 (`std::vector<NodePtr>&`)。
   */
  std::vector<NodePtr>& getParams() noexcept;

  /**
   * @brief パラメータリストを取得します (const版)。
   * @return パラメータノードのリストへのconst参照 (`const std::vector<NodePtr>&`)。
   */
  const std::vector<NodePtr>& getParams() const noexcept;

  /**
   * @brief 関数本体を取得します (非const版)。
   * @return 関数本体 (BlockStatement) ノードへの参照 (`NodePtr&`)。
   */
  NodePtr& getBody() noexcept;

  /**
   * @brief 関数本体を取得します (const版)。
   * @return 関数本体 (BlockStatement) ノードへのconst参照 (`const NodePtr&`)。
   */
  const NodePtr& getBody() const noexcept;

  /**
   * @brief 関数が非同期 (`async`) かどうかを返します。
   * @return 非同期の場合は true、それ以外は false。
   */
  bool isAsync() const noexcept;

  /**
   * @brief 関数がジェネレータ (`function*`) かどうかを返します。
   * @return ジェネレータの場合は true、それ以外は false。
   */
  bool isGenerator() const noexcept;

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;

 private:
  NodePtr m_id;                   ///< 関数名 (Identifier or null)
  std::vector<NodePtr> m_params;  ///< パラメータリスト (Identifier or Pattern)
  NodePtr m_body;                 ///< 関数本体 (BlockStatement)
  bool m_isAsync;                 ///< async 関数か
  bool m_isGenerator;             ///< generator 関数か
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H