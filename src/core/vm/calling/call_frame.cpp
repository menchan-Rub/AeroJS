/**
 * @file call_frame.cpp
 * @brief 仮想マシンのコールフレーム実装
 * 
 * JavaScriptエンジンのVM実行におけるコールフレームの実装。
 */

#include "call_frame.h"
#include "../interpreter/bytecode.h"
#include "../../runtime/context.h"
#include "../../runtime/scope.h"
#include "../../runtime/values/function.h"
#include <sstream>
#include <stdexcept>

namespace aerojs {
namespace core {

std::shared_ptr<CallFrame> CallFrame::createGlobalFrame(
    std::shared_ptr<Context> context, 
    BytecodeBlockPtr bytecodeBlock) {
  
  // グローバルスコープを取得
  ScopePtr globalScope = context->getGlobalScope();
  
  // thisはグローバルオブジェクト
  ValuePtr globalObject = context->getGlobalObject();
  
  // グローバルフレームを作成
  auto frame = std::make_shared<CallFrame>(
      context, 
      FrameType::kGlobal, 
      bytecodeBlock, 
      globalScope, 
      globalObject);
  
  // グローバルコードは通常のスクリプトでは非厳格モード
  // ただし、モジュールの場合は厳格モード
  frame->m_strictMode = bytecodeBlock->isStrictMode();
  
  return frame;
}

std::shared_ptr<CallFrame> CallFrame::createFunctionFrame(
    std::shared_ptr<Context> context,
    FunctionPtr function,
    ValuePtr thisValue,
    const std::vector<ValuePtr>& args,
    std::shared_ptr<CallFrame> parentFrame) {
  
  // 関数のバイトコードブロックを取得
  BytecodeBlockPtr bytecodeBlock = function->getBytecodeBlock();
  if (!bytecodeBlock) {
    throw std::runtime_error("関数にバイトコードがありません");
  }
  
  // 関数のスコープを取得
  ScopePtr functionScope = function->createCallScope(args);
  
  // 関数フレームを作成
  auto frame = std::make_shared<CallFrame>(
      context, 
      FrameType::kFunction, 
      bytecodeBlock, 
      functionScope, 
      thisValue);
  
  // 引数を設定
  frame->m_arguments = args;
  
  // ローカル変数の領域を確保
  frame->m_localVariables.resize(bytecodeBlock->getLocalVariableCount());
  
  // 関数の厳格モードを設定
  frame->m_strictMode = bytecodeBlock->isStrictMode() || 
                       (parentFrame && parentFrame->isStrictMode());
  
  // 親フレームを設定
  frame->m_parentFrame = parentFrame;
  
  return frame;
}

std::shared_ptr<CallFrame> CallFrame::createEvalFrame(
    std::shared_ptr<Context> context,
    BytecodeBlockPtr bytecodeBlock,
    std::shared_ptr<CallFrame> parentFrame,
    bool isDirectEval) {
  
  // スコープを決定
  // 直接evalの場合は呼び出し元のスコープを使用
  // 間接evalの場合はグローバルスコープを使用
  ScopePtr evalScope;
  if (isDirectEval && parentFrame) {
    evalScope = parentFrame->getScope();
  } else {
    evalScope = context->getGlobalScope();
  }
  
  // thisを決定
  // 直接evalの場合は呼び出し元のthisを使用
  // 間接evalの場合はグローバルオブジェクトを使用
  ValuePtr thisValue;
  if (isDirectEval && parentFrame) {
    thisValue = parentFrame->getThisValue();
  } else {
    thisValue = context->getGlobalObject();
  }
  
  // evalフレームを作成
  auto frame = std::make_shared<CallFrame>(
      context, 
      FrameType::kEval, 
      bytecodeBlock, 
      evalScope, 
      thisValue);
  
  // ローカル変数の領域を確保
  frame->m_localVariables.resize(bytecodeBlock->getLocalVariableCount());
  
  // 厳格モードを設定
  // evalコード自体が厳格モードか、親フレームが厳格モードの場合に厳格モードになる
  frame->m_strictMode = bytecodeBlock->isStrictMode() || 
                       (parentFrame && parentFrame->isStrictMode());
  
  // 親フレームを設定
  frame->m_parentFrame = parentFrame;
  
  return frame;
}

std::shared_ptr<CallFrame> CallFrame::createModuleFrame(
    std::shared_ptr<Context> context,
    BytecodeBlockPtr bytecodeBlock,
    ValuePtr moduleNamespace) {
  
  // モジュールスコープを作成
  ScopePtr moduleScope = context->createModuleScope(moduleNamespace);
  
  // モジュールフレームを作成
  auto frame = std::make_shared<CallFrame>(
      context, 
      FrameType::kModule, 
      bytecodeBlock, 
      moduleScope, 
      moduleNamespace);
  
  // ローカル変数の領域を確保
  frame->m_localVariables.resize(bytecodeBlock->getLocalVariableCount());
  
  // モジュールは常に厳格モード
  frame->m_strictMode = true;
  
  return frame;
}

std::shared_ptr<CallFrame> CallFrame::createNativeFrame(
    std::shared_ptr<Context> context,
    ValuePtr nativeFunction,
    ValuePtr thisValue,
    const std::vector<ValuePtr>& args,
    std::shared_ptr<CallFrame> parentFrame) {
  
  // ネイティブ関数にはバイトコードがないため、nullを設定
  BytecodeBlockPtr bytecodeBlock = nullptr;
  
  // グローバルスコープを使用
  ScopePtr scope = context->getGlobalScope();
  
  // ネイティブフレームを作成
  auto frame = std::make_shared<CallFrame>(
      context, 
      FrameType::kNative, 
      bytecodeBlock, 
      scope, 
      thisValue);
  
  // 引数を設定
  frame->m_arguments = args;
  
  // 親フレームから厳格モードを継承
  frame->m_strictMode = parentFrame ? parentFrame->isStrictMode() : false;
  
  // 親フレームを設定
  frame->m_parentFrame = parentFrame;
  
  return frame;
}

CallFrame::CallFrame(
    std::shared_ptr<Context> context,
    FrameType type,
    BytecodeBlockPtr bytecodeBlock,
    ScopePtr scope,
    ValuePtr thisValue)
    : m_context(context),
      m_type(type),
      m_bytecodeBlock(bytecodeBlock),
      m_scope(scope),
      m_thisValue(thisValue),
      m_instructionPointer(0),
      m_state(FrameState::kActive),
      m_strictMode(false),
      m_jitCode(nullptr) {
  
  // 初期化されていないメンバは初期値で設定済み
}

CallFrame::~CallFrame() {
  // 明示的にリソースを解放する必要はない
  // 全てスマートポインタで管理されている
}

FrameType CallFrame::getType() const {
  return m_type;
}

std::shared_ptr<Context> CallFrame::getContext() const {
  return m_context;
}

BytecodeBlockPtr CallFrame::getBytecodeBlock() const {
  return m_bytecodeBlock;
}

ScopePtr CallFrame::getScope() const {
  return m_scope;
}

ValuePtr CallFrame::getThisValue() const {
  return m_thisValue;
}

size_t CallFrame::getInstructionPointer() const {
  return m_instructionPointer;
}

void CallFrame::setInstructionPointer(size_t ip) {
  if (m_bytecodeBlock && ip < m_bytecodeBlock->getInstructionCount()) {
    m_instructionPointer = ip;
  } else {
    throw std::out_of_range("命令ポインタが範囲外です");
  }
}

bool CallFrame::advanceToNextInstruction() {
  if (!m_bytecodeBlock) {
    return false;  // バイトコードがない場合
  }
  
  m_instructionPointer++;
  return m_instructionPointer < m_bytecodeBlock->getInstructionCount();
}

const BytecodeInstruction& CallFrame::getCurrentInstruction() const {
  if (!m_bytecodeBlock) {
    throw std::runtime_error("バイトコードがありません");
  }
  
  if (m_instructionPointer >= m_bytecodeBlock->getInstructionCount()) {
    throw std::out_of_range("命令ポインタが範囲外です");
  }
  
  return m_bytecodeBlock->getInstruction(m_instructionPointer);
}

const std::vector<ValuePtr>& CallFrame::getArguments() const {
  return m_arguments;
}

ValuePtr CallFrame::getLocalVariable(size_t index) const {
  if (index >= m_localVariables.size()) {
    throw std::out_of_range("ローカル変数のインデックスが範囲外です");
  }
  
  return m_localVariables[index];
}

void CallFrame::setLocalVariable(size_t index, ValuePtr value) {
  if (index >= m_localVariables.size()) {
    // 必要に応じてローカル変数の領域を拡張
    m_localVariables.resize(index + 1);
  }
  
  m_localVariables[index] = value;
}

ValuePtr CallFrame::getScopeVariable(const std::string& name) {
  if (!m_scope) {
    throw std::runtime_error("スコープがありません");
  }
  
  return m_scope->getVariable(name);
}

bool CallFrame::setScopeVariable(const std::string& name, ValuePtr value) {
  if (!m_scope) {
    throw std::runtime_error("スコープがありません");
  }
  
  return m_scope->setVariable(name, value);
}

FrameState CallFrame::getState() const {
  return m_state;
}

void CallFrame::setState(FrameState state) {
  m_state = state;
}

void CallFrame::setReturnValue(ValuePtr value) {
  m_returnValue = value;
}

ValuePtr CallFrame::getReturnValue() const {
  return m_returnValue;
}

std::tuple<std::string, int, int> CallFrame::getSourcePosition() const {
  if (!m_bytecodeBlock) {
    return std::make_tuple("", 0, 0);  // バイトコードがない場合
  }
  
  // 現在の命令の位置情報を取得
  if (m_instructionPointer < m_bytecodeBlock->getInstructionCount()) {
    const auto& instruction = m_bytecodeBlock->getInstruction(m_instructionPointer);
    return instruction.getSourcePosition();
  }
  
  // バイトコードブロック全体の位置情報を返す
  return m_bytecodeBlock->getSourcePosition();
}

void CallFrame::setJITCompiledCode(std::shared_ptr<JITCompiledCode> compiledCode) {
  m_jitCode = compiledCode;
}

std::shared_ptr<JITCompiledCode> CallFrame::getJITCompiledCode() const {
  return m_jitCode;
}

void CallFrame::setParentFrame(std::shared_ptr<CallFrame> parentFrame) {
  m_parentFrame = parentFrame;
}

std::shared_ptr<CallFrame> CallFrame::getParentFrame() const {
  return m_parentFrame;
}

bool CallFrame::isStrictMode() const {
  return m_strictMode;
}

std::string CallFrame::toString() const {
  std::ostringstream oss;
  
  // フレームタイプを文字列に変換
  std::string typeStr;
  switch (m_type) {
    case FrameType::kGlobal:    typeStr = "グローバル"; break;
    case FrameType::kFunction:  typeStr = "関数"; break;
    case FrameType::kEval:      typeStr = "eval"; break;
    case FrameType::kModule:    typeStr = "モジュール"; break;
    case FrameType::kNative:    typeStr = "ネイティブ"; break;
    case FrameType::kGenerator: typeStr = "ジェネレータ"; break;
    case FrameType::kAsync:     typeStr = "非同期"; break;
    case FrameType::kDebugger:  typeStr = "デバッガ"; break;
    default:                    typeStr = "不明"; break;
  }
  
  // ソース位置情報を取得
  auto [filename, line, column] = getSourcePosition();
  
  // 文字列を構築
  oss << typeStr << "フレーム";
  
  // ファイル情報が存在する場合
  if (!filename.empty()) {
    oss << " [" << filename << ":" << line << ":" << column << "]";
  }
  
  // 厳格モード情報
  if (m_strictMode) {
    oss << " (strictモード)";
  }
  
  return oss.str();
}

} // namespace core
} // namespace aerojs 