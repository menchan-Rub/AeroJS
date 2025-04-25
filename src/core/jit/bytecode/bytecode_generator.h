#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "bytecode_structure.h"
#include "bytecode_opcodes.h"

namespace aerojs {
namespace core {

// 抽象構文木のノードタイプの前方宣言
class ASTNode;
class Expression;
class Statement;
class Program;
class FunctionDeclaration;
class ClassDeclaration;
class BlockStatement;
class IfStatement;
class SwitchStatement;
class ForStatement;
class ForInStatement;
class ForOfStatement;
class WhileStatement;
class DoWhileStatement;
class TryStatement;
class ThrowStatement;
class ReturnStatement;
class BreakStatement;
class ContinueStatement;
class ExpressionStatement;
class VariableDeclaration;
class BinaryExpression;
class UnaryExpression;
class CallExpression;
class MemberExpression;
class AssignmentExpression;
class ObjectExpression;
class ArrayExpression;
class Identifier;
class Literal;
class ThisExpression;
class NewExpression;

// バイトコードジェネレーターのコンテキストクラス
class BytecodeGeneratorContext {
public:
    BytecodeGeneratorContext() = default;
    ~BytecodeGeneratorContext() = default;

    // 現在のスコープに変数を追加し、そのインデックスを返す
    uint32_t AddVariable(const std::string& name);
    
    // 変数名からインデックスを取得
    int32_t GetVariableIndex(const std::string& name) const;
    
    // 定数をプールに追加し、そのインデックスを返す
    uint32_t AddConstant(const ConstantValue& value);
    
    // 文字列をテーブルに追加し、そのインデックスを返す
    uint32_t AddString(const std::string& str);
    
    // 新しいスコープに入る
    void EnterScope();
    
    // 現在のスコープを出る
    void ExitScope();
    
    // ループのコンテキストに入る
    void EnterLoop(uint32_t startLabel, uint32_t endLabel);
    
    // ループのコンテキストを出る
    void ExitLoop();
    
    // breakステートメントのジャンプ先を取得
    uint32_t GetBreakTarget() const;
    
    // continueステートメントのジャンプ先を取得
    uint32_t GetContinueTarget() const;
    
    // 新しいラベルを生成
    uint32_t CreateLabel();
    
    // ラベルの位置を設定
    void SetLabelPosition(uint32_t label, uint32_t position);
    
    // ラベルの位置を取得
    uint32_t GetLabelPosition(uint32_t label) const;
    
    // 例外ハンドラを追加
    uint32_t AddExceptionHandler(const ExceptionHandler& handler);

private:
    // 変数スコープのスタック
    std::vector<std::unordered_map<std::string, uint32_t>> m_variableScopes;
    
    // 定数プール
    std::vector<ConstantValue> m_constantPool;
    
    // 文字列テーブル
    std::vector<std::string> m_stringTable;
    
    // ループスタック（開始ラベルと終了ラベルのペア）
    std::vector<std::pair<uint32_t, uint32_t>> m_loopStack;
    
    // ラベルのマップ（ラベル番号 -> バイトコード位置）
    std::unordered_map<uint32_t, uint32_t> m_labelPositions;
    
    // 次に割り当てるラベル番号
    uint32_t m_nextLabel = 0;
    
    // 例外ハンドラリスト
    std::vector<ExceptionHandler> m_exceptionHandlers;
};

// バイトコードジェネレータークラス
class BytecodeGenerator {
public:
    BytecodeGenerator();
    ~BytecodeGenerator();

    // プログラム全体からバイトコードモジュールを生成
    std::unique_ptr<BytecodeModule> Generate(const Program& program);
    
    // 単一の式からバイトコードを生成（評価用）
    std::unique_ptr<BytecodeFunction> GenerateForEval(const Expression& expression);

private:
    // 各種ASTノードのバイトコード生成
    void GenerateProgram(const Program& program);
    void GenerateFunction(const FunctionDeclaration& func);
    void GenerateClass(const ClassDeclaration& cls);
    void GenerateBlock(const BlockStatement& block);
    void GenerateIf(const IfStatement& ifStmt);
    void GenerateSwitch(const SwitchStatement& switchStmt);
    void GenerateFor(const ForStatement& forStmt);
    void GenerateForIn(const ForInStatement& forInStmt);
    void GenerateForOf(const ForOfStatement& forOfStmt);
    void GenerateWhile(const WhileStatement& whileStmt);
    void GenerateDoWhile(const DoWhileStatement& doWhileStmt);
    void GenerateTry(const TryStatement& tryStmt);
    void GenerateThrow(const ThrowStatement& throwStmt);
    void GenerateReturn(const ReturnStatement& returnStmt);
    void GenerateBreak(const BreakStatement& breakStmt);
    void GenerateContinue(const ContinueStatement& continueStmt);
    void GenerateExpression(const ExpressionStatement& exprStmt);
    void GenerateVariableDeclaration(const VariableDeclaration& varDecl);
    
    // 式の評価結果をスタックに残すコード生成
    void GenerateBinaryExpression(const BinaryExpression& binExpr);
    void GenerateUnaryExpression(const UnaryExpression& unaryExpr);
    void GenerateCallExpression(const CallExpression& callExpr);
    void GenerateMemberExpression(const MemberExpression& memberExpr);
    void GenerateAssignmentExpression(const AssignmentExpression& assignExpr);
    void GenerateObjectExpression(const ObjectExpression& objExpr);
    void GenerateArrayExpression(const ArrayExpression& arrayExpr);
    void GenerateIdentifier(const Identifier& id);
    void GenerateLiteral(const Literal& literal);
    void GenerateThisExpression(const ThisExpression& thisExpr);
    void GenerateNewExpression(const NewExpression& newExpr);
    
    // 生成中の現在の関数にバイトコード命令を追加
    void EmitOpcode(BytecodeOpcode opcode);
    void EmitOpcodeWithOperand(BytecodeOpcode opcode, uint32_t operand);
    void EmitJump(BytecodeOpcode opcode, uint32_t labelId);
    void EmitLabel(uint32_t labelId);
    
    // コンテキスト
    BytecodeGeneratorContext m_context;
    
    // 生成中のモジュール
    std::unique_ptr<BytecodeModule> m_module;
    
    // 現在処理中の関数
    BytecodeFunction* m_currentFunction = nullptr;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H 