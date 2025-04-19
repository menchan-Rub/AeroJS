/**
 * @file vm.cpp
 * @brief 仮想マシン実装
 * 
 * JavaScriptエンジンの仮想マシン(VM)の実装。
 * バイトコード実行、JIT最適化、メモリ管理を担当します。
 */

#include "vm.h"
#include "stack/stack.h"
#include "calling/call_frame.h"
#include "exception/exception.h"
#include "interpreter/interpreter.h"
#include "jit/jit_compiler.h"
#include "memory/heap.h"
#include "memory/garbage_collector.h"
#include "../runtime/context.h"
#include "../runtime/global_object.h"
#include "../runtime/values/value.h"
#include "../runtime/values/function.h"
#include "../runtime/values/native_function.h"
#include "../runtime/values/error.h"
#include "../parser/parser.h"
#include "../compiler/compiler.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <thread>

namespace aerojs {
namespace core {

// プロファイリング用のタイマークラス
class ScopedTimer {
public:
  ScopedTimer(std::string name, std::chrono::microseconds& result)
    : m_name(std::move(name)), m_result(result), m_start(std::chrono::high_resolution_clock::now()) {}
  
  ~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    m_result = std::chrono::duration_cast<std::chrono::microseconds>(end - m_start);
  }

private:
  std::string m_name;
  std::chrono::microseconds& m_result;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

VM::VM() : VM(VMOptions()) {}

VM::VM(const VMOptions& options) 
  : m_executionMode(ExecutionMode::kInterpreter), 
    m_currentException(nullptr),
    m_isDebuggerAttached(false),
    m_executedInstructionCount(0),
    m_gcCycleCount(0) {
  initialize(options);
}

VM::~VM() {
  // 循環参照を防ぐために明示的にクリーンアップ
  m_callStack.clear();
  m_heap->shutdown();
  m_heap.reset();
  m_interpreter.reset();
  m_jitCompiler.reset();
}

// ムーブコンストラクタ
VM::VM(VM&& other) noexcept 
  : m_options(std::move(other.m_options)),
    m_heap(std::move(other.m_heap)),
    m_interpreter(std::move(other.m_interpreter)),
    m_jitCompiler(std::move(other.m_jitCompiler)),
    m_callStack(std::move(other.m_callStack)),
    m_executionMode(other.m_executionMode.load()),
    m_currentException(std::move(other.m_currentException)),
    m_isDebuggerAttached(other.m_isDebuggerAttached),
    m_executedInstructionCount(other.m_executedInstructionCount.load()),
    m_gcCycleCount(other.m_gcCycleCount.load()) {
}

// ムーブ代入演算子
VM& VM::operator=(VM&& other) noexcept {
  if (this != &other) {
    m_options = std::move(other.m_options);
    m_heap = std::move(other.m_heap);
    m_interpreter = std::move(other.m_interpreter);
    m_jitCompiler = std::move(other.m_jitCompiler);
    m_callStack = std::move(other.m_callStack);
    m_executionMode.store(other.m_executionMode.load());
    m_currentException = std::move(other.m_currentException);
    m_isDebuggerAttached = other.m_isDebuggerAttached;
    m_executedInstructionCount.store(other.m_executedInstructionCount.load());
    m_gcCycleCount.store(other.m_gcCycleCount.load());
  }
  return *this;
}

void VM::initialize(const VMOptions& options) {
  m_options = options;
  
  // ヒープの初期化
  m_heap = std::make_shared<Heap>(options.initialHeapSize, options.maxHeapSize);
  
  // インタープリタの初期化
  m_interpreter = std::make_shared<Interpreter>(shared_from_this());
  
  // JITコンパイラの初期化（有効な場合）
  if (options.enableJIT) {
    m_jitCompiler = std::make_shared<JITCompiler>(shared_from_this(), options.inlineThreshold, 
                                               options.optimizationThreshold, options.reoptimizationThreshold);
  }
  
  // 初期実行モードの設定
  m_executionMode = options.enableJIT ? ExecutionMode::kBaselineJIT : ExecutionMode::kInterpreter;
  
  // 統計情報の初期化
  m_executedInstructionCount = 0;
  m_gcCycleCount = 0;
  
  // デバッガ設定
  m_isDebuggerAttached = options.enableDebugger;
  if (m_isDebuggerAttached) {
    enableDebugger(); // デフォルトポートでデバッガを有効化
  }
}

std::shared_ptr<Context> VM::createContext(std::shared_ptr<GlobalObject> globalObject) {
  // グローバルオブジェクトが指定されていない場合は新しく作成
  if (!globalObject) {
    globalObject = std::make_shared<GlobalObject>(shared_from_this());
  }
  
  // 新しいコンテキストを作成して返す
  return std::make_shared<Context>(shared_from_this(), globalObject, m_options.strictMode);
}

ValuePtr VM::evaluate(std::shared_ptr<Context> context, 
                    const std::string& code, 
                    const std::string& sourceFile,
                    int startLine) {
  // パフォーマンス計測用
  std::chrono::microseconds parseTime;
  std::chrono::microseconds executeTime;
  
  try {
    // ソースコードをパースしてバイトコードに変換
    {
      ScopedTimer timer("Parse", parseTime);
      auto bytecodeBlock = context->parseAndCompile(code, sourceFile, startLine);
      
      // バイトコードを実行
      ScopedTimer executeTimer("Execute", executeTime);
      return execute(context, bytecodeBlock);
    }
  } catch (const std::exception& e) {
    // ネイティブ例外をJavaScript例外に変換
    auto exception = std::make_shared<Exception>("EvalError", e.what(), sourceFile, startLine);
    throwException(exception);
    return nullptr;
  }
}

ValuePtr VM::execute(std::shared_ptr<Context> context, 
                   std::shared_ptr<BytecodeBlock> bytecodeBlock) {
  // 現在の実行モードを取得
  ExecutionMode mode = m_executionMode.load();
  
  // 実行モードに応じた実行
  switch (mode) {
    case ExecutionMode::kInterpreter:
      // インタープリタモードで実行
      return m_interpreter->execute(context, bytecodeBlock);
      
    case ExecutionMode::kBaselineJIT:
    case ExecutionMode::kOptimizedJIT:
    case ExecutionMode::kUltraOptimizedJIT:
      if (m_options.enableJIT && m_jitCompiler) {
        // JITコンパイルを試みる
        auto compiledCode = m_jitCompiler->compile(bytecodeBlock, mode);
        if (compiledCode) {
          // JITコンパイルされたコードがある場合は実行
          return m_jitCompiler->execute(context, compiledCode);
        }
      }
      // JIT実行が失敗した場合はインタープリタにフォールバック
      return m_interpreter->execute(context, bytecodeBlock);
      
    default:
      // 未知の実行モードの場合はエラー
      throwException(createException("Unknown execution mode", "InternalError"));
      return nullptr;
  }
}

ValuePtr VM::callFunction(std::shared_ptr<Context> context,
                        ValuePtr function,
                        ValuePtr thisValue,
                        const std::vector<ValuePtr>& args) {
  if (!function || !function->isCallable()) {
    throwException(createException("Not a function", "TypeError"));
    return nullptr;
  }
  
  // コールフレームを作成
  auto frame = std::make_shared<CallFrame>(context, function, thisValue, args);
  
  // フレームスタックに追加
  pushFrame(frame);
  
  try {
    // 関数を実行
    ValuePtr result = function->call(thisValue, args);
    
    // フレームをポップ
    popFrame();
    
    return result;
  } catch (const std::exception& e) {
    // フレームをポップしてから例外を再スロー
    popFrame();
    throwException(createException(e.what(), "Error"));
    return nullptr;
  }
}

ValuePtr VM::registerNativeFunction(const std::string& name,
                                  std::function<ValuePtr(const std::vector<ValuePtr>&)> function,
                                  std::shared_ptr<Context> context) {
  // ネイティブ関数オブジェクトを作成
  auto nativeFunction = std::make_shared<NativeFunction>(name, std::move(function));
  
  // コンテキストが指定されている場合はグローバルオブジェクトに登録
  if (context) {
    context->getGlobalObject()->set(name, nativeFunction);
  }
  
  return nativeFunction;
}

void VM::collectGarbage() {
  if (!m_options.enableGC) {
    return;
  }
  
  // スレッドセーフにGCを実行
  std::lock_guard<std::mutex> lock(m_vmMutex);
  
  // 現在のコールスタックのルート参照を収集
  std::vector<ValuePtr> roots;
  for (const auto& frame : m_callStack) {
    frame->collectRoots(roots);
  }
  
  // GCを実行
  m_heap->collectGarbage(roots);
  
  // GCカウントを増やす
  m_gcCycleCount++;
}

std::string VM::getStatistics() const {
  std::stringstream ss;
  
  ss << "VM Statistics:\n";
  ss << "  Execution Mode: " << static_cast<int>(m_executionMode.load()) << "\n";
  ss << "  Executed Instructions: " << m_executedInstructionCount.load() << "\n";
  ss << "  GC Cycles: " << m_gcCycleCount.load() << "\n";
  ss << "  Current Heap Size: " << m_heap->getCurrentSize() << " bytes\n";
  ss << "  Max Heap Size: " << m_heap->getMaxSize() << " bytes\n";
  ss << "  Current Stack Depth: " << m_callStack.size() << "\n";
  ss << "  JIT Compiled Methods: " << (m_jitCompiler ? m_jitCompiler->getCompiledMethodCount() : 0) << "\n";
  
  return ss.str();
}

std::string VM::getStackTrace() const {
  std::stringstream ss;
  
  ss << "Stack Trace:\n";
  
  // コールスタックを逆順に出力
  for (auto it = m_callStack.rbegin(); it != m_callStack.rend(); ++it) {
    const auto& frame = *it;
    ss << "  at " << frame->getFunctionName() << " (";
    
    // ソースの位置情報がある場合は出力
    const auto& sourceInfo = frame->getSourceInfo();
    if (!sourceInfo.filename.empty()) {
      ss << sourceInfo.filename << ":" << sourceInfo.line << ":" << sourceInfo.column;
    } else {
      ss << "native";
    }
    
    ss << ")\n";
  }
  
  return ss.str();
}

void VM::setExecutionMode(ExecutionMode mode) {
  // JITが無効な場合はインタープリタモードのみ許可
  if (!m_options.enableJIT && mode != ExecutionMode::kInterpreter) {
    return;
  }
  
  m_executionMode.store(mode);
  
  // JITコンパイラに実行モードが変わったことを通知
  if (m_jitCompiler) {
    m_jitCompiler->notifyExecutionModeChanged(mode);
  }
}

ExecutionMode VM::getExecutionMode() const {
  return m_executionMode.load();
}

bool VM::enableDebugger(int port) {
  if (m_isDebuggerAttached) {
    return true; // すでに有効
  }
  
  try {
    // デバッガサーバーの初期化
    m_debugServer = std::make_unique<DebugServer>(*this, port);
    if (!m_debugServer->start()) {
      throw std::runtime_error("デバッグサーバーの起動に失敗しました");
    }
    
    // デバッグモードを有効化
    m_isDebuggerAttached = true;
    m_debugMode.store(true, std::memory_order_release);
    
    // デバッグイベントハンドラを登録
    registerDebugEventHandlers();
    
    LOG_INFO("デバッガが有効化されました (port: {})", port);
    return true;
  } catch (const std::exception& e) {
    LOG_ERROR("デバッガの起動に失敗: {}", e.what());
    m_debugServer.reset();
    m_isDebuggerAttached = false;
    m_debugMode.store(false, std::memory_order_release);
    return false;
  }
}

void VM::disableDebugger() {
  if (!m_isDebuggerAttached) {
    return;
  }
  
  try {
    // デバッグイベントハンドラの登録解除
    unregisterDebugEventHandlers();
    
    // デバッグサーバーの停止
    if (m_debugServer) {
      m_debugServer->stop();
      m_debugServer.reset();
    }
    
    // デバッグモードを無効化
    m_isDebuggerAttached = false;
    m_debugMode.store(false, std::memory_order_release);
    
    LOG_INFO("デバッガが無効化されました");
  } catch (const std::exception& e) {
    LOG_ERROR("デバッガの停止中にエラーが発生: {}", e.what());
  }
}

size_t VM::getHeapSize() const {
  return m_heap ? m_heap->getCurrentSize() : 0;
}

const VMOptions& VM::getOptions() const {
  return m_options;
}

ExceptionPtr VM::createException(const std::string& message, const std::string& errorType) {
  // 現在のコールスタックから位置情報を取得
  std::string fileName = "";
  int lineNumber = 0;
  int columnNumber = 0;
  
  if (!m_callStack.empty()) {
    auto currentFrame = m_callStack.back();
    const auto& sourceInfo = currentFrame->getSourceInfo();
    fileName = sourceInfo.filename;
    lineNumber = sourceInfo.line;
    columnNumber = sourceInfo.column;
  }
  
  // スタックトレース情報を収集
  std::vector<StackTraceEntry> stackTrace;
  stackTrace.reserve(m_callStack.size());
  
  for (auto it = m_callStack.rbegin(); it != m_callStack.rend(); ++it) {
    const auto& frame = *it;
    const auto& sourceInfo = frame->getSourceInfo();
    
    StackTraceEntry entry;
    entry.functionName = frame->getFunctionName();
    entry.fileName = sourceInfo.filename;
    entry.lineNumber = sourceInfo.line;
    entry.columnNumber = sourceInfo.column;
    entry.isNative = sourceInfo.filename.empty();
    
    stackTrace.push_back(std::move(entry));
  }
  
  // 例外オブジェクトを作成
  auto exception = std::make_shared<Exception>(
    errorType, 
    message, 
    fileName, 
    lineNumber, 
    columnNumber,
    std::move(stackTrace)
  );
  
  // デバッグモードの場合、例外情報をログに記録
  if (m_options.debugLogging) {
    LOG_DEBUG("例外が作成されました: {} - {}", errorType, message);
    LOG_DEBUG("場所: {}:{}:{}", fileName, lineNumber, columnNumber);
  }
  
  return exception;
}

void VM::pushFrame(std::shared_ptr<CallFrame> frame) {
  // スタックオーバーフローチェック
  if (m_callStack.size() >= m_options.maxStackSize / sizeof(CallFrame)) {
    throwException(createException("Maximum call stack size exceeded", "RangeError"));
    return;
  }
  
  // パフォーマンスメトリクスの更新
  m_metrics.maxStackDepth = std::max(m_metrics.maxStackDepth, m_callStack.size() + 1);
  
  // デバッグモードの場合、フレーム情報をログに記録
  if (m_debugMode.load(std::memory_order_acquire)) {
    const auto& sourceInfo = frame->getSourceInfo();
    LOG_TRACE("フレーム追加: {} ({}:{}:{})", 
              frame->getFunctionName(),
              sourceInfo.filename.empty() ? "native" : sourceInfo.filename,
              sourceInfo.line,
              sourceInfo.column);
    
    // デバッガにフレーム追加イベントを通知
    if (m_debugServer && m_isDebuggerAttached) {
      m_debugServer->notifyFramePush(frame);
    }
  }
  
  m_callStack.push_back(std::move(frame));
}

std::shared_ptr<CallFrame> VM::popFrame() {
  if (m_callStack.empty()) {
    LOG_ERROR("空のコールスタックからフレームをポップしようとしました");
    return nullptr;
  }
  
  auto frame = m_callStack.back();
  m_callStack.pop_back();
  
  // デバッグモードの場合、フレーム情報をログに記録
  if (m_debugMode.load(std::memory_order_acquire)) {
    const auto& sourceInfo = frame->getSourceInfo();
    LOG_TRACE("フレーム削除: {} ({}:{}:{})", 
              frame->getFunctionName(),
              sourceInfo.filename.empty() ? "native" : sourceInfo.filename,
              sourceInfo.line,
              sourceInfo.column);
    
    // デバッガにフレーム削除イベントを通知
    if (m_debugServer && m_isDebuggerAttached) {
      m_debugServer->notifyFramePop(frame);
    }
  }
  
  return frame;
}

std::shared_ptr<CallFrame> VM::currentFrame() const {
  return m_callStack.empty() ? nullptr : m_callStack.back();
}

void VM::throwException(ExceptionPtr exception) {
  m_currentException = std::move(exception);
  
  // 例外メトリクスの更新
  m_metrics.exceptionCount++;
  
  // デバッガがアタッチされている場合は通知
  if (m_isDebuggerAttached && m_debugServer) {
    m_debugServer->notifyException(m_currentException);
    
    // デバッグモードでブレークポイントが設定されている場合は実行を一時停止
    if (m_options.breakOnException) {
      m_debugServer->pauseExecution(BreakReason::Exception);
    }
  }
  
  // 例外情報をログに記録
  if (m_options.debugLogging) {
    LOG_DEBUG("例外がスローされました: {} - {}", 
              m_currentException->getType(), 
              m_currentException->getMessage());
    LOG_DEBUG("スタックトレース:\n{}", getStackTrace());
  }
  
  // 現在の実行を中断するためのシグナルを設定
  m_exceptionPending.store(true, std::memory_order_release);
  
  if (m_interpreter) {
    m_interpreter->setExceptionSignal(true);
  }
  
  if (m_jitCompiler) {
    m_jitCompiler->setExceptionSignal(true);
  }
}

ExceptionPtr VM::catchException() {
  auto exception = m_currentException;
  m_currentException = nullptr;
  
  // 例外シグナルをクリア
  if (m_interpreter) {
    m_interpreter->setExceptionSignal(false);
  }
  
  if (m_jitCompiler) {
    m_jitCompiler->setExceptionSignal(false);
  }
  
  return exception;
}

bool VM::hasUnhandledException() const {
  return m_currentException != nullptr;
}

void VM::clearException() {
  m_currentException = nullptr;
  
  // 例外シグナルをクリア
  if (m_interpreter) {
    m_interpreter->setExceptionSignal(false);
  }
  
  if (m_jitCompiler) {
    m_jitCompiler->setExceptionSignal(false);
  }
}

} // namespace core
} // namespace aerojs 