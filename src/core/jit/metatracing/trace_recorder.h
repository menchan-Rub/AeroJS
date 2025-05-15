/**
 * @file trace_recorder.h
 * @brief 最先端のトレース記録システム
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_TRACE_RECORDER_H
#define AEROJS_TRACE_RECORDER_H

#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <optional>
#include <queue>
#include <set>
#include <bitset>
#include <variant>

#include "src/core/vm/bytecode/bytecode.h"
#include "src/core/jit/ir/ir_graph.h"
#include "src/core/runtime/values/value.h"
#include "src/core/jit/profiler/type_info.h"

namespace aerojs {
namespace core {
namespace jit {
namespace metatracing {

using namespace runtime;
using namespace vm::bytecode;
using namespace ir;

// 前方宣言
class TraceBuilder;
class TracingJIT;
class Trace;
class TraceNode;
class GuardNode;
class OperationNode;
class MergePoint;
class TraceRecorder;

/**
 * @brief トレースの記録状態
 */
enum class RecordingState {
  Idle,            // 記録していない
  Recording,       // トレース記録中
  Aborted,         // 記録中止
  Completed,       // 記録完了
  Optimizing,      // 最適化中
  Failed           // 失敗
};

/**
 * @brief トレース記録の開始理由
 */
enum class TraceReason {
  HotLoop,         // ホットループの検出
  HotFunction,     // ホット関数の検出
  SideExit,        // 既存トレースからの脱出
  Continuation,    // 既存トレースの継続
  Manual           // 手動で指定
};

/**
 * @brief トレース記録中止の理由
 */
enum class AbortReason {
  None,                 // 中止なし
  TooLong,              // トレースが長すぎる
  TooComplex,           // 複雑すぎる
  UnrecordableOp,       // 記録不能な命令
  Blacklisted,          // ブラックリスト登録済み
  Nested,               // ネストされたループ
  Divergent,            // 分岐が多すぎる
  OutOfMemory,          // メモリ不足
  Timeout,              // タイムアウト
  SpeculationFailure    // 投機的実行の失敗
};

/**
 * @brief トレースノードの種別
 */
enum class TraceNodeKind {
  Guard,                // ガード
  Operation,            // 操作
  Call,                 // 関数呼び出し
  MergePoint,           // マージポイント
  LoopHead,             // ループヘッド
  LoopEnd,              // ループ終端
  SideExit,             // サイド出口
  Annotation            // アノテーション
};

/**
 * @brief トレース内の命令操作の種別
 */
enum class OpKind {
  // 算術演算
  Add,                  // 加算
  Sub,                  // 減算
  Mul,                  // 乗算
  Div,                  // 除算
  Mod,                  // 剰余
  Neg,                  // 符号反転
  
  // ビット演算
  BitAnd,               // ビット論理積
  BitOr,                // ビット論理和
  BitXor,               // ビット排他的論理和
  BitNot,               // ビット否定
  ShiftLeft,            // 左シフト
  ShiftRight,           // 右シフト
  ShiftRightUnsigned,   // 符号なし右シフト
  
  // 論理演算
  LogicalAnd,           // 論理積
  LogicalOr,            // 論理和
  LogicalNot,           // 論理否定
  
  // 比較演算
  Equal,                // 等価
  NotEqual,             // 非等価
  LessThan,             // 未満
  LessEqual,            // 以下
  GreaterThan,          // 超過
  GreaterEqual,         // 以上
  StrictEqual,          // 厳密等価
  StrictNotEqual,       // 厳密非等価
  
  // メモリ操作
  Load,                 // ロード
  Store,                // ストア
  PropertyLoad,         // プロパティロード
  PropertyStore,        // プロパティストア
  ElementLoad,          // 要素ロード
  ElementStore,         // 要素ストア
  
  // オブジェクト操作
  CreateObject,         // オブジェクト生成
  CreateArray,          // 配列生成
  GetPrototype,         // プロトタイプ取得
  SetPrototype,         // プロトタイプ設定
  HasProperty,          // プロパティ存在確認
  DeleteProperty,       // プロパティ削除
  
  // 制御フロー
  Call,                 // 関数呼び出し
  Return,               // 戻り値
  GetLocal,             // ローカル変数取得
  SetLocal,             // ローカル変数設定
  GetArgument,          // 引数取得
  
  // 型操作
  TypeOf,               // 型取得
  InstanceOf,           // インスタンス確認
  
  // その他
  GetConstant,          // 定数取得
  Box,                  // ボックス化
  Unbox                 // アンボックス化
};

/**
 * @brief ガードの種別
 */
enum class GuardKind {
  TypeCheck,            // 型チェック
  ShapeCheck,           // 形状チェック
  BoundsCheck,          // 境界チェック
  NullCheck,            // nullチェック
  UndefinedCheck,       // undefinedチェック
  ZeroCheck,            // ゼロチェック
  OverflowCheck,        // オーバーフローチェック
  ReferenceCheck,       // 参照チェック
  ConditionCheck,       // 条件チェック
  ArrayCheck,           // 配列チェック
  ObjectCheck,          // オブジェクトチェック
  PropertyCheck,        // プロパティチェック
  ValueCheck            // 値チェック
};

/**
 * @brief トレースノードの基底クラス
 * 
 * トレース内のすべてのノードの基底クラス
 */
class TraceNode {
public:
  TraceNode(TraceNodeKind kind, uint32_t id, uint32_t bcOffset)
    : m_kind(kind), m_id(id), m_bytecodeOffset(bcOffset) {}
  
  virtual ~TraceNode() = default;
  
  // 基本アクセサ
  TraceNodeKind getKind() const { return m_kind; }
  uint32_t getId() const { return m_id; }
  uint32_t getBytecodeOffset() const { return m_bytecodeOffset; }
  
  // ノード種別チェック
  bool isGuard() const { return m_kind == TraceNodeKind::Guard; }
  bool isOperation() const { return m_kind == TraceNodeKind::Operation; }
  bool isCall() const { return m_kind == TraceNodeKind::Call; }
  bool isMergePoint() const { return m_kind == TraceNodeKind::MergePoint; }
  bool isLoopHead() const { return m_kind == TraceNodeKind::LoopHead; }
  bool isLoopEnd() const { return m_kind == TraceNodeKind::LoopEnd; }
  bool isSideExit() const { return m_kind == TraceNodeKind::SideExit; }
  
  // キャスト
  template<typename T>
  T* as() { return dynamic_cast<T*>(this); }
  
  template<typename T>
  const T* as() const { return dynamic_cast<const T*>(this); }
  
  // IRノードへの変換（実装はサブクラスで行う）
  virtual IRNode* toIRNode(IRGraph* graph) const = 0;
  
  // デバッグ用文字列表現
  virtual std::string toString() const = 0;
  
protected:
  TraceNodeKind m_kind;        // ノード種別
  uint32_t m_id;               // ノードID
  uint32_t m_bytecodeOffset;   // バイトコードオフセット
};

/**
 * @brief トレース内の操作ノード
 * 
 * 計算や操作を表すノード
 */
class OperationNode : public TraceNode {
public:
  OperationNode(OpKind opKind, uint32_t id, uint32_t bcOffset, 
               const std::vector<TraceNode*>& inputs = {})
    : TraceNode(TraceNodeKind::Operation, id, bcOffset), 
      m_opKind(opKind), m_inputs(inputs), m_typeInfo() {}
  
  // 操作情報
  OpKind getOpKind() const { return m_opKind; }
  
  // 入力ノード
  const std::vector<TraceNode*>& getInputs() const { return m_inputs; }
  void addInput(TraceNode* input) { m_inputs.push_back(input); }
  void setInput(size_t index, TraceNode* input) { 
    if (index < m_inputs.size()) {
      m_inputs[index] = input;
    }
  }
  size_t getInputCount() const { return m_inputs.size(); }
  
  // 型情報
  void setTypeInfo(const profiler::TypeInfo& info) { m_typeInfo = info; }
  const profiler::TypeInfo& getTypeInfo() const { return m_typeInfo; }
  
  // 特殊操作のチェック
  bool isPure() const;  // 副作用のない操作か
  bool isCommutative() const;  // 交換法則が成り立つか
  bool isAssociative() const;  // 結合法則が成り立つか
  
  // IRノードへの変換
  IRNode* toIRNode(IRGraph* graph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  OpKind m_opKind;                 // 操作種別
  std::vector<TraceNode*> m_inputs; // 入力ノード
  profiler::TypeInfo m_typeInfo;    // 型情報
};

/**
 * @brief ガードノード
 * 
 * 型や条件のチェックを表すノード
 */
class GuardNode : public TraceNode {
public:
  GuardNode(GuardKind guardKind, uint32_t id, uint32_t bcOffset, 
           TraceNode* input, uint32_t sideExitId)
    : TraceNode(TraceNodeKind::Guard, id, bcOffset), 
      m_guardKind(guardKind), m_input(input), m_sideExitId(sideExitId),
      m_expectedType(runtime::ValueType::Any), m_expectedShape(0),
      m_failureCount(0), m_successCount(0) {}
  
  // ガード情報
  GuardKind getGuardKind() const { return m_guardKind; }
  TraceNode* getInput() const { return m_input; }
  void setInput(TraceNode* input) { m_input = input; }
  
  // サイド出口
  uint32_t getSideExitId() const { return m_sideExitId; }
  void setSideExitId(uint32_t id) { m_sideExitId = id; }
  
  // 期待値
  runtime::ValueType getExpectedType() const { return m_expectedType; }
  void setExpectedType(runtime::ValueType type) { m_expectedType = type; }
  
  uint32_t getExpectedShape() const { return m_expectedShape; }
  void setExpectedShape(uint32_t shape) { m_expectedShape = shape; }
  
  // 統計
  uint32_t getFailureCount() const { return m_failureCount; }
  uint32_t getSuccessCount() const { return m_successCount; }
  void incrementFailureCount() { m_failureCount++; }
  void incrementSuccessCount() { m_successCount++; }
  float getSuccessRatio() const {
    uint32_t total = m_successCount + m_failureCount;
    return total > 0 ? static_cast<float>(m_successCount) / total : 1.0f;
  }
  
  // IRノードへの変換
  IRNode* toIRNode(IRGraph* graph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  GuardKind m_guardKind;        // ガード種別
  TraceNode* m_input;           // 入力ノード
  uint32_t m_sideExitId;        // サイド出口ID
  runtime::ValueType m_expectedType; // 期待される型
  uint32_t m_expectedShape;     // 期待されるオブジェクト形状
  uint32_t m_failureCount;      // 失敗回数
  uint32_t m_successCount;      // 成功回数
};

/**
 * @brief マージポイントノード
 * 
 * 制御フローが合流する点を表す
 */
class MergePoint : public TraceNode {
public:
  MergePoint(uint32_t id, uint32_t bcOffset, const std::vector<uint32_t>& traceIds)
    : TraceNode(TraceNodeKind::MergePoint, id, bcOffset), m_traceIds(traceIds) {}
  
  // マージするトレースIDs
  const std::vector<uint32_t>& getTraceIds() const { return m_traceIds; }
  void addTraceId(uint32_t traceId) { m_traceIds.push_back(traceId); }
  
  // IRノードへの変換
  IRNode* toIRNode(IRGraph* graph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  std::vector<uint32_t> m_traceIds;  // マージするトレースのID一覧
};

/**
 * @brief コールノード
 * 
 * 関数呼び出しを表す
 */
class CallNode : public TraceNode {
public:
  CallNode(uint32_t id, uint32_t bcOffset, uint32_t functionId, 
          const std::vector<TraceNode*>& args)
    : TraceNode(TraceNodeKind::Call, id, bcOffset), 
      m_functionId(functionId), m_args(args), m_inlined(false) {}
  
  // 関数情報
  uint32_t getFunctionId() const { return m_functionId; }
  
  // 引数
  const std::vector<TraceNode*>& getArgs() const { return m_args; }
  void addArg(TraceNode* arg) { m_args.push_back(arg); }
  
  // インライン化フラグ
  bool isInlined() const { return m_inlined; }
  void setInlined(bool inlined) { m_inlined = inlined; }
  
  // IRノードへの変換
  IRNode* toIRNode(IRGraph* graph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  uint32_t m_functionId;            // 呼び出し先関数ID
  std::vector<TraceNode*> m_args;   // 引数ノード
  bool m_inlined;                   // インライン展開されたかどうか
};

/**
 * @brief トレースのサイド出口
 */
class SideExit {
public:
  SideExit(uint32_t id, uint32_t bcOffset, GuardNode* guard)
    : m_id(id), m_bytecodeOffset(bcOffset), m_guard(guard),
      m_sideTraceId(0), m_hasSideTrace(false), m_executionCount(0) {}
  
  // 基本情報
  uint32_t getId() const { return m_id; }
  uint32_t getBytecodeOffset() const { return m_bytecodeOffset; }
  
  // ガード
  GuardNode* getGuard() const { return m_guard; }
  
  // サイドトレース
  uint32_t getSideTraceId() const { return m_sideTraceId; }
  void setSideTraceId(uint32_t id) { 
    m_sideTraceId = id; 
    m_hasSideTrace = true;
  }
  
  bool hasSideTrace() const { return m_hasSideTrace; }
  
  // 実行統計
  uint32_t getExecutionCount() const { return m_executionCount; }
  void incrementExecutionCount() { m_executionCount++; }
  
  // サイドトレースが必要かどうか
  bool needsSideTrace() const {
    return !m_hasSideTrace && m_executionCount > 10;
  }
  
private:
  uint32_t m_id;                // 出口ID
  uint32_t m_bytecodeOffset;    // バイトコードオフセット
  GuardNode* m_guard;           // 出口の原因となったガード
  uint32_t m_sideTraceId;       // サイドトレースID
  bool m_hasSideTrace;          // サイドトレースがあるかどうか
  uint32_t m_executionCount;    // 実行回数
};

/**
 * @brief トレース
 * 
 * 実行経路を記録したトレース本体
 */
class Trace {
public:
  Trace(uint32_t id, TraceReason reason, BytecodeFunction* function, uint32_t bcOffset)
    : m_id(id), m_reason(reason), m_function(function), m_startOffset(bcOffset),
      m_rootTraceId(0), m_isRootTrace(true), m_isCompiled(false),
      m_executionCount(0), m_abortReason(AbortReason::None),
      m_nextNodeId(0) {}
  
  // 基本情報
  uint32_t getId() const { return m_id; }
  TraceReason getReason() const { return m_reason; }
  BytecodeFunction* getFunction() const { return m_function; }
  uint32_t getStartOffset() const { return m_startOffset; }
  
  // ルートトレース情報
  bool isRootTrace() const { return m_isRootTrace; }
  uint32_t getRootTraceId() const { return m_rootTraceId; }
  void setRootTraceId(uint32_t id) { 
    m_rootTraceId = id; 
    m_isRootTrace = false;
  }
  
  // コンパイル状態
  bool isCompiled() const { return m_isCompiled; }
  void setCompiled(bool compiled) { m_isCompiled = compiled; }
  
  // 実行統計
  uint32_t getExecutionCount() const { return m_executionCount; }
  void incrementExecutionCount() { m_executionCount++; }
  
  // 中止理由
  AbortReason getAbortReason() const { return m_abortReason; }
  void setAbortReason(AbortReason reason) { m_abortReason = reason; }
  bool isAborted() const { return m_abortReason != AbortReason::None; }
  
  // ノード管理
  uint32_t getNextNodeId() { return m_nextNodeId++; }
  
  template<typename T, typename... Args>
  T* createNode(Args&&... args) {
    T* node = new T(getNextNodeId(), std::forward<Args>(args)...);
    m_nodes.push_back(std::unique_ptr<TraceNode>(node));
    return node;
  }
  
  void addNode(std::unique_ptr<TraceNode> node) {
    m_nodes.push_back(std::move(node));
  }
  
  const std::vector<std::unique_ptr<TraceNode>>& getNodes() const { return m_nodes; }
  size_t getNodeCount() const { return m_nodes.size(); }
  
  // ループ情報
  TraceNode* getLoopHead() const;
  TraceNode* getLoopEnd() const;
  bool isLoop() const { return getLoopHead() != nullptr && getLoopEnd() != nullptr; }
  
  // サイド出口管理
  SideExit* createSideExit(uint32_t bcOffset, GuardNode* guard) {
    uint32_t exitId = static_cast<uint32_t>(m_sideExits.size());
    m_sideExits.push_back(std::make_unique<SideExit>(exitId, bcOffset, guard));
    return m_sideExits.back().get();
  }
  
  SideExit* getSideExit(uint32_t exitId) const {
    return exitId < m_sideExits.size() ? m_sideExits[exitId].get() : nullptr;
  }
  
  const std::vector<std::unique_ptr<SideExit>>& getSideExits() const { return m_sideExits; }
  size_t getSideExitCount() const { return m_sideExits.size(); }
  
  // IRへの変換
  std::unique_ptr<IRGraph> toIR() const;
  
  // デバッグ用
  std::string toString() const;
  
private:
  uint32_t m_id;                                     // トレースID
  TraceReason m_reason;                              // 記録理由
  BytecodeFunction* m_function;                      // 関数
  uint32_t m_startOffset;                            // 開始バイトコードオフセット
  uint32_t m_rootTraceId;                            // ルートトレースID
  bool m_isRootTrace;                                // ルートトレースかどうか
  bool m_isCompiled;                                 // コンパイル済みかどうか
  uint32_t m_executionCount;                         // 実行回数
  AbortReason m_abortReason;                         // 中止理由
  uint32_t m_nextNodeId;                             // 次のノードID
  std::vector<std::unique_ptr<TraceNode>> m_nodes;   // トレースノード
  std::vector<std::unique_ptr<SideExit>> m_sideExits; // サイド出口
};

/**
 * @brief トレース記録設定
 */
struct TraceRecorderConfig {
  uint32_t maxTraceLength = 1000;        // 最大トレース長
  uint32_t maxTraceComplexity = 200;     // 最大トレース複雑度
  uint32_t maxRecursionDepth = 5;        // 最大再帰深度
  uint32_t maxSideExits = 50;            // 最大サイド出口数
  uint32_t maxRecordingTimeMs = 1000;    // 最大記録時間（ミリ秒）
  uint32_t hotLoopThreshold = 10;        // ホットループ閾値
  uint32_t hotFunctionThreshold = 20;    // ホット関数閾値
  uint32_t sideExitHotThreshold = 10;    // サイド出口のホット閾値
  
  bool recordLoops = true;               // ループ記録を有効化
  bool recordMethodCalls = true;         // メソッド呼び出し記録を有効化
  bool recordRecursion = true;           // 再帰呼び出し記録を有効化
  bool recordSideExits = true;           // サイド出口記録を有効化
  bool useTypeSpeculation = true;        // 型推測を使用
  bool useShapeSpeculation = true;       // 形状推測を使用
  bool useConstantSpeculation = true;    // 定数推測を使用
  bool useInlining = true;               // インライン化を使用
  
  std::vector<std::string> blacklistedFunctions; // ブラックリスト関数名
};

/**
 * @brief トレース記録機
 * 
 * JavaScriptの実行パスを記録し、最適化のためのトレースを生成する
 */
class TraceRecorder {
public:
  TraceRecorder(TracingJIT* jit);
  ~TraceRecorder();
  
  // トレース記録設定
  void setConfig(const TraceRecorderConfig& config) { m_config = config; }
  const TraceRecorderConfig& getConfig() const { return m_config; }
  
  // 状態確認
  RecordingState getState() const { return m_state; }
  bool isRecording() const { return m_state == RecordingState::Recording; }
  bool hasAborted() const { return m_state == RecordingState::Aborted; }
  bool hasCompleted() const { return m_state == RecordingState::Completed; }
  
  // トレース記録開始
  bool startRecording(BytecodeFunction* function, uint32_t bcOffset, TraceReason reason);
  
  // トレース記録中止
  void abortRecording(AbortReason reason);
  
  // トレース記録終了
  Trace* endRecording();
  
  // 命令記録
  void recordBytecode(Opcode opcode, uint32_t bcOffset);
  
  // 値記録
  TraceNode* recordConstantValue(const Value& value, uint32_t bcOffset);
  TraceNode* recordLocalLoad(uint32_t index, uint32_t bcOffset);
  TraceNode* recordLocalStore(uint32_t index, TraceNode* value, uint32_t bcOffset);
  TraceNode* recordArgumentLoad(uint32_t index, uint32_t bcOffset);
  
  // 操作記録
  TraceNode* recordBinaryOp(OpKind kind, TraceNode* left, TraceNode* right, uint32_t bcOffset);
  TraceNode* recordUnaryOp(OpKind kind, TraceNode* operand, uint32_t bcOffset);
  TraceNode* recordPropertyLoad(TraceNode* object, const std::string& prop, uint32_t bcOffset);
  TraceNode* recordPropertyStore(TraceNode* object, const std::string& prop, TraceNode* value, uint32_t bcOffset);
  TraceNode* recordElementLoad(TraceNode* array, TraceNode* index, uint32_t bcOffset);
  TraceNode* recordElementStore(TraceNode* array, TraceNode* index, TraceNode* value, uint32_t bcOffset);
  
  // 関数呼び出し記録
  TraceNode* recordCall(uint32_t functionId, const std::vector<TraceNode*>& args, uint32_t bcOffset);
  TraceNode* recordReturn(TraceNode* value, uint32_t bcOffset);
  
  // ガード記録
  GuardNode* recordTypeGuard(GuardKind kind, TraceNode* value, runtime::ValueType expectedType, uint32_t bcOffset);
  GuardNode* recordShapeGuard(TraceNode* object, uint32_t expectedShape, uint32_t bcOffset);
  GuardNode* recordBoundsGuard(TraceNode* index, TraceNode* array, uint32_t bcOffset);
  
  // ループ記録
  void recordLoopHead(uint32_t bcOffset);
  void recordLoopEnd(uint32_t bcOffset, TraceNode* condition);
  
  // 現在のトレース取得
  Trace* getCurrentTrace() const { return m_currentTrace.get(); }
  
  // デバッグ情報
  std::string getRecordingInfo() const;
  
private:
  TracingJIT* m_jit;                       // 所属するトレーシングJIT
  RecordingState m_state;                  // 記録状態
  TraceRecorderConfig m_config;            // 記録設定
  std::unique_ptr<Trace> m_currentTrace;   // 現在記録中のトレース
  std::unordered_map<uint32_t, TraceNode*> m_locals; // ローカル変数マップ
  TraceNode* m_lastNode;                   // 最後に記録したノード
  uint32_t m_recordedOps;                  // 記録した命令数
  uint32_t m_recursionDepth;               // 再帰深度
  
  // サイド出口生成
  SideExit* createSideExit(GuardNode* guard, uint32_t bcOffset);
  
  // 実行状態の検証
  bool validateRecording();
  
  // トレースのクリーンアップと最適化
  void optimizeTrace(Trace* trace);
  
  // 冗長なガードの削除
  void eliminateRedundantGuards(Trace* trace);
  
  // トレースノードの型推論
  void inferTypes(Trace* trace);
};

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs

    // 定数定義
    static constexpr size_t MAX_TRACE_LENGTH = 10000;     ///< 最大トレース長
    static constexpr size_t MAX_GUARD_FAILURES = 10;      ///< 最大ガード失敗数
    static constexpr size_t MAX_SIDE_EXITS = 20;          ///< 最大サイドエグジット数
    static constexpr size_t MAX_NESTED_TRACE_DEPTH = 5;   ///< 最大ネストトレース深度
    static constexpr size_t MAX_LOOP_ITERATIONS = 100;    ///< 最大ループ反復回数
    static constexpr size_t MAX_INLINE_CALL_DEPTH = 3;    ///< 最大インライン呼び出し深度
    
    /**
     * @brief スタックスナップショット
     */
    struct StackSnapshot {
        std::vector<runtime::Value> values;  ///< スタック値
        size_t stackPointer;                 ///< スタックポインタ
        size_t framePointer;                 ///< フレームポインタ
    };
    
    /**
     * @brief トレース命令
     */
    struct TraceInstruction {
        TraceOpcode opcode;                  ///< トレースオペコード
        vm::bytecode::BytecodeAddress location; ///< バイトコード位置
        vm::bytecode::Opcode bytecodeOp;     ///< バイトコードオペコード
        std::vector<runtime::Value> operands; ///< オペランド
        uint64_t timestamp;                  ///< タイムスタンプ
        
        // ガード関連情報
        GuardCondition guardCondition;       ///< ガード条件
        runtime::ValueType expectedType;     ///< 期待される型
        
        // サイドエグジット関連情報
        SideExitType sideExitType;           ///< サイドエグジットタイプ
        
        // 最適化ヒント関連情報
        OptimizationHint optimizationHint;   ///< 最適化ヒント
        
        // スタックスナップショット
        bool hasStackSnapshot = false;       ///< スナップショットがあるかどうか
        StackSnapshot stackSnapshot;         ///< スタックスナップショット
    };
    
    /**
     * @brief コンテキストスナップショット
     */
    struct ContextSnapshot {
        vm::bytecode::BytecodeAddress instruction; ///< 現在の命令
        std::vector<runtime::Value> stackValues;   ///< スタック値
        size_t stackPointer;                    ///< スタックポインタ
        size_t framePointer;                    ///< フレームポインタ
        
        /**
         * @brief コンストラクタ
         * @param context 実行コンテキスト
         */
        explicit ContextSnapshot(runtime::execution::ExecutionContext* context);
        
        /**
         * @brief コピーコンストラクタ
         */
        ContextSnapshot(const ContextSnapshot& other);
        
        /**
         * @brief デストラクタ
         */
        ~ContextSnapshot();
    };
    
    /**
     * @brief サイドエグジット情報
     */
    struct SideExit {
        vm::bytecode::BytecodeAddress location; ///< バイトコード位置
        SideExitType type;                    ///< サイドエグジットタイプ
        size_t instructionIndex;              ///< トレース命令インデックス
        std::unique_ptr<ContextSnapshot> contextSnapshot; ///< コンテキストスナップショット
        
        // ガード失敗情報（ガード失敗によるサイドエグジットの場合）
        GuardCondition guardCondition;        ///< ガード条件
        runtime::ValueType expectedType;      ///< 期待される型
        runtime::Value actualValue;           ///< 実際の値
    };
    
    /**
     * @brief トレース情報
     */
    struct Trace {
        vm::bytecode::BytecodeAddress* entryPoint; ///< トレース開始位置
        vm::bytecode::BytecodeAddress* exitPoint;  ///< トレース終了位置
        uint64_t startTimestamp;                ///< 開始タイムスタンプ
        uint64_t executionTimeNs;               ///< 実行時間（ナノ秒）
        std::vector<TraceInstruction> instructions; ///< トレース命令列
        std::vector<SideExit> sideExits;        ///< サイドエグジット情報
        std::unique_ptr<ContextSnapshot> contextSnapshot; ///< コンテキストスナップショット
        ExitReason exitReason;                  ///< 終了理由
        size_t executedBytecodes;               ///< 実行されたバイトコード数
    };
    
    /**
     * @brief コンストラクタ
     * @param jit トレーシングJITエンジンへの参照
     */
    explicit TraceRecorder(TracingJIT& jit);
    
    /**
     * @brief デストラクタ
     */
    ~TraceRecorder();
    
    /**
     * @brief トレース記録を開始する
     * @param context 実行コンテキスト
     * @param entryPoint トレース開始位置
     * @return 記録開始に成功したらtrue
     */
    bool startRecording(runtime::execution::ExecutionContext* context, 
                         const vm::bytecode::BytecodeAddress& entryPoint);
    
    /**
     * @brief トレース記録を強制終了する
     * @param reason 終了理由
     */
    void abortRecording(ExitReason reason);
    
    /**
     * @brief トレース記録を完了する
     * @return 記録されたトレース
     */
    std::unique_ptr<Trace> finishRecording();
    
    /**
     * @brief バイトコード命令の実行を記録する
     * @param context 実行コンテキスト
     * @param location バイトコードの位置
     * @param opcode バイトコードのオペコード
     * @param operands オペランドリスト
     */
    void recordBytecodeExecution(runtime::execution::ExecutionContext* context,
                                  const vm::bytecode::BytecodeAddress& location,
                                  vm::bytecode::Opcode opcode,
                                  const std::vector<runtime::Value>& operands);
    
    /**
     * @brief ガード条件を記録する
     * @param context 実行コンテキスト
     * @param location バイトコードの位置
     * @param condition ガード条件
     * @param expectedType 期待される値の型
     * @param actualValue 実際の値
     * @return ガードが成功した場合はtrue
     */
    bool recordGuardCondition(runtime::execution::ExecutionContext* context,
                               const vm::bytecode::BytecodeAddress& location,
                               GuardCondition condition,
                               runtime::ValueType expectedType,
                               const runtime::Value& actualValue);
    
    /**
     * @brief サイドエグジットを記録する
     * @param context 実行コンテキスト
     * @param location バイトコードの位置
     * @param exitType サイドエグジットのタイプ
     */
    void recordSideExit(runtime::execution::ExecutionContext* context,
                         const vm::bytecode::BytecodeAddress& location,
                         SideExitType exitType);
    
    /**
     * @brief 最適化条件のヒントを記録する
     * @param location バイトコードの位置
     * @param hint 最適化ヒント
     * @param data 追加データ
     */
    void recordOptimizationHint(const vm::bytecode::BytecodeAddress& location,
                                 OptimizationHint hint,
                                 const runtime::Value& data);
    
    /**
     * @brief トレースレコーダーをリセットする
     */
    void reset();
    
    /**
     * @brief トレース記録中かどうかを返す
     */
    bool isRecording() const;
    
    /**
     * @brief 現在の記録深度を返す
     */
    int getRecordingDepth() const;
    
    /**
     * @brief 最後のエントリーポイントを返す
     */
    const vm::bytecode::BytecodeAddress* getLastEntryPoint() const;
    
    /**
     * @brief 統計情報の収集を有効化/無効化する
     */
    void enableStatistics(bool enable);
    
private:
    TracingJIT& m_jit;                       ///< トレーシングJITエンジンへの参照
    
    bool m_isRecording;                      ///< 記録中フラグ
    Trace m_currentTrace;                    ///< 現在のトレース
    ExitReason m_exitReason;                 ///< 終了理由
    
    int m_guardFailureCount;                 ///< ガード失敗カウント
    int m_recordingDepth;                    ///< 記録深度
    int m_loopIterationCount;                ///< ループ反復カウント
    
    const vm::bytecode::BytecodeAddress* m_lastEntryPoint; ///< 最後のエントリーポイント
    
    bool m_statisticsEnabled;                ///< 統計情報収集フラグ
    
    /**
     * @brief ガード失敗を記録する
     * @param context 実行コンテキスト
     * @param location バイトコードの位置
     * @param condition ガード条件
     * @param expectedType 期待される値の型
     * @param actualValue 実際の値
     */
    void recordGuardFailure(runtime::execution::ExecutionContext* context,
                             const vm::bytecode::BytecodeAddress& location,
                             GuardCondition condition,
                             runtime::ValueType expectedType,
                             const runtime::Value& actualValue);
    
    /**
     * @brief ガード条件を評価する
     * @param condition ガード条件
     * @param expectedType 期待される値の型
     * @param actualValue 実際の値
     * @return ガードが成功した場合はtrue
     */
    bool evaluateGuard(GuardCondition condition,
                        runtime::ValueType expectedType,
                        const runtime::Value& actualValue);
    
    /**
     * @brief 現在のタイムスタンプを取得する
     */
    uint64_t getCurrentTimestamp();
    
    /**
     * @brief スタックスナップショットを取得する
     * @param context 実行コンテキスト
     * @return スタックスナップショット
     */
    StackSnapshot captureStackSnapshot(runtime::execution::ExecutionContext* context);
    
    /**
     * @brief 空のトレースを初期化する
     */
    void initializeEmptyTrace();
    
    /**
     * @brief 後方ジャンプかどうかをチェックする
     * @param current 現在の位置
     * @param target ジャンプ先
     * @return 後方ジャンプならtrue
     */
    bool isBackwardJump(const vm::bytecode::BytecodeAddress& current,
                         const vm::bytecode::BytecodeAddress& target);
    
    /**
     * @brief バイトコードのジャンプターゲットを取得する
     * @param location 現在の位置
     * @param opcode バイトコードのオペコード
     * @param operands オペランドリスト
     * @return ジャンプ先アドレス
     */
    vm::bytecode::BytecodeAddress getBytecodeJumpTarget(
        const vm::bytecode::BytecodeAddress& location,
        vm::bytecode::Opcode opcode,
        const std::vector<runtime::Value>& operands);
};

} // namespace metatracing
} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_JIT_METATRACING_TRACE_RECORDER_H 