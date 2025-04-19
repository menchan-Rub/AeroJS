/**
 * @file symbol.h
 * @brief AeroJS JavaScript エンジンのシンボルクラスの定義
 * @version 0.1.0
 * @license MIT
 */

#pragma once

#include "../utils/memory/smart_ptr/ref_counted.h"
#include <string>
#include <unordered_map>

namespace aerojs {
namespace core {

// 前方宣言
class Context;

/**
 * @class Symbol
 * @brief JavaScript のSymbolを表すクラス
 * 
 * 一意の識別子として使用される、ECMAScript Symbol型の実装。
 * 各シンボルは自身の説明を持ち、同じ説明であっても異なるシンボルインスタンスは等しくない。
 */
class Symbol : public utils::RefCounted {
public:
    /**
     * @brief シンボルを作成
     * @param ctx 所属するコンテキスト
     * @param description シンボルの説明（省略可能）
     */
    explicit Symbol(Context* ctx, const std::string& description = "");
    
    /**
     * @brief シンボルの説明を取得
     * @return シンボルの説明文字列
     */
    const std::string& description() const { return m_description; }
    
    /**
     * @brief シンボルを文字列表現に変換
     * @return "Symbol(description)" 形式の文字列
     */
    std::string toString() const;
    
    /**
     * @brief シンボルのハッシュ値を取得
     * @return このシンボル固有のハッシュ値
     */
    size_t hash() const { return m_id; }
    
    /**
     * @brief シンボルの等価性比較
     * @param other 比較対象のシンボル
     * @return 同一のシンボルの場合はtrue
     */
    bool equals(const Symbol* other) const;
    
    /**
     * @brief グローバルシンボルレジストリから指定された鍵のシンボルを取得・作成
     * @param ctx 所属するコンテキスト
     * @param key シンボルの鍵
     * @return 既存のシンボルまたは新規作成されたシンボル
     */
    static Symbol* forKey(Context* ctx, const std::string& key);
    
    /**
     * @brief 共有された既知のシンボルを取得
     * 
     * Well-Known Symbols（Symbol.iterator, Symbol.hasInstance など）の取得に使用
     * 
     * @param ctx 所属するコンテキスト
     * @param name 既知のシンボル名
     * @return 既知のシンボルインスタンス
     */
    static Symbol* wellKnown(Context* ctx, const std::string& name);
    
private:
    // 所属コンテキスト
    Context* m_context;
    
    // シンボルの説明
    std::string m_description;
    
    // シンボルの一意識別子
    size_t m_id;
    
    // 次のシンボルIDのための静的カウンタ
    static size_t s_nextId;
    
    // グローバルシンボルレジストリ（キー → シンボル）
    static std::unordered_map<std::string, Symbol*> s_registry;
    
    // Well-known シンボル保持用のマップ
    static std::unordered_map<std::string, Symbol*> s_wellKnownSymbols;
};

} // namespace core
} // namespace aerojs 