# AeroJS ディレクトリ構造

```
/AeroJS/
│
├── src/                           # ソースコード
│   ├── core/                      # コアエンジン部分
│   │   ├── parser/                # 構文解析器
│   │   │   ├── lexer/             # 字句解析器
│   │   │   │   ├── token/         # トークン定義
│   │   │   │   ├── scanner/       # ソーススキャナー
│   │   │   │   └── regexp/        # 正規表現エンジン
│   │   │   ├── ast/               # 抽象構文木
│   │   │   │   ├── nodes/         # ASTノード定義
│   │   │   │   ├── visitors/      # ASTビジター
│   │   │   │   └── transformers/  # ASTトランスフォーマー
│   │   │   ├── json/              # JSONパーサー
│   │   │   └── sourcemap/         # ソースマップ処理
│   │   │
│   │   ├── compiler/              # コンパイラ
│   │   │   ├── bytecode/          # バイトコード定義
│   │   │   ├── codegen/           # コード生成
│   │   │   ├── pipeline/          # コンパイルパイプライン
│   │   │   └── serializer/        # バイトコードシリアライザー
│   │   │
│   │   ├── vm/                    # 仮想マシン
│   │   │   ├── interpreter/       # インタープリター
│   │   │   ├── stack/             # スタックマネージャー
│   │   │   ├── calling/           # 呼び出し規約
│   │   │   └── exception/         # 例外ハンドリング
│   │   │
│   │   ├── jit/                   # JITコンパイラ
│   │   │   ├── baseline/          # ベースラインJIT
│   │   │   ├── optimizing/        # 最適化JIT
│   │   │   ├── backend/           # コード生成バックエンド
│   │   │   │   ├── x86_64/        # x86_64用コード生成
│   │   │   │   ├── arm64/         # ARM64用コード生成
│   │   │   │   └── riscv/         # RISC-V用コード生成
│   │   │   ├── profiler/          # 実行プロファイラ
│   │   │   └── deoptimizer/       # 最適化解除機構
│   │   │
│   │   ├── runtime/               # ランタイムシステム
│   │   │   ├── types/             # 型システム
│   │   │   ├── builtins/          # 組み込みオブジェクト
│   │   │   │   ├── array/         # 配列オブジェクト
│   │   │   │   ├── string/        # 文字列オブジェクト
│   │   │   │   ├── math/          # 数学関数
│   │   │   │   ├── date/          # 日付関数
│   │   │   │   ├── regexp/        # 正規表現オブジェクト
│   │   │   │   ├── json/          # JSONオブジェクト
│   │   │   │   ├── promise/       # Promiseオブジェクト
│   │   │   │   ├── map/           # Map/Set/WeakMap
│   │   │   │   ├── typed_array/   # TypedArray
│   │   │   │   └── function/      # 関数オブジェクト
│   │   │   ├── globals/           # グローバルオブジェクト
│   │   │   ├── context/           # 実行コンテキスト
│   │   │   ├── symbols/           # Symbolサポート
│   │   │   ├── iteration/         # イテレータ実装
│   │   │   └── reflection/        # リフレクションAPI
│   │   │
│   │   ├── gc/                    # ガベージコレクション
│   │   │   ├── allocator/         # メモリアロケータ
│   │   │   ├── collector/         # GCアルゴリズム
│   │   │   │   ├── mark_sweep/    # マーク＆スイープGC
│   │   │   │   ├── generational/  # 世代別GC
│   │   │   │   └── incremental/   # インクリメンタルGC
│   │   │   ├── barrier/           # 書き込みバリア
│   │   │   ├── handles/           # GCハンドル
│   │   │   └── roots/             # ルート管理
│   │   │
│   │   └── objects/               # オブジェクトモデル
│   │       ├── model/             # オブジェクトモデル定義
│   │       ├── layout/            # オブジェクトレイアウト
│   │       ├── properties/        # プロパティ管理
│   │       ├── prototype/         # プロトタイプチェーン
│   │       └── shape/             # ヒドゥンクラス最適化
│   │
│   ├── api/                       # 外部API
│   │   ├── bindings/              # ネイティブバインディング
│   │   │   ├── c/                 # C言語バインディング
│   │   │   ├── cpp/               # C++バインディング
│   │   │   ├── python/            # Pythonバインディング
│   │   │   └── rust/              # Rustバインディング
│   │   ├── web/                   # Web標準API
│   │   │   ├── dom/               # DOM API
│   │   │   ├── xhr/               # XMLHttpRequest
│   │   │   ├── fetch/             # Fetch API
│   │   │   ├── storage/           # Web Storage
│   │   │   ├── workers/           # Web Workers
│   │   │   ├── canvas/            # Canvas API
│   │   │   ├── webgl/             # WebGL API
│   │   │   └── webgpu/            # WebGPU API
│   │   └── embedding/             # 組込み用API
│   │       ├── engine/            # エンジンAPI
│   │       ├── module/            # モジュールAPI
│   │       ├── value/             # 値操作API
│   │       └── callback/          # コールバック管理
│   │
│   ├── optimization/              # 最適化モジュール
│   │   ├── analyzer/              # 静的解析
│   │   │   ├── type_inference/    # 型推論
│   │   │   ├── escape_analysis/   # エスケープ解析
│   │   │   ├── constant_folding/  # 定数畳み込み
│   │   │   └── data_flow/         # データフロー解析
│   │   ├── ir/                    # 中間表現
│   │   │   ├── builder/           # IR構築
│   │   │   ├── validator/         # IR検証
│   │   │   ├── printer/           # IRデバッグ出力
│   │   │   └── serializer/        # IRシリアライズ
│   │   └── passes/                # 最適化パス
│   │       ├── inline/            # インライン展開
│   │       ├── dce/               # デッドコード除去
│   │       ├── loop/              # ループ最適化
│   │       ├── cse/               # 共通部分式除去
│   │       ├── licm/              # ループ不変コード移動
│   │       ├── peephole/          # ピープホール最適化
│   │       └── vectorization/     # ベクトル化
│   │
│   ├── platform/                  # プラットフォーム依存コード
│   │   ├── linux/                 # Linux実装
│   │   │   ├── memory/            # メモリ管理
│   │   │   ├── thread/            # スレッド管理
│   │   │   ├── time/              # 時間関連機能
│   │   │   └── file/              # ファイル操作
│   │   ├── windows/               # Windows実装
│   │   │   ├── memory/            # メモリ管理
│   │   │   ├── thread/            # スレッド管理
│   │   │   ├── time/              # 時間関連機能
│   │   │   └── file/              # ファイル操作
│   │   ├── macos/                 # macOS実装
│   │   │   ├── memory/            # メモリ管理
│   │   │   ├── thread/            # スレッド管理
│   │   │   ├── time/              # 時間関連機能
│   │   │   └── file/              # ファイル操作
│   │   └── freebsd/               # FreeBSD実装
│   │       ├── memory/            # メモリ管理
│   │       ├── thread/            # スレッド管理
│   │       ├── time/              # 時間関連機能
│   │       └── file/              # ファイル操作
│   │
│   ├── wasm/                      # WebAssemblyサポート
│   │   ├── parser/                # WASM バイナリパーサー
│   │   ├── compiler/              # WASM コンパイラ
│   │   ├── runtime/               # WASM ランタイム
│   │   ├── gc/                    # WASM GCサポート
│   │   └── api/                   # WASM API
│   │
│   └── utils/                     # ユーティリティ
│       ├── concurrent/            # 並行処理
│       │   ├── mutex/             # ミューテックス
│       │   ├── atomic/            # アトミック操作
│       │   ├── thread_pool/       # スレッドプール
│       │   └── task_queue/        # タスクキュー
│       ├── containers/            # コンテナ
│       │   ├── hashmap/           # ハッシュマップ
│       │   ├── vector/            # ベクトル
│       │   ├── list/              # リスト
│       │   └── string/            # 文字列コンテナ
│       ├── memory/                # メモリ管理
│       │   ├── allocators/        # カスタムアロケータ
│       │   ├── pool/              # メモリプール
│       │   ├── arena/             # アリーナアロケータ
│       │   └── smart_ptr/         # スマートポインタ
│       └── debugger/              # デバッグ機能
│           ├── inspector/         # JSインスペクタ
│           ├── profiler/          # プロファイラ
│           ├── heap_snapshot/     # ヒープスナップショット
│           └── logging/           # ロギング機能
│
├── include/                       # 公開ヘッダーファイル
│   ├── aerojs/                    # メインAPI
│   │   ├── aerojs.h               # メインヘッダー
│   │   ├── engine.h               # エンジンAPI
│   │   ├── value.h                # 値操作API
│   │   ├── object.h               # オブジェクトAPI
│   │   ├── function.h             # 関数API
│   │   ├── exception.h            # 例外処理
│   │   ├── context.h              # コンテキスト管理
│   │   ├── gc.h                   # GC制御API
│   │   ├── module.h               # モジュールAPI
│   │   ├── debug.h                # デバッグAPI
│   │   └── config.h               # 設定API
│   └── internal/                  # 内部API（拡張用）
│       ├── vm/                    # VM関連API
│       ├── compiler/              # コンパイラAPI
│       ├── jit/                   # JIT API
│       ├── gc/                    # GC API
│       └── runtime/               # ランタイムAPI
│
├── test/                          # テストコード
│   ├── unit/                      # ユニットテスト
│   │   ├── core/                  # コアコンポーネントテスト
│   │   ├── api/                   # API テスト
│   │   ├── optimization/          # 最適化テスト
│   │   └── utils/                 # ユーティリティテスト
│   ├── integration/               # 統合テスト
│   │   ├── api_integration/       # API統合テスト
│   │   ├── browser_integration/   # ブラウザ統合テスト
│   │   └── nodejs_integration/    # Node.js統合テスト
│   ├── benchmarks/                # ベンチマーク
│   │   ├── micro/                 # マイクロベンチマーク
│   │   ├── macro/                 # マクロベンチマーク
│   │   ├── jetstream/             # JetStream互換
│   │   ├── speedometer/           # Speedometer互換
│   │   └── octane/                # Octane互換
│   ├── conformance/               # 規格適合性テスト
│   │   ├── test262/               # Test262テストスイート
│   │   ├── wasm/                  # WASM適合性テスト
│   │   └── proposals/             # TC39提案テスト
│   └── fixtures/                  # テスト用データ
│       ├── scripts/               # テストスクリプト
│       ├── wasm_modules/          # WASMモジュール
│       └── data/                  # テストデータ
│
├── tools/                         # 開発ツール
│   ├── build/                     # ビルドスクリプト
│   │   ├── cmake/                 # CMakeモジュール
│   │   ├── gn/                    # GNビルド設定
│   │   ├── scripts/               # ビルドスクリプト
│   │   └── config/                # ビルド設定
│   ├── format/                    # コードフォーマッタ
│   │   ├── clang_format/          # clang-format設定
│   │   └── scripts/               # フォーマットスクリプト
│   ├── lint/                      # リンター
│   │   ├── clang_tidy/            # clang-tidy設定
│   │   ├── cpplint/               # cpplint設定
│   │   └── scripts/               # リントスクリプト
│   ├── profiler/                  # プロファイラ
│   │   ├── heap_profiler/         # ヒーププロファイラ
│   │   ├── cpu_profiler/          # CPUプロファイラ
│   │   └── visualization/         # 可視化ツール
│   ├── visualizer/                # 可視化ツール
│   │   ├── ir_visualizer/         # IR可視化
│   │   ├── ast_visualizer/        # AST可視化
│   │   ├── heap_visualizer/       # ヒープ可視化
│   │   └── performance_visualizer/ # パフォーマンス可視化
│   ├── benchmark_runner/          # ベンチマーク実行ツール
│   ├── fuzzer/                    # ファズテストツール
│   ├── api_doc_generator/         # API文書生成ツール
│   └── release/                   # リリースツール
│
├── docs/                          # ドキュメント
│   ├── api/                       # API リファレンス
│   │   ├── public/                # 公開API文書
│   │   ├── internal/              # 内部API文書
│   │   └── examples/              # API使用例
│   ├── design/                    # 設計ドキュメント
│   │   ├── architecture/          # アーキテクチャ文書
│   │   ├── performance/           # パフォーマンス設計
│   │   ├── security/              # セキュリティ設計
│   │   └── roadmap/               # 開発ロードマップ
│   ├── internals/                 # 内部構造
│   │   ├── vm/                    # VM解説
│   │   ├── gc/                    # GC解説
│   │   ├── jit/                   # JIT解説
│   │   ├── compiler/              # コンパイラ解説
│   │   └── optimization/          # 最適化解説
│   ├── contributing/              # 貢献ガイド
│   │   ├── code_style/            # コードスタイル
│   │   ├── testing/               # テスト方法
│   │   ├── debugging/             # デバッグ方法
│   │   └── review/                # レビュープロセス
│   └── examples/                  # サンプルコード
│       ├── embedding/             # 組込み例
│       ├── extensions/            # 拡張機能例
│       └── applications/          # アプリケーション例
│
├── third_party/                   # サードパーティライブラリ
│   ├── icu/                       # 国際化対応
│   ├── zlib/                      # 圧縮ライブラリ
│   ├── googletest/                # テストフレームワーク
│   ├── benchmark/                 # ベンチマークライブラリ
│   ├── llvm/                      # コード生成バックエンド
│   ├── abseil/                    # Abseilユーティリティ
│   ├── mimalloc/                  # 高性能メモリアロケータ
│   ├── tbb/                       # スレッディングライブラリ
│   ├── protobuf/                  # Protocolバッファー
│   └── json/                      # JSONライブラリ
│
├── examples/                      # サンプルプロジェクト
│   ├── simple_runtime/            # 簡易実行環境
│   │   ├── src/                   # ソースコード
│   │   ├── include/               # ヘッダーファイル
│   │   └── samples/               # サンプルスクリプト
│   ├── embedding/                 # 組込み例
│   │   ├── c_embedding/           # C言語組込み
│   │   ├── cpp_embedding/         # C++組込み
│   │   ├── python_embedding/      # Python組込み
│   │   └── rust_embedding/        # Rust組込み
│   └── benchmarks/                # ベンチマーク例
│       ├── simple_benchmarks/     # 単純ベンチマーク
│       ├── real_world/            # 実世界ベンチマーク
│       └── comparison/            # 他エンジン比較
│
├── build/                         # ビルド生成物（git管理外）
│   ├── debug/                     # デバッグビルド
│   ├── release/                   # リリースビルド
│   ├── coverage/                  # カバレッジビルド
│   └── artifacts/                 # ビルド成果物
│
├── .github/                       # GitHub設定
│   ├── workflows/                 # CI/CDワークフロー
│   │   ├── build.yml              # ビルドワークフロー
│   │   ├── test.yml               # テストワークフロー
│   │   ├── benchmark.yml          # ベンチマークワークフロー
│   │   └── release.yml            # リリースワークフロー
│   ├── ISSUE_TEMPLATE/            # Issue テンプレート
│   └── PULL_REQUEST_TEMPLATE.md   # PR テンプレート
│
├── scripts/                       # 各種スクリプト
│   ├── bootstrap.py               # 初期セットアップ
│   ├── generate_bindings.py       # バインディング生成
│   ├── release.py                 # リリース作成
│   ├── format_code.py             # コード整形
│   ├── lint_code.py               # コード検証
│   ├── run_tests.py               # テスト実行
│   ├── run_benchmarks.py          # ベンチマーク実行
│   └── generate_docs.py           # ドキュメント生成
│
├── .gitignore                     # Git除外設定
├── .clang-format                  # コードフォーマット設定
├── .clang-tidy                    # 静的解析設定
├── .editorconfig                  # エディタ設定
├── .gitattributes                 # Git属性設定
├── CMakeLists.txt                 # CMakeメインファイル
├── BUILD.gn                       # GNビルドファイル
├── LICENSE                        # ライセンス
├── README.md                      # プロジェクト概要
├── CONTRIBUTING.md                # 貢献ガイド
├── CODE_OF_CONDUCT.md             # 行動規範
└── CHANGELOG.md                   # 変更履歴
```

## 主要ディレクトリの詳細説明

### src/core/
AeroJSエンジンの中核となるコンポーネントを含むディレクトリです。JavaScriptの解析、コンパイル、実行、最適化に関わる全てのコードが含まれます。

#### parser/
JavaScriptソースコードを解析し、抽象構文木（AST）を生成する機能を提供します。字句解析、構文解析、AST構築の各段階を担当するコンポーネントが含まれます。さらに、JSONパーサーやソースマップ処理も含まれます。

#### compiler/
ASTからバイトコードへの変換を担当します。バイトコード命令の定義、コード生成、コンパイルパイプライン、バイトコードのシリアライズ/デシリアライズ機能が含まれます。

#### vm/
バイトコードを実行する仮想マシンの実装です。インタープリター、スタック管理、呼び出し規約、例外処理などが含まれます。これはエンジンの基本的な実行環境となります。

#### jit/
Just-In-Timeコンパイラの実装です。ベースラインJIT、最適化JIT、各種プラットフォーム向けのコード生成バックエンド、プロファイラ、最適化解除機構などが含まれます。これにより、高頻度で実行されるコードを高速化します。

#### runtime/
JavaScriptのランタイム機能を実装します。型システム、組み込みオブジェクト（Array、String、Math、Dateなど）、グローバルオブジェクト、実行コンテキスト、Symbol、イテレータ、リフレクションAPIなどが含まれます。

#### gc/
ガベージコレクションシステムを実装します。メモリアロケータ、各種GCアルゴリズム（マーク＆スイープ、世代別、インクリメンタル）、書き込みバリア、GCハンドル、ルート管理などが含まれます。

#### objects/
オブジェクトモデルを定義します。JavaScriptオブジェクトの内部表現、レイアウト、プロパティ管理、プロトタイプチェーン、ヒドゥンクラス最適化などの実装が含まれます。

### src/api/
エンジンの外部インターフェイスを提供します。ネイティブバインディング、Web標準API、組込みAPIなどが含まれます。

### src/optimization/
コード最適化に特化したモジュールです。静的解析、中間表現（IR）、最適化パスなどが含まれます。これにより、コードの実行効率を向上させます。

### src/platform/
各種プラットフォーム固有の実装を提供します。メモリ管理、スレッド、時間関連機能、ファイル操作などのプラットフォーム依存コードが含まれます。

### src/wasm/
WebAssemblyのサポートを実装します。WASMバイナリのパース、コンパイル、実行、GCサポート、APIなどが含まれます。

### src/utils/
エンジン全体で使用される汎用ユーティリティを提供します。並行処理、コンテナ、メモリ管理、デバッグ機能などが含まれます。

### include/
外部アプリケーションがAeroJSを利用するための公開ヘッダーファイルが含まれます。メインAPIとエンジン内部へのアクセスを提供する内部APIが区別されています。

### test/
各種テストコードとテストフレームワークが含まれます。ユニットテスト、統合テスト、ベンチマーク、ECMAScript適合性テスト（Test262）などが含まれます。

### tools/
開発を支援するツール群です。ビルドスクリプト、フォーマッタ、リンター、プロファイラ、可視化ツール、ベンチマーク実行ツール、ファズテストツール、API文書生成ツール、リリースツールなどが含まれます。

### docs/
プロジェクトに関する文書が含まれます。API仕様書、設計ドキュメント、内部アーキテクチャの説明、貢献ガイド、サンプルコードなどが含まれます。

### third_party/
依存するサードパーティライブラリが含まれます。国際化対応、圧縮ライブラリ、テストフレームワーク、ベンチマークライブラリ、コード生成バックエンド、各種ユーティリティライブラリなどが含まれます。

### examples/
AeroJSの使用例やデモアプリケーションが含まれます。簡易実行環境、各種言語への組込み例、ベンチマーク例などが含まれます。初めてエンジンを使用するユーザー向けのガイドとしても機能します。

### build/
ビルド生成物が配置されるディレクトリです。デバッグビルド、リリースビルド、カバレッジビルド、ビルド成果物などが含まれます。このディレクトリはgitの管理外となります。

### .github/
GitHub関連の設定ファイルが含まれます。CI/CDワークフロー、Issueテンプレート、PRテンプレートなどが含まれます。

### scripts/
各種スクリプトが含まれます。初期セットアップ、バインディング生成、リリース作成、コード整形、コード検証、テスト実行、ベンチマーク実行、ドキュメント生成などのスクリプトが含まれます。
