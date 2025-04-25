/**
 * @file array_expression.cpp
 * @brief AST (抽象構文木) の ArrayExpression ノードの実装
 * @author AeroJS 開発チーム
 * @copyright Copyright (c) 2024 AeroJS プロジェクト
 * @license MIT ライセンス
 * @details JavaScript の配列リテラルを表現する AST ノードの実装。
 *          配列要素には式、スプレッド構文、または空要素(Elision)が含まれる。
 */

#include "array_expression.h"

#include <algorithm>     // std::transform 用
#include <fmt/format.h>  // 文字列フォーマット
#include <fmt/ranges.h>  // 配列フォーマット
#include <iterator>      // std::back_inserter 用
#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../visitors/ast_visitor.h"
#include "../../visitors/const_ast_visitor.h"
#include "../node_utils.h"
#include "spread_element.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

ArrayExpression::ArrayExpression(std::vector<ElementType> elements,
                                 const SourceLocation& location,
                                 Node* parent)
    : ExpressionNode(NodeType::ArrayExpression, location, parent),
      m_elements(std::move(elements)) {
  // ソースロケーションの検証
  if (!location.isValid()) {
#ifndef NDEBUG
    fprintf(stderr, "警告: ArrayExpressionに無効なソースロケーションが指定されました\n");
#endif
  }

  // 各要素の親ノードを設定
  for (auto& element : m_elements) {
    if (element) {
      element->setParent(this);
    }
  }
}

[[nodiscard]] const std::vector<ArrayExpression::ElementType>& ArrayExpression::getElements() const noexcept {
  return m_elements;
}

void ArrayExpression::accept(AstVisitor& visitor) {
  visitor.Visit(*this);
}

void ArrayExpression::accept(ConstAstVisitor& visitor) const {
  visitor.Visit(*this);
}

[[nodiscard]] std::vector<Node*> ArrayExpression::getChildren() {
  std::vector<Node*> children;
  children.reserve(m_elements.size());

  for (const auto& element : m_elements) {
    if (element) {
      children.push_back(element.get());
    }
  }
  return children;
}

[[nodiscard]] std::vector<const Node*> ArrayExpression::getChildren() const {
  std::vector<const Node*> children;
  children.reserve(m_elements.size());

  for (const auto& element : m_elements) {
    if (element) {
      children.push_back(element.get());
    }
  }
  return children;
}

[[nodiscard]] nlohmann::json ArrayExpression::toJson(bool pretty) const {
  nlohmann::json jsonNode;
  baseJson(jsonNode);

  jsonNode["elements"] = nlohmann::json::array();

  for (const auto& element : m_elements) {
    if (element) {
      jsonNode["elements"].push_back(element->toJson(pretty));
    } else {
      jsonNode["elements"].push_back(nullptr);
    }
  }

  return jsonNode;
}

[[nodiscard]] std::string ArrayExpression::toString() const {
  auto element_to_string = [](const ElementType& elem) -> std::string {
    return elem ? elem->toString() : "null";
  };

  std::vector<std::string> element_strings;
  element_strings.reserve(m_elements.size());
  std::transform(m_elements.begin(), m_elements.end(),
                 std::back_inserter(element_strings),
                 element_to_string);

  return fmt::format("ArrayExpression<elements: [{}]>", fmt::join(element_strings, ", "));
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs