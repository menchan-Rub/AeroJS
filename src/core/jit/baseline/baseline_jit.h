/**
 * @file baseline_jit.h
 * @brief 高速なベースラインJITコンパイラ
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include "../jit_compiler.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <string>

namespace aerojs {

// 前方宣言
class Context;
class Function;
class BytecodeEmitter;
class CodeGenerator;
class NativeCode;

/**
 * @brief ベースラインJITコンパイラ
 *
 * 最適化を行わない単純で高速なJITコンパイラ
 */
class BaselineJIT : public JITCompiler {
public:
    // コンストラクタ
    explicit BaselineJIT(Context* context);
    
    // デストラクタ
    ~BaselineJIT() override;
    
    // JITCompilerインターフェース実装
    bool compile(Function* function) override;
    void* getCompiledCode(uint64_t functionId) override;
    bool hasCompiledCode(uint64_t functionId) const override;
    
    // 統計情報
    struct Stats {
        size_t compiledFunctions = 0;
        size_t totalCompilationTimeMs = 0;
        size_t totalCodeSize = 0;
        size_t averageCodeSize = 0;
    };
    
    const Stats& getStats() const { return _stats; }
    
private:
    // コンポーネント
    Context* _context;
    std::unique_ptr<BytecodeEmitter> _emitter;
    std::unique_ptr<CodeGenerator> _codeGenerator;
    
    // コード管理
    std::unordered_map<uint64_t, NativeCode*> _codeMap;
    
    // バイトコード変換
    bool generateBytecodeLowering(Function* function, std::vector<uint8_t>& outputBuffer);
    
    // ネイティブコード生成
    NativeCode* generateNativeCode(Function* function, const std::vector<uint8_t>& bytecode);
    
    // IC（インラインキャッシュ）管理
    void setupInlineCaches(NativeCode* code, Function* function);
    
    // メモリ管理
    void freeCompiledCode(uint64_t functionId);
    
    // 統計情報
    Stats _stats;
    
    // プロファイリングサポート
    void emitProfilingHooks(Function* function, std::vector<uint8_t>& buffer);
};

} // namespace aerojs