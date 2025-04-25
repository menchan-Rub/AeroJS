/*******************************************************************************
 * @file src/core/parser/ast/nodes/patterns/assignment_pattern.h
 * @brief AssignmentPattern AST ノード (例: `[a = 1]` や `{b = 2}` の `=`) のヘッダーファイル。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、ECMAScript の分割代入におけるデフォルト値を表現する
 * AssignmentPattern クラスを定義します。
 * 例: `const { key = defaultValue } = obj;` や `function fn(p = defaultValue) {}`
 *
 * Corresponds to the ESTree 'AssignmentPattern' interface:
 * interface AssignmentPattern <: Pattern {
 *   type: "AssignmentPattern";
 *   left: Pattern;
 *   right: Expression; // Default value expression
 * }
 *
 * Adheres to AeroJS Coding Standards v1.2.
 ******************************************************************************/

#ifndef AEROJS_PARSER_AST_NODES_PATTERNS_ASSIGNMENT_PATTERN_H
#define AEROJS_PARSER_AST_NODES_PATTERNS_ASSIGNMENT_PATTERN_H

#include <memory>  // For std::unique_ptr
#include <vector>  // For NodeList typedef in Node.h

#include "src/core/parser/ast/node_type.h"
#include "src/core/parser/ast/nodes/expression.h"
#include "src/core/parser/ast/nodes/node.h"  // Forward declaration needed?
#include "src/core/parser/ast/nodes/pattern.h"
#include "src/core/parser/ast/source_location.h"

namespace aerojs::parser::ast {

class ASTVisitor;
namespace utils {
class JsonWriter;
}

/**
 * @class AssignmentPattern
 * @brief AST ノード: 代入パターン (Assignment Pattern)。
 *
 * 分割代入においてデフォルト値を指定するために使用されます。
 * 例: `[a = 1] = []` における `a = 1`
 * 例: `{b: c = 2} = {}` における `c = 2`
 *
 * ECMAScript Spec: 13.3.3 Destructuring Assignment
 *                  14.5.14 Destructuring Binding Patterns
 */
class AssignmentPattern final : public Pattern {
 public:
  /**
   * @brief AssignmentPattern ノードを構築します。
   * @param location ノードのソース位置情報。
   * @param left パターン (左辺)。Identifier または他の Pattern。
   * @param right デフォルト値として使用される式 (右辺)。
   * @throws std::invalid_argument left または right が null の場合。
   */
  explicit AssignmentPattern(const SourceLocation& location,
                             std::unique_ptr<Pattern> left,
                             std::unique_ptr<Expression> right);

  /**
   * @brief デストラクタ。
   * 仮想デストラクタを定義して、ポリモーフィックな削除を可能にします。
   */
  virtual ~AssignmentPattern() override = default;

  // --- ムーブセマンティクス ---
  AssignmentPattern(AssignmentPattern&& other) noexcept;
  AssignmentPattern& operator=(AssignmentPattern&& other) noexcept;

  // --- コピーセマンティクス (禁止) ---
  // ASTノードは通常、所有権を持つ unique_ptr で管理されるため、
  // コピーコンストラクタとコピー代入演算子は明示的に禁止します。
  // 必要であれば Clone() メソッドを使用してください。
  AssignmentPattern(const AssignmentPattern&) = delete;
  AssignmentPattern& operator=(const AssignmentPattern&) = delete;

  // --- Getters ---
  NodeType GetType() const noexcept override;

  /**
   * @brief 左辺のパターンを取得します。
   * @return 左辺のパターンへの const 参照。
   */
  [[nodiscard]] const Pattern& GetLeft() const noexcept;

  /**
   * @brief 左辺のパターンを取得します (非 const 版)。
   * @return 左辺のパターンへの参照。
   */
  [[nodiscard]] Pattern& GetLeft() noexcept;

  /**
   * @brief 右辺の式 (デフォルト値) を取得します。
   * @return 右辺の式への const 参照。
   */
  [[nodiscard]] const Expression& GetRight() const noexcept;

  /**
   * @brief 右辺の式 (デフォルト値) を取得します (非 const 版)。
   * @return 右辺の式への参照。
   */
  [[nodiscard]] Expression& GetRight() noexcept;

  // --- AST Visitor ---
  /**
   * @brief ASTVisitor を受け入れ、対応する Visit メソッドを呼び出します。
   * @param visitor ASTVisitor のインスタンス。
   */
  void Accept(ASTVisitor& visitor) override;

  // --- Validation ---
  /**
   * @brief ノードとその子ノードの構造的な正当性を検証します。
   * @throws error::SyntaxError 検証に失敗した場合。
   */
  void Validate() const override;

  // --- Child Nodes ---
  /**
   * @brief このノードが直接持つ子ノードのリストを取得します。
   * @return 子ノード (Node*) のリスト。
   */
  [[nodiscard]] NodeList GetChildren() const override;

  // --- Serialization ---
  /**
   * @brief ノードをデバッグ目的の文字列表現に変換します。
   * @param indent インデント文字列。
   * @return ノードの文字列表現。
   */
  [[nodiscard]] std::string ToString(const std::string& indent = "") const override;

  /**
   * @brief ノードを JSON 形式でシリアライズします。
   * @param writer JsonWriter のインスタンス。
   */
  void ToJson(utils::JsonWriter& writer) const override;

  // --- Cloning ---
  /**
   * @brief ノードのディープコピーを作成します。
   * @return このノードの新しいインスタンスへのユニークポインタ。
   */
  [[nodiscard]] std::unique_ptr<Node> Clone() const override;

 private:
  std::unique_ptr<Pattern> m_left;      ///< パターン (左辺)。
  std::unique_ptr<Expression> m_right;  ///< デフォルト値の式 (右辺)。
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_NODES_PATTERNS_ASSIGNMENT_PATTERN_H