/**
 * @file symbol.h
 * @brief JavaScript のシンボル型の定義
 */

#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <functional> // for std::hash
#include <string_view> // より効率的な文字列操作のため
#include <optional>

namespace aerojs {
namespace core {

// 前方宣言とスマートポインタの型エイリアス定義
class Symbol;
using SymbolPtr = std::shared_ptr<Symbol>;
using SymbolWeakPtr = std::weak_ptr<Symbol>;

/**
 * @brief JavaScript のシンボル型を表すクラス
 * 
 * Symbol クラスは JavaScript の Symbol 型を実装します。
 * Symbol はユニークな識別子として機能し、ECMAScript 仕様に準拠した
 * 完全な実装を提供します。シンボルは不変オブジェクトとして設計されています。
 * 
 * このクラスはスレッドセーフに設計され、効率的なグローバルレジストリとWell-knownシンボルの
 * キャッシングを提供します。シンボルはJavaScriptエンジン全体で一意であり、
 * 効率的な比較やハッシュテーブルでの使用が可能です。
 */
class Symbol : public std::enable_shared_from_this<Symbol> {
public:
    /**
     * @brief 説明付きシンボルを作成する
     * 
     * @param description シンボルの説明（オプション）
     */
    explicit Symbol(const std::string& description = "");

    /**
     * @brief キー登録の有無を指定してシンボルを作成する
     * 
     * @param description シンボルの説明
     * @param registerKey キーレジストリに登録するかどうか
     */
    Symbol(const std::string& description, bool registerKey);

    /**
     * @brief ムーブコンストラクタ - 高効率な移動操作
     * 
     * @param other 移動元のシンボル
     */
    Symbol(Symbol&& other) noexcept;

    /**
     * @brief ムーブ代入演算子 - 高効率な移動操作
     * 
     * @param other 移動元のシンボル
     * @return *this
     */
    Symbol& operator=(Symbol&& other) noexcept;

    /**
     * @brief コピーコンストラクタ (削除)
     * シンボルはコピー不可能な一意なオブジェクト
     */
    Symbol(const Symbol&) = delete;

    /**
     * @brief コピー代入演算子 (削除)
     * シンボルはコピー不可能な一意なオブジェクト
     */
    Symbol& operator=(const Symbol&) = delete;

    /**
     * @brief デストラクタ
     * レジストリからこのシンボルへの参照を安全に削除します
     */
    ~Symbol();

    /**
     * @brief 説明付きシンボルを作成する
     * 
     * @param description シンボルの説明（オプション）
     * @return 新しいシンボルのスマートポインタ
     */
    static SymbolPtr create(const std::string& description = "");

    /**
     * @brief キー登録の有無を指定してシンボルを作成する
     * 
     * @param description シンボルの説明
     * @param registerKey キーレジストリに登録するかどうか
     * @return 新しいシンボルのスマートポインタ
     */
    static SymbolPtr create(const std::string& description, bool registerKey);

    /**
     * @brief シンボルの説明を取得する
     * 
     * @return シンボルの説明
     */
    constexpr const std::string& description() const noexcept {
        return m_description;
    }

    /**
     * @brief シンボルの説明を文字列ビューとして取得する (高効率)
     * 
     * @return シンボルの説明の文字列ビュー
     */
    constexpr std::string_view descriptionView() const noexcept {
        return std::string_view(m_description);
    }

    /**
     * @brief シンボルの一意な識別子を取得する
     * 
     * @return シンボルのID
     */
    constexpr int id() const noexcept {
        return m_id;
    }

    /**
     * @brief シンボルの文字列表現を取得する
     * 
     * @return シンボルの文字列表現（"Symbol(description)"形式）
     */
    std::string toString() const;

    /**
     * @brief デバッグ用の詳細な文字列表現を取得する
     * 
     * @return デバッグ用の文字列表現（ID情報を含む）
     */
    std::string debugString() const;

    /**
     * @brief 等価比較演算子
     * 
     * @param other 比較対象のシンボル
     * @return 同一シンボルの場合はtrue
     */
    constexpr bool operator==(const Symbol& other) const noexcept {
        return m_id == other.m_id;
    }

    /**
     * @brief 非等価比較演算子
     * 
     * @param other 比較対象のシンボル
     * @return 異なるシンボルの場合はtrue
     */
    constexpr bool operator!=(const Symbol& other) const noexcept {
        return m_id != other.m_id;
    }

    /**
     * @brief 二つのシンボルポインタが同じシンボルを指しているか比較する
     * 
     * @param lhs 左辺のシンボルポインタ
     * @param rhs 右辺のシンボルポインタ
     * @return 同じシンボルを指す場合はtrue
     */
    static constexpr bool equals(const SymbolPtr& lhs, const SymbolPtr& rhs) noexcept;

    /**
     * @brief グローバルシンボルレジストリ内のキーに対応するシンボルを取得または作成する
     * 
     * シンボルレジストリに指定されたキーが存在する場合はそのシンボルを返し、
     * 存在しない場合は新たに作成して登録します。
     * この操作はスレッドセーフです。
     * 
     * @param key グローバルシンボルのキー
     * @return キーに対応するシンボル
     * @throws std::invalid_argument キーが空の場合
     */
    static SymbolPtr for_(const std::string& key);

    /**
     * @brief シンボルに対応するグローバルシンボルレジストリのキーを取得する
     * 
     * 効率的なO(1)で逆引きを実行します。
     * 
     * @param symbol キーを取得するシンボル
     * @return グローバルシンボルレジストリのキー（存在しない場合は空文字列）
     * @throws std::invalid_argument シンボルがnullの場合
     */
    static std::string keyFor(const SymbolPtr& symbol);

    /**
     * @brief シンボルに対応するグローバルシンボルレジストリのキーを取得する
     * 
     * @param symbol キーを取得するシンボル
     * @return グローバルシンボルレジストリのキー（存在しない場合は空文字列）
     * @throws std::invalid_argument シンボルがnullの場合
     */
    static std::string keyFor(Symbol* symbol);

    /**
     * @brief 指定されたIDを持つシンボルを取得する
     * 
     * @param id 検索するシンボルのID
     * @return 指定されたIDを持つシンボルのオプション（存在しない場合はnullopt）
     */
    static std::optional<SymbolPtr> getById(int id);

    /**
     * @brief グローバルシンボルレジストリのサイズを取得する
     * 
     * @return レジストリ内のシンボル数
     */
    static size_t registrySize();

    /**
     * @brief グローバルシンボルレジストリの内容をデバッグ用に文字列化する
     * 
     * @return レジストリ内容の文字列表現
     */
    static std::string dumpRegistry();

    /**
     * @brief カスタムシンボルをレジストリに登録する
     * 
     * @param name シンボルの名前
     * @param symbol 登録するシンボル
     * @throws std::invalid_argument 名前が空またはシンボルがnullの場合
     * @throws std::invalid_argument 既に同じ名前で異なるシンボルが登録されている場合
     */
    static void registerCustomSymbol(const std::string& name, SymbolPtr symbol);

    /**
     * @brief シンボルレジストリをリセットする（主にテスト用）
     * 
     * 警告: このメソッドはテスト環境でのみ使用し、実行中のアプリケーションでは
     * 使用しないでください。レジストリのリセットはメモリリークを引き起こす可能性があります。
     */
    static void resetRegistry();

    /**
     * @brief レジストリから特定のシンボルを安全に削除する
     * 
     * @param symbolId 削除するシンボルのID
     * @return 削除に成功した場合はtrue、シンボルが見つからない場合はfalse
     */
    static bool removeFromRegistry(int symbolId);

    // ECMAScript 仕様に基づく Well-known シンボル
    static SymbolPtr hasInstance();
    static SymbolPtr isConcatSpreadable();
    static SymbolPtr iterator();
    static SymbolPtr asyncIterator();
    static SymbolPtr match();
    static SymbolPtr matchAll();
    static SymbolPtr replace();
    static SymbolPtr search();
    static SymbolPtr species();
    static SymbolPtr split();
    static SymbolPtr toPrimitive();
    static SymbolPtr toStringTag();
    static SymbolPtr unscopables();

private:
    std::string m_description; ///< シンボルの説明
    int m_id;                  ///< シンボルの一意な識別子

    // グローバルシンボルレジストリの管理
    static std::atomic<int> s_nextId;                         ///< 次のシンボルID
    static std::unordered_map<std::string, SymbolPtr> s_registry;  ///< グローバルシンボルレジストリ
    static std::unordered_map<int, SymbolPtr> s_keyRegistry;       ///< IDによるシンボルレジストリ
    static std::unordered_map<SymbolPtr, std::string,    ///< シンボルから名前への逆引きマップ
                             std::hash<SymbolPtr>,
                             std::equal_to<SymbolPtr>> s_reverseRegistry; 
    static std::mutex s_mutex;                                ///< レジストリ操作用ミューテックス

    // Well-known シンボルのための静的変数
    static SymbolPtr s_hasInstanceSymbol;
    static SymbolPtr s_isConcatSpreadableSymbol;
    static SymbolPtr s_iteratorSymbol;
    static SymbolPtr s_asyncIteratorSymbol;
    static SymbolPtr s_matchSymbol;
    static SymbolPtr s_matchAllSymbol;
    static SymbolPtr s_replaceSymbol;
    static SymbolPtr s_searchSymbol;
    static SymbolPtr s_speciesSymbol;
    static SymbolPtr s_splitSymbol;
    static SymbolPtr s_toPrimitiveSymbol;
    static SymbolPtr s_toStringTagSymbol;
    static SymbolPtr s_unscopablesSymbol;
};

/**
 * @brief Well-known シンボルへの簡易アクセスを提供する名前空間
 */
namespace well_known {

/**
 * @brief Symbol.asyncIterator を取得する
 * @return Symbol.asyncIterator シンボル
 */
inline SymbolPtr getAsyncIterator() {
    return Symbol::asyncIterator();
}

/**
 * @brief Symbol.hasInstance を取得する
 * @return Symbol.hasInstance シンボル
 */
inline SymbolPtr getHasInstance() {
    return Symbol::hasInstance();
}

/**
 * @brief Symbol.isConcatSpreadable を取得する
 * @return Symbol.isConcatSpreadable シンボル
 */
inline SymbolPtr getIsConcatSpreadable() {
    return Symbol::isConcatSpreadable();
}

/**
 * @brief Symbol.iterator を取得する
 * @return Symbol.iterator シンボル
 */
inline SymbolPtr getIterator() {
    return Symbol::iterator();
}

/**
 * @brief Symbol.match を取得する
 * @return Symbol.match シンボル
 */
inline SymbolPtr getMatch() {
    return Symbol::match();
}

/**
 * @brief Symbol.matchAll を取得する
 * @return Symbol.matchAll シンボル
 */
inline SymbolPtr getMatchAll() {
    return Symbol::matchAll();
}

/**
 * @brief Symbol.replace を取得する
 * @return Symbol.replace シンボル
 */
inline SymbolPtr getReplace() {
    return Symbol::replace();
}

/**
 * @brief Symbol.search を取得する
 * @return Symbol.search シンボル
 */
inline SymbolPtr getSearch() {
    return Symbol::search();
}

/**
 * @brief Symbol.species を取得する
 * @return Symbol.species シンボル
 */
inline SymbolPtr getSpecies() {
    return Symbol::species();
}

/**
 * @brief Symbol.split を取得する
 * @return Symbol.split シンボル
 */
inline SymbolPtr getSplit() {
    return Symbol::split();
}

/**
 * @brief Symbol.toPrimitive を取得する
 * @return Symbol.toPrimitive シンボル
 */
inline SymbolPtr getToPrimitive() {
    return Symbol::toPrimitive();
}

/**
 * @brief Symbol.toStringTag を取得する
 * @return Symbol.toStringTag シンボル
 */
inline SymbolPtr getToStringTag() {
    return Symbol::toStringTag();
}

/**
 * @brief Symbol.unscopables を取得する
 * @return Symbol.unscopables シンボル
 */
inline SymbolPtr getUnscopables() {
    return Symbol::unscopables();
}

} // namespace well_known

// シンボルポインタの等価比較（インライン）
constexpr bool Symbol::equals(const SymbolPtr& lhs, const SymbolPtr& rhs) noexcept {
    // null チェックを含む安全な比較
    if (lhs == nullptr && rhs == nullptr) {
        return true;
    }
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }
    return *lhs == *rhs;
}

} // namespace core
} // namespace aerojs

// std::hashのための特殊化
namespace std {
    /**
     * @brief Symbol クラスのためのハッシュ関数の特殊化
     */
    template<>
    struct hash<aerojs::core::Symbol> {
        /**
         * @brief シンボルのためのハッシュ関数
         * 
         * @param s ハッシュ値を計算するシンボル
         * @return シンボルのハッシュ値
         */
        constexpr size_t operator()(const aerojs::core::Symbol& s) const noexcept {
            return static_cast<size_t>(s.id());
        }
    };

    /**
     * @brief SymbolPtr のためのハッシュ関数の特殊化
     */
    template<>
    struct hash<aerojs::core::SymbolPtr> {
        /**
         * @brief シンボルポインタのためのハッシュ関数
         * 
         * @param s ハッシュ値を計算するシンボルポインタ
         * @return シンボルポインタのハッシュ値、nullの場合は0
         */
        constexpr size_t operator()(const aerojs::core::SymbolPtr& s) const noexcept {
            return s ? static_cast<size_t>(s->id()) : 0;
        }
    };
} 