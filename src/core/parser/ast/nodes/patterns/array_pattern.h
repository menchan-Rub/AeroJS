/*******************************************************************************
 * @file src/core/parser/ast/nodes/patterns/array_pattern.h
 * @brief ArrayPattern AST ノードクラスの定義。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * This file defines the ArrayPattern class, representing array destructuring
 * patterns according to the ESTree specification.
 * Example: `const [a, , b, ...rest] = someArray;`
 *
 * Corresponds to the ESTree 'ArrayPattern' interface:
 * interface ArrayPattern <: Pattern {
 *   type: "ArrayPattern";
 *   elements: Array<Pattern | null>; // null represents an array hole
 * }
 *
 * Adheres to AeroJS Coding Standards v1.2.
 ******************************************************************************/

#ifndef AEROJS_PARSER_AST_NODES_PATTERNS_ARRAY_PATTERN_H
#define AEROJS_PARSER_AST_NODES_PATTERNS_ARRAY_PATTERN_H

#include <memory>
#include <nlohmann/json_fwd.hpp>
#include <vector>

#include "src/core/parser/ast/ast_node_base.h"
#include "src/core/parser/ast/nodes/pattern.h"
#include "src/core/parser/ast/source_location.h"
#include "src/core/parser/ast/visitor/ast_visitor.h"

namespace aerojs::parser::ast {

// Forward declare RestElement as it can be an element
class RestElement;

/**
 * @brief 配列パターンを表す AST ノード。
 * @details 配列の分割代入に使用されます。
 * 例: `const [a, , b] = arr;`
 *      `function foo([x, y]) { ... }`
 * 要素リストには、配列の穴を表すために nullptr を含めることができます。
 * `RestElement` も要素として許可されます (ただし、最後の要素である必要があります)。
 */
class ArrayPattern final : public Pattern {
 public:
  /**
   * @brief ArrayPattern の要素リストの型エイリアス。
   * PatternPtr のベクター。要素は Pattern または RestElement (最後の要素のみ) であるべきです。
   * nullptr は配列の穴 (empty slot) を表します。
   */
  using ElementList = std::vector<NodePtr>;  // Allows RestElement and nullptrs

  /**
   * @brief ArrayPattern オブジェクトを構築します。
   * @param location ノードのソース位置情報。
   * @param elements パターン要素のリスト (PatternPtr または nullptr)。要素の所有権はこのノードに移譲されます。
   * @throws std::invalid_argument elements が不正な場合 (例: RestElement が最後以外の位置にある場合など)。
   *         パーサーが検証を行うべきですが、コンストラクタでも基本的なチェックを行います。
   */
  explicit ArrayPattern(const SourceLocation& location,
                        ElementList&& elements);

  /**
   * @brief デフォルトデストラクタ。
   */
  virtual ~ArrayPattern() override = default;

  // Prevent copy construction and assignment
  ArrayPattern(const ArrayPattern&) = delete;
  ArrayPattern& operator=(const ArrayPattern&) = delete;

  /**
   * @brief ムーブコンストラクタ。
   * @param other ムーブ元の ArrayPattern オブジェクト。
   */
  ArrayPattern(ArrayPattern&& other) noexcept;

  /**
   * @brief ムーブ代入演算子。
   * @param other ムーブ元の ArrayPattern オブジェクト。
   * @return *this への参照。
   */
  ArrayPattern& operator=(ArrayPattern&& other) noexcept;

  /**
   * @brief ノードタイプを取得します。
   * @return NodeType::ARRAY_PATTERN
   */
  [[nodiscard]] NodeType GetType() const noexcept override;

  /**
   * @brief パターン要素のリストを取得します (const 版)。
   * @return 要素のリスト (const 参照)。 nullptr は配列の穴を表します。
   */
  [[nodiscard]] const ElementList& GetElements() const noexcept;

  /**
   * @brief パターン要素のリストを取得します (非 const 版)。
   * @return 要素のリスト (非 const 参照)。 nullptr は配列の穴を表します。
   */
  [[nodiscard]] ElementList& GetElements() noexcept;

  /**
   * @brief AST ビジターを受け入れ、対応する Visit メソッドを呼び出します。
   * @param visitor AST ビジターの参照。
   */
  void Accept(ASTVisitor& visitor) override;

  /**
   * @brief ノードの妥当性を検証します。
   * @details 要素が Pattern または最後の RestElement であること、nullptr が妥当な位置にあることを確認します。
   * @throws error::SyntaxError 検証に失敗した場合。
   */
  void Validate() const override;

  /**
   * @brief 子ノードのリストを取得します。
   * @details null でない要素ノードを返します。
   * @return 子ノードのリスト (Node* の vector)。
   */
  [[nodiscard]] NodeList GetChildren() const override;

  /**
   * @brief ノードの文字列表現を取得します。
   * @param indent インデント文字列。
   * @return ノードの文字列表現。
   */
  [[nodiscard]] std::string ToString(const std::string& indent = "") const override;

  /**
   * @brief ノードの JSON 表現を JsonWriter に書き込みます。
   * @param writer JsonWriter オブジェクト。
   */
  void ToJson(utils::JsonWriter& writer) const override;

  /**
   * @brief ノードのディープコピーを作成します。
   * @return このノードの新しいディープコピー (unique_ptr)。
   * @throws std::runtime_error クローン作成に失敗した場合。
   */
  [[nodiscard]] std::unique_ptr<Node> Clone() const override;

 private:
  ElementList m_elements;  ///< パターン要素のリスト (PatternPtr、RestElementPtr または nullptr)
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_NODES_PATTERNS_ARRAY_PATTERN_H