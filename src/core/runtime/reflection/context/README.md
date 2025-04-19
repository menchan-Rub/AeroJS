# @context オブジェクト

## 概要

`@context` オブジェクトは、JavaScriptの実行環境を分離して管理するためのAPIを提供します。ECMAScript Context APIの仕様に基づいた実装で、別々のグローバル環境でコードを安全に実行することができます。

## 使用方法

### 新しいコンテキストの作成

```javascript
// オプションを指定してコンテキストを作成
const ctx = @context.create({
  strictMode: true,          // 厳格モードを有効にする
  hasConsole: true,          // コンソールAPIを有効にする
  hasModules: true,          // ES Modulesを有効にする
  hasSharedArrayBuffer: false, // SharedArrayBufferを無効にする
  locale: "ja-JP"            // ロケールを設定
});

// デフォルトオプションでコンテキストを作成
const ctx2 = @context.create();
```

### コンテキスト内でのコード実行

```javascript
// コンテキスト内でコードを評価
const result = ctx.evaluate("1 + 2");
console.log(result); // 3

// 変数を定義して利用
ctx.evaluate("var x = 10; var y = 20;");
const sum = ctx.evaluate("x + y");
console.log(sum); // 30

// コンテキスト間は分離されている
ctx2.evaluate("var x = 100;");
console.log(ctx.evaluate("x")); // 10 (ctx2の値は影響しない)
console.log(ctx2.evaluate("x")); // 100
```

### グローバル変数の管理

```javascript
// グローバル変数の設定
ctx.setGlobal("message", "こんにちは世界");

// グローバル変数の取得
const message = ctx.getGlobal("message");
console.log(message); // "こんにちは世界"

// グローバル変数の削除
const deleted = ctx.deleteGlobal("message");
console.log(deleted); // true
console.log(ctx.getGlobal("message")); // undefined
```

### モジュールの読み込み

```javascript
// モジュールを読み込む（ファイルシステムからの相対パス）
const mathModule = ctx.importModule("./math.js");
console.log(mathModule.add(5, 3)); // 8

// 標準モジュールの読み込み
const fs = ctx.importModule("fs");
```

### その他の操作

```javascript
// グローバルオブジェクトの取得
const globalObj = ctx.getGlobalObject();

// コンテキストオプションの取得
const options = ctx.getOptions();
console.log(options.strictMode); // true

// コンテキストの破棄（メモリリーク防止のため終了時に呼ぶべき）
ctx.destroy();
```

## 使用例: サンドボックス実行

```javascript
// 安全なコード実行サンドボックスを作成
function createSandbox(code) {
  const sandbox = @context.create({
    strictMode: true,
    hasConsole: false // コンソールAPIを無効化
  });
  
  // 制限されたAPIのみを提供
  sandbox.setGlobal("safeLog", function(msg) {
    console.log("[Sandbox]", msg);
  });
  
  try {
    // ユーザーコードの実行 (10ms以内の実行を許可)
    const timeoutPromise = new Promise((_, reject) => {
      setTimeout(() => reject(new Error("実行タイムアウト")), 10);
    });
    
    const executionPromise = Promise.resolve().then(() => {
      return sandbox.evaluate(code);
    });
    
    return Promise.race([executionPromise, timeoutPromise])
      .finally(() => sandbox.destroy());
  } catch (e) {
    sandbox.destroy();
    return Promise.reject(new Error("実行エラー: " + e.message));
  }
}

// 使用例
createSandbox(`
  safeLog("Hello from sandbox!");
  const result = 42 * 2;
  safeLog("Result: " + result);
`).then(
  result => console.log("実行結果:", result),
  error => console.error("エラー:", error)
);
```

## 注意事項

- 各コンテキストは完全に分離されているため、あるコンテキストで定義された変数や関数は他のコンテキストからはアクセスできません。
- コンテキストを使い終わったら必ず `destroy()` メソッドを呼び出して、メモリリークを防止してください。
- コンテキスト間でオブジェクトを共有する場合は、プリミティブ値（数値、文字列、真偽値）を使用するか、シリアライズ可能なオブジェクトのみを使用してください。 