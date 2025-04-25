/**
 * @file symbols.h
 * @brief JavaScript のシンボル型の定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_CORE_RUNTIME_SYMBOLS_H
#define AERO_CORE_RUNTIME_SYMBOLS_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "../object.h"
#include "../values/value.h"

namespace aero {

/**
 * @brief JavaScriptのSymbol型の実装
 *
 * Symbol型はECMAScriptの原始型の一つで、一意の識別子として機能します。
 * このクラスはSymbolの生成、グローバルなSymbolの管理、および一般的なWell-Known Symbolsを提供します。
 */
class Symbol {
 public:
  /**
   * @brief 説明付きのシンボルを作成
   *
   * @param description シンボルの説明（オプション）
   * @return 新しいシンボルインスタンス
   */
  static Symbol* create(const std::string& description = "");

  /**
   * @brief 指定されたキーに対応するグローバルシンボルを取得または作成
   *
   * 指定されたキーに対応するグローバルシンボルがすでに存在する場合はそれを返し、
   * 存在しない場合は新しいグローバルシンボルを作成して返します。
   *
   * @param key シンボルのキー
   * @return グローバルシンボルインスタンス
   */
  static Symbol* for_(const std::string& key);

  /**
   * @brief 指定されたシンボルのキーを取得
   *
   * グローバルシンボルレジストリに登録されているシンボルのキーを返します。
   * グローバルシンボルでない場合はundefinedを返します。
   *
   * @param symbol キーを取得するシンボル
   * @return キー文字列、またはグローバルシンボルでない場合はundefined
   */
  static Value keyFor(Symbol* symbol);

  /**
   * @brief シンボルの説明を取得
   *
   * @return シンボルの説明文字列
   */
  const std::string& description() const {
    return m_description;
  }

  /**
   * @brief シンボルのID値を取得
   *
   * @return シンボルの一意のID
   */
  uint64_t id() const {
    return m_id;
  }

  /**
   * @brief 2つのシンボルが等しいかどうかを比較
   *
   * @param other 比較対象のシンボル
   * @return 等しい場合はtrue、そうでなければfalse
   */
  bool equals(const Symbol* other) const;

  /**
   * @brief シンボルの文字列表現を取得
   *
   * ECMAScript仕様に従い、"Symbol(description)"の形式で返します。
   *
   * @return シンボルの文字列表現
   */
  std::string toString() const;

 public:
  // Well-Known Symbols
  // https://tc39.es/ecma262/#sec-well-known-symbols

  /**
   * @brief Symbol.hasInstance
   *
   * instanceof演算子の振る舞いをカスタマイズするためのシンボル
   */
  static Symbol* hasInstance();

  /**
   * @brief Symbol.isConcatSpreadable
   *
   * Array.prototype.concatメソッドでオブジェクトが配列要素として連結されるかどうかを指定するシンボル
   */
  static Symbol* isConcatSpreadable();

  /**
   * @brief Symbol.iterator
   *
   * オブジェクトのデフォルトイテレータを指定するシンボル
   */
  static Symbol* iterator();

  /**
   * @brief Symbol.match
   *
   * 文字列に対して正規表現マッチングを行うメソッドを指定するシンボル
   */
  static Symbol* match();

  /**
   * @brief Symbol.matchAll
   *
   * 文字列に対して一致するすべての正規表現マッチを返すメソッドを指定するシンボル
   */
  static Symbol* matchAll();

  /**
   * @brief Symbol.replace
   *
   * 部分文字列の置換処理を行うメソッドを指定するシンボル
   */
  static Symbol* replace();

  /**
   * @brief Symbol.search
   *
   * 文字列内の部分文字列を検索するメソッドを指定するシンボル
   */
  static Symbol* search();

  /**
   * @brief Symbol.species
   *
   * 派生オブジェクトを作成するためのコンストラクタ関数を指定するシンボル
   */
  static Symbol* species();

  /**
   * @brief Symbol.split
   *
   * 文字列を指定した区切り文字で分割するメソッドを指定するシンボル
   */
  static Symbol* split();

  /**
   * @brief Symbol.toPrimitive
   *
   * オブジェクトを原始値に変換するメソッドを指定するシンボル
   */
  static Symbol* toPrimitive();

  /**
   * @brief Symbol.toStringTag
   *
   * Object.prototype.toStringメソッドで使用されるデフォルトの文字列説明を指定するシンボル
   */
  static Symbol* toStringTag();

  /**
   * @brief Symbol.unscopables
   *
   * withステートメントのバインディングから除外されるオブジェクトのプロパティ名を指定するシンボル
   */
  static Symbol* unscopables();

  /**
   * @brief Symbol.asyncIterator
   *
   * オブジェクトのデフォルト非同期イテレータを指定するシンボル
   */
  static Symbol* asyncIterator();

 private:
  /**
   * @brief シンボルコンストラクタ
   *
   * @param description シンボルの説明
   * @param id シンボルのID（指定しない場合は自動生成）
   * @param isGlobal グローバルシンボルかどうか
   * @param key グローバルシンボルの場合のキー
   */
  Symbol(const std::string& description, uint64_t id = 0, bool isGlobal = false, const std::string& key = "");

  /**
   * @brief グローバルシンボルレジストリからシンボルを取得
   *
   * @param key シンボルのキー
   * @return キーに対応するシンボル、存在しない場合はnullptr
   */
  static Symbol* getFromRegistry(const std::string& key);

  /**
   * @brief グローバルシンボルレジストリにシンボルを登録
   *
   * @param key シンボルのキー
   * @param symbol 登録するシンボル
   */
  static void registerToRegistry(const std::string& key, Symbol* symbol);

 private:
  std::string m_description;  ///< シンボルの説明
  uint64_t m_id;              ///< シンボルの一意のID
  bool m_isGlobal;            ///< グローバルシンボルかどうか
  std::string m_key;          ///< グローバルシンボルの場合のキー

  // グローバルシンボルレジストリ
  static std::unordered_map<std::string, Symbol*> s_registry;
  static std::mutex s_registryMutex;

  // 一意のIDを生成するためのカウンタ
  static std::atomic<uint64_t> s_nextId;

  // Well-Known Symbols
  static Symbol* s_hasInstance;
  static Symbol* s_isConcatSpreadable;
  static Symbol* s_iterator;
  static Symbol* s_match;
  static Symbol* s_matchAll;
  static Symbol* s_replace;
  static Symbol* s_search;
  static Symbol* s_species;
  static Symbol* s_split;
  static Symbol* s_toPrimitive;
  static Symbol* s_toStringTag;
  static Symbol* s_unscopables;
  static Symbol* s_asyncIterator;
};

/**
 * @brief Symbolプロトタイプを初期化
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 */
void initializeSymbolPrototype(ExecutionContext* ctx, Object* globalObj);

/**
 * @brief Symbolコンストラクタを初期化
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 * @param prototype Symbolプロトタイプ
 * @return Symbolコンストラクタ関数
 */
Function* initializeSymbolConstructor(ExecutionContext* ctx, Object* globalObj, Object* prototype);

}  // namespace aero

#endif  // AERO_CORE_RUNTIME_SYMBOLS_H