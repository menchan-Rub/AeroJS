/**
 * @file test_main.cpp
 * @brief AeroJS Basic Test Program
 * @version 3.0.0
 * @license MIT
 */

#include "src/core/engine.h"
#include "src/core/value.h"
#include "src/core/context.h"
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>

using namespace aerojs::core;

// === Basic Test Utility ===
class BasicTester {
private:
    int totalTests_ = 0;
    int passedTests_ = 0;
    std::chrono::high_resolution_clock::time_point startTime_;
    
public:
    BasicTester() : startTime_(std::chrono::high_resolution_clock::now()) {}
    
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
        }
    }
    
    void printSummary() {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime_);
        
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passedTests_ << "/" << totalTests_ << std::endl;
        std::cout << "Success Rate: " << (100.0 * passedTests_ / totalTests_) << "%" << std::endl;
        std::cout << "Execution Time: " << duration.count() << " ms" << std::endl;
        
        if (passedTests_ == totalTests_) {
            std::cout << "\nAll tests passed! AeroJS is working correctly!" << std::endl;
        } else {
            std::cout << "\nSome tests failed. Further improvements needed." << std::endl;
        }
    }
    
    bool allTestsPassed() const {
        return passedTests_ == totalTests_;
    }
};

// === Value System Test ===
bool testValueSystem(BasicTester& tester) {
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
        
        // Copy and move test
        Value copyVal = numVal;
        Value moveVal = std::move(Value::fromNumber(123));
        
        bool copyMove = (copyVal.isNumber() && moveVal.isNumber());
        tester.printResult("Copy and Move", copyMove);
        
        return basicCreation && typeConversion && comparison && copyMove;
    } catch (const std::exception& e) {
        std::cerr << "Value System Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Engine System Test ===
bool testEngineSystem(BasicTester& tester) {
    tester.printHeader("Engine System Test");
    
    try {
        // Engine initialization test
        Engine engine;
        bool initResult = engine.initialize();
        tester.printResult("Engine Initialization", initResult);
        
        if (!initResult) return false;
        
        // Basic evaluation test
        Value result1 = engine.evaluate("42");
        bool basicEval = (result1.isNumber() && result1.toNumber() == 42);
        tester.printResult("Basic Evaluation", basicEval);
        
        // String evaluation test
        Value result2 = engine.evaluate("\"Hello\"");
        bool stringEval = (result2.isString() && result2.toString() == "Hello");
        tester.printResult("String Evaluation", stringEval);
        
        // Boolean evaluation test
        Value result3 = engine.evaluate("true");
        bool boolEval = (result3.isBoolean() && result3.toBoolean() == true);
        tester.printResult("Boolean Evaluation", boolEval);
        
        engine.shutdown();
        return initResult && basicEval && stringEval && boolEval;
    } catch (const std::exception& e) {
        std::cerr << "Engine System Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Performance Test ===
bool testPerformance(BasicTester& tester) {
    tester.printHeader("Performance Test");
    
    try {
        Engine engine;
        engine.initialize();
        
        // Speed test
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            Value result = engine.evaluate("42");
            if (!result.isNumber()) {
                tester.printResult("Speed Test", false);
                return false;
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        bool speedTest = (duration.count() < 5000); // Less than 5 seconds
        tester.printResult("Speed Test (1k ops)", speedTest);
        
        // Memory test
        std::vector<Value> values;
        for (int i = 0; i < 100; ++i) {
            values.push_back(Value::fromNumber(i));
        }
        
        bool memoryTest = (values.size() == 100);
        tester.printResult("Memory Test", memoryTest);
        
        engine.shutdown();
        return speedTest && memoryTest;
    } catch (const std::exception& e) {
        std::cerr << "Performance Test Exception: " << e.what() << std::endl;
        return false;
    }
}

// === Main Function ===
int main() {
    std::cout << "AeroJS Basic Test Program\n" << std::endl;
    std::cout << "Testing core functionality of AeroJS JavaScript engine\n" << std::endl;
    
    BasicTester tester;
    
    std::vector<std::pair<std::string, std::function<bool(BasicTester&)>>> tests = {
        {"Value System", testValueSystem},
        {"Engine System", testEngineSystem},
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
    
    if (tester.allTestsPassed() && passedSuites == static_cast<int>(tests.size())) {
        std::cout << "\nSuccess! AeroJS core functionality is working correctly!" << std::endl;
        return 0;
    } else {
        std::cout << "\nSome issues found. Continue development to improve." << std::endl;
        return 1;
    }
} 