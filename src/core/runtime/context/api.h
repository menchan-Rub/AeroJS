/**
 * @file api.h
 * @brief @context APIのヘッダファイル
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_RUNTIME_CONTEXT_API_H
#define AERO_RUNTIME_CONTEXT_API_H

#include <string>
#include <vector>

namespace aero {

class ExecutionContext;
class Object;
class Value;

/**
 * @brief コンテキストオプション
 *
 * JavaScript実行環境の設定オプションを格納する構造体
 */
struct ContextOptions {
  /** 厳格モードを有効にするかどうか */
  bool strictMode = false;

  /** コンソールAPIを有効にするかどうか */
  bool hasConsole = true;

  /** ES Modulesを有効にするかどうか */
  bool hasModules = true;

  /** SharedArrayBufferを有効にするかどうか */
  bool hasSharedArrayBuffer = false;

  /** ロケール設定（空文字列の場合はシステムデフォルト） */
  std::string locale = "";
};

/**
 * @brief @context API
 *
 * JavaScript実行環境（コンテキスト）を作成・管理するためのAPI
 */
class ContextAPI {
 public:
  /**
   * @brief 新しいコンテキストを作成
   *
   * @param ctx 現在の実行コンテキスト
   * @param options コンテキスト作成オプション
   * @return 新しいコンテキストオブジェクト
   */
  static Object* create(ExecutionContext* ctx, const ContextOptions& options = ContextOptions());

  /**
   * @brief コンテキストでJavaScriptコードを評価
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト [code, options]
   * @return 評価結果
   */
  static Value evaluate(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストのグローバル変数を設定
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト [name, value]
   * @return undefined
   */
  static Value setGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストのグローバル変数を取得
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト [name]
   * @return グローバル変数の値
   */
  static Value getGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストのグローバル変数を削除
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト [name]
   * @return 削除が成功したかどうか
   */
  static Value deleteGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストでモジュールをインポート
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト [specifier, options]
   * @return モジュールのエクスポート
   */
  static Value importModule(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストのグローバルオブジェクトを取得
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト (未使用)
   * @return グローバルオブジェクト
   */
  static Value getGlobalObject(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストのオプションを取得
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト (未使用)
   * @return オプションオブジェクト
   */
  static Value getOptions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストを破棄
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト (未使用)
   * @return undefined
   */
  static Value destroy(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);
};

/**
 * @brief @context APIをグローバルオブジェクトに登録
 *
 * @param ctx 実行コンテキスト
 * @param globalObj グローバルオブジェクト
 */
void registerContextAPI(ExecutionContext* ctx, Object* globalObj);

}  // namespace aero

#endif  // AERO_RUNTIME_CONTEXT_API_H