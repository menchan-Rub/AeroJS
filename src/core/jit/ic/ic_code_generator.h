#pragma once

#include <string>
#include <vector>
#include <memory>
#include "inline_cache.h"
#include "../jit_code_generator.h"
#include "../../object/js_object.h"
#include "../../value/js_value.h"

namespace aerojs {
namespace core {

// 前方宣言
class JITCompiler;
class IRFunction;
class IRInstruction;
class CodeBlock;

/**
 * @brief インラインキャッシュを利用するための特殊なJITコード生成クラス
 * 
 * この特殊なコードジェネレータは、プロパティアクセスやメソッド呼び出しに対して
 * インラインキャッシュを活用した最適化コードを生成します。
 */
class ICCodeGenerator : public JITCodeGenerator {
public:
    /**
     * @brief コンストラクタ
     * @param compiler 関連するJITコンパイラ
     */
    explicit ICCodeGenerator(JITCompiler* compiler);
    
    /**
     * @brief デストラクタ
     */
    ~ICCodeGenerator() override;
    
    /**
     * @brief 指定されたIR関数に対してインラインキャッシュを利用したコードを生成
     * @param func コード生成対象のIR関数
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成したコードのエントリポイント
     */
    void* GenerateCode(IRFunction* func, CodeBlock* codeBlock) override;
    
    /**
     * @brief プロパティアクセスに対してインラインキャッシュを利用したコードを生成
     * @param inst プロパティアクセス命令
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GeneratePropertyAccess(IRInstruction* inst, CodeBlock* codeBlock);
    
    /**
     * @brief プロパティ設定に対してインラインキャッシュを利用したコードを生成
     * @param inst プロパティ設定命令
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GeneratePropertySet(IRInstruction* inst, CodeBlock* codeBlock);
    
    /**
     * @brief メソッド呼び出しに対してインラインキャッシュを利用したコードを生成
     * @param inst メソッド呼び出し命令
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateMethodCall(IRInstruction* inst, CodeBlock* codeBlock);
    
    /**
     * @brief モノモーフィック状態のプロパティアクセスコードを生成
     * @param objReg オブジェクトのレジスタ
     * @param resultReg 結果を格納するレジスタ
     * @param propertyName プロパティ名
     * @param cacheId キャッシュID
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateMonomorphicPropertyAccess(
        int objReg, 
        int resultReg, 
        const std::string& propertyName, 
        uint32_t cacheId, 
        CodeBlock* codeBlock
    );
    
    /**
     * @brief ポリモーフィック状態のプロパティアクセスコードを生成
     * @param objReg オブジェクトのレジスタ
     * @param resultReg 結果を格納するレジスタ
     * @param propertyName プロパティ名
     * @param cacheId キャッシュID
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GeneratePolymorphicPropertyAccess(
        int objReg, 
        int resultReg, 
        const std::string& propertyName, 
        uint32_t cacheId, 
        CodeBlock* codeBlock
    );
    
    /**
     * @brief モノモーフィック状態のメソッド呼び出しコードを生成
     * @param objReg オブジェクトのレジスタ
     * @param resultReg 結果を格納するレジスタ
     * @param argsRegs 引数のレジスタ配列
     * @param methodName メソッド名
     * @param cacheId キャッシュID
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateMonomorphicMethodCall(
        int objReg, 
        int resultReg, 
        const std::vector<int>& argsRegs,
        const std::string& methodName, 
        uint32_t cacheId, 
        CodeBlock* codeBlock
    );
    
    /**
     * @brief プロトタイプチェーンを使用するプロパティアクセスコードを生成
     * @param objReg オブジェクトのレジスタ
     * @param resultReg 結果を格納するレジスタ
     * @param propertyName プロパティ名
     * @param cacheId キャッシュID
     * @param protoDepth プロトタイプチェーンの深さ
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateProtoPropertyAccess(
        int objReg, 
        int resultReg, 
        const std::string& propertyName, 
        uint32_t cacheId,
        uint32_t protoDepth,
        CodeBlock* codeBlock
    );
    
    /**
     * @brief インラインキャッシュステータスに基づいて適切なコード生成戦略を選択
     * @param inst 元のIR命令
     * @param cacheId キャッシュID
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t SelectICStrategy(IRInstruction* inst, uint32_t cacheId, CodeBlock* codeBlock);
    
    /**
     * @brief メガモーフィック状態のフォールバックコードを生成（通常の低速パスを使用）
     * @param inst 元のIR命令
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateMegamorphicFallback(IRInstruction* inst, CodeBlock* codeBlock);
    
    /**
     * @brief インラインキャッシュを無効化するためのガード条件コードを生成
     * @param objReg オブジェクトのレジスタ
     * @param shapeId 予期される形状ID
     * @param slowPathLabel 低速パスのジャンプラベル
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateICGuard(
        int objReg, 
        uint32_t shapeId, 
        const std::string& slowPathLabel, 
        CodeBlock* codeBlock
    );
    
private:
    /**
     * @brief キャッシュIDを生成
     * @param propertyName プロパティ名またはメソッド名
     * @param isMethod メソッドキャッシュか否か
     * @return 生成されたキャッシュID
     */
    uint32_t GetOrCreateCacheId(const std::string& propertyName, bool isMethod);
    
    /**
     * @brief インラインキャッシュを使用するかどうかを決定
     * @param inst 元のIR命令
     * @return キャッシュを使用すべきか否か
     */
    bool ShouldUseInlineCache(IRInstruction* inst) const;
    
    /**
     * @brief 適切なキャッシュアップグレード戦略を適用
     * @param cacheId キャッシュID
     * @param isMethod メソッドキャッシュか否か
     * @return アップグレード後のキャッシュステータス
     */
    ICStatus UpgradeCacheIfNeeded(uint32_t cacheId, bool isMethod);
    
    /**
     * @brief バインディングレジスタ生成に使用するスタブコードを生成
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたスタブコードのオフセット
     */
    size_t GenerateBindingStub(CodeBlock* codeBlock);
    
    /**
     * @brief キャッシュミス時のフォールバックコードを生成
     * @param objReg オブジェクトのレジスタ
     * @param resultReg 結果を格納するレジスタ
     * @param propertyName プロパティ名
     * @param codeBlock 生成コードの保存先となるコードブロック
     * @return 生成されたコードオフセット
     */
    size_t GenerateCacheMissFallback(
        int objReg, 
        int resultReg, 
        const std::string& propertyName, 
        CodeBlock* codeBlock
    );
    
    /** JITコンパイラへの参照 */
    JITCompiler* m_compiler;
    
    /** 命令ごとのキャッシュIDマップ */
    std::unordered_map<IRInstruction*, uint32_t> m_instructionToCacheId;
    
    /** IC生成のためのヘルパー関数ポインタのキャッシュ */
    void* m_getCachedPropertyHelper;
    void* m_setCachedPropertyHelper;
    void* m_callCachedMethodHelper;
    
    /** ICコード生成ヒューリスティック設定 */
    static constexpr size_t kMaxInlinedICSize = 64;  // インライン展開する最大ICコードサイズ
    static constexpr uint32_t kMaxProtoChainDepth = 2;  // 最適化するプロトタイプチェーンの最大深さ
};

} // namespace core
} // namespace aerojs 