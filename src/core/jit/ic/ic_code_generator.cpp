/**
 * @file ic_code_generator.cpp
 * @brief インラインキャッシュ用コード生成機能の実装
 * @version 1.0.0
 * @license MIT
 */

#include "ic_code_generator.h"
#include "arm64_ic_generator.h"
#include "riscv_ic_generator.h"
#include "x86_64_ic_generator.h"
#include "../../context.h"
#include "../code_cache.h"
#include <cassert>
#include <cstring>

namespace aerojs {
namespace core {

// JITコンパイラのコンストラクタを使用する場合
ICCodeGenerator::ICCodeGenerator(JITCompiler* compiler)
    : m_compiler(compiler)
    , _context(nullptr)
    , m_getCachedPropertyHelper(nullptr)
    , m_setCachedPropertyHelper(nullptr)
    , m_callCachedMethodHelper(nullptr) {
    
    // ネイティブアーキテクチャを検出
#if AEROJS_TARGET_X86_64
    _targetArch = ArchitectureType::X86_64;
#elif AEROJS_TARGET_ARM64
    _targetArch = ArchitectureType::ARM64;
#elif AEROJS_TARGET_RISCV
    _targetArch = ArchitectureType::RISCV;
#else
    #error "Unsupported architecture"
#endif
}

// コンテキストを使用する場合のコンストラクタ
ICCodeGenerator::ICCodeGenerator(Context* context)
    : m_compiler(nullptr)
    , _context(context)
    , m_getCachedPropertyHelper(nullptr)
    , m_setCachedPropertyHelper(nullptr)
    , m_callCachedMethodHelper(nullptr) {
    
    // ネイティブアーキテクチャを検出
#if AEROJS_TARGET_X86_64
    _targetArch = ArchitectureType::X86_64;
#elif AEROJS_TARGET_ARM64
    _targetArch = ArchitectureType::ARM64;
#elif AEROJS_TARGET_RISCV
    _targetArch = ArchitectureType::RISCV;
#else
    #error "Unsupported architecture"
#endif
}

ICCodeGenerator::~ICCodeGenerator() = default;

ArchitectureType ICCodeGenerator::getTargetArchitecture() const {
    return _targetArch;
}

// プロパティキャッシュに基づいたスタブコード生成
NativeCode* ICCodeGenerator::generatePropertyStub(PropertyCache* cache, uint64_t siteId) {
    if (!cache) {
        return nullptr;
    }
    
    // キャッシュ状態に応じたスタブコードを生成
    switch (cache->getState()) {
        case CacheEntry::State::Uninitialized:
            return generateUninitializedPropertyStub(siteId);
            
        case CacheEntry::State::Monomorphic:
            return generateMonomorphicPropertyStub(cache, siteId);
            
        case CacheEntry::State::Polymorphic:
            return generatePolymorphicPropertyStub(cache, siteId);
            
        case CacheEntry::State::Megamorphic:
            return generateMegamorphicPropertyStub(siteId);
            
        default:
            return nullptr;
    }
}

// メソッドキャッシュに基づいたスタブコード生成
NativeCode* ICCodeGenerator::generateMethodStub(MethodCache* cache, uint64_t siteId) {
    if (!cache) {
        return nullptr;
    }
    
    // キャッシュ状態に応じたスタブコードを生成
    switch (cache->getState()) {
        case CacheEntry::State::Uninitialized:
            return generateUninitializedMethodStub(siteId);
            
        case CacheEntry::State::Monomorphic:
            return generateMonomorphicMethodStub(cache, siteId);
            
        case CacheEntry::State::Polymorphic:
            return generatePolymorphicMethodStub(cache, siteId);
            
        case CacheEntry::State::Megamorphic:
            return generateMegamorphicMethodStub(siteId);
            
        default:
            return nullptr;
    }
}

// 未初期化状態のプロパティアクセススタブを生成
NativeCode* ICCodeGenerator::generateUninitializedPropertyStub(uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のスタブ生成
            code = X86_64_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用の未初期化プロパティアクセススタブを生成
            code = ARM64_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用の未初期化プロパティアクセススタブを生成
            code = RISCV_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// モノモーフィック状態のプロパティアクセススタブを生成
NativeCode* ICCodeGenerator::generateMonomorphicPropertyStub(PropertyCache* cache, uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のモノモーフィックプロパティアクセススタブを生成
            code = X86_64_ICGenerator::generateMonomorphicPropertyStub(cache);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のモノモーフィックプロパティアクセススタブを生成
            code = ARM64_ICGenerator::generateMonomorphicPropertyStub(cache);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のモノモーフィックプロパティアクセススタブを生成
            code = RISCV_ICGenerator::generateMonomorphicPropertyStub(cache);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// ポリモーフィック状態のプロパティアクセススタブを生成
NativeCode* ICCodeGenerator::generatePolymorphicPropertyStub(PropertyCache* cache, uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のポリモーフィックプロパティアクセススタブを生成
            code = X86_64_ICGenerator::generatePolymorphicPropertyStub(cache);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のポリモーフィックプロパティアクセススタブを生成
            code = ARM64_ICGenerator::generatePolymorphicPropertyStub(cache);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のポリモーフィックプロパティアクセススタブを生成
            code = RISCV_ICGenerator::generatePolymorphicPropertyStub(cache);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// メガモーフィック状態のプロパティアクセススタブを生成
NativeCode* ICCodeGenerator::generateMegamorphicPropertyStub(uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のメガモーフィックプロパティアクセススタブを生成
            code = X86_64_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のメガモーフィックプロパティアクセススタブを生成
            code = ARM64_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のメガモーフィックプロパティアクセススタブを生成
            code = RISCV_ICGenerator::generateMegamorphicPropertyStub(siteId);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// 未初期化状態のメソッド呼び出しスタブを生成
NativeCode* ICCodeGenerator::generateUninitializedMethodStub(uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用の未初期化メソッド呼び出しスタブを生成
            code = X86_64_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用の未初期化メソッド呼び出しスタブを生成
            // 実装がないため、メガモーフィックスタブを代わりに使用
            code = ARM64_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用の未初期化メソッド呼び出しスタブを生成
            // 実装がないため、メガモーフィックスタブを代わりに使用
            code = RISCV_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// モノモーフィック状態のメソッド呼び出しスタブを生成
NativeCode* ICCodeGenerator::generateMonomorphicMethodStub(MethodCache* cache, uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のモノモーフィックメソッド呼び出しスタブを生成
            code = X86_64_ICGenerator::generateMonomorphicMethodStub(cache);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のモノモーフィックメソッド呼び出しスタブを生成
            code = ARM64_ICGenerator::generateMonomorphicMethodStub(cache);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のモノモーフィックメソッド呼び出しスタブを生成
            code = RISCV_ICGenerator::generateMonomorphicMethodStub(cache);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// ポリモーフィック状態のメソッド呼び出しスタブを生成
NativeCode* ICCodeGenerator::generatePolymorphicMethodStub(MethodCache* cache, uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のポリモーフィックメソッド呼び出しスタブを生成
            code = X86_64_ICGenerator::generatePolymorphicMethodStub(cache);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のポリモーフィックメソッド呼び出しスタブを生成
            code = ARM64_ICGenerator::generatePolymorphicMethodStub(cache);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のポリモーフィックメソッド呼び出しスタブを生成
            code = RISCV_ICGenerator::generatePolymorphicMethodStub(cache);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

// メガモーフィック状態のメソッド呼び出しスタブを生成
NativeCode* ICCodeGenerator::generateMegamorphicMethodStub(uint64_t siteId) {
    // アーキテクチャごとの実装を呼び出す
    std::unique_ptr<NativeCode> code;
    
    switch (_targetArch) {
        case ArchitectureType::X86_64:
            // X86_64用のメガモーフィックメソッド呼び出しスタブを生成
            code = X86_64_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
            
        case ArchitectureType::ARM64:
            // ARM64用のメガモーフィックメソッド呼び出しスタブを生成
            code = ARM64_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
            
        case ArchitectureType::RISCV:
            // RISC-V用のメガモーフィックメソッド呼び出しスタブを生成
            code = RISCV_ICGenerator::generateMegamorphicMethodStub(siteId);
            break;
    }
    
    if (code) {
        // 実行可能にする
        code->buffer.makeExecutable();
        
        // 結果をコードキャッシュに登録して返す
        if (_context) {
            CodeCache* codeCache = _context->getCodeCache();
            return codeCache->registerCode(std::move(code));
        }
    }
    
    return nullptr;
}

} // namespace core
} // namespace aerojs 