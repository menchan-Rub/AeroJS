/**
 * @file wasm_adapters.h
 * @brief WebAssemblyとJavaScript間のアダプター完璧実装
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "wasm_module.h"
#include "wasm_table.h"
#include "wasm_global.h"
#include <functional>
#include <memory>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

/**
 * @brief JavaScript関数をWASM関数として使用するためのアダプター
 */
class JSWasmFunctionAdapter : public WasmFunction {
public:
    /**
     * @brief コンストラクタ
     * @param jsFunction JavaScript関数
     * @param wasmType WASM関数型
     */
    JSWasmFunctionAdapter(const Value& jsFunction, const WasmFunctionType& wasmType);
    
    /**
     * @brief デストラクタ
     */
    ~JSWasmFunctionAdapter() override;
    
    /**
     * @brief 関数を呼び出し
     * @param args 引数のリスト
     * @return 戻り値のリスト
     */
    std::vector<WasmValue> call(const std::vector<WasmValue>& args) override;
    
    /**
     * @brief 関数型の取得
     * @return 関数型
     */
    const WasmFunctionType& getFunctionType() const override;
    
    /**
     * @brief JavaScript関数の取得
     * @return JavaScript関数
     */
    const Value& getJSFunction() const;
    
    /**
     * @brief 型変換エラーハンドリング
     * @param error エラーメッセージ
     * @param context 実行コンテキスト
     */
    void handleTypeError(const std::string& error, execution::ExecutionContext* context);
    
    /**
     * @brief 引数の型検証
     * @param args WASM引数
     * @return 検証結果
     */
    bool validateArguments(const std::vector<WasmValue>& args);
    
    /**
     * @brief 統計情報
     */
    struct AdapterStats {
        uint64_t callCount;           // 呼び出し回数
        uint64_t typeErrors;          // 型エラー回数
        uint64_t successfulCalls;     // 成功した呼び出し
        double averageExecutionTime;  // 平均実行時間
    };
    
    AdapterStats getStats() const;

private:
    Value m_jsFunction;                   // JavaScript関数
    WasmFunctionType m_wasmType;          // WASM関数型
    mutable std::atomic<uint64_t> m_callCount;
    mutable std::atomic<uint64_t> m_typeErrors;
    mutable std::atomic<uint64_t> m_successfulCalls;
    mutable double m_totalExecutionTime;
    mutable std::mutex m_statsMutex;
    
    /**
     * @brief 引数をJavaScript値に変換
     * @param wasmArgs WASM引数
     * @return JavaScript引数
     */
    std::vector<Value> convertArgsToJS(const std::vector<WasmValue>& wasmArgs);
    
    /**
     * @brief 戻り値をWASM値に変換
     * @param jsResult JavaScript戻り値
     * @return WASM戻り値
     */
    std::vector<WasmValue> convertResultToWasm(const Value& jsResult);
};

/**
 * @brief WASMバイトコード関数の実装
 */
class WasmBytecodeFunction : public WasmFunction {
public:
    /**
     * @brief コンストラクタ
     * @param functionType 関数型
     * @param bytecode バイトコード
     * @param locals ローカル変数定義
     * @param context 実行コンテキスト
     */
    WasmBytecodeFunction(const WasmFunctionType& functionType, 
                         const std::vector<uint8_t>& bytecode,
                         const std::vector<std::pair<WasmValueType, uint32_t>>& locals,
                         execution::ExecutionContext* context);
    
    /**
     * @brief デストラクタ
     */
    ~WasmBytecodeFunction() override;
    
    /**
     * @brief 関数を呼び出し
     * @param args 引数のリスト
     * @return 戻り値のリスト
     */
    std::vector<WasmValue> call(const std::vector<WasmValue>& args) override;
    
    /**
     * @brief 関数型の取得
     * @return 関数型
     */
    const WasmFunctionType& getFunctionType() const override;
    
    /**
     * @brief バイトコードの取得
     * @return バイトコード
     */
    const std::vector<uint8_t>& getBytecode() const;
    
    /**
     * @brief ローカル変数定義の取得
     * @return ローカル変数定義
     */
    const std::vector<std::pair<WasmValueType, uint32_t>>& getLocals() const;
    
    /**
     * @brief 実行統計の取得
     */
    struct ExecutionStats {
        uint64_t executionCount;        // 実行回数
        uint64_t totalInstructions;     // 実行された命令数
        double averageExecutionTime;    // 平均実行時間
        uint64_t stackOverflows;        // スタックオーバーフロー回数
        uint64_t typeErrors;            // 型エラー回数
    };
    
    ExecutionStats getExecutionStats() const;

private:
    WasmFunctionType m_functionType;      // 関数型
    std::vector<uint8_t> m_bytecode;      // バイトコード
    std::vector<std::pair<WasmValueType, uint32_t>> m_locals;  // ローカル変数
    execution::ExecutionContext* m_context;  // 実行コンテキスト
    
    // 実行統計
    mutable std::atomic<uint64_t> m_executionCount;
    mutable std::atomic<uint64_t> m_totalInstructions;
    mutable double m_totalExecutionTime;
    mutable std::atomic<uint64_t> m_stackOverflows;
    mutable std::atomic<uint64_t> m_typeErrors;
    mutable std::mutex m_statsMutex;
    
    /**
     * @brief バイトコードインタープリター
     * @param args 引数
     * @return 戻り値
     */
    std::vector<WasmValue> executeInterpreter(const std::vector<WasmValue>& args);
    
    /**
     * @brief スタックフレームの設定
     * @param args 引数
     * @return ローカル変数配列
     */
    std::vector<WasmValue> setupStackFrame(const std::vector<WasmValue>& args);
    
    /**
     * @brief 命令の実行
     * @param opcode オペコード
     * @param pc プログラムカウンタ
     * @param stack スタック
     * @param locals ローカル変数
     * @return 実行継続フラグ
     */
    bool executeInstruction(uint8_t opcode, size_t& pc, 
                          std::vector<WasmValue>& stack,
                          std::vector<WasmValue>& locals);
};

/**
 * @brief WebAssemblyオブジェクト抽出ヘルパー関数群
 */

/**
 * @brief WebAssembly.TableオブジェクトからStandardWasmTableを抽出
 * @param jsTable JavaScript WebAssembly.Tableオブジェクト
 * @return 抽出されたテーブル
 */
std::unique_ptr<StandardWasmTable> extractWasmTable(const Value& jsTable);

/**
 * @brief WebAssembly.MemoryオブジェクトからStandardWasmMemoryを抽出
 * @param jsMemory JavaScript WebAssembly.Memoryオブジェクト
 * @return 抽出されたメモリ
 */
std::unique_ptr<WasmMemory> extractWasmMemory(const Value& jsMemory);

/**
 * @brief WebAssembly.GlobalオブジェクトからStandardWasmGlobalを抽出
 * @param jsGlobal JavaScript WebAssembly.Globalオブジェクト
 * @return 抽出されたグローバル変数
 */
std::unique_ptr<StandardWasmGlobal> extractWasmGlobal(const Value& jsGlobal);

/**
 * @brief ReferenceManagerシングルトン
 * JavaScript値への参照を管理する
 */
class ReferenceManager {
public:
    static ReferenceManager* getInstance();
    
    /**
     * @brief 強参照の作成
     * @param value JavaScript値
     * @return 参照ポインタ
     */
    void* createStrongReference(const Value& value);
    
    /**
     * @brief 弱参照の作成
     * @param value JavaScript値
     * @return 参照ポインタ
     */
    void* createWeakReference(const Value& value);
    
    /**
     * @brief 参照の解放
     * @param ref 参照ポインタ
     */
    void releaseReference(void* ref);
    
    /**
     * @brief 参照から値を取得
     * @param ref 参照ポインタ
     * @return JavaScript値
     */
    Value getValue(void* ref);
    
    /**
     * @brief 参照の有効性チェック
     * @param ref 参照ポインタ
     * @return 有効かどうか
     */
    bool isValidReference(void* ref);

private:
    ReferenceManager();
    ~ReferenceManager();
    
    struct ReferenceEntry {
        Value value;
        bool isStrong;
        uint32_t refCount;
        std::chrono::steady_clock::time_point createTime;
    };
    
    std::unordered_map<void*, ReferenceEntry> m_references;
    std::mutex m_mutex;
    uint64_t m_nextId;
    
    void* createReferenceInternal(const Value& value, bool isStrong);
};

/**
 * @brief GarbageCollectorシングルトン
 * WASM外部参照のGC統合
 */
class GarbageCollector {
public:
    static GarbageCollector* getInstance();
    
    /**
     * @brief 外部参照の登録
     * @param ref 外部参照
     */
    void registerExternalReference(void* ref);
    
    /**
     * @brief 外部参照の解除
     * @param ref 外部参照
     */
    void unregisterExternalReference(void* ref);
    
    /**
     * @brief ルートセットに追加
     * @param root ルートオブジェクト
     */
    void addRoot(void* root);
    
    /**
     * @brief ルートセットから削除
     * @param root ルートオブジェクト
     */
    void removeRoot(void* root);
    
    /**
     * @brief GCの実行
     */
    void collect();
    
    /**
     * @brief マーキングフェーズ
     */
    void markPhase();
    
    /**
     * @brief スイープフェーズ
     */
    void sweepPhase();

private:
    GarbageCollector();
    ~GarbageCollector();
    
    std::unordered_set<void*> m_externalReferences;
    std::unordered_set<void*> m_roots;
    std::unordered_set<void*> m_markedObjects;
    std::mutex m_mutex;
    bool m_collectingGarbage;
};

/**
 * @brief WASM例外クラス
 */
class WasmRuntimeException : public std::runtime_error {
public:
    WasmRuntimeException(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief WASM型検証ヘルパー
 */
class WasmTypeChecker {
public:
    /**
     * @brief 型の互換性チェック
     * @param expected 期待される型
     * @param actual 実際の型
     * @return 互換性があるかどうか
     */
    static bool isCompatible(WasmValueType expected, WasmValueType actual);
    
    /**
     * @brief 暗黙的型変換が可能かチェック
     * @param from 変換元の型
     * @param to 変換先の型
     * @return 変換可能かどうか
     */
    static bool canImplicitlyConvert(WasmValueType from, WasmValueType to);
    
    /**
     * @brief 型変換の実行
     * @param value 変換する値
     * @param targetType 変換先の型
     * @return 変換後の値
     */
    static WasmValue convertType(const WasmValue& value, WasmValueType targetType);
    
    /**
     * @brief 数値型かどうかの判定
     * @param type 型
     * @return 数値型かどうか
     */
    static bool isNumericType(WasmValueType type);
    
    /**
     * @brief 参照型かどうかの判定
     * @param type 型
     * @return 参照型かどうか
     */
    static bool isReferenceType(WasmValueType type);
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 