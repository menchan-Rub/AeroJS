/**
 * @file literals.cpp
 * @brief AeroJS AST のリテラルノードクラス実装
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、`literals.h` で宣言された `Literal` クラスのメソッドを実装します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#include "literals.h"

#include <iomanip>            // For std::setprecision in toString
#include <nlohmann/json.hpp>  // For JSON implementation
#include <sstream>            // For toString implementation

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Static helper function to determine LiteralKind from LiteralValue
LiteralKind Literal::determineKind(const LiteralValue& value) {
  if (std::holds_alternative<std::monostate>(value)) {
    return LiteralKind::Null;
  } else if (std::holds_alternative<bool>(value)) {
    return LiteralKind::Boolean;
  } else if (std::holds_alternative<double>(value)) {
    return LiteralKind::Numeric;
  } else if (std::holds_alternative<std::string>(value)) {
    return LiteralKind::String;
  } else if (std::holds_alternative<RegExpLiteralValue>(value)) {
    return LiteralKind::RegExp;
  } else if (std::holds_alternative<core::BigIntValuePlaceholder>(value)) {
    return LiteralKind::BigInt;
  }
  return LiteralKind::Unknown;
}

Literal::Literal(const SourceLocation& location,
                 LiteralValue&& value,
                 std::string raw,
                 Node* parent)
    : Node(NodeType::Literal, location, parent),
      m_value(std::move(value)),
      m_raw(std::move(raw)),
      m_kind(determineKind(m_value))  // Determine kind based on the moved-in value
{
}

const LiteralValue& Literal::getValue() const noexcept {
  return m_value;
}

const std::string& Literal::getRaw() const noexcept {
  return m_raw;
}

LiteralKind Literal::getKind() const noexcept {
  return m_kind;
}

void Literal::accept(AstVisitor* visitor) {
  visitor->visitLiteral(this);
}

void Literal::accept(ConstAstVisitor* visitor) const {
  visitor->visitLiteral(this);
}

std::vector<Node*> Literal::getChildren() {
  return {};  // Literals have no children
}

std::vector<const Node*> Literal::getChildren() const {
  return {};  // Literals have no children
}

nlohmann::json Literal::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);
  jsonNode["raw"] = m_raw;

  // Add value based on kind
  std::visit([&jsonNode](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::monostate>) {
      jsonNode["value"] = nullptr;
    } else if constexpr (std::is_same_v<T, bool>) {
      jsonNode["value"] = arg;
    } else if constexpr (std::is_same_v<T, double>) {
      jsonNode["value"] = arg;
    } else if constexpr (std::is_same_v<T, std::string>) {
      jsonNode["value"] = arg;
    } else if constexpr (std::is_same_v<T, RegExpLiteralValue>) {
      jsonNode["value"] = nullptr;  // Value is complex, represented by regex field
      jsonNode["regex"] = {
          {"pattern", arg.pattern},
          {"flags", arg.flags}};
    } else if constexpr (std::is_same_v<T, core::BigIntValuePlaceholder>) {
      // For BigInt, ESTree spec often uses a string representation in 'bigint'
      jsonNode["value"] = nullptr;
      jsonNode["bigint"] = arg.value;
    }
  },
             m_value);

  return jsonNode;
}

std::string Literal::toString() const {
  std::ostringstream oss;
  oss << "Literal[";

  std::visit([&oss](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::monostate>) {
      oss << "null";
    } else if constexpr (std::is_same_v<T, bool>) {
      oss << (arg ? "true" : "false");
    } else if constexpr (std::is_same_v<T, double>) {
      // Ensure sufficient precision for doubles
      oss << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << arg;
    } else if constexpr (std::is_same_v<T, std::string>) {
      // Simple string representation for debugging, might need escaping for complex strings
      oss << '\"' << arg << '\"';
    } else if constexpr (std::is_same_v<T, RegExpLiteralValue>) {
      oss << "/" << arg.pattern << "/" << arg.flags;
    } else if constexpr (std::is_same_v<T, core::BigIntValuePlaceholder>) {
      oss << arg.value << "n";
    }
  },
             m_value);

  oss << "]";
  return oss.str();
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs