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

}  // namespace core
}  // namespace aerojs