/**
 * @file literal.h
 * @brief AST Literal ノードの定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERAL_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERAL_H

#include <cstdint>  // For int64_t (BigInt)
#include <regex>    // For std::regex if representing regex literals directly
#include <string>
#include <variant>

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// 正規表現リテラルを表す構造体 (例)
struct RegExpLiteral {
  std::string pattern;
  std::string flags;

  bool operator==(const RegExpLiteral& other) const {
    return pattern == other.pattern && flags == other.flags;
  }
};

/**
 * @typedef LiteralValue
 * @brief リテラルが保持できる値の型
 */
using LiteralValue = std::variant<
    std::nullptr_t,  // null
    bool,            // true, false
    double,          // Numbers (including integers that fit in double)
    std::string,     // Strings and BigInts (BigInts as strings for precision)
    RegExpLiteral    // Regular Expressions
                     // Note: Consider a dedicated BigInt type if string representation is insufficient
    >;

/**
 * @enum LiteralType
 * @brief Literal ノードの種類
 */
enum class LiteralType {
  Null,
  Boolean,
  Number,
  String,
  RegExp,
  BigInt
};

/**
 * @class Literal
 * @brief リテラル値を表す AST ノード (例: "hello", 123, true, null, /abc/g, 123n)
 */
class Literal final : public ExpressionNode {
 public:
  /**
   * @brief Literal ノードのコンストラクタ
   * @param value リテラルの値 (LiteralValue 型)
   * @param rawValue ソースコード上のリテラルの生の文字列表現 (例: "\"hello\\n\"", "1.2e3")
   * @param type リテラルの種類 (LiteralType)
   * @param location ソースコード上の位置情報
   * @param parent 親ノード (オプション)
   */
  Literal(LiteralValue value,
          std::string rawValue,
          LiteralType type,
          const SourceLocation& location,
          Node* parent = nullptr);

  ~Literal() override = default;

  /**
   * @brief リテラルの値を取得します。
   * @return リテラルの値 (std::variant)
   */
  const LiteralValue& getValue() const noexcept;

  /**
   * @brief リテラルの種類を取得します。
   * @return リテラルの種類 (LiteralType)
   */
  LiteralType getLiteralType() const noexcept;

  /**
   * @brief リテラルの生の文字列表現を取得します。
   * @return 生の文字列表現 (std::string)
   */
  const std::string& getRawValue() const noexcept;

  // Visitor パターンを受け入れる
  void accept(AstVisitor& visitor) override;
  void accept(ConstAstVisitor& visitor) const override;

  // 子ノードは持たない
  std::vector<Node*> getChildren() override;
  std::vector<const Node*> getChildren() const override;

  // JSON 表現を生成
  nlohmann::json toJson(bool pretty = false) const override;

  // 文字列表現を生成 (デバッグ用)
  std::string toString() const override;

 private:
  LiteralValue m_value;       ///< リテラルの値
  std::string m_rawValue;     ///< ソース上の生の文字列表現
  LiteralType m_literalType;  ///< リテラルの種類
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERAL_H