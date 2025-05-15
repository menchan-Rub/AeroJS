# AeroJS コアモジュール

> **バージョン**: 2.0.0
> **最終更新日**: 2024-06-24
> **著者**: AeroJS Team

---

## 目次

1. [概要](#1-概要)
2. [主要コンポーネント](#2-主要コンポーネント)
3. [アーキテクチャ](#3-アーキテクチャ)
4. [コンテキスト管理](#4-コンテキスト管理)
5. [値とオブジェクト](#5-値とオブジェクト)
6. [例外処理](#6-例外処理)
7. [エンジン初期化](#7-エンジン初期化)
8. [使用例](#8-使用例)
9. [パフォーマンス最適化](#9-パフォーマンス最適化)
10. [スレッドセーフティ](#10-スレッドセーフティ)
11. [拡張ガイド](#11-拡張ガイド)
12. [APIリファレンス](#12-apiリファレンス)
13. [ライセンス](#13-ライセンス)

---

## 1. 概要

AeroJSコアモジュールはJavaScriptエンジンの中心的な機能を提供します。ECMAScript仕様に準拠した処理を行い、高速かつ効率的なコード実行を実現します。コンテキスト管理、値の表現、オブジェクト操作、例外処理など、JavaScriptランタイムの基本要素を提供します。

---

## 2. 主要コンポーネント

AeroJSコアモジュールは以下の主要コンポーネントで構成されています：

- **コンテキスト管理**: 実行コンテキストとグローバルオブジェクトの管理
- **値とオブジェクト**: JavaScript値の表現と操作
- **シンボル**: ECMAScript Symbol実装
- **例外処理**: エラーと例外のハンドリング
- **エンジンコントロール**: 初期化、設定、リソース管理

関連サブモジュール：

- **JIT (Just-In-Time)コンパイラ**: バイトコードのネイティブコードへの変換
- **パーサー**: JavaScript構文解析器
- **ランタイム**: 組み込みオブジェクトと標準ライブラリ
- **変換システム**: AST変換と最適化
- **VM**: 仮想マシン実装

---

## 3. アーキテクチャ

AeroJSエンジンは階層化アーキテクチャで設計されています：

```
┌─────────────────────────────────────────────────┐
│               アプリケーション                  │
└───────────────────────┬─────────────────────────┘
                        ▼
┌─────────────────────────────────────────────────┐
│                 AeroJS API                      │
└───────┬───────────────┬───────────────┬─────────┘
        ▼               ▼               ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│ コンテキスト  │ │    エンジン   │ │   グローバル  │
│    管理       │ │               │ │   オブジェクト │
└───────┬───────┘ └───────┬───────┘ └───────┬───────┘
        │                 │                 │
        ▼                 ▼                 ▼
┌─────────────────────────────────────────────────┐
│                    コア                        │
└───────┬───────────────┬───────────────┬─────────┘
        ▼               ▼               ▼
┌───────────────┐ ┌───────────────┐ ┌───────────────┐
│  パーサー     │ │     VM        │ │    JIT        │
└───────────────┘ └───────────────┘ └───────────────┘
```

---

## 4. コンテキスト管理

コンテキストは実行環境を表し、以下の機能を提供します：

- グローバルオブジェクトへのアクセス
- スコープチェーン管理
- 現在の実行状態の追跡
- 例外状態の管理

コンテキスト作成例：

```cpp
// エンジンのインスタンス化
Engine* engine = Engine::getInstance();

// コンテキストの作成
Context* context = new Context(engine);

// コンテキスト設定
ContextOptions options;
options.strictMode = true;
context->setOptions(options);
```

---

## 5. 値とオブジェクト

JavaScriptの値は`Value`クラスで表現され、以下の型をサポートします：

- プリミティブ値: Undefined, Null, Boolean, Number, String, Symbol, BigInt
- 参照値: Object, Array, Function, などの複合型

値操作の例：

```cpp
// 値の作成
Value number = Value::createNumber(42);
Value str = Value::createString("Hello");
Value obj = Value::createObject();

// プロパティ操作
obj.setProperty("name", str);
obj.setProperty("count", number);

// 値の変換
int32_t n = number.toInt32(); // 42
std::string s = str.toString(); // "Hello"
```

---

## 6. 例外処理

例外処理機構は以下の特徴を持ちます：

- JavaScriptエラーオブジェクトの生成と管理
- try/catch/finally構文のサポート
- スタックトレース情報の収集
- カスタム例外タイプのサポート

例外操作例：

```cpp
try {
  // 実行中に例外が発生する可能性のあるコード
  context->evaluateScript("throw new Error('エラーが発生しました');");
} catch (const Exception& e) {
  // 例外情報へのアクセス
  std::cout << "例外が発生: " << e.getMessage() << std::endl;
  std::cout << "スタックトレース: " << e.getStackTrace() << std::endl;
}
```

---

## 7. エンジン初期化

```cpp
// エンジンシングルトンの取得
Engine* engine = Engine::getInstance();

// エンジン初期化（オプションで設定をカスタマイズ可能）
EngineOptions options;
options.gcThreshold = 1024 * 1024; // 1MB
options.enableJIT = true;
engine->initialize(options);

// コンテキストの作成
Context* context = new Context(engine);

// JavaScript実行
Value result = context->evaluateScript("1 + 1");
int value = result.toInt32(); // 2

// クリーンアップ
delete context;
engine->shutdown();
```

---

## 8. 使用例

```cpp
// エンジン作成とコンテキスト初期化
Engine* engine = Engine::getInstance();
Context* context = new Context(engine);

// グローバルオブジェクトの取得
Value global = context->getGlobalObject();

// カスタム関数の登録
context->registerNativeFunction("log", [](const std::vector<Value>& args) -> Value {
  for (const auto& arg : args) {
    std::cout << arg.toString() << " ";
  }
  std::cout << std::endl;
  return Value::createUndefined();
}, 1);

// JavaScript実行
context->evaluateScript(R"(
  function fibonacci(n) {
    if (n <= 1) return n;
    return fibonacci(n-1) + fibonacci(n-2);
  }
  
  for (let i = 0; i < 10; i++) {
    log(`fibonacci(${i}) = ${fibonacci(i)}`);
  }
)");

// クリーンアップ
delete context;
```

---

## 9. パフォーマンス最適化

AeroJSコアは以下のパフォーマンス最適化を実装しています：

- **インラインキャッシュ**: プロパティアクセスを高速化
- **型推論**: 型情報に基づく最適化
- **JIT最適化**: ホットパスのネイティブコード生成
- **メモリ最適化**: 効率的なメモリレイアウトとキャッシュ活用
- **並列実行**: 適切な場面での並列処理活用

---

## 10. スレッドセーフティ

エンジンは複数のコンテキストを並列に実行可能ですが、各コンテキストは単一スレッドでの使用を前提としています。並列処理を行う場合は、コンテキストごとに別々のスレッドを使用してください。

```cpp
// スレッド1
Context* ctx1 = new Context(engine);
ctx1->evaluateScript("// コード1");

// スレッド2
Context* ctx2 = new Context(engine);
ctx2->evaluateScript("// コード2");
```

---

## 11. 拡張ガイド

AeroJSコアを拡張するための主要なポイント：

1. **カスタム関数の追加**: `registerNativeFunction`を使用
2. **カスタムオブジェクトの実装**: `Object`クラスを継承
3. **プロパティのカスタマイズ**: カスタムプロパティディスクリプタの設定
4. **イベントインターセプト**: 特定の操作のフック処理実装

---

## 12. APIリファレンス

完全なAPIリファレンスについては、以下を参照してください：

- `Engine`: JavaScriptエンジンの初期化と管理
- `Context`: 実行コンテキストとグローバル環境
- `Value`: JavaScript値の表現と操作
- `Object`: オブジェクトとプロパティ操作
- `Exception`: 例外処理とエラー情報

---

## 13. ライセンス

AeroJSは[MITライセンス](LICENSE)の下で公開されています。 