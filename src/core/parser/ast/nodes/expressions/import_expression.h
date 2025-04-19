// === src/core/parser/ast/nodes/expressions/import_expression.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H

#include "../node.h"
#include <memory>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @class ImportExpression
 * @brief 動的インポート式を表す AST ノード (例: `import('./module.js')`)
 */
class ImportExpression final : public ExpressionNode {
public:
    ImportExpression(std::unique_ptr<ExpressionNode> source,
                     const SourceLocation& location,
                     Node* parent = nullptr);
    ~ImportExpression() override = default;

    const ExpressionNode& getSource() const;
    ExpressionNode& getSource();

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<ExpressionNode> m_source; // Module specifier expression
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_IMPORT_EXPRESSION_H 