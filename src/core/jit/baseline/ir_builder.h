#pragma once

#include <vector>
#include <functional>
#include "../ir/ir.h"
#include "../bytecode/bytecode_opcodes.h"

namespace aerojs {
namespace core {

// 前方宣言
class JITProfiler;

/**
 * IRBuilder クラス
 * バイトコードからIR（中間表現）を構築するためのクラス
 */
class IRBuilder {
public:
    IRBuilder();
    ~IRBuilder();
    
    /**
     * バイトコードをIR関数に変換
     * @param bytecodes バイトコード列
     * @param functionId 関数ID
     * @return 構築されたIR関数
     */
    std::unique_ptr<IRFunction> BuildFromBytecode(const std::vector<uint8_t>& bytecodes, uint32_t functionId);
    
    /**
     * プロファイラを設定
     * @param profiler JITプロファイラインスタンス
     */
    void SetProfiler(JITProfiler* profiler);
    
    /**
     * 命令エミットコールバックを設定
     * @param callback 命令がエミットされた時に呼び出される関数
     */
    void SetInstructionEmitCallback(std::function<void(uint32_t, IROpcode, uint32_t)> callback);
    
    /**
     * 型推論コールバックを設定
     * @param callback 型が推論された時に呼び出される関数
     */
    void SetTypeInferenceCallback(std::function<void(uint32_t, uint32_t, IRType)> callback);
    
    /**
     * 最適化設定を行う
     * @param enable 最適化を有効にするかどうか
     */
    void EnableOptimizations(bool enable);
    
private:
    // 内部実装
    JITProfiler* m_profiler;
    bool m_enableOptimizations;
    
    // コールバック関数
    std::function<void(uint32_t, IROpcode, uint32_t)> m_instructionEmitCallback;
    std::function<void(uint32_t, uint32_t, IRType)> m_typeInferenceCallback;
    
    // IR構築メソッド
    void BuildBasicBlocks(const std::vector<uint8_t>& bytecodes, IRFunction& function);
    void EmitIRForBytecode(const uint8_t* bytecode, size_t offset, IRFunction& function);
    
    // バイトコード命令からIR命令へのマッピング
    IROpcode MapBytecodeToIROpcode(BytecodeOpcode bytecodeOp);
    
    // 型推論
    IRType InferType(const IRInstruction& inst);
    
    // プロファイル情報を用いた最適化
    void OptimizeUsingProfile(IRFunction& function, uint32_t functionId);
};

} // namespace core
} // namespace aerojs 