# AeroJS ユーティリティライブラリ

このディレクトリには、AeroJSエンジンの共通ユーティリティが含まれています。

## 概要

AeroJSエンジンで使用される共通ユーティリティライブラリです。メモリ管理、コンテナ、文字列処理、ロギングなど、エンジン全体で必要となる基本的な機能を提供します。

## 主要コンポーネント

### メモリ管理

- **アロケータ**: カスタムメモリアロケーション戦略
- **プール**: オブジェクトプール実装
- **スマートポインタ**: 安全なメモリ所有権管理
- **ガベージコレクション**: メモリ回収機構

### コンテナ

- **ハッシュマップ**: 高速な連想配列実装
- **文字列**: 効率的な文字列処理とインターニング
- **リスト・ベクタ**: 動的配列実装
- **スタック・キュー**: LIFO/FIFOコンテナ

### ユーティリティ関数

- **ロギング**: 診断情報の出力
- **デバッグヘルパー**: 開発とデバッグに役立つ機能
- **ファイル入出力**: ファイルシステム操作
- **文字列変換**: フォーマットと変換関数

## メモリ管理システム

AeroJSは効率的なメモリ管理を実現するために、以下のコンポーネントを提供します：

1. **カスタムアロケータ**: システムレベルの割り当てを最適化
2. **オブジェクトプール**: 頻繁に使用されるオブジェクト用のプール
3. **世代別GC**: 効率的なガベージコレクション
4. **弱参照**: オブジェクト間の弱い参照

## ハッシュマップ実装

高性能な連想配列アクセスのための特殊なハッシュマップ実装：

- オープンアドレッシング方式
- 低衝突ハッシュ関数
- サイズ自動調整
- イテレータサポート

## 文字列処理

JavaScript文字列処理のための最適化された文字列クラス：

- UTF-16エンコーディング
- 文字列インターニング
- ロープアルゴリズムによる大きな文字列の効率的な操作
- 正規表現マッチングサポート

## スマートポインタ

メモリリークを防ぐための安全なポインタ管理：

- **SharedPtr**: 共有所有権
- **UniquePtr**: 排他的所有権
- **WeakPtr**: 弱参照
- **WeakHandle**: GCとの統合用弱参照

## 使用方法

```cpp
// ハッシュマップの使用例
HashMap<std::string, int> map;
map.insert("key1", 100);
map.insert("key2", 200);

if (map.contains("key1")) {
    int value = map.get("key1");
    // 値の使用...
}

// スマートポインタの使用例
SharedPtr<Object> obj = MakeShared<Object>();
obj->setProperty("name", "value");

// 弱参照の使用例
WeakPtr<Object> weakRef = obj.toWeak();
if (auto ptr = weakRef.lock()) {
    // オブジェクトがまだ生存している
} else {
    // オブジェクトは解放済み
}
```

## サブディレクトリ

- **`memory/`**: メモリ管理システム
- **`containers/`**: コンテナデータ構造
- **`string/`**: 文字列操作ユーティリティ 

## ロギングシステム

AeroJSは高性能で柔軟なロギングシステムを提供します。

### 特徴

- **高性能**: コンパイル時ログレベル制御により、リリースビルドでデバッグログを完全に除去
- **カテゴリ別管理**: コンポーネント別のログカテゴリをサポート
- **マルチターゲット**: コンソール、ファイル、コールバック、システムログに同時出力可能
- **非同期ロギング**: 高負荷環境での性能を最適化
- **型安全**: テンプレートベースの安全なフォーマット
- **クロスプラットフォーム**: Windows/Linux/macOS対応

### 基本的な使用方法

```cpp
#include "utils/logging.h"

int main() {
    // ロギングシステムの初期化
    aerojs::utils::ConfigureDebugLogging();
    
    // 基本ログ出力
    AEROJS_LOG_INFO("アプリケーションを開始しました");
    AEROJS_LOG_DEBUG("デバッグ情報: %d", 42);
    AEROJS_LOG_WARNING("警告メッセージ");
    AEROJS_LOG_ERROR("エラーが発生しました: %s", error.c_str());
    
    // カテゴリ別ログ
    AEROJS_JIT_LOG_INFO("JITコンパイルが完了しました");
    AEROJS_GC_LOG_DEBUG("ガベージコレクションを実行中");
    
    // パフォーマンス測定
    {
        AEROJS_SCOPED_TIMER("重い処理");
        // 処理...
    } // ここで自動的に時間がログ出力される
    
    return 0;
}
```

### カテゴリ

利用可能なログカテゴリ：

- **aerojs**: デフォルトカテゴリ
- **jit**: JITコンパイラ関連
- **parser**: パーサー関連
- **runtime**: ランタイム関連
- **network**: ネットワーク関連
- **gc**: ガベージコレクタ関連
- **optimizer**: 最適化器関連
- **profiler**: プロファイラ関連

### 設定オプション

#### デバッグ設定
```cpp
aerojs::utils::ConfigureDebugLogging();
```
- ログレベル: TRACE
- 出力先: コンソール + ファイル
- 色付き出力: 有効
- ソース位置: 表示

#### プロダクション設定
```cpp
aerojs::utils::ConfigureProductionLogging();
```
- ログレベル: WARNING以上
- 出力先: ファイル（ローテーション付き）
- 非同期ロギング: 有効
- 最大ファイルサイズ: 100MB

#### カスタム設定
```cpp
aero::LoggerOptions options;
options.level = aerojs::utils::LogLevel::INFO;
options.useColors = true;
options.asyncLogging = true;
options.logFilePath = "custom.log";
options.targets = {aero::LogTarget::CONSOLE, aero::LogTarget::FILE};
aerojs::utils::InitializeLogging(options);
```

### 高度な機能

#### エラーコンテキスト付きログ
```cpp
AEROJS_LOG_ERROR_WITH_CONTEXT("詳細なエラー情報: %s", details.c_str());
// 出力: [filename:line in function] 詳細なエラー情報: ...
```

#### アサーション
```cpp
AEROJS_LOG_ASSERT(condition, "条件が満たされませんでした: %d", value);
```

#### メモリ使用量ログ
```cpp
AEROJS_LOG_MEMORY_USAGE("処理完了時点");
// 出力: 処理完了時点 - RSS: 123 KB, VSize: 456 KB
```

#### スタックトレース
```cpp
aerojs::utils::DumpStackTrace("error");
```

### パフォーマンス最適化

#### コンパイル時ログレベル制御
```cpp
// CMakeでNDEBUGが定義されている場合、TRACEとDEBUGログは完全に除去される
#ifndef NDEBUG
    AEROJS_LOG_TRACE("デバッグビルドでのみ実行");
#endif
```

#### 非同期ロギング
```cpp
// 高負荷環境では非同期ロギングを有効化
aerojs::utils::EnableAsyncLogging(true);
```

### ログレベル

| レベル | 用途 | プロダクション |
|--------|------|----------------|
| TRACE | 詳細なトレース情報 | 無効 |
| DEBUG | デバッグ情報 | 無効 |
| INFO | 一般的な情報 | 有効 |
| WARNING | 警告メッセージ | 有効 |
| ERROR | エラーメッセージ | 有効 |
| CRITICAL | 致命的エラー | 有効 |

### ファイル構成

- `logging.h`: ロギングシステムのヘッダー
- `logging.cpp`: ロギングシステムの実装
- `test_logging.cpp`: テストプログラム

### 依存関係

- `src/core/utils/logger.h`: コアロガー実装
- 標準ライブラリのみ（外部依存なし）

### 使用例

JITコンパイラでの使用例：
```cpp
void CompileFunction(const IRFunction& function) {
    AEROJS_JIT_SCOPED_TIMER("関数コンパイル");
    AEROJS_JIT_LOG_INFO("関数 '%s' のコンパイルを開始", function.GetName().c_str());
    
    try {
        // コンパイル処理
        AEROJS_JIT_LOG_DEBUG("最適化パス %d を実行中", passId);
        
        // 成功ログ
        AEROJS_JIT_LOG_INFO("コンパイル完了: %zu バイト生成", codeSize);
    } catch (const std::exception& e) {
        AEROJS_JIT_LOG_ERROR_WITH_CONTEXT("コンパイルエラー: %s", e.what());
        throw;
    }
}
``` 