/**
 * @file cpu_features.cpp
 * @brief AeroJS JavaScript エンジンのCPU機能検出ヘルパーの実装
 * @version 1.0.0
 * @license MIT
 */

#include "cpu_features.h"
#include <cstring>
#include <thread>

#if defined(_WIN32)
  #include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
  #include <unistd.h>
  #include <sys/sysinfo.h>
  #include <fstream>
#endif

namespace aerojs {
namespace utils {

// 静的メンバ変数の初期化
uint64_t CPUFeatures::s_features = 0;
bool CPUFeatures::s_initialized = false;

// 初期化処理
void CPUFeatures::initialize() {
  if (s_initialized) {
    return;
  }
  
  // アーキテクチャに基づく機能検出
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
  detectX86Features();
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
  detectARMFeatures();
#elif defined(__riscv)
  detectRISCVFeatures();
#endif
  
  s_initialized = true;
}

// x86/x64アーキテクチャ向けのCPUID呼び出し
void CPUFeatures::cpuidex(int result[4], int eax, int ecx) {
#if defined(_MSC_VER)
  __cpuidex(result, eax, ecx);
#elif defined(__GNUC__) || defined(__clang__)
  __cpuid_count(eax, ecx, result[0], result[1], result[2], result[3]);
#else
  // CPUID命令をサポートしていない環境では全てゼロ
  result[0] = result[1] = result[2] = result[3] = 0;
#endif
}

// x86/x64向け機能検出
void CPUFeatures::detectX86Features() {
  int info[4];
  
  // CPUID 0: ベンダーID
  cpuidex(info, 0, 0);
  int maxStandardFunc = info[0];
  
  // CPUID 1: 基本機能フラグ
  if (maxStandardFunc >= 1) {
    cpuidex(info, 1, 0);
    
    // EDXレジスタのフラグ
    if (info[3] & (1 << 25)) s_features |= SSE;
    if (info[3] & (1 << 26)) s_features |= SSE2;
    
    // ECXレジスタのフラグ
    if (info[2] & (1 << 0))  s_features |= SSE3;
    if (info[2] & (1 << 9))  s_features |= SSSE3;
    if (info[2] & (1 << 19)) s_features |= SSE4_1;
    if (info[2] & (1 << 20)) s_features |= SSE4_2;
    if (info[2] & (1 << 28)) s_features |= AVX;
    if (info[2] & (1 << 12)) s_features |= FMA;
    if (info[2] & (1 << 25)) s_features |= AES;
    if (info[2] & (1 << 1))  s_features |= PCLMUL;
    if (info[2] & (1 << 23)) s_features |= POPCNT;
    if (info[2] & (1 << 29)) s_features |= F16C;
  }
  
  // CPUID 7: 拡張機能フラグ
  if (maxStandardFunc >= 7) {
    cpuidex(info, 7, 0);
    
    // EBXレジスタのフラグ
    if (info[1] & (1 << 5))  s_features |= AVX2;
    if (info[1] & (1 << 3))  s_features |= BMI1;
    if (info[1] & (1 << 8))  s_features |= BMI2;
    if (info[1] & (1 << 16)) s_features |= AVX512F;
    if (info[1] & (1 << 22)) s_features |= MOVBE;
  }
  
  // AMDの拡張機能（CPUID 0x80000001）
  cpuidex(info, 0x80000000, 0);
  int maxExtendedFunc = info[0];
  
  if (maxExtendedFunc >= 0x80000001) {
    cpuidex(info, 0x80000001, 0);
    
    // ECXレジスタのフラグ
    if (info[2] & (1 << 5)) s_features |= LZCNT;
  }
  
  // OSがXSAVEをサポートしていることを確認（AVX/AVX2使用に必須）
  if (s_features & AVX) {
    cpuidex(info, 1, 0);
    bool osUsesXSAVE_XRSTORE = (info[2] & (1 << 27)) != 0;
    
    if (!osUsesXSAVE_XRSTORE) {
      // OSがXSAVEをサポートしていない場合、AVX系は無効化
      s_features &= ~AVX;
      s_features &= ~AVX2;
      s_features &= ~AVX512F;
      s_features &= ~FMA;
    } else {
      // XCR0レジスタを確認（AVXステートが保存可能か）
      uint64_t xcrFeatureMask = 0;
#if defined(_MSC_VER)
      xcrFeatureMask = _xgetbv(0);
#elif defined(__GNUC__) || defined(__clang__)
      uint32_t eax, edx;
      __asm__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
      xcrFeatureMask = (static_cast<uint64_t>(edx) << 32) | eax;
#endif

      // XMMとYMMをサポートしていなければAVXは使用できない
      bool avxSupported = (xcrFeatureMask & 0x6) == 0x6;
      if (!avxSupported) {
        s_features &= ~AVX;
        s_features &= ~AVX2;
        s_features &= ~AVX512F;
        s_features &= ~FMA;
      }
      
      // XMMとYMMとOPMASKとZMM0-15とZMM16-31をサポートしていなければAVX-512は使用できない
      bool avx512Supported = (xcrFeatureMask & 0xE6) == 0xE6;
      if (!avx512Supported) {
        s_features &= ~AVX512F;
      }
    }
  }
}

// ARM向け機能検出
void CPUFeatures::detectARMFeatures() {
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
  s_features |= NEON;
#endif

#if defined(__ARM_FEATURE_SVE)
  s_features |= SVE;
#endif

  // 追加のARM機能検出はOSやハードウェア固有の方法が必要
}

// RISC-V向け機能検出
void CPUFeatures::detectRISCVFeatures() {
  // 現在はコンパイル時の定義に基づき、実行時検出は限定的
#if defined(__riscv_v)
  s_features |= RV_V;
#endif
}

// 利用可能なCPU機能をリストで取得
std::vector<std::string> CPUFeatures::getAvailableFeatures() {
  if (!s_initialized) {
    initialize();
  }
  
  std::vector<std::string> featureList;
  
  if (s_features & SSE)     featureList.push_back("SSE");
  if (s_features & SSE2)    featureList.push_back("SSE2");
  if (s_features & SSE3)    featureList.push_back("SSE3");
  if (s_features & SSSE3)   featureList.push_back("SSSE3");
  if (s_features & SSE4_1)  featureList.push_back("SSE4.1");
  if (s_features & SSE4_2)  featureList.push_back("SSE4.2");
  if (s_features & AVX)     featureList.push_back("AVX");
  if (s_features & AVX2)    featureList.push_back("AVX2");
  if (s_features & AVX512F) featureList.push_back("AVX-512F");
  if (s_features & FMA)     featureList.push_back("FMA");
  if (s_features & AES)     featureList.push_back("AES");
  if (s_features & PCLMUL)  featureList.push_back("PCLMUL");
  if (s_features & POPCNT)  featureList.push_back("POPCNT");
  if (s_features & BMI1)    featureList.push_back("BMI1");
  if (s_features & BMI2)    featureList.push_back("BMI2");
  if (s_features & LZCNT)   featureList.push_back("LZCNT");
  if (s_features & F16C)    featureList.push_back("F16C");
  if (s_features & MOVBE)   featureList.push_back("MOVBE");
  if (s_features & NEON)    featureList.push_back("NEON");
  if (s_features & SVE)     featureList.push_back("SVE");
  if (s_features & RV_V)    featureList.push_back("RISC-V Vector");
  
  return featureList;
}

// CPU名の取得
std::string CPUFeatures::getCPUName() {
  if (!s_initialized) {
    initialize();
  }
  
  char brand[49]; // CPUブランド文字列用（48文字+null終端）
  std::memset(brand, 0, sizeof(brand));
  
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
  // x86/x64アーキテクチャでのCPU名取得
  int info[4];
  
  // CPUID 0x80000000: 拡張機能の最大値
  cpuidex(info, 0x80000000, 0);
  
  if (static_cast<uint32_t>(info[0]) >= 0x80000004) {
    // CPUID 0x80000002-0x80000004: プロセッサ名文字列
    cpuidex(reinterpret_cast<int*>(brand), 0x80000002, 0);
    cpuidex(reinterpret_cast<int*>(brand) + 4, 0x80000003, 0);
    cpuidex(reinterpret_cast<int*>(brand) + 8, 0x80000004, 0);
    
    // 先頭の空白をスキップ
    char* p = brand;
    while (*p == ' ') {
      p++;
    }
    
    return std::string(p);
  }
#elif defined(__linux__)
  // Linuxでの取得方法
  try {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
      if (line.find("model name") != std::string::npos ||
          line.find("Processor") != std::string::npos) {
        size_t pos = line.find(':');
        if (pos != std::string::npos) {
          return line.substr(pos + 2);
        }
      }
    }
  } catch (...) {
    // エラー時はデフォルト値を返す
  }
#elif defined(__APPLE__)
  // macOSでの取得方法（簡易版）
  char buffer[128];
  size_t size = sizeof(buffer);
  if (sysctlbyname("machdep.cpu.brand_string", &buffer, &size, nullptr, 0) == 0) {
    return std::string(buffer);
  }
#endif
  
  return "Unknown CPU";
}

// ハードウェアスレッド数を取得
int CPUFeatures::getNumHardwareThreads() {
  // std::thread::hardware_concurrencyを使用（C++11以降）
  unsigned int numThreads = std::thread::hardware_concurrency();
  
  // 失敗した場合のフォールバック
  if (numThreads == 0) {
#if defined(_WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    numThreads = sysinfo.dwNumberOfProcessors;
#elif defined(__linux__)
    numThreads = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(__APPLE__)
    int nm[2];
    size_t len = 4;
    nm[0] = CTL_HW;
    nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &numThreads, &len, NULL, 0);
    
    if (numThreads < 1) {
      nm[1] = HW_NCPU;
      sysctl(nm, 2, &numThreads, &len, NULL, 0);
    }
#endif
  }
  
  return static_cast<int>(numThreads);
}

// キャッシュラインサイズを取得
int CPUFeatures::getCacheLineSize() {
  if (!s_initialized) {
    initialize();
  }
  
#if defined(__x86_64__) || defined(_M_X64) || defined(i386) || defined(_M_IX86)
  // x86/x64アーキテクチャでのキャッシュラインサイズ取得
  int info[4];
  cpuidex(info, 1, 0);
  
  // EBXレジスタからCLFLUSHラインサイズを抽出
  int cacheLineSize = ((info[1] >> 8) & 0xff) * 8;
  
  if (cacheLineSize > 0) {
    return cacheLineSize;
  }
#elif defined(__linux__)
  try {
    std::ifstream cpuinfo("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    int size = 0;
    cpuinfo >> size;
    if (size > 0) {
      return size;
    }
  } catch (...) {
    // エラー時はデフォルト値を返す
  }
#endif
  
  // デフォルト値（一般的なx86/ARMプロセッサでは64バイト）
  return 64;
}

// L1キャッシュサイズを取得（簡易実装）
int CPUFeatures::getL1CacheSize() {
  // 簡易実装：一般的な値を返す
  return 32 * 1024; // 32KB
}

// L2キャッシュサイズを取得（簡易実装）
int CPUFeatures::getL2CacheSize() {
  // 簡易実装：一般的な値を返す
  return 256 * 1024; // 256KB
}

// L3キャッシュサイズを取得（簡易実装）
int CPUFeatures::getL3CacheSize() {
  // 簡易実装：一般的な値を返す
  return 8 * 1024 * 1024; // 8MB
}

// 物理コア数を取得（簡易実装）
int CPUFeatures::getNumPhysicalCores() {
  // 簡易実装：論理コア数を返す
  return getNumHardwareThreads();
}

}  // namespace utils
}  // namespace aerojs 