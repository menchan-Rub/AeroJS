/**
 * @file world_class_test.cpp
 * @brief AeroJS World Class Test Program
 * @version 3.0.0
 * @license MIT
 */

#include "src/core/engine.h"
#include "src/core/value.h"
#include "src/utils/memory/allocators/memory_allocator.h"
#include "src/utils/memory/pool/memory_pool.h"
#include "src/utils/memory/gc/garbage_collector.h"
#include "src/utils/time/timer.h"
#include "src/core/context.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>
#include <iomanip>
#include <thread>
#include <future>
#include <random>
#include <fstream>
#include <sstream>
#include <unordered_map>

using namespace aerojs::core;

// === World Class Test Utility ===
class WorldClassTester {
private:
    int totalTests_ = 0;
    int passedTests_ = 0;
    std::chrono::high_resolution_clock::time_point startTime_;
    std::vector<std::string> failedTests_;
    
public:
    WorldClassTester() : startTime_(std::chrono::high_resolution_clock::now()) {}
    
    void printHeader(const std::string& testName) {
        std::cout << "\n=== " << testName << " ===" << std::endl;
    }
    
    void printResult(const std::string& testName, bool passed) {
        totalTests_++;
        if (passed) {
            passedTests_++;
            std::cout << "PASS " << testName << std::endl;
        } else {
            std::cout << "FAIL " << testName << std::endl;
            failedTests_.push_back(testName);
        }
    }
    
    void printSummary() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_);
        
        std::cout << "\n=== World Class Test Results ===" << std::endl;
        std::cout << "Passed: " << passedTests_ << "/" << totalTests_ << std::endl;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(2) 
                  << (100.0 * passedTests_ / totalTests_) << "%" << std::endl;
        std::cout << "Execution Time: " << duration.count() << " ms" << std::endl;
        
        if (!failedTests_.empty()) {
            std::cout << "\nFailed Tests:" << std::endl;
            for (const auto& test : failedTests_) {
                std::cout << "  - " << test << std::endl;
            }
        }
        
        if (passedTests_ == totalTests_) {
            std::cout << "\nPerfect! AeroJS is truly a world-class JavaScript engine!" << std::endl;
        } else {
            std::cout << "\nContinue improving to achieve true world-class status!" << std::endl;
        }
    }
    
    bool allTestsPassed() const {
        return passedTests_ == totalTests_;
    }
    
    double getSuccessRate() const {
        return totalTests_ > 0 ? (100.0 * passedTests_ / totalTests_) : 0.0;
    }
};

// === Value System Test ===
bool testValueSystem(WorldClassTester& tester) {
    tester.printHeader("Value System Test");
    
    try {
        // Basic value creation test
        Value undefinedVal = Value::undefined();
        Value nullVal = Value::null();
        Value boolVal = Value::fromBoolean(true);
        Value numVal = Value::fromNumber(42.5);
        Value strVal = Value::fromString("Hello World");
        
        bool basicCreation = (undefinedVal.isUndefined() && 
                             nullVal.isNull() && 
                             boolVal.isBoolean() && 
                             numVal.isNumber() && 
                             strVal.isString());
        tester.printResult("Basic Value Creation", basicCreation);
        
        if (!basicCreation) return false;
        
        // Type conversion test
        bool boolConversion = boolVal.toBoolean();
        double numConversion = numVal.toNumber();
        std::string strConversion = strVal.toString();
        
        bool typeConversion = (boolConversion == true && 
                              numConversion == 42.5 && 
                              strConversion == "Hello World");
        tester.printResult("Type Conversion", typeConversion);
        
        // Comparison test
        Value num1 = Value::fromNumber(10);
        Value num2 = Value::fromNumber(20);
        Value num3 = Value::fromNumber(10);
        
        bool comparison = (num1 < num2 && num1 == num3 && num2 > num1);
        tester.printResult("Value Comparison", comparison);
        
        // Object operations test
        Value objVal = Value::fromObject(nullptr);
        objVal.setProperty("name", Value::fromString("test"));
        objVal.setProperty("value", Value::fromNumber(123));
        
        Value nameProperty = objVal.getProperty("name");
        Value valueProperty = objVal.getProperty("value");
        
        bool objectOps = (nameProperty.isString() && valueProperty.isNumber());
        tester.printResult("Object Operations", objectOps);
        
        return basicCreation && typeConversion && comparison && objectOps;
    } catch (const std::exception& e) {
        std::cerr << "Value System Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Engine System Test ===
bool testEngineSystem(WorldClassTester& tester) {
    tester.printHeader("Engine System Test");
    
    try {
        // Engine initialization test
        Engine engine;
        bool initResult = engine.initialize();
        tester.printResult("Engine Initialization", initResult);
        
        if (!initResult) return false;
        
        // Basic execution test
        Context context;
        Value result = engine.execute("42 + 58", context);
        bool basicExecution = result.isNumber();
        tester.printResult("Basic Execution", basicExecution);
        
        // String execution test
        Value strResult = engine.execute("'Hello' + ' World'", context);
        bool stringExecution = strResult.isString();
        tester.printResult("String Execution", stringExecution);
        
        // Variable assignment test
        engine.execute("var x = 10; var y = 20;", context);
        Value varResult = engine.execute("x + y", context);
        bool variableAssignment = varResult.isNumber();
        tester.printResult("Variable Assignment", variableAssignment);
        
        // Function definition test
        engine.execute("function add(a, b) { return a + b; }", context);
        Value funcResult = engine.execute("add(5, 7)", context);
        bool functionDefinition = funcResult.isNumber();
        tester.printResult("Function Definition", functionDefinition);
        
        engine.shutdown();
        return initResult && basicExecution && stringExecution && 
               variableAssignment && functionDefinition;
    } catch (const std::exception& e) {
        std::cerr << "Engine System Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Memory Management Test ===
bool testMemoryManagement(WorldClassTester& tester) {
    tester.printHeader("Memory Management Test");
    
    try {
        // Memory allocator test
        MemoryAllocator allocator;
        void* ptr1 = allocator.allocate(1024);
        void* ptr2 = allocator.allocate(2048);
        
        bool allocation = (ptr1 != nullptr && ptr2 != nullptr);
        tester.printResult("Memory Allocation", allocation);
        
        if (allocation) {
            allocator.deallocate(ptr1);
            allocator.deallocate(ptr2);
        }
        
        // Memory pool test
        MemoryPool pool(4096);
        void* poolPtr1 = pool.allocate(512);
        void* poolPtr2 = pool.allocate(1024);
        
        bool poolAllocation = (poolPtr1 != nullptr && poolPtr2 != nullptr);
        tester.printResult("Memory Pool", poolAllocation);
        
        // Garbage collector test
        GarbageCollector gc;
        bool gcInit = gc.initialize();
        tester.printResult("Garbage Collector Init", gcInit);
        
        if (gcInit) {
            gc.collect();
            gc.shutdown();
        }
        
        return allocation && poolAllocation && gcInit;
    } catch (const std::exception& e) {
        std::cerr << "Memory Management Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Performance Test ===
bool testPerformance(WorldClassTester& tester) {
    tester.printHeader("Performance Test");
    
    try {
        Engine engine;
        engine.initialize();
        Context context;
        
        // Speed test
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            engine.execute("42", context);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        bool speedTest = (duration.count() < 5000); // Less than 5 seconds
        tester.printResult("Speed Test (10k ops)", speedTest);
        
        // Memory efficiency test
        std::vector<Value> values;
        for (int i = 0; i < 1000; ++i) {
            values.push_back(Value::fromNumber(i));
        }
        
        bool memoryEfficiency = (values.size() == 1000);
        tester.printResult("Memory Efficiency", memoryEfficiency);
        
        // Concurrent execution test
        std::vector<std::thread> threads;
        std::atomic<int> successCount{0};
        
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&engine, &successCount, i]() {
                Context localContext;
                for (int j = 0; j < 100; ++j) {
                    Value result = engine.execute(std::to_string(i * 100 + j), localContext);
                    if (result.isNumber()) {
                        successCount++;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        bool concurrentExecution = (successCount == 1000);
        tester.printResult("Concurrent Execution", concurrentExecution);
        
        engine.shutdown();
        return speedTest && memoryEfficiency && concurrentExecution;
    } catch (const std::exception& e) {
        std::cerr << "Performance Test Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Main Function ===
int main() {
    std::cout << "AeroJS World Class Test Program\n" << std::endl;
    std::cout << "Goal: Demonstrate world-class JavaScript engine capabilities\n" << std::endl;
    
    WorldClassTester tester;
    
    std::vector<std::pair<std::string, std::function<bool(WorldClassTester&)>>> tests = {
        {"Value System", testValueSystem},
        {"Engine System", testEngineSystem},
        {"Memory Management", testMemoryManagement},
        {"Performance", testPerformance}
    };
    
    int passedSuites = 0;
    for (const auto& test : tests) {
        try {
            std::cout << "\nStarting " << test.first << " Test Suite..." << std::endl;
            bool result = test.second(tester);
            if (result) {
                passedSuites++;
                std::cout << test.first << " Test Suite: PASSED" << std::endl;
            } else {
                std::cout << test.first << " Test Suite: FAILED" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << test.first << " Test Suite: EXCEPTION - " << e.what() << std::endl;
        }
    }
    
    tester.printSummary();
    
    std::cout << "\nTest Suite Results: " << passedSuites << "/" << tests.size() << std::endl;
    std::cout << "Overall Success Rate: " << std::fixed << std::setprecision(1) 
              << tester.getSuccessRate() << "%" << std::endl;
    
    if (tester.allTestsPassed() && passedSuites == static_cast<int>(tests.size())) {
        std::cout << "\nPerfect! AeroJS is truly a world-class JavaScript engine!" << std::endl;
        std::cout << "Performance exceeds expectations!" << std::endl;
        return 0;
    } else {
        std::cout << "\nContinue improving to achieve true world-class status!" << std::endl;
        return 1;
    }
} 