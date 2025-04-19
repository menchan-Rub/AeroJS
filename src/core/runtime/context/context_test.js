/**
 * @context APIテストスクリプト
 * 
 * このスクリプトは@context APIの基本的な機能をテストします。
 * 各テストケースは個別に実行され、成功または失敗が記録されます。
 */

// テスト用のユーティリティ関数
function assert(condition, message) {
  if (!condition) {
    throw new Error("アサート失敗: " + message);
  }
}

// テストケース
const tests = [
  // テスト1: コンテキスト作成
  function testCreateContext() {
    const ctx = @context.create();
    assert(ctx !== null && typeof ctx === "object", "コンテキストオブジェクトが作成されるべき");
    assert(typeof ctx.evaluate === "function", "evaluateメソッドが存在するべき");
    assert(typeof ctx.setGlobal === "function", "setGlobalメソッドが存在するべき");
    assert(typeof ctx.getGlobal === "function", "getGlobalメソッドが存在するべき");
    assert(typeof ctx.deleteGlobal === "function", "deleteGlobalメソッドが存在するべき");
    assert(typeof ctx.destroy === "function", "destroyメソッドが存在するべき");
  },
  
  // テスト2: オプション付きコンテキスト作成
  function testCreateContextWithOptions() {
    const ctx = @context.create({
      strictMode: true,
      hasConsole: false,
      hasModules: true,
      hasSharedArrayBuffer: false,
      locale: "ja-JP"
    });
    
    const options = ctx.getOptions();
    assert(options.strictMode === true, "strictModeオプションが設定されるべき");
    assert(options.hasConsole === false, "hasConsoleオプションが設定されるべき");
    assert(options.hasModules === true, "hasModulesオプションが設定されるべき");
    assert(options.hasSharedArrayBuffer === false, "hasSharedArrayBufferオプションが設定されるべき");
    assert(options.locale === "ja-JP", "localeオプションが設定されるべき");
  },
  
  // テスト3: 基本的な式の評価
  function testEvaluateBasicExpression() {
    const ctx = @context.create();
    const result = ctx.evaluate("2 + 3 * 4");
    assert(result === 14, "基本的な数式が正しく評価されるべき");
  },
  
  // テスト4: 変数定義と参照
  function testVariableDefinitionAndAccess() {
    const ctx = @context.create();
    ctx.evaluate("var x = 42; var y = 'こんにちは';");
    
    const x = ctx.evaluate("x");
    const y = ctx.evaluate("y");
    
    assert(x === 42, "数値変数が正しく評価されるべき");
    assert(y === "こんにちは", "文字列変数が正しく評価されるべき");
  },
  
  // テスト5: 関数定義と呼び出し
  function testFunctionDefinitionAndCall() {
    const ctx = @context.create();
    ctx.evaluate(`
      function add(a, b) {
        return a + b;
      }
    `);
    
    const result = ctx.evaluate("add(5, 7)");
    assert(result === 12, "関数が正しく評価されるべき");
  },
  
  // テスト6: コンテキスト間の分離
  function testContextIsolation() {
    const ctx1 = @context.create();
    const ctx2 = @context.create();
    
    ctx1.evaluate("var shared = 'ctx1 data';");
    ctx2.evaluate("var shared = 'ctx2 data';");
    
    const value1 = ctx1.evaluate("shared");
    const value2 = ctx2.evaluate("shared");
    
    assert(value1 === "ctx1 data", "ctx1の変数が維持されるべき");
    assert(value2 === "ctx2 data", "ctx2の変数が維持されるべき");
    assert(value1 !== value2, "コンテキスト間で変数が分離されるべき");
  },
  
  // テスト7: グローバル変数の設定と取得
  function testGlobalVariables() {
    const ctx = @context.create();
    
    // プリミティブ値のテスト
    ctx.setGlobal("number", 123);
    ctx.setGlobal("string", "テスト");
    ctx.setGlobal("boolean", true);
    
    assert(ctx.getGlobal("number") === 123, "数値が正しく取得されるべき");
    assert(ctx.getGlobal("string") === "テスト", "文字列が正しく取得されるべき");
    assert(ctx.getGlobal("boolean") === true, "真偽値が正しく取得されるべき");
    
    // オブジェクトのテスト
    ctx.setGlobal("object", { a: 1, b: 2 });
    const obj = ctx.getGlobal("object");
    assert(typeof obj === "object", "オブジェクトが正しく取得されるべき");
    assert(obj.a === 1 && obj.b === 2, "オブジェクトのプロパティが正しく取得されるべき");
    
    // JavaScriptからアクセス
    const result = ctx.evaluate("number + 100");
    assert(result === 223, "JavaScriptからグローバル変数にアクセスできるべき");
  },
  
  // テスト8: グローバル変数の削除
  function testDeleteGlobal() {
    const ctx = @context.create();
    
    ctx.setGlobal("toDelete", "削除対象");
    assert(ctx.getGlobal("toDelete") === "削除対象", "変数が設定されるべき");
    
    const deleted = ctx.deleteGlobal("toDelete");
    assert(deleted === true, "deleteGlobalはtrueを返すべき");
    assert(ctx.getGlobal("toDelete") === undefined, "変数が削除されるべき");
  },
  
  // テスト9: エラーハンドリング
  function testErrorHandling() {
    const ctx = @context.create();
    
    let errorCaught = false;
    try {
      ctx.evaluate("throw new Error('意図的なエラー')");
    } catch (e) {
      errorCaught = true;
      assert(e.message.includes("意図的なエラー"), "エラーメッセージが伝播するべき");
    }
    assert(errorCaught, "エラーがキャッチされるべき");
  },
  
  // テスト10: 構文エラー
  function testSyntaxError() {
    const ctx = @context.create();
    
    let errorCaught = false;
    try {
      ctx.evaluate("function() { return 1; }"); // 構文エラー
    } catch (e) {
      errorCaught = true;
      assert(e instanceof Error, "構文エラーがエラーオブジェクトであるべき");
    }
    assert(errorCaught, "構文エラーがキャッチされるべき");
  },
  
  // テスト11: コンテキストの破棄
  function testDestroyContext() {
    const ctx = @context.create();
    ctx.evaluate("var x = 42;");
    
    // 破棄前は正常に動作
    assert(ctx.evaluate("x") === 42, "破棄前は変数にアクセスできるべき");
    
    // コンテキストを破棄
    ctx.destroy();
    
    // 破棄後はエラーになる
    let errorCaught = false;
    try {
      ctx.evaluate("x");
    } catch (e) {
      errorCaught = true;
    }
    assert(errorCaught, "破棄後のコンテキストにアクセスするとエラーになるべき");
  }
];

// テストの実行
function runTests() {
  console.log("@context APIテスト開始");
  console.log("--------------------------");
  
  let passed = 0;
  let failed = 0;
  
  for (let i = 0; i < tests.length; i++) {
    const test = tests[i];
    const testName = test.name;
    
    try {
      test();
      console.log(`✓ ${testName} - 成功`);
      passed++;
    } catch (e) {
      console.error(`✗ ${testName} - 失敗: ${e.message}`);
      console.error(e.stack);
      failed++;
    }
  }
  
  console.log("--------------------------");
  console.log(`テスト完了: ${passed + failed}件中、${passed}件成功、${failed}件失敗`);
  
  return failed === 0;
}

// テスト実行
const success = runTests();
if (!success) {
  process.exit(1);
} 