// === src/core/parser/ast/nodes/expressions/function_expression.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H

#include "../node.h"
#include "../declarations/variable_declaration.h" // For FunctionParameterList concept
#include <vector>
#include <memory>
#include <optional>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class Identifier;
class BlockStatement;
class PatternNode; // Function parameters are patterns
class ExpressionNode; // Arrow function body can be an expression

// Common structure for Function-like nodes (Declaration, Expression, Arrow)
// Consider creating a common base or helper if significant overlap exists
struct FunctionData {
    std::optional<std::unique_ptr<Identifier>> id; // Optional name for FunctionExpression
    std::vector<std::unique_ptr<PatternNode>> params;
    std::unique_ptr<Node> body; // BlockStatement or Expression (for Arrow)
    bool generator = false;
    bool async = false;
    bool expression = false; // True for Arrow function with expression body
};

/**
 * @class FunctionExpression
 * @brief 関数式を表す AST ノード (例: `const fn = function(a) { return a; }`)
 */
class FunctionExpression final : public ExpressionNode {
public:
    FunctionExpression(FunctionData data,
                       const SourceLocation& location,
                       Node* parent = nullptr);
    ~FunctionExpression() override = default;

    // --- Getters ---
    const std::optional<std::unique_ptr<Identifier>>& getId() const;
    const std::vector<std::unique_ptr<PatternNode>>& getParams() const;
    const BlockStatement& getBody() const; // FunctionExpression always has BlockStatement
    BlockStatement& getBody();
    bool isGenerator() const;
    bool isAsync() const;

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (id?, params, body)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    FunctionData m_data;
};

/**
 * @class ArrowFunctionExpression
 * @brief アロー関数式を表す AST ノード (例: `(a) => a + 1`, `async () => {}`)
 */
class ArrowFunctionExpression final : public ExpressionNode {
public:
     ArrowFunctionExpression(FunctionData data, // id is always nullopt here
                            const SourceLocation& location,
                            Node* parent = nullptr);
    ~ArrowFunctionExpression() override = default;

    // --- Getters ---
    const std::vector<std::unique_ptr<PatternNode>>& getParams() const;
    const Node& getBody() const; // BlockStatement or Expression
    Node& getBody();
    bool isGenerator() const; // Always false for arrow functions
    bool isAsync() const;
    bool isExpression() const; // Body is an Expression, not BlockStatement

    // Visitor パターンを受け入れる
    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;

    // 子ノードを取得 (params, body)
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;

    // JSON 表現を生成
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
     FunctionData m_data; // id is always nullopt
};


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_FUNCTION_EXPRESSION_H 