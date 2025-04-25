/**
 * @file program.cpp
 * @brief AeroJS 抽象構文木 (AST) Program ノードクラスの実装ファイル
 * @author AeroJS 開発者一同
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、program.h ヘッダーファイルで宣言された Program クラスのメソッドを実装します。
 * Program ノードは、JavaScript コード全体のルートを表し、スクリプトまたはモジュールのいずれかのタイプを持ちます。
 *
 * コーディング規約: AeroJS コーディング規約 バージョン 1.2 に準拠
 */

#include "program.h"  // Program クラスの宣言

#include <algorithm>          // std::find_if 用
#include <memory>             // std::unique_ptr (NodePtr) 用
#include <nlohmann/json.hpp>  // JSON シリアライズ (nlohmann/json ライブラリ)
#include <sstream>            // toString 実装用 (文字列ストリーム)
#include <stdexcept>          // 例外処理用
#include <string>             // 例外メッセージ用 (std::string)
#include <string_view>        // "use strict" 比較用
#include <utility>            // std::move 用
#include <vector>             // std::vector 用

#include "expression_statement.h"  // 'use strict' ディレクティブのチェック用
#include "literal.h"               // 'use strict' ディレクティブのチェック用 (StringLiteral を想定)

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// JSON シリアライズで使う定数文字列
static constexpr const char* JSON_SOURCE_TYPE_MODULE = "module";   // JSON でのモジュールタイプ
static constexpr const char* JSON_SOURCE_TYPE_SCRIPT = "script";   // JSON でのスクリプトタイプ
static constexpr const char* JSON_KEY_SOURCE_TYPE = "sourceType";  // JSON キー: ソースタイプ
static constexpr const char* JSON_KEY_BODY = "body";               // JSON キー: プログラム本体

// toString で使う定数文字列
static constexpr const char* TO_STRING_TYPE_MODULE = "Module";  // toString でのモジュールタイプ
static constexpr const char* TO_STRING_TYPE_SCRIPT = "Script";  // toString でのスクリプトタイプ

// 'use strict' ディレクティブの文字列
static constexpr std::string_view USE_STRICT_DIRECTIVE = "use strict";  // 厳格モードを示すディレクティブ

Program::Program(const SourceLocation& location,
                 std::vector<NodePtr>&& body,
                 ProgramType type,
                 Node* parent)
    : Node(NodeType::Program, location, parent),  // 基底クラス Node のコンストラクタ呼び出し (型、位置、親を設定)
      m_body(std::move(body)),                    // body の所有権をムーブしてメンバ変数へ
      m_programType(type),                        // プログラムタイプ (Script or Module) を設定
      m_isStrict(false)                           // 厳格モードフラグを false で初期化
{
  // 不変条件: location は有効であること (Node 基底クラスで検証想定)
  // 不変条件: type は有効な ProgramType 列挙値であること

  // プログラム本体 (m_body) の各子ノードに、この Program ノードを親として設定します。
  // これで AST の親子関係が正しくなり、Visitor パターンなどで辿れるようになります。
  // 子ノードが nullptr (例: 空文) の可能性もあるため、チェックしてから設定します。
  for (auto& child_node_ptr : m_body) {
    if (child_node_ptr) {
      // 子ノードの setParent を呼び出し、この Program インスタンス (this) を親に設定
      child_node_ptr->setParent(this);
    }
    // else: 子ノードが nullptr なら、親設定は不要なので何もしない
  }

  // プログラム冒頭の 'use strict' ディレクティブを確認します (ECMAScript 仕様)。
  // Module は常に strict モードなので、このチェックは主に Script の場合に意味があります。
  if (!m_body.empty()) {
    // m_body の先頭から最初の有効な (null でない) ノードを探します。
    // (空文などが nullptr として含まれる場合があるため)
    auto first_valid_statement_it = std::find_if(
        m_body.begin(), m_body.end(),
        [](const NodePtr& node_ptr) { return node_ptr != nullptr; }  // null でないノードを見つけるラムダ
    );

    // 有効な最初の文が見つかった場合
    if (first_valid_statement_it != m_body.end()) {
      const Node* first_statement = first_valid_statement_it->get();  // スマートポインタから生ポインタ取得

      // 最初の文が ExpressionStatement か確認します。
      // ('use strict' は文字列リテラルを含む式文として表現されるため)
      if (first_statement->getNodeType() == NodeType::ExpressionStatement) {
        // NodeType を確認済みなので、効率の良い static_cast で ExpressionStatement へダウンキャストします。
        const auto* expr_stmt = static_cast<const ExpressionStatement*>(first_statement);
        const Node* expression = expr_stmt->getExpression();  // 式文から式ノードを取得

        // 式が Literal ノードか確認します。
        if (expression && expression->getNodeType() == NodeType::Literal) {
          // Literal ノードへダウンキャストします。
          const auto* literal = static_cast<const Literal*>(expression);

          // リテラルが文字列で、かつ値が "use strict" かどうかを確認します。
          // (getValueAsString は文字列リテラルの場合に std::optional<std::string_view> を返すと想定)
          const auto literal_value_opt = literal->getValueAsString();
          if (literal_value_opt.has_value() && literal_value_opt.value() == USE_STRICT_DIRECTIVE) {
            // 'use strict' ディレクティブ発見。厳格モードフラグを true に設定します。
            m_isStrict = true;
          }
        }
      }
    }
  }
  // 注意: Module (`ProgramType::Module`) は ECMAScript 仕様により常に厳格モードです。
  //       このコンストラクタでは、明示的な 'use strict' ディレクティブの有無で `m_isStrict` を設定しますが、
  //       `isStrictMode()` メソッドはプログラムタイプも考慮して最終的な厳格モード状態を返します。
  //       (つまり `m_programType` が `Module` なら `isStrictMode()` は `m_isStrict` の値に関わらず `true` を返します)
}

[[nodiscard]] ProgramType Program::getProgramType() const noexcept {
  // プログラムの種類 (Script または Module) を返します。
  // コード解析や実行時の挙動 (厳格モードなど) に影響します。
  // noexcept は例外を送出しないことを示します。
  return m_programType;
}

[[nodiscard]] bool Program::isStrictMode() const noexcept {
  // プログラムが厳格モード (Strict Mode) かどうかを返します。
  // Module なら常に true、Script なら先頭の 'use strict' ディレクティブの有無 (`m_isStrict`) によります (ECMAScript 仕様準拠)。
  // noexcept は例外を送出しないことを示します。
  if (m_programType == ProgramType::Module) {
    return true;  // モジュールは常に厳格モード
  } else {
    return m_isStrict;  // スクリプトの場合は、コンストラクタで検出したディレクティブの有無による
  }
}

std::vector<NodePtr>& Program::getBody() noexcept {
  // プログラム本体 (文や宣言のリスト) への参照を返します。
  // AST 変換などで body の内容を変更するのに使います。
  // noexcept は例外を送出しないことを示します。
  return m_body;
}

[[nodiscard]] const std::vector<NodePtr>& Program::getBody() const noexcept {
  // プログラム本体 (文や宣言のリスト) への const 参照を返します。
  // 読み取り専用アクセス用です。この参照経由で body は変更できません。
  // noexcept は例外を送出しないことを示します。
  return m_body;
}

void Program::accept(AstVisitor* visitor) {
  // Visitor パターン: ビジターにこの Program ノードを訪問 (visit) させます。
  // ビジターは visitProgram メソッドでノードに対する操作 (コード生成、意味解析など) を行います。
  if (!visitor) {
    // visitor が null でないか確認します。null の場合は不正操作を防ぐため例外を投げます。
    throw std::invalid_argument("Program::accept に渡された AstVisitor が null です。");
  }
  // ビジターの visitProgram メソッドを呼び出し、自身 (this) を渡します。
  visitor->visitProgram(this);
}

void Program::accept(ConstAstVisitor* visitor) const {
  // Visitor パターン: const ビジターにこの Program ノードを訪問 (visit) させます。
  // AST を変更しない操作 (情報収集、検証など) に使います。
  if (!visitor) {
    // visitor が null でないか確認します。null の場合は不正操作を防ぐため例外を投げます。
    throw std::invalid_argument("Program::accept (const) に渡された ConstAstVisitor が null です。");
  }
  // const ビジターの visitProgram メソッドを呼び出し、自身 (this) を渡します。
  visitor->visitProgram(this);
}

[[nodiscard]] std::vector<Node*> Program::getChildren() {
  // 子ノード (m_body の要素) の生ポインタ (Node*) のリストを返します。
  // AST を直接辿る際に便利ですが、ポインタの所有権は移動しません。
  // 呼び出し元はポインタのライフタイムを管理しません。
  // [[nodiscard]] は戻り値を使わない場合に警告を出すための属性です。

  std::vector<Node*> children;
  // パフォーマンスのため、事前に vector の容量を確保します (再確保のオーバーヘッド削減)。
  children.reserve(m_body.size());

  // m_body 内の各 NodePtr から生ポインタを取り出して children に追加します。
  for (const auto& node_ptr : m_body) {
    // node_ptr.get() で生ポインタを取得。
    // node_ptr が nullptr (例: 空文) なら nullptr がそのまま追加されます。
    // 呼び出し元は nullptr の可能性を考慮する必要があります。
    children.push_back(node_ptr.get());
  }

  // 生ポインタの vector を返します。
  return children;
}

[[nodiscard]] std::vector<const Node*> Program::getChildren() const {
  // 子ノード (m_body の要素) の const 生ポインタ (const Node*) のリストを返します。
  // 読み取り専用の走査に使います。ポインタ経由での子ノード変更はできません。
  // ポインタの所有権は移動しません。
  // [[nodiscard]] は戻り値を使わない場合に警告を出すための属性です。

  std::vector<const Node*> children;
  // パフォーマンスのため、事前に vector の容量を確保します。
  children.reserve(m_body.size());

  // m_body 内の各 NodePtr から const 生ポインタを取り出して children に追加します。
  for (const auto& node_ptr : m_body) {
    // node_ptr.get() で生ポインタを取得 (const 版なので const Node*)。
    // node_ptr が nullptr なら nullptr がそのまま追加されます。
    children.push_back(node_ptr.get());
  }

  // const 生ポインタの vector を返します。
  return children;
}

[[nodiscard]] nlohmann::json Program::toJson(bool pretty) const {
  // この Program ノードを ESTree 仕様に準拠するような JSON オブジェクトに変換します。
  // [[nodiscard]] は生成した JSON を使わない場合に警告を出すための属性です。

  nlohmann::json jsonNode;

  // 基底クラスの toJson 相当 (baseJson と仮定) を呼び出し、
  // 型 ("Program") や位置情報 (location) などを JSON に追加します。
  baseJson(jsonNode);

  // プログラムの種類 ("module" または "script") を sourceType として追加します。
  jsonNode[JSON_KEY_SOURCE_TYPE] = (m_programType == ProgramType::Module)
                                       ? JSON_SOURCE_TYPE_MODULE   // モジュールの場合
                                       : JSON_SOURCE_TYPE_SCRIPT;  // スクリプトの場合

  // プログラム本体 (body) を表す JSON 配列を準備します。
  jsonNode[JSON_KEY_BODY] = nlohmann::json::array();

  // body の各要素 (文や宣言) を再帰的に JSON に変換し、body 配列に追加します。
  for (const auto& stmt_ptr : m_body) {
    if (stmt_ptr) {
      // 子ノードが存在すれば、その toJson を再帰呼び出しして結果を追加。
      // pretty フラグは子ノードのシリアライズにも伝播させます。
      jsonNode[JSON_KEY_BODY].push_back(stmt_ptr->toJson(pretty));
    } else {
      // 子ノードが nullptr (例: 空文) の場合は JSON の null を追加します。
      // これで元のソース構造における要素の欠落などを JSON で表現できます (ESTree 準拠)。
      jsonNode[JSON_KEY_BODY].push_back(nullptr);
    }
  }

  // 完成した Program ノードの JSON オブジェクトを返します。
  // pretty フラグは、最終的に JSON を文字列にダンプする際の整形 (インデント等) に使われます。
  // この toJson メソッド内での整形は不要です。
  return jsonNode;
}

[[nodiscard]] std::string Program::toString() const {
  // デバッグやログ出力用に、この Program ノードの簡単な文字列表現を生成します。
  // [[nodiscard]] は生成した文字列を使わない場合に警告を出すための属性です。

  std::ostringstream oss;  // ostringstream で効率的に文字列を組み立てます。

  // ノードの基本情報 (種類、strict モード、要素数、位置情報) を出力します。
  oss << "Program[type="                                                                           // ノードタイプ固定文字列
      << ((m_programType == ProgramType::Module) ? TO_STRING_TYPE_MODULE : TO_STRING_TYPE_SCRIPT)  // プログラムタイプ
      << ", strict=" << (isStrictMode() ? "true" : "false")                                        // 厳格モードか (isStrictMode() の結果)
      << ", bodySize=" << m_body.size()                                                            // プログラム本体の要素数
      << ", location=" << getLocation().toString()                                                 // ソース位置 (SourceLocation::toString() があると仮定)
      << "]";                                                                                      // 閉じ括弧

  // 組み立てた文字列を返します。
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs