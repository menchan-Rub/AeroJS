# AeroJS ランタイム環境

## 概要

AeroJSランタイム環境は、JavaScriptコードの実行環境を提供します。ECMAScript仕様に準拠したグローバルオブジェクト、組み込みオブジェクト、型システム、実行コンテキストなどの機能を実装しています。

## 主要コンポーネント

### コンテキスト管理
- **ExecutionContext**: JavaScript実行コンテキストの管理
- **スコープチェーン**: 変数参照の解決
- **クロージャ**: 関数のレキシカルスコープの保持

### グローバルオブジェクト
- **GlobalObject**: JavaScript実行環境のルートオブジェクト
- **組み込みメソッド**: eval()、parseInt()など標準機能の実装
- **グローバル名前空間**: 標準ビルトインオブジェクトの登録

### 値とオブジェクト
- **JavaScriptの値**: プリミティブ型と参照型の表現
- **型変換**: JavaScript型システムに基づく型変換ロジック
- **プロパティディスクリプタ**: オブジェクトプロパティの属性管理

### シンボル
- **Symbol型**: ES6 Symbol型の実装
- **Well-known Symbols**: イテレータやtoStringTagなど特殊シンボルの提供

### イテレーション
- **Iterator Protocol**: イテレータプロトコルの実装
- **for-of対応**: オブジェクトの反復処理サポート

### リフレクション
- **内部メタデータ**: オブジェクトの内部状態へのアクセス
- **Reflect API**: リフレクション操作の実装

## サブディレクトリ

- **`builtins/`**: 標準組み込みオブジェクト（Array、String、Promise等）
- **`context/`**: 実行コンテキスト管理
- **`globals/`**: グローバル関数と値
- **`iteration/`**: イテレータとイテレーションプロトコル
- **`reflection/`**: リフレクションAPI
- **`symbols/`**: Symbol型と関連機能
- **`types/`**: JavaScript型システム
- **`values/`**: JavaScript値の表現と操作

## 実装状況

- ✅ ECMAScript 2022標準組み込みオブジェクト: 実装済み
- ✅ Promise API: すべてのメソッド実装済み
- ✅ WeakRef: ES2021仕様に基づき実装済み
- ✅ FinalizationRegistry: 実装済み
- ✅ イテレータプロトコル: 実装済み
- ✅ Proxy/Reflect API: 実装済み
- ✅ メタプログラミング機能: 実装済み

## 使用例

```cpp
// グローバルオブジェクトの取得
GlobalObject* globalObj = context->getGlobalObject();

// 組み込みオブジェクトの取得
Value mathObj = globalObj->get("Math");
Value arrayConstructor = globalObj->get("Array");

// Promiseの作成
Value executor = /* resolve, rejectを呼ぶ関数 */;
Value promise = promiseConstructor({executor}, nullptr, globalObj);

// 組み込みメソッドの呼び出し
std::vector<Value> args = {Value(10)};
Value parsed = globalObj->callGlobalFunction("parseInt", args);
``` 