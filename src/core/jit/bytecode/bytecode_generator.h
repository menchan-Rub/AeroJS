#ifndef AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H
#define AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <stack>
#include <functional>
#include <optional>
#include <variant>
#include <bitset>

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

// 最適化レベル
enum class OptimizationLevel {
    None,       // 最適化なし
    Basic,      // 基本的な最適化
    Aggressive  // 積極的な最適化
};

// ジェネレーターの実行フラグ
struct BytecodeGeneratorFlags {
    bool strictMode = false;        // strict modeを使用
    bool optimizeConstFolding = true;   // 定数畳み込み最適化
    bool optimizeDeadCode = true;      // デッドコード除去
    bool optimizeJumps = true;         // ジャンプ最適化
    bool optimizeLocalAccess = true;   // ローカル変数アクセス最適化
    bool generateSourceMap = true;     // ソースマップを生成
    bool inlineSmallFunctions = false; // 小さな関数のインライン化
    bool verboseLogging = false;       // 詳細なログ出力
    
    // 最適化レベルから設定を生成
    static BytecodeGeneratorFlags fromOptimizationLevel(OptimizationLevel level) {
        BytecodeGeneratorFlags flags;
        
        switch (level) {
            case OptimizationLevel::None:
                flags.optimizeConstFolding = false;
                flags.optimizeDeadCode = false;
                flags.optimizeJumps = false;
                flags.optimizeLocalAccess = false;
                flags.inlineSmallFunctions = false;
                break;
                
            case OptimizationLevel::Basic:
                // デフォルト設定を使用
                break;
                
            case OptimizationLevel::Aggressive:
                flags.inlineSmallFunctions = true;
                break;
        }
        
        return flags;
    }
};

// 生成されたコードのメタデータ
struct CodegenMetadata {
    uint32_t stackDepth = 0;       // 最大スタック深度
    uint32_t localVarCount = 0;    // ローカル変数の数
    uint32_t instructionCount = 0; // 命令数
    uint32_t optimizedInstructions = 0; // 最適化された命令数
    std::vector<std::pair<uint32_t, uint32_t>> sourceMapEntries; // ソースマップエントリ (コードオフセット, ソース位置)
};

// スコープ内の変数情報
struct VariableInfo {
    uint32_t index;              // 変数インデックス
    bool isParameter = false;    // パラメータかどうか
    bool isConst = false;        // constで宣言されたか
    bool isLocal = true;         // ローカル変数（falseならクロージャ変数）
    uint32_t firstUsage = 0;     // 最初に使用される命令インデックス
    uint32_t lastUsage = 0;      // 最後に使用される命令インデックス
    uint32_t usageCount = 0;     // 使用回数
};

// バイトコードジェネレーターのコンテキストクラス
class BytecodeGeneratorContext {
public:
    BytecodeGeneratorContext(const BytecodeGeneratorFlags& flags) 
        : m_flags(flags), m_nextLabel(0) {
        // 現在のスコープに初期化
        m_variableScopes.emplace_back();
    }
    
    ~BytecodeGeneratorContext() = default;

    // フラグの取得
    const BytecodeGeneratorFlags& getFlags() const { return m_flags; }
    
    // 現在のスコープに変数を追加し、そのインデックスを返す
    uint32_t AddVariable(const std::string& name, bool isParameter = false, bool isConst = false) {
        // スコープが空の場合は作成
        if (m_variableScopes.empty()) {
            m_variableScopes.emplace_back();
        }
        
        auto& scope = m_variableScopes.back();
        uint32_t index = static_cast<uint32_t>(scope.size());
        
        VariableInfo info;
        info.index = index;
        info.isParameter = isParameter;
        info.isConst = isConst;
        
        scope[name] = info;
        return index;
    }
    
    // 変数名からインデックスと情報を取得
    std::optional<VariableInfo> GetVariableInfo(const std::string& name) const {
        // すべてのスコープを内側から順に検索
        for (auto it = m_variableScopes.rbegin(); it != m_variableScopes.rend(); ++it) {
            auto var = it->find(name);
            if (var != it->end()) {
                return var->second;
            }
        }
        
        return std::nullopt;
    }
    
    // 変数の使用を記録
    void RecordVariableUsage(const std::string& name, uint32_t instructionIndex) {
        for (auto& scope : m_variableScopes) {
            auto it = scope.find(name);
            if (it != scope.end()) {
                // 変数情報を更新
                VariableInfo& info = it->second;
                if (info.usageCount == 0 || instructionIndex < info.firstUsage) {
                    info.firstUsage = instructionIndex;
                }
                if (instructionIndex > info.lastUsage) {
                    info.lastUsage = instructionIndex;
                }
                info.usageCount++;
                return;
            }
        }
    }
    
    // 定数をプールに追加し、そのインデックスを返す
    uint32_t AddConstant(const ConstantValue& value) {
        // 重複をチェック（最適化のため）
        for (uint32_t i = 0; i < m_constantPool.size(); ++i) {
            if (ConstantsEqual(m_constantPool[i], value)) {
                return i;
            }
        }
        
        uint32_t index = static_cast<uint32_t>(m_constantPool.size());
        m_constantPool.push_back(value);
        return index;
    }
    
    // 文字列をテーブルに追加し、そのインデックスを返す
    uint32_t AddString(const std::string& str) {
        // 既存の文字列を検索（文字列インターニング）
        auto it = m_stringIndexMap.find(str);
        if (it != m_stringIndexMap.end()) {
            return it->second;
        }
        
        uint32_t index = static_cast<uint32_t>(m_stringTable.size());
        m_stringTable.push_back(str);
        m_stringIndexMap[str] = index;
        return index;
    }
    
    // 新しいスコープに入る
    void EnterScope() {
        m_variableScopes.emplace_back();
    }
    
    // 現在のスコープを出る
    void ExitScope() {
        if (!m_variableScopes.empty()) {
            m_variableScopes.pop_back();
        }
    }
    
    // ループのコンテキストに入る
    void EnterLoop(uint32_t startLabel, uint32_t endLabel) {
        m_loopStack.emplace_back(startLabel, endLabel);
    }
    
    // ループのコンテキストを出る
    void ExitLoop() {
        if (!m_loopStack.empty()) {
            m_loopStack.pop_back();
        }
    }
    
    // breakステートメントのジャンプ先を取得
    uint32_t GetBreakTarget() const {
        if (m_loopStack.empty()) {
            return 0; // エラー状態
        }
        return m_loopStack.back().second; // 終了ラベル
    }
    
    // continueステートメントのジャンプ先を取得
    uint32_t GetContinueTarget() const {
        if (m_loopStack.empty()) {
            return 0; // エラー状態
        }
        return m_loopStack.back().first; // 開始ラベル
    }
    
    // 新しいラベルを生成
    uint32_t CreateLabel() {
        return m_nextLabel++;
    }
    
    // ラベルの位置を設定
    void SetLabelPosition(uint32_t label, uint32_t position) {
        m_labelPositions[label] = position;
        
        // このラベルを参照する保留中のジャンプを解決
        auto range = m_pendingJumps.equal_range(label);
        for (auto it = range.first; it != range.second; ++it) {
            uint32_t jumpIndex = it->second;
            ResolveJumpTarget(jumpIndex, position);
        }
        
        // 解決したジャンプを削除
        m_pendingJumps.erase(label);
    }
    
    // ラベルの位置を取得
    uint32_t GetLabelPosition(uint32_t label) const {
        auto it = m_labelPositions.find(label);
        if (it != m_labelPositions.end()) {
            return it->second;
        }
        return 0; // エラー状態
    }
    
    // 未解決のジャンプを登録
    void AddPendingJump(uint32_t labelId, uint32_t jumpInstructionIndex) {
        // ラベルがすでに解決済みかチェック
        auto it = m_labelPositions.find(labelId);
        if (it != m_labelPositions.end()) {
            // ラベルが既に定義されている場合は直接解決
            ResolveJumpTarget(jumpInstructionIndex, it->second);
        } else {
            // 未解決の場合はペンディングリストに追加
            m_pendingJumps.emplace(labelId, jumpInstructionIndex);
        }
    }
    
    // ジャンプターゲットの解決を行う関数
    void ResolveJumpTarget(uint32_t jumpIndex, uint32_t targetPosition) {
        // 実装はバイトコードジェネレーターに委任
        if (m_resolveJumpCallback) {
            m_resolveJumpCallback(jumpIndex, targetPosition);
        }
    }
    
    // ジャンプ解決コールバックを設定
    void SetResolveJumpCallback(std::function<void(uint32_t, uint32_t)> callback) {
        m_resolveJumpCallback = std::move(callback);
    }
    
    // 例外ハンドラを追加
    uint32_t AddExceptionHandler(const ExceptionHandler& handler) {
        uint32_t index = static_cast<uint32_t>(m_exceptionHandlers.size());
        m_exceptionHandlers.push_back(handler);
        return index;
    }
    
    // 定数プールの取得
    const std::vector<ConstantValue>& GetConstantPool() const {
        return m_constantPool;
    }
    
    // 文字列テーブルの取得
    const std::vector<std::string>& GetStringTable() const {
        return m_stringTable;
    }
    
    // 例外ハンドラリストの取得
    const std::vector<ExceptionHandler>& GetExceptionHandlers() const {
        return m_exceptionHandlers;
    }
    
    // 定数値の比較（最適化用）
    static bool ConstantsEqual(const ConstantValue& a, const ConstantValue& b) {
        if (a.type != b.type) {
            return false;
        }
        
        switch (a.type) {
            case ConstantType::Undefined:
            case ConstantType::Null:
                return true; // 同じ型なら常に等しい
                
            case ConstantType::Boolean:
                return a.booleanValue == b.booleanValue;
                
            case ConstantType::Number:
                return a.numberValue == b.numberValue;
                
            case ConstantType::String:
                return a.stringIndex == b.stringIndex;
                
            case ConstantType::Object:
                return a.objectIndex == b.objectIndex;
                
            case ConstantType::Array:
                return a.arrayIndex == b.arrayIndex;
                
            case ConstantType::Function:
                return a.functionIndex == b.functionIndex;
                
            case ConstantType::RegExp:
                return a.regexpIndex == b.regexpIndex;
                
            default:
                return false;
        }
    }

private:
    // 変数スコープスタック
    std::vector<std::unordered_map<std::string, VariableInfo>> m_variableScopes;
    
    // 定数プールとインデックス
    std::vector<ConstantValue> m_constantPool;
    
    // 文字列テーブルとインデックスマップ（インターニング用）
    std::vector<std::string> m_stringTable;
    std::unordered_map<std::string, uint32_t> m_stringIndexMap;
    
    // ループスタック（開始ラベルと終了ラベルのペア）
    std::vector<std::pair<uint32_t, uint32_t>> m_loopStack;
    
    // ラベルのマップ（ラベル番号 -> バイトコード位置）
    std::unordered_map<uint32_t, uint32_t> m_labelPositions;
    
    // 未解決のジャンプ（ラベル -> 命令インデックス）
    std::unordered_multimap<uint32_t, uint32_t> m_pendingJumps;
    
    // 次に割り当てるラベル番号
    uint32_t m_nextLabel;
    
    // 例外ハンドラリスト
    std::vector<ExceptionHandler> m_exceptionHandlers;
    
    // ジャンプ解決用コールバック
    std::function<void(uint32_t, uint32_t)> m_resolveJumpCallback;
    
    // ジェネレーターフラグ
    BytecodeGeneratorFlags m_flags;
};

// 命令列最適化器
class BytecodeOptimizer {
public:
    BytecodeOptimizer(BytecodeFunction* function, const BytecodeGeneratorFlags& flags) 
        : m_function(function), m_flags(flags) {}
    
    // 最適化を実行
    void Optimize() {
        if (!m_function) return;
        
        // 定数畳み込み最適化
        if (m_flags.optimizeConstFolding) {
            PerformConstantFolding();
        }
        
        // デッドコード除去
        if (m_flags.optimizeDeadCode) {
            RemoveDeadCode();
        }
        
        // ジャンプ最適化
        if (m_flags.optimizeJumps) {
            OptimizeJumps();
        }
    }
    
private:
    // 定数畳み込み最適化
    void PerformConstantFolding() {
        auto& instructions = m_function->GetInstructions();
        
        // 定数の追跡
        std::unordered_map<uint32_t, int32_t> constantRegs;
        
        // 最適化後の命令リスト
        std::vector<BytecodeInstruction> optimizedInstructions;
        
        for (const auto& inst : instructions) {
            if (inst.opcode == BytecodeOpcode::LoadConst) {
                // 定数ロード命令を追跡
                uint32_t destReg = inst.operand >> 8;
                int32_t constValue = inst.operand & 0xFF;
                constantRegs[destReg] = constValue;
                optimizedInstructions.push_back(inst);
            } 
            else if ((inst.opcode == BytecodeOpcode::Add || 
                      inst.opcode == BytecodeOpcode::Sub ||
                      inst.opcode == BytecodeOpcode::Mul ||
                      inst.opcode == BytecodeOpcode::Div) &&
                     constantRegs.find(inst.operand >> 16) != constantRegs.end() &&
                     constantRegs.find((inst.operand >> 8) & 0xFF) != constantRegs.end()) {
                
                // 両方のオペランドが定数の場合、計算を事前に行う
                uint32_t destReg = inst.operand & 0xFF;
                int32_t lhs = constantRegs[inst.operand >> 16];
                int32_t rhs = constantRegs[(inst.operand >> 8) & 0xFF];
                int32_t result = 0;
                
                // 演算を実行
                switch (inst.opcode) {
                    case BytecodeOpcode::Add: result = lhs + rhs; break;
                    case BytecodeOpcode::Sub: result = lhs - rhs; break;
                    case BytecodeOpcode::Mul: result = lhs * rhs; break;
                    case BytecodeOpcode::Div:
                        if (rhs != 0) result = lhs / rhs;
                        else {
                            // ゼロ除算は最適化しない
                            optimizedInstructions.push_back(inst);
                            continue;
                        }
                        break;
                    default: break;
                }
                
                // 最適化された定数ロード命令を生成
                BytecodeInstruction newInst(BytecodeOpcode::LoadConst, 
                                           (destReg << 8) | (result & 0xFF));
                optimizedInstructions.push_back(newInst);
                
                // 結果を定数マップに記録
                constantRegs[destReg] = result;
            }
            else {
                // その他の命令はそのままコピー
                optimizedInstructions.push_back(inst);
            }
        }
        
        // 最適化された命令で元の命令を置き換え
        m_function->SetInstructions(optimizedInstructions);
    }
    
    // デッドコード除去
    void RemoveDeadCode() {
        auto& instructions = m_function->GetInstructions();
        if (instructions.empty()) return;
        
        // コード到達可能性分析
        std::vector<bool> reachable(instructions.size(), false);
        std::vector<uint32_t> workList;
        
        // エントリポイントは常に到達可能
        reachable[0] = true;
        workList.push_back(0);
        
        // 到達可能性分析を実行
        while (!workList.empty()) {
            uint32_t currentIndex = workList.back();
            workList.pop_back();
            
            if (currentIndex >= instructions.size()) continue;
            
            const auto& inst = instructions[currentIndex];
            
            // 分岐命令の場合、両方のパスを追跡
            if (inst.opcode == BytecodeOpcode::JumpIfTrue ||
                inst.opcode == BytecodeOpcode::JumpIfFalse) {
                uint32_t targetIndex = inst.operand;
                
                // 分岐先が有効かつまだ訪問していない場合
                if (targetIndex < instructions.size() && !reachable[targetIndex]) {
                    reachable[targetIndex] = true;
                    workList.push_back(targetIndex);
                }
                
                // フォールスルーパスも到達可能
                if (currentIndex + 1 < instructions.size() && !reachable[currentIndex + 1]) {
                    reachable[currentIndex + 1] = true;
                    workList.push_back(currentIndex + 1);
                }
            }
            // 無条件ジャンプ
            else if (inst.opcode == BytecodeOpcode::Jump) {
                uint32_t targetIndex = inst.operand;
                
                // ジャンプ先が有効かつまだ訪問していない場合
                if (targetIndex < instructions.size() && !reachable[targetIndex]) {
                    reachable[targetIndex] = true;
                    workList.push_back(targetIndex);
                }
            }
            // リターン命令やThrow命令の場合、後続のコードは実行されない
            else if (inst.opcode != BytecodeOpcode::Return && 
                     inst.opcode != BytecodeOpcode::Throw) {
                // 通常の命令の場合、次の命令に進む
                if (currentIndex + 1 < instructions.size() && !reachable[currentIndex + 1]) {
                    reachable[currentIndex + 1] = true;
                    workList.push_back(currentIndex + 1);
                }
            }
        }
        
        // 到達不可能なコードを除去
        std::vector<BytecodeInstruction> optimizedInstructions;
        for (size_t i = 0; i < instructions.size(); i++) {
            if (reachable[i]) {
                optimizedInstructions.push_back(instructions[i]);
            }
        }
        
        // 最適化された命令で元の命令を置き換え
        m_function->SetInstructions(optimizedInstructions);
    }
    
    // ジャンプチェーンとジャンプ距離の最適化
    void OptimizeJumps() {
        auto& instructions = m_function->GetInstructions();
        if (instructions.empty()) return;
        
        // ジャンプチェーンの最適化
        std::unordered_map<uint32_t, uint32_t> jumpTargets;
        
        // 各ジャンプ命令の最終目的地を特定
        for (size_t i = 0; i < instructions.size(); i++) {
            auto& inst = instructions[i];
            
            if (inst.opcode == BytecodeOpcode::Jump) {
                uint32_t target = inst.operand;
                uint32_t finalTarget = target;
                
                // ジャンプチェーンをたどる
                while (finalTarget < instructions.size() && 
                       instructions[finalTarget].opcode == BytecodeOpcode::Jump) {
                    finalTarget = instructions[finalTarget].operand;
                    
                    // 循環参照を検出して無限ループを防止
                    if (finalTarget == target) break;
                }
                
                // 最終目的地を記録
                jumpTargets[i] = finalTarget;
            }
        }
        
        // 最適化されたターゲットでジャンプ命令を更新
        for (size_t i = 0; i < instructions.size(); i++) {
            auto& inst = instructions[i];
            
            if (inst.opcode == BytecodeOpcode::Jump) {
                auto it = jumpTargets.find(i);
                if (it != jumpTargets.end()) {
                    inst.operand = it->second;
                }
            }
            else if (inst.opcode == BytecodeOpcode::JumpIfTrue ||
                     inst.opcode == BytecodeOpcode::JumpIfFalse) {
                uint32_t target = inst.operand;
                
                // 条件付きジャンプの目的地がJumpなら、そのターゲットに直接ジャンプ
                if (target < instructions.size() && 
                    instructions[target].opcode == BytecodeOpcode::Jump) {
                    inst.operand = instructions[target].operand;
                }
            }
        }
        
        // 未使用のJump命令を除去（NOP命令に置き換え）
        for (size_t i = 0; i < instructions.size() - 1; i++) {
            auto& current = instructions[i];
            auto& next = instructions[i + 1];
            
            // Jumpの直後に別のJumpがあるか、または到達不可能なコードがある場合
            if (current.opcode == BytecodeOpcode::Jump) {
                // 後続の命令が単独のNopや副作用のない命令の場合、それらを除去可能
                size_t j = i + 1;
                while (j < instructions.size() && 
                       (instructions[j].opcode == BytecodeOpcode::Nop ||
                        instructions[j].opcode == BytecodeOpcode::LoadConst)) {
                    // Nopや単純な定数ロードをスキップ
                    instructions[j] = BytecodeInstruction(BytecodeOpcode::Nop, 0);
                    j++;
                }
            }
        }
    }
    
    BytecodeFunction* m_function;
    BytecodeGeneratorFlags m_flags;
};

// バイトコードジェネレータークラス
class BytecodeGenerator {
public:
    BytecodeGenerator(const BytecodeGeneratorFlags& flags = BytecodeGeneratorFlags())
        : m_context(flags), m_metadata(), m_currentInstructionIndex(0) {
        // ジャンプ解決コールバックを設定
        m_context.SetResolveJumpCallback([this](uint32_t jumpIndex, uint32_t targetPosition) {
            this->ResolveJumpAtIndex(jumpIndex, targetPosition);
        });
    }
    
    ~BytecodeGenerator() = default;

    // プログラム全体からバイトコードモジュールを生成
    std::unique_ptr<BytecodeModule> Generate(const Program& program, 
                                           OptimizationLevel level = OptimizationLevel::Basic) {
        // フラグの設定
        m_flags = BytecodeGeneratorFlags::fromOptimizationLevel(level);
        
        // モジュールを作成
        m_module = std::make_unique<BytecodeModule>("main");
        m_currentFunction = nullptr;
        m_currentInstructionIndex = 0;
        
        // プログラムを処理
        GenerateProgram(program);
        
        // 最適化を実行
        if (level != OptimizationLevel::None) {
            OptimizeModule();
        }
        
        return std::move(m_module);
    }
    
    // 単一の式からバイトコードを生成（評価用）
    std::unique_ptr<BytecodeFunction> GenerateForEval(const Expression& expression,
                                                     OptimizationLevel level = OptimizationLevel::Basic) {
        // フラグの設定
        m_flags = BytecodeGeneratorFlags::fromOptimizationLevel(level);
        
        // 評価用関数を作成
        auto function = std::make_unique<BytecodeFunction>("eval", 0);
        m_currentFunction = function.get();
        m_currentInstructionIndex = 0;
        
        // 式を処理
        GenerateExpression(expression);
        
        // Return命令を追加
        EmitOpcode(BytecodeOpcode::Return);
        
        // 関数のメタデータを設定
        function->SetLocalCount(m_metadata.localVarCount);
        
        // 最適化を実行
        if (level != OptimizationLevel::None) {
            BytecodeOptimizer optimizer(function.get(), m_flags);
            optimizer.Optimize();
        }
        
        return function;
    }
    
    // 現在のメタデータを取得
    const CodegenMetadata& GetMetadata() const {
        return m_metadata;
    }

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
    void GenerateExpressionStatement(const ExpressionStatement& exprStmt);
    void GenerateVariableDeclaration(const VariableDeclaration& varDecl);
    
    // 式の評価結果をスタックに残すコード生成
    void GenerateExpression(const Expression& expr);
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
    void EmitOpcode(BytecodeOpcode opcode) {
        if (!m_currentFunction) return;
        
        BytecodeInstruction instruction(opcode);
        m_currentFunction->AddInstruction(instruction);
        m_currentInstructionIndex++;
        m_metadata.instructionCount++;
        
        // スタック深度の更新
        UpdateStackDepth(opcode, 0);
    }
    
    void EmitOpcodeWithOperand(BytecodeOpcode opcode, uint32_t operand) {
        if (!m_currentFunction) return;
        
        BytecodeInstruction instruction(opcode, operand);
        m_currentFunction->AddInstruction(instruction);
        m_currentInstructionIndex++;
        m_metadata.instructionCount++;
        
        // スタック深度の更新
        UpdateStackDepth(opcode, 1);
    }
    
    void EmitJump(BytecodeOpcode opcode, uint32_t labelId) {
        if (!m_currentFunction) return;
        
        // ジャンプ命令をエミット
        EmitOpcodeWithOperand(opcode, 0); // ターゲットは後で解決
        
        // 保留中のジャンプとして記録
        uint32_t jumpInstructionIndex = m_currentInstructionIndex - 1;
        m_context.AddPendingJump(labelId, jumpInstructionIndex);
    }
    
    void EmitLabel(uint32_t labelId) {
        if (!m_currentFunction) return;
        
        // ラベル位置を現在の命令インデックスに設定
        m_context.SetLabelPosition(labelId, m_currentInstructionIndex);
    }
    
    // スタック深度の更新
    void UpdateStackDepth(BytecodeOpcode opcode, uint32_t operandCount);
    
    // 未解決のジャンプのターゲット位置を設定
    void ResolveJumpAtIndex(uint32_t jumpIndex, uint32_t targetPosition) {
        if (!m_currentFunction || jumpIndex >= m_currentFunction->GetInstructionCount()) {
            return;
        }
        
        // ジャンプ命令のオペランドを更新
        auto& instructions = m_currentFunction->GetInstructions();
        instructions[jumpIndex].operand = targetPosition;
    }
    
    // モジュール全体の最適化
    void OptimizeModule() {
        if (!m_module) return;
        
        for (uint32_t i = 0; i < m_module->GetFunctionCount(); ++i) {
            BytecodeFunction* function = m_module->GetFunction(i);
            if (function) {
                BytecodeOptimizer optimizer(function, m_flags);
                optimizer.Optimize();
            }
        }
    }
    
    // コンテキスト
    BytecodeGeneratorContext m_context;
    
    // 生成設定フラグ
    BytecodeGeneratorFlags m_flags;
    
    // 生成メタデータ
    CodegenMetadata m_metadata;
    
    // 生成中のモジュール
    std::unique_ptr<BytecodeModule> m_module;
    
    // 現在処理中の関数
    BytecodeFunction* m_currentFunction;
    
    // 現在の命令インデックス
    uint32_t m_currentInstructionIndex;
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_JIT_BYTECODE_BYTECODE_GENERATOR_H 