/*******************************************************************************
 * @file src/core/parser/ast/nodes/patterns/object_pattern.h
 * @brief ObjectPattern AST ノードクラスのヘッダーファイル。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * This file defines the ObjectPattern class, representing object destructuring
 * patterns according to the ESTree specification.
 * Example: `const { a, b: alias, ...rest } = someObject;`
 *
 * Corresponds to the ESTree 'ObjectPattern' interface:
 * interface ObjectPattern <: Pattern {
 *   type: "ObjectPattern";
 *   properties: Array<Property | RestElement>;
 * }
 *
 * Note: This reuses the `Property` node definition typically used for object
 *       literals, but the `value` within the `Property` node must be a `Pattern`
 *       (or `AssignmentPattern`) in this context, not an `Expression`.
 *       A `RestElement` can also appear as the last property.
 *
 * Adheres to AeroJS Coding Standards v1.2.
 ******************************************************************************/

#ifndef AEROJS_PARSER_AST_NODES_PATTERNS_OBJECT_PATTERN_H
#define AEROJS_PARSER_AST_NODES_PATTERNS_OBJECT_PATTERN_H

#include <memory>
#include <nlohmann/json_fwd.hpp>  // Forward declaration for nlohmann::json
#include <vector>

#include "src/core/parser/ast/node_type.h"                   // For NodeType
#include "src/core/parser/ast/nodes/expressions/property.h"  // Property ノードを再利用
#include "src/core/parser/ast/nodes/patterns/pattern.h"
#include "src/core/parser/ast/nodes/patterns/rest_spread.h"
#include "src/core/parser/ast/visitor/ast_visitor.h"  // For ASTVisitor
#include "src/core/parser/source_location.h"
#include "src/utils/json_writer.h"
#include "src/utils/macros.h"

namespace aerojs::parser::ast {

// Forward declare nodes used within properties
class Property;  // Assuming reuse from expressions/object_expression.h or similar
class RestElement;

/**
 * @brief オブジェクト分割代入パターンを表す AST ノード。
 * @details
 * ECMAScript 仕様における ObjectPattern を表現します。
 * 例: const { a, b: c, ...rest } = obj;
 *
 * このノードは、プロパティのリストを保持します。
 * 各プロパティは、通常のプロパティ (キーと値のペア) または RestElement のいずれかです。
 * 通常のプロパティには Property ノードを再利用します。
 * Property ノードは `expressions/property.h` で定義されています。
 * RestElement ノードは `patterns/rest_spread.h` で定義されています。
 */
class ObjectPattern final : public Pattern {
 public:
  /**
   * @brief プロパティ要素の型エイリアス。
   * Property または RestElement のいずれかへのポインタ。
   * Node を基底クラスとするポインタで保持します。
   */
  using PropertyElement = std::unique_ptr<Node>;
  using PropertyList = std::vector<PropertyElement>;

  /**
   * @brief ObjectPattern のコンストラクタ。
   * @param location ソースコード上の位置情報。
   * @param properties プロパティのリスト (Property または RestElement)。所有権はムーブされる。
   * @throws SyntaxError プロパティリストが無効な場合 (例: RestElement が最後でない、複数の RestElement)。
   */
  explicit ObjectPattern(const SourceLocation& location,
                         PropertyList&& properties);

  /**
   * @brief デフォルトデストラクタ。
   */
  virtual ~ObjectPattern() override = default;

  // コピーコンストラクタとコピー代入演算子を禁止
  DISALLOW_COPY_AND_ASSIGN(ObjectPattern);

  // ムーブコンストラクタ
  ObjectPattern(ObjectPattern&& other) noexcept;

  // ムーブ代入演算子
  ObjectPattern& operator=(ObjectPattern&& other) noexcept;

  /**
   * @brief ノードの型を取得します。
   * @return NodeType::OBJECT_PATTERN。
   */
  [[nodiscard]] virtual NodeType GetType() const noexcept override;

  /**
   * @brief プロパティのリスト (読み取り専用) を取得します。
   * @return プロパティリストへの const 参照。
   */
  [[nodiscard]] const PropertyList& GetProperties() const noexcept;

  /**
   * @brief プロパティのリスト (変更可能) を取得します。
   * @return プロパティリストへの非 const 参照。
   */
  [[nodiscard]] PropertyList& GetProperties() noexcept;

  /**
   * @brief AST ビジターを受け入れ、対応する Visit メソッドを呼び出します。
   * @param visitor AST ビジターの参照。
   */
  virtual void Accept(ASTVisitor& visitor) override;

  /**
   * @brief ノードの妥当性を検証します。
   * @details
   * - プロパティリスト内の各要素 (Property, RestElement) の妥当性を検証します。
   * - RestElement が存在する場合、それがリストの最後の要素であることを確認します。
   * - 複数の RestElement が存在しないことを確認します。
   * @throws SyntaxError 検証に失敗した場合。
   */
  virtual void Validate() const override;

  /**
   * @brief 子ノードのリストを取得します。
   * @return 子ノード (プロパティ要素) の非所有ポインタのリスト。
   */
  [[nodiscard]] virtual NodeList GetChildren() const override;

  /**
   * @brief ノードを文字列表現に変換します。
   * @param indent インデント文字列。
   * @return ノードの文字列表現。
   */
  [[nodiscard]] virtual std::string ToString(const std::string& indent = "") const override;

  /**
   * @brief ノードを JSON オブジェクトに変換します。
   * @param writer JSON ライター。
   */
  virtual void ToJson(utils::JsonWriter& writer) const override;

  /**
   * @brief ノードのディープコピーを作成します。
   * @return ディープコピーされた新しい ObjectPattern ノードの unique_ptr。
   */
  [[nodiscard]] virtual std::unique_ptr<Node> Clone() const override;

 private:
  PropertyList m_properties;  ///< プロパティのリスト (Property または RestElement)。

  /**
   * @brief 内部的な妥当性チェック (主に RestElement の制約)。
   * コンストラクタおよび Validate から呼び出されます。
   * @throws SyntaxError RestElement の配置が無効な場合。
   */
  void InternalValidate() const;
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_NODES_PATTERNS_OBJECT_PATTERN_H