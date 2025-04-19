/**
 * @file literals.h
 * @brief AeroJS AST のリテラルノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptの様々なリテラル値（文字列、数値、真偽値、null、
 * 正規表現、BigInt）を表すASTノード `Literal` を定義します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERALS_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERALS_H

#include <string>
#include <variant>
#include <optional>
#include <regex> // For RegExpLiteralValue
#include <cstdint> // For int64_t (BigInt)

#include "../node.h"
#include "../../visitors/ast_visitor.h"
#include "../../token.h" // For SourceLocation

// Forward declare BigInt type if necessary
// Assuming a placeholder or a dedicated BigInt class might exist elsewhere
// For now, we can use a placeholder like int64_t or a dedicated struct.
namespace aerojs {
namespace core {
// Potentially: #include "bigint/bigint.h"
struct BigIntValuePlaceholder { std::string value; }; // Placeholder
} // namespace core
}

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @struct RegExpLiteralValue
 * @brief 正規表現リテラルの値とフラグを保持する構造体。
 */
struct RegExpLiteralValue {
    std::string pattern; ///< 正規表現パターン本体
    std::string flags;   ///< 正規表現フラグ (例: "g", "i", "m", "u", "y", "s", "d")

    /**
     * @brief 等価比較演算子。
     * @param other 比較対象の RegExpLiteralValue。
     * @return パターンとフラグが両方等しい場合に true。
     */
    bool operator==(const RegExpLiteralValue& other) const {
        return pattern == other.pattern && flags == other.flags;
    }
};

/**
 * @typedef LiteralValue
 * @brief `Literal` ノードが保持できる値の型エイリアス。
 *
 * @details
 * `std::variant` を使用して、様々なリテラルタイプを効率的に格納します。
 * - `std::monostate`: null リテラル用 (または未設定状態)
 * - `bool`: 真偽値リテラル (true, false)
 * - `double`: 数値リテラル (浮動小数点数)
 * - `std::string`: 文字列リテラル
 * - `RegExpLiteralValue`: 正規表現リテラル
 * - `core::BigIntValuePlaceholder`: BigIntリテラル (プレースホルダー)
 */
using LiteralValue = std::variant<
    std::monostate, // Represents null
    bool,
    double,
    std::string,
    RegExpLiteralValue,
    core::BigIntValuePlaceholder // Replace with actual BigInt type if available
>;

/**
 * @enum LiteralType
 * @brief `Literal` ノードが表す具体的なリテラルの種類。
 *
 * @details
 * `LiteralValue` variant のどの型が有効であるかを示します。
 */
enum class LiteralKind {
    Null,
    Boolean,
    Numeric,
    String,
    RegExp,
    BigInt,
    Unknown // エラーまたは未定義の場合
};

/**
 * @class Literal
 * @brief JavaScriptのリテラル値を表すASTノードクラス。
 *
 * @details
 * 文字列、数値、真偽値、null、正規表現、BigIntなどのリテラル値を表現します。
 * 値は `std::variant` (`LiteralValue`) に格納され、具体的な種類は
 * `LiteralKind` で示されます。
 * また、リテラルの生のソースコード表現 (`m_raw`) も保持します。
 *
 * メモリ管理:
 * 値が文字列やBigIntの場合、そのデータは `LiteralValue` 内で管理されます。
 *
 * スレッド安全性:
 * `Node` クラスと同様に、`Literal` オブジェクト自体はスレッドセーフではありません。
 */
class Literal final : public Node {
public:
    /**
     * @brief Literalノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param value リテラルの値 (ムーブされる)。`LiteralValue` variant 型。
     * @param raw リテラルの生の文字列表現 (例: "\"hello\"", "1.2e3", "/abc/gi")。
     * @param parent 親ノード (オプション)。
     */
    explicit Literal(const SourceLocation& location,
                     LiteralValue&& value,
                     std::string raw,
                     Node* parent = nullptr);

    /**
     * @brief デストラクタ (デフォルト)。
     */
    ~Literal() override = default;

    // コピーとムーブのコンストラクタ/代入演算子はデフォルトを使用
    Literal(const Literal&) = delete; // コピー禁止
    Literal& operator=(const Literal&) = delete; // コピー代入禁止
    Literal(Literal&&) noexcept = default; // ムーブ許可
    Literal& operator=(Literal&&) noexcept = default; // ムーブ代入許可

    /**
     * @brief ノードタイプを取得します。
     * @return `NodeType::Literal`。
     */
    NodeType getType() const noexcept override final { return NodeType::Literal; }

    /**
     * @brief リテラルの値を取得します。
     * @return リテラルの値を保持する `LiteralValue` variant へのconst参照。
     */
    const LiteralValue& getValue() const noexcept;

    /**
     * @brief リテラルの生の文字列表現を取得します。
     * @return ソースコード上のリテラルの生の文字列。
     */
    const std::string& getRaw() const noexcept;

    /**
     * @brief リテラルの具体的な種類を取得します。
     * @return リテラルの種類を示す `LiteralKind`。
     */
    LiteralKind getKind() const noexcept;

    /**
     * @brief ビジターを受け入れます (非const版)。
     * @param visitor 訪問するビジター。
     */
    void accept(AstVisitor* visitor) override final;

    /**
     * @brief ビジターを受け入れます (const版)。
     * @param visitor 訪問するconstビジター。
     */
    void accept(ConstAstVisitor* visitor) const override final;

    /**
     * @brief 子ノードのリストを取得します。
     * @details リテラルノードは子ノードを持たないため、空のベクターを返します。
     * @return 空の `std::vector<Node*>`。
     */
    std::vector<Node*> getChildren() override final;

    /**
     * @brief 子ノードのリストを取得します (const版)。
     * @return 空の `std::vector<const Node*>`。
     */
    std::vector<const Node*> getChildren() const override final;

    /**
     * @brief このノードをJSON表現に変換します。
     * @param pretty フォーマットするかどうか。
     * @return ノードを表すJSONオブジェクト。
     */
    nlohmann::json toJson(bool pretty = false) const override final;

    /**
     * @brief このノードを文字列表現に変換します。
     * @return "Literal[value=...]" 形式の文字列。
     */
    std::string toString() const override final;

private:
    LiteralValue m_value; ///< リテラルの値 (variant)
    std::string m_raw;    ///< リテラルの生の文字列表現
    LiteralKind m_kind;   ///< リテラルの種類

    // 値から種類を決定するヘルパー関数
    static LiteralKind determineKind(const LiteralValue& value);
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_LITERALS_H 