/**
 * @file context.h
 * @brief AeroJS JavaScript エンジンの実行コンテキストクラス定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_CONTEXT_H
#define AEROJS_CORE_CONTEXT_H

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../utils/containers/string/string_view.h"
#include "../utils/memory/allocators/memory_allocator.h"

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class Object;
class Function;
class Exception;
class Scope;
class SymbolTable;
class Parser;
class Lexer;
class BytecodeGenerator;
class Interpreter;
class JITCompiler;

/**
 * @brief コンテキスト作成時のオプション
 */
struct ContextOptions {
  size_t maxStackSize = 1024 * 1024;  // スタックサイズ上限 (1MB)
  bool enableExceptions = true;       // 例外処理を有効にする
  bool strictMode = false;            // 厳格モード
  bool enableDebugger = false;        // デバッガーサポート
  std::string timezone = "UTC";       // タイムゾーン設定
  std::string locale = "en-US";       // ロケール設定
};

/**
 * @brief ネイティブ関数のコールバック型
 */
using NativeFunction = std::function<Value*(Context*, int, Value**)>;

/**
 * @brief カスタムデータのクリーナー型
 */
using ContextDataCleaner = std::function<void(void*)>;

/**
 * @brief 実行コンテキストクラス
 *
 * JavaScript実行環境を表し、グローバルオブジェクト、変数スコープ、
 * イベントループなどを管理する。
 */
class Context {
 public:
  /**
   * @brief コンストラクタ
   * @param options コンテキスト作成オプション
   */
  explicit Context(const ContextOptions& options);

  /**
   * @brief デストラクタ
   */
  ~Context();

  /**
   * @brief 設定オプションの取得
   * @return 現在の設定オプション
   */
  const ContextOptions& getOptions() const {
    return m_options;
  }

  /**
   * @brief グローバルオブジェクトの取得
   * @return グローバルオブジェクトへのポインタ
   */
  Object* getGlobalObject() const {
    return m_globalObject;
  }

  /**
   * @brief 現在の例外の取得
   * @return 例外オブジェクトへのポインタ、例外がない場合はnull
   */
  Exception* getLastException() const {
    return m_lastException;
  }

  /**
   * @brief 例外クリア
   */
  void clearLastException();

  /**
   * @brief 例外の設定
   * @param exception 設定する例外オブジェクト
   */
  void setLastException(Exception* exception);

  /**
   * @brief エラー例外の設定
   * @param type エラー種別
   * @param message エラーメッセージ
   * @return 生成されたエラー例外オブジェクト
   */
  Exception* setErrorException(const std::string& type, const std::string& message);

  /**
   * @brief 関数の作成
   * @param name 関数名
   * @param func ネイティブ関数
   * @param argCount 引数の数
   * @return 作成された関数オブジェクト
   */
  Function* createFunction(const std::string& name, NativeFunction func, int argCount);

  /**
   * @brief グローバル関数の登録
   * @param name 関数名
   * @param func ネイティブ関数
   * @param argCount 引数の数
   * @return 成功した場合はtrue
   */
  bool registerGlobalFunction(const std::string& name, NativeFunction func, int argCount);

  /**
   * @brief グローバル値の登録
   * @param name 値の名前
   * @param value 設定する値
   * @return 成功した場合はtrue
   */
  bool registerGlobalValue(const std::string& name, Value* value);

  /**
   * @brief スクリプトの評価
   * @param script JavaScript コード
   * @param fileName ファイル名（デバッグ用）
   * @param lineNumber 行番号（デバッグ用）
   * @return 評価結果の値
   */
  Value* evaluateScript(const std::string& script,
                        const std::string& fileName = "<eval>",
                        int lineNumber = 1);

  /**
   * @brief スクリプトファイルの評価
   * @param fileName ファイルパス
   * @return 評価結果の値
   */
  Value* evaluateScriptFile(const std::string& fileName);

  /**
   * @brief 関数の呼び出し
   * @param func 呼び出す関数オブジェクト
   * @param thisObj thisの値（nullの場合はグローバルオブジェクト）
   * @param args 引数配列
   * @param argCount 引数の数
   * @return 関数の戻り値
   */
  Value* callFunction(Function* func, Object* thisObj, Value** args, int argCount);

  /**
   * @brief オブジェクトのメソッド呼び出し
   * @param obj 対象オブジェクト
   * @param methodName メソッド名
   * @param args 引数配列
   * @param argCount 引数の数
   * @return メソッドの戻り値
   */
  Value* callMethod(Object* obj, const std::string& methodName, Value** args, int argCount);

  /**
   * @brief ガベージコレクションの実行
   * @param force 強制GCフラグ
   */
  void collectGarbage(bool force = false);

  /**
   * @brief カスタムデータの設定
   * @param key データキー
   * @param data データポインタ
   * @param cleaner データ解放関数（オプション）
   * @return 成功した場合はtrue
   */
  bool setContextData(const std::string& key, void* data, ContextDataCleaner cleaner = nullptr);

  /**
   * @brief カスタムデータの取得
   * @param key データキー
   * @return データポインタ、存在しない場合はnull
   */
  void* getContextData(const std::string& key) const;

  /**
   * @brief カスタムデータの削除
   * @param key データキー
   * @return 成功した場合はtrue
   */
  bool removeContextData(const std::string& key);

  /**
   * @brief コンテキストのリセット
   * グローバルオブジェクトとビルトインオブジェクトを保持したまま
   * コンテキストをリセットする
   */
  void reset();

  /**
   * @brief 現在の実行スコープの取得
   * @return 現在のスコープ
   */
  Scope* getCurrentScope() const {
    return m_currentScope;
  }

  /**
   * @brief インタプリタの取得
   * @return インタプリタインスタンス
   */
  Interpreter* getInterpreter() const {
    return m_interpreter;
  }

  /**
   * @brief JITコンパイラの取得
   * @return JITコンパイラインスタンス
   */
  JITCompiler* getJITCompiler() const {
    return m_jitCompiler;
  }

 private:
  /**
   * @brief カスタムデータエントリ構造体
   */
  struct ContextDataEntry {
    void* data;
    ContextDataCleaner cleaner;

    ~ContextDataEntry() {
      if (data && cleaner) {
        cleaner(data);
      }
    }
  };

  // スコープ管理メソッド
  void pushScope(Scope* scope);
  void popScope();

  // ビルトインオブジェクトの初期化
  void initializeBuiltins();

  // コンテキスト設定オプション
  ContextOptions m_options;

  // グローバルオブジェクト
  Object* m_globalObject;

  // 最後の例外
  Exception* m_lastException;

  // 現在のスコープ
  Scope* m_currentScope;

  // 各コンポーネント
  SymbolTable* m_symbolTable;
  Parser* m_parser;
  Lexer* m_lexer;
  BytecodeGenerator* m_bytecodeGenerator;
  Interpreter* m_interpreter;
  JITCompiler* m_jitCompiler;

  // 親エンジン
  class Engine* m_engine;

  // カスタムデータ保存用マップ
  std::unordered_map<std::string, ContextDataEntry> m_contextData;

  // スコープチェーン
  std::vector<Scope*> m_scopeChain;

  // スレッド同期用ミューテックス
  std::mutex m_mutex;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_CONTEXT_H