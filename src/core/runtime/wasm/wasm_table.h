/**
 * @file wasm_table.h
 * @brief WebAssemblyテーブルの完璧な実装
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "wasm_module.h"
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>

namespace aerojs {
namespace core {
namespace runtime {
namespace wasm {

/**
 * @brief WebAssemblyテーブルの標準実装
 * FuncRefとExternRefをサポートする高性能テーブル
 */
class StandardWasmTable : public WasmTable {
public:
    /**
     * @brief コンストラクタ
     * @param elemType 要素型（FuncRef or ExternRef）
     * @param initialSize 初期サイズ
     * @param maximumSize 最大サイズ（0の場合は制限なし）
     */
    StandardWasmTable(WasmValueType elemType, uint32_t initialSize, uint32_t maximumSize = 0);
    
    /**
     * @brief デストラクタ
     */
    ~StandardWasmTable() override;
    
    /**
     * @brief テーブルの初期化
     * @return 初期化が成功したかどうか
     */
    bool initialize();
    
    /**
     * @brief 要素の取得
     * @param index インデックス
     * @return 要素の値
     */
    WasmValue get(uint32_t index) const override;
    
    /**
     * @brief 要素の設定
     * @param index インデックス
     * @param value 設定する値
     * @return 設定が成功したかどうか
     */
    bool set(uint32_t index, const WasmValue& value) override;
    
    /**
     * @brief テーブルサイズの取得
     * @return 現在のテーブルサイズ
     */
    uint32_t size() const override;
    
    /**
     * @brief テーブルの拡張
     * @param count 追加する要素数
     * @param initValue 初期化値
     * @return 拡張が成功したかどうか
     */
    bool grow(uint32_t count, const WasmValue& initValue) override;
    
    /**
     * @brief 要素型の取得
     * @return 要素型
     */
    WasmValueType getElementType() const;
    
    /**
     * @brief 最大サイズの取得
     * @return 最大サイズ（0の場合は制限なし）
     */
    uint32_t getMaximumSize() const;
    
    /**
     * @brief テーブルの容量を確認
     * @param newSize 新しいサイズ
     * @return 容量が十分かどうか
     */
    bool hasCapacity(uint32_t newSize) const;
    
    /**
     * @brief インデックスの範囲チェック
     * @param index チェックするインデックス
     * @return インデックスが有効かどうか
     */
    bool isValidIndex(uint32_t index) const;
    
    /**
     * @brief 要素型の互換性チェック
     * @param value チェックする値
     * @return 型が互換性があるかどうか
     */
    bool isCompatibleValue(const WasmValue& value) const;
    
    /**
     * @brief テーブルの統計情報を取得
     * @return 統計情報の構造体
     */
    struct TableStats {
        uint32_t size;              // 現在のサイズ
        uint32_t maximumSize;       // 最大サイズ
        uint32_t usedEntries;       // 使用中エントリ数
        uint32_t nullEntries;       // nullエントリ数
        uint64_t getOperations;     // get操作回数
        uint64_t setOperations;     // set操作回数
        uint64_t growOperations;    // grow操作回数
        WasmValueType elementType;  // 要素型
    };
    
    TableStats getStats() const;
    
    /**
     * @brief テーブルの内容をクリア
     */
    void clear();
    
    /**
     * @brief テーブルの内容を別のテーブルにコピー
     * @param dest コピー先テーブル
     * @param srcOffset コピー元オフセット
     * @param destOffset コピー先オフセット
     * @param count コピーする要素数
     * @return コピーが成功したかどうか
     */
    bool copyTo(StandardWasmTable* dest, uint32_t srcOffset, uint32_t destOffset, uint32_t count) const;
    
    /**
     * @brief 範囲初期化
     * @param offset 開始オフセット
     * @param count 初期化する要素数
     * @param value 初期化値
     * @return 初期化が成功したかどうか
     */
    bool fill(uint32_t offset, uint32_t count, const WasmValue& value);

private:
    WasmValueType m_elementType;        // 要素型
    uint32_t m_currentSize;             // 現在のサイズ
    uint32_t m_maximumSize;             // 最大サイズ（0 = 制限なし）
    
    // テーブルデータ
    std::vector<WasmValue> m_elements;
    
    // スレッドセーフ用のミューテックス
    mutable std::mutex m_mutex;
    
    // 統計情報（アトミック操作で管理）
    mutable std::atomic<uint64_t> m_getOperations;
    mutable std::atomic<uint64_t> m_setOperations;
    mutable std::atomic<uint64_t> m_growOperations;
    
    /**
     * @brief デフォルト値の作成
     * @return 要素型に応じたデフォルト値
     */
    WasmValue createDefaultValue() const;
    
    /**
     * @brief 使用中エントリ数をカウント
     * @return 使用中（非null）エントリ数
     */
    uint32_t countUsedEntries() const;
};

/**
 * @brief WebAssemblyテーブルファクトリ
 */
class WasmTableFactory {
public:
    /**
     * @brief FuncRefテーブルの作成
     * @param initialSize 初期サイズ
     * @param maximumSize 最大サイズ
     * @return FuncRefテーブル
     */
    static std::unique_ptr<StandardWasmTable> createFuncRefTable(uint32_t initialSize, uint32_t maximumSize = 0);
    
    /**
     * @brief ExternRefテーブルの作成
     * @param initialSize 初期サイズ
     * @param maximumSize 最大サイズ
     * @return ExternRefテーブル
     */
    static std::unique_ptr<StandardWasmTable> createExternRefTable(uint32_t initialSize, uint32_t maximumSize = 0);
};

} // namespace wasm
} // namespace runtime
} // namespace core
} // namespace aerojs 