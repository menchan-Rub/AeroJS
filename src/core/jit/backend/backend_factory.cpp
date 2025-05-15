/**
 * @file backend_factory.cpp
 * @brief AeroJS JavaScriptエンジン用バックエンドファクトリの世界最高実装
 * @version 1.0.0
 * @license MIT
 */

#include "backend_factory.h"
#include "core/jit/backend/arm64/arm64_backend.h"
#include "core/jit/backend/x86_64/x86_64_backend.h"
#include "core/jit/backend/riscv/riscv_backend.h"
#include "core/jit/backend/interpreter/interpreter_backend.h"
#include <algorithm>
#include <cctype>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#if defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/machine.h>
#include <sys/types.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#include <fstream>
#include <string>
#include <thread>
#endif

namespace aerojs {
namespace core {

// 世界最高性能CPUベンダー判別
enum class CPUVendor {
    Unknown,
    Intel,
    AMD,
    ARM,
    Apple,
    Qualcomm,
    Samsung,
    Ampere,
    Fujitsu,
    Huawei,
    RISC_V
};

// 詳細なCPU情報構造体
struct CPUInfo {
    std::string name;
    std::string architecture;
    std::string vendor;
    int cores;
    int threads;
    int l1CacheSize;
    int l2CacheSize;
    int l3CacheSize;
    int cacheLine;
    bool littleEndian;
    std::vector<std::string> features;
    
    CPUVendor vendorEnum;
    
    CPUInfo() : cores(1), threads(1), l1CacheSize(0), l2CacheSize(0), 
                l3CacheSize(0), cacheLine(64), littleEndian(true),
                vendorEnum(CPUVendor::Unknown) {}
};

// 世界最高のバックエンド自動選択
std::unique_ptr<Backend> BackendFactory::createBackend(Context* context, JITProfiler* profiler) {
    // 最高性能CPUアーキテクチャを検出
    CPUInfo cpuInfo = detectCPUInfo();
    
    // アーキテクチャに基づいた最適なバックエンドを選択
    if (cpuInfo.architecture == "arm64" || cpuInfo.architecture == "aarch64") {
        return createOptimizedARM64Backend(context, profiler, cpuInfo);
    }
    else if (cpuInfo.architecture == "x86_64" || cpuInfo.architecture == "amd64") {
        return createOptimizedX86_64Backend(context, profiler, cpuInfo);
    }
    else if (cpuInfo.architecture.find("riscv") != std::string::npos) {
        return createOptimizedRISCVBackend(context, profiler, cpuInfo);
    }
    
    // サポートされないアーキテクチャの場合はインタプリタにフォールバック
    return createInterpreterBackend(context);
}

std::unique_ptr<Backend> BackendFactory::createBackendForArchitecture(
    const std::string& archName,
    Context* context,
    JITProfiler* profiler) {
    
    // 大文字小文字を区別しないための正規化
    std::string normalizedArch = archName;
    std::transform(normalizedArch.begin(), normalizedArch.end(), normalizedArch.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    // CPU情報検出
    CPUInfo cpuInfo = detectCPUInfo();
    cpuInfo.architecture = normalizedArch; // ユーザー指定を優先
    
    // 最適なバックエンドを作成
    if (normalizedArch == "x86_64" || normalizedArch == "amd64" || normalizedArch == "x64") {
        return createOptimizedX86_64Backend(context, profiler, cpuInfo);
    }
    else if (normalizedArch == "arm64" || normalizedArch == "aarch64") {
        return createOptimizedARM64Backend(context, profiler, cpuInfo);
    }
    else if (normalizedArch == "riscv" || normalizedArch == "riscv64") {
        return createOptimizedRISCVBackend(context, profiler, cpuInfo);
    }
    else if (normalizedArch == "interpreter") {
        // 明示的にインタプリタが指定された場合
        return createInterpreterBackend(context);
    }
    
    // サポートされないアーキテクチャの場合はインタプリタにフォールバック
    return createInterpreterBackend(context);
}

std::vector<std::string> BackendFactory::getAvailableBackends() {
    std::vector<std::string> backends;
    
    // 対応バックエンドをビルド設定に基づいて追加
#if defined(AEROJS_ENABLE_X86_64)
    backends.push_back("x86_64");
#endif

#if defined(AEROJS_ENABLE_ARM64)
    backends.push_back("arm64");
#endif

#if defined(AEROJS_ENABLE_RISCV)
    backends.push_back("riscv64");
#endif
    
    // インタプリタは常に利用可能
    backends.push_back("interpreter");
    
    return backends;
}

std::string BackendFactory::detectCPUArchitecture() {
    CPUInfo cpuInfo = detectCPUInfo();
    return cpuInfo.architecture;
}

// 詳細なCPU情報検出
CPUInfo BackendFactory::detectCPUInfo() {
    CPUInfo info;
    
#if defined(_WIN32)
    // Windows環境でのCPU検出
    SYSTEM_INFO sysInfo;
    GetNativeSystemInfo(&sysInfo);
    
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            info.architecture = "x86_64";
            break;
        case PROCESSOR_ARCHITECTURE_ARM64:
            info.architecture = "arm64";
            break;
        default:
            info.architecture = "unknown";
            break;
    }
    
    // コア数取得
    info.cores = sysInfo.dwNumberOfProcessors;
    info.threads = sysInfo.dwNumberOfProcessors;
    
    // ベンダー情報取得
    char vendorString[13];
    int cpuInfo[4] = {0};
    
    __cpuid(cpuInfo, 0);
    memcpy(vendorString, &cpuInfo[1], 4);
    memcpy(vendorString + 4, &cpuInfo[3], 4);
    memcpy(vendorString + 8, &cpuInfo[2], 4);
    vendorString[12] = '\0';
    
    info.vendor = vendorString;
    if (info.vendor == "GenuineIntel") {
        info.vendorEnum = CPUVendor::Intel;
    } else if (info.vendor == "AuthenticAMD") {
        info.vendorEnum = CPUVendor::AMD;
    }
    
    // CPU名取得
    char cpuBrand[49];
    memset(cpuBrand, 0, sizeof(cpuBrand));
    
    __cpuid(cpuInfo, 0x80000000);
    if (cpuInfo[0] >= 0x80000004) {
        __cpuid(cpuInfo, 0x80000002);
        memcpy(cpuBrand, cpuInfo, sizeof(cpuInfo));
        
        __cpuid(cpuInfo, 0x80000003);
        memcpy(cpuBrand + 16, cpuInfo, sizeof(cpuInfo));
        
        __cpuid(cpuInfo, 0x80000004);
        memcpy(cpuBrand + 32, cpuInfo, sizeof(cpuInfo));
        
        info.name = cpuBrand;
    }
    
    // ARM64の詳細情報取得（Windows 11以降）
    if (info.architecture == "arm64") {
        // Windows ARM64ではQualcommが一般的
        info.vendorEnum = CPUVendor::Qualcomm;
        
        // キャッシュラインサイズはARMv8で64バイトが一般的
        info.cacheLine = 64;
        
        // NEON/SIMDは常にサポート
        info.features.push_back("neon");
        info.features.push_back("simd");
    }
    
#elif defined(__APPLE__)
    // macOS環境での検出
    char buffer[256];
    size_t size = sizeof(buffer);
    
    // アーキテクチャ取得
    if (sysctlbyname("hw.machine", buffer, &size, nullptr, 0) == 0) {
        info.architecture = buffer;
        
        if (info.architecture == "arm64") {
            info.vendorEnum = CPUVendor::Apple;
            info.vendor = "Apple";
        } else if (info.architecture == "x86_64") {
            // Intelモデル名取得
            size = sizeof(buffer);
            if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) {
                info.name = buffer;
                
                if (info.name.find("Intel") != std::string::npos) {
                    info.vendorEnum = CPUVendor::Intel;
                    info.vendor = "Intel";
                } else if (info.name.find("AMD") != std::string::npos) {
                    info.vendorEnum = CPUVendor::AMD;
                    info.vendor = "AMD";
                }
            }
        }
    }
    
    // コア数とスレッド数
    int cores;
    size = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) == 0) {
        info.cores = cores;
    }
    
    int threads;
    size = sizeof(threads);
    if (sysctlbyname("hw.logicalcpu", &threads, &size, nullptr, 0) == 0) {
        info.threads = threads;
    }
    
    // キャッシュ情報
    int l1d;
    size = sizeof(l1d);
    if (sysctlbyname("hw.l1dcachesize", &l1d, &size, nullptr, 0) == 0) {
        info.l1CacheSize = l1d;
    }
    
    int l2;
    size = sizeof(l2);
    if (sysctlbyname("hw.l2cachesize", &l2, &size, nullptr, 0) == 0) {
        info.l2CacheSize = l2;
    }
    
    int l3;
    size = sizeof(l3);
    if (sysctlbyname("hw.l3cachesize", &l3, &size, nullptr, 0) == 0) {
        info.l3CacheSize = l3;
    }
    
    // ARM64固有の機能検出
    if (info.architecture == "arm64") {
        // Apple Siliconの詳細検出
        size = sizeof(buffer);
        if (sysctlbyname("machdep.cpu.brand_string", buffer, &size, nullptr, 0) == 0) {
            info.name = buffer;
            
            // Apple固有の機能
            info.features.push_back("neon");
            info.features.push_back("simd");
            info.features.push_back("crypto");
            info.features.push_back("crc32");
            
            // M1/M2/M3判別
            if (info.name.find("M1") != std::string::npos) {
                info.features.push_back("apple-m1");
                info.cacheLine = 128;  // Apple M1は128バイトキャッシュライン
            } else if (info.name.find("M2") != std::string::npos) {
                info.features.push_back("apple-m2");
                info.features.push_back("bf16");
                info.cacheLine = 128;
            } else if (info.name.find("M3") != std::string::npos) {
                info.features.push_back("apple-m3");
                info.features.push_back("bf16");
                info.cacheLine = 128;
            }
        }
    }
    
#elif defined(__linux__)
    // Linux環境での検出
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        std::string machine(unameData.machine);
        
        if (machine == "x86_64") {
            info.architecture = "x86_64";
        } else if (machine == "aarch64") {
            info.architecture = "arm64";
        } else if (machine.find("riscv") != std::string::npos) {
            info.architecture = "riscv64";
        } else {
            info.architecture = machine;
        }
    }
    
    // /proc/cpuinfoから詳細情報取得
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        int coreCount = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (info.architecture == "x86_64") {
                // x86_64固有の情報
                if (line.find("vendor_id") != std::string::npos) {
                    size_t pos = line.find(":");
                    if (pos != std::string::npos) {
                        info.vendor = line.substr(pos + 1);
                        info.vendor.erase(0, info.vendor.find_first_not_of(" \t"));
                        
                        if (info.vendor.find("Intel") != std::string::npos) {
                            info.vendorEnum = CPUVendor::Intel;
                        } else if (info.vendor.find("AMD") != std::string::npos) {
                            info.vendorEnum = CPUVendor::AMD;
                        }
                    }
                } else if (line.find("model name") != std::string::npos) {
                    size_t pos = line.find(":");
                    if (pos != std::string::npos) {
                        info.name = line.substr(pos + 1);
                        info.name.erase(0, info.name.find_first_not_of(" \t"));
                    }
                }
            } else if (info.architecture == "arm64") {
                // ARM64固有の情報
                if (line.find("CPU implementer") != std::string::npos) {
                    size_t pos = line.find(":");
                    if (pos != std::string::npos) {
                        std::string impl = line.substr(pos + 1);
                        impl.erase(0, impl.find_first_not_of(" \t"));
                        
                        // ベンダー判定
                        if (impl == "0x41") {
                            info.vendor = "ARM";
                            info.vendorEnum = CPUVendor::ARM;
                        } else if (impl == "0x51") {
                            info.vendor = "Qualcomm";
                            info.vendorEnum = CPUVendor::Qualcomm;
                        } else if (impl == "0x53") {
                            info.vendor = "Samsung";
                            info.vendorEnum = CPUVendor::Samsung;
                        } else if (impl == "0xc0") {
                            info.vendor = "Ampere";
                            info.vendorEnum = CPUVendor::Ampere;
                        } else if (impl == "0x46") {
                            info.vendor = "Fujitsu";
                            info.vendorEnum = CPUVendor::Fujitsu;
                        } else if (impl == "0x48") {
                            info.vendor = "HiSilicon";
                            info.vendorEnum = CPUVendor::Huawei;
                        }
                    }
                } else if (line.find("Processor") != std::string::npos ||
                           line.find("model name") != std::string::npos) {
                    size_t pos = line.find(":");
                    if (pos != std::string::npos) {
                        info.name = line.substr(pos + 1);
                        info.name.erase(0, info.name.find_first_not_of(" \t"));
                    }
                }
                
                // ARM64は常にNEON/SIMDをサポート
                info.features.push_back("neon");
                info.features.push_back("simd");
            } else if (info.architecture == "riscv64") {
                // RISC-V固有の情報
                if (line.find("processor") != std::string::npos) {
                    coreCount++;
                }
                info.vendorEnum = CPUVendor::RISC_V;
                info.vendor = "RISC-V";
            }
            
            // 共通情報
            if (line.find("processor") != std::string::npos) {
                coreCount++;
            }
        }
        
        if (coreCount > 0) {
            info.cores = coreCount;
            // スレッド数は単純化のためコア数と同じに設定
            info.threads = coreCount;
        }
    }
    
    // キャッシュ情報
    std::ifstream cacheinfo("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if (cacheinfo.is_open()) {
        std::string line;
        if (std::getline(cacheinfo, line)) {
            info.cacheLine = std::stoi(line);
        }
    }
    
    // スレッド数取得
    info.threads = std::thread::hardware_concurrency();
    if (info.threads == 0) {
        info.threads = info.cores;
    }
#else
    // その他の環境ではプリプロセッサマクロから判断
#if defined(__arm64__) || defined(__aarch64__)
    info.architecture = "arm64";
    info.features.push_back("neon");
    info.features.push_back("simd");
#elif defined(__x86_64__) || defined(_M_X64)
    info.architecture = "x86_64";
#elif defined(__riscv) && (__riscv_xlen == 64)
    info.architecture = "riscv64";
    info.vendorEnum = CPUVendor::RISC_V;
#else
    info.architecture = "unknown";
#endif

    // スレッド数取得
    info.threads = std::thread::hardware_concurrency();
    info.cores = info.threads;
#endif

    return info;
}

bool BackendFactory::isArchitectureSupported(const std::string& archName) {
    std::string normalizedArch = archName;
    std::transform(normalizedArch.begin(), normalizedArch.end(), normalizedArch.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    if (normalizedArch == "x86_64" || normalizedArch == "amd64" || normalizedArch == "x64") {
#if defined(AEROJS_ENABLE_X86_64)
        return true;
#else
        return false;
#endif
    } else if (normalizedArch == "arm64" || normalizedArch == "aarch64") {
#if defined(AEROJS_ENABLE_ARM64)
        return true;
#else
        return false;
#endif
    } else if (normalizedArch == "riscv" || normalizedArch == "riscv64") {
#if defined(AEROJS_ENABLE_RISCV)
        return true;
#else
        return false;
#endif
    } else if (normalizedArch == "interpreter") {
        return true;
    }
    
    return false;
}

// CPU情報に基づいた最適なバックエンド作成（ARM64）
std::unique_ptr<Backend> BackendFactory::createOptimizedARM64Backend(
    Context* context, JITProfiler* profiler, const CPUInfo& cpuInfo) {
#if defined(AEROJS_ENABLE_ARM64)
    auto backend = std::make_unique<arm64::ARM64Backend>(context, profiler);
    
    if (backend) {
        // CPU固有の最適化ヒント設定
        if (cpuInfo.vendorEnum == CPUVendor::Apple) {
            backend->setAppleSiliconOptimizations(true);
            
            // キャッシュラインサイズ設定
            backend->setCacheLineSize(cpuInfo.cacheLine);
            
            // Apple M1以降のプロセッサ向け特殊最適化
            if (std::find(cpuInfo.features.begin(), cpuInfo.features.end(), "apple-m1") != cpuInfo.features.end() ||
                std::find(cpuInfo.features.begin(), cpuInfo.features.end(), "apple-m2") != cpuInfo.features.end() ||
                std::find(cpuInfo.features.begin(), cpuInfo.features.end(), "apple-m3") != cpuInfo.features.end()) {
                backend->setAppleSiliconGeneration(cpuInfo.name.find("M1") != std::string::npos ? 1 : 
                                                 cpuInfo.name.find("M2") != std::string::npos ? 2 : 3);
            }
        }
        else if (cpuInfo.vendorEnum == CPUVendor::Qualcomm) {
            backend->setSnapdragonOptimizations(true);
            
            // Snapdragon固有の最適化
            if (cpuInfo.name.find("888") != std::string::npos || 
                cpuInfo.name.find("8 Gen") != std::string::npos) {
                backend->setSnapdragonGeneration(8);
            }
        }
        else if (cpuInfo.vendorEnum == CPUVendor::Ampere) {
            // Ampere Altra向け最適化
            backend->setServerClassOptimizations(true);
        }
        
        // 共通の最適化設定
        if (cpuInfo.threads > 4) {
            backend->setConcurrentCompilationThreads(std::min(4, cpuInfo.threads / 2));
        }
        
        // 初期化
        if (backend->initialize()) {
            return backend;
        }
    }
#endif
    
    // 失敗したらインタプリタにフォールバック
    return createInterpreterBackend(context);
}

// CPU情報に基づいた最適なバックエンド作成（x86_64）
std::unique_ptr<Backend> BackendFactory::createOptimizedX86_64Backend(
    Context* context, JITProfiler* profiler, const CPUInfo& cpuInfo) {
#if defined(AEROJS_ENABLE_X86_64)
    auto backend = std::make_unique<x86_64::X86_64Backend>(context, profiler);
    
    if (backend) {
        // 初期化前の設定
        
        // 初期化
        if (backend->initialize()) {
            return backend;
        }
    }
#endif
    
    // 失敗したらインタプリタにフォールバック
    return createInterpreterBackend(context);
}

// CPU情報に基づいた最適なバックエンド作成（RISC-V）
std::unique_ptr<Backend> BackendFactory::createOptimizedRISCVBackend(
    Context* context, JITProfiler* profiler, const CPUInfo& cpuInfo) {
#if defined(AEROJS_ENABLE_RISCV)
    auto backend = std::make_unique<riscv::RISCVBackend>(context, profiler);
    
    if (backend) {
        // 初期化前の設定
        
        // 初期化
        if (backend->initialize()) {
            return backend;
        }
    }
#endif
    
    // 失敗したらインタプリタにフォールバック
    return createInterpreterBackend(context);
}

std::unique_ptr<Backend> BackendFactory::createInterpreterBackend(Context* context) {
    auto backend = std::make_unique<interpreter::InterpreterBackend>(context);
    
    if (backend) {
        if (backend->initialize()) {
            return backend;
        }
    }
    
    // 初期化失敗時（実際には起きないはず）
    return nullptr;
}

} // namespace core
} // namespace aerojs