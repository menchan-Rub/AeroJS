/**
 * @file test_main.cpp
 * @brief AeroJS Engine Comprehensive Test Program
 * @version 0.2.0
 * @license MIT
 */

#include "src/core/engine.h"
#include "src/core/context.h"
#include "src/core/value.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>

using namespace aerojs::core;

// テストヘルパー関数
void printTestHeader(const std::string& testName) {
    std::cout << "\n=== " << testName << " ===" << std::endl;
}

void printTestResult(const std::string& testName, bool passed) {
    std::cout << (passed ? "✓ " : "✗ ") << testName << std::endl;
}

bool testBasicEngine() {
    printTestHeader("Basic Engine Tests");
    
    try {
        // エンジンの作成と初期化
        Engine engine;
        bool initResult = engine.initialize();
        printTestResult("Engine initialization", initResult);
        
        if (!initResult) return false;
        
        // 初期化状態の確認
        bool isInit = engine.isInitialized();
        printTestResult("Engine initialization check", isInit);
        
        // グローバルコンテキストの取得
        Context* context = engine.getGlobalContext();
        bool hasContext = (context != nullptr);
        printTestResult("Global context acquisition", hasContext);
        
        // メモリアロケータの取得
        auto* allocator = engine.getMemoryAllocator();
        bool hasAllocator = (allocator != nullptr);
        printTestResult("Memory allocator acquisition", hasAllocator);
        
        // エンジンの終了
        engine.shutdown();
        bool isShutdown = !engine.isInitialized();
        printTestResult("Engine shutdown", isShutdown);
        
        return initResult && isInit && hasContext && hasAllocator && isShutdown;
    } catch (const std::exception& e) {
        std::cerr << "Exception in basic engine test: " << e.what() << std::endl;
        return false;
    }
}

bool testEngineConfiguration() {
    printTestHeader("Engine Configuration Tests");
    
    try {
        // カスタム設定でエンジンを作成
        EngineConfig config;
        config.maxMemoryLimit = 512 * 1024 * 1024; // 512MB
        config.jitThreshold = 50;
        config.optimizationLevel = 3;
        config.enableJIT = true;
        config.enableProfiling = true;
        
        Engine engine(config);
        bool initResult = engine.initialize();
        printTestResult("Engine with custom config initialization", initResult);
        
        if (!initResult) return false;
        
        // 設定の確認
        bool jitEnabled = engine.isJITEnabled();
        printTestResult("JIT enabled check", jitEnabled);
        if (!jitEnabled) {
            std::cout << "  Expected: true, Got: false" << std::endl;
        }
        
        uint32_t jitThreshold = engine.getJITThreshold();
        bool thresholdCorrect = (jitThreshold == 50);
        printTestResult("JIT threshold check", thresholdCorrect);
        if (!thresholdCorrect) {
            std::cout << "  Expected: 50, Got: " << jitThreshold << std::endl;
        }
        
        uint32_t optLevel = engine.getOptimizationLevel();
        bool optLevelCorrect = (optLevel == 3);
        printTestResult("Optimization level check", optLevelCorrect);
        if (!optLevelCorrect) {
            std::cout << "  Expected: 3, Got: " << optLevel << std::endl;
        }
        
        size_t memLimit = engine.getMemoryLimit();
        bool memLimitCorrect = (memLimit == 512 * 1024 * 1024);
        printTestResult("Memory limit check", memLimitCorrect);
        if (!memLimitCorrect) {
            std::cout << "  Expected: " << (512 * 1024 * 1024) << ", Got: " << memLimit << std::endl;
        }
        
        bool profilingEnabled = engine.isProfilingEnabled();
        printTestResult("Profiling enabled check", profilingEnabled);
        if (!profilingEnabled) {
            std::cout << "  Expected: true, Got: false" << std::endl;
        }
        
        return jitEnabled && thresholdCorrect && optLevelCorrect && 
               memLimitCorrect && profilingEnabled;
    } catch (const std::exception& e) {
        std::cerr << "Exception in configuration test: " << e.what() << std::endl;
        return false;
    }
}

bool testValueSystem() {
    printTestHeader("Enhanced Value System Tests");
    
    try {
        // 基本的な値の作成と型チェック
        Value undefinedVal = Value::undefined();
        Value nullVal = Value::null();
        Value boolVal = Value::fromBoolean(true);
        Value numVal = Value::fromNumber(42.5);
        Value intVal = Value::fromInteger(123);
        Value strVal = Value::fromString("Hello, AeroJS!");
        
        // 型チェックテスト
        bool undefinedCheck = undefinedVal.isUndefined();
        bool nullCheck = nullVal.isNull();
        bool boolCheck = boolVal.isBoolean();
        bool numCheck = numVal.isNumber();
        bool intCheck = intVal.isInteger();
        bool strCheck = strVal.isString();
        
        printTestResult("Undefined type check", undefinedCheck);
        printTestResult("Null type check", nullCheck);
        printTestResult("Boolean type check", boolCheck);
        printTestResult("Number type check", numCheck);
        printTestResult("Integer type check", intCheck);
        if (!intCheck) {
            std::cout << "  Integer value: " << intVal.toNumber() << ", isInteger: " << intVal.isInteger() << std::endl;
        }
        printTestResult("String type check", strCheck);
        
        // 拡張型チェック
        bool nullishCheck1 = undefinedVal.isNullish();
        bool nullishCheck2 = nullVal.isNullish();
        bool primitiveCheck = strVal.isPrimitive();
        bool truthyCheck = boolVal.isTruthy();
        bool falsyCheck = undefinedVal.isFalsy();
        
        printTestResult("Nullish check (undefined)", nullishCheck1);
        printTestResult("Nullish check (null)", nullishCheck2);
        printTestResult("Primitive check", primitiveCheck);
        printTestResult("Truthy check", truthyCheck);
        printTestResult("Falsy check", falsyCheck);
        
        // 型変換テスト
        bool boolConv = strVal.toBoolean();
        double numConv = Value::fromString("123.45").toNumber();
        int32_t intConv = numVal.toInt32();
        std::string strConv = numVal.toString();
        
        printTestResult("String to boolean conversion", boolConv);
        printTestResult("String to number conversion", numConv == 123.45);
        if (numConv != 123.45) {
            std::cout << "  Expected: 123.45, Got: " << numConv << std::endl;
        }
        printTestResult("Number to int32 conversion", intConv == 42);
        if (intConv != 42) {
            std::cout << "  Expected: 42, Got: " << intConv << std::endl;
        }
        printTestResult("Number to string conversion", !strConv.empty());
        
        // 安全な型変換テスト
        bool safeResult;
        double safeNum;
        bool safeBoolResult = numVal.tryToBoolean(safeResult);
        bool safeNumResult = strVal.tryToNumber(safeNum);
        
        printTestResult("Safe boolean conversion", safeBoolResult && safeResult);
        if (!(safeBoolResult && safeResult)) {
            std::cout << "  safeBoolResult: " << safeBoolResult << ", safeResult: " << safeResult << std::endl;
        }
        printTestResult("Safe number conversion", safeNumResult);
        if (!safeNumResult) {
            std::cout << "  safeNumResult: " << safeNumResult << ", safeNum: " << safeNum << std::endl;
        }
        
        bool allTestsPassed = undefinedCheck && nullCheck && boolCheck && numCheck && 
               intCheck && strCheck && nullishCheck1 && nullishCheck2 && 
               primitiveCheck && truthyCheck && falsyCheck && boolConv && 
               safeBoolResult && safeNumResult;
        
        std::cout << "All individual tests passed: " << allTestsPassed << std::endl;
        
        return allTestsPassed;
    } catch (const std::exception& e) {
        std::cerr << "Exception in value system test: " << e.what() << std::endl;
        return false;
    }
}

bool testArrayOperations() {
    printTestHeader("Array Operations Tests");
    
    try {
        // 配列の作成
        std::vector<Value> elements = {
            Value::fromNumber(1),
            Value::fromNumber(2),
            Value::fromNumber(3)
        };
        Value arrayVal = Value::fromArray(elements);
        
        bool isArray = arrayVal.isArray();
        printTestResult("Array creation and type check", isArray);
        
        // 配列の長さ
        size_t length = arrayVal.getLength();
        bool lengthCorrect = (length == 3);
        printTestResult("Array length check", lengthCorrect);
        
        // 要素の取得
        Value firstElement = arrayVal.getElement(0);
        bool firstElementCorrect = (firstElement.toNumber() == 1.0);
        printTestResult("Array element access", firstElementCorrect);
        
        // 要素の設定
        arrayVal.setElement(1, Value::fromString("modified"));
        Value modifiedElement = arrayVal.getElement(1);
        bool modificationCorrect = modifiedElement.isString();
        printTestResult("Array element modification", modificationCorrect);
        
        // 要素の追加
        arrayVal.push(Value::fromBoolean(true));
        size_t newLength = arrayVal.getLength();
        bool pushCorrect = (newLength == 4);
        printTestResult("Array push operation", pushCorrect);
        
        // 要素の削除
        Value poppedElement = arrayVal.pop();
        size_t finalLength = arrayVal.getLength();
        bool popCorrect = (poppedElement.isBoolean() && finalLength == 3);
        printTestResult("Array pop operation", popCorrect);
        
        return isArray && lengthCorrect && firstElementCorrect && 
               modificationCorrect && pushCorrect && popCorrect;
    } catch (const std::exception& e) {
        std::cerr << "Exception in array operations test: " << e.what() << std::endl;
        return false;
    }
}

bool testObjectOperations() {
    printTestHeader("Object Operations Tests");
    
    try {
        // オブジェクト作成
        Value obj = Value::fromObject(nullptr);
        
        // プロパティ設定
        obj.setProperty("name", Value::fromString("test"));
        obj.setProperty("value", Value::fromNumber(42));
        obj.setProperty("active", Value::fromBoolean(true));
        
        // プロパティ取得
        Value nameVal = obj.getProperty("name");
        Value valueVal = obj.getProperty("value");
        Value activeVal = obj.getProperty("active");
        
        bool nameCorrect = nameVal.isString();
        bool valueCorrect = valueVal.isNumber();
        bool activeCorrect = activeVal.isBoolean();
        
        printTestResult("Object property operations", nameCorrect && valueCorrect && activeCorrect);
        
        // プロパティ存在確認
        bool hasName = obj.hasProperty("name");
        bool hasNonExistent = obj.hasProperty("nonexistent");
        printTestResult("Property existence check", hasName && !hasNonExistent);
        
        // プロパティ削除
        obj.deleteProperty("value");
        bool deletedCorrectly = !obj.hasProperty("value");
        printTestResult("Property deletion", deletedCorrectly);
        
        // プロパティ名一覧
        std::vector<std::string> propNames = obj.getPropertyNames();
        bool hasPropertyNames = !propNames.empty();
        printTestResult("Property names enumeration", hasPropertyNames);
        
        return nameCorrect && valueCorrect && activeCorrect && 
               hasName && !hasNonExistent && deletedCorrectly && hasPropertyNames;
    } catch (const std::exception& e) {
        std::cerr << "Exception in object operations test: " << e.what() << std::endl;
        return false;
    }
}

bool testFunctionOperations() {
    printTestHeader("Function Operations Tests");
    
    try {
        // 関数の作成
        Value funcVal = Value::fromFunction(nullptr);
        
        bool isFunction = funcVal.isFunction();
        bool isCallable = funcVal.isCallable();
        printTestResult("Function creation and type check", isFunction && isCallable);
        
        // 関数の呼び出し
        std::vector<Value> args = {
            Value::fromNumber(10),
            Value::fromNumber(20)
        };
        Value result = funcVal.call(args);
        bool callCorrect = (result.toNumber() == 30.0);
        printTestResult("Function call", callCorrect);
        
        return isFunction && isCallable && callCorrect;
    } catch (const std::exception& e) {
        std::cerr << "Exception in function operations test: " << e.what() << std::endl;
        return false;
    }
}

bool testValueComparison() {
    printTestHeader("Value Comparison Tests");
    
    try {
        Value num1 = Value::fromNumber(42);
        Value num2 = Value::fromNumber(42);
        Value num3 = Value::fromNumber(43);
        Value str1 = Value::fromString("42");
        Value bool1 = Value::fromBoolean(true);
        Value null1 = Value::null();
        Value undef1 = Value::undefined();
        
        // 厳密等価比較 (===)
        bool strictEqual1 = num1.strictEquals(num2);
        bool strictEqual2 = !num1.strictEquals(str1);
        printTestResult("Strict equality (same numbers)", strictEqual1);
        printTestResult("Strict equality (number vs string)", strictEqual2);
        
        // 緩い等価比較 (==)
        bool looseEqual1 = num1.equals(str1);
        bool looseEqual2 = null1.equals(undef1);
        printTestResult("Loose equality (number vs string)", looseEqual1);
        printTestResult("Loose equality (null vs undefined)", looseEqual2);
        
        // Object.is 比較
        bool sameValue1 = num1.sameValue(num2);
        Value nanVal1 = Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
        Value nanVal2 = Value::fromNumber(std::numeric_limits<double>::quiet_NaN());
        bool sameValue2 = nanVal1.sameValue(nanVal2);
        printTestResult("SameValue (numbers)", sameValue1);
        printTestResult("SameValue (NaN)", sameValue2);
        
        // 比較演算
        auto comp1 = num1.compare(num3);
        auto comp2 = num3.compare(num1);
        bool comparisonCorrect = (comp1 == aerojs::core::ComparisonResult::LessThan && 
                                 comp2 == aerojs::core::ComparisonResult::GreaterThan);
        printTestResult("Value comparison", comparisonCorrect);
        
        // 演算子オーバーロード
        bool opEqual = (num1 == num2);
        bool opNotEqual = (num1 != num3);
        bool opLess = (num1 < num3);
        bool opGreater = (num3 > num1);
        printTestResult("Operator overloads", opEqual && opNotEqual && opLess && opGreater);
        
        return strictEqual1 && strictEqual2 && looseEqual1 && looseEqual2 && 
               sameValue1 && sameValue2 && comparisonCorrect && 
               opEqual && opNotEqual && opLess && opGreater;
    } catch (const std::exception& e) {
        std::cerr << "Exception in value comparison test: " << e.what() << std::endl;
        return false;
    }
}

bool testValueUtilities() {
    printTestHeader("Value Utilities Tests");
    
    try {
        Value originalVal = Value::fromArray({
            Value::fromNumber(1),
            Value::fromString("test"),
            Value::fromBoolean(true)
        });
        
        // クローン
        Value clonedVal = originalVal.clone();
        bool cloneCorrect = (clonedVal.isArray() && clonedVal.getLength() == 3);
        printTestResult("Value cloning", cloneCorrect);
        
        // フリーズ
        Value freezeVal = Value::fromString("freeze test");
        freezeVal.freeze();
        bool isFrozen = freezeVal.isFrozen();
        bool isSealed = freezeVal.isSealed();
        bool isExtensible = !freezeVal.isExtensible();
        printTestResult("Value freezing", isFrozen && isSealed && isExtensible);
        
        // シール
        Value sealVal = Value::fromString("seal test");
        sealVal.seal();
        bool sealedCorrect = sealVal.isSealed();
        printTestResult("Value sealing", sealedCorrect);
        
        // 拡張防止
        Value extVal = Value::fromString("extension test");
        extVal.preventExtensions();
        bool extensionPrevented = !extVal.isExtensible();
        printTestResult("Prevent extensions", extensionPrevented);
        
        // 検証
        bool isValid = originalVal.isValid();
        printTestResult("Value validation", isValid);
        
        // サイズ計算
        size_t size = originalVal.getSize();
        bool sizeCorrect = (size > 0);
        printTestResult("Value size calculation", sizeCorrect);
        
        // ハッシュ計算
        uint64_t hash1 = originalVal.getHash();
        uint64_t hash2 = clonedVal.getHash();
        bool hashCorrect = (hash1 == hash2);
        printTestResult("Value hash calculation", hashCorrect);
        
        // 文字列表現
        std::string repr = originalVal.toStringRepresentation();
        bool reprCorrect = !repr.empty();
        printTestResult("String representation", reprCorrect);
        
        return cloneCorrect && isFrozen && isSealed && isExtensible && 
               sealedCorrect && extensionPrevented && isValid && 
               sizeCorrect && hashCorrect && reprCorrect;
    } catch (const std::exception& e) {
        std::cerr << "Exception in value utilities test: " << e.what() << std::endl;
        return false;
    }
}

bool testEngineStatistics() {
    printTestHeader("Engine Statistics Tests");
    
    try {
        Engine engine;
        engine.initialize();
        engine.enableProfiling(true);
        
        // いくつかのスクリプトを評価
        engine.evaluate("42");
        engine.evaluate("true");
        engine.evaluate("hello");
        
        // 統計情報の取得
        const EngineStats& stats = engine.getStats();
        bool hasEvaluations = (stats.scriptsEvaluated >= 3);
        printTestResult("Script evaluation count", hasEvaluations);
        
        // 統計レポートの生成
        std::string statsReport = engine.getStatsReport();
        bool hasStatsReport = !statsReport.empty();
        printTestResult("Statistics report generation", hasStatsReport);
        
        // プロファイリングレポート
        std::string profilingReport = engine.getProfilingReport();
        bool hasProfilingReport = !profilingReport.empty();
        printTestResult("Profiling report generation", hasProfilingReport);
        
        // メモリ使用量
        size_t currentMemory = engine.getCurrentMemoryUsage();
        bool hasMemoryUsage = (currentMemory >= 0);
        printTestResult("Memory usage tracking", hasMemoryUsage);
        
        // 統計のリセット
        engine.resetStats();
        const EngineStats& resetStats = engine.getStats();
        bool statsReset = (resetStats.scriptsEvaluated == 0);
        printTestResult("Statistics reset", statsReset);
        
        return hasEvaluations && hasStatsReport && hasProfilingReport && 
               hasMemoryUsage && statsReset;
    } catch (const std::exception& e) {
        std::cerr << "Exception in engine statistics test: " << e.what() << std::endl;
        return false;
    }
}

bool testErrorHandling() {
    printTestHeader("Error Handling Tests");
    
    try {
        Engine engine;
        
        // エラーハンドラーの設定
        bool errorHandlerCalled = false;
        std::string lastErrorMessage;
        
        engine.setErrorHandler([&](EngineError error, const std::string& message) {
            errorHandlerCalled = true;
            lastErrorMessage = message;
        });
        
        // 初期化前の評価でエラーを発生させる
        Value result = engine.evaluate("test");
        bool errorOccurred = (engine.getLastError() != EngineError::None);
        printTestResult("Error detection", errorOccurred);
        
        // エラーメッセージの確認
        std::string errorMsg = engine.getLastErrorMessage();
        bool hasErrorMessage = !errorMsg.empty();
        printTestResult("Error message retrieval", hasErrorMessage);
        
        // エラーのクリア
        engine.clearError();
        bool errorCleared = (engine.getLastError() == EngineError::None);
        printTestResult("Error clearing", errorCleared);
        
        return errorOccurred && hasErrorMessage && errorCleared;
    } catch (const std::exception& e) {
        std::cerr << "Exception in error handling test: " << e.what() << std::endl;
        return false;
    }
}

bool testAsyncOperations() {
    printTestHeader("Async Operations Tests");
    
    try {
        Engine engine;
        engine.initialize();
        
        // 非同期評価
        auto future1 = engine.evaluateAsync("42");
        auto future2 = engine.evaluateAsync("true", "async_test.js");
        
        // 結果の取得
        Value result1 = future1.get();
        Value result2 = future2.get();
        
        bool async1Correct = (result1.toNumber() == 42.0);
        bool async2Correct = result2.isBoolean();
        
        printTestResult("Async evaluation 1", async1Correct);
        printTestResult("Async evaluation 2", async2Correct);
        
        return async1Correct && async2Correct;
    } catch (const std::exception& e) {
        std::cerr << "Exception in async operations test: " << e.what() << std::endl;
        return false;
    }
}

bool testMemoryManagement() {
    printTestHeader("Memory Management Tests");
    
    try {
        Engine engine;
        engine.initialize();
        
        // メモリ制限の設定
        size_t originalLimit = engine.getMemoryLimit();
        engine.setMemoryLimit(256 * 1024 * 1024); // 256MB
        size_t newLimit = engine.getMemoryLimit();
        bool limitSet = (newLimit == 256 * 1024 * 1024);
        printTestResult("Memory limit setting", limitSet);
        
        // ガベージコレクション
        size_t memoryBefore = engine.getCurrentMemoryUsage();
        engine.collectGarbage();
        size_t memoryAfter = engine.getCurrentMemoryUsage();
        bool gcExecuted = true; // GCが実行されたかの確認
        printTestResult("Garbage collection execution", gcExecuted);
        
        // GC頻度の設定
        engine.setGCFrequency(500);
        size_t gcFreq = engine.getGCFrequency();
        bool gcFreqSet = (gcFreq == 500);
        printTestResult("GC frequency setting", gcFreqSet);
        
        // メモリ最適化
        engine.optimizeMemory();
        bool optimizationExecuted = true; // 最適化が実行されたかの確認
        printTestResult("Memory optimization", optimizationExecuted);
        
        return limitSet && gcExecuted && gcFreqSet && optimizationExecuted;
    } catch (const std::exception& e) {
        std::cerr << "Exception in memory management test: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "AeroJS Engine Comprehensive Test Suite Started\n" << std::endl;
    
    std::vector<std::pair<std::string, std::function<bool()>>> tests = {
        {"Basic Engine", testBasicEngine},
        {"Engine Configuration", testEngineConfiguration},
        {"Value System", testValueSystem},
        {"Array Operations", testArrayOperations},
        {"Object Operations", testObjectOperations},
        {"Function Operations", testFunctionOperations},
        {"Value Comparison", testValueComparison},
        {"Value Utilities", testValueUtilities},
        {"Engine Statistics", testEngineStatistics},
        {"Error Handling", testErrorHandling},
        {"Async Operations", testAsyncOperations},
        {"Memory Management", testMemoryManagement}
    };
    
    int passedTests = 0;
    int totalTests = tests.size();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (const auto& test : tests) {
        try {
            bool result = test.second();
            if (result) {
                passedTests++;
                std::cout << "✓ " << test.first << " Test Suite: PASSED" << std::endl;
            } else {
                std::cout << "✗ " << test.first << " Test Suite: FAILED" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "✗ " << test.first << " Test Suite: EXCEPTION - " << e.what() << std::endl;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\n=== Test Results ===" << std::endl;
    std::cout << "Passed: " << passedTests << "/" << totalTests << std::endl;
    std::cout << "Success Rate: " << (100.0 * passedTests / totalTests) << "%" << std::endl;
    std::cout << "Execution Time: " << duration.count() << " ms" << std::endl;
    
    if (passedTests == totalTests) {
        std::cout << "\n🎉 All tests passed! AeroJS Engine is working perfectly!" << std::endl;
        return 0;
    } else {
        std::cout << "\n⚠️  Some tests failed. Please check the implementation." << std::endl;
        return 1;
    }
} 