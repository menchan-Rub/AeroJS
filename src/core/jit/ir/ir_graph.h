/**
 * @file ir_graph.h
 * @brief 最先端の中間表現（IR）グラフの定義
 * @version 2.0.0
 * @license MIT
 */

#ifndef AEROJS_IR_GRAPH_H
#define AEROJS_IR_GRAPH_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <optional>
#include <functional>
#include <cassert>
#include <bitset>
#include <stack>
#include <queue>
#include <list>
#include <algorithm>
#include <variant>
#include <ostream>

#include "src/core/jit/profiler/type_info.h"
#include "src/core/runtime/values/value.h"
#include "src/utils/containers/hashmap/hashmap.h"

namespace aerojs {
namespace core {
namespace jit {
namespace ir {

// 前方宣言
class IRNode;
class IRGraph;
class BasicBlock;
class Value;
class Instruction;
class ConstantValue;
class VariableValue;
class PhiNode;

/**
 * @brief IRノードの種類
 */
enum class NodeType {
  // 値系
  Constant,      // 定数
  Variable,      // 変数
  Phi,           // Φノード
  Parameter,     // パラメータ
  
  // 制御フロー系
  BasicBlock,    // 基本ブロック
  Branch,        // 分岐
  Jump,          // ジャンプ
  Return,        // リターン
  Throw,         // 例外送出
  
  // 算術演算系
  Add,           // 加算
  Sub,           // 減算
  Mul,           // 乗算
  Div,           // 除算
  Mod,           // 剰余
  Neg,           // 符号反転
  
  // ビット演算系
  BitAnd,        // ビット論理積
  BitOr,         // ビット論理和
  BitXor,        // ビット排他的論理和
  BitNot,        // ビット否定
  ShiftLeft,     // 左シフト
  ShiftRight,    // 右シフト
  ShiftRightUnsigned, // 符号なし右シフト
  
  // 論理演算系
  LogicalAnd,    // 論理積
  LogicalOr,     // 論理和
  LogicalNot,    // 論理否定
  
  // 比較演算系
  Equal,         // 等価
  NotEqual,      // 非等価
  LessThan,      // 未満
  LessEqual,     // 以下
  GreaterThan,   // 超過
  GreaterEqual,  // 以上
  StrictEqual,   // 厳密等価
  StrictNotEqual, // 厳密非等価
  
  // メモリ操作系
  Load,          // ロード
  Store,         // ストア
  PropertyLoad,  // プロパティロード
  PropertyStore, // プロパティストア
  ElementLoad,   // 要素ロード
  ElementStore,  // 要素ストア
  
  // オブジェクト操作系
  CreateObject,      // オブジェクト生成
  CreateArray,       // 配列生成
  CreateFunction,    // 関数生成
  CreateClosure,     // クロージャ生成
  GetPrototype,      // プロトタイプ取得
  SetPrototype,      // プロトタイプ設定
  HasProperty,       // プロパティ存在確認
  DeleteProperty,    // プロパティ削除
  
  // 関数呼び出し系
  Call,              // 関数呼び出し
  Construct,         // コンストラクタ呼び出し
  ApplyFunction,     // apply呼び出し
  CallMethod,        // メソッド呼び出し
  
  // 型操作系
  TypeOf,            // typeof演算子
  InstanceOf,        // instanceof演算子
  TypeGuard,         // 型ガード
  TypeConversion,    // 型変換
  
  // 制御命令系
  Guard,             // ガード命令
  Unreachable,       // 到達不能
  Checkpoint,        // チェックポイント
  Deoptimize,        // 最適化解除
  OSREntry,          // OSRエントリポイント
  OSRExit,           // OSR出口
  
  // ループ関連
  LoopBegin,         // ループ開始
  LoopEnd,           // ループ終了
  LoopExit,          // ループ脱出
  LoopBack,          // ループバック
  
  // SIMD操作系
  VectorLoad,        // ベクトルロード
  VectorStore,       // ベクトルストア
  VectorAdd,         // ベクトル加算
  VectorSub,         // ベクトル減算
  VectorMul,         // ベクトル乗算
  VectorDiv,         // ベクトル除算
  VectorShuffle,     // ベクトルシャッフル
  
  // メタ情報系
  FrameState,        // フレーム状態
  StatePoint,        // 状態ポイント
  Metadata           // メタデータ
};

/**
 * @brief IRノードの型情報
 */
class IRType {
public:
  /**
   * @brief 型種別
   */
  enum class Kind {
    Any,          // 任意型
    Void,         // void型
    Boolean,      // ブール型
    Int32,        // 32ビット整数型
    Int64,        // 64ビット整数型
    Float64,      // 64ビット浮動小数点型
    String,       // 文字列型
    Object,       // オブジェクト型
    Array,        // 配列型
    Function,     // 関数型
    Symbol,       // シンボル型
    BigInt,       // BigInt型
    Undefined,    // undefined型
    Null,         // null型
    Vector,       // ベクトル型
    Union,        // 共用型
    ObjectShape,  // オブジェクト形状
    Tuple         // タプル型
  };
  
  IRType() : kind(Kind::Any), nullable(true) {}
  explicit IRType(Kind k) : kind(k), nullable(k == Kind::Null || k == Kind::Undefined) {}
  
  // 共用型を作成する
  static IRType createUnion(const std::vector<IRType>& types);
  
  // オブジェクト形状を作成する
  static IRType createObjectShape(uint32_t shapeId);
  
  // タプル型を作成する
  static IRType createTuple(const std::vector<IRType>& elementTypes);
  
  // 特定型判定
  bool isNumber() const { return kind == Kind::Int32 || kind == Kind::Int64 || kind == Kind::Float64; }
  bool isInteger() const { return kind == Kind::Int32 || kind == Kind::Int64; }
  bool isObject() const { return kind == Kind::Object || kind == Kind::Array || kind == Kind::Function; }
  bool isPrimitive() const { return isNumber() || kind == Kind::Boolean || kind == Kind::String || 
                                   kind == Kind::Symbol || kind == Kind::BigInt ||
                                   kind == Kind::Undefined || kind == Kind::Null; }
  bool isAny() const { return kind == Kind::Any; }
  bool isVoid() const { return kind == Kind::Void; }
  bool isUnion() const { return kind == Kind::Union; }
  bool isTuple() const { return kind == Kind::Tuple; }
  
  // プロパティ
  Kind kind;                             // 型種別
  bool nullable;                         // null/undefinedを許可するか
  std::optional<uint32_t> objectShapeId; // オブジェクト形状ID（オブジェクト型のみ）
  std::vector<IRType> unionTypes;        // 共用型の構成要素（共用型のみ）
  std::vector<IRType> tupleTypes;        // タプルの構成要素（タプル型のみ）
  size_t vectorSize;                     // ベクトルサイズ（ベクトル型のみ）
};

/**
 * @brief IR最適化フラグ
 */
enum class OptimizationFlag : uint32_t {
  None                  = 0,
  ConstantFolding       = 1 << 0,   // 定数畳み込み
  DeadCodeElimination   = 1 << 1,   // デッドコード除去
  CommonSubexpression   = 1 << 2,   // 共通部分式削除
  LoopInvariantMotion   = 1 << 3,   // ループ不変式移動
  LoopUnrolling         = 1 << 4,   // ループ展開
  Inlining              = 1 << 5,   // インライン化
  TailCallOptimization  = 1 << 6,   // 末尾呼出し最適化
  TypeSpecialization    = 1 << 7,   // 型特殊化
  BoundsCheckElimination = 1 << 8,  // 境界チェック除去
  RegisterAllocation    = 1 << 9,   // レジスタ割り当て
  ValueNumbering        = 1 << 10,  // 値番号付け
  Vectorization         = 1 << 11,  // ベクトル化
  MemoryOpt             = 1 << 12,  // メモリアクセス最適化
  Parallelization       = 1 << 13,  // 並列化
  TypeGuardElimination  = 1 << 14   // 型ガード除去
};

inline OptimizationFlag operator|(OptimizationFlag a, OptimizationFlag b) {
  return static_cast<OptimizationFlag>(
      static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(OptimizationFlag a, OptimizationFlag b) {
  return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief IR基本ノードクラス
 * 
 * すべてのIRノードの基底クラス
 */
class IRNode {
public:
  IRNode(NodeType type, IRGraph* graph, uint32_t id = 0)
    : m_type(type), m_graph(graph), m_id(id) {}
  
  virtual ~IRNode() = default;
  
  // 型情報
  NodeType getType() const { return m_type; }
  
  // グラフアクセス
  IRGraph* getGraph() const { return m_graph; }
  
  // ID操作
  uint32_t getId() const { return m_id; }
  void setId(uint32_t id) { m_id = id; }
  
  // 型キャスト
  template<typename T>
  T* as() {
    return dynamic_cast<T*>(this);
  }
  
  template<typename T>
  const T* as() const {
    return dynamic_cast<const T*>(this);
  }
  
  // ノード種別チェック
  bool isConstant() const { return m_type == NodeType::Constant; }
  bool isVariable() const { return m_type == NodeType::Variable; }
  bool isPhi() const { return m_type == NodeType::Phi; }
  bool isBasicBlock() const { return m_type == NodeType::BasicBlock; }
  bool isBranch() const { return m_type == NodeType::Branch; }
  bool isJump() const { return m_type == NodeType::Jump; }
  bool isReturn() const { return m_type == NodeType::Return; }
  bool isThrow() const { return m_type == NodeType::Throw; }
  bool isInstruction() const { return !isConstant() && !isVariable() && !isPhi() && !isBasicBlock(); }
  
  // デバッグ用
  virtual std::string toString() const = 0;
  
protected:
  NodeType m_type;     // ノード種別
  IRGraph* m_graph;    // 所属グラフ
  uint32_t m_id;       // ノードID
};

/**
 * @brief 値ノード
 * 
 * IR内で計算結果や変数などの値を表す
 */
class Value : public IRNode {
public:
  Value(NodeType type, IRGraph* graph, const IRType& valueType = IRType(IRType::Kind::Any))
    : IRNode(type, graph), m_valueType(valueType), m_uses() {}
  
  virtual ~Value() = default;
  
  // 型情報アクセス
  const IRType& getValueType() const { return m_valueType; }
  void setValueType(const IRType& type) { m_valueType = type; }
  
  // 使用箇所管理
  const std::vector<Instruction*>& getUses() const { return m_uses; }
  void addUse(Instruction* user) { m_uses.push_back(user); }
  void removeUse(Instruction* user);
  void replaceAllUsesWith(Value* newValue);
  
  // ランタイム値の取得
  virtual std::optional<runtime::Value> getRuntimeValue() const { return std::nullopt; }
  
protected:
  IRType m_valueType;                // 値の型情報
  std::vector<Instruction*> m_uses;  // この値を使用する命令のリスト
};

/**
 * @brief 定数値
 */
class ConstantValue : public Value {
public:
  ConstantValue(IRGraph* graph, const runtime::Value& value);
  
  // 定数値アクセス
  const runtime::Value& getValue() const { return m_value; }
  
  // ランタイム値の取得をオーバーライド
  std::optional<runtime::Value> getRuntimeValue() const override { return m_value; }
  
  // デバッグ用
  std::string toString() const override;
  
private:
  runtime::Value m_value;  // 定数値
};

/**
 * @brief 変数値
 */
class VariableValue : public Value {
public:
  VariableValue(IRGraph* graph, uint32_t index, const std::string& name = "", 
                const IRType& type = IRType(IRType::Kind::Any))
    : Value(NodeType::Variable, graph, type), m_index(index), m_name(name) {}
  
  // 変数情報アクセス
  uint32_t getIndex() const { return m_index; }
  const std::string& getName() const { return m_name; }
  void setName(const std::string& name) { m_name = name; }
  
  // デバッグ用
  std::string toString() const override;
  
private:
  uint32_t m_index;    // 変数インデックス
  std::string m_name;  // 変数名（デバッグ用）
};

/**
 * @brief パラメータ値
 */
class ParameterValue : public Value {
public:
  ParameterValue(IRGraph* graph, uint32_t index, const std::string& name = "", 
                 const IRType& type = IRType(IRType::Kind::Any))
    : Value(NodeType::Parameter, graph, type), m_index(index), m_name(name) {}
  
  // パラメータ情報アクセス
  uint32_t getIndex() const { return m_index; }
  const std::string& getName() const { return m_name; }
  void setName(const std::string& name) { m_name = name; }
  
  // デバッグ用
  std::string toString() const override;
  
private:
  uint32_t m_index;    // パラメータインデックス
  std::string m_name;  // パラメータ名（デバッグ用）
};

/**
 * @brief IR命令基底クラス
 * 
 * すべての計算命令の基底クラス
 */
class Instruction : public Value {
public:
  Instruction(NodeType type, IRGraph* graph, 
              const std::vector<Value*>& operands = {},
              const IRType& resultType = IRType(IRType::Kind::Any))
    : Value(type, graph, resultType), m_operands(operands), m_block(nullptr) {}
  
  virtual ~Instruction() = default;
  
  // オペランドアクセス
  const std::vector<Value*>& getOperands() const { return m_operands; }
  Value* getOperand(size_t index) const { 
    assert(index < m_operands.size());
    return m_operands[index]; 
  }
  void setOperand(size_t index, Value* value);
  void addOperand(Value* value);
  size_t getOperandCount() const { return m_operands.size(); }
  
  // ブロックアクセス
  BasicBlock* getBlock() const { return m_block; }
  void setBlock(BasicBlock* block) { m_block = block; }
  
  // 命令の削除
  virtual void remove();
  
  // 命令の複製
  virtual Instruction* clone(IRGraph* newGraph) const = 0;
  
  // 命令が純粋であるか（副作用なし）
  virtual bool isPure() const { return false; }
  
  // 命令が制御フローを変更するか
  virtual bool isControlFlow() const { return false; }
  
  // 命令が定数であるか
  virtual bool isConstantInstruction() const { return false; }
  
  // 命令がメモリアクセスを行うか
  virtual bool isMemoryAccess() const { return false; }
  
  // デバッグ用
  std::string toString() const override;
  
protected:
  std::vector<Value*> m_operands;  // 命令のオペランド
  BasicBlock* m_block;             // 所属する基本ブロック
};

/**
 * @brief 基本ブロック
 * 
 * 命令の連続したシーケンスを表す
 */
class BasicBlock : public IRNode {
public:
  explicit BasicBlock(IRGraph* graph, const std::string& label = "")
    : IRNode(NodeType::BasicBlock, graph), m_label(label), 
      m_instructions(), m_predecessors(), m_successors(),
      m_dominator(nullptr), m_immediateDominated(), m_loopHeader(false), 
      m_loopDepth(0), m_visited(false), m_unreachable(false) {}
  
  // ラベルアクセス
  const std::string& getLabel() const { return m_label; }
  void setLabel(const std::string& label) { m_label = label; }
  
  // 命令アクセス
  const std::vector<Instruction*>& getInstructions() const { return m_instructions; }
  void addInstruction(Instruction* instruction);
  void insertInstructionBefore(Instruction* newInstr, Instruction* position);
  void insertInstructionAfter(Instruction* newInstr, Instruction* position);
  void removeInstruction(Instruction* instruction);
  size_t getInstructionCount() const { return m_instructions.size(); }
  
  // 入出力ブロックアクセス
  const std::vector<BasicBlock*>& getPredecessors() const { return m_predecessors; }
  const std::vector<BasicBlock*>& getSuccessors() const { return m_successors; }
  void addPredecessor(BasicBlock* block);
  void addSuccessor(BasicBlock* block);
  void removePredecessor(BasicBlock* block);
  void removeSuccessor(BasicBlock* block);
  size_t getPredecessorCount() const { return m_predecessors.size(); }
  size_t getSuccessorCount() const { return m_successors.size(); }
  
  // 終端命令アクセス
  Instruction* getTerminator() const {
    return m_instructions.empty() ? nullptr : 
           m_instructions.back()->isControlFlow() ? m_instructions.back() : nullptr;
  }
  
  // PHIノードアクセス
  std::vector<PhiNode*> getPhiNodes() const;
  
  // 支配関係
  BasicBlock* getDominator() const { return m_dominator; }
  void setDominator(BasicBlock* block) { m_dominator = block; }
  const std::vector<BasicBlock*>& getImmediateDominated() const { return m_immediateDominated; }
  void addImmediateDominated(BasicBlock* block) { m_immediateDominated.push_back(block); }
  bool dominates(BasicBlock* block) const;
  
  // ループ情報
  bool isLoopHeader() const { return m_loopHeader; }
  void setLoopHeader(bool value) { m_loopHeader = value; }
  uint32_t getLoopDepth() const { return m_loopDepth; }
  void setLoopDepth(uint32_t depth) { m_loopDepth = depth; }
  
  // 訪問フラグ（グラフ走査用）
  bool isVisited() const { return m_visited; }
  void setVisited(bool visited) { m_visited = visited; }
  
  // 到達可能性
  bool isUnreachable() const { return m_unreachable; }
  void setUnreachable(bool unreachable) { m_unreachable = unreachable; }
  
  // デバッグ用
  std::string toString() const override;
  
private:
  std::string m_label;                      // ブロックラベル
  std::vector<Instruction*> m_instructions; // 命令リスト
  std::vector<BasicBlock*> m_predecessors;  // 先行ブロック
  std::vector<BasicBlock*> m_successors;    // 後続ブロック
  
  // 支配木情報
  BasicBlock* m_dominator;                  // このブロックを支配するブロック
  std::vector<BasicBlock*> m_immediateDominated; // このブロックが直接支配するブロック
  
  // ループ情報
  bool m_loopHeader;                        // ループヘッダーかどうか
  uint32_t m_loopDepth;                     // ループネスト深さ
  
  // 解析フラグ
  bool m_visited;                           // 訪問済みフラグ
  bool m_unreachable;                       // 到達不能フラグ
};

/**
 * @brief PHIノード
 * 
 * 制御フロー合流時の値選択を表す
 */
class PhiNode : public Instruction {
public:
  PhiNode(IRGraph* graph, const IRType& resultType = IRType(IRType::Kind::Any))
    : Instruction(NodeType::Phi, graph, {}, resultType), m_incomingValues(), m_incomingBlocks() {}
  
  // 入力値と対応するブロックの追加
  void addIncoming(Value* value, BasicBlock* block);
  
  // 入力値の取得
  Value* getIncomingValue(size_t index) const {
    assert(index < m_incomingValues.size());
    return m_incomingValues[index];
  }
  
  // 入力ブロックの取得
  BasicBlock* getIncomingBlock(size_t index) const {
    assert(index < m_incomingBlocks.size());
    return m_incomingBlocks[index];
  }
  
  // 入力数の取得
  size_t getIncomingCount() const {
    assert(m_incomingValues.size() == m_incomingBlocks.size());
    return m_incomingValues.size();
  }
  
  // 特定の入力ブロックに対応する値を取得
  Value* getIncomingValueForBlock(BasicBlock* block) const;
  
  // 入力値と対応するブロックの変更
  void setIncomingValue(size_t index, Value* value);
  void setIncomingBlock(size_t index, BasicBlock* block);
  
  // 特定の入力ブロックに対応する値を削除
  void removeIncomingValue(size_t index);
  
  // クローン作成
  Instruction* clone(IRGraph* newGraph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  std::vector<Value*> m_incomingValues;      // 入力値リスト
  std::vector<BasicBlock*> m_incomingBlocks; // 対応するブロックリスト
};

/**
 * @brief 分岐命令
 */
class BranchInstruction : public Instruction {
public:
  BranchInstruction(IRGraph* graph, Value* condition, 
                    BasicBlock* trueBlock, BasicBlock* falseBlock)
    : Instruction(NodeType::Branch, graph, {condition}) {
    m_trueBlock = trueBlock;
    m_falseBlock = falseBlock;
  }
  
  // 条件アクセス
  Value* getCondition() const { return getOperand(0); }
  void setCondition(Value* condition) { setOperand(0, condition); }
  
  // ブロックアクセス
  BasicBlock* getTrueBlock() const { return m_trueBlock; }
  BasicBlock* getFalseBlock() const { return m_falseBlock; }
  void setTrueBlock(BasicBlock* block) { m_trueBlock = block; }
  void setFalseBlock(BasicBlock* block) { m_falseBlock = block; }
  
  // 制御フロー命令
  bool isControlFlow() const override { return true; }
  
  // クローン作成
  Instruction* clone(IRGraph* newGraph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  BasicBlock* m_trueBlock;   // 条件が真の場合のジャンプ先
  BasicBlock* m_falseBlock;  // 条件が偽の場合のジャンプ先
};

/**
 * @brief ジャンプ命令
 */
class JumpInstruction : public Instruction {
public:
  JumpInstruction(IRGraph* graph, BasicBlock* targetBlock)
    : Instruction(NodeType::Jump, graph), m_targetBlock(targetBlock) {}
  
  // ターゲットブロックアクセス
  BasicBlock* getTargetBlock() const { return m_targetBlock; }
  void setTargetBlock(BasicBlock* block) { m_targetBlock = block; }
  
  // 制御フロー命令
  bool isControlFlow() const override { return true; }
  
  // クローン作成
  Instruction* clone(IRGraph* newGraph) const override;
  
  // デバッグ用
  std::string toString() const override;
  
private:
  BasicBlock* m_targetBlock; // ジャンプ先ブロック
};

/**
 * @brief リターン命令
 */
class ReturnInstruction : public Instruction {
public:
  ReturnInstruction(IRGraph* graph, Value* returnValue = nullptr)
    : Instruction(NodeType::Return, graph, returnValue ? std::vector<Value*>{returnValue} : std::vector<Value*>{}) {}
  
  // 戻り値アクセス
  Value* getReturnValue() const { return getOperandCount() > 0 ? getOperand(0) : nullptr; }
  void setReturnValue(Value* value) { 
    if (getOperandCount() > 0) setOperand(0, value); 
    else addOperand(value);
  }
  
  // 戻り値があるか
  bool hasReturnValue() const { return getOperandCount() > 0; }
  
  // 制御フロー命令
  bool isControlFlow() const override { return true; }
  
  // クローン作成
  Instruction* clone(IRGraph* newGraph) const override;
  
  // デバッグ用
  std::string toString() const override;
};

/**
 * @brief バイナリ命令基底クラス
 */
class BinaryInstruction : public Instruction {
public:
  BinaryInstruction(NodeType type, IRGraph* graph, Value* left, Value* right,
                    const IRType& resultType = IRType(IRType::Kind::Any))
    : Instruction(type, graph, {left, right}, resultType) {}
  
  // オペランドアクセス
  Value* getLeft() const { return getOperand(0); }
  Value* getRight() const { return getOperand(1); }
  void setLeft(Value* value) { setOperand(0, value); }
  void setRight(Value* value) { setOperand(1, value); }
  
  // 純粋命令
  bool isPure() const override { return true; }
  
  // 命令が定数かどうか
  bool isConstantInstruction() const override {
    return getLeft()->isConstant() && getRight()->isConstant();
  }
  
  // デバッグ用
  std::string toString() const override;
};

/**
 * @brief 中間表現(IR)グラフクラス
 * 
 * 最適化と変換の対象となる関数のIR表現全体を管理
 */
class IRGraph {
public:
  IRGraph();
  ~IRGraph();
  
  // ノード生成
  ConstantValue* createConstant(const runtime::Value& value);
  VariableValue* createVariable(uint32_t index, const std::string& name = "", 
                               const IRType& type = IRType(IRType::Kind::Any));
  ParameterValue* createParameter(uint32_t index, const std::string& name = "", 
                                 const IRType& type = IRType(IRType::Kind::Any));
  BasicBlock* createBasicBlock(const std::string& label = "");
  PhiNode* createPhi(const IRType& resultType = IRType(IRType::Kind::Any));
  BranchInstruction* createBranch(Value* condition, BasicBlock* trueBlock, BasicBlock* falseBlock);
  JumpInstruction* createJump(BasicBlock* targetBlock);
  ReturnInstruction* createReturn(Value* returnValue = nullptr);
  BinaryInstruction* createBinaryOp(NodeType op, Value* left, Value* right, 
                                   const IRType& resultType = IRType(IRType::Kind::Any));
  
  // 汎用命令生成
  template<typename T, typename... Args>
  T* create(Args&&... args) {
    T* node = new T(this, std::forward<Args>(args)...);
    node->setId(m_nextNodeId++);
    registerNode(node);
    return node;
  }
  
  // ノード管理
  void registerNode(IRNode* node);
  void removeNode(IRNode* node);
  
  // エントリーブロック管理
  BasicBlock* getEntryBlock() const { return m_entryBlock; }
  void setEntryBlock(BasicBlock* block) { m_entryBlock = block; }
  
  // パラメータ管理
  const std::vector<ParameterValue*>& getParameters() const { return m_parameters; }
  void addParameter(ParameterValue* param) { m_parameters.push_back(param); }
  size_t getParameterCount() const { return m_parameters.size(); }
  
  // ブロック管理
  const std::vector<BasicBlock*>& getBasicBlocks() const { return m_basicBlocks; }
  size_t getBasicBlockCount() const { return m_basicBlocks.size(); }
  
  // 変数管理
  const std::vector<VariableValue*>& getVariables() const { return m_variables; }
  size_t getVariableCount() const { return m_variables.size(); }
  
  // 最適化フラグ
  OptimizationFlag getAppliedOptimizations() const { return m_appliedOptimizations; }
  void setAppliedOptimizations(OptimizationFlag flags) { m_appliedOptimizations = flags; }
  void addAppliedOptimization(OptimizationFlag flag) {
    m_appliedOptimizations = m_appliedOptimizations | flag;
  }
  bool hasAppliedOptimization(OptimizationFlag flag) const {
    return (m_appliedOptimizations & flag);
  }
  
  // 型プロファイル情報
  void setTypeInfo(std::unique_ptr<profiler::TypeInfo> typeInfo) {
    m_typeInfo = std::move(typeInfo);
  }
  
  const profiler::TypeInfo* getTypeInfo() const {
    return m_typeInfo.get();
  }
  
  // グラフ検証
  bool verify() const;
  
  // グラフ解析
  void computeDominators();
  void computeLoopInfo();
  
  // グラフクローン作成
  std::unique_ptr<IRGraph> clone() const;
  
  // デバッグ用
  std::string toString() const;
  void dump() const;
  
private:
  uint32_t m_nextNodeId;                      // 次のノードID
  std::unordered_map<uint32_t, IRNode*> m_nodes; // すべてのノード
  std::vector<BasicBlock*> m_basicBlocks;     // 基本ブロックリスト
  std::vector<ParameterValue*> m_parameters;  // パラメータリスト
  std::vector<VariableValue*> m_variables;    // 変数リスト
  BasicBlock* m_entryBlock;                   // エントリーブロック
  
  OptimizationFlag m_appliedOptimizations;    // 適用済み最適化フラグ
  std::unique_ptr<profiler::TypeInfo> m_typeInfo; // 型プロファイル情報
  
  // 支配関係計算用ヘルパー
  void calculateImmediateDominators();
  void clearDominatorInfo();
  
  // ループ情報計算用ヘルパー
  void identifyLoops();
  void clearLoopInfo();
};

} // namespace ir
} // namespace jit
} // namespace core
} // namespace aerojs

#endif // AEROJS_IR_GRAPH_H 