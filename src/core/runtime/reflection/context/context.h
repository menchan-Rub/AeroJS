/**
 * @file context.h
 * @brief JavaScript の Context API の定義
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#ifndef AERO_CONTEXT_H
#define AERO_CONTEXT_H

#include <string>
#include <vector>

namespace aero {

class Value;
class Object;
class ExecutionContext;
class String;

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
 * @brief Context API
 *
 * JavaScriptの実行環境（コンテキスト）を作成・管理するためのAPI
 * ECMAScriptの仕様に準拠した安全な実行環境を提供します
 */
class ContextAPI {
 public:
  /**
   * @brief 新しいコンテキストを作成
   *
   * 指定されたオプションに基づいて新しいJavaScript実行環境を作成します
   *
   * @param ctx 現在の実行コンテキスト
   * @param options コンテキスト作成オプション
   * @return 新しいコンテキストを表すオブジェクト
   */
  static Object* create(ExecutionContext* ctx, const ContextOptions& options);

  /**
   * @brief コードを評価する
   *
   * 指定されたコンテキスト内でJavaScriptコードを評価し、結果を返します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（コード文字列とオプション）
   * @return 評価結果
   */
  static Value evaluate(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief グローバル変数を設定
   *
   * 指定されたコンテキスト内のグローバルオブジェクトにプロパティを設定します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（プロパティ名と値）
   * @return undefined
   */
  static Value setGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief グローバル変数を取得
   *
   * 指定されたコンテキスト内のグローバルオブジェクトからプロパティを取得します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（プロパティ名）
   * @return プロパティの値
   */
  static Value getGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief グローバル変数を削除
   *
   * 指定されたコンテキスト内のグローバルオブジェクトからプロパティを削除します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（プロパティ名）
   * @return 削除に成功したかどうか
   */
  static Value deleteGlobal(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief モジュールを読み込む
   *
   * 指定されたコンテキスト内でモジュールを読み込みます
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（モジュール指定子）
   * @return モジュールのエクスポート
   */
  static Value importModule(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief グローバルオブジェクトを取得
   *
   * 指定されたコンテキスト内のグローバルオブジェクトを取得します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（なし）
   * @return グローバルオブジェクト
   */
  static Value getGlobalObject(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストオプションを取得
   *
   * 指定されたコンテキストの設定オプションを取得します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（なし）
   * @return オプションオブジェクト
   */
  static Value getOptions(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);

  /**
   * @brief コンテキストを破棄
   *
   * 指定されたコンテキストを破棄し、リソースを解放します
   *
   * @param ctx 現在の実行コンテキスト
   * @param thisValue コンテキストオブジェクト
   * @param args 引数リスト（なし）
   * @return undefined
   */
  static Value destroy(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args);
};

/**
 * @brief Context APIをグローバルオブジェクトに登録
 *
 * @param ctx 現在の実行コンテキスト
 * @param global グローバルオブジェクト
 */
void registerContextAPI(ExecutionContext* ctx, Object* global);

}  // namespace aero

#endif  // AERO_CONTEXT_H