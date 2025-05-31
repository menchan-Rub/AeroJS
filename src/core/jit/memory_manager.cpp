#include "memory_manager.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aerojs {
namespace core {

// 従来のレガシー関数の実装
void* allocate_executable_memory(const void* code, size_t size) noexcept {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  size_t pageSize = si.dwPageSize;
  size_t totalSize = size + 2 * pageSize;
  // まず読み書き用に確保
  void* base = VirtualAlloc(nullptr, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!base) return nullptr;
  // コード領域
  void* codePtr = reinterpret_cast<uint8_t*>(base) + pageSize;
  std::memcpy(codePtr, code, size);
  // ガードページ設定
  DWORD old;
  VirtualProtect(base, pageSize, PAGE_NOACCESS, &old);
  VirtualProtect(reinterpret_cast<uint8_t*>(base) + pageSize + size, pageSize, PAGE_NOACCESS, &old);
  // コードページを実行可能に設定
  if (!VirtualProtect(codePtr, size, PAGE_EXECUTE_READ, &old)) {
    VirtualFree(base, 0, MEM_RELEASE);
    return nullptr;
  }
  FlushInstructionCache(GetCurrentProcess(), codePtr, size);
  return codePtr;
#else
  long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) return nullptr;
  size_t allocSize = (size + pageSize - 1) & ~(pageSize - 1);
  size_t totalSize = allocSize + 2 * pageSize;
  // 読み書き用に確保
  void* base = mmap(nullptr, totalSize,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (base == MAP_FAILED) return nullptr;
  // コード領域
  void* codePtr = reinterpret_cast<uint8_t*>(base) + pageSize;
  std::memcpy(codePtr, code, size);
  // ガードページ設定
  mprotect(base, pageSize, PROT_NONE);
  mprotect(reinterpret_cast<uint8_t*>(codePtr) + allocSize, pageSize, PROT_NONE);
  // コードページ実行権限のみ設定
  if (mprotect(codePtr, allocSize, PROT_READ | PROT_EXEC) != 0) {
    munmap(base, totalSize);
    return nullptr;
  }
  __builtin___clear_cache(reinterpret_cast<char*>(codePtr), reinterpret_cast<char*>(codePtr) + size);
  return codePtr;
#endif
}

void free_executable_memory(void* ptr, size_t size) noexcept {
#ifdef _WIN32
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  size_t pageSize = si.dwPageSize;
  // base の算出
  void* base = reinterpret_cast<uint8_t*>(ptr) - pageSize;
  VirtualFree(base, 0, MEM_RELEASE);
#else
  long pageSize = sysconf(_SC_PAGESIZE);
  if (pageSize <= 0) return;
  size_t allocSize = (size + pageSize - 1) & ~(pageSize - 1);
  size_t totalSize = allocSize + 2 * pageSize;
  void* base = reinterpret_cast<uint8_t*>(ptr) - pageSize;
  munmap(base, totalSize);
#endif
}

// 新しいMemoryManagerクラスの実装
MemoryManager::MemoryManager() : m_totalAllocatedMemory(0) {}

MemoryManager::~MemoryManager() {
    // 確保したすべてのメモリ領域を解放
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [ptr, region] : m_memoryRegions) {
        #ifdef _WIN32
        VirtualFree(region.baseAddress, 0, MEM_RELEASE);
        #else
        munmap(region.baseAddress, region.size);
        #endif
    }
    m_memoryRegions.clear();
    m_totalAllocatedMemory = 0;
}

size_t MemoryManager::getPageSize() {
    #ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
    #else
    long pageSize = sysconf(_SC_PAGESIZE);
    return pageSize > 0 ? static_cast<size_t>(pageSize) : 4096;
    #endif
}

size_t MemoryManager::alignToPageSize(size_t size) {
    size_t pageSize = getPageSize();
    return (size + pageSize - 1) & ~(pageSize - 1);
}

void* MemoryManager::allocateExecutableMemory(size_t size) {
    // サイズをページ境界に合わせる
    size_t alignedSize = alignToPageSize(size);
    
    // ガードページを含めた領域を確保
    size_t pageSize = getPageSize();
    size_t totalSize = alignedSize + 2 * pageSize; // 前後にガードページ

    // メモリ確保
    void* base = nullptr;
    #ifdef _WIN32
    base = VirtualAlloc(nullptr, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!base) return nullptr;
    #else
    base = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) return nullptr;
    #endif

    // コード領域のポインタ計算（ガードページの後）
    void* codePtr = reinterpret_cast<uint8_t*>(base) + pageSize;

    // ガードページの設定
    #ifdef _WIN32
    DWORD oldProtect;
    VirtualProtect(base, pageSize, PAGE_NOACCESS, &oldProtect);
    VirtualProtect(reinterpret_cast<uint8_t*>(codePtr) + alignedSize, pageSize, PAGE_NOACCESS, &oldProtect);
    #else
    mprotect(base, pageSize, PROT_NONE);
    mprotect(reinterpret_cast<uint8_t*>(codePtr) + alignedSize, pageSize, PROT_NONE);
    #endif

    // 領域の登録
    std::lock_guard<std::mutex> lock(m_mutex);
    MemoryRegion region{base, totalSize, MemoryProtection::ReadWrite};
    m_memoryRegions[codePtr] = region;
    m_totalAllocatedMemory += totalSize;

    return codePtr;
}

bool MemoryManager::protectMemory(void* ptr, size_t size, MemoryProtection protection) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // 登録済みのメモリ領域かチェック
    auto it = m_memoryRegions.find(ptr);
    if (it == m_memoryRegions.end()) {
        return false;
    }
    
    // アライメント済みサイズの計算
    size_t alignedSize = alignToPageSize(size);
    
    #ifdef _WIN32
    DWORD protectFlags;
    switch (protection) {
        case MemoryProtection::NoAccess:
            protectFlags = PAGE_NOACCESS;
            break;
        case MemoryProtection::ReadOnly:
            protectFlags = PAGE_READONLY;
            break;
        case MemoryProtection::ReadWrite:
            protectFlags = PAGE_READWRITE;
            break;
        case MemoryProtection::ReadExecute:
            protectFlags = PAGE_EXECUTE_READ;
            break;
        case MemoryProtection::ReadWriteExecute:
            protectFlags = PAGE_EXECUTE_READWRITE;
            break;
        default:
            return false;
    }
    
    DWORD oldProtect;
    bool result = VirtualProtect(ptr, alignedSize, protectFlags, &oldProtect) != 0;
    
    if (result && (protection == MemoryProtection::ReadExecute || 
                  protection == MemoryProtection::ReadWriteExecute)) {
        FlushInstructionCache(GetCurrentProcess(), ptr, alignedSize);
    }
    #else
    int protectFlags;
    switch (protection) {
        case MemoryProtection::NoAccess:
            protectFlags = PROT_NONE;
            break;
        case MemoryProtection::ReadOnly:
            protectFlags = PROT_READ;
            break;
        case MemoryProtection::ReadWrite:
            protectFlags = PROT_READ | PROT_WRITE;
            break;
        case MemoryProtection::ReadExecute:
            protectFlags = PROT_READ | PROT_EXEC;
            break;
        case MemoryProtection::ReadWriteExecute:
            protectFlags = PROT_READ | PROT_WRITE | PROT_EXEC;
            break;
        default:
            return false;
    }
    
    bool result = mprotect(ptr, alignedSize, protectFlags) == 0;
    
    if (result && (protection == MemoryProtection::ReadExecute || 
                  protection == MemoryProtection::ReadWriteExecute)) {
        __builtin___clear_cache(reinterpret_cast<char*>(ptr), 
                               reinterpret_cast<char*>(ptr) + alignedSize);
    }
    #endif
    
    if (result) {
        it->second.currentProtection = protection;
    }
    
    return result;
}

bool MemoryManager::freeMemory(void* ptr) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_memoryRegions.find(ptr);
    if (it == m_memoryRegions.end()) {
        return false;
    }
    
    const MemoryRegion& region = it->second;
    
    #ifdef _WIN32
    bool result = VirtualFree(region.baseAddress, 0, MEM_RELEASE) != 0;
    #else
    bool result = munmap(region.baseAddress, region.size) == 0;
    #endif
    
    if (result) {
        m_totalAllocatedMemory -= region.size;
        m_memoryRegions.erase(it);
    }
    
    return result;
}

void MemoryManager::flushInstructionCache(void* ptr, size_t size) {
    #ifdef _WIN32
    FlushInstructionCache(GetCurrentProcess(), ptr, size);
    #else
    __builtin___clear_cache(reinterpret_cast<char*>(ptr), 
                           reinterpret_cast<char*>(ptr) + size);
    #endif
}

size_t MemoryManager::getTotalAllocatedMemory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_totalAllocatedMemory;
}

}  // namespace core
}  // namespace aerojs