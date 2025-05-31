/**
 * @file riscv_ic_generator.h
 * @brief RISC-Vアーキテクチャ向けのインラインキャッシュコード生成
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "ic_code_generator.h"
#include "inline_cache.h"
#include <memory>

namespace aerojs {
namespace core {

/**
 * @brief RISC-Vアーキテクチャ向けのインラインキャッシュコード生成クラス
 * 
 * このクラスはRISC-Vプロセッサ向けのインラインキャッシュスタブコードを生成します。
 */
class RISCV_ICGenerator {
public:
    /**
     * @brief モノモーフィックプロパティアクセス用のスタブコードを生成
     * @param cachePtr プロパティキャッシュへのポインタ
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cachePtr);
    
    /**
     * @brief ポリモーフィックプロパティアクセス用のスタブコードを生成
     * @param cachePtr プロパティキャッシュへのポインタ
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cachePtr);
    
    /**
     * @brief メガモーフィックプロパティアクセス用のスタブコードを生成
     * @param siteId サイトID
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId);
    
    /**
     * @brief モノモーフィックメソッド呼び出し用のスタブコードを生成
     * @param cachePtr メソッドキャッシュへのポインタ
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cachePtr);
    
    /**
     * @brief ポリモーフィックメソッド呼び出し用のスタブコードを生成
     * @param cachePtr メソッドキャッシュへのポインタ
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cachePtr);
    
    /**
     * @brief メガモーフィックメソッド呼び出し用のスタブコードを生成
     * @param siteId サイトID
     * @return 生成されたネイティブコード
     */
    static std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId);
};

} // namespace core
} // namespace aerojs 