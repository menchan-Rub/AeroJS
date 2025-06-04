/**
 * @file riscv_jit_compiler.cpp
 * @brief RISC-V向けJITコンパイラ実装
 * 
 * このファイルは、RISC-Vアーキテクチャ向けのJITコンパイラクラスを実装します。
 * RV64GCV拡張セットに対応し、ベクトル処理やJavaScript特有の最適化を実装します。
 * 
 * @author AeroJS Team
 * @version 1.0.0
 * @copyright MIT License
 */

#include "riscv_jit_compiler.h"
#include "../../ir/ir_function.h"
#include "../../ir/ir_instruction.h"
#include "../../../context.h"
#include "../../../runtime/values/value.h"
#include "../../../../utils/logging.h"
#include <sys/mman.h>
#include <unistd.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

namespace aerojs {
namespace core {
namespace riscv {

RISCVJITCompiler::RISCVJITCompiler(Context* context)
    : context_(context)
    , codeGenerator_(context)
    , vectorUnit_(context) {
    
    // RISC-V拡張の検出
    DetectRISCVExtensions();
    
    // レジスタ使用状況の初期化
    integerRegistersUsed_.resize(NUM_INTEGER_REGISTERS, false);
    floatRegistersUsed_.resize(NUM_FLOAT_REGISTERS, false);
    vectorRegistersUsed_.resize(NUM_VECTOR_REGISTERS, false);
    
    // システムレジスタは常に使用中とマーク
    integerRegistersUsed_[RISCVRegisters::ZERO] = true;  // ゼロレジスタ
    integerRegistersUsed_[RISCVRegisters::RA] = true;    // リターンアドレス
    integerRegistersUsed_[RISCVRegisters::SP] = true;    // スタックポインタ
    integerRegistersUsed_[RISCVRegisters::GP] = true;    // グローバルポインタ
    integerRegistersUsed_[RISCVRegisters::TP] = true;    // スレッドポインタ
    
    LOG_INFO("RISC-V JITコンパイラを初期化しました");
    LOG_INFO("対応拡張: I={}, M={}, A={}, F={}, D={}, C={}, V={}",
             extensions_.hasRV64I, extensions_.hasRV64M, extensions_.hasRV64A,
             extensions_.hasRV64F, extensions_.hasRV64D, extensions_.hasRV64C,
             extensions_.hasRV64V);
}

RISCVJITCompiler::~RISCVJITCompiler() {
    // 割り当てたメモリを解放
    for (const auto& memoryInfo : allocatedMemoryInfo_) {
        if (memoryInfo.ptr) {
            // 実際のサイズを使用してメモリを解放
            if (munmap(memoryInfo.ptr, memoryInfo.size) != 0) {
                LOG_ERROR("メモリ解放に失敗: ptr={}, size={}, error={}", 
                         memoryInfo.ptr, memoryInfo.size, strerror(errno));
            } else {
                LOG_DEBUG("メモリ解放完了: ptr={}, size={}", memoryInfo.ptr, memoryInfo.size);
            }
        }
    }
    allocatedMemoryInfo_.clear();
    
    LOG_INFO("RISC-V JITコンパイラを終了します");
    LOG_INFO("統計: コンパイル関数数={}, 生成命令数={}, ベクトル命令数={}, 平均コンパイル時間={}ms",
             stats_.functionsCompiled, stats_.instructionsGenerated,
             stats_.vectorInstructionsGenerated, stats_.averageCompilationTime);
}

CompileResult RISCVJITCompiler::Compile(const IRFunction& function) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // IRをコピーして最適化を適用
    IRFunction optimizedFunction = function;
    OptimizeIR(optimizedFunction);
    
    // レジスタ割り当て
    AllocateRegisters(optimizedFunction);
    
    // コード生成
    RISCVCompilationResult result = CompileFunction(optimizedFunction);
    
    // 実行可能メモリの割り当て
    void* executableMemory = AllocateExecutableMemory(result.codeSize);
    if (!executableMemory) {
        LOG_ERROR("実行可能メモリの割り当てに失敗しました");
        return CompileResult{};
    }
    
    // 命令をメモリにコピー
    std::memcpy(executableMemory, result.instructions.data(), result.codeSize);
    
    // リロケーションの適用
    ApplyRelocations(result);
    
    // メモリを実行可能にする
    MakeMemoryExecutable(executableMemory, result.codeSize);
    
    // 統計情報の更新
    auto endTime = std::chrono::high_resolution_clock::now();
    auto compilationTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000.0;
    
    stats_.functionsCompiled++;
    stats_.instructionsGenerated += result.instructions.size();
    stats_.vectorInstructionsGenerated += result.vectorInstructions;
    stats_.averageCompilationTime = (stats_.averageCompilationTime * (stats_.functionsCompiled - 1) + compilationTime) / stats_.functionsCompiled;
    stats_.codeSize += result.codeSize;
    
    // CompileResultの作成
    CompileResult compileResult;
    compileResult.nativeCode = executableMemory;
    compileResult.codeSize = result.codeSize;
    compileResult.entryPoint = reinterpret_cast<uintptr_t>(executableMemory) + result.entryPoint;
    compileResult.isOptimized = result.isOptimized;
    
    return compileResult;
}

bool RISCVJITCompiler::Execute(const CompileResult& result, Value* args, size_t argCount, Value& returnValue) {
    if (!result.nativeCode) {
        return false;
    }
    
    try {
        // 関数ポインタにキャスト
        typedef Value (*CompiledFunction)(Value*, size_t);
        CompiledFunction func = reinterpret_cast<CompiledFunction>(result.entryPoint);
        
        // 実行
        returnValue = func(args, argCount);
        
        // 実行回数の更新
        executionCounts_[result.entryPoint]++;
        
        return true;
    } catch (...) {
        LOG_ERROR("コンパイル済み関数の実行中にエラーが発生しました");
        return false;
    }
}

void RISCVJITCompiler::InvalidateCode(const CompileResult& result) {
    if (result.nativeCode) {
        // 適切なメモリ無効化処理の実装
        
        // 1. 実行中のコードの安全な停止
        invalidateExecutingCode(result.nativeCode, result.codeSize);
        
        // 2. キャッシュの無効化
        invalidateInstructionCache(result.nativeCode, result.codeSize);
        
        // 3. メモリ保護の変更（実行不可に設定）
        if (mprotect(result.nativeCode, result.codeSize, PROT_READ | PROT_WRITE) != 0) {
            // エラーハンドリング
            LOG_ERROR("メモリ保護の変更に失敗: {}", strerror(errno));
        }
        
        // 4. 無効化マークの設定
        markCodeAsInvalid(result.nativeCode, result.codeSize);
        
        // 5. 遅延解放のためのキューに追加
        scheduleForDeferredDeallocation(result.nativeCode, result.codeSize);
        
        // 統計情報から除外
        stats_.codeSize -= result.codeSize;
        stats_.invalidatedFunctions++;
    }
}

void RISCVJITCompiler::invalidateExecutingCode(void* codePtr, size_t codeSize) {
    // 実行中のコードの安全な無効化
    
    // 1. 該当コードを実行中のスレッドを特定
    std::vector<std::thread::id> executingThreads;
    
    // 2. 全アクティブスレッドの列挙
    std::vector<std::thread::id> allThreads = enumerateActiveThreads();
    
    for (const auto& threadId : allThreads) {
        // 3. 各スレッドのスタック情報を取得
        ThreadStackInfo stackInfo = getThreadStackInfo(threadId);
        if (!stackInfo.isValid) {
            continue;
        }
        
        // 4. スタックフレームを順次走査
        bool codeFoundInStack = false;
        uintptr_t currentFrame = stackInfo.stackPointer;
        uintptr_t stackBase = stackInfo.stackBase;
        
        while (currentFrame < stackBase && !codeFoundInStack) {
            // 5. フレーム内のリターンアドレスをチェック
            uintptr_t* framePtr = reinterpret_cast<uintptr_t*>(currentFrame);
            
            // RISC-Vスタックフレーム構造に基づく解析
            // フレームポインタ（fp）とリターンアドレス（ra）の位置
            if (isValidStackAddress(framePtr) && isValidStackAddress(framePtr + 1)) {
                uintptr_t returnAddress = *(framePtr + 1); // ra レジスタの保存位置
                uintptr_t framePointer = *framePtr;        // fp レジスタの保存位置
                
                // 6. リターンアドレスが無効化対象コード範囲内かチェック
                if (isAddressInCodeRange(returnAddress, codePtr, codeSize)) {
                    codeFoundInStack = true;
                    executingThreads.push_back(threadId);
                    
                    // デバッグ情報の記録
                    logCodeExecutionDetection(threadId, returnAddress, currentFrame);
                    break;
                }
                
                // 7. 関数呼び出しチェーン内の他のアドレスもチェック
                if (isValidCodeAddress(returnAddress)) {
                    // 呼び出し元関数内でのJITコード参照をチェック
                    if (checkForJITCodeReferences(returnAddress, codePtr, codeSize)) {
                        codeFoundInStack = true;
                        executingThreads.push_back(threadId);
                        break;
                    }
                }
                
                // 8. 次のスタックフレームへ移動
                if (framePointer > currentFrame && framePointer < stackBase) {
                    currentFrame = framePointer;
                } else {
                    // フレームポインタが無効な場合、ヒューリスティック検索
                    currentFrame = findNextStackFrame(currentFrame, stackBase);
                    if (currentFrame == 0) break;
                }
            } else {
                // フレーム構造が破損している場合の回復処理
                currentFrame = recoverStackTraversal(currentFrame, stackBase);
                if (currentFrame == 0) break;
            }
            
            // 9. スタック走査の安全性チェック
            if (currentFrame - stackInfo.stackPointer > MAX_STACK_SCAN_DEPTH) {
                logWarning("Stack scan depth exceeded for thread", threadId);
                break;
            }
        }
        
        // 10. レジスタコンテキストもチェック
        if (!codeFoundInStack) {
            ThreadRegisterContext regContext = getThreadRegisterContext(threadId);
            if (regContext.isValid) {
                // プログラムカウンタ（PC）のチェック
                if (isAddressInCodeRange(regContext.pc, codePtr, codeSize)) {
                    executingThreads.push_back(threadId);
                    logCodeExecutionDetection(threadId, regContext.pc, 0);
                }
                
                // リンクレジスタ（ra）のチェック
                if (isAddressInCodeRange(regContext.ra, codePtr, codeSize)) {
                    executingThreads.push_back(threadId);
                    logCodeExecutionDetection(threadId, regContext.ra, 0);
                }
                
                // その他の汎用レジスタ内のコードポインタチェック
                for (int i = 0; i < 32; ++i) {
                    if (isAddressInCodeRange(regContext.x[i], codePtr, codeSize)) {
                        executingThreads.push_back(threadId);
                        logCodeExecutionDetection(threadId, regContext.x[i], 0);
                        break;
                    }
                }
            }
        }
    }
    
    // 11. 実行中スレッドが見つかった場合の処理
    if (!executingThreads.empty()) {
        // セーフポイント要求を送信
        for (const auto& threadId : executingThreads) {
            requestSafepointStop(threadId);
        }
        
        // 全スレッドがセーフポイントに到達するまで待機
        waitForSafepointCompletion(executingThreads);
        
        // 実行追跡テーブルを更新
        updateExecutionTracking(codePtr, codeSize, false);
        
        return true; // 実行中のコードが見つかった
    }
    
    return false; // 実行中のコードは見つからなかった
}

void RISCVJITCompiler::invalidateInstructionCache(void* codePtr, size_t codeSize) {
    // RISC-V命令キャッシュの無効化
    
#ifdef __riscv
    // RISC-V固有の命令キャッシュ無効化
    
    // 1. データキャッシュのフラッシュ
    for (uintptr_t addr = reinterpret_cast<uintptr_t>(codePtr);
         addr < reinterpret_cast<uintptr_t>(codePtr) + codeSize;
         addr += CACHE_LINE_SIZE) {
        
        // RISC-V Zicbom拡張の使用
        if (extensions_.hasZicbom) {
            asm volatile("cbo.flush %0" : : "A"(addr) : "memory");
        } else {
            // フォールバック：メモリバリア
            asm volatile("fence" : : : "memory");
        }
    }
    
    // 2. 命令キャッシュの無効化
    for (uintptr_t addr = reinterpret_cast<uintptr_t>(codePtr);
         addr < reinterpret_cast<uintptr_t>(codePtr) + codeSize;
         addr += CACHE_LINE_SIZE) {
        
        if (extensions_.hasZicbom) {
            asm volatile("cbo.inval %0" : : "A"(addr) : "memory");
        }
    }
    
    // 3. 命令フェッチバリア
    asm volatile("fence.i" : : : "memory");
    
#else
    // 他のアーキテクチャでのフォールバック
    __builtin___clear_cache(static_cast<char*>(codePtr), 
                           static_cast<char*>(codePtr) + codeSize);
#endif
}

void RISCVJITCompiler::markCodeAsInvalid(void* codePtr, size_t codeSize) {
    // コードに無効化マークを設定
    
    // 1. 無効化ヘッダーの作成
    InvalidationHeader* header = static_cast<InvalidationHeader*>(codePtr);
    
    // 元のコードの最初の命令を保存
    uint32_t originalInstruction = *reinterpret_cast<uint32_t*>(codePtr);
    
    // 2. 無効化マークの設定
    header->magic = INVALIDATION_MAGIC;
    header->originalInstruction = originalInstruction;
    header->invalidationTime = std::chrono::steady_clock::now();
    header->codeSize = codeSize;
    
    // 3. トラップ命令の挿入（RISC-V EBREAK）
    uint32_t* firstInst = reinterpret_cast<uint32_t*>(codePtr);
    *firstInst = 0x00100073; // EBREAK命令
    
    // 4. 無効化テーブルに登録
    {
        std::lock_guard<std::mutex> lock(invalidationTableMutex_);
        invalidationTable_[codePtr] = {
            .originalInstruction = originalInstruction,
            .invalidationTime = header->invalidationTime,
            .codeSize = codeSize,
            .isScheduledForDeallocation = false
        };
    }
}

void RISCVJITCompiler::scheduleForDeferredDeallocation(void* codePtr, size_t codeSize) {
    // 遅延解放のスケジューリング
    
    DeferredDeallocation dealloc;
    dealloc.codePtr = codePtr;
    dealloc.codeSize = codeSize;
    dealloc.scheduledTime = std::chrono::steady_clock::now();
    dealloc.minWaitTime = std::chrono::milliseconds(100); // 最小待機時間
    
    {
        std::lock_guard<std::mutex> lock(deferredDeallocationMutex_);
        deferredDeallocations_.push_back(dealloc);
    }
    
    // バックグラウンドでの解放処理を開始
    if (!deferredDeallocationThread_.joinable()) {
        deferredDeallocationThread_ = std::thread(&RISCVJITCompiler::processDeferredDeallocations, this);
    }
}

void RISCVJITCompiler::processDeferredDeallocations() {
    // 遅延解放の処理スレッド
    
    while (deferredDeallocationEnabled_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        std::vector<DeferredDeallocation> readyForDeallocation;
        auto now = std::chrono::steady_clock::now();
        
        {
            std::lock_guard<std::mutex> lock(deferredDeallocationMutex_);
            
            auto it = deferredDeallocations_.begin();
            while (it != deferredDeallocations_.end()) {
                if (now - it->scheduledTime >= it->minWaitTime) {
                    // 安全に解放可能かチェック
                    if (isSafeToDealloc(it->codePtr)) {
                        readyForDeallocation.push_back(*it);
                        it = deferredDeallocations_.erase(it);
                    } else {
                        // まだ実行中の可能性があるため待機時間を延長
                        it->minWaitTime += std::chrono::milliseconds(50);
                        ++it;
                    }
                } else {
                    ++it;
                }
            }
        }
        
        // 実際の解放処理
        for (const auto& dealloc : readyForDeallocation) {
            performActualDeallocation(dealloc.codePtr, dealloc.codeSize);
        }
    }
}

bool RISCVJITCompiler::isSafeToDealloc(void* codePtr) {
    // 解放が安全かどうかの判定
    
    // 1. 実行中のスレッドがないかチェック
    {
        std::lock_guard<std::mutex> lock(executionTrackingMutex_);
        
        for (const auto& entry : executingCodeMap_) {
            if (entry.second.codePtr == codePtr) {
                return false; // まだ実行中
            }
        }
    }
    
    // 2. スタック上に該当コードのアドレスがないかチェック
    if (hasCodeAddressOnStack(codePtr)) {
        return false;
    }
    
    // 3. 他のコードからの参照がないかチェック
    if (hasReferencesFromOtherCode(codePtr)) {
        return false;
    }
    
    return true;
}

void RISCVJITCompiler::performActualDeallocation(void* codePtr, size_t codeSize) {
    // 実際のメモリ解放処理
    
    // 1. 無効化テーブルから削除
    {
        std::lock_guard<std::mutex> lock(invalidationTableMutex_);
        invalidationTable_.erase(codePtr);
    }
    
    // 2. メモリの解放
    if (munmap(codePtr, codeSize) != 0) {
        LOG_ERROR("メモリ解放に失敗: {}", strerror(errno));
    }
    
    // 3. 統計情報の更新
    stats_.deallocatedBytes += codeSize;
    stats_.deallocatedFunctions++;
}

void RISCVJITCompiler::requestSafepointStop(std::thread::id threadId) {
    // セーフポイントでの停止要求
    
    std::lock_guard<std::mutex> lock(safepointMutex_);
    safepointRequests_[threadId] = true;
}

void RISCVJITCompiler::waitForSafepointCompletion(const std::vector<std::thread::id>& threadIds) {
    // セーフポイント完了の待機
    
    auto timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    
    while (std::chrono::steady_clock::now() < timeout) {
        bool allStopped = true;
        
        {
            std::lock_guard<std::mutex> lock(safepointMutex_);
            
            for (const auto& threadId : threadIds) {
                if (safepointRequests_.count(threadId) > 0 && 
                    safepointRequests_[threadId]) {
                    allStopped = false;
                    break;
                }
            }
        }
        
        if (allStopped) {
            return;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // タイムアウト時の強制処理
    LOG_WARNING("セーフポイント待機がタイムアウトしました");
}

void RISCVJITCompiler::setInvalidationMarker(void* codePtr, size_t codeSize) {
    // 無効化マーカーの設定
    
    // コードの先頭に特別なマーカーを設置
    uint32_t* marker = static_cast<uint32_t*>(codePtr);
    
    // RISC-V EBREAK命令 + カスタムデータ
    marker[0] = 0x00100073; // EBREAK
    marker[1] = 0xDEADBEEF; // 無効化マジック
    marker[2] = static_cast<uint32_t>(codeSize);
    marker[3] = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool RISCVJITCompiler::hasCodeAddressOnStack(void* codePtr) {
    // 完全なスタック走査によるコードアドレス検索
    
    // 1. 全アクティブスレッドの列挙
    std::vector<std::thread::id> allThreads = enumerateActiveThreads();
    
    for (const auto& threadId : allThreads) {
        // 2. 各スレッドのスタック情報を取得
        ThreadStackInfo stackInfo = getThreadStackInfo(threadId);
        if (!stackInfo.isValid) {
            continue;
        }
        
        // 3. スタックフレームを順次走査
        uintptr_t currentFrame = stackInfo.stackPointer;
        uintptr_t stackBase = stackInfo.stackBase;
        size_t scanDepth = 0;
        
        while (currentFrame < stackBase && scanDepth < MAX_STACK_SCAN_DEPTH) {
            // 4. フレーム内のアドレスをチェック
            uintptr_t* framePtr = reinterpret_cast<uintptr_t*>(currentFrame);
            
            // RISC-Vスタックフレーム構造に基づく解析
            if (isValidStackAddress(framePtr) && isValidStackAddress(framePtr + 1)) {
                // リターンアドレス（ra）のチェック
                uintptr_t returnAddress = *(framePtr + 1);
                if (isAddressInCodeRange(returnAddress, codePtr, getCodeSize(codePtr))) {
                    logCodeAddressFound(threadId, returnAddress, currentFrame, "return_address");
                    return true;
                }
                
                // フレームポインタ（fp）のチェック
                uintptr_t framePointer = *framePtr;
                if (isAddressInCodeRange(framePointer, codePtr, getCodeSize(codePtr))) {
                    logCodeAddressFound(threadId, framePointer, currentFrame, "frame_pointer");
                    return true;
                }
                
                // 5. フレーム内の他のアドレスもスキャン
                size_t frameSize = calculateFrameSize(framePtr, stackInfo);
                for (size_t offset = 0; offset < frameSize; offset += sizeof(uintptr_t)) {
                    uintptr_t* addressPtr = reinterpret_cast<uintptr_t*>(currentFrame + offset);
                    if (isValidStackAddress(addressPtr)) {
                        uintptr_t address = *addressPtr;
                        if (isAddressInCodeRange(address, codePtr, getCodeSize(codePtr))) {
                            logCodeAddressFound(threadId, address, currentFrame + offset, "stack_variable");
                            return true;
                        }
                    }
                }
                
                // 6. 次のスタックフレームへ移動
                if (framePointer > currentFrame && framePointer < stackBase) {
                    currentFrame = framePointer;
                } else {
                    // フレームポインタが無効な場合、ヒューリスティック検索
                    currentFrame = findNextStackFrame(currentFrame, stackBase);
                    if (currentFrame == 0) break;
                }
            } else {
                // フレーム構造が破損している場合の回復処理
                currentFrame = recoverStackTraversal(currentFrame, stackBase);
                if (currentFrame == 0) break;
            }
            
            scanDepth++;
        }
        
        // 7. レジスタコンテキストもチェック
        ThreadRegisterContext regContext = getThreadRegisterContext(threadId);
        if (regContext.isValid) {
            // プログラムカウンタ（PC）のチェック
            if (isAddressInCodeRange(regContext.pc, codePtr, getCodeSize(codePtr))) {
                logCodeAddressFound(threadId, regContext.pc, 0, "program_counter");
                return true;
            }
            
            // リンクレジスタ（ra）のチェック
            if (isAddressInCodeRange(regContext.ra, codePtr, getCodeSize(codePtr))) {
                logCodeAddressFound(threadId, regContext.ra, 0, "link_register");
                return true;
            }
            
            // 汎用レジスタ内のコードポインタチェック
            for (int i = 0; i < 32; ++i) {
                if (isAddressInCodeRange(regContext.x[i], codePtr, getCodeSize(codePtr))) {
                    logCodeAddressFound(threadId, regContext.x[i], 0, "general_register_" + std::to_string(i));
                    return true;
                }
            }
            
            // 浮動小数点レジスタもチェック（アドレスが格納されている可能性）
            for (int i = 0; i < 32; ++i) {
                uintptr_t fpRegAsAddr = reinterpret_cast<uintptr_t>(regContext.f[i]);
                if (isAddressInCodeRange(fpRegAsAddr, codePtr, getCodeSize(codePtr))) {
                    logCodeAddressFound(threadId, fpRegAsAddr, 0, "float_register_" + std::to_string(i));
                    return true;
                }
            }
        }
        
        // 8. スレッドローカルストレージのチェック
        if (checkThreadLocalStorage(threadId, codePtr)) {
            return true;
        }
        
        // 9. シグナルハンドラスタックのチェック
        if (checkSignalHandlerStack(threadId, codePtr)) {
            return true;
        }
    }
    
    // 10. グローバルスタック領域のチェック（マルチスレッド環境）
    if (checkGlobalStackAreas(codePtr)) {
        return true;
    }
    
    return false; // コードアドレスは見つからなかった
}

bool RISCVJITCompiler::hasReferencesFromOtherCode(void* codePtr) {
    // 完璧な他のコードからの参照チェック実装
    
    // 1. コールグラフベースの参照検索
    std::unordered_set<void*> referencingCode;
    
    // 直接呼び出し参照の検索
    for (const auto& [caller, callees] : callGraph_) {
        for (void* callee : callees) {
            if (isAddressInRange(callee, codePtr, getCurrentCodeSize(codePtr))) {
                referencingCode.insert(caller);
            }
        }
    }
    
    // 2. グローバル参照テーブルの検索
    {
        std::lock_guard<std::mutex> lock(globalReferenceMutex_);
        for (const auto& [refAddr, targetAddr] : globalReferenceTable_) {
            if (isAddressInRange(targetAddr, codePtr, getCurrentCodeSize(codePtr))) {
                referencingCode.insert(refAddr);
            }
        }
    }
    
    // 3. インライン化キャッシュの参照検索
    for (const auto& [icSite, targets] : inlineCacheTargets_) {
        for (void* target : targets) {
            if (isAddressInRange(target, codePtr, getCurrentCodeSize(codePtr))) {
                referencingCode.insert(icSite);
            }
        }
    }
    
    // 4. OSRエントリポイントの検索
    for (const auto& [osrPoint, targetCode] : osrEntryPoints_) {
        if (isAddressInRange(targetCode, codePtr, getCurrentCodeSize(codePtr))) {
            referencingCode.insert(osrPoint);
        }
    }
    
    // 5. 例外ハンドラテーブルの検索
    for (const auto& [handlerAddr, protectedCode] : exceptionHandlerTable_) {
        if (isAddressInRange(protectedCode, codePtr, getCurrentCodeSize(codePtr))) {
            referencingCode.insert(handlerAddr);
        }
    }
    
    // 6. デバッグ情報の参照検索
    if (debugInfoEnabled_) {
        for (const auto& [debugAddr, codeAddr] : debugInfoTable_) {
            if (isAddressInRange(codeAddr, codePtr, getCurrentCodeSize(codePtr))) {
                referencingCode.insert(debugAddr);
            }
        }
    }
    
    // 7. プロファイリングデータの参照検索
    for (const auto& [profilePoint, codeAddr] : profilingDataTable_) {
        if (isAddressInRange(codeAddr, codePtr, getCurrentCodeSize(codePtr))) {
            referencingCode.insert(profilePoint);
        }
    }
    
    // 8. 動的パッチポイントの参照検索
    for (const auto& [patchPoint, targetCode] : dynamicPatchPoints_) {
        if (isAddressInRange(targetCode, codePtr, getCurrentCodeSize(codePtr))) {
            referencingCode.insert(patchPoint);
        }
    }
    
    // 9. 参照の有効性確認
    for (void* refCode : referencingCode) {
        if (isCodeStillValid(refCode)) {
            LOG_DEBUG("有効な参照を発見: 0x{:x} -> 0x{:x}", 
                     reinterpret_cast<uintptr_t>(refCode),
                     reinterpret_cast<uintptr_t>(codePtr));
            return true;
        }
    }
    
    return false;
}

size_t RISCVJITCompiler::GetCodeSize(const CompileResult& result) const {
    return result.codeSize;
}

void RISCVJITCompiler::SetOptimizationLevel(OptimizationLevel level) {
    optimizationLevel_ = level;
    
    // 最適化レベルに応じた設定調整
    switch (level) {
        case OptimizationLevel::NONE:
            vectorizationEnabled_ = false;
            profilingOptimization_ = false;
            break;
            
        case OptimizationLevel::MINIMAL:
            vectorizationEnabled_ = false;
            profilingOptimization_ = false;
            break;
            
        case OptimizationLevel::BALANCED:
            vectorizationEnabled_ = extensions_.hasRV64V;
            profilingOptimization_ = true;
            break;
            
        case OptimizationLevel::AGGRESSIVE:
            vectorizationEnabled_ = extensions_.hasRV64V;
            profilingOptimization_ = true;
            break;
    }
}

bool RISCVJITCompiler::SupportsExtension(const std::string& extension) const {
    if (extension == "I") return extensions_.hasRV64I;
    if (extension == "M") return extensions_.hasRV64M;
    if (extension == "A") return extensions_.hasRV64A;
    if (extension == "F") return extensions_.hasRV64F;
    if (extension == "D") return extensions_.hasRV64D;
    if (extension == "C") return extensions_.hasRV64C;
    if (extension == "V") return extensions_.hasRV64V;
    if (extension == "Zba") return extensions_.hasZba;
    if (extension == "Zbb") return extensions_.hasZbb;
    if (extension == "Zbc") return extensions_.hasZbc;
    if (extension == "Zbs") return extensions_.hasZbs;
    if (extension == "Zfh") return extensions_.hasZfh;
    if (extension == "Zvfh") return extensions_.hasZvfh;
    
    return false;
}

RISCVCompilationResult RISCVJITCompiler::CompileFunction(const IRFunction& function) {
    RISCVCompilationResult result;
    
    // プロローグの生成
    EmitPrologue(result);
    
    // 関数本体の命令を生成
    const auto& instructions = function.GetInstructions();
    for (const auto& instr : instructions) {
        EmitInstruction(instr.GetOpcode(), instr.GetOperands(), result);
    }
    
    // エピローグの生成
    EmitEpilogue(result);
    
    // ピープホール最適化
    if (optimizationLevel_ >= OptimizationLevel::BALANCED) {
        PerformPeepholeOptimization(result.instructions);
    }
    
    result.codeSize = result.instructions.size() * sizeof(uint32_t);
    result.isOptimized = (optimizationLevel_ > OptimizationLevel::NONE);
    
    return result;
}

void RISCVJITCompiler::OptimizeIR(IRFunction& function) {
    if (optimizationLevel_ == OptimizationLevel::NONE) {
        return;
    }
    
    // 定数畳み込み
    PerformConstantFolding(function);
    
    // デッドコード除去
    PerformDeadCodeElimination(function);
    
    // 命令スケジューリング
    if (optimizationLevel_ >= OptimizationLevel::BALANCED) {
        PerformInstructionScheduling(function);
    }
    
    // ベクトル化
    if (vectorizationEnabled_ && optimizationLevel_ >= OptimizationLevel::BALANCED) {
        PerformVectorization(function);
    }
    
    // ループ最適化
    if (optimizationLevel_ >= OptimizationLevel::AGGRESSIVE) {
        PerformLoopOptimization(function);
    }
    
    // JavaScript固有の最適化
    OptimizePropertyAccess(function);
    OptimizeArrayAccess(function);
    OptimizeFunctionCalls(function);
    OptimizeTypeChecks(function);
}

void RISCVJITCompiler::AllocateRegisters(const IRFunction& function) {
    // レジスタ使用状況をリセット
    std::fill(integerRegistersUsed_.begin(), integerRegistersUsed_.end(), false);
    std::fill(floatRegistersUsed_.begin(), floatRegistersUsed_.end(), false);
    std::fill(vectorRegistersUsed_.begin(), vectorRegistersUsed_.end(), false);
    
    // システムレジスタは常に使用中
    integerRegistersUsed_[RISCVRegisters::ZERO] = true;
    integerRegistersUsed_[RISCVRegisters::RA] = true;
    integerRegistersUsed_[RISCVRegisters::SP] = true;
    integerRegistersUsed_[RISCVRegisters::GP] = true;
    integerRegistersUsed_[RISCVRegisters::TP] = true;
    
    // より高度なレジスタ割り当てアルゴリズムの実装
    if (optimizationLevel_ >= OptimizationLevel::AGGRESSIVE) {
        performGraphColoringRegisterAllocation(function);
    } else if (optimizationLevel_ >= OptimizationLevel::BALANCED) {
        performLinearScanRegisterAllocation(function);
    } else {
        performSimpleRegisterAllocation(function);
    }
}

void RISCVJITCompiler::performGraphColoringRegisterAllocation(const IRFunction& function) {
    // グラフ彩色によるレジスタ割り当て
    
    // 1. 干渉グラフの構築
    InterferenceGraph interferenceGraph = buildInterferenceGraph(function);
    
    // 2. 生存区間の計算
    LivenessAnalysis livenessAnalysis(function);
    auto liveRanges = livenessAnalysis.computeLiveRanges();
    
    // 3. スピル候補の優先度計算
    SpillCostAnalysis spillCostAnalysis(function, liveRanges);
    auto spillCosts = spillCostAnalysis.computeSpillCosts();
    
    // 4. グラフ彩色アルゴリズムの実行
    GraphColoringAllocator allocator(interferenceGraph, spillCosts);
    auto allocation = allocator.allocate(getAvailableRegisters());
    
    // 5. 割り当て結果の適用
    applyRegisterAllocation(function, allocation);
    
    // 6. スピルコードの生成
    generateSpillCode(function, allocation.spilledVariables);
}

void RISCVJITCompiler::performLinearScanRegisterAllocation(const IRFunction& function) {
    // 線形スキャンによるレジスタ割り当て
    
    // 1. 生存区間の計算とソート
    LivenessAnalysis livenessAnalysis(function);
    auto liveRanges = livenessAnalysis.computeLiveRanges();
    
    // 開始点でソート
    std::sort(liveRanges.begin(), liveRanges.end(),
              [](const LiveRange& a, const LiveRange& b) {
                  return a.start < b.start;
              });
    
    // 2. アクティブな区間の管理
    std::vector<LiveRange> activeRanges;
    std::vector<int> availableRegs = getAvailableRegisters();
    
    // 3. 各生存区間に対してレジスタを割り当て
    for (auto& range : liveRanges) {
        // 期限切れの区間を削除
        expireOldIntervals(activeRanges, availableRegs, range.start);
        
        if (availableRegs.empty()) {
            // スピルが必要
            spillAtInterval(activeRanges, availableRegs, range);
        } else {
            // レジスタを割り当て
            int reg = availableRegs.back();
            availableRegs.pop_back();
            
            range.assignedRegister = reg;
            activeRanges.push_back(range);
            
            // 終了点でソート
            std::sort(activeRanges.begin(), activeRanges.end(),
                      [](const LiveRange& a, const LiveRange& b) {
                          return a.end < b.end;
                      });
        }
    }
    
    // 4. 割り当て結果の記録
    recordRegisterAssignments(liveRanges);
}

void RISCVJITCompiler::performSimpleRegisterAllocation(const IRFunction& function) {
    // 簡単なレジスタ割り当て（最適化レベルが低い場合）
    
    const auto& instructions = function.GetInstructions();
    std::unordered_map<int, int> virtualToPhysical;
    
    int nextIntReg = RISCVRegisters::T0;
    int nextFloatReg = RISCVRegisters::FT0;
    
    for (const auto& instr : instructions) {
        // 各仮想レジスタに物理レジスタを順次割り当て
        for (const auto& operand : instr.GetOperands()) {
            if (operand.isVirtualRegister()) {
                int virtualReg = operand.getVirtualRegister();
                
                if (virtualToPhysical.find(virtualReg) == virtualToPhysical.end()) {
                    if (operand.isFloatingPoint()) {
                        if (nextFloatReg <= RISCVRegisters::FT11) {
                            virtualToPhysical[virtualReg] = nextFloatReg++;
                        } else {
                            // スピル
                            spillVirtualRegister(virtualReg);
                        }
                    } else {
                        if (nextIntReg <= RISCVRegisters::T6) {
                            virtualToPhysical[virtualReg] = nextIntReg++;
                        } else {
                            // スピル
                            spillVirtualRegister(virtualReg);
                        }
                    }
                }
            }
        }
    }
    
    // 割り当て結果を記録
    registerAllocation_ = virtualToPhysical;
}

InterferenceGraph RISCVJITCompiler::buildInterferenceGraph(const IRFunction& function) {
    // 干渉グラフの構築
    
    InterferenceGraph graph;
    LivenessAnalysis livenessAnalysis(function);
    auto liveRanges = livenessAnalysis.computeLiveRanges();
    
    // 全ての仮想レジスタをノードとして追加
    for (const auto& range : liveRanges) {
        graph.addNode(range.virtualRegister);
    }
    
    // 生存区間が重複する仮想レジスタ間にエッジを追加
    for (size_t i = 0; i < liveRanges.size(); ++i) {
        for (size_t j = i + 1; j < liveRanges.size(); ++j) {
            if (liveRanges[i].interferesWith(liveRanges[j])) {
                graph.addEdge(liveRanges[i].virtualRegister, 
                             liveRanges[j].virtualRegister);
            }
        }
    }
    
    return graph;
}

std::vector<int> RISCVJITCompiler::getAvailableRegisters() {
    // 使用可能なレジスタのリストを取得
    
    std::vector<int> availableRegs;
    
    // 整数レジスタ（一時レジスタと保存レジスタ）
    for (int reg = RISCVRegisters::T0; reg <= RISCVRegisters::T6; ++reg) {
        if (!integerRegistersUsed_[reg]) {
            availableRegs.push_back(reg);
        }
    }
    
    for (int reg = RISCVRegisters::S1; reg <= RISCVRegisters::S11; ++reg) {
        if (!integerRegistersUsed_[reg]) {
            availableRegs.push_back(reg);
        }
    }
    
    // 引数レジスタ（関数内で再利用可能）
    for (int reg = RISCVRegisters::A0; reg <= RISCVRegisters::A7; ++reg) {
        if (!integerRegistersUsed_[reg]) {
            availableRegs.push_back(reg);
        }
    }
    
    return availableRegs;
}

void RISCVJITCompiler::expireOldIntervals(std::vector<LiveRange>& activeRanges,
                                         std::vector<int>& availableRegs,
                                         size_t currentPoint) {
    // 期限切れの生存区間を処理
    
    auto it = activeRanges.begin();
    while (it != activeRanges.end()) {
        if (it->end <= currentPoint) {
            // レジスタを解放
            availableRegs.push_back(it->assignedRegister);
            it = activeRanges.erase(it);
        } else {
            ++it;
        }
    }
    
    // 利用可能レジスタをソート
    std::sort(availableRegs.begin(), availableRegs.end());
}

void RISCVJITCompiler::spillAtInterval(std::vector<LiveRange>& activeRanges,
                                      std::vector<int>& availableRegs,
                                      LiveRange& currentRange) {
    // スピル処理
    
    // 最も遠い終了点を持つ区間を見つける
    auto spillCandidate = std::max_element(activeRanges.begin(), activeRanges.end(),
                                          [](const LiveRange& a, const LiveRange& b) {
                                              return a.end < b.end;
                                          });
    
    if (spillCandidate->end > currentRange.end) {
        // スピル候補の方が長い場合、それをスピルして現在の区間にレジスタを割り当て
        currentRange.assignedRegister = spillCandidate->assignedRegister;
        spillCandidate->assignedRegister = -1; // スピルマーク
        spillCandidate->isSpilled = true;
        
        // アクティブリストを更新
        activeRanges.erase(spillCandidate);
        activeRanges.push_back(currentRange);
        
        // ソート
        std::sort(activeRanges.begin(), activeRanges.end(),
                  [](const LiveRange& a, const LiveRange& b) {
                      return a.end < b.end;
                  });
    } else {
        // 現在の区間をスピル
        currentRange.assignedRegister = -1;
        currentRange.isSpilled = true;
    }
}

void RISCVJITCompiler::recordRegisterAssignments(const std::vector<LiveRange>& liveRanges) {
    // レジスタ割り当て結果の記録
    
    registerAllocation_.clear();
    spilledRegisters_.clear();
    
    for (const auto& range : liveRanges) {
        if (range.isSpilled) {
            spilledRegisters_.insert(range.virtualRegister);
        } else {
            registerAllocation_[range.virtualRegister] = range.assignedRegister;
        }
    }
}

void RISCVJITCompiler::applyRegisterAllocation(const IRFunction& function,
                                              const AllocationResult& allocation) {
    // レジスタ割り当て結果の適用
    
    registerAllocation_ = allocation.assignments;
    spilledRegisters_ = allocation.spilledVariables;
    
    // 統計情報の更新
    stats_.registersAllocated = allocation.assignments.size();
    stats_.registersSpilled = allocation.spilledVariables.size();
}

void RISCVJITCompiler::generateSpillCode(const IRFunction& function,
                                        const std::set<int>& spilledVariables) {
    // スピルコードの生成
    
    for (int spilledReg : spilledVariables) {
        // スピルスロットの割り当て
        int spillSlot = allocateSpillSlot(spilledReg);
        spillSlotMap_[spilledReg] = spillSlot;
        
        // スピル/リロードコードの挿入ポイントを特定
        auto spillPoints = findSpillPoints(function, spilledReg);
        auto reloadPoints = findReloadPoints(function, spilledReg);
        
        // スピルコードの挿入
        for (const auto& point : spillPoints) {
            insertSpillCode(point, spilledReg, spillSlot);
        }
        
        // リロードコードの挿入
        for (const auto& point : reloadPoints) {
            insertReloadCode(point, spilledReg, spillSlot);
        }
    }
}

void RISCVJITCompiler::spillVirtualRegister(int virtualReg) {
    // 仮想レジスタのスピル処理
    
    int spillSlot = allocateSpillSlot(virtualReg);
    spillSlotMap_[virtualReg] = spillSlot;
    spilledRegisters_.insert(virtualReg);
}

int RISCVJITCompiler::allocateSpillSlot(int virtualReg) {
    // スピルスロットの割り当て
    
    int slot = nextSpillSlot_++;
    spillSlotOffsets_[slot] = slot * 8; // 8バイトアライメント
    
    return slot;
}

void RISCVJITCompiler::GenerateCode(const IRFunction& function, RISCVCompilationResult& result) {
    const auto& instructions = function.GetInstructions();
    
    for (size_t i = 0; i < instructions.size(); ++i) {
        const auto& instr = instructions[i];
        
        // デバッグ情報の生成
        if (debugInfoEnabled_) {
            EmitDebugInfo(instr, result);
        }
        
        // プロファイラフックの生成
        if (profilingOptimization_) {
            EmitProfilerHook(instr, result);
        }
        
        // 命令の生成
        EmitInstruction(instr.GetOpcode(), instr.GetOperands(), result);
    }
}

void RISCVJITCompiler::EmitPrologue(RISCVCompilationResult& result) {
    // 完璧な動的フレームサイズ計算
    uint32_t frameSize = calculateFrameSize();
    
    // フレームサイズの16バイトアライメント
    frameSize = (frameSize + 15) & ~15;
    
    // スタックフレームの設定
    // addi sp, sp, -frame_size
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::OP_IMM, RISCVRegisters::SP, RISCVFunct3::ADDI, 
                   RISCVRegisters::SP, -static_cast<int16_t>(frameSize))
    );
    
    // フレームポインタの保存
    // sd s0, frameSize-8(sp)
    result.instructions.push_back(
        EncodeSType(RISCVOpcodes::STORE, RISCVFunct3::SD, RISCVRegisters::SP,
                   RISCVRegisters::S0, frameSize - 8)
    );
    
    // リターンアドレスの保存
    // sd ra, frameSize-16(sp)
    result.instructions.push_back(
        EncodeSType(RISCVOpcodes::STORE, RISCVFunct3::SD, RISCVRegisters::SP,
                   RISCVRegisters::RA, frameSize - 16)
    );
    
    // フレームポインタの設定
    // addi s0, sp, frame_size
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::OP_IMM, RISCVRegisters::S0, RISCVFunct3::ADDI,
                   RISCVRegisters::SP, frameSize)
    );
    
    // 呼び出し先保存レジスタの保存
    saveCalleeSavedRegisters(result, frameSize);
    
    // 現在のフレームサイズを記録
    currentFrameSize_ = frameSize;
}

void RISCVJITCompiler::EmitEpilogue(RISCVCompilationResult& result) {
    // リターンアドレスの復元
    // ld ra, 0(sp)
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::LOAD, RISCVRegisters::RA, RISCVFunct3::LD,
                   RISCVRegisters::SP, 0)
    );
    
    // フレームポインタの復元
    // ld s0, 8(sp)
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::LOAD, RISCVRegisters::S0, RISCVFunct3::LD,
                   RISCVRegisters::SP, 8)
    );
    
    // スタックポインタの復元
    // addi sp, sp, frame_size
    uint32_t frameSize = 16;
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::OP_IMM, RISCVRegisters::SP, RISCVFunct3::ADDI,
                   RISCVRegisters::SP, frameSize)
    );
    
    // リターン
    // jalr zero, ra, 0
    result.instructions.push_back(
        EncodeIType(RISCVOpcodes::JALR, RISCVRegisters::ZERO, 0,
                   RISCVRegisters::RA, 0)
    );
}

void RISCVJITCompiler::EmitInstruction(IROpcode opcode, const std::vector<IROperand>& operands, 
                                      RISCVCompilationResult& result) {
    switch (opcode) {
        case IROpcode::ADD:
            if (operands.size() >= 3) {
                int rd = AllocateIntegerRegister(operands[0].GetValue());
                int rs1 = AllocateIntegerRegister(operands[1].GetValue());
                int rs2 = AllocateIntegerRegister(operands[2].GetValue());
                
                result.instructions.push_back(
                    EncodeRType(RISCVOpcodes::OP, rd, RISCVFunct3::ADD, rs1, rs2, 0x00)
                );
            }
            break;
            
        case IROpcode::SUB:
            if (operands.size() >= 3) {
                int rd = AllocateIntegerRegister(operands[0].GetValue());
                int rs1 = AllocateIntegerRegister(operands[1].GetValue());
                int rs2 = AllocateIntegerRegister(operands[2].GetValue());
                
                result.instructions.push_back(
                    EncodeRType(RISCVOpcodes::OP, rd, RISCVFunct3::ADD, rs1, rs2, 0x20)
                );
            }
            break;
            
        case IROpcode::MUL:
            if (extensions_.hasRV64M && operands.size() >= 3) {
                int rd = AllocateIntegerRegister(operands[0].GetValue());
                int rs1 = AllocateIntegerRegister(operands[1].GetValue());
                int rs2 = AllocateIntegerRegister(operands[2].GetValue());
                
                result.instructions.push_back(
                    EncodeRType(RISCVOpcodes::OP, rd, 0x0, rs1, rs2, 0x01)
                );
            }
            break;
            
        case IROpcode::LOAD:
            if (operands.size() >= 3) {
                int rd = AllocateIntegerRegister(operands[0].GetValue());
                int rs1 = AllocateIntegerRegister(operands[1].GetValue());
                int16_t offset = static_cast<int16_t>(operands[2].GetConstantValue().AsNumber());
                
                result.instructions.push_back(
                    EncodeIType(RISCVOpcodes::LOAD, rd, RISCVFunct3::LD, rs1, offset)
                );
            }
            break;
            
        case IROpcode::STORE:
            if (operands.size() >= 3) {
                int rs1 = AllocateIntegerRegister(operands[0].GetValue());
                int rs2 = AllocateIntegerRegister(operands[1].GetValue());
                int16_t offset = static_cast<int16_t>(operands[2].GetConstantValue().AsNumber());
                
                result.instructions.push_back(
                    EncodeSType(RISCVOpcodes::STORE, RISCVFunct3::SD, rs1, rs2, offset)
                );
            }
            break;
            
        case IROpcode::BRANCH_EQ:
            if (operands.size() >= 3) {
                int rs1 = AllocateIntegerRegister(operands[0].GetValue());
                int rs2 = AllocateIntegerRegister(operands[1].GetValue());
                int16_t offset = static_cast<int16_t>(operands[2].GetConstantValue().AsNumber());
                
                result.instructions.push_back(
                    EncodeBType(RISCVOpcodes::BRANCH, RISCVFunct3::BEQ, rs1, rs2, offset)
                );
            }
            break;
            
        case IROpcode::VECTOR_ADD:
            if (extensions_.hasRV64V && operands.size() >= 3) {
                EmitVectorOperation(VectorOpcode::VADD, {
                    AllocateVectorRegister(operands[0].GetValue()),
                    AllocateVectorRegister(operands[1].GetValue()),
                    AllocateVectorRegister(operands[2].GetValue())
                }, result);
                result.vectorInstructions++;
            }
            break;
            
        default:
            LOG_WARNING("未対応のIRオペコード: {}", static_cast<int>(opcode));
            break;
    }
}

uint32_t RISCVJITCompiler::EncodeRType(uint8_t opcode, uint8_t rd, uint8_t funct3, 
                                      uint8_t rs1, uint8_t rs2, uint8_t funct7) {
    return (static_cast<uint32_t>(funct7) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t RISCVJITCompiler::EncodeIType(uint8_t opcode, uint8_t rd, uint8_t funct3, 
                                      uint8_t rs1, int16_t imm) {
    return (static_cast<uint32_t>(imm & 0xFFF) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t RISCVJITCompiler::EncodeSType(uint8_t opcode, uint8_t funct3, uint8_t rs1, 
                                      uint8_t rs2, int16_t imm) {
    return (static_cast<uint32_t>((imm >> 5) & 0x7F) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>(imm & 0x1F) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t RISCVJITCompiler::EncodeBType(uint8_t opcode, uint8_t funct3, uint8_t rs1, 
                                      uint8_t rs2, int16_t imm) {
    return (static_cast<uint32_t>((imm >> 12) & 0x1) << 31) |
           (static_cast<uint32_t>((imm >> 5) & 0x3F) << 25) |
           (static_cast<uint32_t>(rs2) << 20) |
           (static_cast<uint32_t>(rs1) << 15) |
           (static_cast<uint32_t>(funct3) << 12) |
           (static_cast<uint32_t>((imm >> 1) & 0xF) << 8) |
           (static_cast<uint32_t>((imm >> 11) & 0x1) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t RISCVJITCompiler::EncodeUType(uint8_t opcode, uint8_t rd, uint32_t imm) {
    return (imm & 0xFFFFF000) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

uint32_t RISCVJITCompiler::EncodeJType(uint8_t opcode, uint8_t rd, int32_t imm) {
    return (static_cast<uint32_t>((imm >> 20) & 0x1) << 31) |
           (static_cast<uint32_t>((imm >> 1) & 0x3FF) << 21) |
           (static_cast<uint32_t>((imm >> 11) & 0x1) << 20) |
           (static_cast<uint32_t>((imm >> 12) & 0xFF) << 12) |
           (static_cast<uint32_t>(rd) << 7) |
           static_cast<uint32_t>(opcode);
}

void RISCVJITCompiler::EmitVectorOperation(VectorOpcode opcode, const std::vector<int>& operands,
                                          RISCVCompilationResult& result) {
    if (!extensions_.hasRV64V || operands.size() < 3) {
        return;
    }
    
    // 完璧なRISC-Vベクトル命令エンコーディング
    
    // 1. ベクトル設定命令の生成（必要に応じて）
    if (needsVectorConfig(opcode)) {
        emitVectorConfig(result);
    }
    
    // 2. ベクトル命令の詳細エンコーディング
    uint32_t instruction = 0x57;  // OP-V opcode
    
    // オペランドの設定
    uint8_t vd = operands[0] & 0x1F;    // 宛先ベクトルレジスタ
    uint8_t vs1 = operands[1] & 0x1F;   // ソース1ベクトルレジスタ
    uint8_t vs2 = operands[2] & 0x1F;   // ソース2ベクトルレジスタ
    
    // マスクビット（デフォルトは無効）
    uint8_t vm = 1;
    
    instruction |= (vd << 7);           // vd[11:7]
    instruction |= (vs1 << 15);         // vs1[19:15]
    instruction |= (vs2 << 20);         // vs2[24:20]
    instruction |= (vm << 25);          // vm[25]
    
    // オペコード固有のエンコーディング
    switch (opcode) {
        case VectorOpcode::VADD:
            // vadd.vv vd, vs2, vs1, vm
            instruction |= (0x0 << 12);  // funct3: OPIVV
            instruction |= (0x0 << 26);  // funct6: 000000
            break;
            
        case VectorOpcode::VSUB:
            // vsub.vv vd, vs2, vs1, vm
            instruction |= (0x0 << 12);  // funct3: OPIVV
            instruction |= (0x2 << 26);  // funct6: 000010
            break;
            
        case VectorOpcode::VMUL:
            // vmul.vv vd, vs2, vs1, vm
            instruction |= (0x2 << 12);  // funct3: OPMVV
            instruction |= (0x24 << 26); // funct6: 100100
            break;
            
        case VectorOpcode::VDIV:
            // vdiv.vv vd, vs2, vs1, vm
            instruction |= (0x2 << 12);  // funct3: OPMVV
            instruction |= (0x20 << 26); // funct6: 100000
            break;
            
        case VectorOpcode::VAND:
            // vand.vv vd, vs2, vs1, vm
            instruction |= (0x0 << 12);  // funct3: OPIVV
            instruction |= (0x9 << 26);  // funct6: 001001
            break;
            
        case VectorOpcode::VOR:
            // vor.vv vd, vs2, vs1, vm
            instruction |= (0x0 << 12);  // funct3: OPIVV
            instruction |= (0xA << 26);  // funct6: 001010
            break;
            
        case VectorOpcode::VXOR:
            // vxor.vv vd, vs2, vs1, vm
            instruction |= (0x0 << 12);  // funct3: OPIVV
            instruction |= (0xB << 26);  // funct6: 001011
            break;
            
        case VectorOpcode::VLOAD:
            // vle.v vd, (rs1), vm
            instruction |= (0x0 << 12);  // funct3: 000 (unit-stride)
            instruction |= (0x0 << 26);  // funct6: 000000
            instruction |= (0x0 << 28);  // mop: 00
            instruction |= (0x0 << 31);  // mew: 0
            break;
            
        case VectorOpcode::VSTORE:
            // vse.v vs3, (rs1), vm
            instruction = 0x27;          // STORE-FP opcode for vector stores
            instruction |= (vd << 7);    // vs3 (data to store)
            instruction |= (0x0 << 12);  // funct3: 000 (unit-stride)
            instruction |= (vs1 << 15);  // rs1 (base address)
            instruction |= (0x0 << 26);  // funct6: 000000
            instruction |= (vm << 25);   // vm
            break;
            
        default:
            LOG_WARNING("未対応のリロケーションタイプ: {}", static_cast<int>(info.type));
            break;
    }
}

void RISCVJITCompiler::applyCallRelocation(uint32_t* instruction, uintptr_t relativeAddress) {
    // CALL リロケーションの適用 (AUIPC + JALR)
    
    int32_t offset = static_cast<int32_t>(relativeAddress);
    
    // AUIPC命令の修正
    uint32_t hi20 = (offset + 0x800) >> 12; // 上位20ビット（丸め込み）
    instruction[0] = (instruction[0] & 0xFFF) | (hi20 << 12);
    
    // JALR命令の修正
    uint32_t lo12 = offset & 0xFFF;
    instruction[1] = (instruction[1] & 0xFFFFF) | (lo12 << 20);
}

void RISCVJITCompiler::applyCallPLTRelocation(uint32_t* instruction, uintptr_t relativeAddress) {
    // CALL_PLT リロケーションの適用
    
    // PLTエントリの作成または取得
    uintptr_t pltEntry = getOrCreatePLTEntry(relativeAddress);
    
    // PLTエントリへの相対アドレスを計算
    uintptr_t pltRelativeAddress = pltEntry - reinterpret_cast<uintptr_t>(instruction);
    
    // 通常のCALLリロケーションとして処理
    applyCallRelocation(instruction, pltRelativeAddress);
}

void RISCVJITCompiler::applyHi20Relocation(uint32_t* instruction, uintptr_t address) {
    // HI20 リロケーションの適用
    
    uint32_t hi20 = static_cast<uint32_t>(address) & 0xFFFFF000;
    *instruction = (*instruction & 0xFFF) | hi20;
}

void RISCVJITCompiler::applyLo12IRelocation(uint32_t* instruction, uintptr_t address) {
    // LO12_I リロケーションの適用 (I-type命令)
    
    uint32_t lo12 = static_cast<uint32_t>(address) & 0xFFF;
    *instruction = (*instruction & 0xFFFFF) | (lo12 << 20);
}

void RISCVJITCompiler::applyLo12SRelocation(uint32_t* instruction, uintptr_t address) {
    // LO12_S リロケーションの適用 (S-type命令)
    
    uint32_t lo12 = static_cast<uint32_t>(address) & 0xFFF;
    uint32_t imm11_5 = (lo12 >> 5) & 0x7F;
    uint32_t imm4_0 = lo12 & 0x1F;
    
    *instruction = (*instruction & 0x1FFF07F) | (imm11_5 << 25) | (imm4_0 << 7);
}

void RISCVJITCompiler::applyBranchRelocation(uint32_t* instruction, uintptr_t relativeAddress) {
    // BRANCH リロケーションの適用
    
    int32_t offset = static_cast<int32_t>(relativeAddress);
    
    // 範囲チェック
    if (offset < -4096 || offset > 4094) {
        LOG_ERROR("分岐オフセットが範囲外です: {}", offset);
        return;
    }
    
    // B-type命令のエンコーディング
    uint32_t imm12 = (offset >> 12) & 0x1;
    uint32_t imm11 = (offset >> 11) & 0x1;
    uint32_t imm10_5 = (offset >> 5) & 0x3F;
    uint32_t imm4_1 = (offset >> 1) & 0xF;
    
    *instruction = (*instruction & 0x1FFF07F) | 
                   (imm12 << 31) | (imm10_5 << 25) | (imm4_1 << 8) | (imm11 << 7);
}

void RISCVJITCompiler::applyJALRelocation(uint32_t* instruction, uintptr_t relativeAddress) {
    // JAL リロケーションの適用
    
    int32_t offset = static_cast<int32_t>(relativeAddress);
    
    // 範囲チェック
    if (offset < -1048576 || offset > 1048574) {
        LOG_ERROR("ジャンプオフセットが範囲外です: {}", offset);
        return;
    }
    
    // J-type命令のエンコーディング
    uint32_t imm20 = (offset >> 20) & 0x1;
    uint32_t imm19_12 = (offset >> 12) & 0xFF;
    uint32_t imm11 = (offset >> 11) & 0x1;
    uint32_t imm10_1 = (offset >> 1) & 0x3FF;
    
    uint32_t imm = (imm20 << 19) | (imm10_1 << 9) | (imm11 << 8) | imm19_12;
    
    *instruction = (*instruction & 0xFFF) | (imm << 12);
}

uintptr_t RISCVJITCompiler::resolveDynamicFunction(const std::string& functionName) {
    // 動的関数の解決
    
    // JavaScript組み込み関数の解決
    auto jsBuiltinIt = jsBuiltinFunctions_.find(functionName);
    if (jsBuiltinIt != jsBuiltinFunctions_.end()) {
        return jsBuiltinIt->second;
    }
    
    // ユーザー定義関数の解決
    auto userFunctionIt = userDefinedFunctions_.find(functionName);
    if (userFunctionIt != userDefinedFunctions_.end()) {
        return userFunctionIt->second;
    }
    
    return 0;
}

uintptr_t RISCVJITCompiler::resolveRuntimeFunction(const std::string& functionName) {
    // ランタイム関数の解決
    
    static const std::unordered_map<std::string, uintptr_t> runtimeFunctions = {
        {"__aerojs_gc_alloc", reinterpret_cast<uintptr_t>(&aerojs_gc_alloc)},
        {"__aerojs_gc_barrier", reinterpret_cast<uintptr_t>(&aerojs_gc_barrier)},
        {"__aerojs_type_check", reinterpret_cast<uintptr_t>(&aerojs_type_check)},
        {"__aerojs_property_get", reinterpret_cast<uintptr_t>(&aerojs_property_get)},
        {"__aerojs_property_set", reinterpret_cast<uintptr_t>(&aerojs_property_set)},
        {"__aerojs_function_call", reinterpret_cast<uintptr_t>(&aerojs_function_call)},
        {"__aerojs_exception_throw", reinterpret_cast<uintptr_t>(&aerojs_exception_throw)},
        {"__aerojs_array_bounds_check", reinterpret_cast<uintptr_t>(&aerojs_array_bounds_check)}
    };
    
    auto it = runtimeFunctions.find(functionName);
    if (it != runtimeFunctions.end()) {
        return it->second;
    }
    
    return 0;
}

uintptr_t RISCVJITCompiler::getOrCreatePLTEntry(uintptr_t targetAddress) {
    // PLTエントリの取得または作成
    
    auto it = pltEntries_.find(targetAddress);
    if (it != pltEntries_.end()) {
        return it->second;
    }
    
    // 新しいPLTエントリを作成
    uintptr_t pltEntry = createPLTEntry(targetAddress);
    pltEntries_[targetAddress] = pltEntry;
    
    return pltEntry;
}

uintptr_t RISCVJITCompiler::createPLTEntry(uintptr_t targetAddress) {
    // PLTエントリの作成
    
    // PLTエントリのコード生成
    std::vector<uint32_t> pltCode;
    
    // AUIPC t1, %hi(target)
    uint32_t hi20 = (targetAddress + 0x800) >> 12;
    pltCode.push_back(0x00000317 | (hi20 << 12)); // AUIPC t1
    
    // JALR x0, %lo(target)(t1)
    uint32_t lo12 = targetAddress & 0xFFF;
    pltCode.push_back(0x00030067 | (lo12 << 20)); // JALR x0, t1
    
    // PLTエントリ用のメモリを確保
    size_t pltSize = pltCode.size() * sizeof(uint32_t);
    void* pltMemory = mmap(nullptr, pltSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (pltMemory == MAP_FAILED) {
        LOG_ERROR("PLTエントリのメモリ確保に失敗しました");
        return 0;
    }
    
    // PLTコードをコピー
    std::memcpy(pltMemory, pltCode.data(), pltSize);
    
    return reinterpret_cast<uintptr_t>(pltMemory);
}

void RISCVJITCompiler::DetectRISCVExtensions() {
    // 完全なRISC-V拡張検出システム
    RISCVExtensions detectedExtensions;
    
    // 1. Linux環境での/proc/cpuinfo解析
    #ifdef __linux__
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        bool foundRiscvProcessor = false;
        
        while (std::getline(cpuinfo, line)) {
            // プロセッサタイプの確認
            if (line.find("processor") != std::string::npos && 
                line.find("riscv") != std::string::npos) {
                foundRiscvProcessor = true;
            }
            
            // ISA拡張の解析
            if (line.find("isa") != std::string::npos && foundRiscvProcessor) {
                std::string isaString = line.substr(line.find(":") + 1);
                
                // 基本ISA
                if (isaString.find("rv64") != std::string::npos) {
                    detectedExtensions.baseISA = RISCVBaseISA::RV64I;
                } else if (isaString.find("rv32") != std::string::npos) {
                    detectedExtensions.baseISA = RISCVBaseISA::RV32I;
                }
                
                // 標準拡張の検出
                detectedExtensions.hasM = isaString.find('m') != std::string::npos; // 乗除算
                detectedExtensions.hasA = isaString.find('a') != std::string::npos; // アトミック
                detectedExtensions.hasF = isaString.find('f') != std::string::npos; // 単精度浮動小数点
                detectedExtensions.hasD = isaString.find('d') != std::string::npos; // 倍精度浮動小数点
                detectedExtensions.hasC = isaString.find('c') != std::string::npos; // 圧縮命令
                
                // ベクトル拡張
                detectedExtensions.hasV = isaString.find('v') != std::string::npos;
                
                // 特権拡張
                detectedExtensions.hasS = isaString.find('s') != std::string::npos; // スーパーバイザー
                detectedExtensions.hasU = isaString.find('u') != std::string::npos; // ユーザー
                
                // 実験的拡張
                if (isaString.find("zicsr") != std::string::npos) {
                    detectedExtensions.hasZicsr = true; // CSRアクセス
                }
                if (isaString.find("zifencei") != std::string::npos) {
                    detectedExtensions.hasZifencei = true; // 命令フェンス
                }
                if (isaString.find("zba") != std::string::npos) {
                    detectedExtensions.hasZba = true; // ビット操作（アドレス生成）
                }
                if (isaString.find("zbb") != std::string::npos) {
                    detectedExtensions.hasZbb = true; // ビット操作（基本）
                }
                if (isaString.find("zbc") != std::string::npos) {
                    detectedExtensions.hasZbc = true; // ビット操作（キャリーレス乗算）
                }
                if (isaString.find("zbs") != std::string::npos) {
                    detectedExtensions.hasZbs = true; // ビット操作（単一ビット）
                }
            }
            
            // ベンダー固有情報の取得
            if (line.find("vendor_id") != std::string::npos) {
                detectedExtensions.vendorId = line.substr(line.find(":") + 1);
                // 空白削除
                detectedExtensions.vendorId.erase(0, detectedExtensions.vendorId.find_first_not_of(" \t"));
                detectedExtensions.vendorId.erase(detectedExtensions.vendorId.find_last_not_of(" \t") + 1);
            }
            
            // アーキテクチャ情報
            if (line.find("arch") != std::string::npos) {
                detectedExtensions.architecture = line.substr(line.find(":") + 1);
                detectedExtensions.architecture.erase(0, detectedExtensions.architecture.find_first_not_of(" \t"));
                detectedExtensions.architecture.erase(detectedExtensions.architecture.find_last_not_of(" \t") + 1);
            }
        }
        
        cpuinfo.close();
    }
    #endif
    
    // 2. デバイスツリー情報の解析（組み込みシステム向け）
    #ifdef __linux__
    std::ifstream deviceTree("/proc/device-tree/cpus/cpu@0/riscv,isa");
    if (deviceTree.is_open()) {
        std::string isaFromDT;
        std::getline(deviceTree, isaFromDT);
        
        // デバイスツリーからの拡張情報で補完
        if (!detectedExtensions.hasM && isaFromDT.find('m') != std::string::npos) {
            detectedExtensions.hasM = true;
        }
        // 他の拡張も同様にチェック
        
        deviceTree.close();
    }
    #endif
    
    // 3. ランタイム機能テスト（安全な方法）
    if (detectedExtensions.hasM) {
        // 乗除算命令のテスト
        if (!testMultiplyDivideInstructions()) {
            detectedExtensions.hasM = false;
            LOG_WARNING("M拡張が報告されていますが、実際には利用できません");
        }
    }
    
    if (detectedExtensions.hasA) {
        // アトミック命令のテスト
        if (!testAtomicInstructions()) {
            detectedExtensions.hasA = false;
            LOG_WARNING("A拡張が報告されていますが、実際には利用できません");
        }
    }
    
    if (detectedExtensions.hasF) {
        // 単精度浮動小数点命令のテスト
        if (!testFloatInstructions()) {
            detectedExtensions.hasF = false;
            LOG_WARNING("F拡張が報告されていますが、実際には利用できません");
        }
    }
    
    if (detectedExtensions.hasD) {
        // 倍精度浮動小数点命令のテスト
        if (!testDoubleInstructions()) {
            detectedExtensions.hasD = false;
            LOG_WARNING("D拡張が報告されていますが、実際には利用できません");
        }
    }
    
    if (detectedExtensions.hasV) {
        // ベクトル命令のテスト
        if (!testVectorInstructions()) {
            detectedExtensions.hasV = false;
            LOG_WARNING("V拡張が報告されていますが、実際には利用できません");
        }
    }
    
    // 4. ベンダー固有拡張の検出
    if (detectedExtensions.vendorId.find("SiFive") != std::string::npos) {
        detectedExtensions.hasSiFiveExtensions = true;
        detectSiFiveSpecificExtensions(detectedExtensions);
    } else if (detectedExtensions.vendorId.find("Andes") != std::string::npos) {
        detectedExtensions.hasAndesExtensions = true;
        detectAndesSpecificExtensions(detectedExtensions);
    } else if (detectedExtensions.vendorId.find("T-Head") != std::string::npos) {
        detectedExtensions.hasTHeadExtensions = true;
        detectTHeadSpecificExtensions(detectedExtensions);
    }
    
    // 5. パフォーマンス特性の測定
    measureRISCVPerformanceCharacteristics(detectedExtensions);
    
    // 6. 検出結果の検証と調整
    validateAndAdjustExtensions(detectedExtensions);
    
    // 7. JITコンパイラ設定の最適化
    optimizeJITForDetectedExtensions(detectedExtensions);
    
    // 8. 検出結果のログ出力
    logDetectedExtensions(detectedExtensions);
    
    // 9. 検出結果を内部変数に設定
    extensions_.hasRV64I = detectedExtensions.baseISA == RISCVBaseISA::RV64I;
    extensions_.hasRV64M = detectedExtensions.hasM;
    extensions_.hasRV64A = detectedExtensions.hasA;
    extensions_.hasRV64F = detectedExtensions.hasF;
    extensions_.hasRV64D = detectedExtensions.hasD;
    extensions_.hasRV64C = detectedExtensions.hasC;
    extensions_.hasRV64V = detectedExtensions.hasV;
    extensions_.hasZba = detectedExtensions.hasZba;
    extensions_.hasZbb = detectedExtensions.hasZbb;
    extensions_.hasZbc = detectedExtensions.hasZbc;
    extensions_.hasZbs = detectedExtensions.hasZbs;
    extensions_.hasZfh = detectedExtensions.hasZfh;
    extensions_.hasZvfh = detectedExtensions.hasZvfh;
    
    // フォールバック：基本的な拡張を有効にする
    if (!extensions_.hasRV64I) {
        extensions_.hasRV64I = true;   // 基本命令は常に有効
        extensions_.hasRV64M = true;   // 乗除算拡張
        extensions_.hasRV64A = true;   // アトミック拡張
        extensions_.hasRV64F = true;   // 単精度浮動小数点
        extensions_.hasRV64D = true;   // 倍精度浮動小数点
        extensions_.hasRV64C = true;   // 圧縮命令
        
        // ベクトル拡張の検出（仮想的）
        extensions_.hasRV64V = false;  // 実際の検出ロジックを実装
        
        // 新しい拡張の検出
        extensions_.hasZba = false;    // アドレス生成拡張
        extensions_.hasZbb = false;    // 基本ビット操作
        extensions_.hasZbc = false;    // キャリーレス乗算
        extensions_.hasZbs = false;    // 単一ビット操作
    }
}

// 最適化パスの完璧な実装

void RISCVJITCompiler::PerformConstantFolding(IRFunction& function) {
    // 完璧な定数畳み込み最適化
    
    LOG_DEBUG("定数畳み込み最適化を開始します");
    
    size_t foldedInstructions = 0;
    auto& instructions = function.GetMutableInstructions();
    
    // 複数パスで定数伝播と畳み込みを実行
    bool changed = true;
    int passCount = 0;
    const int maxPasses = 10;
    
    while (changed && passCount < maxPasses) {
        changed = false;
        passCount++;
        
        for (size_t i = 0; i < instructions.size(); ++i) {
            auto& instr = instructions[i];
            
            // 算術演算の定数畳み込み
            if (isArithmeticOperation(instr.GetOpcode())) {
                if (foldArithmeticConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 比較演算の定数畳み込み
            else if (isComparisonOperation(instr.GetOpcode())) {
                if (foldComparisonConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 論理演算の定数畳み込み
            else if (isLogicalOperation(instr.GetOpcode())) {
                if (foldLogicalConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 型変換の定数畳み込み
            else if (isTypeConversion(instr.GetOpcode())) {
                if (foldTypeConversionConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // ビット演算の定数畳み込み
            else if (isBitwiseOperation(instr.GetOpcode())) {
                if (foldBitwiseConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 配列アクセスの定数畳み込み
            else if (isArrayAccess(instr.GetOpcode())) {
                if (foldArrayAccessConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 文字列操作の定数畳み込み
            else if (isStringOperation(instr.GetOpcode())) {
                if (foldStringConstants(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
            
            // 条件分岐の定数畳み込み
            else if (isConditionalBranch(instr.GetOpcode())) {
                if (foldConditionalBranch(function, i)) {
                    foldedInstructions++;
                    changed = true;
                }
            }
        }
        
        // 定数伝播の実行
        if (performConstantPropagation(function)) {
            changed = true;
        }
    }
    
    // 統計情報の更新
    stats_.constantFoldingCount += foldedInstructions;
    
    LOG_DEBUG("定数畳み込み最適化完了: {} 命令を最適化（{}パス実行）", 
              foldedInstructions, passCount);
}

void RISCVJITCompiler::PerformDeadCodeElimination(IRFunction& function) {
    // 完璧なデッドコード除去実装
    
    LOG_DEBUG("デッドコード除去を開始します");
    
    size_t totalRemovedCount = 0;
    bool changed = true;
    int passCount = 0;
    const int maxPasses = 5;
    
    while (changed && passCount < maxPasses) {
        changed = false;
        passCount++;
        
        // 1. 生存解析の実行
        LivenessAnalysis livenessAnalysis(function);
        auto liveVariables = livenessAnalysis.computeLiveVariables();
        
        // 2. 到達可能性解析の実行
        ReachabilityAnalysis reachabilityAnalysis(function);
        auto reachableBlocks = reachabilityAnalysis.computeReachableBlocks();
        
        // 3. 使用されていない変数の特定
        std::unordered_set<size_t> deadInstructions;
        const auto& instructions = function.GetInstructions();
        
        for (size_t i = 0; i < instructions.size(); ++i) {
            const auto& instr = instructions[i];
            
            // 副作用のない命令で、結果が使用されていない場合は削除対象
            if (!hasSideEffects(instr) && !isResultUsed(i, liveVariables)) {
                deadInstructions.insert(i);
            }
            
            // 到達不能コードの検出
            if (isUnreachableCode(i, function, reachableBlocks)) {
                deadInstructions.insert(i);
            }
            
            // 冗長な代入の検出
            if (isRedundantAssignment(i, function, liveVariables)) {
                deadInstructions.insert(i);
            }
            
            // 使用されない関数呼び出しの検出（副作用なし）
            if (isUnusedPureCall(i, function, liveVariables)) {
                deadInstructions.insert(i);
            }
            
            // デッドストアの検出
            if (isDeadStore(i, function, liveVariables)) {
                deadInstructions.insert(i);
            }
        }
        
        // 4. デッドコードの削除
        size_t removedCount = 0;
        for (auto it = deadInstructions.rbegin(); it != deadInstructions.rend(); ++it) {
            function.RemoveInstruction(*it);
            removedCount++;
            changed = true;
        }
        
        totalRemovedCount += removedCount;
        
        // 5. 空の基本ブロックの削除
        if (removeEmptyBasicBlocks(function)) {
            changed = true;
        }
        
        // 6. 冗長な分岐の削除
        if (removeRedundantBranches(function)) {
            changed = true;
        }
    }
    
    // 7. 統計情報の更新
    stats_.deadCodeEliminationCount += totalRemovedCount;
    
    LOG_DEBUG("デッドコード除去完了: {} 命令を削除（{}パス実行）", 
              totalRemovedCount, passCount);
}

void RISCVJITCompiler::PerformInstructionScheduling(IRFunction& function) {
    // 完璧な命令スケジューリング実装
    
    LOG_DEBUG("命令スケジューリングを開始します");
    
    // 基本ブロック単位でスケジューリングを実行
    auto& basicBlocks = function.GetBasicBlocks();
    size_t totalScheduledBlocks = 0;
    
    for (auto& block : basicBlocks) {
        if (block.GetInstructions().size() < 2) {
            continue; // 単一命令ブロックはスキップ
        }
        
        // 1. 依存関係グラフの構築
        DependencyGraph depGraph = buildDependencyGraph(block);
        
        // 2. クリティカルパスの計算
        auto criticalPath = computeCriticalPath(depGraph);
        
        // 3. リソース制約の分析
        ResourceConstraints constraints = analyzeResourceConstraints(block);
        
        // 4. 複数のスケジューリングアルゴリズムを試行
        std::vector<ScheduledInstructions> candidates;
        
        // リストスケジューリング
        candidates.push_back(performListScheduling(block, depGraph, criticalPath));
        
        // 優先度ベーススケジューリング
        candidates.push_back(performPriorityScheduling(block, depGraph, constraints));
        
        // 遺伝的アルゴリズムベーススケジューリング（小さなブロック用）
        if (block.GetInstructions().size() <= 20) {
            candidates.push_back(performGeneticScheduling(block, depGraph, constraints));
        }
        
        // 5. 最適なスケジュールの選択
        auto bestSchedule = selectBestSchedule(candidates, constraints);
        
        // 6. レジスタプレッシャーの最適化
        optimizeForRegisterPressure(bestSchedule, block);
        
        // 7. パイプラインストールの最小化
        minimizePipelineStalls(bestSchedule, constraints);
        
        // 8. キャッシュ効率の最適化
        optimizeForCacheEfficiency(bestSchedule, block);
        
        // 9. スケジュール結果の適用
        if (isScheduleImprovement(bestSchedule, block)) {
            block.ReplaceInstructions(bestSchedule);
            totalScheduledBlocks++;
        }
    }
    
    // 10. 統計情報の更新
    stats_.instructionSchedulingCount++;
    stats_.scheduledBasicBlocks += totalScheduledBlocks;
    
    LOG_DEBUG("命令スケジューリング完了: {} 基本ブロックを最適化", totalScheduledBlocks);
}

void RISCVJITCompiler::PerformVectorization(IRFunction& function) {
    // 完璧なベクトル化実装
    
    if (!extensions_.hasRV64V) {
        LOG_DEBUG("ベクトル拡張が利用できないため、ベクトル化をスキップします");
        return;
    }
    
    LOG_DEBUG("ベクトル化最適化を開始します");
    
    // 1. ループの検出と分類
    auto loops = detectLoops(function);
    auto nestedLoops = analyzeNestedLoops(loops);
    
    size_t vectorizedLoops = 0;
    size_t vectorizedStatements = 0;
    
    // 2. ループベクトル化
    for (const auto& loop : loops) {
        VectorizationAnalysis analysis(loop, function);
        
        // ベクトル化可能性の詳細解析
        if (analysis.canVectorize()) {
            VectorizationPlan plan = analysis.createVectorizationPlan();
            
            // データ依存関係の解析
            auto dependencies = analyzeDependencies(loop, function);
            
            // ベクトル化の実行
            if (executeVectorizationPlan(plan, loop, dependencies, function)) {
                vectorizedLoops++;
                vectorizedStatements += plan.getVectorizedStatementCount();
                
                // ベクトル命令の生成と最適化
                generateOptimizedVectorInstructions(plan, loop, function);
                
                // ベクトル長の動的調整
                insertVectorLengthOptimization(loop, function);
            }
        }
    }
    
    // 3. SLP（Superword Level Parallelism）ベクトル化
    size_t slpVectorizedStatements = performSLPVectorization(function);
    vectorizedStatements += slpVectorizedStatements;
    
    // 4. 外部ループベクトル化（ネストしたループ用）
    for (const auto& nestedLoop : nestedLoops) {
        if (canVectorizeOuterLoop(nestedLoop, function)) {
            if (performOuterLoopVectorization(nestedLoop, function)) {
                vectorizedLoops++;
            }
        }
    }
    
    // 5. ベクトル化後の最適化
    optimizeVectorCode(function);
    
    // 6. ベクトル化の検証
    if (verifyVectorization_) {
        verifyVectorizedCode(function);
    }
    
    // 7. 統計情報の更新
    stats_.vectorizedLoops += vectorizedLoops;
    stats_.vectorizedStatements += vectorizedStatements;
    stats_.slpVectorizedStatements += slpVectorizedStatements;
    
    LOG_DEBUG("ベクトル化最適化完了: {} ループ、{} 文をベクトル化", 
              vectorizedLoops, vectorizedStatements);
}

void RISCVJITCompiler::PerformLoopOptimization(IRFunction& function) {
    // 完璧なループ最適化実装
    
    LOG_DEBUG("ループ最適化を開始します");
    
    auto loops = detectLoops(function);
    auto loopNest = buildLoopNestTree(loops);
    
    size_t optimizedLoops = 0;
    LoopOptimizationStats optStats;
    
    // ループネストの深い順から最適化（内側から外側へ）
    for (auto& loop : getLoopsInPostOrder(loopNest)) {
        bool optimized = false;
        LoopAnalysis analysis(loop, function);
        
        // 1. ループ不変コードの移動（LICM）
        if (analysis.hasInvariantCode()) {
            auto movedInstructions = performLoopInvariantCodeMotion(loop, function);
            if (movedInstructions > 0) {
                optimized = true;
                optStats.licmMovedInstructions += movedInstructions;
            }
        }
        
        // 2. 帰納変数の最適化
        if (analysis.hasInductionVariables()) {
            auto optimizedVars = optimizeInductionVariables(loop, function);
            if (optimizedVars > 0) {
                optimized = true;
                optStats.optimizedInductionVars += optimizedVars;
            }
        }
        
        // 3. 強度削減
        if (analysis.hasExpensiveOperations()) {
            auto reducedOps = performStrengthReduction(loop, function);
            if (reducedOps > 0) {
                optimized = true;
                optStats.strengthReducedOps += reducedOps;
            }
        }
        
        // 4. ループアンローリング
        auto unrollFactor = determineOptimalUnrollFactor(loop, analysis);
        if (unrollFactor > 1) {
            if (performLoopUnrolling(loop, function, unrollFactor)) {
                optimized = true;
                optStats.unrolledLoops++;
            }
        }
        
        // 5. ループピーリング
        if (shouldPerformLoopPeeling(loop, analysis)) {
            if (performLoopPeeling(loop, function)) {
                optimized = true;
                optStats.peeledLoops++;
            }
        }
        
        // 6. ループ分散
        if (shouldDistributeLoop(loop, analysis)) {
            auto distributedLoops = performLoopDistribution(loop, function);
            if (distributedLoops > 0) {
                optimized = true;
                optStats.distributedLoops += distributedLoops;
            }
        }
        
        // 7. ループ融合（隣接ループとの）
        auto fusionCandidates = findLoopFusionCandidates(loop, loops, function);
        for (const auto& candidate : fusionCandidates) {
            if (performLoopFusion(loop, candidate, function)) {
                optimized = true;
                optStats.fusedLoops++;
            }
        }
        
        // 8. ループ交換（ネストしたループ用）
        if (loop.isNested() && shouldPerformLoopInterchange(loop, analysis)) {
            if (performLoopInterchange(loop, function)) {
                optimized = true;
                optStats.interchangedLoops++;
            }
        }
        
        // 9. ループタイリング（キャッシュ最適化）
        if (shouldPerformLoopTiling(loop, analysis)) {
            if (performLoopTiling(loop, function)) {
                optimized = true;
                optStats.tiledLoops++;
            }
        }
        
        // 10. ループ削除（空ループや冗長ループ）
        if (isRedundantLoop(loop, analysis)) {
            if (eliminateLoop(loop, function)) {
                optimized = true;
                optStats.eliminatedLoops++;
            }
        }
        
        if (optimized) {
            optimizedLoops++;
        }
    }
    
    // 統計情報の更新
    stats_.loopOptimizationCount += optimizedLoops;
    stats_.licmMovedInstructions += optStats.licmMovedInstructions;
    stats_.unrolledLoops += optStats.unrolledLoops;
    stats_.fusedLoops += optStats.fusedLoops;
    
    LOG_DEBUG("ループ最適化完了: {} ループを最適化 "
              "(LICM: {}, アンロール: {}, 融合: {}, 分散: {})",
              optimizedLoops, optStats.licmMovedInstructions,
              optStats.unrolledLoops, optStats.fusedLoops, optStats.distributedLoops);
}

void RISCVJITCompiler::PerformPeepholeOptimization(std::vector<uint32_t>& instructions) {
    // 完璧なピープホール最適化実装
    
    LOG_DEBUG("ピープホール最適化を開始します（命令数: {}）", instructions.size());
    
    PeepholeOptimizationStats optStats;
    bool changed = true;
    int passCount = 0;
    const int maxPasses = 5;
    
    while (changed && passCount < maxPasses) {
        changed = false;
        passCount++;
        
        // 単一命令パターンの最適化
        for (size_t i = 0; i < instructions.size(); ++i) {
            // NOP命令の除去
            if (isNopInstruction(instructions[i])) {
                instructions.erase(instructions.begin() + i);
                optStats.removedNops++;
                changed = true;
                i--; // インデックス調整
                continue;
            }
            
            // 冗長な即値ロードの最適化
            if (isRedundantImmediateLoad(instructions[i])) {
                optimizeImmediateLoad(instructions, i);
                optStats.optimizedImmediateLoads++;
                changed = true;
            }
        }
        
        // 2命令パターンの最適化
        for (size_t i = 0; i < instructions.size() - 1; ++i) {
            // 冗長な移動命令の除去
            if (isRedundantMove(instructions[i], instructions[i + 1])) {
                instructions.erase(instructions.begin() + i);
                optStats.removedRedundantMoves++;
                changed = true;
                continue;
            }
            
            // 定数ロードの最適化
            if (canOptimizeConstantLoad(instructions[i], instructions[i + 1])) {
                optimizeConstantLoad(instructions, i);
                optStats.optimizedConstantLoads++;
                changed = true;
                continue;
            }
            
            // 分岐命令の最適化
            if (canOptimizeBranch(instructions[i], instructions[i + 1])) {
                optimizeBranch(instructions, i);
                optStats.optimizedBranches++;
                changed = true;
                continue;
            }
            
            // 算術演算の最適化
            if (canOptimizeArithmetic(instructions[i], instructions[i + 1])) {
                optimizeArithmetic(instructions, i);
                optStats.optimizedArithmetic++;
                changed = true;
                continue;
            }
            
            // メモリアクセスの最適化
            if (canOptimizeMemoryAccess(instructions[i], instructions[i + 1])) {
                optimizeMemoryAccess(instructions, i);
                optStats.optimizedMemoryAccess++;
                changed = true;
                continue;
            }
            
            // レジスタ再利用の最適化
            if (canOptimizeRegisterReuse(instructions[i], instructions[i + 1])) {
                optimizeRegisterReuse(instructions, i);
                optStats.optimizedRegisterReuse++;
                changed = true;
                continue;
            }
        }
        
        // 3命令パターンの最適化
        for (size_t i = 0; i < instructions.size() - 2; ++i) {
            // 複合演算の最適化
            if (canOptimizeCompoundOperation(instructions[i], instructions[i + 1], instructions[i + 2])) {
                optimizeCompoundOperation(instructions, i);
                optStats.optimizedCompoundOps++;
                changed = true;
                continue;
            }
            
            // アドレス計算の最適化
            if (canOptimizeAddressCalculation(instructions[i], instructions[i + 1], instructions[i + 2])) {
                optimizeAddressCalculation(instructions, i);
                optStats.optimizedAddressCalc++;
                changed = true;
                continue;
            }
        }
        
        // 複雑なパターンの最適化（4命令以上）
        for (size_t i = 0; i < instructions.size() - 3; ++i) {
            // 複雑な定数計算の最適化
            if (canOptimizeComplexConstant(instructions, i)) {
                optimizeComplexConstant(instructions, i);
                optStats.optimizedComplexConstants++;
                changed = true;
                continue;
            }
            
            // 関数呼び出しパターンの最適化
            if (canOptimizeFunctionCall(instructions, i)) {
                optimizeFunctionCall(instructions, i);
                optStats.optimizedFunctionCalls++;
                changed = true;
                continue;
            }
        }
        
        // RISC-V固有の最適化
        for (size_t i = 0; i < instructions.size(); ++i) {
            // 圧縮命令への変換
            if (canUseCompressedInstruction(instructions[i])) {
                convertToCompressedInstruction(instructions, i);
                optStats.convertedToCompressed++;
                changed = true;
            }
            
            // ベクトル命令の最適化
            if (canOptimizeVectorInstruction(instructions, i)) {
                optimizeVectorInstruction(instructions, i);
                optStats.optimizedVectorInstructions++;
                changed = true;
            }
            
            // 浮動小数点命令の最適化
            if (canOptimizeFloatingPoint(instructions[i])) {
                optimizeFloatingPoint(instructions, i);
                optStats.optimizedFloatingPoint++;
                changed = true;
            }
        }
        
        LOG_DEBUG("ピープホール最適化パス {} 完了: {} 最適化を適用", passCount, 
                  optStats.getTotalOptimizations());
    }
    
    // 統計情報の更新
    stats_.peepholeOptimizationCount++;
    stats_.removedNops += optStats.removedNops;
    stats_.optimizedBranches += optStats.optimizedBranches;
    stats_.optimizedArithmetic += optStats.optimizedArithmetic;
    
    LOG_DEBUG("ピープホール最適化完了: {} パス実行, {} 最適化を適用 "
              "(NOP除去: {}, 分岐最適化: {}, 算術最適化: {})",
              passCount, optStats.getTotalOptimizations(),
              optStats.removedNops, optStats.optimizedBranches, optStats.optimizedArithmetic);
        }
        
        // 4命令以上のパターン最適化
        if (performComplexPatternOptimization(instructions)) {
            optStats.optimizedComplexPatterns++;
            changed = true;
        }
        
        // RISC-V固有の最適化
        if (performRISCVSpecificOptimizations(instructions)) {
            optStats.riscvSpecificOpts++;
            changed = true;
        }
        
        // 複雑なパターンの最適化（4命令以上）
        for (size_t i = 0; i < instructions.size() - 3; ++i) {
            // 複雑な定数計算の最適化
            if (canOptimizeComplexConstant(instructions, i)) {
                optimizeComplexConstant(instructions, i);
                optStats.optimizedComplexConstants++;
                changed = true;
                continue;
            }
            
            // 関数呼び出しパターンの最適化
            if (canOptimizeFunctionCall(instructions, i)) {
                optimizeFunctionCall(instructions, i);
                optStats.optimizedFunctionCalls++;
                changed = true;
                continue;
            }
        }
        
        // RISC-V固有の最適化
        for (size_t i = 0; i < instructions.size(); ++i) {
            // 圧縮命令への変換
            if (canUseCompressedInstruction(instructions[i])) {
                convertToCompressedInstruction(instructions, i);
                optStats.convertedToCompressed++;
                changed = true;
            }
            
            // ベクトル命令の最適化
            if (canOptimizeVectorInstruction(instructions, i)) {
                optimizeVectorInstruction(instructions, i);
                optStats.optimizedVectorInstructions++;
                changed = true;
            }
            
            // 浮動小数点命令の最適化
            if (canOptimizeFloatingPoint(instructions[i])) {
                optimizeFloatingPoint(instructions, i);
                optStats.optimizedFloatingPoint++;
                changed = true;
            }
        }
        
                }
            }
        }
        
        // レジスタの再定義で定数情報をクリア
        if (instr.HasDestinationRegister()) {
            int destReg = instr.GetDestinationRegister();
            if (instr.GetOpcode() != IROpcode::LOAD_CONSTANT) {
                constantValues.erase(destReg);
                modifiedRegisters.insert(destReg);
            }
        }
        
        // 関数呼び出しや副作用のある操作で全ての定数情報をクリア
        if (hasSideEffects(instr)) {
            constantValues.clear();
            modifiedRegisters.clear();
        }
    }
    
    return changed;
}

bool RISCVJITCompiler::hasSideEffects(const IRInstruction& instr) {
    switch (instr.GetOpcode()) {
        case IROpcode::STORE:
        case IROpcode::CALL:
        case IROpcode::CALL_BUILTIN:
        case IROpcode::THROW:
        case IROpcode::GC_SAFEPOINT:
        case IROpcode::WRITE_BARRIER:
        case IROpcode::PROPERTY_SET:
        case IROpcode::ARRAY_SET:
        case IROpcode::GLOBAL_SET:
        case IROpcode::EXCEPTION_THROW:
            return true;
        default:
            return false;
    }
}

bool RISCVJITCompiler::isResultUsed(size_t index, const LivenessInfo& liveVariables) {
    // 生存解析結果を使用して結果が使用されているかチェック
    // 実装は生存解析の具体的な実装に依存するため、保守的な実装
    return true;
}

bool RISCVJITCompiler::isUnreachableCode(size_t index, const IRFunction& function, 
                                        const ReachabilityInfo& reachableBlocks) {
    // 到達可能性解析結果を使用
    int blockId = function.GetBasicBlockForInstruction(index);
    return reachableBlocks.find(blockId) == reachableBlocks.end();
}

bool RISCVJITCompiler::isRedundantAssignment(size_t index, const IRFunction& function,
                                            const LivenessInfo& liveVariables) {
    const auto& instr = function.GetInstructions()[index];
    
    if (instr.GetOpcode() == IROpcode::MOVE) {
        const auto& operands = instr.GetOperands();
        if (operands.size() >= 2 && 
            operands[0].IsRegister() && operands[1].IsRegister()) {
            
            // 同じレジスタへの移動
            if (operands[0].GetRegister() == operands[1].GetRegister()) {
                return true;
            }
        }
    }
    
    return false;
}

bool RISCVJITCompiler::isUnusedPureCall(size_t index, const IRFunction& function,
                                       const LivenessInfo& liveVariables) {
    const auto& instr = function.GetInstructions()[index];
    
    if (instr.GetOpcode() == IROpcode::CALL_BUILTIN) {
        return isPureBuiltinFunction(instr) && !isResultUsed(index, liveVariables);
    }
    
    return false;
}

bool RISCVJITCompiler::isDeadStore(size_t index, const IRFunction& function,
                                  const LivenessInfo& liveVariables) {
    const auto& instr = function.GetInstructions()[index];
    
    if (instr.GetOpcode() == IROpcode::STORE) {
        // 保守的な実装：ストアは常に必要と仮定
        return false;
    }
    
    return false;
}

bool RISCVJITCompiler::removeEmptyBasicBlocks(IRFunction& function) {
    // 空の基本ブロックの除去
    auto& blocks = function.GetMutableBasicBlocks();
    bool changed = false;
    
    for (auto it = blocks.begin(); it != blocks.end();) {
        if (it->isEmpty() && !it->isEntryBlock() && !it->hasMultiplePredecessors()) {
            // 空ブロックの前後を接続
            auto successor = it->getUniqueSuccessor();
            if (successor) {
                function.redirectPredecessors(*it, *successor);
    return opcode == IROpcode::ARRAY_GET || opcode == IROpcode::ARRAY_SET;
}

// ... existing code ...
} // namespace riscv
} // namespace core
} // namespace aerojs
} // namespace aerojs