/**
 * @file debugger_statement.h
 * @brief AeroJS AST の debugger 文ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの `debugger` 文
 * を表すASTノード `DebuggerStatement` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENTS_DEBUGGER_STATEMENT_H
#define AEROJS_PARSER_AST_NODES_STATEMENTS_DEBUGGER_STATEMENT_H

#include <memory>
#include <string>
#include <vector>

#include "../../common.h"
#include "../../visitors/ast_visitor.h"
#include "statement_node.h"  // 基底クラス

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class DebuggerStatement
 * @brief `debugger` 文を表すノード。
 *
 * @details
 * この文は、利用可能なデバッグ機能があれば、実行中のJavaScriptコードに
 * ブレークポイントを設定する機能を提供します。デバッガがアクティブでない場合は
 * 何の効果もありません。子ノードは持ちません。
 */
class DebuggerStatement final : public StatementNode {
 public:
  /**
   * @brief DebuggerStatementノードのコンストラクタ。
   * @param location ソースコード内の位置情報。
   * @param parent 親ノード (オプション)。
   */
  explicit DebuggerStatement(const SourceLocation& location,
                             Node* parent = nullptr);

  ~DebuggerStatement() override = default;

  DebuggerStatement(const DebuggerStatement&) = delete;
  DebuggerStatement& operator=(const DebuggerStatement&) = delete;
  DebuggerStatement(DebuggerStatement&&) noexcept = default;
  DebuggerStatement& operator=(DebuggerStatement&&) noexcept = default;

  NodeType getType() const noexcept override final {
    return NodeType::DebuggerStatement;
  }

  void accept(AstVisitor* visitor) override final;
  void accept(ConstAstVisitor* visitor) const override final;

  std::vector<Node*> getChildren() override final;
  std::vector<const Node*> getChildren() const override final;

  nlohmann::json toJson(bool pretty = false) const override final;
  std::string toString() const override final;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENTS_DEBUGGER_STATEMENT_H