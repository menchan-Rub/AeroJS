/**
 * @file program.h
 * @brief AeroJS AST のプログラムノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、JavaScriptプログラム全体 (スクリプトまたはモジュール) を
 * 表すASTノード `Program` を定義します。
 * `Program` は、トップレベルの文のリストを保持します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_NODES_PROGRAM_H
#define AEROJS_PARSER_AST_NODES_PROGRAM_H

#include <vector>
#include <memory> // For std::vector<NodePtr>

#include "node.h"
#include "../visitors/ast_visitor.h" // For AstVisitor and ConstAstVisitor

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @enum ProgramType
 * @brief プログラムの種類（スクリプトまたはモジュール）を示す列挙型。
 */
enum class ProgramType {
    Script,  ///< 通常のスクリプト
    Module   ///< ES6 モジュール
};

/**
 * @class Program
 * @brief ASTのルートノードであり、JavaScriptプログラム全体を表すクラス。
 *
 * @details
 * このクラスは、ソースコードからパースされたJavaScriptプログラムの
 * トップレベル構造を表します。プログラムは、トップレベルの文 (Statement) の
 * シーケンスで構成されます。また、プログラムが通常のスクリプトか
 * ES6モジュールであるかの情報も保持します。
 *
 * メンバ:
 * - `m_body`: プログラムのトップレベル文を含む `std::vector<NodePtr>`。
 * - `m_programType`: プログラムの種類 (`ProgramType`)。
 *
 * メモリ管理:
 * `m_body` 内の `NodePtr` (`std::unique_ptr<Node>`) が子ノードの所有権を管理します。
 *
 * スレッド安全性:
 * `Node` クラスと同様に、`Program` オブジェクト自体はスレッドセーフではありません。
 */
class Program final : public Node {
public:
    /**
     * @brief Programノードのコンストラクタ。
     * @param location ソースコード内のこのノードの位置情報。
     * @param body プログラム本体を構成するトップレベル文のリスト (ムーブされる)。
     * @param type プログラムの種類 (Script または Module)。
     * @param parent 親ノード (通常はnullptr)。
     */
    explicit Program(const SourceLocation& location,
                     std::vector<NodePtr>&& body,
                     ProgramType type,
                     Node* parent = nullptr);

    /**
     * @brief デストラクタ (デフォルト)。
     */
    ~Program() override = default;

    // コピーとムーブのコンストラクタ/代入演算子はデフォルトを使用
    Program(const Program&) = delete; // コピー禁止
    Program& operator=(const Program&) = delete; // コピー代入禁止
    Program(Program&&) noexcept = default; // ムーブ許可
    Program& operator=(Program&&) noexcept = default; // ムーブ代入許可

    /**
     * @brief ノードタイプを取得します。
     * @return `NodeType::Program`。
     */
    NodeType getType() const noexcept override final { return NodeType::Program; }

    /**
     * @brief プログラムの種類を取得します。
     * @return `ProgramType::Script` または `ProgramType::Module`。
     */
    ProgramType getProgramType() const noexcept;

    /**
     * @brief プログラム本体の文リストを取得します (非const版)。
     * @return トップレベル文のリストへの参照 (`std::vector<NodePtr>&`)。
     */
    std::vector<NodePtr>& getBody() noexcept;

    /**
     * @brief プログラム本体の文リストを取得します (const版)。
     * @return トップレベル文のリストへのconst参照 (`const std::vector<NodePtr>&`)。
     */
    const std::vector<NodePtr>& getBody() const noexcept;

    /**
     * @brief ビジターを受け入れます (非const版)。
     * @param visitor 訪問するビジター。
     */
    void accept(AstVisitor* visitor) override final;

    /**
     * @brief ビジターを受け入れます (const版)。
     * @param visitor 訪問するconstビジター。
     */
    void accept(ConstAstVisitor* visitor) const override final;

    /**
     * @brief 子ノードのリストを取得します。
     * @return プログラム本体の文への非所有ポインタのベクター。
     */
    std::vector<Node*> getChildren() override final;

    /**
     * @brief 子ノードのリストを取得します (const版)。
     * @return プログラム本体の文へのconst非所有ポインタのベクター。
     */
    std::vector<const Node*> getChildren() const override final;

    /**
     * @brief このノードをJSON表現に変換します。
     * @param pretty フォーマットするかどうか。
     * @return ノードを表すJSONオブジェクト。
     */
    nlohmann::json toJson(bool pretty = false) const override final;

    /**
     * @brief このノードを文字列表現に変換します。
     * @return "Program[type=...]" 形式の文字列。
     */
    std::string toString() const override final;

private:
    std::vector<NodePtr> m_body;        ///< プログラムのトップレベル文
    ProgramType m_programType;          ///< プログラムの種類 (Script/Module)
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_PROGRAM_H 