#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARROW_FUNCTION_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARROW_FUNCTION_EXPRESSION_H

#include <memory>
#include <nlohmann/json.hpp>  // JSON support
#include <vector>

#include "src/core/parser/ast/location.h"
#include "src/core/parser/ast/nodes/expressions/expression.h"
#include "src/core/parser/ast/nodes/node.h"                        // NodePtr と Forward declaration
#include "src/core/parser/ast/nodes/patterns/pattern.h"            // PatternPtr
#include "src/core/parser/ast/nodes/statements/block_statement.h"  // BlockStatementPtr
#include "src/core/parser/ast/visitor/ast_visitor.h"               // ASTVisitor

namespace aerojs::parser::ast {

// Forward declaration
class Identifier;
class BlockStatement;
class Pattern;
class ExpressionNode;  // Forward declare ExpressionNode

/**
 * @brief Represents an Arrow Function Expression node in the AST.
 *
 * Arrow Function Expressions (=>) are a concise syntax for writing function expressions.
 * They lexically bind the `this` value. Arrow functions can be async, but not generators.
 * The body can either be a single expression (concise body) or a block statement (block body).
 *
 * Specification: ECMAScript 2015 (6th Edition) - 14.2 Arrow Function Definitions
 *                ECMAScript 2017 (8th Edition) - Async Arrow Functions
 * Example: `(a, b) => a + b` (concise body)
 *          `x => { return x * x; }` (block body)
 *          `async () => await fetch('/api')` (async concise body)
 *          `async () => { await delay(1000); return 42; }` (async block body)
 */
class ArrowFunctionExpression final : public ExpressionNode {
 public:
  using ArrowFunctionExpressionPtr = std::unique_ptr<ArrowFunctionExpression>;
  using IdentifierPtr = std::unique_ptr<Identifier>;  // Although arrow functions don't have an ID, keep for consistency? No, remove.
  using BlockStatementPtr = std::unique_ptr<BlockStatement>;
  using ExpressionPtr = std::unique_ptr<ExpressionNode>;
  using PatternPtr = std::unique_ptr<Pattern>;

 private:
  // Arrow functions don't have an 'id'.
  std::vector<PatternPtr> params;
  // The body must be either a BlockStatement or an ExpressionNode.
  NodePtr body;
  bool async;
  // Arrow functions cannot be generators.
  bool expression;  // True if the body is an expression (concise), false if it's a block statement.

 public:
  /**
   * @brief Constructs an ArrowFunctionExpression node.
   *
   * @param loc Source code location.
   * @param params A vector of parameter patterns (Identifier or other patterns).
   * @param body The function body (either an ExpressionNode or a BlockStatement).
   * @param async Indicates if the function is async.
   * @param expression Indicates if the body is an expression (true) or a block statement (false).
   */
  ArrowFunctionExpression(
      Location loc,
      std::vector<PatternPtr>&& params,
      NodePtr&& body,
      bool async,
      bool expression);

  ~ArrowFunctionExpression() override = default;

  ArrowFunctionExpression(const ArrowFunctionExpression&) = delete;
  ArrowFunctionExpression(ArrowFunctionExpression&&) = default;
  ArrowFunctionExpression& operator=(const ArrowFunctionExpression&) = delete;
  ArrowFunctionExpression& operator=(ArrowFunctionExpression&&) = default;

  // --- Getters ---
  [[nodiscard]] const std::vector<PatternPtr>& GetParams() const noexcept;
  [[nodiscard]] const Node* GetBody() const noexcept;
  [[nodiscard]] bool IsAsync() const noexcept;
  /**
   * @brief Checks if the function body is an expression (concise body).
   * @return True if the body is an expression, false if it's a block statement.
   */
  [[nodiscard]] bool IsConciseBody() const noexcept;  // Renamed from IsExpressionBody for clarity

  // --- AST Visitor ---
  void Accept(ASTVisitor* visitor) override;

  // --- Children ---
  [[nodiscard]] std::vector<Node*> GetChildren() override;
  [[nodiscard]] const std::vector<const Node*> GetChildren() const override;

  // --- Serialization ---
  [[nodiscard]] std::string ToString(const std::string& indent = "") const override;
  [[nodiscard]] nlohmann::json ToJson() const override;

 private:
  /**
   * @brief Validates the structure of the ArrowFunctionExpression.
   *
   * Ensures parameters are valid patterns.
   * Ensures the body is either a BlockStatement or an ExpressionNode, matching the 'expression' flag.
   * Throws std::runtime_error if validation fails.
   */
  void Validate() const;
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_NODES_EXPRESSIONS_ARROW_FUNCTION_EXPRESSION_H