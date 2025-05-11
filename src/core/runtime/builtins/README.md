# JavaScript組み込みオブジェクト

## 概要

AeroJSエンジンの標準組み込みオブジェクト実装です。ECMAScript仕様に準拠した組み込みオブジェクト、メソッド、プロパティを提供し、JavaScript言語の基本機能を実現します。

## 実装されているオブジェクト

### 基本オブジェクト
- **Object**: オブジェクト操作と継承
- **Function**: 関数オブジェクトと呼び出し
- **Boolean**: 論理値
- **Symbol**: シンボル型
- **Error**: エラー処理（標準エラー型とカスタムエラー）

### データ構造と型
- **Array**: 配列とその操作メソッド
- **TypedArray**: 型付き配列（Int8Array, Float64Arrayなど）
- **Map/Set**: キー値コレクションとユニーク値コレクション
- **WeakMap/WeakSet**: 弱参照コレクション

### 数値と日時
- **Number**: 数値操作
- **Math**: 数学関数
- **Date**: 日時処理

### テキスト処理
- **String**: 文字列操作
- **RegExp**: 正規表現

### 特殊機能
- **JSON**: JSON解析と文字列化
- **Promise**: 非同期処理
- **WeakRef**: 弱参照
- **FinalizationRegistry**: オブジェクト解放後の処理
- **Proxy/Reflect**: メタプログラミング

## ディレクトリ構造

各組み込みオブジェクトは専用のディレクトリに実装されています：

```
builtins/
  ├── array/        # Array実装
  ├── bigint/       # BigInt実装
  ├── boolean/      # Boolean実装
  ├── date/         # Date実装
  ├── error/        # Error階層実装
  ├── finalization/ # FinalizationRegistry実装
  ├── function/     # Function実装
  ├── json/         # JSON実装
  ├── map/          # Map実装
  ├── math/         # Math実装
  ├── number/       # Number実装
  ├── promise/      # Promise実装
  ├── regexp/       # RegExp実装
  ├── set/          # Set実装
  ├── string/       # String実装
  ├── weakmap/      # WeakMap実装
  └── weakref/      # WeakRef実装
```

## 実装パターン

各組み込みオブジェクトは以下の共通パターンで実装されています：

1. **クラス定義**: 各オブジェクト型の内部表現
2. **プロトタイプ設定**: プロトタイプチェーンの構築
3. **コンストラクタ関数**: new演算子での生成をサポート
4. **静的プロパティ/メソッド**: クラスに直接付属する機能
5. **インスタンスメソッド**: プロトタイプを通して継承される機能

## モジュール化

各組み込みオブジェクトは次のファイルに分かれています：

- **`*.h`**: クラス定義とインターフェース
- **`*.cpp`**: メソッド実装
- **`*_static.cpp`**: 静的メソッドの実装
- **`module_init.cpp`**: グローバルオブジェクトへの統合
- **`module.cpp`**: モジュール初期化エントリポイント

## 実装状況

- ✅ 基本オブジェクト: 完全実装
- ✅ 配列と集合型: 完全実装
- ✅ 数値と文字列: 完全実装
- ✅ 正規表現: 完全実装
- ✅ Promise: すべてのメソッド実装済み
- ✅ WeakRef: ES2021仕様に基づき実装済み
- ✅ FinalizationRegistry: 実装済み
- ✅ Proxy/Reflect: 実装済み

## グローバル統合

すべての組み込みオブジェクトは`modules.h`で宣言された初期化関数を通じてグローバルオブジェクトに登録されます：

```cpp
// グローバルオブジェクトへの組み込みオブジェクト登録
void initializeBuiltins(GlobalObject* globalObj) {
    registerObjectBuiltin(globalObj);
    registerFunctionBuiltin(globalObj);
    registerArrayBuiltin(globalObj);
    // ... 他のオブジェクト
}
``` 