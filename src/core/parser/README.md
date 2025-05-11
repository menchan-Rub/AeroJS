# AeroJS パーサーシステム

## 概要

AeroJSパーサーは、JavaScriptソースコードを解析して抽象構文木（AST）に変換するコンポーネントです。ECMAScript最新仕様に準拠し、高速で正確な構文解析を行います。

## 主要コンポーネント

### レキサー（字句解析）
- **Token**: JavaScript言語のトークン定義
- **Scanner**: ソースコードから意味のあるトークン列を生成
- **位置情報**: トークンの行番号、列番号、オフセット情報の管理

### パーサー（構文解析）
- **Parser**: トークン列からASTを構築
- **Error Handling**: 詳細なエラーメッセージと位置情報を提供
- **AST Nodes**: 式、文、宣言などの抽象構文要素

### JSON処理
- **JSONParser**: JSON形式のパースと生成
- **JSONStringify**: JavaScriptオブジェクトからJSONへの変換

### ソースマップ
- **SourceMap**: デバッグ情報とオリジナルソースの対応付け
- **位置マッピング**: 変換後コードと元コードの位置情報の変換

## 使用方法

```cpp
// パーサーの初期化
Parser parser;
parser.setStrictMode(true); // 厳格モード有効化

// ソースコードの解析
std::string source = "function add(a, b) { return a + b; }";
std::unique_ptr<ProgramNode> ast = parser.parse(source);

// 解析結果を使用
if (parser.hasError()) {
    std::cout << "解析エラー: " << parser.getErrorMessage() << std::endl;
} else {
    // ASTの処理...
    ASTVisitor visitor;
    ast->accept(&visitor);
}
```

## AST構造

AST（抽象構文木）は以下のクラス階層で表現されます：

- **Node**: すべてのASTノードの基底クラス
  - **Statement**: 文を表現するノード
    - **BlockStatement**: ブロック文
    - **IfStatement**: if文
    - など
  - **Expression**: 式を表現するノード
    - **BinaryExpression**: 二項演算式
    - **CallExpression**: 関数呼び出し
    - など
  - **Declaration**: 宣言を表現するノード
    - **FunctionDeclaration**: 関数宣言
    - **VariableDeclaration**: 変数宣言
    - など

## 実装状況

- ✅ ECMAScript 2022構文: 完全サポート
- ✅ JSX構文解析: サポート済み
- ✅ TypeScript型構文: 基本サポート
- ✅ JSONパーサー: 実装済み（コメントとシングルクォートサポートが今後の予定）
- ✅ ソースマップ生成: 実装済み

## AST訪問者パターン

パーサーはVisitorパターンをサポートし、AST解析を容易にします：

```cpp
class MyVisitor : public ASTVisitor {
public:
    void visitBinaryExpression(BinaryExpression* node) override {
        // 二項演算式の処理
    }
    
    void visitFunctionDeclaration(FunctionDeclaration* node) override {
        // 関数宣言の処理
    }
    // その他のオーバーライド...
};
``` 