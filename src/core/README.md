# AeroJSコアエンジン

## 概要

AeroJSコアエンジンはJavaScriptエンジンの心臓部であり、JavaScript実行の基本機能を提供します。ECMAScript仕様に準拠した処理を行い、高速かつ効率的なコード実行を実現します。

## 主要コンポーネント

- **コンテキスト管理**: 実行コンテキストとグローバルオブジェクトの管理
- **値とオブジェクト**: JavaScript値の表現と操作
- **シンボル**: ECMAScript Symbol実装
- **例外処理**: エラーと例外のハンドリング

## サブディレクトリ

- **`jit/`**: Just-In-Timeコンパイラシステム
- **`parser/`**: JavaScript構文解析器
- **`runtime/`**: ランタイム環境と組み込みオブジェクト
- **`transformers/`**: コード変換とAST最適化
- **`utils/`**: ユーティリティ関数
- **`vm/`**: 仮想マシン実装

## 使用方法

エンジンは通常、以下のようにインスタンス化して使用します：

```cpp
// エンジンシングルトンの取得
Engine* engine = Engine::getInstance();

// コンテキストの作成
Context* context = new Context(engine);

// コードの実行
Value result = context->evaluateScript("1 + 1");

// 結果の使用
int value = result.toInt32();
```

## スレッドセーフティ

エンジンは複数のコンテキストを並列に実行可能ですが、各コンテキストは単一スレッドでの使用を前提としています。並列処理を行う場合は、コンテキストごとに別々のスレッドを使用してください。 