/**
 * @file statement_node.h
 * @brief AeroJS AST の文 (Statement) ノードの基底クラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AeroJSのASTにおける全ての文 (Statement) ノードが
 * 継承する基底クラス `StatementNode` を定義します。
 * 文は値を評価しない実行単位です（式文を除く）。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_STATEMENT_NODE_H
#define AEROJS_PARSER_AST_NODES_STATEMENT_NODE_H

#include "src/core/parser/ast/nodes/node.h"
#include "src/core/parser/common.h"  // SourceLocation

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class StatementNode
 * @brief 全ての文 (Statement) ノードの抽象基底クラス。
 *
 * @details
 * 具象的な文ノード (例: `IfStatement`, `ForStatement`, `ExpressionStatement`)
 * は、このクラスを継承します。
 * 現在は `Node` クラスからの追加機能はありませんが、将来的に文特有の
 * 共通機能（例: ラベル管理など）を追加する可能性があります。
 */
class StatementNode : public Node {
 public:
  /**
   * @brief StatementNodeのコンストラクタ。
   * @param type 派生クラスのノードタイプ。
   * @param location ソースコード内の位置情報。
   * @param parent 親ノード (オプション)。
   */
  StatementNode(NodeType type,
                const SourceLocation& location,
                Node* parent = nullptr)
      : Node(type, location, parent) {
  }

  ~StatementNode() override = default;

  StatementNode(const StatementNode&) = delete;
  StatementNode& operator=(const StatementNode&) = delete;
  StatementNode(StatementNode&&) noexcept = default;
  StatementNode& operator=(StatementNode&&) noexcept = default;

  /**
   * @brief このノードが文 (Statement) であるかを示すフラグ。
   * @return 常に true。
   */
  bool isStatement() const noexcept override final {
    return true;
  }

  /**
   * @brief このノードが式 (Expression) であるかを示すフラグ。
   * @return 常に false。
   */
  bool isExpression() const noexcept override final {
    return false;
  }

 protected:
  // 基底クラスなので protected なデフォルトコンストラクタは不要か、
  // あるいは派生クラスからの呼び出し専用とする。
  // StatementNode() = default; // 必要に応じて
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_STATEMENT_NODE_H