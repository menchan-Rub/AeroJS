/**
 * @file function_declaration.h
 * @brief AeroJS AST の関数宣言ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの関数宣言 (`function name(...) { ... }`)
 * を表すASTノード `FunctionDeclaration` を定義します。
 * 関数式 (`FunctionExpression`) やアロー関数 (`ArrowFunctionExpression`)
 * とは区別されます。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_DECLARATIONS_FUNCTION_DECLARATION_H
#define AEROJS_PARSER_AST_NODES_DECLARATIONS_FUNCTION_DECLARATION_H

#include <memory>
#include <optional>  // 関数の ID がない場合があるため
#include <string>
#include <vector>

#include "../../visitors/ast_visitor.h"
#include "../node.h"
#include "src/core/parser/ast/nodes/declaration_node.h"
#include "src/core/parser/ast/nodes/expressions/identifier.h"  // 関数名
#include "src/core/parser/ast/nodes/patterns/pattern_node.h"   // パラメータ
#include "src/core/parser/ast/nodes/statements/statements.h"   // 関数本体 (BlockStatement)
#include "src/core/parser/common.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class FunctionDeclaration
 * @brief 関数宣言 (`function name(...) { ... }`) を表すノード。
 *
 * @details
 * 関数名 (`id`)、パラメータリスト (`params`)、関数本体 (`body`)、
 * および async/generator フラグを持ちます。
 * 関数名は export default の場合は省略可能です。
 */
class FunctionDeclaration final : public Node {  // DeclarationNode を継承すべきか？ Node でよいか？
 public:
  /**
   * @brief FunctionDeclarationノードのコンストラクタ。
   * @param location ソースコード内のこのノードの位置情報。
   * @param id 関数名 (Identifier、export default の場合は null を指す unique_ptr)。ムーブされる。
   * @param params パラメータリスト (Identifier または Pattern のリスト)。ムーブされる。
   * @param body 関数本体 (BlockStatement)。ムーブされる。
   * @param isAsync 非同期関数 (`async function`) かどうか。
   * @param isGenerator ジェネレータ関数 (`function*`) かどうか。
   * @param parent 親ノード (オプション)。
   */
  explicit FunctionDeclaration(const SourceLocation& location,
                               NodePtr&& id,  // export default の場合は null
                               std::vector<NodePtr>&& params,
                               NodePtr&& body,
                               bool isAsync,
                               bool isGenerator,
                               Node* parent = nullptr);

  ~FunctionDeclaration() override = default;

  FunctionDeclaration(const FunctionDeclaration&) = delete;
  FunctionDeclaration& operator=(const FunctionDeclaration&) = delete;
  FunctionDeclaration(FunctionDeclaration&&) noexcept = default;
  FunctionDeclaration& operator=(FunctionDeclaration&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::FunctionDeclaration;
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

#endif  // AEROJS_PARSER_AST_NODES_DECLARATIONS_FUNCTION_DECLARATION_H