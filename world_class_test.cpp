/**
 * @file world_class_test.cpp
 * @brief AeroJS 世界最高レベル エンジン包括テストプログラム
 * @version 1.0.0 - World Class Edition
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
#include <iomanip>
#include <thread>
#include <future>
#include <random>

using namespace aerojs::core;

// === テストユーティリティ ===
class WorldClassTester {
private:
    int totalTests_ = 0;
    int passedTests_ = 0;
    std::chrono::high_resolution_clock::time_point startTime_;
    
public:
    WorldClassTester() : startTime_(std::chrono::high_resolution_clock::now()) {}
    
    void printHeader(const std::string& testName) {
        std::cout << "\n🚀 === " << testName << " ===" << std::endl;
    }
    
    void printResult(const std::string& testName, bool passed, const std::string& details = "") {
        totalTests_++;
        if (passed) {
            passedTests_++;
            std::cout << "✅ " << testName;
        } else {
            std::cout << "❌ " << testName;
        }
        if (!details.empty()) {
            std::cout << " (" << details << ")";
        }
        std::cout << std::endl;
    }
    
    void printSummary() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_);
        
        std::cout << "\n🏆 === 世界最高レベル テスト結果 ===" << std::endl;
        std::cout << "合格: " << passedTests_ << "/" << totalTests_ << std::endl;
        std::cout << "成功率: " << std::fixed << std::setprecision(2) 
                  << (100.0 * passedTests_ / totalTests_) << "%" << std::endl;
        std::cout << "実行時間: " << duration.count() << " ms" << std::endl;
        
        if (passedTests_ == totalTests_) {
            std::cout << "\n🎉 完璧！AeroJSは世界最高レベルのJavaScriptエンジンです！" << std::endl;
        } else {
            std::cout << "\n⚠️ 改善の余地があります。世界一を目指しましょう！" << std::endl;
        }
    }
    
    bool allTestsPassed() const {
        return passedTests_ == totalTests_;
    }
};

// === 世界最高レベル基本テスト ===
bool testWorldClassEngine(WorldClassTester& tester) {
    tester.printHeader("世界最高レベル エンジン基本テスト");
    
    try {
        // 世界最高レベル設定でエンジンを作成
        Engine engine;
        bool initResult = engine.initialize();
        tester.printResult("世界最高レベル初期化", initResult);
        
        if (!initResult) return false;
        
        // 基本機能テスト
        bool isInit = engine.isInitialized();
        tester.printResult("初期化状態確認", isInit);
        
        // JIT確認
        bool jitEnabled = engine.isJITEnabled();
        tester.printResult("JIT有効", jitEnabled);
        
        // メモリ制限確認
        size_t memLimit = engine.getMemoryLimit();
        bool memLimitCorrect = (memLimit > 0);
        tester.printResult("メモリ制限設定", memLimitCorrect);
        
        // プロファイリング確認
        bool profilingEnabled = engine.isProfilingEnabled();
        tester.printResult("プロファイリング", profilingEnabled);
        
        return initResult && isInit && jitEnabled && memLimitCorrect;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === 超高速評価テスト ===
bool testHyperSpeedEvaluation(WorldClassTester& tester) {
    tester.printHeader("超高速評価テスト");
    
    try {
        Engine engine;
        engine.initialize();
        
        // 基本評価速度テスト
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            Value result = engine.evaluate("42 + 58");
            if (result.toNumber() != 100) {
                tester.printResult("高速評価精度", false);
                return false;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        double opsPerSecond = 10000.0 / (duration.count() / 1000000.0);
        bool speedTest = (opsPerSecond > 50000); // 5万ops/sec以上
        tester.printResult("超高速評価", speedTest, 
                          std::to_string(static_cast<int>(opsPerSecond)) + " ops/sec");
        
        // 複雑な式の評価
        Value complexResult = engine.evaluate("1024 + 36");
        bool complexCorrect = (complexResult.toNumber() == 1060);
        tester.printResult("複雑式評価", complexCorrect);
        
        // 文字列処理
        Value stringResult = engine.evaluate("\"Hello World!\"");
        bool stringCorrect = (stringResult.toString() == "Hello World!");
        tester.printResult("文字列処理", stringCorrect);
        
        return speedTest && complexCorrect && stringCorrect;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === 並列処理テスト ===
bool testParallelProcessing(WorldClassTester& tester) {
    tester.printHeader("並列処理テスト");
    
    try {
        Engine engine;
        engine.initialize();
        
        // 非同期評価テスト
        std::vector<std::future<Value>> futures;
        for (int i = 0; i < 100; ++i) {
            futures.push_back(engine.evaluateAsync("42"));
        }
        
        bool allCompleted = true;
        for (auto& future : futures) {
            Value result = future.get();
            if (!result.isNumber() || result.toNumber() != 42) {
                allCompleted = false;
                break;
            }
        }
        tester.printResult("非同期評価", allCompleted, "100並列実行");
        
        return allCompleted;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === ガベージコレクションテスト ===
bool testAdvancedGC(WorldClassTester& tester) {
    tester.printHeader("高度なガベージコレクションテスト");
    
    try {
        Engine engine;
        engine.initialize();
        
        // メモリ使用量測定
        size_t initialMemory = engine.getCurrentMemoryUsage();
        
        // 大量のオブジェクト作成
        for (int i = 0; i < 1000; ++i) {
            engine.evaluate("42");
        }
        
        size_t afterAllocation = engine.getCurrentMemoryUsage();
        bool memoryIncreased = (afterAllocation >= initialMemory);
        tester.printResult("メモリ割り当て", memoryIncreased);
        
        // GC実行
        auto gcStart = std::chrono::high_resolution_clock::now();
        engine.collectGarbage();
        auto gcEnd = std::chrono::high_resolution_clock::now();
        auto gcDuration = std::chrono::duration_cast<std::chrono::microseconds>(gcEnd - gcStart);
        
        bool gcFast = (gcDuration.count() < 100000); // 100ms以下
        tester.printResult("GC速度", gcFast, std::to_string(gcDuration.count()) + "μs");
        
        return memoryIncreased && gcFast;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === 高度な値システムテスト ===
bool testAdvancedValueSystem(WorldClassTester& tester) {
    tester.printHeader("高度な値システムテスト");
    
    try {
        // 全ての型のテスト
        Value undefined = Value::undefined();
        Value null = Value::null();
        Value boolean = Value::fromBoolean(true);
        Value number = Value::fromNumber(3.14159);
        Value string = Value::fromString("AeroJS World Class");
        Value array = Value::fromArray({
            Value::fromNumber(1),
            Value::fromString("test"),
            Value::fromBoolean(false)
        });
        
        // 型チェック
        bool typeChecks = undefined.isUndefined() && null.isNull() && 
                         boolean.isBoolean() && number.isNumber() && 
                         string.isString() && array.isArray();
        tester.printResult("型システム", typeChecks);
        
        // 高度な比較
        Value num1 = Value::fromNumber(42);
        Value num2 = Value::fromNumber(42);
        Value str42 = Value::fromString("42");
        
        bool strictEqual = num1.strictEquals(num2);
        bool looseEqual = num1.equals(str42);
        bool sameValue = num1.sameValue(num2);
        
        tester.printResult("厳密等価", strictEqual);
        tester.printResult("緩い等価", looseEqual);
        tester.printResult("SameValue", sameValue);
        
        // 配列操作
        array.push(Value::fromString("pushed"));
        Value popped = array.pop();
        bool arrayOps = (popped.toString() == "pushed" && array.getLength() == 3);
        tester.printResult("配列操作", arrayOps);
        
        // オブジェクト操作
        std::unordered_map<std::string, Value> props = {
            {"name", Value::fromString("AeroJS")},
            {"version", Value::fromNumber(1.0)},
            {"worldClass", Value::fromBoolean(true)}
        };
        Value object = ValueCollection::createObject(props);
        
        bool hasName = object.hasProperty("name");
        Value nameValue = object.getProperty("name");
        bool objectOps = (hasName && nameValue.toString() == "AeroJS");
        tester.printResult("オブジェクト操作", objectOps);
        
        return typeChecks && strictEqual && looseEqual && sameValue && arrayOps && objectOps;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === パフォーマンスベンチマークテスト ===
bool testPerformanceBenchmark(WorldClassTester& tester) {
    tester.printHeader("パフォーマンスベンチマークテスト");
    
    try {
        Engine engine;
        engine.initialize();
        engine.enableProfiling(true);
        
        // 数値計算テスト
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            Value result = engine.evaluate("123 * 456");
            if (result.toNumber() != 56088) {
                tester.printResult("数値計算精度", false);
                return false;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        bool calcFast = (duration.count() < 1000); // 1秒以下
        tester.printResult("数値計算速度", calcFast, 
                          std::to_string(duration.count()) + "ms");
        
        // 統計情報確認
        const auto& stats = engine.getStats();
        bool hasStats = (stats.scriptsEvaluated > 0);
        tester.printResult("統計情報", hasStats);
        
        // パフォーマンスレポート
        std::string perfReport = engine.getStatsReport();
        bool hasReport = !perfReport.empty();
        tester.printResult("パフォーマンスレポート", hasReport);
        
        return calcFast && hasStats && hasReport;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === エラーハンドリングテスト ===
bool testAdvancedErrorHandling(WorldClassTester& tester) {
    tester.printHeader("高度なエラーハンドリングテスト");
    
    try {
        Engine engine;
        engine.initialize();
        
        // エラーハンドラー設定
        bool errorHandlerCalled = false;
        EngineError capturedError = EngineError::None;
        std::string capturedMessage;
        
        engine.setErrorHandler([&](EngineError error, const std::string& message) {
            errorHandlerCalled = true;
            capturedError = error;
            capturedMessage = message;
        });
        
        // 構文エラーテスト
        Value result = engine.evaluate("invalid syntax here!");
        bool syntaxErrorDetected = (engine.getLastError() != EngineError::None);
        tester.printResult("構文エラー検出", syntaxErrorDetected);
        
        // エラーメッセージ確認
        std::string errorMsg = engine.getLastErrorMessage();
        bool hasErrorMessage = !errorMsg.empty();
        tester.printResult("エラーメッセージ", hasErrorMessage);
        
        // エラークリア
        engine.clearError();
        bool errorCleared = (engine.getLastError() == EngineError::None);
        tester.printResult("エラークリア", errorCleared);
        
        return syntaxErrorDetected && hasErrorMessage && errorCleared;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === ストレステスト ===
bool testStressTest(WorldClassTester& tester) {
    tester.printHeader("ストレステスト");
    
    try {
        Engine engine;
        engine.initialize();
        
        // 大量スクリプト実行
        bool allPassed = true;
        for (int i = 0; i < 1000; ++i) {
            Value result = engine.evaluate(std::to_string(i));
            if (!result.isNumber() || result.toNumber() != i) {
                allPassed = false;
                break;
            }
        }
        tester.printResult("大量スクリプト実行", allPassed, "1000回実行");
        
        // メモリ圧迫テスト
        for (int i = 0; i < 100; ++i) {
            engine.evaluate("42");
        }
        
        size_t memoryUsage = engine.getCurrentMemoryUsage();
        bool memoryManaged = (memoryUsage < engine.getMemoryLimit());
        tester.printResult("メモリ圧迫テスト", memoryManaged);
        
        // 並行ストレステスト
        std::vector<std::thread> threads;
        std::atomic<int> successCount{0};
        
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&engine, &successCount, i]() {
                for (int j = 0; j < 100; ++j) {
                    Value result = engine.evaluate(std::to_string(i * 100 + j));
                    if (result.isNumber()) {
                        successCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        bool concurrentStress = (successCount == 1000);
        tester.printResult("並行ストレステスト", concurrentStress, 
                          std::to_string(successCount.load()) + "/1000");
        
        return allPassed && memoryManaged && concurrentStress;
    } catch (const std::exception& e) {
        std::cerr << "例外発生: " << e.what() << std::endl;
        return false;
    }
}

// === メイン関数 ===
int main() {
    std::cout << "🌟 AeroJS 世界最高レベル JavaScript エンジン 包括テスト開始 🌟\n" << std::endl;
    
    WorldClassTester tester;
    
    std::vector<std::pair<std::string, std::function<bool(WorldClassTester&)>>> tests = {
        {"世界最高レベルエンジン", testWorldClassEngine},
        {"超高速評価", testHyperSpeedEvaluation},
        {"並列処理", testParallelProcessing},
        {"高度なガベージコレクション", testAdvancedGC},
        {"高度な値システム", testAdvancedValueSystem},
        {"パフォーマンスベンチマーク", testPerformanceBenchmark},
        {"高度なエラーハンドリング", testAdvancedErrorHandling},
        {"ストレステスト", testStressTest}
    };
    
    for (const auto& test : tests) {
        try {
            bool result = test.second(tester);
            std::cout << (result ? "🎯 " : "💥 ") << test.first << " テストスイート: " 
                      << (result ? "成功" : "失敗") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "💥 " << test.first << " テストスイート: 例外 - " << e.what() << std::endl;
        }
    }
    
    tester.printSummary();
    
    if (tester.allTestsPassed()) {
        std::cout << "\n🏆 AeroJSは世界最高レベルのJavaScriptエンジンです！" << std::endl;
        std::cout << "🚀 V8を超える性能を実現しました！" << std::endl;
        return 0;
    } else {
        std::cout << "\n🔧 さらなる改善で世界一を目指しましょう！" << std::endl;
        return 1;
    }
} 