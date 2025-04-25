/*******************************************************************************
 * @file src/core/parser/ast/nodes/patterns/assignment_pattern.cpp
 * @brief AssignmentPattern AST ノードの実装ファイル。
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AssignmentPattern クラスのメソッド実装を提供します。
 * これには、コンストラクタ、ゲッター、検証、ビジター受け入れ、子ノード取得、
 * 文字列化、JSON シリアライズ、クローン作成が含まれます。
 ******************************************************************************/

#include "src/core/parser/ast/nodes/patterns/assignment_pattern.h"

#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "src/core/parser/ast/ast_node_base.h"  // For NodeList
#include "src/core/parser/ast/ast_visitor.h"
#include "src/core/parser/ast/nodes/expression.h"  // Include the full definition
#include "src/core/parser/error/syntax_error.h"
#include "src/utils/json_writer.h"
#include "src/utils/logging.h"

namespace aerojs::parser::ast {

AssignmentPattern::AssignmentPattern(const SourceLocation& location,
                                     std::unique_ptr<Pattern> left,
                                     std::unique_ptr<Expression> right)
    : Pattern(location),
      m_left(std::move(left)),
      m_right(std::move(right)) {
  AERO_LOG_TRACE("Creating AssignmentPattern at {}", location.ToString());
  if (!m_left) {
    AERO_LOG_ERROR("AssignmentPattern created with null left pattern at {}", location.ToString());
    throw std::invalid_argument("AssignmentPattern 'left' pattern cannot be null.");
  }
  if (!m_right) {
    AERO_LOG_ERROR("AssignmentPattern created with null right expression at {}", location.ToString());
    throw std::invalid_argument("AssignmentPattern 'right' expression cannot be null.");
  }
  // 親ポインタを設定
  m_left->SetParent(this);
  m_right->SetParent(this);
  AERO_LOG_DEBUG("AssignmentPattern created successfully at {}", location.ToString());
}

AssignmentPattern::AssignmentPattern(AssignmentPattern&& other) noexcept
    : Pattern(std::move(other)),  // 基底クラスのムーブコンストラクタを呼び出す
      m_left(std::move(other.m_left)),
      m_right(std::move(other.m_right)) {
  AERO_LOG_TRACE("Moving AssignmentPattern from {}", other.GetSourceLocation().ToString());
  // 移動元のリソースを無効化 (unique_ptr が自動で行う)
  // 移動後の子ノードの親ポインタを更新
  if (m_left) m_left->SetParent(this);
  if (m_right) m_right->SetParent(this);
}

AssignmentPattern& AssignmentPattern::operator=(AssignmentPattern&& other) noexcept {
  AERO_LOG_TRACE("Move assigning AssignmentPattern from {}", other.GetSourceLocation().ToString());
  if (this != &other) {
    Pattern::operator=(std::move(other));  // 基底クラスのムーブ代入
    m_left = std::move(other.m_left);
    m_right = std::move(other.m_right);
    // 移動後の子ノードの親ポインタを更新
    if (m_left) m_left->SetParent(this);
    if (m_right) m_right->SetParent(this);
  }
  return *this;
}

NodeType AssignmentPattern::GetType() const noexcept {
  return NodeType::ASSIGNMENT_PATTERN;
}

const Pattern& AssignmentPattern::GetLeft() const noexcept {
  // コンストラクタで null チェック済みのため、ここでは assert を使用可能
  // assert(m_left != nullptr);
  return *m_left;
}

Pattern& AssignmentPattern::GetLeft() noexcept {
  // assert(m_left != nullptr);
  return *m_left;
}

const Expression& AssignmentPattern::GetRight() const noexcept {
  // assert(m_right != nullptr);
  return *m_right;
}

Expression& AssignmentPattern::GetRight() noexcept {
  // assert(m_right != nullptr);
  return *m_right;
}

void AssignmentPattern::Accept(ASTVisitor& visitor) {
  AERO_LOG_TRACE("AssignmentPattern at {} accepting visitor", GetSourceLocation().ToString());
  visitor.VisitAssignmentPattern(*this);
}

void AssignmentPattern::Validate() const {
  AERO_LOG_TRACE("Validating AssignmentPattern at {}", GetSourceLocation().ToString());
  // 1. Check that left and right are not null (already done in constructor)
  if (!m_left) {
    throw error::SyntaxError(GetSourceLocation(), "AssignmentPattern 'left' is unexpectedly null.");
  }
  if (!m_right) {
    throw error::SyntaxError(GetSourceLocation(), "AssignmentPattern 'right' is unexpectedly null.");
  }

  // 2. Validate child nodes recursively
  AERO_LOG_DEBUG("Validating left child of AssignmentPattern");
  m_left->Validate();
  AERO_LOG_DEBUG("Validating right child of AssignmentPattern");
  m_right->Validate();
  AERO_LOG_TRACE("AssignmentPattern at {} validation successful", GetSourceLocation().ToString());
}

NodeList AssignmentPattern::GetChildren() const {
  NodeList children;
  if (m_left) {
    children.push_back(m_left.get());
  }
  if (m_right) {
    children.push_back(m_right.get());
  }
  return children;
}

std::string AssignmentPattern::ToString(const std::string& indent) const {
  std::ostringstream oss;
  oss << indent << "AssignmentPattern " << GetSourceLocation().ToString() << "\n";
  std::string childIndent = indent + "  ";
  oss << childIndent << "Left:\n";
  oss << (m_left ? m_left->ToString(childIndent + "  ") : childIndent + "  <null>\n");
  oss << childIndent << "Right:\n";
  oss << (m_right ? m_right->ToString(childIndent + "  ") : childIndent + "  <null>\n");
  return oss.str();
}

void AssignmentPattern::ToJson(utils::JsonWriter& writer) const {
  writer.StartObject();
  writer.WriteProperty("type", "AssignmentPattern");
  writer.WriteProperty("location", GetSourceLocation());

  writer.WritePropertyName("left");
  if (m_left) {
    m_left->ToJson(writer);
  } else {
    writer.WriteNull();
  }

  writer.WritePropertyName("right");
  if (m_right) {
    m_right->ToJson(writer);
  } else {
    writer.WriteNull();
  }

  writer.EndObject();
}

std::unique_ptr<Node> AssignmentPattern::Clone() const {
  AERO_LOG_TRACE("Cloning AssignmentPattern at {}", GetSourceLocation().ToString());
  if (!m_left || !m_right) {
    AERO_LOG_WARN("Cloning an AssignmentPattern with potentially null children at {}", GetSourceLocation().ToString());
    // Decide whether to throw or return a potentially invalid clone
    // Throwing might be safer to prevent propagation of invalid state
    throw std::logic_error("Cannot clone AssignmentPattern with null children");
    // Alternatively, handle cloning potentially null children:
    // PatternPtr cloned_left = m_left ? std::unique_ptr<Pattern>(static_cast<Pattern*>(m_left->Clone().release())) : nullptr;
    // ExpressionPtr cloned_right = m_right ? std::unique_ptr<Expression>(static_cast<Expression*>(m_right->Clone().release())) : nullptr;
  }

  auto cloned_left_node = m_left->Clone();
  if (!cloned_left_node) {
    throw std::runtime_error("Failed to clone left child of AssignmentPattern");
  }
  auto* cloned_left_pattern = dynamic_cast<Pattern*>(cloned_left_node.get());
  if (!cloned_left_pattern) {
    throw std::runtime_error("Cloned left child is not a Pattern");
  }
  PatternPtr cloned_left(static_cast<Pattern*>(cloned_left_node.release()));

  auto cloned_right_node = m_right->Clone();
  if (!cloned_right_node) {
    throw std::runtime_error("Failed to clone right child of AssignmentPattern");
  }
  auto* cloned_right_expression = dynamic_cast<Expression*>(cloned_right_node.get());
  if (!cloned_right_expression) {
    throw std::runtime_error("Cloned right child is not an Expression");
  }
  ExpressionPtr cloned_right(static_cast<Expression*>(cloned_right_node.release()));

  return std::make_unique<AssignmentPattern>(GetSourceLocation(), std::move(cloned_left), std::move(cloned_right));
}

}  // namespace aerojs::parser::ast