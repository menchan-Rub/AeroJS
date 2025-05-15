/**
 * @file jit_compiler.cpp
 * @brief JITコンパイラのための基本インターフェースの実装
 * @version 1.0.0
 * @license MIT
 */

#include "jit_compiler.h"
#include "baseline/baseline_jit.h"
#include "optimizing/optimizing_jit.h"
#include "metatracing/tracing_jit.h"
#include "../context.h"

namespace aerojs {

JITCompiler* JITCompiler::create(Context* context, CompilerType type) {
    if (!context) {
        return nullptr;
    }
    
    switch (type) {
        case CompilerType::Baseline:
            return new BaselineJIT(context);
            
        case CompilerType::Optimizing:
            return new OptimizingJIT(context);
            
        case CompilerType::Tracing:
            return new TracingJIT(context);
            
        default:
            return nullptr;
    }
}

} // namespace aerojs 