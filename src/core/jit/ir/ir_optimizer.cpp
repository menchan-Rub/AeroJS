/**
 * @file ir_optimizer.cpp
 * @brief AeroJS JavaScript エンジンのIR最適化器の実装
 * @version 1.0.0
 * @license MIT
 */

#include "ir_optimizer.h"
#include "ir_builder.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cassert>
#include <chrono>

namespace aerojs {
namespace core {

//===============================================================
// 定数畳み込み最適化パス
//===============================================================
bool ConstantFoldingPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 各ブロックの命令を処理
    for (IRBlock* block : function->blocks) {
        // イテレータを使用して命令を処理（削除の可能性があるため）
        auto it = block->instructions.begin();
        while (it != block->instructions.end()) {
            IRInstruction* inst = *it;
            
            // 定数オペランドのみの命令を処理
            if (inst->result && inst->operands.size() >= 1) {
                bool allConstant = true;
                
                // すべてのオペランドが定数かチェック
                for (IRValue* operand : inst->operands) {
                    if (!operand || !operand->isConstant()) {
                        allConstant = false;
                        break;
                    }
                }
                
                if (allConstant) {
                    bool folded = false;
                    
                    // オペコードに基づいて計算
                    switch (inst->opcode) {
                        case IROpcode::Add:
                        {
                            if (inst->operands.size() == 2) {
                                if (inst->operands[0]->type == IRType::Int32 && 
                                    inst->operands[1]->type == IRType::Int32) {
                                    // 整数の加算
                                    int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                    int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                    int32_t result = val1 + val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Int32;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = std::to_string(result);
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                                else if (inst->operands[0]->type == IRType::Float64 || 
                                         inst->operands[1]->type == IRType::Float64) {
                                    // 浮動小数点数の加算
                                    double val1 = std::stod(inst->operands[0]->debugInfo);
                                    double val2 = std::stod(inst->operands[1]->debugInfo);
                                    double result = val1 + val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Float64;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = std::to_string(result);
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                                else if (inst->operands[0]->type == IRType::String || 
                                         inst->operands[1]->type == IRType::String) {
                                    // 文字列連結（debugInfoから文字列を取得）
                                    std::string val1 = inst->operands[0]->debugInfo;
                                    std::string val2 = inst->operands[1]->debugInfo;
                                    
                                    // 引用符を削除
                                    if (val1.size() >= 2 && val1.front() == '"' && val1.back() == '"') {
                                        val1 = val1.substr(1, val1.size() - 2);
                                    }
                                    if (val2.size() >= 2 && val2.front() == '"' && val2.back() == '"') {
                                        val2 = val2.substr(1, val2.size() - 2);
                                    }
                                    
                                    std::string result = val1 + val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::String;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = "\"" + result + "\"";
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                            }
                            break;
                        }
                        
                        case IROpcode::Sub:
                        {
                            if (inst->operands.size() == 2) {
                                if (inst->operands[0]->type == IRType::Int32 && 
                                    inst->operands[1]->type == IRType::Int32) {
                                    // 整数の減算
                                    int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                    int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                    int32_t result = val1 - val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Int32;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = std::to_string(result);
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                                else if (inst->operands[0]->type == IRType::Float64 || 
                                         inst->operands[1]->type == IRType::Float64) {
                                    // 浮動小数点数の減算
                                    double val1 = std::stod(inst->operands[0]->debugInfo);
                                    double val2 = std::stod(inst->operands[1]->debugInfo);
                                    double result = val1 - val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Float64;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = std::to_string(result);
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                            }
                            break;
                        }
                        
                        case IROpcode::Mul:
                        {
                            if (inst->operands.size() == 2) {
                                if (inst->operands[0]->type == IRType::Int32 && 
                                    inst->operands[1]->type == IRType::Int32) {
                                    // 整数の乗算
                                    int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                    int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                    
                                    // オーバーフローチェック
                                    if ((val1 == 0) || (val2 == 0) || 
                                        (val1 <= INT32_MAX / val2 && val1 >= INT32_MIN / val2)) {
                                        int32_t result = val1 * val2;
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Int32;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = std::to_string(result);
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    }
                                }
                                else if (inst->operands[0]->type == IRType::Float64 || 
                                         inst->operands[1]->type == IRType::Float64) {
                                    // 浮動小数点数の乗算
                                    double val1 = std::stod(inst->operands[0]->debugInfo);
                                    double val2 = std::stod(inst->operands[1]->debugInfo);
                                    double result = val1 * val2;
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Float64;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = std::to_string(result);
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                            }
                            break;
                        }
                        
                        case IROpcode::Div:
                        {
                            if (inst->operands.size() == 2) {
                                if (inst->operands[0]->type == IRType::Int32 && 
                                    inst->operands[1]->type == IRType::Int32) {
                                    // 整数の除算
                                    int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                    int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                    
                                    // ゼロ除算チェック
                                    if (val2 != 0) {
                                        // JavaScript互換の除算（結果は浮動小数点）
                                        double result = static_cast<double>(val1) / val2;
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Float64;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = std::to_string(result);
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    }
                                }
                                else if (inst->operands[0]->type == IRType::Float64 || 
                                         inst->operands[1]->type == IRType::Float64) {
                                    // 浮動小数点数の除算
                                    double val1 = std::stod(inst->operands[0]->debugInfo);
                                    double val2 = std::stod(inst->operands[1]->debugInfo);
                                    
                                    // ゼロ除算チェック（結果はJavaScriptの仕様に従って処理）
                                    if (val2 != 0.0) {
                                        double result = val1 / val2;
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Float64;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = std::to_string(result);
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    } else {
                                        // ゼロ除算の場合はInfinityまたはNaNを設定
                                        std::string result;
                                        if (val1 == 0.0) {
                                            result = "NaN"; // 0/0 はNaN
                                        } else if (val1 > 0.0) {
                                            result = "Infinity"; // 正の数/0 はInfinity
                                        } else {
                                            result = "-Infinity"; // 負の数/0 は-Infinity
                                        }
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Float64;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = result;
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    }
                                }
                            }
                            break;
                        }
                        
                        case IROpcode::Mod:
                        {
                            if (inst->operands.size() == 2) {
                                if (inst->operands[0]->type == IRType::Int32 && 
                                    inst->operands[1]->type == IRType::Int32) {
                                    // 整数の剰余
                                    int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                    int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                    
                                    // ゼロ除算チェック
                                    if (val2 != 0) {
                                        int32_t result = val1 % val2;
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Int32;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = std::to_string(result);
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    }
                                }
                                else if (inst->operands[0]->type == IRType::Float64 || 
                                         inst->operands[1]->type == IRType::Float64) {
                                    // 浮動小数点数の剰余
                                    double val1 = std::stod(inst->operands[0]->debugInfo);
                                    double val2 = std::stod(inst->operands[1]->debugInfo);
                                    
                                    // ゼロ除算チェック
                                    if (val2 != 0.0) {
                                        double result = std::fmod(val1, val2);
                                        
                                        // 結果値を更新
                                        inst->result->type = IRType::Float64;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = std::to_string(result);
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    } else {
                                        // ゼロ除算の場合はNaN
                                        // 結果値を更新
                                        inst->result->type = IRType::Float64;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = "NaN";
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                        
                                        folded = true;
                                    }
                                }
                            }
                            break;
                        }
                        
                        case IROpcode::BitAnd:
                        case IROpcode::BitOr:
                        case IROpcode::BitXor:
                        {
                            if (inst->operands.size() == 2 && 
                                inst->operands[0]->type == IRType::Int32 && 
                                inst->operands[1]->type == IRType::Int32) {
                                
                                int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                int32_t result = 0;
                                
                                // 演算子に応じた計算
                                if (inst->opcode == IROpcode::BitAnd) {
                                    result = val1 & val2;
                                } else if (inst->opcode == IROpcode::BitOr) {
                                    result = val1 | val2;
                                } else { // BitXor
                                    result = val1 ^ val2;
                                }
                                
                                // 結果値を更新
                                inst->result->type = IRType::Int32;
                                inst->result->setFlag(IRValueFlags::Constant);
                                inst->result->debugInfo = std::to_string(result);
                                
                                // 命令を定数ロードに変更
                                inst->opcode = IROpcode::LoadConst;
                                inst->operands.clear();
                                
                                folded = true;
                            }
                            break;
                        }
                        
                        case IROpcode::LeftShift:
                        case IROpcode::RightShift:
                        case IROpcode::UnsignedRightShift:
                        {
                            if (inst->operands.size() == 2 && 
                                inst->operands[0]->type == IRType::Int32 && 
                                inst->operands[1]->type == IRType::Int32) {
                                
                                int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                int32_t result = 0;
                                
                                // シフト量を0-31に制限（JavaScript仕様）
                                val2 &= 0x1F;
                                
                                // 演算子に応じた計算
                                if (inst->opcode == IROpcode::LeftShift) {
                                    result = val1 << val2;
                                } else if (inst->opcode == IROpcode::RightShift) {
                                    result = val1 >> val2;
                                } else { // UnsignedRightShift
                                    result = static_cast<uint32_t>(val1) >> val2;
                                }
                                
                                // 結果値を更新
                                inst->result->type = IRType::Int32;
                                inst->result->setFlag(IRValueFlags::Constant);
                                inst->result->debugInfo = std::to_string(result);
                                
                                // 命令を定数ロードに変更
                                inst->opcode = IROpcode::LoadConst;
                                inst->operands.clear();
                                
                                folded = true;
                            }
                            break;
                        }
                        
                        case IROpcode::LogicalAnd:
                        case IROpcode::LogicalOr:
                        {
                            if (inst->operands.size() == 2 && 
                                inst->operands[0]->type == IRType::Boolean && 
                                inst->operands[1]->type == IRType::Boolean) {
                                
                                bool val1 = (inst->operands[0]->debugInfo == "true");
                                bool val2 = (inst->operands[1]->debugInfo == "true");
                                bool result = false;
                                
                                // 演算子に応じた計算
                                if (inst->opcode == IROpcode::LogicalAnd) {
                                    result = val1 && val2;
                                } else { // LogicalOr
                                    result = val1 || val2;
                                }
                                
                                // 結果値を更新
                                inst->result->type = IRType::Boolean;
                                inst->result->setFlag(IRValueFlags::Constant);
                                inst->result->debugInfo = result ? "true" : "false";
                                
                                // 命令を定数ロードに変更
                                inst->opcode = IROpcode::LoadConst;
                                inst->operands.clear();
                                
                                folded = true;
                            }
                            break;
                        }
                        
                        case IROpcode::LogicalNot:
                        {
                            if (inst->operands.size() == 1 && 
                                inst->operands[0]->type == IRType::Boolean) {
                                
                                bool val = (inst->operands[0]->debugInfo == "true");
                                bool result = !val;
                                
                                // 結果値を更新
                                inst->result->type = IRType::Boolean;
                                inst->result->setFlag(IRValueFlags::Constant);
                                inst->result->debugInfo = result ? "true" : "false";
                                
                                // 命令を定数ロードに変更
                                inst->opcode = IROpcode::LoadConst;
                                inst->operands.clear();
                                
                                folded = true;
                            }
                            break;
                        }
                        
                        // 比較演算
                        case IROpcode::Equal:
                        case IROpcode::NotEqual:
                        case IROpcode::StrictEqual:
                        case IROpcode::StrictNotEqual:
                        case IROpcode::LessThan:
                        case IROpcode::LessThanOrEqual:
                        case IROpcode::GreaterThan:
                        case IROpcode::GreaterThanOrEqual:
                        {
                            if (inst->operands.size() == 2) {
                                // 型が一致する場合のみ定数畳み込みを行う（単純化のため）
                                if (inst->operands[0]->type == inst->operands[1]->type) {
                                    bool result = false;
                                    
                                    if (inst->operands[0]->type == IRType::Int32) {
                                        int32_t val1 = std::stoi(inst->operands[0]->debugInfo);
                                        int32_t val2 = std::stoi(inst->operands[1]->debugInfo);
                                        
                                        switch (inst->opcode) {
                                            case IROpcode::Equal:
                                            case IROpcode::StrictEqual:
                                                result = (val1 == val2);
                                                break;
                                            case IROpcode::NotEqual:
                                            case IROpcode::StrictNotEqual:
                                                result = (val1 != val2);
                                                break;
                                            case IROpcode::LessThan:
                                                result = (val1 < val2);
                                                break;
                                            case IROpcode::LessThanOrEqual:
                                                result = (val1 <= val2);
                                                break;
                                            case IROpcode::GreaterThan:
                                                result = (val1 > val2);
                                                break;
                                            case IROpcode::GreaterThanOrEqual:
                                                result = (val1 >= val2);
                                                break;
                                            default:
                                                break;
                                        }
                                        folded = true;
                                    }
                                    else if (inst->operands[0]->type == IRType::Float64) {
                                        double val1 = std::stod(inst->operands[0]->debugInfo);
                                        double val2 = std::stod(inst->operands[1]->debugInfo);
                                        
                                        switch (inst->opcode) {
                                            case IROpcode::Equal:
                                            case IROpcode::StrictEqual:
                                                result = (val1 == val2);
                                                break;
                                            case IROpcode::NotEqual:
                                            case IROpcode::StrictNotEqual:
                                                result = (val1 != val2);
                                                break;
                                            case IROpcode::LessThan:
                                                result = (val1 < val2);
                                                break;
                                            case IROpcode::LessThanOrEqual:
                                                result = (val1 <= val2);
                                                break;
                                            case IROpcode::GreaterThan:
                                                result = (val1 > val2);
                                                break;
                                            case IROpcode::GreaterThanOrEqual:
                                                result = (val1 >= val2);
                                                break;
                                            default:
                                                break;
                                        }
                                        folded = true;
                                    }
                                    else if (inst->operands[0]->type == IRType::Boolean) {
                                        bool val1 = (inst->operands[0]->debugInfo == "true");
                                        bool val2 = (inst->operands[1]->debugInfo == "true");
                                        
                                        switch (inst->opcode) {
                                            case IROpcode::Equal:
                                            case IROpcode::StrictEqual:
                                                result = (val1 == val2);
                                                break;
                                            case IROpcode::NotEqual:
                                            case IROpcode::StrictNotEqual:
                                                result = (val1 != val2);
                                                break;
                                            default:
                                                // 他の比較はブール値では非標準
                                                break;
                                        }
                                        folded = true;
                                    }
                                    else if (inst->operands[0]->type == IRType::String && 
                                            (inst->opcode == IROpcode::Equal || 
                                             inst->opcode == IROpcode::NotEqual || 
                                             inst->opcode == IROpcode::StrictEqual || 
                                             inst->opcode == IROpcode::StrictNotEqual)) {
                                        std::string val1 = inst->operands[0]->debugInfo;
                                        std::string val2 = inst->operands[1]->debugInfo;
                                        
                                        // 引用符を削除
                                        if (val1.size() >= 2 && val1.front() == '"' && val1.back() == '"') {
                                            val1 = val1.substr(1, val1.size() - 2);
                                        }
                                        if (val2.size() >= 2 && val2.front() == '"' && val2.back() == '"') {
                                            val2 = val2.substr(1, val2.size() - 2);
                                        }
                                        
                                        switch (inst->opcode) {
                                            case IROpcode::Equal:
                                            case IROpcode::StrictEqual:
                                                result = (val1 == val2);
                                                break;
                                            case IROpcode::NotEqual:
                                            case IROpcode::StrictNotEqual:
                                                result = (val1 != val2);
                                                break;
                                            default:
                                                break;
                                        }
                                        folded = true;
                                    }
                                    
                                    if (folded) {
                                        // 結果値を更新
                                        inst->result->type = IRType::Boolean;
                                        inst->result->setFlag(IRValueFlags::Constant);
                                        inst->result->debugInfo = result ? "true" : "false";
                                        
                                        // 命令を定数ロードに変更
                                        inst->opcode = IROpcode::LoadConst;
                                        inst->operands.clear();
                                    }
                                }
                                // Null/Undefinedに対する特別な処理
                                else if ((inst->operands[0]->debugInfo == "null" || 
                                          inst->operands[0]->debugInfo == "undefined") && 
                                         (inst->operands[1]->debugInfo == "null" || 
                                          inst->operands[1]->debugInfo == "undefined")) {
                                    bool result = false;
                                    
                                    switch (inst->opcode) {
                                        case IROpcode::Equal:
                                            // == ではnullとundefinedは等しい
                                            result = true;
                                            break;
                                        case IROpcode::NotEqual:
                                            result = false;
                                            break;
                                        case IROpcode::StrictEqual:
                                            // === では同じ型のみ等しい
                                            result = (inst->operands[0]->debugInfo == inst->operands[1]->debugInfo);
                                            break;
                                        case IROpcode::StrictNotEqual:
                                            result = (inst->operands[0]->debugInfo != inst->operands[1]->debugInfo);
                                            break;
                                        default:
                                            break;
                                    }
                                    
                                    // 結果値を更新
                                    inst->result->type = IRType::Boolean;
                                    inst->result->setFlag(IRValueFlags::Constant);
                                    inst->result->debugInfo = result ? "true" : "false";
                                    
                                    // 命令を定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    
                                    folded = true;
                                }
                            }
                            break;
                        }
                        
                        default:
                            break;
                    }
                    
                    if (folded) {
                        changed = true;
                    }
                }
            }
            
            ++it;
        }
    }
    
    return changed;
}

//===============================================================
// 共通部分式削除最適化パス
//===============================================================
bool CommonSubexpressionEliminationPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 式のハッシュをキー、IRValueを値とするマップ
    std::unordered_map<size_t, IRValue*> expressionMap;
    
    // 各ブロックの命令を処理
    for (IRBlock* block : function->blocks) {
        expressionMap.clear(); // ブロックごとにリセット（簡易版）
        
        for (IRInstruction* inst : block->instructions) {
            // 結果を持ち、副作用のない式のみを処理
            if (inst->result && inst->operands.size() > 0) {
                // 式のハッシュ値を計算
                size_t hash = static_cast<size_t>(inst->opcode);
                for (IRValue* operand : inst->operands) {
                    if (operand) {
                        hash = hash * 31 + static_cast<size_t>(operand->id);
                    }
                }
                
                // ハッシュが既に存在するか確認
                auto it = expressionMap.find(hash);
                if (it != expressionMap.end()) {
                    // 共通部分式を発見、使用箇所を置き換え
                    // 実際の実装ではより厳密な式の等価性チェックが必要
                    
                    // 古い値への参照を減らす
                    for (IRValue* operand : inst->operands) {
                        if (operand) {
                            operand->refCount--;
                        }
                    }
                    
                    // 共通式の結果を使用
                    inst->result = it->second;
                    it->second->refCount++;
                    
                    changed = true;
                } else {
                    // 新しい式をマップに追加
                    expressionMap[hash] = inst->result;
                }
            }
        }
    }
    
    return changed;
}

//===============================================================
// デッドコード削除最適化パス
//===============================================================
bool DeadCodeEliminationPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 実行フロー解析（到達可能なブロックを特定）
    std::unordered_set<IRBlock*> reachableBlocks;
    std::vector<IRBlock*> workList = { function->entryBlock };
    
    while (!workList.empty()) {
        IRBlock* block = workList.back();
        workList.pop_back();
        
        if (reachableBlocks.insert(block).second) {
            // 後続ブロックを追加
            for (IRBlock* succ : block->successors) {
                if (reachableBlocks.find(succ) == reachableBlocks.end()) {
                    workList.push_back(succ);
                }
            }
        }
    }
    
    // 例外ハンドラは明示的に到達可能とする
    for (IRBlock* block : function->blocks) {
        if (block->isHandler && reachableBlocks.find(block) == reachableBlocks.end()) {
            reachableBlocks.insert(block);
            workList.push_back(block);
            
            // 処理を繰り返す
            while (!workList.empty()) {
                IRBlock* handlerBlock = workList.back();
                workList.pop_back();
                
                for (IRBlock* succ : handlerBlock->successors) {
                    if (reachableBlocks.find(succ) == reachableBlocks.end()) {
                        reachableBlocks.insert(succ);
                        workList.push_back(succ);
                    }
                }
            }
        }
    }
    
    // 到達不能なブロックを削除
    auto blockIt = function->blocks.begin();
    while (blockIt != function->blocks.end()) {
        IRBlock* block = *blockIt;
        
        if (reachableBlocks.find(block) == reachableBlocks.end() && 
            block != function->entryBlock && block != function->exitBlock) {
            
            // ブロックへの参照を削除
            for (IRBlock* other : function->blocks) {
                auto& succs = other->successors;
                succs.erase(std::remove(succs.begin(), succs.end(), block), succs.end());
                
                auto& preds = other->predecessors;
                preds.erase(std::remove(preds.begin(), preds.end(), block), preds.end());
            }
            
            // ブロック内の命令の解放
            for (IRInstruction* inst : block->instructions) {
                if (inst->result) {
                    inst->result->setFlag(IRValueFlags::Eliminated);
                }
                
                // オペランドの参照カウント減少
                for (IRValue* operand : inst->operands) {
                    if (operand) {
                        operand->refCount--;
                    }
                }
                
                // 削除した命令で解放された値の参照カウントをチェック
                for (IRValue* value : function->values) {
                    if (value->refCount <= 0 && !value->isConstant()) {
                        value->setFlag(IRValueFlags::Eliminated);
                    }
                }
            }
            
            // ブロックを削除
            blockIt = function->blocks.erase(blockIt);
            changed = true;
        } else {
            ++blockIt;
        }
    }
    
    // データフロー解析（生きている値を特定）
    std::unordered_set<IRValue*> initialLiveValues;
    
    // 初期化：すべての関数の引数と外部から参照される値は生存
    for (auto& pair : function->arguments) {
        IRValue* arg = pair.second;
        if (arg) {
            initialLiveValues.insert(arg);
            arg->setFlag(IRValueFlags::LiveOut);
        }
    }
    
    // 生存解析の繰り返し計算
    bool liveChanged = true;
    std::unordered_map<IRBlock*, std::unordered_set<IRValue*>> blockLiveIn;
    std::unordered_map<IRBlock*, std::unordered_set<IRValue*>> blockLiveOut;
    
    // 各ブロックのdef-useセットを構築
    std::unordered_map<IRBlock*, std::unordered_set<IRValue*>> blockDefs;
    std::unordered_map<IRBlock*, std::unordered_set<IRValue*>> blockUses;
    
    for (IRBlock* block : reachableBlocks) {
        for (IRInstruction* inst : block->instructions) {
            // 結果値があれば定義
            if (inst->result) {
                blockDefs[block].insert(inst->result);
            }
            
            // オペランドは使用
            for (IRValue* operand : inst->operands) {
                if (operand && blockDefs[block].find(operand) == blockDefs[block].end()) {
                    blockUses[block].insert(operand);
                }
            }
        }
    }
    
    // 反復データフロー解析アルゴリズム
    while (liveChanged) {
        liveChanged = false;
        
        // 逆順でブロックを処理
        for (auto it = reachableBlocks.rbegin(); it != reachableBlocks.rend(); ++it) {
            IRBlock* block = *it;
            
            // LiveOut = Union(LiveIn(successor) for successor in successors)
            std::unordered_set<IRValue*> newLiveOut;
            for (IRBlock* succ : block->successors) {
                const auto& succLiveIn = blockLiveIn[succ];
                newLiveOut.insert(succLiveIn.begin(), succLiveIn.end());
            }
            
            // 終了ブロックの後続がない場合、初期生存値を使用
            if (block->successors.empty() && block != function->exitBlock) {
                newLiveOut = initialLiveValues;
            }
            
            // LiveIn = Uses + (LiveOut - Defs)
            std::unordered_set<IRValue*> newLiveIn = blockUses[block];
            for (IRValue* value : newLiveOut) {
                if (blockDefs[block].find(value) == blockDefs[block].end()) {
                    newLiveIn.insert(value);
                }
            }
            
            // 変更があるか確認
            if (newLiveIn != blockLiveIn[block] || newLiveOut != blockLiveOut[block]) {
                liveChanged = true;
                blockLiveIn[block] = newLiveIn;
                blockLiveOut[block] = newLiveOut;
            }
        }
    }
    
    // 到達可能なブロック内で命令を評価し、不要な命令を削除
    for (IRBlock* block : reachableBlocks) {
        auto instIt = block->instructions.begin();
        while (instIt != block->instructions.end()) {
            IRInstruction* inst = *instIt;
            
            // 副作用のある命令は保持
            bool hasSideEffects = (
                inst->opcode == IROpcode::Call ||
                inst->opcode == IROpcode::StoreGlobal ||
                inst->opcode == IROpcode::StoreProperty ||
                inst->opcode == IROpcode::StoreElement ||
                inst->opcode == IROpcode::StoreLocal ||
                inst->opcode == IROpcode::CreateObject ||
                inst->opcode == IROpcode::CreateArray ||
                inst->opcode == IROpcode::CreateFunction ||
                inst->opcode == IROpcode::Return ||
                inst->opcode == IROpcode::Throw ||
                inst->opcode == IROpcode::Jump ||
                inst->opcode == IROpcode::Branch ||
                inst->opcode == IROpcode::TailCall
            );
            
            // PHIノードは特別扱い（制御フロー解析で必要なため）
            bool isPhi = (inst->opcode == IROpcode::Phi);
            
            // 値が生存しているかチェック
            bool isLive = hasSideEffects || isPhi;
            
            if (!isLive && inst->result) {
                // ブロックの出口で生存しているか確認
                const auto& liveOut = blockLiveOut[block];
                isLive = (liveOut.find(inst->result) != liveOut.end());
                
                // 明示的に他のブロックから参照されている場合も生存
                if (inst->result->isLiveOut()) {
                    isLive = true;
                }
            }
            
            if (isLive) {
                // 生存している命令はそのまま保持
                ++instIt;
            } else {
                // 不要な命令を削除
                if (inst->result) {
                    inst->result->setFlag(IRValueFlags::Eliminated);
                }
                
                // オペランドの参照カウント減少
                for (IRValue* operand : inst->operands) {
                    if (operand) {
                        operand->refCount--;
                    }
                }
                
                // 命令を削除
                instIt = block->instructions.erase(instIt);
                changed = true;
            }
        }
    }
    
    // 解放された値リソースのクリーンアップ（オプション）
    // これは実際の実装では通常はGCやメモリマネージャが担当
    auto valueIt = function->values.begin();
    while (valueIt != function->values.end()) {
        IRValue* value = *valueIt;
        
        if (value->isEliminated()) {
            // 削除済みの値を解放
            valueIt = function->values.erase(valueIt);
        } else {
            ++valueIt;
        }
    }
    
    return changed;
}

//===============================================================
// 命令スケジューリング最適化パス
//===============================================================
bool InstructionSchedulingPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // 各ブロックを処理
    for (IRBlock* block : function->blocks) {
        // 依存関係グラフの構築
        std::unordered_map<IRInstruction*, std::vector<IRInstruction*>> dependencies;
        std::unordered_map<IRInstruction*, int> inDegrees;
        
        // 命令間の依存関係を構築
        for (size_t i = 0; i < block->instructions.size(); ++i) {
            IRInstruction* inst = block->instructions[i];
            inDegrees[inst] = 0;
            
            // このインストラクションの結果を使用する他のインストラクションを見つける
            for (size_t j = i + 1; j < block->instructions.size(); ++j) {
                IRInstruction* later = block->instructions[j];
                
                bool depends = false;
                
                // 直接的な依存関係をチェック
                for (IRValue* operand : later->operands) {
                    if (operand && inst->result == operand) {
                        depends = true;
                        break;
                    }
                }
                
                // メモリアクセスによる依存関係もチェック
                if (!depends) {
                    depends = hasMemoryDependency(inst, later);
                }
                
                // 制御依存関係をチェック
                if (!depends) {
                    depends = hasControlDependency(inst, later);
                }
                
                if (depends) {
                    dependencies[inst].push_back(later);
                    inDegrees[later]++;
                }
            }
        }
        
        // 実行可能な命令のキュー（入次数が0の命令）
        std::vector<IRInstruction*> ready;
        for (auto& pair : inDegrees) {
            if (pair.second == 0) {
                ready.push_back(pair.first);
            }
        }
        
        // 命令のスケジューリング
        std::vector<IRInstruction*> scheduled;
        
        while (!ready.empty()) {
            // 最適な命令を選択
            IRInstruction* next = selectBestInstruction(ready, dependencies, function);
            
            // 選択した命令をreadyリストから削除
            auto it = std::find(ready.begin(), ready.end(), next);
            if (it != ready.end()) {
                ready.erase(it);
            }
            
            scheduled.push_back(next);
            
            // 依存する命令の入次数を減らし、実行可能になったものをキューに追加
            for (IRInstruction* dependent : dependencies[next]) {
                inDegrees[dependent]--;
                if (inDegrees[dependent] == 0) {
                    ready.push_back(dependent);
                }
            }
        }
        
        // スケジュールされた命令が元の命令と異なる場合、ブロックを更新
        if (scheduled.size() == block->instructions.size() && 
            !std::equal(scheduled.begin(), scheduled.end(), block->instructions.begin())) {
            
            block->instructions = scheduled;
            changed = true;
        }
    }
    
    return changed;
}

// 命令間のメモリ依存関係をチェック
bool InstructionSchedulingPass::hasMemoryDependency(IRInstruction* first, IRInstruction* second)
{
    if (!first || !second) {
        return false;
    }
    
    // 読み取り-書き込み、書き込み-読み取り、書き込み-書き込みの依存関係をチェック
    bool firstReads = isMemoryRead(first->opcode);
    bool firstWrites = isMemoryWrite(first->opcode);
    bool secondReads = isMemoryRead(second->opcode);
    bool secondWrites = isMemoryWrite(second->opcode);
    
    // 両方とも読み取りのみなら依存関係なし
    if (firstReads && !firstWrites && secondReads && !secondWrites) {
        return false;
    }
    
    // それ以外のメモリアクセスパターンは保守的に依存関係ありとする
    // 実際の実装ではより詳細なエイリアス解析が必要
    if ((firstReads || firstWrites) && (secondReads || secondWrites)) {
        return true;
    }
    
    return false;
}

// 命令間の制御依存関係をチェック
bool InstructionSchedulingPass::hasControlDependency(IRInstruction* first, IRInstruction* second)
{
    if (!first || !second) {
        return false;
    }
    
    // 制御フロー命令は後続の命令に依存している
    if (isControlFlowInstruction(first->opcode)) {
        return true;
    }
    
    // 副作用のある命令は保守的に制御依存関係ありとする
    if (hasSideEffects(first->opcode) || hasSideEffects(second->opcode)) {
        return true;
    }
    
    return false;
}

// 命令がメモリ読み取りかどうかを判定
bool InstructionSchedulingPass::isMemoryRead(IROpcode opcode)
{
    switch (opcode) {
        case IROpcode::LoadGlobal:
        case IROpcode::LoadLocal:
        case IROpcode::LoadArg:
        case IROpcode::LoadProperty:
        case IROpcode::LoadElement:
            return true;
        default:
            return false;
    }
}

// 命令がメモリ書き込みかどうかを判定
bool InstructionSchedulingPass::isMemoryWrite(IROpcode opcode)
{
    switch (opcode) {
        case IROpcode::StoreGlobal:
        case IROpcode::StoreLocal:
        case IROpcode::StoreArg:
        case IROpcode::StoreProperty:
        case IROpcode::StoreElement:
            return true;
        default:
            return false;
    }
}

// 命令が制御フロー命令かどうかを判定
bool InstructionSchedulingPass::isControlFlowInstruction(IROpcode opcode)
{
    switch (opcode) {
        case IROpcode::Jump:
        case IROpcode::Branch:
        case IROpcode::Return:
        case IROpcode::Throw:
        case IROpcode::Call:
        case IROpcode::TailCall:
            return true;
        default:
            return false;
    }
}

// 命令が副作用を持つかどうかを判定
bool InstructionSchedulingPass::hasSideEffects(IROpcode opcode)
{
    switch (opcode) {
        case IROpcode::Call:
        case IROpcode::TailCall:
        case IROpcode::Throw:
        case IROpcode::StoreGlobal:
        case IROpcode::StoreProperty:
        case IROpcode::StoreElement:
        case IROpcode::CreateObject:
        case IROpcode::CreateArray:
        case IROpcode::CreateFunction:
            return true;
        default:
            return false;
    }
}

// レイテンシテーブル（各命令タイプの実行時間の概算）
std::unordered_map<IROpcode, int> InstructionSchedulingPass::getLatencyTable()
{
    std::unordered_map<IROpcode, int> latencies;
    
    // 基本算術演算
    latencies[IROpcode::Add] = 1;
    latencies[IROpcode::Sub] = 1;
    latencies[IROpcode::Neg] = 1;
    latencies[IROpcode::Inc] = 1;
    latencies[IROpcode::Dec] = 1;
    
    // 乗算は加算より遅い
    latencies[IROpcode::Mul] = 3;
    
    // 除算は非常に遅い
    latencies[IROpcode::Div] = 10;
    latencies[IROpcode::Mod] = 10;
    
    // ビット操作
    latencies[IROpcode::BitAnd] = 1;
    latencies[IROpcode::BitOr] = 1;
    latencies[IROpcode::BitXor] = 1;
    latencies[IROpcode::BitNot] = 1;
    latencies[IROpcode::LeftShift] = 1;
    latencies[IROpcode::RightShift] = 1;
    latencies[IROpcode::UnsignedRightShift] = 1;
    
    // 論理演算
    latencies[IROpcode::LogicalAnd] = 1;
    latencies[IROpcode::LogicalOr] = 1;
    latencies[IROpcode::LogicalNot] = 1;
    
    // 比較演算
    latencies[IROpcode::Equal] = 1;
    latencies[IROpcode::NotEqual] = 1;
    latencies[IROpcode::StrictEqual] = 1;
    latencies[IROpcode::StrictNotEqual] = 1;
    latencies[IROpcode::LessThan] = 1;
    latencies[IROpcode::LessThanOrEqual] = 1;
    latencies[IROpcode::GreaterThan] = 1;
    latencies[IROpcode::GreaterThanOrEqual] = 1;
    
    // メモリ操作
    latencies[IROpcode::LoadConst] = 1;
    latencies[IROpcode::LoadLocal] = 1;
    latencies[IROpcode::StoreLocal] = 1;
    latencies[IROpcode::LoadArg] = 1;
    latencies[IROpcode::StoreArg] = 1;
    
    // グローバル変数アクセスは比較的遅い
    latencies[IROpcode::LoadGlobal] = 5;
    latencies[IROpcode::StoreGlobal] = 5;
    
    // プロパティアクセスは非常に遅い
    latencies[IROpcode::LoadProperty] = 8;
    latencies[IROpcode::StoreProperty] = 8;
    latencies[IROpcode::LoadElement] = 8;
    latencies[IROpcode::StoreElement] = 8;
    
    // オブジェクト生成
    latencies[IROpcode::CreateObject] = 10;
    latencies[IROpcode::CreateArray] = 10;
    latencies[IROpcode::CreateFunction] = 15;
    
    // 制御フロー
    latencies[IROpcode::Jump] = 1;
    latencies[IROpcode::Branch] = 2;
    latencies[IROpcode::Return] = 2;
    latencies[IROpcode::Throw] = 20;
    
    // 関数呼び出し
    latencies[IROpcode::Call] = 20;
    latencies[IROpcode::TailCall] = 15;
    
    // 型操作
    latencies[IROpcode::TypeOf] = 3;
    latencies[IROpcode::InstanceOf] = 5;
    
    // デフォルト値
    latencies[IROpcode::NoOp] = 0;
    latencies[IROpcode::Phi] = 0;
    
    return latencies;
}

// 命令の優先順位を計算（クリティカルパス解析）
int InstructionSchedulingPass::calculatePriority(IRInstruction* inst, 
                                                 const std::unordered_map<IRInstruction*, std::vector<IRInstruction*>>& dependencies,
                                                 const std::unordered_map<IROpcode, int>& latencies,
                                                 std::unordered_map<IRInstruction*, int>& memo)
{
    // メモ化で既に計算済みならその値を返す
    auto it = memo.find(inst);
    if (it != memo.end()) {
        return it->second;
    }
    
    // この命令のレイテンシを取得
    int latency = 1; // デフォルト値
    auto latIt = latencies.find(inst->opcode);
    if (latIt != latencies.end()) {
        latency = latIt->second;
    }
    
    // 依存関係がない場合は自身のレイテンシを返す
    auto depIt = dependencies.find(inst);
    if (depIt == dependencies.end() || depIt->second.empty()) {
        memo[inst] = latency;
        return latency;
    }
    
    // 依存する命令からの最大パス長を計算
    int maxDepLength = 0;
    for (IRInstruction* dep : depIt->second) {
        int depLength = calculatePriority(dep, dependencies, latencies, memo);
        maxDepLength = std::max(maxDepLength, depLength);
    }
    
    // この命令のレイテンシと依存命令からの最大パス長の和
    int priority = latency + maxDepLength;
    memo[inst] = priority;
    return priority;
}

// 最適な命令を選択
IRInstruction* InstructionSchedulingPass::selectBestInstruction(const std::vector<IRInstruction*>& ready,
                                                               const std::unordered_map<IRInstruction*, std::vector<IRInstruction*>>& dependencies,
                                                               IRFunction* function)
{
    if (ready.empty()) {
        return nullptr;
    }
    
    // サイズが1の場合はその命令を返す
    if (ready.size() == 1) {
        return ready[0];
    }
    
    // レイテンシテーブルを取得
    std::unordered_map<IROpcode, int> latencies = getLatencyTable();
    
    // 各命令の優先順位を計算するためのメモ化テーブル
    std::unordered_map<IRInstruction*, int> priorityMemo;
    
    // 各命令の優先順位を計算（クリティカルパス解析）
    std::unordered_map<IRInstruction*, int> priorities;
    for (IRInstruction* inst : ready) {
        priorities[inst] = calculatePriority(inst, dependencies, latencies, priorityMemo);
    }
    
    // 優先順位が最も高い命令を選択
    IRInstruction* best = ready[0];
    int highestPriority = priorities[best];
    
    for (size_t i = 1; i < ready.size(); ++i) {
        IRInstruction* inst = ready[i];
        int priority = priorities[inst];
        
        if (priority > highestPriority) {
            highestPriority = priority;
            best = inst;
        }
        else if (priority == highestPriority) {
            // 優先順位が同じ場合は、レイテンシの低い命令を優先
            int bestLatency = latencies[best->opcode];
            int instLatency = latencies[inst->opcode];
            
            if (instLatency < bestLatency) {
                best = inst;
            }
            // レイテンシも同じ場合は、元の順序を維持
        }
    }
    
    return best;
}

//===============================================================
// ループ不変コード移動最適化パス
//===============================================================
bool LoopInvariantCodeMotionPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // ループを識別（簡易実装）
    for (IRBlock* block : function->blocks) {
        if (block->isLoopHeader) {
            // ループヘッダーブロックを見つけた
            
            // ループ内のブロックを収集（簡易実装）
            std::unordered_set<IRBlock*> loopBlocks;
            std::vector<IRBlock*> workList = { block };
            
            while (!workList.empty()) {
                IRBlock* current = workList.back();
                workList.pop_back();
                
                if (loopBlocks.insert(current).second) {
                    // 新しいブロックを見つけた場合、後続ブロックを追加
                    for (IRBlock* succ : current->successors) {
                        // ループに属すると思われるブロックをワークリストに追加
                        // バックエッジでない場合はループの一部（簡易実装）
                        if (succ != block && loopBlocks.find(succ) == loopBlocks.end()) {
                            workList.push_back(succ);
                        }
                    }
                }
            }
            
            // プレヘッダーブロックを識別または作成
            IRBlock* preheader = nullptr;
            
            // 単一の非ループプレデセッサを探す
            for (IRBlock* pred : block->predecessors) {
                if (loopBlocks.find(pred) == loopBlocks.end()) {
                    if (!preheader) {
                        preheader = pred;
                    } else {
                        // 複数のプレデセッサがある場合、新しいプレヘッダーブロックを作成
                        IRBlock* newPreheader = new IRBlock(function->blocks.size());
                        function->addBlock(newPreheader);
                        
                        // すべての非ループ先行ブロックをプレヘッダーに接続
                        for (auto it = block->predecessors.begin(); it != block->predecessors.end();) {
                            IRBlock* pred = *it;
                            if (loopBlocks.find(pred) == loopBlocks.end()) {
                                // ループ外の先行ブロックを新しいプレヘッダーに接続し直す
                                for (size_t i = 0; i < pred->successors.size(); ++i) {
                                    if (pred->successors[i] == block) {
                                        pred->successors[i] = newPreheader;
                                    }
                                }
                                
                                // プレヘッダーをループヘッダーに接続
                                newPreheader->addSuccessor(block);
                                
                                // loopHeaderの先行ブロックリストを更新（ループ外の前任者を削除）
                                it = block->predecessors.erase(it);
                            } else {
                                ++it;
                            }
                        }
                        
                        // プレヘッダーをループヘッダーの先行ブロックとして追加
                        block->addPredecessor(newPreheader);
                        
                        // ジャンプ命令を追加してループヘッダーに接続
                        IRInstruction* jumpInst = new IRInstruction(IROpcode::Jump);
                        jumpInst->debugInfo = "Jump to loop header: " + std::to_string(block->id);
                        newPreheader->addInstruction(jumpInst);
                        
                        preheader = newPreheader;
                        break;
                    }
                }
            }
            
            if (!preheader) {
                continue; // プレヘッダーを作成できない場合はスキップ
            }
            
            // ループ内で定義される値のセット
            std::unordered_set<IRValue*> loopDefinedValues;
            
            // ループ内のすべての命令を収集して定義される値を識別
            for (IRBlock* loopBlock : loopBlocks) {
                for (IRInstruction* inst : loopBlock->instructions) {
                    if (inst->result) {
                        loopDefinedValues.insert(inst->result);
                    }
                }
            }
            
            // 各ブロックで不変な命令を特定して移動
            for (IRBlock* loopBlock : loopBlocks) {
                auto it = loopBlock->instructions.begin();
                while (it != loopBlock->instructions.end()) {
                    IRInstruction* inst = *it;
                    
                    // 命令が不変かどうかをチェック
                    bool isInvariant = true;
                    
                    // 副作用のある命令は不変とみなさない
                    if (inst->opcode == IROpcode::Call || 
                        inst->opcode == IROpcode::StoreGlobal || 
                        inst->opcode == IROpcode::StoreProperty ||
                        inst->opcode == IROpcode::StoreElement ||
                        inst->opcode == IROpcode::StoreLocal ||
                        inst->opcode == IROpcode::CreateObject ||
                        inst->opcode == IROpcode::CreateArray ||
                        inst->opcode == IROpcode::Throw) {
                        isInvariant = false;
                    }
                    
                    // 制御フロー命令は移動できない
                    if (inst->opcode == IROpcode::Jump || 
                        inst->opcode == IROpcode::Branch || 
                        inst->opcode == IROpcode::Return) {
                        isInvariant = false;
                    }
                    
                    // PHI命令はループ不変コード移動の対象外
                    if (inst->opcode == IROpcode::Phi) {
                        isInvariant = false;
                    }
                    
                    // すべてのオペランドがループ外で定義されているか確認
                    for (IRValue* operand : inst->operands) {
                        if (!operand) continue;
                        
                        // ループ内で定義された値を使用している場合は不変ではない
                        if (loopDefinedValues.find(operand) != loopDefinedValues.end()) {
                            isInvariant = false;
                            break;
                        }
                    }
                    
                    if (isInvariant && inst->result) {
                        // 命令をプレヘッダーに移動
                        
                        // プレヘッダーの最後（ただしジャンプ命令の前）に命令を挿入
                        if (preheader->instructions.empty() || 
                            preheader->instructions.back()->opcode != IROpcode::Jump) {
                            preheader->addInstruction(inst);
                        } else {
                            // ジャンプ命令の前に挿入
                            preheader->instructions.insert(preheader->instructions.end() - 1, inst);
                        }
                        
                        // ループ内で定義される値のセットを更新
                        loopDefinedValues.erase(inst->result);
                        
                        // ブロックから命令を削除
                        it = loopBlock->instructions.erase(it);
                        changed = true;
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
    
    return changed;
}

//===============================================================
// 命令組み合わせ最適化パス
//===============================================================
bool InstructionCombiningPass::run(IRFunction* function)
{
    if (!function) {
        return false;
    }

    bool changed = false;
    
    // 各ブロックの命令を処理
    for (IRBlock* block : function->blocks) {
        auto it = block->instructions.begin();
        while (it != block->instructions.end()) {
            IRInstruction* inst = *it;
            
            bool combined = false;
            
            // 特定のパターンをチェック
            if (inst->opcode == IROpcode::Add && inst->operands.size() == 2) {
                // パターン1: x + 0 = x
                if (inst->operands[1]->isConstant() && inst->operands[1]->debugInfo == "0") {
                    // x + 0 を x に置き換え
                    IRValue* result = inst->result;
                    IRValue* operand = inst->operands[0];
                    
                    // 元の命令のオペランドの参照カウントを減らす
                    for (IRValue* op : inst->operands) {
                        if (op) {
                            op->refCount--;
                        }
                    }
                    
                    // 新しい命令を作成（コピー命令）
                    inst->opcode = IROpcode::LoadVar;
                    inst->operands.clear();
                    inst->addOperand(operand);
                    inst->debugInfo = "Combined: x + 0 -> x";
                    
                    combined = true;
                    changed = true;
                }
                // パターン2: 0 + x = x
                else if (inst->operands[0]->isConstant() && inst->operands[0]->debugInfo == "0") {
                    // 0 + x を x に置き換え
                    IRValue* result = inst->result;
                    IRValue* operand = inst->operands[1];
                    
                    // 元の命令のオペランドの参照カウントを減らす
                    for (IRValue* op : inst->operands) {
                        if (op) {
                            op->refCount--;
                        }
                    }
                    
                    // 新しい命令を作成（コピー命令）
                    inst->opcode = IROpcode::LoadVar;
                    inst->operands.clear();
                    inst->addOperand(operand);
                    inst->debugInfo = "Combined: 0 + x -> x";
                    
                    combined = true;
                    changed = true;
                }
                // パターン3: x + x = x * 2
                else if (!inst->operands[0]->isConstant() && 
                         !inst->operands[1]->isConstant() && 
                         inst->operands[0]->id == inst->operands[1]->id) {
                    
                    // x + x を x * 2 に置き換え
                    IRValue* x = inst->operands[0];
                    
                    // 元の命令のオペランドの参照カウントを減らす
                    for (IRValue* op : inst->operands) {
                        if (op) {
                            op->refCount--;
                        }
                    }
                    
                    // 定数2を作成または既存の定数を検索
                    IRValue* constTwo = nullptr;
                    for (IRValue* value : function->values) {
                        if (value->isConstant() && value->type == IRType::Int32 && value->debugInfo == "2") {
                            constTwo = value;
                            break;
                        }
                    }
                    
                    if (!constTwo) {
                        // 既存の定数2が見つからなければ新しく作成
                        constTwo = new IRValue(function->values.size(), IRType::Int32);
                        constTwo->setFlag(IRValueFlags::Constant);
                        constTwo->debugInfo = "2";
                        function->addValue(constTwo);
                    }
                    
                    // 乗算命令に変更
                    inst->opcode = IROpcode::Mul;
                    inst->operands.clear();
                    inst->addOperand(x);
                    inst->addOperand(constTwo);
                    inst->debugInfo = "Combined: x + x -> x * 2";
                    
                    combined = true;
                    changed = true;
                }
                // パターン4: (x * c1) + (x * c2) = x * (c1 + c2)
                else {
                    // オペランドが両方とも乗算命令の結果であるかチェック
                    IRInstruction* mul1 = findDefiningInstruction(function, inst->operands[0]);
                    IRInstruction* mul2 = findDefiningInstruction(function, inst->operands[1]);
                    
                    if (mul1 && mul2 && 
                        mul1->opcode == IROpcode::Mul && mul2->opcode == IROpcode::Mul &&
                        mul1->operands.size() == 2 && mul2->operands.size() == 2) {
                        
                        // 共通の変数xを探す
                        IRValue* x = nullptr;
                        IRValue* c1 = nullptr;
                        IRValue* c2 = nullptr;
                        
                        // ケース1: (x * c1) + (x * c2)
                        if (mul1->operands[0]->id == mul2->operands[0]->id && 
                            mul1->operands[1]->isConstant() && mul2->operands[1]->isConstant()) {
                            x = mul1->operands[0];
                            c1 = mul1->operands[1];
                            c2 = mul2->operands[1];
                        }
                        // ケース2: (c1 * x) + (x * c2)
                        else if (mul1->operands[1]->id == mul2->operands[0]->id && 
                                 mul1->operands[0]->isConstant() && mul2->operands[1]->isConstant()) {
                            x = mul1->operands[1];
                            c1 = mul1->operands[0];
                            c2 = mul2->operands[1];
                        }
                        // ケース3: (x * c1) + (c2 * x)
                        else if (mul1->operands[0]->id == mul2->operands[1]->id && 
                                 mul1->operands[1]->isConstant() && mul2->operands[0]->isConstant()) {
                            x = mul1->operands[0];
                            c1 = mul1->operands[1];
                            c2 = mul2->operands[0];
                        }
                        // ケース4: (c1 * x) + (c2 * x)
                        else if (mul1->operands[1]->id == mul2->operands[1]->id && 
                                 mul1->operands[0]->isConstant() && mul2->operands[0]->isConstant()) {
                            x = mul1->operands[1];
                            c1 = mul1->operands[0];
                            c2 = mul2->operands[0];
                        }
                        
                        if (x && c1 && c2 && c1->type == c2->type) {
                            // 定数を加算
                            IRValue* sum = nullptr;
                            
                            if (c1->type == IRType::Int32) {
                                int32_t val1 = std::stoi(c1->debugInfo);
                                int32_t val2 = std::stoi(c2->debugInfo);
                                int32_t result = val1 + val2;
                                
                                // 結果が0の場合、x * 0 = 0 になるため別処理
                                if (result == 0) {
                                    // 定数0を作成または既存の定数を検索
                                    IRValue* constZero = nullptr;
                                    for (IRValue* value : function->values) {
                                        if (value->isConstant() && value->type == IRType::Int32 && value->debugInfo == "0") {
                                            constZero = value;
                                            break;
                                        }
                                    }
                                    
                                    if (!constZero) {
                                        constZero = new IRValue(function->values.size(), IRType::Int32);
                                        constZero->setFlag(IRValueFlags::Constant);
                                        constZero->debugInfo = "0";
                                        function->addValue(constZero);
                                    }
                                    
                                    // 元の命令のオペランドの参照カウントを減らす
                                    for (IRValue* op : inst->operands) {
                                        if (op) {
                                            op->refCount--;
                                        }
                                    }
                                    
                                    // 定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    inst->addOperand(constZero);
                                    inst->debugInfo = "Combined: (x * c1) + (x * c2) -> 0, where c1 + c2 = 0";
                                    
                                    combined = true;
                                    changed = true;
                                }
                                else {
                                    // 定数を作成または検索
                                    std::string resultStr = std::to_string(result);
                                    for (IRValue* value : function->values) {
                                        if (value->isConstant() && value->type == IRType::Int32 && value->debugInfo == resultStr) {
                                            sum = value;
                                            break;
                                        }
                                    }
                                    
                                    if (!sum) {
                                        sum = new IRValue(function->values.size(), IRType::Int32);
                                        sum->setFlag(IRValueFlags::Constant);
                                        sum->debugInfo = resultStr;
                                        function->addValue(sum);
                                    }
                                    
                                    // 元の命令のオペランドの参照カウントを減らす
                                    for (IRValue* op : inst->operands) {
                                        if (op) {
                                            op->refCount--;
                                        }
                                    }
                                    
                                    // 乗算命令に変更
                                    inst->opcode = IROpcode::Mul;
                                    inst->operands.clear();
                                    inst->addOperand(x);
                                    inst->addOperand(sum);
                                    inst->debugInfo = "Combined: (x * c1) + (x * c2) -> x * (c1 + c2)";
                                    
                                    combined = true;
                                    changed = true;
                                }
                            }
                            else if (c1->type == IRType::Float64) {
                                double val1 = std::stod(c1->debugInfo);
                                double val2 = std::stod(c2->debugInfo);
                                double result = val1 + val2;
                                
                                // 結果が0の場合、x * 0 = 0 になるため別処理
                                if (result == 0.0) {
                                    // 定数0を作成または既存の定数を検索
                                    IRValue* constZero = nullptr;
                                    for (IRValue* value : function->values) {
                                        if (value->isConstant() && value->type == IRType::Float64 && value->debugInfo == "0.0") {
                                            constZero = value;
                                            break;
                                        }
                                    }
                                    
                                    if (!constZero) {
                                        constZero = new IRValue(function->values.size(), IRType::Float64);
                                        constZero->setFlag(IRValueFlags::Constant);
                                        constZero->debugInfo = "0.0";
                                        function->addValue(constZero);
                                    }
                                    
                                    // 元の命令のオペランドの参照カウントを減らす
                                    for (IRValue* op : inst->operands) {
                                        if (op) {
                                            op->refCount--;
                                        }
                                    }
                                    
                                    // 定数ロードに変更
                                    inst->opcode = IROpcode::LoadConst;
                                    inst->operands.clear();
                                    inst->addOperand(constZero);
                                    inst->debugInfo = "Combined: (x * c1) + (x * c2) -> 0.0, where c1 + c2 = 0";
                                    
                                    combined = true;
                                    changed = true;
                                }
                                else {
                                    // 定数を作成または検索
                                    std::string resultStr = std::to_string(result);
                                    for (IRValue* value : function->values) {
                                        if (value->isConstant() && value->type == IRType::Float64 && value->debugInfo == resultStr) {
                                            sum = value;
                                            break;
                                        }
                                    }
                                    
                                    if (!sum) {
                                        sum = new IRValue(function->values.size(), IRType::Float64);
                                        sum->setFlag(IRValueFlags::Constant);
                                        sum->debugInfo = resultStr;
                                        function->addValue(sum);
                                    }
                                    
                                    // 元の命令のオペランドの参照カウントを減らす
                                    for (IRValue* op : inst->operands) {
                                        if (op) {
                                            op->refCount--;
                                        }
                                    }
                                    
                                    // 乗算命令に変更
                                    inst->opcode = IROpcode::Mul;
                                    inst->operands.clear();
                                    inst->addOperand(x);
                                    inst->addOperand(sum);
                                    inst->debugInfo = "Combined: (x * c1) + (x * c2) -> x * (c1 + c2)";
                                    
                                    combined = true;
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
            else if (inst->opcode == IROpcode::Mul && inst->operands.size() == 2) {
                // パターン5: x * 1 = x
                if (inst->operands[1]->isConstant()) {
                    if (inst->operands[1]->type == IRType::Int32 && inst->operands[1]->debugInfo == "1") {
                        // x * 1 を x に置き換え
                        IRValue* operand = inst->operands[0];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // コピー命令に変更
                        inst->opcode = IROpcode::LoadVar;
                        inst->operands.clear();
                        inst->addOperand(operand);
                        inst->debugInfo = "Combined: x * 1 -> x";
                        
                        combined = true;
                        changed = true;
                    }
                    // パターン6: x * 0 = 0
                    else if ((inst->operands[1]->type == IRType::Int32 && inst->operands[1]->debugInfo == "0") ||
                            (inst->operands[1]->type == IRType::Float64 && inst->operands[1]->debugInfo == "0.0")) {
                        // x * 0 を 0 に置き換え
                        IRValue* zero = inst->operands[1];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // 定数ロードに変更
                        inst->opcode = IROpcode::LoadConst;
                        inst->operands.clear();
                        inst->addOperand(zero);
                        inst->debugInfo = "Combined: x * 0 -> 0";
                        
                        combined = true;
                        changed = true;
                    }
                    // パターン7: x * 2 = x + x
                    else if (inst->operands[1]->type == IRType::Int32 && inst->operands[1]->debugInfo == "2") {
                        // この最適化は通常有効ではないため、特定の条件下でのみ適用（例：加算が乗算より効率的なアーキテクチャ向け）
                        // ここでは例として含めていますが、実際には多くの場合、x * 2はLEA命令（アドレス計算）で効率的に実装できるため、
                        // アーキテクチャに応じて選択すべきです
                    }
                }
                // パターン8: 1 * x = x
                else if (inst->operands[0]->isConstant()) {
                    if (inst->operands[0]->type == IRType::Int32 && inst->operands[0]->debugInfo == "1") {
                        // 1 * x を x に置き換え
                        IRValue* operand = inst->operands[1];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // コピー命令に変更
                        inst->opcode = IROpcode::LoadVar;
                        inst->operands.clear();
                        inst->addOperand(operand);
                        inst->debugInfo = "Combined: 1 * x -> x";
                        
                        combined = true;
                        changed = true;
                    }
                    // パターン9: 0 * x = 0
                    else if ((inst->operands[0]->type == IRType::Int32 && inst->operands[0]->debugInfo == "0") ||
                            (inst->operands[0]->type == IRType::Float64 && inst->operands[0]->debugInfo == "0.0")) {
                        // 0 * x を 0 に置き換え
                        IRValue* zero = inst->operands[0];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // 定数ロードに変更
                        inst->opcode = IROpcode::LoadConst;
                        inst->operands.clear();
                        inst->addOperand(zero);
                        inst->debugInfo = "Combined: 0 * x -> 0";
                        
                        combined = true;
                        changed = true;
                    }
                }
            }
            else if (inst->opcode == IROpcode::Sub && inst->operands.size() == 2) {
                // パターン10: x - 0 = x
                if (inst->operands[1]->isConstant()) {
                    if ((inst->operands[1]->type == IRType::Int32 && inst->operands[1]->debugInfo == "0") ||
                        (inst->operands[1]->type == IRType::Float64 && inst->operands[1]->debugInfo == "0.0")) {
                        // x - 0 を x に置き換え
                        IRValue* operand = inst->operands[0];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // コピー命令に変更
                        inst->opcode = IROpcode::LoadVar;
                        inst->operands.clear();
                        inst->addOperand(operand);
                        inst->debugInfo = "Combined: x - 0 -> x";
                        
                        combined = true;
                        changed = true;
                    }
                }
                // パターン11: x - x = 0
                else if (!inst->operands[0]->isConstant() && 
                        !inst->operands[1]->isConstant() && 
                        inst->operands[0]->id == inst->operands[1]->id) {
                    // x - x を 0 に置き換え
                    
                    // 適切な型の0定数を作成または既存の定数を検索
                    IRValue* zero = nullptr;
                    IRType zeroType = inst->operands[0]->type;
                    
                    if (zeroType == IRType::Float64) {
                        for (IRValue* value : function->values) {
                            if (value->isConstant() && value->type == zeroType && value->debugInfo == "0.0") {
                                zero = value;
                                break;
                            }
                        }
                        
                        if (!zero) {
                            zero = new IRValue(function->values.size(), zeroType);
                            zero->setFlag(IRValueFlags::Constant);
                            zero->debugInfo = "0.0";
                            function->addValue(zero);
                        }
                    }
                    else {
                        for (IRValue* value : function->values) {
                            if (value->isConstant() && value->type == IRType::Int32 && value->debugInfo == "0") {
                                zero = value;
                                break;
                            }
                        }
                        
                        if (!zero) {
                            zero = new IRValue(function->values.size(), IRType::Int32);
                            zero->setFlag(IRValueFlags::Constant);
                            zero->debugInfo = "0";
                            function->addValue(zero);
                        }
                    }
                    
                    // 元の命令のオペランドの参照カウントを減らす
                    for (IRValue* op : inst->operands) {
                        if (op) {
                            op->refCount--;
                        }
                    }
                    
                    // 定数ロードに変更
                    inst->opcode = IROpcode::LoadConst;
                    inst->operands.clear();
                    inst->addOperand(zero);
                    inst->debugInfo = "Combined: x - x -> 0";
                    
                    combined = true;
                    changed = true;
                }
            }
            else if (inst->opcode == IROpcode::Div && inst->operands.size() == 2) {
                // パターン12: x / 1 = x
                if (inst->operands[1]->isConstant()) {
                    if ((inst->operands[1]->type == IRType::Int32 && inst->operands[1]->debugInfo == "1") ||
                        (inst->operands[1]->type == IRType::Float64 && inst->operands[1]->debugInfo == "1.0")) {
                        // x / 1 を x に置き換え
                        IRValue* operand = inst->operands[0];
                        
                        // 元の命令のオペランドの参照カウントを減らす
                        for (IRValue* op : inst->operands) {
                            if (op) {
                                op->refCount--;
                            }
                        }
                        
                        // コピー命令に変更
                        inst->opcode = IROpcode::LoadVar;
                        inst->operands.clear();
                        inst->addOperand(operand);
                        inst->debugInfo = "Combined: x / 1 -> x";
                        
                        combined = true;
                        changed = true;
                    }
                }
                // パターン13: x / x = 1 (x != 0)
                else if (!inst->operands[0]->isConstant() && 
                        !inst->operands[1]->isConstant() && 
                        inst->operands[0]->id == inst->operands[1]->id) {
                    // この最適化はゼロ除算の可能性があるため、特別な条件下でのみ適用すべき
                    // ここでは例として含めていますが、実際には条件付きの最適化が必要
                }
            }
            
            // 他のパターンも同様に実装
            
            ++it;
        }
    }
    
    return changed;
}

// 値を定義している命令を探す
IRInstruction* InstructionCombiningPass::findDefiningInstruction(IRFunction* function, IRValue* value)
{
    if (!value || !function) {
        return nullptr;
    }
    
    // すべてのブロックの命令から探す
    for (IRBlock* block : function->blocks) {
        for (IRInstruction* inst : block->instructions) {
            if (inst->result && inst->result->id == value->id) {
                return inst;
            }
        }
    }
    
    return nullptr;
}

//===============================================================
// IR最適化器
//===============================================================
IROptimizer::IROptimizer(Context* context, JITProfiler* profiler)
    : m_context(context)
    , m_profiler(profiler)
    , m_level(OptimizationLevel::O2)
{
    // デフォルトの最適化パスを追加
    addDefaultPasses();
}

IROptimizer::~IROptimizer()
{
    // クリーンアップ処理
}

void IROptimizer::setOptimizationLevel(OptimizationLevel level)
{
    if (m_level != level) {
        m_level = level;
        // 最適化レベルに基づいてパスを再設定
        setupPassesForLevel();
    }
}

bool IROptimizer::optimize(IRFunction* function)
{
    if (!function) {
        return false;
    }
    
    bool changed = false;
    
    // プロファイル情報の取得と設定
    if (m_profiler) {
        uint64_t functionId = function->functionId;
        
        // プロファイル情報を取得
        auto profileData = m_profiler->getFunctionProfile(functionId);
        
        // プロファイル情報に基づいて最適化ヒントを設定
        for (IRBlock* block : function->blocks) {
            for (IRInstruction* inst : block->instructions) {
                // 命令のバイトコードオフセットに対応するプロファイル情報を取得
                uint32_t offset = inst->bytecodeOffset;
                auto instructionProfile = m_profiler->getInstructionProfile(functionId, offset);
                
                // 型情報が利用可能な場合、型ヒントを設定
                if (instructionProfile.hasTypeInfo) {
                    if (inst->result) {
                        // プロファイル情報に基づいて型を特定
                        IRType inferredType = mapProfileTypeToIRType(instructionProfile.observedType);
                        
                        // 推論された型をIR値に設定
                        if (inferredType != IRType::Any) {
                            inst->result->type = inferredType;
                            inst->result->setFlag(IRValueFlags::Reusable);
                        }
                    }
                }
                
                // 実行頻度が高い場合、命令に重要フラグを設定
                if (instructionProfile.executionCount > 1000) {
                    // ホットパスとして識別
                    inst->debugInfo += " [hot]";
                }
                
                // 例外発生履歴がある場合、チェックフラグを設定
                if (instructionProfile.exceptionCount > 0) {
                    inst->result->setFlag(IRValueFlags::MustCheck);
                }
            }
            
            // ブロックの実行回数を取得（存在する場合）
            auto blockProfile = m_profiler->getBlockProfile(functionId, block->id);
            
            // ホットブロックとして識別
            if (blockProfile.executionCount > 500) {
                // ループヘッダーの場合、ループ最適化の優先度を上げる
                if (block->isLoopHeader) {
                    // ホットループとしてマーク
                    block->debugInfo = "HotLoop: executed " + std::to_string(blockProfile.executionCount) + " times";
                }
            }
        }
    }
    
    // 最適化パスの実行
    for (auto& pass : m_passes) {
        const std::string& passName = pass->getName();
        
        // パスが有効かどうかを確認
        auto enabledIt = m_enabledPasses.find(passName);
        if (enabledIt != m_enabledPasses.end() && !enabledIt->second) {
            // パスが明示的に無効化されている場合はスキップ
            continue;
        }
        
        // パス実行時間の計測
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // パスを実行
        bool passChanged = pass->run(function);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        // 統計情報を更新
        auto& stats = m_stats[passName];
        stats.passExecutionCount++;
        if (passChanged) {
            stats.optimizationCount++;
            changed = true;
        }
        
        // ログに記録（デバッグ用）
        std::cout << "Pass: " << passName 
                  << ", Changed: " << (passChanged ? "yes" : "no") 
                  << ", Time: " << duration.count() << "µs" << std::endl;
    }
    
    return changed;
}

void IROptimizer::addPass(std::unique_ptr<OptimizationPass> pass)
{
    if (pass) {
        m_passes.push_back(std::move(pass));
    }
}

void IROptimizer::enablePass(const std::string& passName, bool enable)
{
    m_enabledPasses[passName] = enable;
}

std::string IROptimizer::dumpOptimizationInfo() const
{
    std::stringstream ss;
    
    ss << "=== Optimization Statistics ===" << std::endl;
    ss << "Optimization Level: ";
    
    switch (m_level) {
        case OptimizationLevel::O0:
            ss << "O0 (No Optimization)";
            break;
        case OptimizationLevel::O1:
            ss << "O1 (Basic Optimization)";
            break;
        case OptimizationLevel::O2:
            ss << "O2 (Standard Optimization)";
            break;
        case OptimizationLevel::O3:
            ss << "O3 (Aggressive Optimization)";
            break;
    }
    ss << std::endl << std::endl;
    
    ss << "Passes:" << std::endl;
    for (const auto& pass : m_passes) {
        const std::string& name = pass->getName();
        ss << "  " << name;
        
        auto enabledIt = m_enabledPasses.find(name);
        if (enabledIt != m_enabledPasses.end() && !enabledIt->second) {
            ss << " [DISABLED]";
        }
        
        auto statsIt = m_stats.find(name);
        if (statsIt != m_stats.end()) {
            const auto& stats = statsIt->second;
            ss << " - Executions: " << stats.passExecutionCount
               << ", Optimizations: " << stats.optimizationCount;
            
            if (stats.passExecutionCount > 0) {
                double successRate = 100.0 * stats.optimizationCount / stats.passExecutionCount;
                ss << " (" << std::fixed << std::setprecision(1) << successRate << "%)";
            }
        }
        
        ss << std::endl;
    }
    
    return ss.str();
}

void IROptimizer::setupPassesForLevel()
{
    // 古いパスをクリア
    m_passes.clear();
    
    // 最適化レベルに基づいてパスを追加
    switch (m_level) {
        case OptimizationLevel::O0:
            // 最適化なし
            break;
            
        case OptimizationLevel::O1:
            // 基本的な最適化のみ
            addPass(std::make_unique<ConstantFoldingPass>());
            addPass(std::make_unique<DeadCodeEliminationPass>());
            break;
            
        case OptimizationLevel::O2:
            // 標準的な最適化（デフォルト）
            addPass(std::make_unique<ConstantFoldingPass>());
            addPass(std::make_unique<CommonSubexpressionEliminationPass>());
            addPass(std::make_unique<DeadCodeEliminationPass>());
            addPass(std::make_unique<InstructionCombiningPass>());
            break;
            
        case OptimizationLevel::O3:
            // 積極的な最適化
            addPass(std::make_unique<ConstantFoldingPass>());
            addPass(std::make_unique<CommonSubexpressionEliminationPass>());
            addPass(std::make_unique<DeadCodeEliminationPass>());
            addPass(std::make_unique<InstructionCombiningPass>());
            addPass(std::make_unique<LoopInvariantCodeMotionPass>());
            addPass(std::make_unique<InstructionSchedulingPass>());
            break;
    }
}

void IROptimizer::addDefaultPasses()
{
    // デフォルトの最適化レベルに基づいてパスを設定
    setupPassesForLevel();
}

// プロファイルタイプをIRタイプに変換
IRType IROptimizer::mapProfileTypeToIRType(ProfiledValueType profileType)
{
    switch (profileType) {
        case ProfiledValueType::Undefined:
            return IRType::Any;
        case ProfiledValueType::Null:
            return IRType::Any;
        case ProfiledValueType::Boolean:
            return IRType::Boolean;
        case ProfiledValueType::Int32:
            return IRType::Int32;
        case ProfiledValueType::Int64:
            return IRType::Int64;
        case ProfiledValueType::Float:
            return IRType::Float64;
        case ProfiledValueType::String:
            return IRType::String;
        case ProfiledValueType::Object:
            return IRType::Object;
        case ProfiledValueType::Array:
            return IRType::Array;
        case ProfiledValueType::Function:
            return IRType::Function;
        default:
            return IRType::Any;
    }
}

} // namespace core
} // namespace aerojs 