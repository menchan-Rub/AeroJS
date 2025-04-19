// === src/core/parser/ast/nodes/expressions/super.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

/**
 * @class Super
 * @brief `super` キーワードを表す AST ノード
 */
class Super final : public Node { // Not an Expression directly, used within Call/MemberExpression
public:
    Super(const SourceLocation& location, Node* parent = nullptr);
    ~Super() override = default;

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_SUPER_H 