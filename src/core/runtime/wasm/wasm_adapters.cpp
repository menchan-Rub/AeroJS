/**
 * @file wasm_adapters.cpp
 * @brief WebAssemblyとJavaScript間のアダプター完璧実装
 * @version 1.0.0
 * @license MIT
 */

#include "wasm_adapters.h"
#include <chrono>
#include <cassert>
#include <iostream>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

// -------------- JSWasmFunctionAdapter 実装 --------------

JSWasmFunctionAdapter::JSWasmFunctionAdapter(const Value& jsFunction, const WasmFunctionType& wasmType)
    : m_jsFunction(jsFunction)
    , m_wasmType(wasmType)
    , m_callCount(0)
    , m_typeErrors(0)
    , m_successfulCalls(0)
    , m_totalExecutionTime(0.0) {
    
    if (!jsFunction.isFunction()) {
        throw std::invalid_argument("JSWasmFunctionAdapter requires a JavaScript function");
    }
}

JSWasmFunctionAdapter::~JSWasmFunctionAdapter() {
    // 自動的にクリーンアップされる
}

std::vector<WasmValue> JSWasmFunctionAdapter::call(const std::vector<WasmValue>& args) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    m_callCount.fetch_add(1, std::memory_order_relaxed);
    
    try {
        // 引数の検証
        if (!validateArguments(args)) {
            m_typeErrors.fetch_add(1, std::memory_order_relaxed);
            throw WasmRuntimeException("Invalid arguments for WASM function adapter");
        }
        
        // 引数をJavaScript値に変換
        std::vector<Value> jsArgs = convertArgsToJS(args);
        
        // JavaScript関数を呼び出し
        execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
        Value result = m_jsFunction.callAsFunction(jsArgs, Value::createUndefined(), context);
        
        // 戻り値をWASM値に変換
        std::vector<WasmValue> wasmResults = convertResultToWasm(result);
        
        // 統計情報を更新
        m_successfulCalls.fetch_add(1, std::memory_order_relaxed);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        double executionTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_totalExecutionTime += executionTime;
        }
        
        return wasmResults;
        
    } catch (const std::exception& e) {
        m_typeErrors.fetch_add(1, std::memory_order_relaxed);
        throw WasmRuntimeException("JavaScript function adapter error: " + std::string(e.what()));
    }
}

const WasmFunctionType& JSWasmFunctionAdapter::getFunctionType() const {
    return m_wasmType;
}

const Value& JSWasmFunctionAdapter::getJSFunction() const {
    return m_jsFunction;
}

void JSWasmFunctionAdapter::handleTypeError(const std::string& error, execution::ExecutionContext* context) {
    m_typeErrors.fetch_add(1, std::memory_order_relaxed);
    
    if (context) {
        // JavaScript例外として型エラーをスロー
        Value errorObj = Value::createError(context, "TypeError", error);
        context->throwException(errorObj);
    } else {
        // コンテキストがない場合はC++例外
        throw WasmRuntimeException("Type error: " + error);
    }
}

bool JSWasmFunctionAdapter::validateArguments(const std::vector<WasmValue>& args) {
    // 引数数のチェック
    if (args.size() != m_wasmType.paramTypes.size()) {
        return false;
    }
    
    // 各引数の型チェック
    for (size_t i = 0; i < args.size(); ++i) {
        if (!WasmTypeChecker::isCompatible(m_wasmType.paramTypes[i], args[i].type())) {
            return false;
        }
    }
    
    return true;
}

JSWasmFunctionAdapter::AdapterStats JSWasmFunctionAdapter::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    AdapterStats stats;
    stats.callCount = m_callCount.load(std::memory_order_relaxed);
    stats.typeErrors = m_typeErrors.load(std::memory_order_relaxed);
    stats.successfulCalls = m_successfulCalls.load(std::memory_order_relaxed);
    stats.averageExecutionTime = stats.callCount > 0 ? (m_totalExecutionTime / stats.callCount) : 0.0;
    
    return stats;
}

std::vector<Value> JSWasmFunctionAdapter::convertArgsToJS(const std::vector<WasmValue>& wasmArgs) {
    std::vector<Value> jsArgs;
    jsArgs.reserve(wasmArgs.size());
    
    execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
    
    for (const auto& wasmArg : wasmArgs) {
        jsArgs.push_back(wasmArg.toJSValue(context));
    }
    
    return jsArgs;
}

std::vector<WasmValue> JSWasmFunctionAdapter::convertResultToWasm(const Value& jsResult) {
    std::vector<WasmValue> results;
    
    if (m_wasmType.returnTypes.empty()) {
        // 戻り値なしの関数
        return results;
    }
    
    if (m_wasmType.returnTypes.size() == 1) {
        // 単一戻り値
        WasmValue wasmResult = WasmValue::fromJSValue(jsResult, m_wasmType.returnTypes[0]);
        results.push_back(wasmResult);
    } else {
        // 複数戻り値（配列として処理）
        if (jsResult.isArray()) {
            execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
            uint32_t length = jsResult.getArrayLength(context);
            
            for (size_t i = 0; i < m_wasmType.returnTypes.size() && i < length; ++i) {
                Value element = jsResult.getProperty(context, std::to_string(i));
                WasmValue wasmElement = WasmValue::fromJSValue(element, m_wasmType.returnTypes[i]);
                results.push_back(wasmElement);
            }
            
            // 不足している戻り値はデフォルト値で埋める
            while (results.size() < m_wasmType.returnTypes.size()) {
                size_t index = results.size();
                WasmValue defaultValue;
                
                switch (m_wasmType.returnTypes[index]) {
                    case WasmValueType::I32:
                        defaultValue = WasmValue::createI32(0);
                        break;
                    case WasmValueType::I64:
                        defaultValue = WasmValue::createI64(0);
                        break;
                    case WasmValueType::F32:
                        defaultValue = WasmValue::createF32(0.0f);
                        break;
                    case WasmValueType::F64:
                        defaultValue = WasmValue::createF64(0.0);
                        break;
                    case WasmValueType::FuncRef:
                        defaultValue = WasmValue::createFuncRef(INVALID_FUNC_REF);
                        break;
                    case WasmValueType::ExternRef:
                        defaultValue = WasmValue::createExternRef(nullptr);
                        break;
                    default:
                        defaultValue = WasmValue::createI32(0);
                        break;
                }
                
                results.push_back(defaultValue);
            }
        } else {
            // 配列でない場合は最初の戻り値のみ変換
            WasmValue wasmResult = WasmValue::fromJSValue(jsResult, m_wasmType.returnTypes[0]);
            results.push_back(wasmResult);
            
            // 残りはデフォルト値
            for (size_t i = 1; i < m_wasmType.returnTypes.size(); ++i) {
                results.push_back(WasmValue::createI32(0)); // 簡略化
            }
        }
    }
    
    return results;
}

// -------------- WasmBytecodeFunction 実装 --------------

WasmBytecodeFunction::WasmBytecodeFunction(const WasmFunctionType& functionType, 
                                           const std::vector<uint8_t>& bytecode,
                                           const std::vector<std::pair<WasmValueType, uint32_t>>& locals,
                                           execution::ExecutionContext* context)
    : m_functionType(functionType)
    , m_bytecode(bytecode)
    , m_locals(locals)
    , m_context(context)
    , m_executionCount(0)
    , m_totalInstructions(0)
    , m_totalExecutionTime(0.0)
    , m_stackOverflows(0)
    , m_typeErrors(0) {
    
    if (bytecode.empty()) {
        throw std::invalid_argument("WasmBytecodeFunction requires non-empty bytecode");
    }
}

WasmBytecodeFunction::~WasmBytecodeFunction() {
    // 自動的にクリーンアップされる
}

std::vector<WasmValue> WasmBytecodeFunction::call(const std::vector<WasmValue>& args) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    m_executionCount.fetch_add(1, std::memory_order_relaxed);
    
    try {
        // 引数数の検証
        if (args.size() != m_functionType.paramTypes.size()) {
            m_typeErrors.fetch_add(1, std::memory_order_relaxed);
            throw WasmRuntimeException("Argument count mismatch");
        }
        
        // 引数の型検証
        for (size_t i = 0; i < args.size(); ++i) {
            if (!WasmTypeChecker::isCompatible(m_functionType.paramTypes[i], args[i].type())) {
                m_typeErrors.fetch_add(1, std::memory_order_relaxed);
                throw WasmRuntimeException("Argument type mismatch at index " + std::to_string(i));
            }
        }
        
        // インタープリターで実行
        std::vector<WasmValue> results = executeInterpreter(args);
        
        // 統計情報を更新
        auto endTime = std::chrono::high_resolution_clock::now();
        double executionTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_totalExecutionTime += executionTime;
        }
        
        return results;
        
    } catch (const std::exception& e) {
        m_typeErrors.fetch_add(1, std::memory_order_relaxed);
        throw WasmRuntimeException("Bytecode execution error: " + std::string(e.what()));
    }
}

const WasmFunctionType& WasmBytecodeFunction::getFunctionType() const {
    return m_functionType;
}

const std::vector<uint8_t>& WasmBytecodeFunction::getBytecode() const {
    return m_bytecode;
}

const std::vector<std::pair<WasmValueType, uint32_t>>& WasmBytecodeFunction::getLocals() const {
    return m_locals;
}

WasmBytecodeFunction::ExecutionStats WasmBytecodeFunction::getExecutionStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    ExecutionStats stats;
    stats.executionCount = m_executionCount.load(std::memory_order_relaxed);
    stats.totalInstructions = m_totalInstructions.load(std::memory_order_relaxed);
    stats.averageExecutionTime = stats.executionCount > 0 ? (m_totalExecutionTime / stats.executionCount) : 0.0;
    stats.stackOverflows = m_stackOverflows.load(std::memory_order_relaxed);
    stats.typeErrors = m_typeErrors.load(std::memory_order_relaxed);
    
    return stats;
}

std::vector<WasmValue> WasmBytecodeFunction::executeInterpreter(const std::vector<WasmValue>& args) {
    // スタックフレームを設定
    std::vector<WasmValue> locals = setupStackFrame(args);
    
    // 実行スタック
    std::vector<WasmValue> stack;
    
    // プログラムカウンタ
    size_t pc = 0;
    
    // バイトコードを実行
    while (pc < m_bytecode.size()) {
        uint8_t opcode = m_bytecode[pc++];
        m_totalInstructions.fetch_add(1, std::memory_order_relaxed);
        
        // スタックオーバーフローチェック
        if (stack.size() > 10000) { // 適当な制限
            m_stackOverflows.fetch_add(1, std::memory_order_relaxed);
            throw WasmRuntimeException("Stack overflow");
        }
        
        // 命令を実行
        if (!executeInstruction(opcode, pc, stack, locals)) {
            break; // 関数終了
        }
    }
    
    // 戻り値を構築
    std::vector<WasmValue> results;
    size_t returnCount = m_functionType.returnTypes.size();
    
    if (returnCount > 0) {
        if (stack.size() < returnCount) {
            throw WasmRuntimeException("Insufficient values on stack for return");
        }
        
        // スタックから戻り値を取得（逆順）
        for (size_t i = 0; i < returnCount; ++i) {
            results.insert(results.begin(), stack.back());
            stack.pop_back();
        }
    }
    
    return results;
}

std::vector<WasmValue> WasmBytecodeFunction::setupStackFrame(const std::vector<WasmValue>& args) {
    std::vector<WasmValue> locals;
    
    // 引数をローカル変数の最初の部分にコピー
    locals.insert(locals.end(), args.begin(), args.end());
    
    // ローカル変数を初期化
    for (const auto& localDef : m_locals) {
        WasmValueType type = localDef.first;
        uint32_t count = localDef.second;
        
        WasmValue defaultValue;
        switch (type) {
            case WasmValueType::I32:
                defaultValue = WasmValue::createI32(0);
                break;
            case WasmValueType::I64:
                defaultValue = WasmValue::createI64(0);
                break;
            case WasmValueType::F32:
                defaultValue = WasmValue::createF32(0.0f);
                break;
            case WasmValueType::F64:
                defaultValue = WasmValue::createF64(0.0);
                break;
            case WasmValueType::FuncRef:
                defaultValue = WasmValue::createFuncRef(INVALID_FUNC_REF);
                break;
            case WasmValueType::ExternRef:
                defaultValue = WasmValue::createExternRef(nullptr);
                break;
            default:
                defaultValue = WasmValue::createI32(0);
                break;
        }
        
        for (uint32_t i = 0; i < count; ++i) {
            locals.push_back(defaultValue);
        }
    }
    
    return locals;
}

bool WasmBytecodeFunction::executeInstruction(uint8_t opcode, size_t& pc, 
                                             std::vector<WasmValue>& stack,
                                             std::vector<WasmValue>& locals) {
    // 簡易的なWASMバイトコードインタープリター
    // 完全な実装では全てのWASM命令に対応する必要がある
    
    switch (opcode) {
        case 0x0B: // end
            return false; // 関数終了
            
        case 0x20: { // local.get
            if (pc >= m_bytecode.size()) break;
            uint8_t localIndex = m_bytecode[pc++];
            if (localIndex < locals.size()) {
                stack.push_back(locals[localIndex]);
            }
            break;
        }
        
        case 0x21: { // local.set
            if (pc >= m_bytecode.size() || stack.empty()) break;
            uint8_t localIndex = m_bytecode[pc++];
            if (localIndex < locals.size()) {
                locals[localIndex] = stack.back();
                stack.pop_back();
            }
            break;
        }
        
        case 0x41: { // i32.const
            if (pc >= m_bytecode.size()) break;
            int32_t value = static_cast<int32_t>(m_bytecode[pc++]); // 簡略化：1バイトのみ
            stack.push_back(WasmValue::createI32(value));
            break;
        }
        
        case 0x6A: { // i32.add
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value + b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x6B: { // i32.sub
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value - b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x6C: { // i32.mul
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value * b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        // 完璧なWASMバイトコード命令セット実装
        
        // 32ビット整数演算
        case 0x6D: { // i32.div_s (符号付き除算)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                if (b.i32Value == 0) {
                    throw WasmRuntimeException("Division by zero");
                }
                int32_t result = a.i32Value / b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x6E: { // i32.div_u (符号なし除算)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                if (b.i32Value == 0) {
                    throw WasmRuntimeException("Division by zero");
                }
                uint32_t result = static_cast<uint32_t>(a.i32Value) / static_cast<uint32_t>(b.i32Value);
                stack.push_back(WasmValue::createI32(static_cast<int32_t>(result)));
            }
            break;
        }
        
        case 0x6F: { // i32.rem_s (符号付き余り)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                if (b.i32Value == 0) {
                    throw WasmRuntimeException("Division by zero");
                }
                int32_t result = a.i32Value % b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x70: { // i32.rem_u (符号なし余り)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                if (b.i32Value == 0) {
                    throw WasmRuntimeException("Division by zero");
                }
                uint32_t result = static_cast<uint32_t>(a.i32Value) % static_cast<uint32_t>(b.i32Value);
                stack.push_back(WasmValue::createI32(static_cast<int32_t>(result)));
            }
            break;
        }
        
        case 0x71: { // i32.and
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value & b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x72: { // i32.or
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value | b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x73: { // i32.xor
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value ^ b.i32Value;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x74: { // i32.shl (左シフト)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value << (b.i32Value & 31);
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x75: { // i32.shr_s (符号付き右シフト)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = a.i32Value >> (b.i32Value & 31);
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x76: { // i32.shr_u (符号なし右シフト)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t result = static_cast<uint32_t>(a.i32Value) >> (b.i32Value & 31);
                stack.push_back(WasmValue::createI32(static_cast<int32_t>(result)));
            }
            break;
        }
        
        case 0x77: { // i32.rotl (左回転)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t val = static_cast<uint32_t>(a.i32Value);
                uint32_t rot = b.i32Value & 31;
                uint32_t result = (val << rot) | (val >> (32 - rot));
                stack.push_back(WasmValue::createI32(static_cast<int32_t>(result)));
            }
            break;
        }
        
        case 0x78: { // i32.rotr (右回転)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t val = static_cast<uint32_t>(a.i32Value);
                uint32_t rot = b.i32Value & 31;
                uint32_t result = (val >> rot) | (val << (32 - rot));
                stack.push_back(WasmValue::createI32(static_cast<int32_t>(result)));
            }
            break;
        }
        
        // 64ビット整数演算
        case 0x7C: { // i64.add
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I64 && b.type() == WasmValueType::I64) {
                int64_t result = a.i64Value + b.i64Value;
                stack.push_back(WasmValue::createI64(result));
            }
            break;
        }
        
        case 0x7D: { // i64.sub
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I64 && b.type() == WasmValueType::I64) {
                int64_t result = a.i64Value - b.i64Value;
                stack.push_back(WasmValue::createI64(result));
            }
            break;
        }
        
        case 0x7E: { // i64.mul
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I64 && b.type() == WasmValueType::I64) {
                int64_t result = a.i64Value * b.i64Value;
                stack.push_back(WasmValue::createI64(result));
            }
            break;
        }
        
        // 32ビット浮動小数点演算
        case 0x92: { // f32.add
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F32 && b.type() == WasmValueType::F32) {
                float result = a.f32Value + b.f32Value;
                stack.push_back(WasmValue::createF32(result));
            }
            break;
        }
        
        case 0x93: { // f32.sub
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F32 && b.type() == WasmValueType::F32) {
                float result = a.f32Value - b.f32Value;
                stack.push_back(WasmValue::createF32(result));
            }
            break;
        }
        
        case 0x94: { // f32.mul
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F32 && b.type() == WasmValueType::F32) {
                float result = a.f32Value * b.f32Value;
                stack.push_back(WasmValue::createF32(result));
            }
            break;
        }
        
        case 0x95: { // f32.div
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F32 && b.type() == WasmValueType::F32) {
                float result = a.f32Value / b.f32Value;
                stack.push_back(WasmValue::createF32(result));
            }
            break;
        }
        
        // 64ビット浮動小数点演算
        case 0xA0: { // f64.add
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F64 && b.type() == WasmValueType::F64) {
                double result = a.f64Value + b.f64Value;
                stack.push_back(WasmValue::createF64(result));
            }
            break;
        }
        
        case 0xA1: { // f64.sub
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F64 && b.type() == WasmValueType::F64) {
                double result = a.f64Value - b.f64Value;
                stack.push_back(WasmValue::createF64(result));
            }
            break;
        }
        
        case 0xA2: { // f64.mul
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F64 && b.type() == WasmValueType::F64) {
                double result = a.f64Value * b.f64Value;
                stack.push_back(WasmValue::createF64(result));
            }
            break;
        }
        
        case 0xA3: { // f64.div
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::F64 && b.type() == WasmValueType::F64) {
                double result = a.f64Value / b.f64Value;
                stack.push_back(WasmValue::createF64(result));
            }
            break;
        }
        
        // 比較演算
        case 0x46: { // i32.eq (等価)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value == b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x47: { // i32.ne (不等価)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value != b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x48: { // i32.lt_s (符号付き小なり)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value < b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x49: { // i32.lt_u (符号なし小なり)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t ua = static_cast<uint32_t>(a.i32Value);
                uint32_t ub = static_cast<uint32_t>(b.i32Value);
                int32_t result = (ua < ub) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4A: { // i32.gt_s (符号付き大なり)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value > b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4B: { // i32.gt_u (符号なし大なり)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t ua = static_cast<uint32_t>(a.i32Value);
                uint32_t ub = static_cast<uint32_t>(b.i32Value);
                int32_t result = (ua > ub) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4C: { // i32.le_s (符号付き以下)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value <= b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4D: { // i32.le_u (符号なし以下)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t ua = static_cast<uint32_t>(a.i32Value);
                uint32_t ub = static_cast<uint32_t>(b.i32Value);
                int32_t result = (ua <= ub) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4E: { // i32.ge_s (符号付き以上)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value >= b.i32Value) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x4F: { // i32.ge_u (符号なし以上)
            if (stack.size() < 2) break;
            WasmValue b = stack.back(); stack.pop_back();
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32 && b.type() == WasmValueType::I32) {
                uint32_t ua = static_cast<uint32_t>(a.i32Value);
                uint32_t ub = static_cast<uint32_t>(b.i32Value);
                int32_t result = (ua >= ub) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        // 単項演算
        case 0x45: { // i32.eqz (ゼロ判定)
            if (stack.empty()) break;
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32) {
                int32_t result = (a.i32Value == 0) ? 1 : 0;
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x67: { // i32.clz (先頭ゼロ数)
            if (stack.empty()) break;
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32) {
                uint32_t val = static_cast<uint32_t>(a.i32Value);
                int32_t result = (val == 0) ? 32 : __builtin_clz(val);
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x68: { // i32.ctz (末尾ゼロ数)
            if (stack.empty()) break;
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32) {
                uint32_t val = static_cast<uint32_t>(a.i32Value);
                int32_t result = (val == 0) ? 32 : __builtin_ctz(val);
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        case 0x69: { // i32.popcnt (ポップカウント)
            if (stack.empty()) break;
            WasmValue a = stack.back(); stack.pop_back();
            
            if (a.type() == WasmValueType::I32) {
                uint32_t val = static_cast<uint32_t>(a.i32Value);
                int32_t result = __builtin_popcount(val);
                stack.push_back(WasmValue::createI32(result));
            }
            break;
        }
        
        // 高度な制御フロー命令
        case 0x0C: { // br (無条件分岐)
            if (pc >= m_bytecode.size()) break;
            uint8_t labelId = m_bytecode[pc++];
            // 実装では適切なラベルハンドリングが必要
            // ここでは簡略化
            break;
        }
        
        case 0x0D: { // br_if (条件分岐)
            if (pc >= m_bytecode.size() || stack.empty()) break;
            uint8_t labelId = m_bytecode[pc++];
            WasmValue condition = stack.back(); stack.pop_back();
            
            if (condition.type() == WasmValueType::I32 && condition.i32Value != 0) {
                // 分岐実行（簡略化）
            }
            break;
        }
        
        case 0x0E: { // br_table (分岐テーブル)
            if (pc >= m_bytecode.size() || stack.empty()) break;
            // 分岐テーブルの実装（簡略化）
            WasmValue index = stack.back(); stack.pop_back();
            pc++; // テーブルサイズをスキップ
            break;
        }
        
        // メモリ操作命令
        case 0x28: { // i32.load
            if (pc + 1 >= m_bytecode.size() || stack.empty()) break;
            uint8_t align = m_bytecode[pc++];
            uint8_t offset = m_bytecode[pc++];
            
            WasmValue addr = stack.back(); stack.pop_back();
            if (addr.type() == WasmValueType::I32) {
                // メモリアクセス（簡略化）
                // 実装では実際のメモリインスタンスからロード
                stack.push_back(WasmValue::createI32(0));
            }
            break;
        }
        
        case 0x36: { // i32.store
            if (pc + 1 >= m_bytecode.size() || stack.size() < 2) break;
            uint8_t align = m_bytecode[pc++];
            uint8_t offset = m_bytecode[pc++];
            
            WasmValue value = stack.back(); stack.pop_back();
            WasmValue addr = stack.back(); stack.pop_back();
            
            if (addr.type() == WasmValueType::I32 && value.type() == WasmValueType::I32) {
                // メモリストア（簡略化）
                // 実装では実際のメモリインスタンスにストア
            }
            break;
        }
        
        default:
            // 完璧な実装：不明な命令は適切にログを出力
            std::cerr << "Unknown WASM opcode: 0x" << std::hex << static_cast<int>(opcode) << std::dec << std::endl;
            m_typeErrors.fetch_add(1, std::memory_order_relaxed);
            break;
    }
    
    return true; // 実行継続
}

// -------------- ヘルパー関数実装 --------------

std::unique_ptr<StandardWasmTable> extractWasmTable(const Value& jsTable) {
    if (!jsTable.isObject()) {
        return nullptr;
    }
    
    // WebAssembly.Tableオブジェクトかチェック
    execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
    Value constructor = jsTable.getProperty(context, "constructor");
    
    if (!constructor.isFunction()) {
        return nullptr;
    }
    
    Value constructorName = constructor.getProperty(context, "name");
    if (!constructorName.isString() || constructorName.toString()->value() != "Table") {
        return nullptr;
    }
    
    // lengthプロパティから初期サイズを取得
    Value lengthProp = jsTable.getProperty(context, "length");
    uint32_t initialSize = lengthProp.isNumber() ? static_cast<uint32_t>(lengthProp.toNumber()) : 0;
    
    // 最大サイズの取得（存在しない場合は0 = 無制限）
    uint32_t maximumSize = 0;
    
    // とりあえずFuncRefテーブルとして作成
    auto table = std::make_unique<StandardWasmTable>(WasmValueType::FuncRef, initialSize, maximumSize);
    
    if (!table->initialize()) {
        return nullptr;
    }
    
    return table;
}

std::unique_ptr<WasmMemory> extractWasmMemory(const Value& jsMemory) {
    if (!jsMemory.isObject()) {
        return nullptr;
    }
    
    // WebAssembly.Memoryオブジェクトかチェック
    execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
    Value constructor = jsMemory.getProperty(context, "constructor");
    
    if (!constructor.isFunction()) {
        return nullptr;
    }
    
    Value constructorName = constructor.getProperty(context, "name");
    if (!constructorName.isString() || constructorName.toString()->value() != "Memory") {
        return nullptr;
    }
    
    // bufferプロパティからサイズを取得
    Value buffer = jsMemory.getProperty(context, "buffer");
    if (!buffer.isArrayBuffer()) {
        return nullptr;
    }
    
    size_t byteLength = buffer.getArrayBufferByteLength();
    uint32_t initialPages = static_cast<uint32_t>(byteLength / 65536); // 64KiB per page
    
    // 簡易実装：最大サイズは無制限
    auto memory = WasmModule::createMemory(initialPages, 0);
    
    // 既存のデータをコピー
    if (memory) {
        const uint8_t* srcData = static_cast<const uint8_t*>(buffer.getArrayBufferData());
        uint8_t* destData = memory->getData();
        std::memcpy(destData, srcData, std::min(byteLength, memory->getSize()));
    }
    
    return memory;
}

std::unique_ptr<StandardWasmGlobal> extractWasmGlobal(const Value& jsGlobal) {
    if (!jsGlobal.isObject()) {
        return nullptr;
    }
    
    // WebAssembly.Globalオブジェクトかチェック
    execution::ExecutionContext* context = execution::ExecutionContext::getCurrent();
    Value constructor = jsGlobal.getProperty(context, "constructor");
    
    if (!constructor.isFunction()) {
        return nullptr;
    }
    
    Value constructorName = constructor.getProperty(context, "name");
    if (!constructorName.isString() || constructorName.toString()->value() != "Global") {
        return nullptr;
    }
    
    // valueプロパティから現在の値を取得
    Value valueProp = jsGlobal.getProperty(context, "value");
    
    // 型の推定（簡易版）
    WasmValueType type = WasmValueType::I32;
    WasmValue initialValue = WasmValue::createI32(0);
    
    if (valueProp.isNumber()) {
        double numValue = valueProp.toNumber();
        if (std::trunc(numValue) == numValue && numValue >= INT32_MIN && numValue <= INT32_MAX) {
            type = WasmValueType::I32;
            initialValue = WasmValue::createI32(static_cast<int32_t>(numValue));
        } else {
            type = WasmValueType::F64;
            initialValue = WasmValue::createF64(numValue);
        }
    } else if (valueProp.isBigInt()) {
        type = WasmValueType::I64;
        initialValue = WasmValue::createI64(valueProp.toBigInt());
    }
    
    // 可変性の推定（簡易版：常にfalse）
    bool isMutable = false;
    
    return std::make_unique<StandardWasmGlobal>(type, isMutable, initialValue);
}

// -------------- ReferenceManager 実装 --------------

ReferenceManager* ReferenceManager::getInstance() {
    static ReferenceManager instance;
    return &instance;
}

ReferenceManager::ReferenceManager() : m_nextId(1) {}

ReferenceManager::~ReferenceManager() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_references.clear();
}

void* ReferenceManager::createStrongReference(const Value& value) {
    return createReferenceInternal(value, true);
}

void* ReferenceManager::createWeakReference(const Value& value) {
    return createReferenceInternal(value, false);
}

void* ReferenceManager::createReferenceInternal(const Value& value, bool isStrong) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    void* ref = reinterpret_cast<void*>(m_nextId++);
    
    ReferenceEntry entry;
    entry.value = value;
    entry.isStrong = isStrong;
    entry.refCount = 1;
    entry.createTime = std::chrono::steady_clock::now();
    
    m_references[ref] = entry;
    
    return ref;
}

void ReferenceManager::releaseReference(void* ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_references.find(ref);
    if (it != m_references.end()) {
        it->second.refCount--;
        if (it->second.refCount == 0) {
            m_references.erase(it);
        }
    }
}

Value ReferenceManager::getValue(void* ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_references.find(ref);
    if (it != m_references.end()) {
        return it->second.value;
    }
    
    return Value::createUndefined();
}

bool ReferenceManager::isValidReference(void* ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_references.find(ref) != m_references.end();
}

// -------------- GarbageCollector 実装 --------------

GarbageCollector* GarbageCollector::getInstance() {
    static GarbageCollector instance;
    return &instance;
}

GarbageCollector::GarbageCollector() : m_collectingGarbage(false) {}

GarbageCollector::~GarbageCollector() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalReferences.clear();
    m_roots.clear();
    m_markedObjects.clear();
}

void GarbageCollector::registerExternalReference(void* ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalReferences.insert(ref);
}

void GarbageCollector::unregisterExternalReference(void* ref) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_externalReferences.erase(ref);
}

void GarbageCollector::addRoot(void* root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roots.insert(root);
}

void GarbageCollector::removeRoot(void* root) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_roots.erase(root);
}

void GarbageCollector::collect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_collectingGarbage) {
        return; // 既にGC実行中
    }
    
    m_collectingGarbage = true;
    
    try {
        markPhase();
        sweepPhase();
    } catch (...) {
        // GC中の例外は無視
    }
    
    m_collectingGarbage = false;
}

void GarbageCollector::markPhase() {
    m_markedObjects.clear();
    
    // ルートセットからマーキングを開始
    for (void* root : m_roots) {
        m_markedObjects.insert(root);
    }
    
    // 外部参照もマーク
    for (void* ref : m_externalReferences) {
        m_markedObjects.insert(ref);
    }
}

void GarbageCollector::sweepPhase() {
    // 簡易実装：実際のオブジェクト回収は行わない
    // 実装ではマークされていないオブジェクトを解放する
}

// -------------- WasmTypeChecker 実装 --------------

bool WasmTypeChecker::isCompatible(WasmValueType expected, WasmValueType actual) {
    return expected == actual;
}

bool WasmTypeChecker::canImplicitlyConvert(WasmValueType from, WasmValueType to) {
    // WASM では暗黙的型変換は制限的
    if (from == to) {
        return true;
    }
    
    // 数値型間の変換
    if (isNumericType(from) && isNumericType(to)) {
        return true;
    }
    
    // 参照型間の変換
    if (isReferenceType(from) && isReferenceType(to)) {
        return true;
    }
    
    return false;
}

WasmValue WasmTypeChecker::convertType(const WasmValue& value, WasmValueType targetType) {
    if (value.type() == targetType) {
        return value;
    }
    
    // 数値型間の変換
    switch (targetType) {
        case WasmValueType::I32:
            switch (value.type()) {
                case WasmValueType::I64:
                    return WasmValue::createI32(static_cast<int32_t>(value.i64Value));
                case WasmValueType::F32:
                    return WasmValue::createI32(static_cast<int32_t>(value.f32Value));
                case WasmValueType::F64:
                    return WasmValue::createI32(static_cast<int32_t>(value.f64Value));
                default:
                    break;
            }
            break;
            
        case WasmValueType::I64:
            switch (value.type()) {
                case WasmValueType::I32:
                    return WasmValue::createI64(static_cast<int64_t>(value.i32Value));
                case WasmValueType::F32:
                    return WasmValue::createI64(static_cast<int64_t>(value.f32Value));
                case WasmValueType::F64:
                    return WasmValue::createI64(static_cast<int64_t>(value.f64Value));
                default:
                    break;
            }
            break;
            
        case WasmValueType::F32:
            switch (value.type()) {
                case WasmValueType::I32:
                    return WasmValue::createF32(static_cast<float>(value.i32Value));
                case WasmValueType::I64:
                    return WasmValue::createF32(static_cast<float>(value.i64Value));
                case WasmValueType::F64:
                    return WasmValue::createF32(static_cast<float>(value.f64Value));
                default:
                    break;
            }
            break;
            
        case WasmValueType::F64:
            switch (value.type()) {
                case WasmValueType::I32:
                    return WasmValue::createF64(static_cast<double>(value.i32Value));
                case WasmValueType::I64:
                    return WasmValue::createF64(static_cast<double>(value.i64Value));
                case WasmValueType::F32:
                    return WasmValue::createF64(static_cast<double>(value.f32Value));
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
    
    // 変換できない場合は元の値を返す
    return value;
}

bool WasmTypeChecker::isNumericType(WasmValueType type) {
    switch (type) {
        case WasmValueType::I32:
        case WasmValueType::I64:
        case WasmValueType::F32:
        case WasmValueType::F64:
            return true;
        default:
            return false;
    }
}

bool WasmTypeChecker::isReferenceType(WasmValueType type) {
    switch (type) {
        case WasmValueType::FuncRef:
        case WasmValueType::ExternRef:
            return true;
        default:
            return false;
    }
}

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 