#ifndef AEROJS_CORE_RUNTIME_BUILTINS_ARRAY_VECTOR_OPS_H
#define AEROJS_CORE_RUNTIME_BUILTINS_ARRAY_VECTOR_OPS_H

#include <vector>
#include "../../../value.h"

namespace aerojs {
namespace core {

class Context;

/**
 * @brief RISC-V Vector拡張を使用した配列操作の準備段階
 * 
 * 配列データをベクトル操作に適した形式に変換する
 * 
 * @param args 引数リスト（配列、操作タイプ、コールバック関数、要素サイズ、TypedArray判定）
 * @param thisValue thisオブジェクト
 * @return 準備されたデータ構造
 */
Value aerojs_riscv_prepare(const std::vector<Value>& args, Value thisValue);

/**
 * @brief RISC-V Vector拡張を使用した配列操作の実行段階
 * 
 * 準備されたデータに対してベクトル操作を実行する
 * 
 * @param args 引数リスト（準備データ、配列長）
 * @param thisValue thisオブジェクト
 * @return 操作結果
 */
Value aerojs_riscv_execute(const std::vector<Value>& args, Value thisValue);

/**
 * @brief RISC-V Vector拡張を使用した配列操作の仕上げ段階
 * 
 * 実行結果からJavaScriptの戻り値を作成する
 * 
 * @param args 引数リスト（実行結果）
 * @param thisValue thisオブジェクト
 * @return 最終結果
 */
Value aerojs_riscv_finalize(const std::vector<Value>& args, Value thisValue);

/**
 * @brief JavaScript配列オブジェクトへの新機能関数登録
 * 
 * @param context 実行コンテキスト
 */
void registerArrayVectorOperations(Context* context);

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_RUNTIME_BUILTINS_ARRAY_VECTOR_OPS_H 