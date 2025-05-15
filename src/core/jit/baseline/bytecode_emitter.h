/**
 * @file bytecode_emitter.h
 * @brief JavaScriptのASTからバイトコードを生成するエミッター
 * @version 1.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <memory>

namespace aerojs {

// 前方宣言
class Context;
class Function;
class Node;
class Expression;
class Statement;
class Variable;

/**
 * @brief バイトコードエミッター
 *
 * JavaScriptのASTからバイトコードを生成するクラス
 */
class BytecodeEmitter {
public:
    // コンストラクタ
    explicit BytecodeEmitter(Context* context);
    
    // デストラクタ
    ~BytecodeEmitter();
    
    // バイトコード生成
    bool lower(Function* function, std::vector<uint8_t>& outputBuffer);
    
    // バイトコード最適化
    void optimize(std::vector<uint8_t>& bytecode);
    
    // デバッグ情報
    struct DebugInfo {
        std::unordered_map<uint32_t, uint32_t> offsetToLine;  // バイトコードオフセットから行番号へのマッピング
        std::unordered_map<uint32_t, std::string> variables;  // 変数IDから名前へのマッピング
    };
    
    const DebugInfo& getDebugInfo() const { return _debugInfo; }
    
private:
    // コンテキスト
    Context* _context;
    
    // デバッグ情報
    DebugInfo _debugInfo;
    
    // エミット状態
    struct {
        Function* currentFunction;
        uint32_t nextRegister;
        uint32_t scopeDepth;
        std::vector<uint8_t> buffer;
        std::unordered_map<std::string, uint32_t> stringConstants;
        std::unordered_map<double, uint32_t> numberConstants;
        std::vector<uint32_t> breakTargets;
        std::vector<uint32_t> continueTargets;
    } _state;
    
    // エミットヘルパー
    void emitByte(uint8_t byte);
    void emitUint16(uint16_t value);
    void emitUint32(uint32_t value);
    void emitOpcode(uint8_t opcode);
    void emitJump(uint8_t opcode);
    void patchJump(uint32_t jumpOffset);
    
    // レジスタ管理
    uint32_t allocateRegister();
    void freeRegister();
    void resetRegisterAllocation();
    
    // 定数管理
    uint32_t addStringConstant(const std::string& str);
    uint32_t addNumberConstant(double value);
    
    // コード生成
    uint32_t emitNode(Node* node);
    uint32_t emitExpression(Expression* expr);
    void emitStatement(Statement* stmt);
    
    // 特定の式のコード生成
    uint32_t emitBinaryExpression(Expression* expr);
    uint32_t emitCallExpression(Expression* expr);
    uint32_t emitAssignmentExpression(Expression* expr);
    uint32_t emitIdentifier(Expression* expr);
    uint32_t emitLiteral(Expression* expr);
    uint32_t emitObjectExpression(Expression* expr);
    uint32_t emitArrayExpression(Expression* expr);
    
    // 特定の文のコード生成
    void emitBlockStatement(Statement* stmt);
    void emitIfStatement(Statement* stmt);
    void emitLoopStatement(Statement* stmt);
    void emitReturnStatement(Statement* stmt);
    void emitVariableDeclaration(Statement* stmt);
    void emitFunctionDeclaration(Statement* stmt);
    
    // スコープ管理
    void enterScope();
    void leaveScope();
    
    // 変数管理
    uint32_t resolveVariable(const std::string& name);
    uint32_t declareVariable(const std::string& name);
    
    // 初期設定
    void initialize();
    void reset();
};

} // namespace aerojs 