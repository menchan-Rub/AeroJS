/**
 * @file program.h
 * @brief AeroJS AST のプログラムノードクラス定義
 * @author AeroJS開発チーム
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license Apache License 2.0
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "nlohmann/json_fwd.hpp"
#include "node.h"

// 前方宣言
namespace aerojs::core::parser::visitors {
class AstVisitor;
class ConstAstVisitor;
}

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @enum ProgramType
 * @brief JavaScriptプログラムの種類を示す列挙型
 */
enum class ProgramType : uint8_t {
  Script,  ///< 通常のJavaScriptスクリプト
  Module   ///< ECMAScriptモジュール（常にstrict mode）
};

/**
 * @brief ProgramTypeを文字列表現に変換
 * @param type 変換対象のProgramType値
 * @return 対応する文字列
 */
[[nodiscard]] inline const char* programTypeToString(ProgramType type) noexcept {
  switch (type) {
    case ProgramType::Script:
      return "Script";
    case ProgramType::Module:
      return "Module";
    default:
      assert(false && "無効なProgramType値です");
      return "Unknown";
  }
}

/**
 * @class Program
 * @brief AST（抽象構文木）のルートノード
 * 
 * JavaScriptプログラム全体を表現するノードで、トップレベルの文やディレクティブの
 * リストを保持し、プログラムの種類（ScriptまたはModule）を区別します。
 * ESTree仕様のProgramノードに対応します。
 */
class Program final : public Node {
 public:
  /**
   * @brief Programノードを構築
   * @param location ソースコード上の位置情報
   * @param body プログラムのトップレベル文のリスト
   * @param type プログラムの種類
   * @param parent 親ノードへのポインタ（通常はnullptr）
   */
  explicit Program(const SourceLocation& location,
                   std::vector<NodePtr>&& body,
                   ProgramType type,
                   Node* parent = nullptr) noexcept;

  /**
   * @brief デストラクタ
   */
  ~Program() override = default;

  // コピー禁止、ムーブ許可
  Program(const Program&) = delete;
  Program& operator=(const Program&) = delete;
  Program(Program&& other) noexcept = default;
  Program& operator=(Program&& other) noexcept = default;

  // Node インターフェースのオーバーライド
  [[nodiscard]] NodeType getType() const noexcept override final {
    return NodeType::Program;
  }

  void accept(visitors::AstVisitor* visitor) override final;
  void accept(visitors::ConstAstVisitor* visitor) const override final;
  [[nodiscard]] std::vector<Node*> getChildren() override final;
  [[nodiscard]] std::vector<const Node*> getChildren() const override final;
  [[nodiscard]] nlohmann::json toJson(bool pretty = false) const override final;
  [[nodiscard]] std::string toString() const override final;

  // Program固有のメソッド
  [[nodiscard]] ProgramType getProgramType() const noexcept {
    return m_programType;
  }

  [[nodiscard]] std::vector<NodePtr>& getBody() noexcept {
    return m_body;
  }

  [[nodiscard]] const std::vector<NodePtr>& getBody() const noexcept {
    return m_body;
  }

 private:
  /** プログラム本体（トップレベル文のリスト） */
  std::vector<NodePtr> m_body;

  /** プログラムの種類（ScriptまたはModule） */
  ProgramType m_programType;
};

// コンストラクタの実装
inline Program::Program(const SourceLocation& location,
                        std::vector<NodePtr>&& body,
                        ProgramType type,
                        Node* parent) noexcept
    : Node(NodeType::Program, location, parent),
      m_body(std::move(body)),
      m_programType(type) {
  // 子ノードの親ポインタを設定
  for (const auto& node_ptr : m_body) {
    if (node_ptr) {
      node_ptr->setParent(this);
    }
  }
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs