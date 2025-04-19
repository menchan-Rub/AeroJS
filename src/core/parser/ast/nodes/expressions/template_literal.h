// === src/core/parser/ast/nodes/expressions/template_literal.h ===
#ifndef AEROJS_PARSER_AST_NODES_EXPRESSIONS_TEMPLATE_LITERAL_H
#define AEROJS_PARSER_AST_NODES_EXPRESSIONS_TEMPLATE_LITERAL_H

#include "../node.h"
#include <vector>
#include <memory>
#include <string>

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// Forward declaration
class ExpressionNode;

/**
 * @struct TemplateElementValue
 * @brief TemplateElement の生の文字列と解釈済みの文字列
 */
struct TemplateElementValue {
    std::string cooked; ///< エスケープシーケンスが解釈された値 (不正な場合は nullopt かもしれない)
    std::string raw;    ///< ソースコード上の生の文字列
};

/**
 * @class TemplateElement
 * @brief テンプレートリテラル内の静的な文字列部分を表すノード
 */
class TemplateElement final : public Node { // Not an expression
public:
     TemplateElement(TemplateElementValue value,
                    bool tail, // この要素がテンプレートリテラルの最後か
                    const SourceLocation& location,
                    Node* parent = nullptr);
    ~TemplateElement() override = default;

    const TemplateElementValue& getValue() const;
    bool isTail() const noexcept;

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    TemplateElementValue m_value;
    bool m_tail;
};

/**
 * @class TemplateLiteral
 * @brief テンプレートリテラルを表す AST ノード (例: `` `Hello ${name}!` ``)
 */
class TemplateLiteral final : public ExpressionNode {
public:
    using ElementList = std::vector<std::unique_ptr<TemplateElement>>;
    using ExpressionList = std::vector<std::unique_ptr<ExpressionNode>>;

    TemplateLiteral(ElementList quasis, // 静的文字列部分 (TemplateElement)
                    ExpressionList expressions, // 埋め込み式部分 (Expression)
                    const SourceLocation& location,
                    Node* parent = nullptr);
    ~TemplateLiteral() override = default;

    const ElementList& getQuasis() const;
    const ExpressionList& getExpressions() const;

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    ElementList m_quasis;
    ExpressionList m_expressions; // quasis より要素数が1つ少ないはず
};

/**
 * @class TaggedTemplateExpression
 * @brief タグ付きテンプレートリテラルを表す AST ノード (例: `html`<p>Hello</p>`)
 */
class TaggedTemplateExpression final : public ExpressionNode {
public:
    TaggedTemplateExpression(std::unique_ptr<ExpressionNode> tag,
                             std::unique_ptr<TemplateLiteral> quasi,
                             const SourceLocation& location,
                             Node* parent = nullptr);
    ~TaggedTemplateExpression() override = default;

    const ExpressionNode& getTag() const;
    ExpressionNode& getTag();
    const TemplateLiteral& getQuasi() const;
    TemplateLiteral& getQuasi();

    void accept(AstVisitor& visitor) override;
    void accept(ConstAstVisitor& visitor) const override;
    std::vector<Node*> getChildren() override;
    std::vector<const Node*> getChildren() const override;
    nlohmann::json toJson(bool pretty = false) const override;
    std::string toString() const override;

private:
    std::unique_ptr<ExpressionNode> m_tag;
    std::unique_ptr<TemplateLiteral> m_quasi;
};

} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_NODES_EXPRESSIONS_TEMPLATE_LITERAL_H 