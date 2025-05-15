# AeroJS 変換システム

> **バージョン**: 2.0.0  
> **最終更新日**: 2024-06-24  
> **著者**: AeroJS Team

---

## 目次

1. [概要](#1-概要)
2. [主要コンポーネント](#2-主要コンポーネント)
3. [変換パイプライン](#3-変換パイプライン)
4. [AST最適化](#4-ast最適化)
5. [静的解析](#5-静的解析)
6. [コード生成](#6-コード生成)
7. [特殊変換](#7-特殊変換)
8. [使用例](#8-使用例)
9. [パフォーマンス特性](#9-パフォーマンス特性)
10. [実装状況](#10-実装状況)
11. [拡張ガイド](#11-拡張ガイド)
12. [API詳細](#12-api詳細)
13. [将来の拡張計画](#13-将来の拡張計画)

---

## 1. 概要

AeroJS変換システムは、JavaScript ASTを分析、最適化、変換するための強力なコンポーネントです。ソースコードの最適化、特殊機能の実装、コード生成のための変換処理を提供します。このモジュールはAeroJSエンジンのパフォーマンスと機能拡張の中核となります。

---

## 2. 主要コンポーネント

AeroJS変換システムは以下の主要コンポーネントで構成されています：

### AST最適化

- **定数畳み込み**: コンパイル時に計算可能な式を事前評価
- **デッドコード除去**: 到達不能または不要なコードの削除
- **インライン展開**: 単純な関数呼び出しを展開
- **ループ最適化**: ループの効率を向上させる変換

### 静的解析

- **型推論**: 変数と式の型情報の推定
- **副作用分析**: 関数やコードブロックの副作用を検出
- **到達性分析**: コードパスの実行可能性を判定
- **使用状況分析**: 変数や関数の使用状況を追跡

### コード生成

- **プリティプリンタ**: 整形されたJavaScriptコードの生成
- **ミニファイア**: サイズを最小化したコードの生成
- **ソースマップ出力**: 変換前後のコード対応付け情報の生成

### 特殊変換

- **ポリフィル注入**: 特定の機能に対するポリフィルの挿入
- **コンパイル時チェック**: 開発時の型チェックと警告
- **デバッグ対応**: デバッグ用のコードの挿入

### サブディレクトリ構造

- **`arch/`**: アーキテクチャ固有の最適化
- **`optimizers_config.json`**: 最適化設定
- **`*.cpp/h`**: 各変換器の実装

---

## 3. 変換パイプライン

AeroJS変換システムは、複数の変換をパイプラインとして組み合わせることで、効率的に変換処理を実行できます：

### パイプラインの構成

```
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│   ソースコード   │ ──> │    パーサー    │ ──> │      AST      │
└───────────────┘     └───────────────┘     └───────┬───────┘
                                                    │
                                                    ▼
┌───────────────┐     ┌───────────────┐     ┌───────────────┐
│  最終コード    │ <── │   コード生成   │ <── │  変換パイプライン │
└───────────────┘     └───────────────┘     └───────────────┘
```

### パイプラインの段階

1. **前処理変換**: シンタックスシュガーの展開や基本的な正規化
   - AST正規化
   - シンタックスシュガー展開
   - モジュール解決

2. **分析変換**: コードの特性に関する情報収集
   - 型推論
   - 変数使用分析
   - 副作用分析

3. **最適化変換**: コードの効率化
   - 定数畳み込み
   - デッドコード除去
   - インライン展開
   - ループ最適化

4. **特殊目的変換**: 特定の目的に応じた変換
   - 並列化変換
   - ポリフィル注入
   - デバッグ情報追加

5. **後処理変換**: コード生成前の最終調整
   - コード整形
   - ソースマップ生成

### パイプラインの作成と実行

```cpp
// 変換パイプライン作成
TransformPipeline pipeline;

// 変換器の追加
pipeline.addTransform(std::make_unique<ASTNormalizer>());
pipeline.addTransform(std::make_unique<TypeInference>());
pipeline.addTransform(std::make_unique<ConstantFolding>());
pipeline.addTransform(std::make_unique<DeadCodeElimination>());
pipeline.addTransform(std::make_unique<InlineFunctions>());

// 変換の実行
TransformContext context;
pipeline.apply(ast, context);
```

---

## 4. AST最適化

AeroJS変換システムは以下のAST最適化を提供します：

### 定数畳み込み (Constant Folding)

コンパイル時に計算可能な式を事前に評価します：

```javascript
// 変換前
const a = 3 + 4;
const b = "Hello, " + "World!";
const c = Math.PI * 2;

// 変換後
const a = 7;
const b = "Hello, World!";
const c = 6.283185307179586;
```

実装: `constant_folding.h/cpp`

### デッドコード除去 (Dead Code Elimination)

到達不可能なコードや使用されない変数を削除します：

```javascript
// 変換前
function foo() {
  let a = 1;
  let b = 2;
  return a;
  b = 3;  // 到達不能
}

// 変換後
function foo() {
  let a = 1;
  return a;
}
```

実装: `dead_code_elimination.h/cpp`

### インライン関数展開 (Function Inlining)

小さな関数の呼び出しを、その本体で置き換えることで関数呼び出しのオーバーヘッドを削減します：

```javascript
// 変換前
function square(x) {
  return x * x;
}
const result = square(5);

// 変換後
const result = 5 * 5;
```

実装: `inline_functions.h/cpp`

### 識別子ルックアップ最適化 (Identifier Lookup Optimization)

変数や関数の参照解決を最適化します：

```javascript
// 変換前
let x = 1;
function foo() {
  return x + 10;
}

// 変換後（内部表現）
// x への参照がスコープチェーンを短縮
let x = 1;
function foo() {
  return [直接参照:x] + 10;
}
```

実装: `identifier_lookup_optimization.h/cpp`

### 配列操作最適化 (Array Optimization)

配列操作を最適化し、メモリアクセスを改善します：

```javascript
// 変換前
const arr = new Array(1000);
for (let i = 0; i < arr.length; i++) {
  arr[i] = i * 2;
}

// 変換後（内部最適化）
const arr = new Array(1000);
const len = arr.length; // length をキャッシュ
for (let i = 0; i < len; i++) {
  arr[i] = i * 2;
}
```

実装: `array_optimization.h/cpp`

### 並列配列操作最適化 (Parallel Array Optimization)

適切な配列操作を並列処理するための変換を提供します：

```javascript
// 変換前
const result = array.map(x => x * 2).filter(x => x > 10);

// 変換後（内部的に並列処理を使用）
const result = __parallel_map_filter(array, x => x * 2, x => x > 10);
```

実装: `parallel_array_optimization.h/cpp`

---

## 5. 静的解析

コードの動作を静的に理解し、最適化の基盤を提供する分析機能：

### 型推論 (Type Inference)

変数や式の型を推測し、より効率的なコード生成を可能にします：

```javascript
// 入力コード
function add(a, b) {
  return a + b;
}
const x = add(1, 2);

// 推論情報
// add: (number, number) -> number
// x: number
```

型推論によって、適切な型に特化した最適化が可能になります。

実装: `type_inference.h/cpp`

### 到達性分析 (Reachability Analysis)

コードの各部分が実行される可能性を分析します：

```javascript
// 入力コード
function example(x) {
  if (x > 0) {
    return "positive";
  } else if (x < 0) {
    return "negative";
  }
  return "zero";
}

// 分析結果
// - `if` 条件内のコードは条件が真の場合のみ実行
// - `return "zero"` は x が 0 の場合のみ実行
```

この分析は不要なコードの削除に役立ちます。

### 副作用分析 (Side Effect Analysis)

関数や式が外部状態に与える影響を分析します：

```javascript
// 入力コード
function pure(x) { return x * 2; }
function impure(x) { console.log(x); return x; }

// 分析結果
// - pure: 副作用なし（純粋関数）
// - impure: 副作用あり（console.logによる外部状態変更）
```

副作用のない関数は、より積極的な最適化が可能です。

### 変数使用分析 (Variable Usage Analysis)

変数の定義と使用状況を分析します：

```javascript
// 入力コード
function example() {
  let a = 1;
  let b = 2;
  return a + b;
}

// 分析結果
// - a: 1回定義、1回使用
// - b: 1回定義、1回使用
```

この分析は不要な変数の削除やインライン化に役立ちます。

---

## 6. コード生成

ASTから様々な形式のコードを生成する機能：

### プリティプリンタ (Pretty Printer)

読みやすく整形されたJavaScriptコードを生成します：

```javascript
// 入力AST（略）

// 生成コード
function fibonacci(n) {
  if (n <= 1) {
    return n;
  }
  return fibonacci(n - 1) + fibonacci(n - 2);
}
```

### ミニファイア (Minifier)

サイズを最小化したJavaScriptコードを生成します：

```javascript
// 入力AST（略）

// 生成コード
function f(n){return n<=1?n:f(n-1)+f(n-2)}
```

### ソースマップ生成 (Source Map Generation)

変換前後のコード位置の対応関係を記録するソースマップを生成します：

```javascript
// ソースマップ例
{
  "version": 3,
  "sources": ["original.js"],
  "names": ["fibonacci", "n"],
  "mappings": "AAAA,SAASA,UAAU,CAACC,CAAC,EAAE;..."
}
```

---

## 7. 特殊変換

特定の目的のための特殊な変換：

### ポリフィル注入 (Polyfill Injection)

環境に応じて必要なポリフィルを自動的に挿入します：

```javascript
// 変換前
const set = new Set([1, 2, 3]);

// 変換後（Set未対応環境向け）
// ポリフィルコードを挿入
if (typeof Set === 'undefined') {
  // Setのポリフィル実装
}
const set = new Set([1, 2, 3]);
```

### デバッグ情報の挿入 (Debug Information Insertion)

デバッグ用の情報をコードに挿入します：

```javascript
// 変換前
function calculate(a, b) {
  return a + b;
}

// 変換後（デバッグモード）
function calculate(a, b) {
  __debug_enter("calculate", {a, b});
  try {
    return a + b;
  } finally {
    __debug_exit("calculate");
  }
}
```

### 開発時警告 (Development Warnings)

開発時に問題を検出して警告します：

```javascript
// 変換前
function divide(a, b) {
  return a / b;
}

// 変換後（開発モード）
function divide(a, b) {
  if (__DEV__ && b === 0) {
    console.warn("Potential division by zero detected");
  }
  return a / b;
}
```

---

## 8. 使用例

AeroJS変換システムの基本的な使用方法：

### 単一変換の適用

```cpp
// ASTの取得
std::unique_ptr<ProgramNode> ast = parser.parse(sourceCode);

// 定数畳み込み変換の作成と適用
ConstantFolding folding;
folding.transform(ast.get());

// 結果の出力
CodeGenerator generator;
std::string optimizedCode = generator.generate(ast.get());
```

### 複数変換の組み合わせ

```cpp
// AST取得
std::unique_ptr<ProgramNode> ast = parser.parse(sourceCode);

// 変換パイプライン作成
TransformPipeline pipeline;
pipeline.addTransform(std::make_unique<ConstantFolding>());
pipeline.addTransform(std::make_unique<DeadCodeElimination>());
pipeline.addTransform(std::make_unique<InlineFunctions>());

// 変換実行
TransformContext context;
pipeline.apply(ast.get(), context);

// 最適化されたコードを生成
CodeGenerator generator;
std::string optimizedCode = generator.generate(ast.get());
```

### カスタム変換器の実装

```cpp
// カスタム変換器の定義
class MyCustomTransformer : public Transformer {
public:
  void visitBinaryExpression(BinaryExpression* node) override {
    // 子ノードを先に処理
    node->getLeft()->accept(this);
    node->getRight()->accept(this);
    
    // 変換ロジック
    if (node->getOperator() == "+" && 
        node->getLeft()->isStringLiteral() && 
        node->getRight()->isStringLiteral()) {
      // 文字列リテラルの結合を定数に変換
      std::string left = node->getLeft()->asStringLiteral()->getValue();
      std::string right = node->getRight()->asStringLiteral()->getValue();
      StringLiteral* newLiteral = new StringLiteral(left + right);
      replaceNode(node, newLiteral);
    }
  }
};
```

---

## 9. パフォーマンス特性

AeroJS変換システムのパフォーマンス特性：

### 最適化効果

| 最適化タイプ | ファイルサイズ削減 | 実行速度向上 | メモリ使用量削減 |
|------------|----------------|-----------|--------------|
| 定数畳み込み | 5-15% | 10-20% | 3-8% |
| デッドコード除去 | 10-30% | 5-15% | 10-25% |
| インライン展開 | 2-10% | 15-40% | -5-10% |
| 型最適化 | 3-12% | 20-50% | 5-15% |
| すべて組み合わせ | 15-50% | 30-120% | 10-35% |

### 変換処理時間

| コード規模 | 単一変換 | 完全パイプライン |
|-----------|---------|---------------|
| 小 (< 1000行) | < 50ms | < 200ms |
| 中 (1000-10000行) | 50-300ms | 200-1000ms |
| 大 (> 10000行) | 300-2000ms | 1-5秒 |

### メモリ使用量

| コード規模 | ピークメモリ使用量 |
|-----------|----------------|
| 小 (< 1000行) | < 50MB |
| 中 (1000-10000行) | 50-200MB |
| 大 (> 10000行) | 200-500MB |

---

## 10. 実装状況

AeroJS変換システムの現在の実装状況：

| 変換器 | 状態 | 詳細 |
|-------|------|------|
| 定数畳み込み | ✅ 実装済み | 基本的な算術、文字列、論理演算をサポート |
| デッドコード除去 | ✅ 実装済み | 到達不能コードと未使用変数の除去 |
| AST正規化 | ✅ 実装済み | AST構造の標準化と簡素化 |
| 型推論 | ✅ 基本実装済み | プリミティブ型と単純なオブジェクト型をサポート |
| インライン展開 | ✅ 実装済み | 単純関数の展開 |
| 識別子ルックアップ最適化 | ✅ 実装済み | スコープチェーンの最適化 |
| 配列操作最適化 | ✅ 実装済み | 基本的な配列操作の最適化 |
| 並列配列操作最適化 | ✅ 実装済み | map, filter, reduceなどの並列化 |
| 高度なループ最適化 | ⚠️ 開発中 | ループアンワインドと融合 |
| ソースマップ生成 | ✅ 実装済み | 標準的なソースマップをサポート |
| デバッグ情報注入 | ✅ 実装済み | デバッグレベルに応じた情報注入 |
| ポリフィル注入 | ⚠️ 部分実装 | 主要機能のポリフィルをサポート |

---

## 11. 拡張ガイド

AeroJS変換システムを拡張するためのガイドライン：

### 新しい変換器の作成

1. `Transformer`クラスを継承して新しい変換器を作成：

```cpp
class MyNewTransformer : public Transformer {
public:
  void visitBinaryExpression(BinaryExpression* node) override {
    // 変換処理...
  }
  
  void visitFunctionDeclaration(FunctionDeclaration* node) override {
    // 変換処理...
  }
  
  // 必要な他のvisitメソッドの実装...
};
```

2. 変換ロジックを実装し、必要に応じてノードを置換：

```cpp
void MyNewTransformer::visitBinaryExpression(BinaryExpression* node) {
  // 子ノードを先に訪問（ボトムアップ処理の場合）
  node->getLeft()->accept(this);
  node->getRight()->accept(this);
  
  // 変換ロジック
  if (条件) {
    // 新しいノードを作成
    Expression* newNode = new ImprovedExpression(...);
    
    // 現在のノードを新しいノードで置換
    replaceNode(node, newNode);
  }
}
```

3. 変換器を変換パイプラインに登録：

```cpp
TransformPipeline pipeline;
pipeline.addTransform(std::make_unique<MyNewTransformer>());
```

### オプション設定の追加

1. 変換器にオプションを追加：

```cpp
class AdvancedTransformer : public Transformer {
public:
  struct Options {
    bool aggressiveMode = false;
    int optimizationLevel = 1;
  };
  
  explicit AdvancedTransformer(const Options& options = Options())
    : m_options(options) {}
  
private:
  Options m_options;
};
```

2. オプションを使用した変換処理：

```cpp
void AdvancedTransformer::visitFunctionDeclaration(FunctionDeclaration* node) {
  // オプションに基づいて変換戦略を変更
  if (m_options.aggressiveMode) {
    // 積極的な最適化
  } else {
    // 通常の最適化
  }
  
  // 最適化レベルに基づく処理
  if (m_options.optimizationLevel >= 2) {
    // レベル2以上の最適化
  }
}
```

### 分析情報の共有

1. 複数の変換器間でコンテキストを共有：

```cpp
struct SharedAnalysisContext {
  std::map<std::string, TypeInfo> typeInfoMap;
  std::set<ASTNode*> unreachableNodes;
  std::map<Expression*, ConstantValue> constantValues;
};

// 型推論の実装
class TypeInference : public Transformer {
public:
  explicit TypeInference(SharedAnalysisContext* context) 
    : m_context(context) {}
  
  void visitVariableDeclaration(VariableDeclaration* node) override {
    // 型情報を収集してコンテキストに格納
    TypeInfo type = inferType(node->getInitializer());
    m_context->typeInfoMap[node->getName()] = type;
  }
  
private:
  SharedAnalysisContext* m_context;
};

// 型情報を使用する別の変換器
class TypeBasedOptimizer : public Transformer {
public:
  explicit TypeBasedOptimizer(SharedAnalysisContext* context)
    : m_context(context) {}
  
  void visitCallExpression(CallExpression* node) override {
    // 型情報に基づいて最適化
    std::string calleeName = node->getCallee()->getName();
    if (m_context->typeInfoMap.count(calleeName)) {
      TypeInfo type = m_context->typeInfoMap[calleeName];
      // 型情報を使用した最適化...
    }
  }
  
private:
  SharedAnalysisContext* m_context;
};
```

---

## 12. API詳細

AeroJS変換システムの主要API：

### 変換器基底クラス

```cpp
class Transformer : public ASTVisitor {
public:
  virtual ~Transformer() = default;
  
  // 変換を適用する主要メソッド
  virtual void transform(ProgramNode* ast);
  
  // AST訪問メソッド（一部のみ表示）
  virtual void visitProgram(ProgramNode* node) override;
  virtual void visitFunctionDeclaration(FunctionDeclaration* node) override;
  virtual void visitBinaryExpression(BinaryExpression* node) override;
  // その他のノード型...
  
protected:
  // ノード置換ユーティリティ
  void replaceNode(ASTNode* oldNode, ASTNode* newNode);
  
  // AST操作ユーティリティ
  Expression* cloneExpression(Expression* expr);
  Statement* cloneStatement(Statement* stmt);
  
  // 変換コンテキスト
  TransformContext* context();
};
```

### 変換パイプライン

```cpp
class TransformPipeline {
public:
  // 変換器の追加
  void addTransform(std::unique_ptr<Transformer> transformer);
  
  // 指定された位置に変換器を挿入
  void insertTransform(size_t index, std::unique_ptr<Transformer> transformer);
  
  // 変換の実行
  void apply(ProgramNode* ast, TransformContext& context);
  
  // パイプラインのクリア
  void clear();
  
  // 変換器の数を取得
  size_t size() const;
};
```

### 変換コンテキスト

```cpp
class TransformContext {
public:
  // オプション設定
  void setOption(const std::string& name, const Value& value);
  Value getOption(const std::string& name) const;
  
  // 診断メッセージの追加
  void addDiagnostic(const Diagnostic& diagnostic);
  const std::vector<Diagnostic>& getDiagnostics() const;
  
  // 型情報
  void setTypeInfo(ASTNode* node, const TypeInfo& type);
  TypeInfo getTypeInfo(ASTNode* node) const;
  
  // 定数値
  void setConstantValue(Expression* expr, const ConstantValue& value);
  ConstantValue getConstantValue(Expression* expr) const;
};
```

### 変換パイプラインビルダー

```cpp
class TransformPipelineBuilder {
public:
  // 事前定義されたパイプラインの作成
  TransformPipeline buildDefaultPipeline();
  TransformPipeline buildMinimalPipeline();
  TransformPipeline buildAggressivePipeline();
  
  // カスタムパイプラインの構築
  TransformPipeline build(const std::vector<std::string>& transformerNames);
  
  // 設定ファイルからのパイプライン構築
  TransformPipeline buildFromConfig(const std::string& configFile);
};
```

---

## 13. 将来の拡張計画

AeroJS変換システムの将来の拡張計画：

1. **より高度なループ最適化**
   - ループアンワインディング
   - ループ融合と分割
   - ベクトル化対応

2. **流れ分析に基づく型推論の強化**
   - コントロールフロー分析
   - パス依存型推論
   - 合流点での型合成

3. **JITコンパイラとの統合による動的最適化**
   - 実行時プロファイルからのフィードバック
   - ホットパスの識別と特化
   - デオプティマイゼーションサポート

4. **並列実行のためのコード変換**
   - タスク分割ヒューリスティック
   - 依存関係解析
   - 自動並列化フレームワーク

5. **WebAssembly変換パイプライン**
   - JavaScript→WebAssembly変換
   - 型変換とメモリモデル
   - 相互運用レイヤー

6. **特殊環境向け最適化**
   - モバイルデバイス向け省電力最適化
   - サーバーサイド実行向け特化
   - IoTデバイス向けリソース効率 