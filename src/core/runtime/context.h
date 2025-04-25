/**
 * @file context.h
 * @brief JavaScript 実行コンテキストの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CONTEXT_H
#define AEROJS_CONTEXT_H

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/core/runtime/values/value.h"
#include "src/utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

// 前方宣言
class Object;
class String;
class Function;
class Value;
class Error;
class Symbol;

/**
 * @brief 実行コンテキストのオプション設定
 */
struct ContextOptions {
  // スタックの最大サイズ
  size_t maxStackSize = 1000;

  // 例外機能の有効化
  bool enableExceptions = true;

  // strictモードの有効化
  bool strictMode = false;

  // デバッガの有効化
  bool enableDebugger = false;

  // タイムゾーン設定
  std::string timezone = "UTC";

  // ロケール設定
  std::string locale = "en-US";

  // 最大実行時間 (ミリ秒, 0は無制限)
  uint64_t maxExecutionTime = 0;

  // 最大メモリ使用量 (バイト, 0は無制限)
  size_t maxMemoryUsage = 0;

  // 最大メモリアロケーション回数 (0は無制限)
  size_t maxAllocations = 0;

  // カスタムJITコンパイルオプション
  bool enableJIT = true;
  uint32_t jitThreshold = 100;
  uint32_t optimizationLevel = 2;
};

/**
 * @brief JavaScript 実行コンテキスト
 *
 * このクラスはJavaScriptコードの実行環境を提供します。
 * 各コンテキストは独自のグローバルオブジェクトと実行スタックを持ちます。
 */
class Context : public utils::RefCounted {
 public:
  /**
   * @brief デフォルトのコンテキストオプションを取得
   * @return デフォルトのコンテキストオプション
   */
  static ContextOptions getDefaultOptions();

  /**
   * @brief デフォルトオプションでコンテキストを作成
   */
  Context();

  /**
   * @brief 指定されたオプションでコンテキストを作成
   * @param options コンテキストオプション
   */
  explicit Context(const ContextOptions& options);

  /**
   * @brief デストラクタ
   */
  ~Context() override;

  /**
   * @brief このコンテキストの現在のオプションを取得
   * @return コンテキストオプション
   */
  const ContextOptions& getOptions() const;

  /**
   * @brief コンテキストのオプションを更新
   * @param options 新しいオプション
   */
  void setOptions(const ContextOptions& options);

  /**
   * @brief グローバルオブジェクトを取得
   * @return グローバルオブジェクト
   */
  Object* getGlobalObject() const;

  /**
   * @brief このコンテキストが厳格モードかどうかを確認
   * @return 厳格モードの場合はtrue
   */
  bool isStrictMode() const;

  /**
   * @brief このコンテキストの厳格モードを設定
   * @param strict 厳格モードにする場合はtrue
   */
  void setStrictMode(bool strict);

  /**
   * @brief 最後に発生した例外を取得
   * @return 例外オブジェクト、なければnull
   */
  Value getLastException() const;

  /**
   * @brief 最後に発生した例外をクリア
   */
  void clearLastException();

  /**
   * @brief 例外を設定
   * @param exception 例外オブジェクト
   */
  void setLastException(const Value& exception);

  /**
   * @brief エラーオブジェクトを例外として設定
   * @param errorType エラータイプ
   * @param message エラーメッセージ
   */
  void setErrorException(ErrorType errorType, const std::string& message);

  /**
   * @brief JavaScriptコードを評価
   * @param script JavaScriptコード
   * @param sourceUrl ソースのURL（オプション）
   * @param startLine 開始行番号（オプション）
   * @return 評価結果
   */
  Value evaluateScript(const std::string& script,
                       const std::string& sourceUrl = "",
                       int startLine = 1);

  /**
   * @brief JavaScriptファイルを評価
   * @param filename ファイル名
   * @return 評価結果
   */
  Value evaluateScriptFile(const std::string& filename);

  /**
   * @brief 関数を呼び出す
   * @param func 呼び出す関数
   * @param thisValue 関数内でのthis値
   * @param args 関数に渡す引数
   * @return 関数の戻り値
   */
  Value callFunction(Function* func, const Value& thisValue,
                     const std::vector<Value>& args);

  /**
   * @brief オブジェクトのメソッドを呼び出す
   * @param obj メソッドを持つオブジェクト
   * @param methodName メソッド名
   * @param args メソッドに渡す引数
   * @return メソッドの戻り値
   */
  Value callMethod(Object* obj, const std::string& methodName,
                   const std::vector<Value>& args);

  /**
   * @brief グローバルオブジェクトに関数を登録
   * @param name 関数名
   * @param func ネイティブ関数
   * @param argCount 期待される引数の数（-1は可変長）
   * @return 登録された関数オブジェクト
   */
  Function* registerGlobalFunction(const std::string& name,
                                   NativeFunction func,
                                   int argCount = -1);

  /**
   * @brief グローバルオブジェクトに値を登録
   * @param name プロパティ名
   * @param value 設定する値
   * @param flags プロパティの属性フラグ
   */
  void registerGlobalValue(const std::string& name,
                           const Value& value,
                           PropertyFlags flags = PropertyFlags::Default);

  /**
   * @brief コンテキストをリセット
   *
   * グローバルオブジェクトの状態をクリアしますが、
   * 組み込みオブジェクトやプロトタイプは維持されます。
   */
  void reset();

  /**
   * @brief ガベージコレクションを実行
   * @param force 強制的にフルGCを実行する場合はtrue
   */
  void collectGarbage(bool force = false);

  /**
   * @brief 文字列オブジェクトを作成
   * @param str 文字列
   * @return 文字列オブジェクト
   */
  String* createString(const std::string& str);

  /**
   * @brief 文字列プールから文字列を取得
   * @param str 文字列
   * @return インターンされた文字列オブジェクト
   */
  String* internString(const std::string& str);

  /**
   * @brief シンボルを作成
   * @param description シンボルの説明（オプション）
   * @return シンボルオブジェクト
   */
  Symbol* createSymbol(const std::string& description = "");

  /**
   * @brief グローバルシンボルを取得
   * @param key グローバルシンボルのキー
   * @return グローバルシンボル
   */
  Symbol* getSymbolFor(const std::string& key);

  /**
   * @brief オブジェクトを作成
   * @param prototype プロトタイプオブジェクト（nullptrの場合はデフォルト）
   * @return 新しいオブジェクト
   */
  Object* createObject(Object* prototype = nullptr);

  /**
   * @brief 配列オブジェクトを作成
   * @param length 配列の初期長さ
   * @return 新しい配列オブジェクト
   */
  Object* createArray(size_t length = 0);

  /**
   * @brief エラーオブジェクトを作成
   * @param errorType エラータイプ
   * @param message エラーメッセージ
   * @return 新しいエラーオブジェクト
   */
  Object* createError(ErrorType errorType, const std::string& message);

  /**
   * @brief 正規表現オブジェクトを作成
   * @param pattern 正規表現パターン
   * @param flags 正規表現フラグ
   * @return 新しい正規表現オブジェクト
   */
  Object* createRegExp(const std::string& pattern, const std::string& flags = "");

  /**
   * @brief 日付オブジェクトを作成
   * @param time Unix時間（ミリ秒）
   * @return 新しい日付オブジェクト
   */
  Object* createDate(double time);

  /**
   * @brief カスタムデータをコンテキストに関連付ける
   * @param key データのキー
   * @param data データへのポインタ
   */
  void setContextData(const std::string& key, void* data);

  /**
   * @brief コンテキストに関連付けられたカスタムデータを取得
   * @param key データのキー
   * @return データへのポインタ、存在しない場合はnullptr
   */
  void* getContextData(const std::string& key) const;

  /**
   * @brief コンテキストからカスタムデータを削除
   * @param key データのキー
   * @return 削除に成功した場合はtrue
   */
  bool removeContextData(const std::string& key);

 private:
  // コンテキストのオプション
  ContextOptions options_;

  // グローバルオブジェクト
  Object* globalObject_;

  // 最後に発生した例外
  Value lastException_;

  // 現在の実行スタック
  std::vector<void*> callStack_;

  // カスタムデータマップ
  std::unordered_map<std::string, void*> contextData_;

  // カスタムデータマップへのアクセスを保護するミューテックス
  mutable std::mutex contextDataMutex_;

  // 初期化処理
  void initialize();

  // 組み込みオブジェクトの初期化
  void initializeBuiltins();

  // コピーおよびムーブの無効化
  Context(const Context&) = delete;
  Context& operator=(const Context&) = delete;
  Context(Context&&) = delete;
  Context& operator=(Context&&) = delete;
};

/**
 * @brief デフォルトの実行コンテキストを作成
 * @return 新しいコンテキスト
 */
Context* createContext();

/**
 * @brief 指定されたオプションで実行コンテキストを作成
 * @param options コンテキストオプション
 * @return 新しいコンテキスト
 */
Context* createContext(const ContextOptions& options);

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CONTEXT_H