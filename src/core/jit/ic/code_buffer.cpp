/**
 * @file code_buffer.cpp
 * @brief ネイティブコードバッファの実装
 * @version 1.0.0
 * @license MIT
 */

#include "inline_cache.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aerojs {
namespace core {

bool CodeBuffer::reserve(size_t capacity) {
    // 既に同じサイズが確保されていたら何もしない
    if (_buffer && _capacity >= capacity) {
        return true;
    }
    
    // 既存のバッファがあれば解放
    if (_buffer) {
        release();
    }
    
    // 新しいバッファを確保
#ifdef _WIN32
    // Windowsではメモリ確保とページ属性設定を分離
    _buffer = static_cast<uint8_t*>(VirtualAlloc(nullptr, capacity, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!_buffer) {
        return false;
    }
#else
    // UNIXシステムでは匿名メモリマッピングを使用
    // 現時点では読み書き可能なメモリとして確保
    _buffer = static_cast<uint8_t*>(mmap(nullptr, capacity, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (_buffer == MAP_FAILED) {
        _buffer = nullptr;
        return false;
    }
#endif
    
    _capacity = capacity;
    _size = 0;
    _executable = false;
    
    return true;
}

void CodeBuffer::emit8(uint8_t value) {
    if (_size + 1 > _capacity) {
        size_t newCapacity = _capacity ? _capacity * 2 : 64;
        reserve(newCapacity);
    }
    
    _buffer[_size++] = value;
}

void CodeBuffer::emit16(uint16_t value) {
    emit8(static_cast<uint8_t>(value & 0xFF));
    emit8(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void CodeBuffer::emit32(uint32_t value) {
    emit16(static_cast<uint16_t>(value & 0xFFFF));
    emit16(static_cast<uint16_t>((value >> 16) & 0xFFFF));
}

void CodeBuffer::emit64(uint64_t value) {
    emit32(static_cast<uint32_t>(value & 0xFFFFFFFF));
    emit32(static_cast<uint32_t>((value >> 32) & 0xFFFFFFFF));
}

void CodeBuffer::emitPtr(void* ptr) {
    emit64(reinterpret_cast<uint64_t>(ptr));
}

void CodeBuffer::emitBytes(const void* data, size_t length) {
    if (_size + length > _capacity) {
        size_t newCapacity = _capacity ? _capacity * 2 : 64;
        while (newCapacity < _size + length) {
            newCapacity *= 2;
        }
        reserve(newCapacity);
    }
    
    std::memcpy(_buffer + _size, data, length);
    _size += length;
}

bool CodeBuffer::makeExecutable() {
    if (!_buffer || _size == 0) {
        return false;
    }
    
    // バッファが既に実行可能な場合は何もしない
    if (_executable) {
        return true;
    }
    
#ifdef _WIN32
    // Windowsではページ属性を変更
    DWORD oldProtect;
    if (!VirtualProtect(_buffer, _size, PAGE_EXECUTE_READ, &oldProtect)) {
        return false;
    }
    
    // Windowsでのキャッシュフラッシュ（必要な場合）
    FlushInstructionCache(GetCurrentProcess(), _buffer, _size);
#else
    // UNIXシステムではメモリ保護属性を変更
    if (mprotect(_buffer, _size, PROT_READ | PROT_EXEC) != 0) {
        return false;
    }
#endif
    
    // メモリキャッシュのフラッシュ（アーキテクチャに依存）
#if defined(__x86_64__) || defined(_M_X64)
    // x86_64ではCPUキャッシュのフラッシュは不要（命令キャッシュは自動的に更新される）
#elif defined(__aarch64__) || defined(_M_ARM64)
    // ARM64ではキャッシュフラッシュが必要
    for (size_t i = 0; i < _size; i += 64) {
        size_t remainingSize = _size - i;
        size_t flushSize = remainingSize < 64 ? remainingSize : 64;
        __builtin___clear_cache(
            reinterpret_cast<char*>(_buffer + i),
            reinterpret_cast<char*>(_buffer + i + flushSize)
        );
    }
#elif defined(__riscv)
    // RISC-Vではキャッシュフラッシュが必要
    // 命令キャッシュをフラッシュするためのフェンス
    __asm__ volatile("fence.i" ::: "memory");
    
    // データキャッシュをフラッシュするための追加のフェンス（実装依存）
    __asm__ volatile("fence rw, rw" ::: "memory");
#endif
    
    _executable = true;
    return true;
}

void CodeBuffer::release() {
    if (!_buffer) {
        return;
    }
    
#ifdef _WIN32
    // Windowsではメモリを解放
    VirtualFree(_buffer, 0, MEM_RELEASE);
#else
    // UNIXシステムではメモリマッピングを解除
    munmap(_buffer, _capacity);
#endif
    
    _buffer = nullptr;
    _size = 0;
    _capacity = 0;
    _executable = false;
}

} // namespace core
} // namespace aerojs 