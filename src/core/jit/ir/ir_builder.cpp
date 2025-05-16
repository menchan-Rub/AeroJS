/**
 * @file ir_builder.cpp
 * @brief AeroJS JavaScript エンジンの中間表現(IR)ビルダーの実装
 * @version 1.0.0
 * @license MIT
 */

#include "ir_builder.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <iomanip>

namespace aerojs {
namespace core {

// コンストラクタ
IRBuilder::IRBuilder(Context* context, JITProfiler* profiler)
    : m_context(context)
    , m_profiler(profiler)
    , m_nextValueId(0)
    , m_nextBlockId(0)
    , m_currentValue(nullptr)
    , m_currentBlock(nullptr)
    , m_currentFunction(nullptr)
    , m_bytecodeIndex(0)
    , m_bytecodeLength(0)
{
}

// デストラクタ
IRBuilder::~IRBuilder()
{
    // メモリクリーンアップは呼び出し元で行う想定
}

// 関数からIRを構築
IRFunction* IRBuilder::build(Function* function)
{
    // 初期化
    m_nextValueId = 0;
    m_nextBlockId = 0;
    m_currentValue = nullptr;
    m_currentBlock = nullptr;
    m_bytecodeIndex = 0;
    
    // 新しいIR関数を作成
    m_currentFunction = new IRFunction();
    m_currentFunction->name = "function_" + std::to_string(function->getFunctionId());
    m_currentFunction->functionId = function->getFunctionId();
    
    // バイトコードスキャンとブロック設定
    scan(function);
    buildBlocks();
    
    // 関数を処理
    processFunction(function);
    
    return m_currentFunction;
}

// IR関数のダンプ
std::string IRBuilder::dumpIR(IRFunction* irFunction) const
{
    if (!irFunction) {
        return "No IR function to dump";
    }
    
    std::stringstream ss;
    ss << "=== IR Function: " << irFunction->name << " (ID: " << irFunction->functionId << ") ===" << std::endl;
    
    // 基本ブロック一覧
    ss << "Blocks (" << irFunction->blocks.size() << "):" << std::endl;
    for (const auto* block : irFunction->blocks) {
        ss << "  Block " << block->id;
        if (block == irFunction->entryBlock) {
            ss << " [Entry]";
        }
        if (block == irFunction->exitBlock) {
            ss << " [Exit]";
        }
        if (block->isLoopHeader) {
            ss << " [LoopHeader]";
        }
        if (block->isHandler) {
            ss << " [ExceptionHandler]";
        }
        ss << std::endl;
        
        // PHI値
        if (!block->phiValues.empty()) {
            ss << "    PHI Values:" << std::endl;
            for (const auto* value : block->phiValues) {
                ss << "      v" << value->id << " : " << static_cast<int>(value->type) << std::endl;
            }
        }
        
        // 先行ブロック
        if (!block->predecessors.empty()) {
            ss << "    Predecessors: ";
            for (const auto* pred : block->predecessors) {
                ss << pred->id << " ";
            }
            ss << std::endl;
        }
        
        // 後続ブロック
        if (!block->successors.empty()) {
            ss << "    Successors: ";
            for (const auto* succ : block->successors) {
                ss << succ->id << " ";
            }
            ss << std::endl;
        }
        
        // 命令
        for (const auto* inst : block->instructions) {
            ss << "    ";
            if (inst->result) {
                ss << "v" << inst->result->id << " = ";
            } else {
                ss << "      ";
            }
            
            ss << static_cast<int>(inst->opcode) << " ";
            
            for (size_t i = 0; i < inst->operands.size(); ++i) {
                if (i > 0) {
                    ss << ", ";
                }
                if (inst->operands[i]) {
                    ss << "v" << inst->operands[i]->id;
                } else {
                    ss << "null";
                }
            }
            
            if (!inst->debugInfo.empty()) {
                ss << " // " << inst->debugInfo;
            }
            
            ss << std::endl;
        }
        
        ss << std::endl;
    }
    
    // 引数リスト
    if (!irFunction->arguments.empty()) {
        ss << "Arguments:" << std::endl;
        for (const auto& arg : irFunction->arguments) {
            ss << "  Arg " << arg.first << ": v" << arg.second->id 
               << " (type: " << static_cast<int>(arg.second->type) << ")" << std::endl;
        }
        ss << std::endl;
    }
    
    // ローカル変数リスト
    if (!irFunction->locals.empty()) {
        ss << "Locals:" << std::endl;
        for (const auto& local : irFunction->locals) {
            ss << "  Local " << local.first << ": v" << local.second->id 
               << " (type: " << static_cast<int>(local.second->type) << ")" << std::endl;
        }
        ss << std::endl;
    }
    
    return ss.str();
}

// バイトコードのスキャン
void IRBuilder::scan(Function* function)
{
    // 関数からバイトコードを取得
    m_bytecode = function->getBytecode();
    m_bytecodeLength = static_cast<uint32_t>(m_bytecode.size());
    m_bytecodeIndex = 0;
    
    // バイトコードをスキャンして基本ブロック境界を識別
    m_blockStarts.clear();
    
    // 常にエントリブロックを追加
    m_blockStarts.insert(0);
    
    while (m_bytecodeIndex < m_bytecodeLength) {
        uint32_t currentOffset = m_bytecodeIndex;
        uint8_t opcode = readByte();
        
        // オペコードに基づいて処理
        switch (opcode) {
            // ジャンプ命令
            case 0x10: // jump
            {
                int32_t offset = readSignedDWord();
                uint32_t targetOffset = static_cast<uint32_t>(static_cast<int32_t>(currentOffset) + offset);
                markBlockStart(targetOffset);
                markBlockStart(m_bytecodeIndex); // ジャンプ先の次の命令も新しいブロックの開始
                break;
            }
            
            // 条件付きジャンプ命令
            case 0x11: // jump_if_true
            case 0x12: // jump_if_false
            {
                readByte(); // condition register
                int32_t offset = readSignedDWord();
                uint32_t targetOffset = static_cast<uint32_t>(static_cast<int32_t>(currentOffset) + offset);
                markBlockStart(targetOffset);
                markBlockStart(m_bytecodeIndex); // フォールスルーも新しいブロックの開始
                break;
            }
            
            // 関数呼び出し
            case 0x20: // call
            {
                readByte(); // dest register
                readByte(); // function register
                uint8_t argCount = readByte();
                for (uint8_t i = 0; i < argCount; ++i) {
                    readByte(); // arg register
                }
                break;
            }
            
            // 戻り値
            case 0x30: // return
            case 0x31: // return_value
            {
                if (opcode == 0x31) {
                    readByte(); // return value register
                }
                // リターン後は新しいブロックの開始
                if (m_bytecodeIndex < m_bytecodeLength) {
                    markBlockStart(m_bytecodeIndex);
                }
                break;
            }
            
            // 例外スロー
            case 0x32: // throw
            {
                readByte(); // exception register
                // スロー後は新しいブロックの開始
                if (m_bytecodeIndex < m_bytecodeLength) {
                    markBlockStart(m_bytecodeIndex);
                }
                break;
            }
            
            // その他の命令
            default:
                // オペコードに応じて引数をスキップ
                // 以下は簡易実装のため、実際には全オペコードに対応する必要がある
                // ロード/ストア命令
                if (opcode >= 0x40 && opcode < 0x50) {
                    readByte(); // dest register
                    if (opcode != 0x40) { // nop以外
                        readByte(); // src register or immediate value
                    }
                }
                // 算術演算命令
                else if (opcode >= 0x50 && opcode < 0x60) {
                    readByte(); // dest register
                    readByte(); // src1 register
                    readByte(); // src2 register
                }
                // 比較演算命令
                else if (opcode >= 0x60 && opcode < 0x70) {
                    readByte(); // dest register
                    readByte(); // src1 register
                    readByte(); // src2 register
                }
                break;
        }
    }
    
    // 走査位置をリセット
    m_bytecodeIndex = 0;
}

// ブロックの構築
void IRBuilder::buildBlocks()
{
    // エントリブロックを作成
    m_currentFunction->entryBlock = createBlock();
    m_currentFunction->addBlock(m_currentFunction->entryBlock);
    
    // 出口ブロックを作成
    m_currentFunction->exitBlock = createBlock();
    m_currentFunction->addBlock(m_currentFunction->exitBlock);
    
    // 他のブロックを作成
    for (uint32_t offset : m_blockStarts) {
        if (offset == 0) {
            // エントリブロックはすでに作成済み
            m_blockMap[0] = m_currentFunction->entryBlock;
        } else {
            IRBlock* block = createBlock();
            m_blockMap[offset] = block;
            m_currentFunction->addBlock(block);
        }
    }
    
    // 現在のブロックをエントリブロックに設定
    m_currentBlock = m_currentFunction->entryBlock;
}

// 関数を処理
void IRBuilder::processFunction(Function* function)
{
    // 引数の設定
    uint32_t paramCount = function->getParameterCount();
    for (uint32_t i = 0; i < paramCount; ++i) {
        IRValue* argValue = createValue(IRType::Any);
        m_currentFunction->addArgument(i, argValue);
    }
    
    // バイトコード処理を実行
    m_bytecodeIndex = 0;
    while (m_bytecodeIndex < m_bytecodeLength) {
        uint32_t currentOffset = m_bytecodeIndex;
        
        // このオフセットで新しいブロックが始まる場合、現在のブロックを更新
        auto blockIt = m_blockMap.find(currentOffset);
        if (blockIt != m_blockMap.end()) {
            m_currentBlock = blockIt->second;
        }
        
        // バイトコード命令を処理
        processNextInstruction();
    }
    
    // 制御フローグラフの構築
    buildControlFlowGraph();
    
    // ループ解析を実行
    detectLoops();
    
    // 例外ハンドラブロックを識別
    identifyExceptionHandlers(function);
    
    // 到達不能なブロックを削除
    removeUnreachableBlocks();
    
    // PHIノードの挿入
    insertPhiNodes();
}

// 制御フローグラフの構築
void IRBuilder::buildControlFlowGraph()
{
    for (IRBlock* block : m_currentFunction->blocks) {
        if (block->instructions.empty()) {
            continue;
        }
        
        // ブロックの最後の命令を取得
        IRInstruction* lastInst = block->instructions.back();
        
        switch (lastInst->opcode) {
            case IROpcode::Jump:
            {
                // ジャンプ先ブロックを見つけて接続
                uint32_t targetId = 0;
                if (!lastInst->debugInfo.empty()) {
                    // デバッグ情報からターゲットIDを抽出
                    size_t pos = lastInst->debugInfo.find("target=");
                    if (pos != std::string::npos) {
                        targetId = std::stoul(lastInst->debugInfo.substr(pos + 7));
                    }
                }
                
                // ターゲットブロックを探す
                for (IRBlock* targetBlock : m_currentFunction->blocks) {
                    if (targetBlock->id == targetId) {
                        block->addSuccessor(targetBlock);
                        break;
                    }
                }
                break;
            }
            case IROpcode::Branch:
            {
                // 条件分岐の場合、true/falseブロックを見つけて接続
                uint32_t trueBlockId = 0;
                uint32_t falseBlockId = 0;
                
                if (!lastInst->debugInfo.empty()) {
                    // デバッグ情報からtrue/falseブロックIDを抽出
                    size_t truePos = lastInst->debugInfo.find("true=");
                    size_t falsePos = lastInst->debugInfo.find("false=");
                    
                    if (truePos != std::string::npos && falsePos != std::string::npos) {
                        trueBlockId = std::stoul(lastInst->debugInfo.substr(truePos + 5, falsePos - truePos - 7));
                        falseBlockId = std::stoul(lastInst->debugInfo.substr(falsePos + 6));
                    }
                }
                
                // trueブロックとfalseブロックを探して接続
                for (IRBlock* targetBlock : m_currentFunction->blocks) {
                    if (targetBlock->id == trueBlockId || targetBlock->id == falseBlockId) {
                        block->addSuccessor(targetBlock);
                    }
                }
                break;
            }
            case IROpcode::Return:
            case IROpcode::Throw:
            {
                // リターンまたはスローの場合、終了ブロックに接続
                block->addSuccessor(m_currentFunction->exitBlock);
                break;
            }
            default:
            {
                // その他の命令の場合、次のブロックに接続（フォールスルー）
                bool foundNext = false;
                
                // 次のブロックを探す（現在のブロックのIDより大きい最小のID）
                IRBlock* nextBlock = nullptr;
                uint32_t minId = std::numeric_limits<uint32_t>::max();
                
                for (IRBlock* candidate : m_currentFunction->blocks) {
                    if (candidate->id > block->id && candidate->id < minId) {
                        nextBlock = candidate;
                        minId = candidate->id;
                        foundNext = true;
                    }
                }
                
                if (foundNext && nextBlock) {
                    block->addSuccessor(nextBlock);
                }
                break;
            }
        }
    }
}

// ループ検出
void IRBuilder::detectLoops()
{
    // 深さ優先探索でループを検出
    std::vector<bool> visited(m_nextBlockId, false);
    std::vector<bool> inStack(m_nextBlockId, false);
    
    // エントリブロックから探索開始
    detectLoopsRecursive(m_currentFunction->entryBlock, visited, inStack);
}

void IRBuilder::detectLoopsRecursive(IRBlock* block, std::vector<bool>& visited, std::vector<bool>& inStack)
{
    if (!block) {
        return;
    }
    
    uint32_t blockId = block->id;
    visited[blockId] = true;
    inStack[blockId] = true;
    
    // 後続ブロックを探索
    for (IRBlock* succ : block->successors) {
        if (!visited[succ->id]) {
            detectLoopsRecursive(succ, visited, inStack);
        } else if (inStack[succ->id]) {
            // バックエッジを検出 - ループヘッダーとしてマーク
            succ->isLoopHeader = true;
        }
    }
    
    inStack[blockId] = false;
}

// 例外ハンドラ識別
void IRBuilder::identifyExceptionHandlers(Function* function)
{
    // 関数から例外ハンドラ情報を取得
    const auto& exceptionTable = function->getExceptionTable();
    
    for (const auto& entry : exceptionTable) {
        uint32_t handlerOffset = entry.handlerOffset;
        
        // 対応するブロックを探す
        auto blockIt = m_blockMap.find(handlerOffset);
        if (blockIt != m_blockMap.end()) {
            IRBlock* handlerBlock = blockIt->second;
            handlerBlock->isHandler = true;
            
            // スローブロックとハンドラブロックの間にエッジを追加
            uint32_t tryStartOffset = entry.tryStartOffset;
            uint32_t tryEndOffset = entry.tryEndOffset;
            
            for (IRBlock* block : m_currentFunction->blocks) {
                // このブロックが例外をスローする可能性があるかチェック
                for (IRInstruction* inst : block->instructions) {
                    uint32_t offset = inst->bytecodeOffset;
                    
                    if (offset >= tryStartOffset && offset < tryEndOffset) {
                        // 例外スロー命令や関数呼び出しなど
                        if (inst->opcode == IROpcode::Call || 
                            inst->opcode == IROpcode::Throw) {
                            block->addSuccessor(handlerBlock);
                            break;
                        }
                    }
                }
            }
        }
    }
}

// 到達不能なブロックを削除
void IRBuilder::removeUnreachableBlocks()
{
    // エントリブロックから到達可能なブロックを収集
    std::vector<bool> reachable(m_nextBlockId, false);
    std::vector<IRBlock*> workList = { m_currentFunction->entryBlock };
    
    while (!workList.empty()) {
        IRBlock* block = workList.back();
        workList.pop_back();
        
        if (!block || reachable[block->id]) {
            continue;
        }
        
        reachable[block->id] = true;
        
        // 後続ブロックをワークリストに追加
        for (IRBlock* succ : block->successors) {
            if (!reachable[succ->id]) {
                workList.push_back(succ);
            }
        }
    }
    
    // 例外ハンドラブロックは明示的に到達可能とする
    for (IRBlock* block : m_currentFunction->blocks) {
        if (block->isHandler) {
            reachable[block->id] = true;
        }
    }
    
    // 到達不能なブロックを削除
    auto it = m_currentFunction->blocks.begin();
    while (it != m_currentFunction->blocks.end()) {
        IRBlock* block = *it;
        
        if (!reachable[block->id] && 
            block != m_currentFunction->entryBlock && 
            block != m_currentFunction->exitBlock) {
            // ブロックへの参照を削除
            for (IRBlock* other : m_currentFunction->blocks) {
                auto& succs = other->successors;
                succs.erase(std::remove(succs.begin(), succs.end(), block), succs.end());
                
                auto& preds = other->predecessors;
                preds.erase(std::remove(preds.begin(), preds.end(), block), preds.end());
            }
            
            // ブロックを削除
            it = m_currentFunction->blocks.erase(it);
        } else {
            ++it;
        }
    }
}

// PHIノードの挿入
void IRBuilder::insertPhiNodes()
{
    // 各ブロックの直近の定義を追跡する
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, IRValue*>> varDefs;
    
    // ブロック間の変数の生存情報を計算
    std::unordered_map<IRBlock*, std::unordered_set<uint32_t>> liveIn;
    std::unordered_map<IRBlock*, std::unordered_set<uint32_t>> liveOut;
    
    // 各ブロック内の変数の定義と使用を収集
    std::unordered_map<IRBlock*, std::unordered_set<uint32_t>> defines;
    std::unordered_map<IRBlock*, std::unordered_set<uint32_t>> uses;
    
    for (IRBlock* block : m_currentFunction->blocks) {
        for (IRInstruction* inst : block->instructions) {
            // 結果値がある場合、それは定義
            if (inst->result) {
                defines[block].insert(inst->result->id);
            }
            
            // オペランドは使用
            for (IRValue* operand : inst->operands) {
                if (operand) {
                    uses[block].insert(operand->id);
                }
            }
        }
    }
    
    // 生存解析の反復計算
    bool changed = true;
    while (changed) {
        changed = false;
        
        for (IRBlock* block : m_currentFunction->blocks) {
            // LiveOut = Union(LiveIn(S) for S in Successors)
            std::unordered_set<uint32_t> newLiveOut;
            for (IRBlock* succ : block->successors) {
                const auto& succLiveIn = liveIn[succ];
                newLiveOut.insert(succLiveIn.begin(), succLiveIn.end());
            }
            
            // LiveIn = Uses + (LiveOut - Defines)
            std::unordered_set<uint32_t> newLiveIn = uses[block];
            for (uint32_t var : newLiveOut) {
                if (defines[block].find(var) == defines[block].end()) {
                    newLiveIn.insert(var);
                }
            }
            
            // 変更があるか確認
            if (newLiveOut != liveOut[block] || newLiveIn != liveIn[block]) {
                changed = true;
                liveOut[block] = newLiveOut;
                liveIn[block] = newLiveIn;
            }
        }
    }
    
    // 複数の定義が合流するポイントでPHIノードを挿入
    for (IRBlock* block : m_currentFunction->blocks) {
        if (block->predecessors.size() <= 1) {
            continue; // 合流点でない場合はスキップ
        }
        
        // このブロックに入る前に生存している変数を調べる
        for (uint32_t var : liveIn[block]) {
            // 異なる定義元があるかチェック
            bool needsPhi = false;
            IRValue* firstDef = nullptr;
            
            for (IRBlock* pred : block->predecessors) {
                if (defines[pred].find(var) != defines[pred].end()) {
                    if (!firstDef) {
                        firstDef = getLastDefinition(pred, var);
                    } else {
                        IRValue* otherDef = getLastDefinition(pred, var);
                        if (otherDef != firstDef) {
                            needsPhi = true;
                            break;
                        }
                    }
                }
            }
            
            if (needsPhi) {
                // PHIノードを作成
                IRValue* phiResult = createValue(IRType::Any); // 実際の型は解析が必要
                IRInstruction* phiInst = createInstruction(IROpcode::Phi, phiResult);
                
                // 各先行ブロックからの値を追加
                for (IRBlock* pred : block->predecessors) {
                    IRValue* predDef = getLastDefinition(pred, var);
                    if (predDef) {
                        phiInst->addOperand(predDef);
                    } else {
                        // 定義が見つからない場合はundefinedを使用
                        IRValue* undefinedValue = createValue(IRType::Any);
                        undefinedValue->setFlag(IRValueFlags::Constant);
                        phiInst->addOperand(undefinedValue);
                    }
                }
                
                // ブロックの先頭に挿入
                block->instructions.insert(block->instructions.begin(), phiInst);
                block->addPhiValue(phiResult);
            }
        }
    }
}

// 変数の最新の定義を取得
IRValue* IRBuilder::getLastDefinition(IRBlock* block, uint32_t varId)
{
    // ブロック内の命令を逆順に走査
    for (auto it = block->instructions.rbegin(); it != block->instructions.rend(); ++it) {
        IRInstruction* inst = *it;
        if (inst->result && inst->result->id == varId) {
            return inst->result;
        }
    }
    
    // ブロック内に定義がなければ先行ブロックを再帰的に検索
    // 注: これは簡略化された実装で、実際にはループを処理するためのより複雑なアルゴリズムが必要
    if (block->predecessors.size() == 1) {
        return getLastDefinition(block->predecessors[0], varId);
    }
    
    return nullptr;
}

// 次の命令を処理
void IRBuilder::processNextInstruction()
{
    if (m_bytecodeIndex >= m_bytecodeLength) {
        return;
    }
    
    uint32_t currentOffset = m_bytecodeIndex;
    uint8_t opcode = readByte();
    
    // オペコードに基づいて処理
    switch (opcode) {
        // NoOp
        case 0x00:
        {
            IRInstruction* inst = createInstruction(IROpcode::NoOp);
            inst->bytecodeOffset = currentOffset;
            m_currentBlock->addInstruction(inst);
            break;
        }
        
        // Load定数
        case 0x01:
        {
            uint8_t destReg = readByte();
            uint32_t value = readDWord();
            
            IRValue* resultValue = createValue(IRType::Int32);
            IRInstruction* inst = createInstruction(IROpcode::LoadConst, resultValue);
            inst->bytecodeOffset = currentOffset;
            // Value定数を作成（簡易実装）
            // 実際の実装ではValueオブジェクトの正しい処理が必要
            inst->debugInfo = "LoadConst: " + std::to_string(value);
            m_currentBlock->addInstruction(inst);
            
            // ローカル変数マップに追加
            m_currentFunction->addLocal(destReg, resultValue);
            break;
        }
        
        // Load変数
        case 0x02:
        {
            uint8_t destReg = readByte();
            uint8_t srcReg = readByte();
            
            IRValue* srcValue = loadLocal(srcReg);
            if (srcValue) {
                IRValue* resultValue = createValue(srcValue->type);
                IRInstruction* inst = createInstruction(IROpcode::LoadLocal, resultValue);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(srcValue);
                m_currentBlock->addInstruction(inst);
                
                // ローカル変数マップに追加
                m_currentFunction->addLocal(destReg, resultValue);
            }
            break;
        }
        
        // Store変数
        case 0x03:
        {
            uint8_t destReg = readByte();
            uint8_t srcReg = readByte();
            
            IRValue* srcValue = loadLocal(srcReg);
            if (srcValue) {
                IRInstruction* inst = createInstruction(IROpcode::StoreLocal);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(srcValue);
                inst->debugInfo = "StoreLocal: " + std::to_string(destReg);
                m_currentBlock->addInstruction(inst);
                
                // ローカル変数マップに追加
                m_currentFunction->addLocal(destReg, srcValue);
            }
            break;
        }
        
        // 加算
        case 0x10:
        {
            uint8_t destReg = readByte();
            uint8_t src1Reg = readByte();
            uint8_t src2Reg = readByte();
            
            IRValue* src1Value = loadLocal(src1Reg);
            IRValue* src2Value = loadLocal(src2Reg);
            
            if (src1Value && src2Value) {
                IRValue* resultValue = createValue(IRType::Any);
                IRInstruction* inst = createInstruction(IROpcode::Add, resultValue);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(src1Value);
                inst->addOperand(src2Value);
                m_currentBlock->addInstruction(inst);
                
                // ローカル変数マップに追加
                m_currentFunction->addLocal(destReg, resultValue);
            }
            break;
        }
        
        // 条件分岐
        case 0x20:
        {
            uint8_t condReg = readByte();
            int32_t offset = readSignedDWord();
            
            IRValue* condValue = loadLocal(condReg);
            if (condValue) {
                uint32_t targetOffset = static_cast<uint32_t>(static_cast<int32_t>(currentOffset) + offset);
                IRBlock* trueBlock = getOrCreateBlockAt(targetOffset);
                IRBlock* falseBlock = getOrCreateBlockAt(m_bytecodeIndex);
                
                IRInstruction* inst = createInstruction(IROpcode::Branch);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(condValue);
                inst->debugInfo = "Branch: true=" + std::to_string(trueBlock->id) + 
                                  ", false=" + std::to_string(falseBlock->id);
                m_currentBlock->addInstruction(inst);
                
                // ブロック間の接続を設定
                m_currentBlock->addSuccessor(trueBlock);
                m_currentBlock->addSuccessor(falseBlock);
            }
            break;
        }
        
        // 関数呼び出し
        case 0x30:
        {
            uint8_t destReg = readByte();
            uint8_t funcReg = readByte();
            uint8_t argCount = readByte();
            std::vector<IRValue*> args;
            
            for (uint8_t i = 0; i < argCount; ++i) {
                uint8_t argReg = readByte();
                IRValue* argValue = loadLocal(argReg);
                if (argValue) {
                    args.push_back(argValue);
                }
            }
            
            IRValue* funcValue = loadLocal(funcReg);
            if (funcValue) {
                IRValue* resultValue = createValue(IRType::Any);
                IRInstruction* inst = createInstruction(IROpcode::Call, resultValue);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(funcValue);
                
                for (IRValue* arg : args) {
                    inst->addOperand(arg);
                }
                
                inst->debugInfo = "Call: args=" + std::to_string(argCount);
                m_currentBlock->addInstruction(inst);
                
                // ローカル変数マップに追加
                m_currentFunction->addLocal(destReg, resultValue);
            }
            break;
        }
        
        // リターン
        case 0x40:
        {
            IRInstruction* inst = createInstruction(IROpcode::Return);
            inst->bytecodeOffset = currentOffset;
            m_currentBlock->addInstruction(inst);
            
            // 終了ブロックへの接続
            m_currentBlock->addSuccessor(m_currentFunction->exitBlock);
            break;
        }
        
        // 値付きリターン
        case 0x41:
        {
            uint8_t retReg = readByte();
            IRValue* retValue = loadLocal(retReg);
            
            if (retValue) {
                IRInstruction* inst = createInstruction(IROpcode::Return);
                inst->bytecodeOffset = currentOffset;
                inst->addOperand(retValue);
                m_currentBlock->addInstruction(inst);
                
                // 終了ブロックへの接続
                m_currentBlock->addSuccessor(m_currentFunction->exitBlock);
            }
            break;
        }
        
        // デフォルト：未実装の命令
        default:
        {
            // 未実装の命令をスキップ
            // 実際の実装では全命令に対応する必要がある
            std::cerr << "Unimplemented bytecode: 0x" << std::hex << static_cast<int>(opcode) 
                      << std::dec << " at offset " << currentOffset << std::endl;
            
            // 簡易的に次の命令に進む
            break;
        }
    }
}

// ブロック開始位置をマーク
void IRBuilder::markBlockStart(uint32_t bytecodeOffset)
{
    if (bytecodeOffset < m_bytecodeLength) {
        m_blockStarts.insert(bytecodeOffset);
    }
}

// 指定オフセットのブロックを取得または作成
IRBlock* IRBuilder::getOrCreateBlockAt(uint32_t bytecodeOffset)
{
    auto it = m_blockMap.find(bytecodeOffset);
    if (it != m_blockMap.end()) {
        return it->second;
    }
    
    // ブロックが存在しない場合は新規作成
    IRBlock* block = createBlock();
    m_blockMap[bytecodeOffset] = block;
    m_currentFunction->addBlock(block);
    
    return block;
}

// 新しいブロックを作成
IRBlock* IRBuilder::createBlock()
{
    IRBlock* block = new IRBlock(m_nextBlockId++);
    return block;
}

// 新しい値を作成
IRValue* IRBuilder::createValue(IRType type)
{
    IRValue* value = new IRValue(m_nextValueId++, type);
    m_currentFunction->addValue(value);
    return value;
}

// 命令を作成
IRInstruction* IRBuilder::createInstruction(IROpcode opcode, IRValue* result)
{
    IRInstruction* inst = new IRInstruction(opcode, result);
    m_currentFunction->addInstruction(inst);
    return inst;
}

// ローカル変数のロード
IRValue* IRBuilder::loadLocal(uint32_t index)
{
    auto it = m_currentFunction->locals.find(index);
    if (it != m_currentFunction->locals.end()) {
        return it->second;
    }
    return nullptr;
}

// ローカル変数のストア
void IRBuilder::storeLocal(uint32_t index, IRValue* value)
{
    if (value) {
        m_currentFunction->locals[index] = value;
    }
}

// 引数のロード
IRValue* IRBuilder::loadArg(uint32_t index)
{
    auto it = m_currentFunction->arguments.find(index);
    if (it != m_currentFunction->arguments.end()) {
        return it->second;
    }
    return nullptr;
}

// 引数のストア
void IRBuilder::storeArg(uint32_t index, IRValue* value)
{
    if (value) {
        m_currentFunction->arguments[index] = value;
    }
}

// 定数のロード
IRValue* IRBuilder::loadConstant(const Value& value)
{
    // Value型に応じて適切なIRTypeを設定
    IRType type = IRType::Any;
    
    // 値の型に基づいて適切なIRTypeを設定
    if (value.isUndefined()) {
        type = IRType::Any;
    } else if (value.isNull()) {
        type = IRType::Any;
    } else if (value.isBoolean()) {
        type = IRType::Boolean;
    } else if (value.isNumber()) {
        // 整数か浮動小数点数かを判断
        if (value.isInt32()) {
            type = IRType::Int32;
        } else if (value.isInt64()) {
            type = IRType::Int64;
        } else {
            type = IRType::Float64;
        }
    } else if (value.isString()) {
        type = IRType::String;
    } else if (value.isObject()) {
        type = IRType::Object;
    } else if (value.isArray()) {
        type = IRType::Array;
    } else if (value.isFunction()) {
        type = IRType::Function;
    }
    
    IRValue* irValue = createValue(type);
    irValue->setFlag(IRValueFlags::Constant);
    
    // 値の実際のデータを保存
    // 実際のエンジン実装に応じて、値のビット表現をIRValueに関連付ける
    // 例えば、Value型のメモリレイアウトを直接参照するか、
    // コンスタントプールに追加してそのインデックスを保持するなど
    
    // 定数値をデバッグ情報として保存（文字列表現）
    if (value.isUndefined()) {
        irValue->debugInfo = "undefined";
    } else if (value.isNull()) {
        irValue->debugInfo = "null";
    } else if (value.isBoolean()) {
        irValue->debugInfo = value.toBoolean() ? "true" : "false";
    } else if (value.isNumber()) {
        irValue->debugInfo = std::to_string(value.toNumber());
    } else if (value.isString()) {
        // 文字列の場合は先頭10文字まで表示
        std::string str = value.toString();
        if (str.length() > 10) {
            irValue->debugInfo = "\"" + str.substr(0, 10) + "...\"";
        } else {
            irValue->debugInfo = "\"" + str + "\"";
        }
    } else if (value.isObject()) {
        irValue->debugInfo = "[object Object]";
    } else if (value.isArray()) {
        irValue->debugInfo = "[object Array]";
    } else if (value.isFunction()) {
        irValue->debugInfo = "[object Function]";
    }
    
    return irValue;
}

// 条件分岐の発行
void IRBuilder::emitBranch(IRValue* condition, IRBlock* trueBlock, IRBlock* falseBlock)
{
    if (!condition || !trueBlock || !falseBlock) {
        return;
    }
    
    IRInstruction* inst = createInstruction(IROpcode::Branch);
    inst->addOperand(condition);
    
    // デバッグ情報を設定
    inst->debugInfo = "Branch: true=" + std::to_string(trueBlock->id) + 
                      ", false=" + std::to_string(falseBlock->id);
    
    m_currentBlock->addInstruction(inst);
    
    // ブロック間の接続を設定
    m_currentBlock->addSuccessor(trueBlock);
    m_currentBlock->addSuccessor(falseBlock);
}

// ジャンプの発行
void IRBuilder::emitJump(IRBlock* target)
{
    if (!target) {
        return;
    }
    
    IRInstruction* inst = createInstruction(IROpcode::Jump);
    
    // デバッグ情報を設定
    inst->debugInfo = "Jump: target=" + std::to_string(target->id);
    
    m_currentBlock->addInstruction(inst);
    
    // ブロック間の接続を設定
    m_currentBlock->addSuccessor(target);
}

// リターンの発行
void IRBuilder::emitReturn(IRValue* value)
{
    IRInstruction* inst = createInstruction(IROpcode::Return);
    
    if (value) {
        inst->addOperand(value);
    }
    
    m_currentBlock->addInstruction(inst);
    
    // 終了ブロックへの接続
    m_currentBlock->addSuccessor(m_currentFunction->exitBlock);
}

// 関数呼び出しの発行
IRValue* IRBuilder::emitCall(IRValue* callee, const std::vector<IRValue*>& args)
{
    if (!callee) {
        return nullptr;
    }
    
    IRValue* resultValue = createValue(IRType::Any);
    IRInstruction* inst = createInstruction(IROpcode::Call, resultValue);
    
    inst->addOperand(callee);
    
    for (IRValue* arg : args) {
        if (arg) {
            inst->addOperand(arg);
        }
    }
    
    inst->debugInfo = "Call: args=" + std::to_string(args.size());
    m_currentBlock->addInstruction(inst);
    
    return resultValue;
}

// 1バイト読み込み
uint8_t IRBuilder::readByte()
{
    if (m_bytecodeIndex < m_bytecodeLength) {
        return m_bytecode[m_bytecodeIndex++];
    }
    return 0;
}

// 2バイト読み込み
uint16_t IRBuilder::readWord()
{
    uint16_t value = 0;
    if (m_bytecodeIndex + 1 < m_bytecodeLength) {
        value = (static_cast<uint16_t>(m_bytecode[m_bytecodeIndex]) |
                 (static_cast<uint16_t>(m_bytecode[m_bytecodeIndex + 1]) << 8));
        m_bytecodeIndex += 2;
    }
    return value;
}

// 4バイト読み込み
uint32_t IRBuilder::readDWord()
{
    uint32_t value = 0;
    if (m_bytecodeIndex + 3 < m_bytecodeLength) {
        value = (static_cast<uint32_t>(m_bytecode[m_bytecodeIndex]) |
                 (static_cast<uint32_t>(m_bytecode[m_bytecodeIndex + 1]) << 8) |
                 (static_cast<uint32_t>(m_bytecode[m_bytecodeIndex + 2]) << 16) |
                 (static_cast<uint32_t>(m_bytecode[m_bytecodeIndex + 3]) << 24));
        m_bytecodeIndex += 4;
    }
    return value;
}

// 符号付き4バイト読み込み
int32_t IRBuilder::readSignedDWord()
{
    return static_cast<int32_t>(readDWord());
}

} // namespace core
} // namespace aerojs 