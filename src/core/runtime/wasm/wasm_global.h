/**
 * @file wasm_global.h
 * @brief WebAssemblyグローバル変数の完璧な実装
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "wasm_module.h"
#include <mutex>
#include <atomic>
#include <memory>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

/**
 * @brief WebAssemblyグローバル変数の標準実装
 * 全ての WASM 値型をサポートする高性能グローバル変数
 */
class StandardWasmGlobal : public WasmGlobal {
public:
    /**
     * @brief コンストラクタ
     * @param type 値型
     * @param mutable_ 可変フラグ
     * @param initialValue 初期値
     */
    StandardWasmGlobal(WasmValueType type, bool mutable_, const WasmValue& initialValue);
    
    /**
     * @brief デストラクタ
     */
    ~StandardWasmGlobal() override;
    
    /**
     * @brief 値の取得
     * @return 現在の値
     */
    WasmValue getValue() const override;
    
    /**
     * @brief 値の設定
     * @param value 新しい値
     */
    void setValue(const WasmValue& value) override;
    
    /**
     * @brief 可変性の確認
     * @return 可変かどうか
     */
    bool isMutable() const override;
    
    /**
     * @brief 型の取得
     * @return 値型
     */
    WasmValueType getType() const override;
    
    /**
     * @brief 型と値の互換性チェック
     * @param type チェックする型
     * @param value チェックする値
     * @return 互換性があるかどうか
     */
    bool validateType(WasmValueType type, const WasmValue& value) const;
    
    /**
     * @brief 可変性の設定（初期化時のみ使用）
     * @param mutable_ 可変フラグ
     */
    void setMutable(bool mutable_);
    
    /**
     * @brief 初期値への復元
     */
    void reset();
    
    /**
     * @brief グローバル変数をJavaScript値に変換
     * @param context 実行コンテキスト
     * @return JavaScript値
     */
    Value toJSValue(ExecutionContext* context) const;
    
    /**
     * @brief JavaScript値からグローバル変数を設定
     * @param value JavaScript値
     * @param context 実行コンテキスト
     * @return 設定が成功したかどうか
     */
    bool fromJSValue(const Value& value, ExecutionContext* context);
    
    /**
     * @brief アトミック比較交換操作（スレッドセーフ）
     * @param expected 期待値
     * @param desired 新しい値
     * @return 交換が成功したかどうか
     */
    bool compareExchange(const WasmValue& expected, const WasmValue& desired);
    
    /**
     * @brief アトミック交換操作
     * @param newValue 新しい値
     * @return 以前の値
     */
    WasmValue exchange(const WasmValue& newValue);
    
    /**
     * @brief アトミック加算操作（数値型のみ）
     * @param value 加算する値
     * @return 操作前の値
     */
    WasmValue fetchAdd(const WasmValue& value);
    
    /**
     * @brief アトミック減算操作（数値型のみ）
     * @param value 減算する値
     * @return 操作前の値
     */
    WasmValue fetchSub(const WasmValue& value);
    
    /**
     * @brief 統計情報の取得
     */
    struct GlobalStats {
        WasmValueType type;         // 値型
        bool isMutable;             // 可変性
        uint64_t readOperations;    // 読み取り操作回数
        uint64_t writeOperations;   // 書き込み操作回数
        uint64_t atomicOperations;  // アトミック操作回数
        WasmValue currentValue;     // 現在の値
        WasmValue initialValue;     // 初期値
    };
    
    GlobalStats getStats() const;
    
    /**
     * @brief 値の範囲チェック（数値型）
     * @param value チェックする値
     * @return 範囲内かどうか
     */
    bool isInRange(const WasmValue& value) const;
    
    /**
     * @brief グローバル変数のクローンを作成
     * @return クローンされたグローバル変数
     */
    std::unique_ptr<StandardWasmGlobal> clone() const;

private:
    WasmValueType m_type;           // 値型
    bool m_mutable;                 // 可変性
    WasmValue m_value;              // 現在の値
    WasmValue m_initialValue;       // 初期値
    
    // スレッドセーフ用のミューテックス
    mutable std::mutex m_mutex;
    
    // 統計情報（アトミック操作で管理）
    mutable std::atomic<uint64_t> m_readOperations;
    mutable std::atomic<uint64_t> m_writeOperations;
    mutable std::atomic<uint64_t> m_atomicOperations;
    
    /**
     * @brief 値の型チェック
     * @param value チェックする値
     * @return 型が一致するかどうか
     */
    bool isTypeCompatible(const WasmValue& value) const;
    
    /**
     * @brief アトミック操作が可能かチェック
     * @return アトミック操作が可能かどうか
     */
    bool supportsAtomicOperations() const;
    
    /**
     * @brief 数値操作のヘルパー関数
     */
    WasmValue performNumericOperation(const WasmValue& current, const WasmValue& operand, 
                                     std::function<WasmValue(const WasmValue&, const WasmValue&)> operation) const;
};

/**
 * @brief WebAssemblyグローバル変数ファクトリ
 */
class WasmGlobalFactory {
public:
    /**
     * @brief I32グローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return I32グローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createI32Global(int32_t initialValue, bool mutable_ = false);
    
    /**
     * @brief I64グローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return I64グローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createI64Global(int64_t initialValue, bool mutable_ = false);
    
    /**
     * @brief F32グローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return F32グローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createF32Global(float initialValue, bool mutable_ = false);
    
    /**
     * @brief F64グローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return F64グローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createF64Global(double initialValue, bool mutable_ = false);
    
    /**
     * @brief FuncRefグローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return FuncRefグローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createFuncRefGlobal(uint32_t initialValue, bool mutable_ = false);
    
    /**
     * @brief ExternRefグローバル変数の作成
     * @param initialValue 初期値
     * @param mutable_ 可変フラグ
     * @return ExternRefグローバル変数
     */
    static std::unique_ptr<StandardWasmGlobal> createExternRefGlobal(void* initialValue, bool mutable_ = false);
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 