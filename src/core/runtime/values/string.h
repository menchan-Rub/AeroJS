/**
 * @file string.h
 * @brief JavaScript文字列クラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_STRING_H
#define AEROJS_STRING_H

#include <string>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "src/utils/memory/smart_ptr/ref_counted.h"
#include "src/utils/containers/string/string_view.h"

namespace aerojs {
namespace core {

/**
 * @brief JavaScript文字列を表現するクラス
 * 
 * このクラスはUTF-8文字列を管理し、参照カウント方式で共有される。
 * 短い文字列はスモールストリング最適化によりインライン保存される。
 */
class String : public utils::RefCounted {
public:
    // 最大スモールストリング長（バイト）
    static constexpr size_t SMALL_STRING_MAX_SIZE = 14;

    /**
     * @brief 文字列が生存する範囲を示す列挙型
     */
    enum class StorageType : uint8_t {
        Small,      // スモールストリング（インライン保存）
        Normal,     // 通常文字列（ヒープ割り当て）
        Static,     // 静的文字列（プログラム内に静的に存在）
        Sliced,     // スライス文字列（別の文字列の一部を参照）
        Concatenated // 連結文字列（2つの文字列を論理的に連結）
    };

    /**
     * @brief 空文字列を作成
     */
    String() : length_(0), utf8Length_(0), storageType_(StorageType::Small) {
        small_.data[0] = '\0';
    }

    /**
     * @brief C文字列から作成
     * @param str C文字列
     */
    explicit String(const char* str);

    /**
     * @brief std::stringから作成
     * @param str C++文字列
     */
    explicit String(const std::string& str);

    /**
     * @brief StringViewから作成
     * @param view 文字列ビュー
     */
    explicit String(const utils::StringView& view);

    /**
     * @brief デストラクタ
     */
    ~String() override;

    /**
     * @brief 文字列長(バイト数)を取得
     * @return 文字列のバイト長
     */
    size_t length() const { return length_; }

    /**
     * @brief UTF-8文字数を取得
     * @return UTF-8文字数
     */
    size_t utf8Length() const { return utf8Length_; }

    /**
     * @brief 文字列がスモールストリングであるかを確認
     * @return スモールストリングの場合true
     */
    bool isSmall() const { return storageType_ == StorageType::Small; }

    /**
     * @brief 文字列が通常文字列であるかを確認
     * @return 通常文字列の場合true
     */
    bool isNormal() const { return storageType_ == StorageType::Normal; }

    /**
     * @brief 文字列が静的文字列であるかを確認
     * @return 静的文字列の場合true
     */
    bool isStatic() const { return storageType_ == StorageType::Static; }

    /**
     * @brief 文字列がスライス文字列であるかを確認
     * @return スライス文字列の場合true
     */
    bool isSliced() const { return storageType_ == StorageType::Sliced; }

    /**
     * @brief 文字列が連結文字列であるかを確認
     * @return 連結文字列の場合true
     */
    bool isConcatenated() const { return storageType_ == StorageType::Concatenated; }

    /**
     * @brief ストレージタイプを取得
     * @return ストレージタイプ
     */
    StorageType storageType() const { return storageType_; }

    /**
     * @brief C文字列に変換
     * @return C文字列へのポインタ
     * @note 返される文字列は一時的なものであり、Stringオブジェクトが存在する間のみ有効
     */
    const char* c_str() const;

    /**
     * @brief std::stringに変換
     * @return 標準文字列
     */
    std::string value() const;

    /**
     * @brief 文字列ビューを取得
     * @return 文字列ビュー
     */
    utils::StringView view() const;

    /**
     * @brief 文字列の一部を取得（部分文字列）
     * @param start 開始位置
     * @param length 長さ
     * @return 部分文字列
     */
    String* substring(size_t start, size_t length) const;

    /**
     * @brief 文字列を連結
     * @param other 連結する文字列
     * @return 新しい連結文字列
     */
    String* concat(const String* other) const;

    /**
     * @brief 文字列内の文字を検索
     * @param ch 検索する文字
     * @param fromIndex 検索開始位置
     * @return 見つかった位置、見つからない場合はsize_t(-1)
     */
    size_t indexOf(char ch, size_t fromIndex = 0) const;

    /**
     * @brief 文字列内の部分文字列を検索
     * @param str 検索する部分文字列
     * @param fromIndex 検索開始位置
     * @return 見つかった位置、見つからない場合はsize_t(-1)
     */
    size_t indexOf(const String* str, size_t fromIndex = 0) const;

    /**
     * @brief 文字列が指定した文字で始まるかを確認
     * @param ch 検索する文字
     * @return 指定した文字で始まる場合true
     */
    bool startsWith(char ch) const;

    /**
     * @brief 文字列が指定した部分文字列で始まるかを確認
     * @param str 検索する部分文字列
     * @return 指定した部分文字列で始まる場合true
     */
    bool startsWith(const String* str) const;

    /**
     * @brief 文字列が指定した文字で終わるかを確認
     * @param ch 検索する文字
     * @return 指定した文字で終わる場合true
     */
    bool endsWith(char ch) const;

    /**
     * @brief 文字列が指定した部分文字列で終わるかを確認
     * @param str 検索する部分文字列
     * @return 指定した部分文字列で終わる場合true
     */
    bool endsWith(const String* str) const;

    /**
     * @brief 文字列が等しいかを確認
     * @param other 比較する文字列
     * @return 等しい場合true
     */
    bool equals(const String* other) const;

    /**
     * @brief 大文字小文字を区別せずに文字列が等しいかを確認
     * @param other 比較する文字列
     * @return 等しい場合true
     */
    bool equalsIgnoreCase(const String* other) const;

    /**
     * @brief 文字列を正規化する（フラット化）
     * @return 正規化された文字列
     * @note スライス文字列や連結文字列を通常文字列に変換する
     */
    String* flatten() const;

    /**
     * @brief 指定位置にある文字を取得
     * @param index 取得する位置
     * @return 文字のUTF-8バイト配列
     */
    std::string charAt(size_t index) const;

    /**
     * @brief 指定位置にあるUTF-8コードポイントを取得
     * @param index 取得する位置
     * @return UTF-8コードポイント
     */
    uint32_t charCodeAt(size_t index) const;

    /**
     * @brief 文字列を小文字に変換
     * @return 小文字に変換された文字列
     */
    String* toLowerCase() const;

    /**
     * @brief 文字列を大文字に変換
     * @return 大文字に変換された文字列
     */
    String* toUpperCase() const;

    /**
     * @brief 文字列のハッシュコードを取得
     * @return ハッシュコード
     */
    size_t hashCode() const;

    /**
     * @brief 静的文字列を作成
     * @param str 文字列定数
     * @return 静的文字列
     */
    static String* createStatic(const char* str);

    /**
     * @brief インターン文字列を取得または作成
     * @param str 文字列
     * @return インターン文字列
     * @note インターン文字列は同じ内容の文字列が一度だけ保存される
     */
    static String* intern(const char* str);
    static String* intern(const std::string& str);

    /**
     * @brief 文字列を作成
     * @param str 文字列
     * @return 新しい文字列
     */
    static String* create(const char* str);
    static String* create(const std::string& str);
    static String* create(const utils::StringView& view);

private:
    // 各ストレージタイプの内部表現
    union {
        // スモールストリング用の内部バッファ
        struct {
            char data[SMALL_STRING_MAX_SIZE + 1]; // +1 for null terminator
        } small_;

        // 通常文字列用のヒープ割り当て
        struct {
            char* data;
            size_t capacity;
        } normal_;

        // 静的文字列用の参照
        struct {
            const char* data;
        } static_;

        // スライス文字列用の参照
        struct {
            String* source;
            size_t offset;
        } sliced_;

        // 連結文字列用の参照
        struct {
            String* left;
            String* right;
        } concatenated_;
    };

    // 文字列の長さ（バイト数）
    size_t length_;

    // UTF-8文字数
    size_t utf8Length_;

    // ストレージタイプ
    StorageType storageType_;

    // 文字列インターニングのための静的マップ
    static std::unordered_map<std::string, String*> internedStrings_;
    static std::mutex internMutex_;

    // 内部使用のコンストラクタ
    String(const char* data, size_t length, StorageType type);
    String(String* source, size_t offset, size_t length);
    String(String* left, String* right);

    // UTF-8文字数を計算
    static size_t calculateUtf8Length(const char* data, size_t byteLength);
};

// インライン関数はヘッダファイルに実装

/**
 * @brief 文字列ハッシュのための特殊化
 */
template<>
struct std::hash<aerojs::core::String*> {
    size_t operator()(const aerojs::core::String* str) const {
        return str ? str->hashCode() : 0;
    }
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_STRING_H 