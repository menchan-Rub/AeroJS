/**
 * @file bytecode_generator.h
 * @brief バイトコードジェネレーターのヘッダーファイル
 *
 * このファイルは、AST（抽象構文木）からバイトコードを生成するためのクラスを定義します。
 * バイトコードジェネレーターはASTノードを訪問して、JavaScriptエンジンの仮想マシンが実行できる
 * バイトコード命令列を生成します。
 */

#ifndef AEROJS_CORE_VM_BYTECODE_BYTECODE_GENERATOR_H_
#define AEROJS_CORE_VM_BYTECODE_BYTECODE_GENERATOR_H_

#include <memory>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../parser/ast/nodes/all_nodes.h"
#include "../../parser/ast/visitors/node_visitor.h"
#include "../interpreter/bytecode_instruction.h"
#include "bytecode_module.h"
#include "constant_pool.h"

namespace aerojs {
namespace core {

/**
 * @brief バイトコード生成のオプション
 */
struct BytecodeGeneratorOptions {
  bool optimize = true;        ///< 最適化を行うかどうか
  bool debug_info = true;      ///< デバッグ情報を含めるかどうか
  bool source_map = true;      ///< ソースマップを生成するかどうか
  bool strict_mode = false;    ///< 厳格モードで生成するかどうか
  int optimization_level = 1;  ///< 最適化レベル（0-3）

  BytecodeGeneratorOptions() = default;
};

/**
 * @brief スコープ情報を表す構造体
 */
struct ScopeInfo {
  enum class Type {
    Global,    ///< グローバルスコープ
    Function,  ///< 関数スコープ
    Block,     ///< ブロックスコープ
    Module,    ///< モジュールスコープ
    Catch,     ///< catch節のスコープ
    Class,     ///< クラススコープ
    With       ///< withステートメントのスコープ
  };

  Type type;                                       ///< スコープの種類
  std::unordered_map<std::string, int> variables;  ///< 変数名と対応するインデックス
  std::vector<std::string> constants;              ///< 定数プール内の定数
  bool strict_mode;                                ///< 厳格モードかどうか
  int loop_depth;                                  ///< ループのネスト深さ
  int function_depth;                              ///< 関数のネスト深さ

  ScopeInfo(Type type, bool strict_mode = false)
      : type(type), strict_mode(strict_mode), loop_depth(0), function_depth(0) {
  }
};

/**
 * @brief バイトコードジェネレーター
 *
 * ASTを訪問してバイトコードを生成するクラスです。
 * NodeVisitorを継承して、各種ASTノードに対応するバイトコードを生成します。
 */
class BytecodeGenerator : public NodeVisitor {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param options バイトコード生成オプション
   */
  explicit BytecodeGenerator(const BytecodeGeneratorOptions& options = BytecodeGeneratorOptions());

  /**
   * @brief デストラクタ
   */
  ~BytecodeGenerator() override;

  /**
   * @brief プログラムからバイトコードを生成
   *
   * @param program 生成元のプログラムAST
   * @param source_file ソースファイル名
   * @return 生成されたバイトコードモジュール
   */
  std::unique_ptr<BytecodeModule> generate(std::shared_ptr<ast::ProgramNode> program,
                                           const std::string& source_file = "");

  /**
   * @brief 式からバイトコードを生成
   *
   * @param expression 生成元の式AST
   * @param source_file ソースファイル名
   * @return 生成されたバイトコードモジュール
   */
  std::unique_ptr<BytecodeModule> generateFromExpression(std::shared_ptr<ast::ExpressionNode> expression,
                                                         const std::string& source_file = "");

  /**
   * @brief 生成されたバイトコードをダンプ（デバッグ用）
   *
   * @param output 出力ストリーム
   */
  void dumpBytecode(std::ostream& output) const;

  // NodeVisitor インターフェースの実装
  std::shared_ptr<Node> visitProgram(ast::ProgramNode* node) override;
  std::shared_ptr<Node> visitBlockStatement(ast::BlockStatementNode* node) override;
  std::shared_ptr<Node> visitExpressionStatement(ast::ExpressionStatementNode* node) override;
  std::shared_ptr<Node> visitIfStatement(ast::IfStatementNode* node) override;
  std::shared_ptr<Node> visitSwitchStatement(ast::SwitchStatementNode* node) override;
  std::shared_ptr<Node> visitForStatement(ast::ForStatementNode* node) override;
  std::shared_ptr<Node> visitForInStatement(ast::ForInStatementNode* node) override;
  std::shared_ptr<Node> visitForOfStatement(ast::ForOfStatementNode* node) override;
  std::shared_ptr<Node> visitWhileStatement(ast::WhileStatementNode* node) override;
  std::shared_ptr<Node> visitDoWhileStatement(ast::DoWhileStatementNode* node) override;
  std::shared_ptr<Node> visitTryStatement(ast::TryStatementNode* node) override;
  std::shared_ptr<Node> visitThrowStatement(ast::ThrowStatementNode* node) override;
  std::shared_ptr<Node> visitReturnStatement(ast::ReturnStatementNode* node) override;
  std::shared_ptr<Node> visitBreakStatement(ast::BreakStatementNode* node) override;
  std::shared_ptr<Node> visitContinueStatement(ast::ContinueStatementNode* node) override;
  std::shared_ptr<Node> visitEmptyStatement(ast::EmptyStatementNode* node) override;
  std::shared_ptr<Node> visitLabeledStatement(ast::LabeledStatementNode* node) override;
  std::shared_ptr<Node> visitWithStatement(ast::WithStatementNode* node) override;
  std::shared_ptr<Node> visitDebuggerStatement(ast::DebuggerStatementNode* node) override;

  std::shared_ptr<Node> visitFunctionDeclaration(ast::FunctionDeclarationNode* node) override;
  std::shared_ptr<Node> visitVariableDeclaration(ast::VariableDeclarationNode* node) override;
  std::shared_ptr<Node> visitClassDeclaration(ast::ClassDeclarationNode* node) override;

  std::shared_ptr<Node> visitIdentifier(ast::IdentifierNode* node) override;
  std::shared_ptr<Node> visitLiteral(ast::LiteralNode* node) override;
  std::shared_ptr<Node> visitRegExpLiteral(ast::RegExpLiteralNode* node) override;
  std::shared_ptr<Node> visitTemplateLiteral(ast::TemplateLiteralNode* node) override;
  std::shared_ptr<Node> visitBinaryExpression(ast::BinaryExpressionNode* node) override;
  std::shared_ptr<Node> visitAssignmentExpression(ast::AssignmentExpressionNode* node) override;
  std::shared_ptr<Node> visitLogicalExpression(ast::LogicalExpressionNode* node) override;
  std::shared_ptr<Node> visitUnaryExpression(ast::UnaryExpressionNode* node) override;
  std::shared_ptr<Node> visitUpdateExpression(ast::UpdateExpressionNode* node) override;
  std::shared_ptr<Node> visitMemberExpression(ast::MemberExpressionNode* node) override;
  std::shared_ptr<Node> visitCallExpression(ast::CallExpressionNode* node) override;
  std::shared_ptr<Node> visitNewExpression(ast::NewExpressionNode* node) override;
  std::shared_ptr<Node> visitConditionalExpression(ast::ConditionalExpressionNode* node) override;
  std::shared_ptr<Node> visitYieldExpression(ast::YieldExpressionNode* node) override;
  std::shared_ptr<Node> visitAwaitExpression(ast::AwaitExpressionNode* node) override;
  std::shared_ptr<Node> visitArrayExpression(ast::ArrayExpressionNode* node) override;
  std::shared_ptr<Node> visitObjectExpression(ast::ObjectExpressionNode* node) override;
  std::shared_ptr<Node> visitFunctionExpression(ast::FunctionExpressionNode* node) override;
  std::shared_ptr<Node> visitArrowFunctionExpression(ast::ArrowFunctionExpressionNode* node) override;
  std::shared_ptr<Node> visitClassExpression(ast::ClassExpressionNode* node) override;
  std::shared_ptr<Node> visitSequenceExpression(ast::SequenceExpressionNode* node) override;
  std::shared_ptr<Node> visitSpreadElement(ast::SpreadElementNode* node) override;
  std::shared_ptr<Node> visitTaggedTemplateExpression(ast::TaggedTemplateExpressionNode* node) override;

 private:
  /**
   * @brief 定数を定数プールに追加
   *
   * @param value 定数値
   * @return 定数のインデックス
   */
  uint32_t addConstant(const Value& value);

  /**
   * @brief バイトコード命令を追加
   *
   * @param opcode 命令のオペコード
   * @param operand1 第1オペランド（デフォルト: 0）
   * @param operand2 第2オペランド（デフォルト: 0）
   * @param location ソースコード内の位置
   * @return 命令のインデックス
   */
  uint32_t emitInstruction(Opcode opcode, uint32_t operand1 = 0, uint32_t operand2 = 0,
                           const SourceLocation& location = SourceLocation());

  /**
   * @brief ジャンプ命令を追加
   *
   * @param opcode ジャンプ命令のオペコード
   * @param location ソースコード内の位置
   * @return ジャンプ命令のインデックス
   */
  uint32_t emitJump(Opcode opcode, const SourceLocation& location = SourceLocation());

  /**
   * @brief ジャンプ命令のオペランドをパッチ
   *
   * @param jump_index ジャンプ命令のインデックス
   * @param jump_to ジャンプ先のインデックス
   */
  void patchJump(uint32_t jump_index, uint32_t jump_to);

  /**
   * @brief ループの開始位置を記録
   *
   * @param loop_start ループの開始位置
   */
  void beginLoop(uint32_t loop_start);

  /**
   * @brief ループの終了
   *
   * @return ループの情報
   */
  std::pair<uint32_t, std::vector<uint32_t>> endLoop();

  /**
   * @brief 新しいスコープを開始
   *
   * @param type スコープの種類
   * @param strict_mode 厳格モードかどうか
   */
  void beginScope(ScopeInfo::Type type, bool strict_mode = false);

  /**
   * @brief スコープを終了
   */
  void endScope();

  /**
   * @brief 現在のスコープに変数を宣言
   *
   * @param name 変数名
   * @param is_const 定数かどうか
   * @return 変数のインデックス
   */
  int declareVariable(const std::string& name, bool is_const = false);

  /**
   * @brief 変数の参照を解決
   *
   * @param name 変数名
   * @return 変数のインデックスと、グローバル変数かどうかを示すフラグのペア
   */
  std::pair<int, bool> resolveVariable(const std::string& name);

  // メンバー変数
  BytecodeGeneratorOptions m_options;                                  ///< バイトコード生成オプション
  std::unique_ptr<BytecodeModule> m_module;                            ///< 生成中のバイトコードモジュール
  std::vector<BytecodeInstruction> m_instructions;                     ///< 生成中の命令列
  std::shared_ptr<ConstantPool> m_constantPool;                        ///< 定数プール
  std::stack<ScopeInfo> m_scopeStack;                                  ///< スコープスタック
  std::stack<std::pair<uint32_t, std::vector<uint32_t>>> m_loopStack;  ///< ループスタック
  bool m_inFunction;                                                   ///< 関数内かどうか
  bool m_inMethod;                                                     ///< メソッド内かどうか
  bool m_inLoop;                                                       ///< ループ内かどうか
  bool m_inSwitch;                                                     ///< switch文内かどうか
  bool m_inTryBlock;                                                   ///< try文内かどうか
  bool m_needsResult;                                                  ///< 式の結果が必要かどうか
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_VM_BYTECODE_BYTECODE_GENERATOR_H_