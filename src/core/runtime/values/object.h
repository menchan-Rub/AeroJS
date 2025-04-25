/**
 * @file object.h
 * @brief JavaScript オブジェクト型の定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_OBJECT_H
#define AEROJS_OBJECT_H

#include <string>
#include <unordered_map>
#include <vector>

#include "src/core/runtime/types/value_type.h"
#include "src/utils/containers/hashmap/hashmap.h"
#include "src/utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class String;
class Symbol;
class Object;
class Function;
class Context;

/**
 * @brief オブジェクトのプロパティを表現する構造体
 */
struct Property {
  Value* value;         // プロパティ値
  PropertyFlags flags;  // プロパティ属性フラグ
  Function* getter;     // ゲッター関数（アクセサプロパティの場合）
  Function* setter;     // セッター関数（アクセサプロパティの場合）

  // デフォルトコンストラクタ
  Property()
      : value(nullptr), flags(PropertyFlags::None), getter(nullptr), setter(nullptr) {
  }

  // 値プロパティ用コンストラクタ
  Property(Value* v, PropertyFlags f)
      : value(v), flags(f), getter(nullptr), setter(nullptr) {
  }

  // アクセサプロパティ用コンストラクタ
  Property(Function* g, Function* s, PropertyFlags f)
      : value(nullptr), flags(f | PropertyFlags::Accessor), getter(g), setter(s) {
  }

  // デストラクタ（所有権関連のクリーンアップを行う）
  ~Property();

  // コピーとムーブは詳細実装で禁止または定義する
  Property(const Property&) = delete;
  Property& operator=(const Property&) = delete;
  Property(Property&&) = delete;
  Property& operator=(Property&&) = delete;
};

/**
 * @brief JavaScript のオブジェクト型を表現するクラス
 *
 * このクラスはJavaScriptのオブジェクトの基本機能を提供し、
 * 特殊なオブジェクトタイプ（配列、関数など）の基底クラスとなる。
 */
class Object : public utils::RefCounted {
 public:
  /**
   * @brief デフォルトコンストラクタ
   *
   * 空のオブジェクトを作成します。
   */
  Object();

  /**
   * @brief プロトタイプ指定コンストラクタ
   * @param prototype このオブジェクトのプロトタイプ
   */
  explicit Object(Object* prototype);

  /**
   * @brief 実行コンテキスト指定コンストラクタ
   * @param context このオブジェクトが作成される実行コンテキスト
   */
  explicit Object(Context* context);

  /**
   * @brief 実行コンテキストとプロトタイプ指定コンストラクタ
   * @param context このオブジェクトが作成される実行コンテキスト
   * @param prototype このオブジェクトのプロトタイプ
   */
  Object(Context* context, Object* prototype);

  /**
   * @brief デストラクタ
   */
  ~Object() override;

  /**
   * @brief 文字列をキーとしたプロパティの取得
   * @param key プロパティ名
   * @return プロパティ値、存在しない場合はundefined
   */
  Value get(const String* key) const;

  /**
   * @brief シンボルをキーとしたプロパティの取得
   * @param key シンボルキー
   * @return プロパティ値、存在しない場合はundefined
   */
  Value get(const Symbol* key) const;

  /**
   * @brief 文字列をキーとしたプロパティの設定
   * @param key プロパティ名
   * @param value 設定する値
   * @return 設定に成功した場合はtrue
   */
  bool set(const String* key, const Value& value);

  /**
   * @brief シンボルをキーとしたプロパティの設定
   * @param key シンボルキー
   * @param value 設定する値
   * @return 設定に成功した場合はtrue
   */
  bool set(const Symbol* key, const Value& value);

  /**
   * @brief プロパティの存在確認
   * @param key プロパティ名
   * @return プロパティが存在する場合はtrue
   */
  bool has(const String* key) const;

  /**
   * @brief プロパティの存在確認
   * @param key シンボルキー
   * @return プロパティが存在する場合はtrue
   */
  bool has(const Symbol* key) const;

  /**
   * @brief 自身のプロパティの存在確認（継承しない）
   * @param key プロパティ名
   * @return プロパティが存在する場合はtrue
   */
  bool hasOwn(const String* key) const;

  /**
   * @brief 自身のプロパティの存在確認（継承しない）
   * @param key シンボルキー
   * @return プロパティが存在する場合はtrue
   */
  bool hasOwn(const Symbol* key) const;

  /**
   * @brief プロパティの定義
   * @param key プロパティ名
   * @param value プロパティ値
   * @param flags プロパティの属性フラグ
   * @return 定義に成功した場合はtrue
   */
  bool defineProperty(const String* key, const Value& value, PropertyFlags flags);

  /**
   * @brief プロパティの定義
   * @param key シンボルキー
   * @param value プロパティ値
   * @param flags プロパティの属性フラグ
   * @return 定義に成功した場合はtrue
   */
  bool defineProperty(const Symbol* key, const Value& value, PropertyFlags flags);

  /**
   * @brief アクセサプロパティの定義
   * @param key プロパティ名
   * @param getter ゲッター関数
   * @param setter セッター関数
   * @param flags プロパティの属性フラグ
   * @return 定義に成功した場合はtrue
   */
  bool defineAccessor(const String* key, Function* getter, Function* setter, PropertyFlags flags);

  /**
   * @brief アクセサプロパティの定義
   * @param key シンボルキー
   * @param getter ゲッター関数
   * @param setter セッター関数
   * @param flags プロパティの属性フラグ
   * @return 定義に成功した場合はtrue
   */
  bool defineAccessor(const Symbol* key, Function* getter, Function* setter, PropertyFlags flags);

  /**
   * @brief プロパティの削除
   * @param key プロパティ名
   * @return 削除に成功した場合はtrue
   */
  bool deleteProperty(const String* key);

  /**
   * @brief プロパティの削除
   * @param key シンボルキー
   * @return 削除に成功した場合はtrue
   */
  bool deleteProperty(const Symbol* key);

  /**
   * @brief プロパティ記述子の取得
   * @param key プロパティ名
   * @return プロパティ記述子オブジェクト
   */
  Object* getOwnPropertyDescriptor(const String* key) const;

  /**
   * @brief プロパティ記述子の取得
   * @param key シンボルキー
   * @return プロパティ記述子オブジェクト
   */
  Object* getOwnPropertyDescriptor(const Symbol* key) const;

  /**
   * @brief オブジェクトの所有プロパティ名の列挙
   * @return プロパティ名の配列
   */
  std::vector<String*> getOwnPropertyNames() const;

  /**
   * @brief オブジェクトの所有シンボルの列挙
   * @return シンボルの配列
   */
  std::vector<Symbol*> getOwnPropertySymbols() const;

  /**
   * @brief オブジェクトの所有かつ列挙可能なプロパティ名の列挙
   * @return プロパティ名の配列
   */
  std::vector<String*> getOwnEnumerablePropertyNames() const;

  /**
   * @brief オブジェクトの所有かつ列挙可能なシンボルの列挙
   * @return シンボルの配列
   */
  std::vector<Symbol*> getOwnEnumerablePropertySymbols() const;

  /**
   * @brief オブジェクトが拡張可能かどうかを確認
   * @return 拡張可能な場合はtrue
   */
  bool isExtensible() const;

  /**
   * @brief オブジェクトの拡張可能性を設定
   * @param extensible 拡張可能にする場合はtrue
   * @return 設定に成功した場合はtrue
   */
  bool setExtensible(bool extensible);

  /**
   * @brief オブジェクトを凍結（変更禁止）
   * @return 成功した場合はtrue
   */
  bool freeze();

  /**
   * @brief オブジェクトをシール（プロパティの追加・削除禁止）
   * @return 成功した場合はtrue
   */
  bool seal();

  /**
   * @brief オブジェクトが凍結されているかどうかを確認
   * @return 凍結されている場合はtrue
   */
  bool isFrozen() const;

  /**
   * @brief オブジェクトがシールされているかどうかを確認
   * @return シールされている場合はtrue
   */
  bool isSealed() const;

  /**
   * @brief オブジェクトのプロトタイプを取得
   * @return プロトタイプオブジェクト
   */
  Object* getPrototype() const;

  /**
   * @brief オブジェクトのプロトタイプを設定
   * @param prototype 新しいプロトタイプ
   * @return 設定に成功した場合はtrue
   */
  bool setPrototype(Object* prototype);

  /**
   * @brief このオブジェクトがプロトタイプチェーンに存在するか確認
   * @param obj チェック対象のオブジェクト
   * @return プロトタイプチェーンに存在する場合はtrue
   */
  bool isPrototypeOf(const Object* obj) const;

  /**
   * @brief オブジェクトの種類を確認
   * @return オブジェクトの種類を表すフラグ
   */
  ObjectFlags getFlags() const;

  /**
   * @brief オブジェクトに特定の種類のフラグが設定されているか確認
   * @param flag 確認するフラグ
   * @return フラグが設定されている場合はtrue
   */
  bool hasFlag(ObjectFlags flag) const;

  /**
   * @brief オブジェクトが配列かどうかを確認
   * @return 配列の場合はtrue
   */
  bool isArray() const;

  /**
   * @brief オブジェクトが関数かどうかを確認
   * @return 関数の場合はtrue
   */
  bool isFunction() const;

  /**
   * @brief オブジェクトが日付オブジェクトかどうかを確認
   * @return 日付オブジェクトの場合はtrue
   */
  bool isDate() const;

  /**
   * @brief オブジェクトが正規表現オブジェクトかどうかを確認
   * @return 正規表現オブジェクトの場合はtrue
   */
  bool isRegExp() const;

  /**
   * @brief オブジェクトがエラーオブジェクトかどうかを確認
   * @return エラーオブジェクトの場合はtrue
   */
  bool isError() const;

  /**
   * @brief オブジェクトがプロミスオブジェクトかどうかを確認
   * @return プロミスオブジェクトの場合はtrue
   */
  bool isPromise() const;

  /**
   * @brief オブジェクトがプロキシオブジェクトかどうかを確認
   * @return プロキシオブジェクトの場合はtrue
   */
  bool isProxy() const;

  /**
   * @brief オブジェクトがMapオブジェクトかどうかを確認
   * @return Mapオブジェクトの場合はtrue
   */
  bool isMap() const;

  /**
   * @brief オブジェクトがSetオブジェクトかどうかを確認
   * @return Setオブジェクトの場合はtrue
   */
  bool isSet() const;

  /**
   * @brief オブジェクトがWeakMapオブジェクトかどうかを確認
   * @return WeakMapオブジェクトの場合はtrue
   */
  bool isWeakMap() const;

  /**
   * @brief オブジェクトがWeakSetオブジェクトかどうかを確認
   * @return WeakSetオブジェクトの場合はtrue
   */
  bool isWeakSet() const;

  /**
   * @brief オブジェクトのクラス名を取得
   * @return クラス名
   */
  std::string getClassName() const;

  /**
   * @brief オブジェクトを文字列に変換
   * @return 文字列表現
   */
  virtual std::string toString() const;

  /**
   * @brief オブジェクトをプリミティブ値に変換
   * @param hint 変換のヒント
   * @return プリミティブ値
   */
  virtual Value toPrimitive(const std::string& hint) const;

 protected:
  // オブジェクトのフラグ
  ObjectFlags flags_;

  // オブジェクトのプロトタイプ
  Object* prototype_;

  // 実行コンテキスト
  Context* context_;

  // 文字列プロパティマップ
  using StringPropertyMap = utils::HashMap<String*, Property*>;
  StringPropertyMap stringProps_;

  // シンボルプロパティマップ
  using SymbolPropertyMap = utils::HashMap<Symbol*, Property*>;
  SymbolPropertyMap symbolProps_;

  // インデックス付きプロパティ（配列など用）
  std::unordered_map<uint32_t, Property*> indexedProps_;

  // プロパティ検索のヘルパーメソッド
  Property* findProperty(const String* key) const;
  Property* findProperty(const Symbol* key) const;
  Property* findOwnProperty(const String* key) const;
  Property* findOwnProperty(const Symbol* key) const;

  // フラグの管理
  void setFlag(ObjectFlags flag, bool value = true);

  // 内部的なプロパティ操作
  bool setPropertyInternal(Property* prop, const Value& value);
  Value getPropertyInternal(Property* prop) const;

  // コピーおよびムーブの無効化
  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;
  Object(Object&&) = delete;
  Object& operator=(Object&&) = delete;
};

/**
 * @brief オブジェクトファクトリ関数
 * @param context 実行コンテキスト
 * @param prototype オブジェクトのプロトタイプ（nullptrの場合はデフォルト）
 * @return 新しいオブジェクト
 */
Object* createObject(Context* context, Object* prototype = nullptr);

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_OBJECT_H