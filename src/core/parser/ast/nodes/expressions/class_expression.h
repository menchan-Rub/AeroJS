// === src/core/parser/ast/nodes/expressions/class_expression.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_CLASS_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_CLASS_EXPRESSION_H

#include <memory>
#include <optional>

#include "../node.h"

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class Identifier;
class ClassBody;
class ExpressionNode;  // SuperClass can be an expression

/**
 * @class ClassExpression
 * @brief クラス式を表す AST ノード (例: `const MyClass = class NamedExpr { ... }`)
 */
class ClassExpression final : public ExpressionNode {
 public:
  ClassExpression(std::optional<std::unique_ptr<Identifier>> id,  // Optional name for the expression
                  std::optional<std::unique_ptr<ExpressionNode>> superClass,
                  std::unique_ptr<ClassBody> body,
                  const SourceLocation& location,
                  Node* parent = nullptr);
  ~ClassExpression() override = default;

  const std::optional<std::unique_ptr<Identifier>>& getId() const;
  const std::optional<std::unique_ptr<ExpressionNode>>& getSuperClass() const;
  const ClassBody& getBody() const;
  ClassBody& getBody();

  void accept(AstVisitor& visitor) override;
  void accept(ConstAstVisitor& visitor) const override;
  std::vector<Node*> getChildren() override;
  std::vector<const Node*> getChildren() const override;
  nlohmann::json toJson(bool pretty = false) const override;
  std::string toString() const override;

 private:
  std::optional<std::unique_ptr<Identifier>> m_id;
  std::optional<std::unique_ptr<ExpressionNode>> m_superClass;
  std::unique_ptr<ClassBody> m_body;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_CLASS_EXPRESSION_H