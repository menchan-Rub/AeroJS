// === src/core/parser/ast/nodes/expressions/super.cpp ===
#include "super.h"
#include "../../visitors/ast_visitor.h"
#include <nlohmann/json.hpp>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

Super::Super(const SourceLocation& location, Node* parent)
    : Node(NodeType::Super, location, parent) {}

void Super::accept(AstVisitor& visitor) { visitor.Visit(*this); }
void Super::accept(ConstAstVisitor& visitor) const { visitor.Visit(*this); }

std::vector<Node*> Super::getChildren() { return {}; }
std::vector<const Node*> Super::getChildren() const { return {}; }

nlohmann::json Super::toJson(bool pretty) const {
    nlohmann::json jsonNode;
    baseJson(jsonNode); // Only includes type and location
    return jsonNode;
}
std::string Super::toString() const { return "Super"; }

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs 