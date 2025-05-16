/**
 * @file arm64_features.cpp
 * @brief ARM64プロセッサ機能検出の実装
 * @version 1.0.0
 * @license MIT
 */

#include "arm64_backend.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#if defined(__APPLE__) || defined(__aarch64__) || defined(_M_ARM64)
// マルチプラットフォームサポート用のヘッダー
#if defined(__APPLE__)
#include <sys/sysctl.h>
#elif defined(__linux__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif
#endif

namespace aerojs {
namespace core {

void ARM64Features::detect() {
    // デフォルト値をセット
    supportsNeon = true;        // ARM64は常にNeonをサポート
    supportsSVE = false;
    supportsSVE2 = false;
    supportsLSE = false;
    supportsDotProduct = false;
    supportsBF16 = false;
    supportsCRC32 = false;
    supportsAES = false;
    supportsPMULL = false;
    supportsSHA1 = false;
    supportsSHA2 = false;
    supportsSHA3 = false;
    supportsATOMICS = false;
    supportsJSCVT = false;
    supportsFJCVTZS = false;

#if defined(__aarch64__) || defined(_M_ARM64)
    // 各プラットフォーム固有の方法で機能を検出
#if defined(__APPLE__)
    // Apple Silicon向け機能検出
    int value = 0;
    size_t size = sizeof(value);
    
    // Apple M1/M2は常に以下をサポート
    supportsLSE = true;
    supportsCRC32 = true;
    supportsAES = true;
    supportsPMULL = true;
    supportsSHA1 = true;
    supportsSHA2 = true;
    supportsATOMICS = true;
    supportsFJCVTZS = true;
    
    // Apple M2以降のみがこれをサポート
    if (sysctlbyname("hw.optional.arm.FEAT_DotProd", &value, &size, NULL, 0) == 0 && value == 1) {
        supportsDotProduct = true;
    }
    
    // SVEはAppleシリコンでは未サポート
    supportsSVE = false;
    supportsSVE2 = false;
    
#elif defined(__linux__)
    // Linuxでの機能検出: /proc/cpuinfoと getauxval()を使用
    
    // /proc/cpuinfoからCPU情報を取得
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("Features") != std::string::npos) {
            // 機能フラグをチェック
            if (line.find("asimd") != std::string::npos) supportsNeon = true;
            if (line.find("sve") != std::string::npos) supportsSVE = true;
            if (line.find("sve2") != std::string::npos) supportsSVE2 = true;
            if (line.find("lse") != std::string::npos) supportsLSE = true;
            if (line.find("dotprod") != std::string::npos) supportsDotProduct = true;
            if (line.find("bf16") != std::string::npos) supportsBF16 = true;
            if (line.find("crc32") != std::string::npos) supportsCRC32 = true;
            if (line.find("aes") != std::string::npos) supportsAES = true;
            if (line.find("pmull") != std::string::npos) supportsPMULL = true;
            if (line.find("sha1") != std::string::npos) supportsSHA1 = true;
            if (line.find("sha2") != std::string::npos) supportsSHA2 = true;
            if (line.find("sha3") != std::string::npos) supportsSHA3 = true;
            if (line.find("atomics") != std::string::npos) supportsATOMICS = true;
            if (line.find("jscvt") != std::string::npos) supportsJSCVT = true;
            if (line.find("fcntl") != std::string::npos) supportsFJCVTZS = true;
            break;
        }
    }
    
    // getauxval()を用いた検出 (Linuxのみ)
    unsigned long hwcaps = getauxval(AT_HWCAP);
    
    // SVEサポートを検証
    #ifdef HWCAP_SVE
    if (hwcaps & HWCAP_SVE) {
        supportsSVE = true;
    }
    #endif
    
    #ifdef HWCAP2_SVE2
    unsigned long hwcaps2 = getauxval(AT_HWCAP2);
    if (hwcaps2 & HWCAP2_SVE2) {
        supportsSVE2 = true;
    }
    #endif
    
#elif defined(_WIN32)
    // Windowsでの機能検出
    // IsProcessorFeaturePresent() またはGetNativeSystemInfo()を使用
    // Windowsでのサポートは限定的であるため、基本機能のみを仮定
    supportsNeon = true;
    supportsLSE = true;
    supportsCRC32 = true;
    supportsAES = true;
    supportsATOMICS = true;
    
    // Windows 11は一部でSVEをサポート (実行時検出が必要)
    // 実用的にはここにインラインアセンブラで SVEサポートを確認するコードを追加
    supportsSVE = false;  // 安全のためデフォルトはfalse
#endif
#endif

    // 実行時のSVEサポート検出コードを統合
    if (supportsSVE) {
        // SVEのベクトル長を検出（プラットフォーム固有の実装が必要）
        // 実行時にアセンブリコードを生成して実行するか、OS APIを使用
    }
}

bool ARM64Backend::detectSVESupport() {
    // SVEサポートのより詳細な実行時検出
    // この関数は実際にSVE命令を試行してサポートの有無を確認します
    
    // まず静的な特徴検出の結果をチェック
    if (!m_features.supportsSVE) {
        return false;
    }
    
    // SVE命令実行をテストする
    bool result = false;
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // アセンブリコードを動的生成して実行する必要がある
    // 実行可能メモリを確保
    const size_t codeSize = 64;
    void* executableMemory = nullptr;
    
#if defined(_WIN32)
    // Windows
    executableMemory = VirtualAlloc(NULL, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    // UNIX系
    executableMemory = mmap(NULL, codeSize, PROT_READ | PROT_WRITE | PROT_EXEC, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (executableMemory == MAP_FAILED) {
        executableMemory = nullptr;
    }
#endif

    if (executableMemory) {
        // SVEのRDVL命令（ベクトル長取得命令）を使ってテスト
        uint32_t* code = static_cast<uint32_t*>(executableMemory);
        
        // SVE命令をエンコード: RDVL x0, #1
        code[0] = 0x04BF0420; // RDVL x0, #1
        code[1] = 0xD65F03C0; // RET
        
        // メモリをフラッシュ（命令キャッシュをクリア）
#if defined(_WIN32)
        FlushInstructionCache(GetCurrentProcess(), executableMemory, codeSize);
#else
        __builtin___clear_cache(static_cast<char*>(executableMemory), 
                               static_cast<char*>(executableMemory) + codeSize);
#endif
        
        // 生成したコードを関数ポインタとして実行
        typedef uint64_t (*TestFunc)();
        TestFunc testFunc = reinterpret_cast<TestFunc>(executableMemory);
        
        // 例外処理でトラップ
        #if defined(_WIN32)
        __try {
            uint64_t sveVL = testFunc();
            // 成功した場合（0より大きい値が返る）
            result = (sveVL > 0);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // 例外が発生した場合はSVEをサポートしていない
            result = false;
        }
        #else
        // シグナルハンドラでSIGILLを捕捉
        struct sigaction oldAction;
        struct sigaction newAction;
        memset(&newAction, 0, sizeof(newAction));
        newAction.sa_handler = [](int) { /* シグナルハンドラ */ };
        sigaction(SIGILL, &newAction, &oldAction);
        
        // テスト実行
        bool caughtSignal = false;
        if (setjmp(jumpBuffer) == 0) {
            uint64_t sveVL = testFunc();
            // 成功した場合（0より大きい値が返る）
            result = (sveVL > 0);
        } else {
            // SIGILLが発生した
            caughtSignal = true;
            result = false;
        }
        
        // シグナルハンドラを元に戻す
        sigaction(SIGILL, &oldAction, NULL);
        #endif
        
        // 確保したメモリを解放
#if defined(_WIN32)
        VirtualFree(executableMemory, 0, MEM_RELEASE);
#else
        munmap(executableMemory, codeSize);
#endif
    }
#endif // defined(__aarch64__) || defined(_M_ARM64)
    
    // 結果をキャッシュ
    m_features.supportsSVE = result;
    
    return result;
}

uint32_t ARM64Backend::getSVEVectorLength() const {
    // SVEのベクトル長をバイト単位で返す
    if (!m_features.supportsSVE) {
        return 16; // SVEがサポートされていない場合はNEONと同じ128ビット=16バイトを返す
    }
    
    uint32_t sveLength = 16; // デフォルト値
    
#if defined(__aarch64__) || defined(_M_ARM64)
    // 実行時検出
    // アセンブリコードを動的生成して実行する必要がある
    // 実行可能メモリを確保
    const size_t codeSize = 64;
    void* executableMemory = nullptr;
    
#if defined(_WIN32)
    // Windows
    executableMemory = VirtualAlloc(NULL, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
#else
    // UNIX系
    executableMemory = mmap(NULL, codeSize, PROT_READ | PROT_WRITE | PROT_EXEC, 
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (executableMemory == MAP_FAILED) {
        executableMemory = nullptr;
    }
#endif

    if (executableMemory) {
        // SVEのRDVL命令（ベクトル長取得命令）を使ってベクトル長を取得
        uint32_t* code = static_cast<uint32_t*>(executableMemory);
        
        // SVE命令をエンコード: RDVL x0, #1
        code[0] = 0x04BF0420; // RDVL x0, #1
        code[1] = 0xD65F03C0; // RET
        
        // メモリをフラッシュ（命令キャッシュをクリア）
#if defined(_WIN32)
        FlushInstructionCache(GetCurrentProcess(), executableMemory, codeSize);
#else
        __builtin___clear_cache(static_cast<char*>(executableMemory), 
                               static_cast<char*>(executableMemory) + codeSize);
#endif
        
        // 生成したコードを関数ポインタとして実行
        typedef uint64_t (*TestFunc)();
        TestFunc testFunc = reinterpret_cast<TestFunc>(executableMemory);
        
        // 例外処理でトラップ
        #if defined(_WIN32)
        __try {
            uint64_t sveVL = testFunc();
            // 成功した場合（ベクトル長を返す）
            if (sveVL > 0) {
                sveLength = static_cast<uint32_t>(sveVL);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            // 例外が発生した場合は変更しない
        }
        #else
        // シグナルハンドラでSIGILLを捕捉
        struct sigaction oldAction;
        struct sigaction newAction;
        memset(&newAction, 0, sizeof(newAction));
        newAction.sa_handler = [](int) { /* シグナルハンドラ */ };
        sigaction(SIGILL, &newAction, &oldAction);
        
        // テスト実行
        if (setjmp(jumpBuffer) == 0) {
            uint64_t sveVL = testFunc();
            // 成功した場合（ベクトル長を返す）
            if (sveVL > 0) {
                sveLength = static_cast<uint32_t>(sveVL);
            }
        }
        
        // シグナルハンドラを元に戻す
        sigaction(SIGILL, &oldAction, NULL);
        #endif
        
        // 確保したメモリを解放
#if defined(_WIN32)
        VirtualFree(executableMemory, 0, MEM_RELEASE);
#else
        munmap(executableMemory, codeSize);
#endif
    }
#endif // defined(__aarch64__) || defined(_M_ARM64)
    
    return sveLength;
}

} // namespace core
} // namespace aerojs 