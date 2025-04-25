/**
 * @file transformer.h
 * @version 1.2.0
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @brief AST変換を行うトランスフォーマーの基本インターフェースと実装。
 *
 * @details
 * このファイルは、AeroJS JavaScriptエンジンのためのASTトランスフォーマーシステムを定義します。
 * トランスフォーマーは、JavaScriptのASTを変換して最適化や解析を行うコンポーネントです。
 *
 * ここでは、純粋仮想インターフェース `ITransformer` と、
 * AST ノードを再帰的に変換する基本的な具象クラス `Transformer` を定義します。
 *
 * コーディング規約: AeroJS Coding Standards v1.2 準拠。
 */

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// AeroJS Core AST components
#include "src/core/parser/ast/node_type.h"
#include "src/core/parser/ast/nodes/node.h"

namespace aerojs::parser::ast {
class Program;
class ExpressionStatement;
class BlockStatement;
class EmptyStatement;
class IfStatement;
class SwitchStatement;
class CaseClause;
class WhileStatement;
class DoWhileStatement;
class ForStatement;
class ForInStatement;
class ForOfStatement;
class ContinueStatement;
class BreakStatement;
class ReturnStatement;
class WithStatement;
class LabeledStatement;
class TryStatement;
class CatchClause;
class ThrowStatement;
class DebuggerStatement;
class VariableDeclaration;
class VariableDeclarator;
class Identifier;
class Literal;
class FunctionDeclaration;
class FunctionExpression;
class ArrowFunctionExpression;
class ClassDeclaration;
class ClassExpression;
class MethodDefinition;
class FieldDefinition;
class ImportDeclaration;
class ExportNamedDeclaration;
class ExportDefaultDeclaration;
class ExportAllDeclaration;
class ImportSpecifier;
class ExportSpecifier;
class ObjectPattern;
class ArrayPattern;
class AssignmentPattern;
class RestElement;
class SpreadElement;
class TemplateElement;
class TemplateLiteral;
class TaggedTemplateExpression;
class ObjectExpression;
class Property;
class ArrayExpression;
class UnaryExpression;
class UpdateExpression;
class BinaryExpression;
class AssignmentExpression;
class LogicalExpression;
class MemberExpression;
class ConditionalExpression;
class CallExpression;
class NewExpression;
class SequenceExpression;
class AwaitExpression;
class YieldExpression;
class PrivateIdentifier;
class Super;
class ThisExpression;
class MetaProperty;
}

namespace aerojs::transformers {

/**
 * @struct TransformResult
 * @brief AST ノードの変換結果を保持する構造体。
 *
 * @details
 * 変換後のノード、変換によって変更があったかどうか、および
 * 後続の変換処理を停止すべきかどうかを示します。
 * イミュータブルなデータ構造として設計されており、static ファクトリメソッドを提供します。
 */
struct TransformResult {
  parser::ast::NodePtr transformedNode;  ///< 変換後のノード (所有権を持つ unique_ptr)。
  bool wasChanged;                       ///< 変換によって AST に変更があったかを示すフラグ。
  bool shouldStopTraversal;              ///< このノード以降の AST トラバーサルを停止すべきかを示すフラグ。

  explicit TransformResult(parser::ast::NodePtr node, bool changed, bool stopTraversal)
      : transformedNode(std::move(node)),
        wasChanged(changed),
        shouldStopTraversal(stopTraversal) {
  }

  static TransformResult Changed(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Changed node cannot be null in TransformResult::Changed.");
    }
    return TransformResult(std::move(node), true, false);
  }

  static TransformResult Unchanged(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Unchanged node cannot be null in TransformResult::Unchanged.");
    }
    return TransformResult(std::move(node), false, false);
  }

  static TransformResult ChangedAndStop(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Stopped node cannot be null in TransformResult::ChangedAndStop.");
    }
    return TransformResult(std::move(node), true, true);
  }

  static TransformResult UnchangedAndStop(parser::ast::NodePtr node) {
    if (!node) {
      throw std::invalid_argument("Stopped node cannot be null in TransformResult::UnchangedAndStop.");
    }
    return TransformResult(std::move(node), false, true);
  }
};

/**
 * @interface ITransformer
 * @brief AST トランスフォーマーの純粋仮想インターフェース。
 *
 * @details
 * 全ての具象トランスフォーマーが実装すべき基本的な操作を定義します。
 * これにより、異なる種類のトランスフォーマーをポリモーフィックに扱うことが可能になります。
 */
class ITransformer {
 public:
  /**
   * @brief 仮想デストラクタ。
   * @details 派生クラスのデストラクタが正しく呼び出されることを保証します。
   */
  virtual ~ITransformer() = default;

  /**
   * @brief 指定された AST ノードを変換します (純粋仮想関数)。
   * @param node 変換対象のノード (所有権を持つ unique_ptr)。変換後、このポインタは無効になる可能性があります。
   * @return TransformResult 変換結果。変換後のノード、変更フラグ、停止フラグを含みます。
   *         変換後のノードの所有権は TransformResult に移ります。
   */
  virtual TransformResult Transform(parser::ast::NodePtr node) = 0;

  /**
   * @brief トランスフォーマーの一意な名前を取得します (純粋仮想関数)。
   * @return トランスフォーマーの名前 (例: "ConstantFolding", "DeadCodeElimination")。
   */
  virtual std::string GetName() const = 0;

  /**
   * @brief トランスフォーマーの目的や機能に関する簡単な説明を取得します (純粋仮想関数)。
   * @return トランスフォーマーの説明。
   */
  virtual std::string GetDescription() const = 0;
};

/**
 * @typedef TransformerPtr
 * @brief ITransformer へのスマートポインタ型エイリアス。
 * @details トランスフォーマーの所有権管理を容易にします。
 *          通常、トランスフォーマーの所有権は明確なため unique_ptr が推奨されますが、
 *          状況に応じて shared_ptr も使用可能です。
 */
using TransformerPtr = std::unique_ptr<ITransformer>;

/**
 * @class Transformer
 * @brief AST ノードを再帰的に変換する基本的なトランスフォーマー実装。
 *
 * @details
 * `ITransformer` インターフェースを実装します。
 * このクラスは、AST ノードを受け取り、その子ノードを再帰的に変換し、
 * 必要に応じてノード自体を変換するための基本的な枠組みを提供します。
 *
 * 具体的な変換ロジックは、派生クラスで `TransformNode` メソッドを
 * オーバーライドすることによって実装されます。
 */
class Transformer : public ITransformer {
 public:
  /**
   * @brief コンストラクタ。
   * @param name トランスフォーマーの名前。
   * @param description トランスフォーマーの説明。
   */
  Transformer(std::string name, std::string description);

  /**
   * @brief デフォルトの仮想デストラクタ。
   */
  ~Transformer() override = default;

  // --- ITransformer インターフェースの実装 ---
  TransformResult Transform(parser::ast::NodePtr node) override;
  std::string GetName() const override;
  std::string GetDescription() const override;

 protected:
  /**
   * @brief ノードを再帰的に変換するための中心的な仮想メソッド。
   * @details
   * デフォルト実装では、ノードの子要素を再帰的に `TransformNode` で変換し、
   * 子に変更があった場合にノードを更新します。
   * 派生クラスは、特定のノードタイプに対してこのメソッドをオーバーライドし、
   * 独自の変換ロジックを実装します。変換を行わない場合や、デフォルトの
   * 子ノード走査を行いたい場合は、基底クラスの `TransformNode` を呼び出すか、
   * あるいは `TransformChildren` ヘルパーを呼び出すことができます。
   *
   * @param node 変換対象のノード (所有権を持つ unique_ptr)。
   * @return TransformResult 変換結果。
   */
  virtual TransformResult TransformNode(parser::ast::NodePtr node);

  /**
   * @brief ノードの子要素のみを再帰的に変換するヘルパーメソッド。
   * @details
   * `TransformNode` のデフォルト実装や、派生クラスが現在のノード自体は
   * 変換せずに子要素のみを変換したい場合に使用します。
   * このメソッドは非仮想です。
   * @param node 子を変換する対象のノード (所有権を持つ unique_ptr)。
   * @return TransformResult 子の変換結果。ノード自体は置き換えられません。
   */
  TransformResult TransformChildren(parser::ast::NodePtr node);

 private:
  std::string m_name;         ///< トランスフォーマーの名前 (読み取り専用)。
  std::string m_description;  ///< トランスフォーマーの説明 (読み取り専用)。

  // ヘルパー関数などをここに追加可能
  // 例: 特定の子ノードを変換する private メソッドなど
};