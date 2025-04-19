// === src/core/parser/ast/nodes/expressions/meta_property.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H

#include "../node.h"
#include <memory>
#include <string>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class Identifier;

/**
 * @class MetaProperty
 * @brief メタプロパティを表す AST ノード (例: `new.target`, `import.meta`)
 */
class MetaProperty final : public ExpressionNode {
public:
    MetaProperty(std::unique_ptr<Identifier> meta,
                 std::unique_ptr<Identifier> property,
                 const SourceLocation& location,
                 Node* parent = nullptr);
    ~MetaProperty() override = default;

    const Identifier& getMeta() const;
    Identifier& getMeta();
    const Identifier& getProperty() const;
    Identifier& getProperty();

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<Identifier> m_meta;     // e.g., 'new' or 'import'
    std::unique_ptr<Identifier> m_property; // e.g., 'target' or 'meta'
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_META_PROPERTY_H 