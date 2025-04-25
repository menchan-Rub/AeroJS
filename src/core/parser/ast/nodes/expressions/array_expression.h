/**
 * @file array_expression.h
 * @brief AST (Abstract Syntax Tree) における ArrayExpression (配列式) ノードの定義を提供します。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 * @details このヘッダーファイルは、JavaScript の配列リテラル（例: `[1, "hello", , ...spread]`）を表現する
 *          `ArrayExpression` クラスを定義します。配列要素には、式、スプレッド要素、または空要素（elision）を
 *          含めることができます。
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H

#include <memory>   // std::unique_ptr
#include <string>   // std::string
#include <variant>  // std::variant (将来的な拡張用、現在はNode*)
#include <vector>   // std::vector

#include "core/parser/ast/nodes/expression_node.h"      // 基底クラス ExpressionNode
#include "core/parser/ast/nodes/node_interface.h"       // Node インターフェース (基底)
#include "core/parser/ast/source_location.h"            // SourceLocation
#include "core/parser/ast/visitor/ast_visitor.h"        // AstVisitor
#include "core/parser/ast/visitor/const_ast_visitor.h"  // ConstAstVisitor
#include "nlohmann/json.hpp"                            // JSON サポート

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- 前方宣言 ---
// これらは ArrayExpression の要素として含まれる可能性があるため、前方宣言します。
class ExpressionNode;  // 配列要素は式である可能性があります
class SpreadElement;   // 配列要素はスプレッド構文である可能性があります

/**
 * @class ArrayExpression
 * @brief 配列リテラル（例: `[1, 2, , ...c]`）を表す AST ノード。
 * @details このクラスは `ExpressionNode` を継承し、JavaScript の配列式を表現します。
 *          要素のリストを保持し、各要素は式 (`ExpressionNode`)、スプレッド要素 (`SpreadElement`)、
 *          または空要素 (`nullptr`) のいずれかです。
 *          このクラスは final であり、これ以上の派生はできません。
 */
class ArrayExpression final : public ExpressionNode {
 public:
  /**
   * @using ElementType
   * @brief 配列要素の型エイリアス。
   * @details 配列の要素は、式 (`ExpressionNode` の派生クラス)、スプレッド要素 (`SpreadElement`)、
   *          または空要素 (elision、`nullptr` で表現) のいずれかです。
   *          所有権を管理するために `std::unique_ptr<Node>` を使用します。
   *          これにより、多様なノード型を統一的に扱いつつ、メモリリークを防ぎます。
   *          将来的に `std::variant<std::unique_ptr<ExpressionNode>, std::unique_ptr<SpreadElement>, std::nullptr_t>`
   *          のような、より型安全な表現への移行も検討可能です。
   */
  using ElementType = std::unique_ptr<Node>;

  /**
   * @brief ArrayExpression ノードのコンストラクタ。
   * @param elements 配列の要素リスト。各要素は `ElementType` (式、スプレッド要素、または `nullptr`) です。
   *                 このベクターはムーブされます。
   * @param location この配列式がソースコード内で占める位置情報。
   * @param parent このノードの親ノードへのポインタ (オプション、デフォルトは `nullptr`)。
   *               親ノードは所有権を持ちません。
   * @throws std::bad_alloc メモリ確保に失敗した場合。
   * @throws // 他の潜在的な例外 (要素の検証など、実装に応じて)
   */
  ArrayExpression(std::vector<ElementType>&& elements,
                  const SourceLocation& location,
                  Node* parent = nullptr);

  /**
   * @brief デストラクタ。
   * @details `m_elements` 内の `std::unique_ptr` が所有するノードを再帰的に解放します。
   *          `override` キーワードにより、基底クラスの仮想デストラクタをオーバーライドしていることを明示します。
   *          `default` 指定により、コンパイラが生成するデフォルトのデストラクタを使用します。
   *          これは `noexcept` であることが期待されます（メンバーと基底クラスのデストラクタが `noexcept` の場合）。
   */
  ~ArrayExpression() override = default;

  // --- コピー禁止 ---
  // AST ノードは通常、一意の存在であり、安易なコピーは意図しない共有や所有権の問題を引き起こすため、
  // コピーコンストラクタとコピー代入演算子を禁止します。
  ArrayExpression(const ArrayExpression&) = delete;
  ArrayExpression& operator=(const ArrayExpression&) = delete;

  // --- ムーブ許可 ---
  // ムーブコンストラクタとムーブ代入演算子はデフォルトで良いでしょう。
  // unique_ptr を持つため、デフォルトのムーブ操作が適切に機能します。
  ArrayExpression(ArrayExpression&&) noexcept = default;
  ArrayExpression& operator=(ArrayExpression&&) noexcept = default;

  /**
   * @brief 配列の要素リストへの const 参照を取得します。
   * @return 要素のベクター (`std::vector<ElementType>`) への const 参照。
   *         要素が空要素 (elision) の場合は `nullptr` が格納されています。
   * @note このメソッドは `noexcept` であり、例外をスローしません。
   */
  [[nodiscard]] const std::vector<ElementType>& getElements() const noexcept;

  /**
   * @brief AstVisitor を受け入れ、ビジット処理を実行します (Visitor パターン)。
   * @param visitor ビジット処理を行う AstVisitor オブジェクトへの参照。
   * @details このメソッドは Visitor パターンの実装の一部であり、ダブルディスパッチを通じて
   *          具体的なノードタイプに基づいた処理を可能にします。
   */
  void accept(AstVisitor& visitor) override;

  /**
   * @brief ConstAstVisitor を受け入れ、ビジット処理を実行します (Visitor パターン)。
   * @param visitor ビジット処理を行う ConstAstVisitor オブジェクトへの参照。
   * @details このメソッドは Visitor パターンの const 版実装の一部です。
   *          ノードの状態を変更しないビジット処理に使用されます。
   */
  void accept(ConstAstVisitor& visitor) const override;

  /**
   * @brief このノードが持つ子ノードのリストを取得します。
   * @return 子ノード (`Node*`) のベクター。空要素 (`nullptr`) は含まれません。
   * @details `m_elements` 内の非 `nullptr` 要素へのポインタを返します。
   *          AST のトラバーサルなどに使用されます。
   */
  [[nodiscard]] std::vector<Node*> getChildren() override;

  /**
   * @brief このノードが持つ子ノードのリスト (const 版) を取得します。
   * @return 子ノード (`const Node*`) のベクター。空要素 (`nullptr`) は含まれません。
   * @details `m_elements` 内の非 `nullptr` 要素への const ポインタを返します。
   *          AST の読み取り専用トラバーサルなどに使用されます。
   */
  [[nodiscard]] std::vector<const Node*> getChildren() const override;

  /**
   * @brief このノードの JSON 表現を生成します。
   * @param pretty 整形された JSON 文字列を生成するかどうか (デフォルトは `false`)。
   * @return このノードを表す `nlohmann::json` オブジェクト。
   * @details デバッグ、シリアライズ、またはツール連携のために AST 構造を JSON 形式で出力します。
   *          `type`, `loc`, `elements` などのプロパティを含むことが期待されます。
   *          要素が `nullptr` の場合は JSON `null` として表現されるべきです。
   */
  [[nodiscard]] nlohmann::json toJson(bool pretty = false) const override;

  /**
   * @brief このノードの文字列表現を生成します (主にデバッグ用途)。
   * @return このノードを表す文字列。
   * @details 例えば `"[1, , ...spread]"` のような形式で、ノードの内容を簡潔に示します。
   *          `toString()` はデバッグやログ出力に役立ちますが、`toJson()` ほど構造化されていません。
   */
  [[nodiscard]] std::string toString() const override;

 private:
  /**
   * @var m_elements
   * @brief 配列の要素を格納するベクター。
   * @details 各要素は `ElementType` (現在は `std::unique_ptr<Node>`) です。
   *          `nullptr` は配列内の空要素 (elision) を表します。
   *          例: `[a, , b]` は `{ unique_ptr<a>, nullptr, unique_ptr<b> }` のように格納されます。
   *          要素の順序はソースコードにおける出現順序と一致します。
   */
  std::vector<ElementType> m_elements;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARRAY_EXPRESSION_H