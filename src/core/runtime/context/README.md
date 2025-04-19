# @context API

## 概要

`@context`はJavaScriptコードを独立した環境で実行するためのAPIです。それぞれのコンテキストは独自のグローバルオブジェクト、変数、関数を持ち、お互いに干渉することなく安全に実行することができます。

ECMAScript Context APIの仕様に基づき実装されており、以下の機能を提供します：

- 分離された実行環境の作成
- コンテキスト内でのコード評価
- グローバル変数の操作
- モジュールのインポート
- コンテキストの破棄

## 使用方法

### コンテキストの作成

```javascript
// デフォルト設定でコンテキストを作成
const ctx = @context.create();

// オプションを指定してコンテキストを作成
const securectx = @context.create({
  strictMode: true,           // 厳格モードを有効化
  hasConsole: false,          // コンソールAPIを無効化
  hasModules: true,           // ESモジュールを有効化
  hasSharedArrayBuffer: false, // SharedArrayBufferを無効化
  locale: "ja-JP"             // 日本語ロケールを設定
});
```

### コードの実行

```javascript
// 単純な式を評価
const result = ctx.evaluate("40 + 2");
console.log(result); // 42

// 複数行のコードを実行
ctx.evaluate(`
  function greet(name) {
    return "こんにちは、" + name + "さん！";
  }
  
  var message = greet("世界");
`);

// 実行したコードで定義した変数や関数にアクセス
const message = ctx.evaluate("message");
console.log(message); // "こんにちは、世界さん！"
```

### コンテキスト間の分離

```javascript
// 別々のコンテキストを作成
const ctx1 = @context.create();
const ctx2 = @context.create();

// 各コンテキストで異なる変数を定義
ctx1.evaluate("var x = 10;");
ctx2.evaluate("var x = 20;");

// 各コンテキストの変数値を確認
console.log(ctx1.evaluate("x")); // 10
console.log(ctx2.evaluate("x")); // 20

// グローバルスコープの変数はコンテキスト間で共有されない
console.log(typeof x); // "undefined"
```

### グローバル変数の操作

```javascript
// グローバル変数の設定
ctx.setGlobal("user", { name: "田中", age: 30 });

// グローバル変数の取得
const user = ctx.getGlobal("user");
console.log(user.name); // "田中"

// 変数が存在するか確認
const hasUser = ctx.evaluate("'user' in globalThis");
console.log(hasUser); // true

// グローバル変数の削除
const deleted = ctx.deleteGlobal("user");
console.log(deleted); // true
console.log(ctx.getGlobal("user")); // undefined
```

### モジュールの読み込み

```javascript
// ファイルシステムからのモジュール読み込み
const math = ctx.importModule("./math.js");
console.log(math.add(5, 3)); // 8

// 組み込みモジュールの読み込み
const fs = ctx.importModule("fs");
```

### コンテキストオプションの取得

```javascript
// 現在のオプション設定を取得
const options = ctx.getOptions();
console.log(options);
/*
{
  strictMode: true,
  hasConsole: false,
  hasModules: true,
  hasSharedArrayBuffer: false,
  locale: "ja-JP"
}
*/
```

### コンテキストの破棄

```javascript
// コンテキストが不要になったら破棄（メモリリーク防止）
ctx.destroy();

// 破棄後のコンテキストは使用不可
try {
  ctx.evaluate("1 + 1");
} catch (e) {
  console.error(e); // "評価対象のコンテキストが無効です"
}
```

## 実装例：コード実行サンドボックス

```javascript
/**
 * 安全なサンドボックス環境でコードを実行する関数
 * @param {string} code 実行するJavaScriptコード
 * @param {Object} options サンドボックスオプション
 * @returns {Promise<any>} 実行結果を返すPromise
 */
function runInSandbox(code, options = {}) {
  // サンドボックスコンテキストの作成
  const sandbox = @context.create({
    strictMode: true,
    hasConsole: false, // コンソールへの直接アクセスを無効化
    hasModules: false  // モジュールのインポートを無効化
  });
  
  // 許可した機能のみをサンドボックスに提供
  sandbox.setGlobal("log", function(message) {
    console.log("[Sandbox]", message);
  });
  
  // ユーザーが指定したグローバル変数を設定
  if (options.globals) {
    for (const [key, value] of Object.entries(options.globals)) {
      sandbox.setGlobal(key, value);
    }
  }
  
  // タイムアウト処理
  const timeoutMs = options.timeout || 1000;
  const timeoutPromise = new Promise((_, reject) => {
    setTimeout(() => reject(new Error("実行がタイムアウトしました")), timeoutMs);
  });
  
  // コード実行処理
  const executionPromise = Promise.resolve().then(() => {
    try {
      return sandbox.evaluate(code);
    } finally {
      // 実行完了後にサンドボックスを破棄
      sandbox.destroy();
    }
  });
  
  // タイムアウトとの競争
  return Promise.race([executionPromise, timeoutPromise]);
}

// 使用例
runInSandbox(`
  // サンドボックス内のコード
  let sum = 0;
  for (let i = 1; i <= 100; i++) {
    sum += i;
  }
  log("1から100までの合計: " + sum);
  return { result: sum };
`, {
  timeout: 500,
  globals: {
    currentTime: Date.now()
  }
})
.then(result => console.log("実行結果:", result))
.catch(error => console.error("エラー:", error.message));
```

## 注意事項

1. コンテキストを使い終わったら必ず `destroy()` メソッドを呼び出して、メモリリークを防止してください。

2. コンテキスト間でオブジェクトを共有する場合は注意が必要です：
   - プリミティブ値（数値、文字列、真偽値）は安全に共有できます
   - オブジェクトを共有する場合は、シリアライズとデシリアライズを検討してください
   - 関数はコンテキスト間で直接共有できません

3. セキュリティ上の理由から、信頼できないコードを実行する場合は、適切な権限制限（コンソールの無効化、モジュールアクセスの制限など）を設定してください。

4. 大量のコンテキストを作成すると、メモリ使用量が増加する可能性があります。長時間使用しないコンテキストは破棄することをお勧めします。 