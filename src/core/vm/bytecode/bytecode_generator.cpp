/**
 * @file bytecode_generator.cpp
 * @brief バイトコードジェネレーターの実装ファイル
 * 
 * このファイルは、AST（抽象構文木）からバイトコードを生成するバイトコードジェネレーターを実装します。
 * 各種ASTノードを訪問し、対応するバイトコード命令を生成します。
 */

#include "bytecode_generator.h"
#include <iostream>
#include <sstream>
#include <cassert>

namespace aerojs {
namespace core {

BytecodeGenerator::BytecodeGenerator(const BytecodeGeneratorOptions& options)
    : m_options(options),
      m_inFunction(false),
      m_inMethod(false),
      m_inLoop(false),
      m_inSwitch(false),
      m_inTryBlock(false),
      m_needsResult(true) {
    
    // 定数プールを初期化
    m_constantPool = std::make_shared<ConstantPool>();
    
    // バイトコードモジュールを初期化
    BytecodeModuleMetadata metadata;
    metadata.strict_mode = options.strict_mode;
    m_module = std::make_unique<BytecodeModule>(metadata);
    
    // グローバルスコープを作成
    beginScope(ScopeInfo::Type::Global, options.strict_mode);
}

BytecodeGenerator::~BytecodeGenerator() {
    // すべてのスコープがスタックから取り除かれていることを確認
    assert(m_scopeStack.size() <= 1);
    
    // グローバルスコープを閉じる
    if (!m_scopeStack.empty()) {
        endScope();
    }
}

std::unique_ptr<BytecodeModule> BytecodeGenerator::generate(
    std::shared_ptr<ast::ProgramNode> program, 
    const std::string& source_file) {
    
    // メタデータを設定
    BytecodeModuleMetadata metadata = m_module->getMetadata();
    metadata.source_file = source_file;
    metadata.is_module = program->isModule();
    m_module->setMetadata(metadata);
    
    // ASTを訪問してバイトコードを生成
    program->accept(this);
    
    // 生成された命令列をモジュールに設定
    m_module->setInstructions(m_instructions);
    
    // 定数プールをモジュールに設定
    m_module->setConstantPool(m_constantPool);
    
    return std::move(m_module);
}

std::unique_ptr<BytecodeModule> BytecodeGenerator::generateFromExpression(
    std::shared_ptr<ast::ExpressionNode> expression,
    const std::string& source_file) {
    
    // メタデータを設定
    BytecodeModuleMetadata metadata = m_module->getMetadata();
    metadata.source_file = source_file;
    m_module->setMetadata(metadata);
    
    // 式を評価
    expression->accept(this);
    
    // 結果を返す命令を追加
    emitInstruction(Opcode::RET, 0, 0, expression->getLocation());
    
    // 生成された命令列をモジュールに設定
    m_module->setInstructions(m_instructions);
    
    // 定数プールをモジュールに設定
    m_module->setConstantPool(m_constantPool);
    
    return std::move(m_module);
}

void BytecodeGenerator::dumpBytecode(std::ostream& output) const {
    m_module->dump(output, true);
}

uint32_t BytecodeGenerator::addConstant(const Value& value) {
    return m_constantPool->addConstant(value);
}

uint32_t BytecodeGenerator::emitInstruction(
    Opcode opcode,
    uint32_t operand1,
    uint32_t operand2,
    const SourceLocation& location) {
    
    // 命令を作成
    BytecodeInstruction instruction(opcode, operand1, operand2);
    
    // 命令を命令列に追加
    uint32_t index = static_cast<uint32_t>(m_instructions.size());
    m_instructions.push_back(instruction);
    
    // ソースマップエントリを追加（デバッグ情報が有効な場合）
    if (m_options.debug_info && location.isValid()) {
        SourceMapEntry entry(index, location.line, location.column, location.filename);
        m_module->addSourceMapEntry(entry);
    }
    
    return index;
}

uint32_t BytecodeGenerator::emitJump(Opcode opcode, const SourceLocation& location) {
    // ジャンプ命令を追加（ジャンプ先は仮の値0）
    return emitInstruction(opcode, 0, 0, location);
}

void BytecodeGenerator::patchJump(uint32_t jump_index, uint32_t jump_to) {
    // 命令列の範囲外をチェック
    if (jump_index >= m_instructions.size()) {
        // エラー: 無効なジャンプインデックス
        return;
    }
    
    // ジャンプ命令のオペランドを更新
    m_instructions[jump_index].operand1 = jump_to;
}

void BytecodeGenerator::beginLoop(uint32_t loop_start) {
    // ループスタックにループ情報を追加
    m_loopStack.push(std::make_pair(loop_start, std::vector<uint32_t>()));
    m_inLoop = true;
}

std::pair<uint32_t, std::vector<uint32_t>> BytecodeGenerator::endLoop() {
    if (m_loopStack.empty()) {
        // エラー: 対応するbeginLoopが存在しない
        return std::make_pair(0, std::vector<uint32_t>());
    }
    
    // ループ情報を取得して削除
    auto loop_info = m_loopStack.top();
    m_loopStack.pop();
    
    // ループスタックが空になった場合、ループフラグをリセット
    if (m_loopStack.empty()) {
        m_inLoop = false;
    }
    
    return loop_info;
}

void BytecodeGenerator::beginScope(ScopeInfo::Type type, bool strict_mode) {
    // 新しいスコープを作成してスタックに追加
    ScopeInfo scope(type, strict_mode);
    
    // 親スコープから厳格モードを継承
    if (!m_scopeStack.empty()) {
        scope.strict_mode = scope.strict_mode || m_scopeStack.top().strict_mode;
    }
    
    m_scopeStack.push(scope);
}

void BytecodeGenerator::endScope() {
    if (m_scopeStack.empty()) {
        // エラー: 対応するbeginScopeが存在しない
        return;
    }
    
    // スコープスタックから最上位のスコープを取り除く
    m_scopeStack.pop();
}

int BytecodeGenerator::declareVariable(const std::string& name, bool is_const) {
    if (m_scopeStack.empty()) {
        // エラー: スコープスタックが空
        return -1;
    }
    
    // 現在のスコープの変数マップから変数のインデックスを取得
    auto& variables = m_scopeStack.top().variables;
    auto it = variables.find(name);
    
    if (it != variables.end()) {
        // 変数がすでに存在する場合は既存のインデックスを返す
        return it->second;
    }
    
    // 新しい変数を宣言
    int index = static_cast<int>(variables.size());
    variables[name] = index;
    
    return index;
}

std::pair<int, bool> BytecodeGenerator::resolveVariable(const std::string& name) {
    if (m_scopeStack.empty()) {
        // エラー: スコープスタックが空
        return std::make_pair(-1, false);
    }
    
    // スコープスタックを上から順に探索
    auto scope_stack_copy = m_scopeStack;
    bool is_global = false;
    
    while (!scope_stack_copy.empty()) {
        auto& scope = scope_stack_copy.top();
        auto& variables = scope.variables;
        auto it = variables.find(name);
        
        if (it != variables.end()) {
            // 変数が見つかった場合、インデックスとグローバル変数かどうかのフラグを返す
            is_global = (scope.type == ScopeInfo::Type::Global);
            return std::make_pair(it->second, is_global);
        }
        
        scope_stack_copy.pop();
    }
    
    // 変数が見つからない場合、グローバル変数として新しく宣言
    auto& global_scope = m_scopeStack.front();
    auto& variables = global_scope.variables;
    int index = static_cast<int>(variables.size());
    variables[name] = index;
    
    return std::make_pair(index, true);
}

// NodeVisitorインターフェースの実装

// Program
std::shared_ptr<Node> BytecodeGenerator::visitProgram(ast::ProgramNode* node) {
    // モジュールモードの場合は厳格モードを有効にする
    if (node->isModule()) {
        m_scopeStack.top().strict_mode = true;
    }
    
    // プログラム本体の各ステートメントを処理
    for (const auto& statement : node->getBody()) {
        statement->accept(this);
    }
    
    // 明示的なreturnがない場合はundefinedを返す
    emitInstruction(Opcode::LOAD_UNDEFINED, 0, 0, node->getLocation());
    emitInstruction(Opcode::RET, 0, 0, node->getLocation());
    
    return nullptr;
}

// BlockStatement
std::shared_ptr<Node> BytecodeGenerator::visitBlockStatement(ast::BlockStatementNode* node) {
    // 新しいブロックスコープを開始
    beginScope(ScopeInfo::Type::Block, m_scopeStack.top().strict_mode);
    
    // ブロック内の各ステートメントを処理
    for (const auto& statement : node->getBody()) {
        statement->accept(this);
    }
    
    // ブロックスコープを終了
    endScope();
    
    return nullptr;
}

// ExpressionStatement
std::shared_ptr<Node> BytecodeGenerator::visitExpressionStatement(ast::ExpressionStatementNode* node) {
    // 式を評価
    node->getExpression()->accept(this);
    
    // 式の結果が必要ない場合はPOPを追加
    if (!m_needsResult) {
        emitInstruction(Opcode::POP, 0, 0, node->getLocation());
    }
    
    return nullptr;
}

// IfStatement
std::shared_ptr<Node> BytecodeGenerator::visitIfStatement(ast::IfStatementNode* node) {
    // テスト式を評価
    node->getTest()->accept(this);
    
    // テスト結果が偽の場合にelse部分またはif文の終了にジャンプ
    uint32_t jumpToElse = emitJump(Opcode::JUMP_IF_FALSE, node->getLocation());
    
    // 条件が真の場合に実行する部分
    node->getConsequent()->accept(this);
    
    // else部分があるかどうかでジャンプ処理を分岐
    if (node->getAlternate()) {
        // if部分実行後にelse部分をスキップするジャンプ
        uint32_t jumpToEnd = emitJump(Opcode::JUMP, node->getLocation());
        
        // else部分の開始位置
        uint32_t elsePos = static_cast<uint32_t>(m_instructions.size());
        
        // elseへのジャンプをパッチ
        patchJump(jumpToElse, elsePos);
        
        // else部分を実行
        node->getAlternate()->accept(this);
        
        // if-else文の終了位置
        uint32_t endPos = static_cast<uint32_t>(m_instructions.size());
        
        // 終了位置へのジャンプをパッチ
        patchJump(jumpToEnd, endPos);
    } else {
        // if文の終了位置
        uint32_t endPos = static_cast<uint32_t>(m_instructions.size());
        
        // elseがない場合は直接終了位置にジャンプ
        patchJump(jumpToElse, endPos);
    }
    
    return nullptr;
}

// ForStatement
std::shared_ptr<Node> BytecodeGenerator::visitForStatement(ast::ForStatementNode* node) {
    // 新しいブロックスコープを開始
    beginScope(ScopeInfo::Type::Block, m_scopeStack.top().strict_mode);
    
    // 初期化部分があれば実行
    if (node->getInit()) {
        node->getInit()->accept(this);
    }
    
    // ループの開始位置
    uint32_t loopStart = static_cast<uint32_t>(m_instructions.size());
    
    // ループ情報をスタックに追加
    beginLoop(loopStart);
    
    // テスト式がある場合
    uint32_t jumpToEnd = 0;
    if (node->getTest()) {
        // テスト式を評価
        node->getTest()->accept(this);
        
        // テスト結果が偽の場合にループを抜ける
        jumpToEnd = emitJump(Opcode::JUMP_IF_FALSE, node->getLocation());
    }
    
    // ループ本体を実行
    node->getBody()->accept(this);
    
    // 更新式がある場合
    uint32_t updatePos = static_cast<uint32_t>(m_instructions.size());
    if (node->getUpdate()) {
        // 更新式を評価
        node->getUpdate()->accept(this);
        
        // 結果は使わないのでスタックからポップ
        emitInstruction(Opcode::POP, 0, 0, node->getLocation());
    }
    
    // ループの先頭に戻る
    emitInstruction(Opcode::JUMP, loopStart, 0, node->getLocation());
    
    // ループの終了位置
    uint32_t endPos = static_cast<uint32_t>(m_instructions.size());
    
    // テスト式がある場合はジャンプをパッチ
    if (node->getTest()) {
        patchJump(jumpToEnd, endPos);
    }
    
    // ループ情報を取得
    auto loop_info = endLoop();
    
    // break命令のジャンプ先をパッチ
    for (uint32_t break_jump : loop_info.second) {
        patchJump(break_jump, endPos);
    }
    
    // ブロックスコープを終了
    endScope();
    
    return nullptr;
}

// WhileStatement
std::shared_ptr<Node> BytecodeGenerator::visitWhileStatement(ast::WhileStatementNode* node) {
    // ループの開始位置
    uint32_t loopStart = static_cast<uint32_t>(m_instructions.size());
    
    // ループ情報をスタックに追加
    beginLoop(loopStart);
    
    // テスト式を評価
    node->getTest()->accept(this);
    
    // テスト結果が偽の場合にループを抜ける
    uint32_t jumpToEnd = emitJump(Opcode::JUMP_IF_FALSE, node->getLocation());
    
    // ループ本体を実行
    node->getBody()->accept(this);
    
    // ループの先頭に戻る
    emitInstruction(Opcode::JUMP, loopStart, 0, node->getLocation());
    
    // ループの終了位置
    uint32_t endPos = static_cast<uint32_t>(m_instructions.size());
    
    // ジャンプをパッチ
    patchJump(jumpToEnd, endPos);
    
    // ループ情報を取得
    auto loop_info = endLoop();
    
    // break命令のジャンプ先をパッチ
    for (uint32_t break_jump : loop_info.second) {
        patchJump(break_jump, endPos);
    }
    
    return nullptr;
}

// DoWhileStatement
std::shared_ptr<Node> BytecodeGenerator::visitDoWhileStatement(ast::DoWhileStatementNode* node) {
    // ループの開始位置
    uint32_t loopStart = static_cast<uint32_t>(m_instructions.size());
    
    // ループ情報をスタックに追加
    beginLoop(loopStart);
    
    // ループ本体を実行
    node->getBody()->accept(this);
    
    // テスト式を評価
    node->getTest()->accept(this);
    
    // テスト結果が真の場合にループの先頭に戻る
    emitInstruction(Opcode::JUMP_IF_TRUE, loopStart, 0, node->getLocation());
    
    // ループの終了位置
    uint32_t endPos = static_cast<uint32_t>(m_instructions.size());
    
    // ループ情報を取得
    auto loop_info = endLoop();
    
    // break命令のジャンプ先をパッチ
    for (uint32_t break_jump : loop_info.second) {
        patchJump(break_jump, endPos);
    }
    
    return nullptr;
}

// ReturnStatement
std::shared_ptr<Node> BytecodeGenerator::visitReturnStatement(ast::ReturnStatementNode* node) {
    if (node->getArgument()) {
        // 戻り値がある場合は評価して返す
        node->getArgument()->accept(this);
    } else {
        // 戻り値がない場合はundefinedを返す
        emitInstruction(Opcode::LOAD_UNDEFINED, 0, 0, node->getLocation());
    }
    
    // return命令
    emitInstruction(Opcode::RET, 0, 0, node->getLocation());
    
    return nullptr;
}

// BreakStatement
std::shared_ptr<Node> BytecodeGenerator::visitBreakStatement(ast::BreakStatementNode* node) {
    if (!m_inLoop && !m_inSwitch) {
        // エラー: ループまたはswitch文の外側でbreak
        // エラー処理…
        return nullptr;
    }
    
    // ブレーク先（ループの終了位置）へのジャンプ
    uint32_t jumpToEnd = emitJump(Opcode::JUMP, node->getLocation());
    
    // ジャンプをループスタックに記録
    if (!m_loopStack.empty()) {
        m_loopStack.top().second.push_back(jumpToEnd);
    }
    
    return nullptr;
}

// ContinueStatement
std::shared_ptr<Node> BytecodeGenerator::visitContinueStatement(ast::ContinueStatementNode* node) {
    if (!m_inLoop) {
        // エラー: ループの外側でcontinue
        // エラー処理…
        return nullptr;
    }
    
    // 現在のループの先頭へジャンプ
    if (!m_loopStack.empty()) {
        uint32_t loopStart = m_loopStack.top().first;
        emitInstruction(Opcode::JUMP, loopStart, 0, node->getLocation());
    }
    
    return nullptr;
}

// EmptyStatement
std::shared_ptr<Node> BytecodeGenerator::visitEmptyStatement(ast::EmptyStatementNode* node) {
    // 何もしない
    return nullptr;
}

// Identifier
std::shared_ptr<Node> BytecodeGenerator::visitIdentifier(ast::IdentifierNode* node) {
    // 変数名を解決
    auto [index, is_global] = resolveVariable(node->getName());
    
    if (index < 0) {
        // エラー: 変数が見つからない
        // エラー処理…
        return nullptr;
    }
    
    // グローバル変数またはローカル変数に応じた変数読み込み命令
    if (is_global) {
        emitInstruction(Opcode::LOAD_GLOBAL, static_cast<uint32_t>(index), 0, node->getLocation());
    } else {
        emitInstruction(Opcode::LOAD_LOCAL, static_cast<uint32_t>(index), 0, node->getLocation());
    }
    
    return nullptr;
}

// Literal
std::shared_ptr<Node> BytecodeGenerator::visitLiteral(ast::LiteralNode* node) {
    const auto& value = node->getValue();
    
    // リテラルの種類に応じて適切な命令を生成
    switch (node->getLiteralType()) {
        case ast::LiteralType::Null:
            emitInstruction(Opcode::LOAD_NULL, 0, 0, node->getLocation());
            break;
            
        case ast::LiteralType::Boolean:
            if (value.getBool()) {
                emitInstruction(Opcode::LOAD_TRUE, 0, 0, node->getLocation());
            } else {
                emitInstruction(Opcode::LOAD_FALSE, 0, 0, node->getLocation());
            }
            break;
            
        case ast::LiteralType::Number: {
            double num = value.getNumber();
            
            // 小さな整数値は即値として最適化
            if (num == 0.0) {
                emitInstruction(Opcode::LOAD_ZERO, 0, 0, node->getLocation());
            } else if (num == 1.0) {
                emitInstruction(Opcode::LOAD_ONE, 0, 0, node->getLocation());
            } else if (num == -1.0) {
                emitInstruction(Opcode::LOAD_NEG_ONE, 0, 0, node->getLocation());
            } else if (isnan(num)) {
                emitInstruction(Opcode::LOAD_NAN, 0, 0, node->getLocation());
            } else if (std::isinf(num)) {
                if (num > 0) {
                    emitInstruction(Opcode::LOAD_INFINITY, 0, 0, node->getLocation());
                } else {
                    emitInstruction(Opcode::LOAD_NEG_INFINITY, 0, 0, node->getLocation());
                }
            } else {
                // 定数プールに追加してロード
                uint32_t const_index = m_constantPool->addNumber(num);
                emitInstruction(Opcode::LOAD_CONST, const_index, 0, node->getLocation());
            }
            break;
        }
            
        case ast::LiteralType::String: {
            // 定数プールに文字列を追加
            uint32_t const_index = m_constantPool->addString(value.getString());
            emitInstruction(Opcode::LOAD_CONST, const_index, 0, node->getLocation());
            break;
        }
            
        case ast::LiteralType::BigInt: {
            // 定数プールにBigIntを追加
            uint32_t const_index = m_constantPool->addBigInt(value.getString());
            emitInstruction(Opcode::LOAD_CONST, const_index, 0, node->getLocation());
            break;
        }
            
        case ast::LiteralType::RegExp: {
            // 正規表現リテラルはRegExpオブジェクトを生成する命令に変換
            auto pattern = node->getRegExpPattern();
            auto flags = node->getRegExpFlags();
            
            uint32_t pattern_index = m_constantPool->addString(pattern);
            uint32_t flags_index = m_constantPool->addString(flags);
            
            emitInstruction(Opcode::CREATE_REGEXP, pattern_index, flags_index, node->getLocation());
            break;
        }
            
        default:
            // エラー: 未知のリテラル型
            // エラー処理…
            break;
    }
    
    return nullptr;
}

// BinaryExpression
std::shared_ptr<Node> BytecodeGenerator::visitBinaryExpression(ast::BinaryExpressionNode* node) {
    // 演算子に応じた命令を選択
    Opcode opcode;
    
    switch (node->getOperator()) {
        case ast::BinaryOperator::Add:
            opcode = Opcode::ADD;
            break;
        case ast::BinaryOperator::Subtract:
            opcode = Opcode::SUB;
            break;
        case ast::BinaryOperator::Multiply:
            opcode = Opcode::MUL;
            break;
        case ast::BinaryOperator::Divide:
            opcode = Opcode::DIV;
            break;
        case ast::BinaryOperator::Modulo:
            opcode = Opcode::MOD;
            break;
        case ast::BinaryOperator::Exponentiation:
            opcode = Opcode::POW;
            break;
        case ast::BinaryOperator::BitwiseAnd:
            opcode = Opcode::BIT_AND;
            break;
        case ast::BinaryOperator::BitwiseOr:
            opcode = Opcode::BIT_OR;
            break;
        case ast::BinaryOperator::BitwiseXor:
            opcode = Opcode::BIT_XOR;
            break;
        case ast::BinaryOperator::LeftShift:
            opcode = Opcode::SHL;
            break;
        case ast::BinaryOperator::RightShift:
            opcode = Opcode::SHR;
            break;
        case ast::BinaryOperator::UnsignedRightShift:
            opcode = Opcode::USHR;
            break;
        case ast::BinaryOperator::Equal:
            opcode = Opcode::EQ;
            break;
        case ast::BinaryOperator::NotEqual:
            opcode = Opcode::NE;
            break;
        case ast::BinaryOperator::StrictEqual:
            opcode = Opcode::STRICT_EQ;
            break;
        case ast::BinaryOperator::StrictNotEqual:
            opcode = Opcode::STRICT_NE;
            break;
        case ast::BinaryOperator::LessThan:
            opcode = Opcode::LT;
            break;
        case ast::BinaryOperator::LessThanOrEqual:
            opcode = Opcode::LE;
            break;
        case ast::BinaryOperator::GreaterThan:
            opcode = Opcode::GT;
            break;
        case ast::BinaryOperator::GreaterThanOrEqual:
            opcode = Opcode::GE;
            break;
        case ast::BinaryOperator::In:
            opcode = Opcode::IN;
            break;
        case ast::BinaryOperator::InstanceOf:
            opcode = Opcode::INSTANCEOF;
            break;
        default:
            // エラー: 未知の二項演算子
            // エラー処理…
            return nullptr;
    }
    
    // 左辺と右辺を評価
    node->getLeft()->accept(this);
    node->getRight()->accept(this);
    
    // 二項演算命令を追加
    emitInstruction(opcode, 0, 0, node->getLocation());
    
    return nullptr;
}

// CallExpression
std::shared_ptr<Node> BytecodeGenerator::visitCallExpression(ast::CallExpressionNode* node) {
    // 関数オブジェクトを評価
    node->getCallee()->accept(this);
    
    // 引数を評価して各引数をスタックにプッシュ
    uint32_t argc = 0;
    for (const auto& arg : node->getArguments()) {
        arg->accept(this);
        argc++;
    }
    
    // 関数呼び出し命令
    emitInstruction(Opcode::CALL, argc, 0, node->getLocation());
    
    return nullptr;
}

// その他のノード訪問関数も同様に実装...

} // namespace core
} // namespace aerojs 