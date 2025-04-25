#include "src/core/parser/ast/nodes/node_forward_declarations.h"

namespace aerojs::parser::ast {

class ASTVisitor {
 public:
  virtual ~ASTVisitor() = default;

  // Base Node
  virtual void visit(Node& node) = 0;

  // Program
  virtual void visit(Program& node) = 0;

  // Expressions
  virtual void visit(ExpressionNode& node) = 0;
  virtual void visit(Identifier& node) = 0;
  virtual void visit(Literal& node) = 0;
  virtual void visit(ThisExpression& node) = 0;
  virtual void visit(ArrayExpression& node) = 0;
  virtual void visit(ObjectExpression& node) = 0;
  virtual void visit(Property& node) = 0;
  virtual void visit(FunctionExpression& node) = 0;
  virtual void visit(UnaryExpression& node) = 0;
  virtual void visit(UpdateExpression& node) = 0;
  virtual void visit(BinaryExpression& node) = 0;
  virtual void visit(AssignmentExpression& node) = 0;
  virtual void visit(LogicalExpression& node) = 0;
  virtual void visit(MemberExpression& node) = 0;
  virtual void visit(ConditionalExpression& node) = 0;
  virtual void visit(CallExpression& node) = 0;
  virtual void visit(NewExpression& node) = 0;
  virtual void visit(SequenceExpression& node) = 0;
  virtual void visit(ArrowFunctionExpression& node) = 0;
  virtual void visit(YieldExpression& node) = 0;
  virtual void visit(AwaitExpression& node) = 0;
  virtual void visit(TaggedTemplateExpression& node) = 0;
  virtual void visit(TemplateLiteral& node) = 0;
  virtual void visit(TemplateElement& node) = 0;
  virtual void visit(Super& node) = 0;
  virtual void visit(SpreadElement& node) = 0;
  virtual void visit(MetaProperty& node) = 0;
  virtual void visit(ParenthesizedExpression& node) = 0;
  virtual void visit(ImportExpression& node) = 0;
  virtual void visit(ChainExpression& node) = 0;

  // Statements
  virtual void visit(StatementNode& node) = 0;
  virtual void visit(EmptyStatement& node) = 0;
  virtual void visit(BlockStatement& node) = 0;
  virtual void visit(ExpressionStatement& node) = 0;
  virtual void visit(IfStatement& node) = 0;
  virtual void visit(LabeledStatement& node) = 0;
  virtual void visit(BreakStatement& node) = 0;
  virtual void visit(ContinueStatement& node) = 0;
  virtual void visit(WithStatement& node) = 0;
  virtual void visit(SwitchStatement& node) = 0;
  virtual void visit(SwitchCase& node) = 0;
  virtual void visit(ReturnStatement& node) = 0;
  virtual void visit(ThrowStatement& node) = 0;
  virtual void visit(TryStatement& node) = 0;
  virtual void visit(CatchClause& node) = 0;
  virtual void visit(WhileStatement& node) = 0;
  virtual void visit(DoWhileStatement& node) = 0;
  virtual void visit(ForStatement& node) = 0;
  virtual void visit(ForInStatement& node) = 0;
  virtual void visit(ForOfStatement& node) = 0;
  virtual void visit(DebuggerStatement& node) = 0;

  // Declarations
  virtual void visit(DeclarationNode& node) = 0;
  virtual void visit(FunctionDeclaration& node) = 0;
  virtual void visit(VariableDeclaration& node) = 0;
  virtual void visit(VariableDeclarator& node) = 0;
  virtual void visit(ClassDeclaration& node) = 0;
  virtual void visit(ClassBody& node) = 0;
  virtual void visit(MethodDefinition& node) = 0;
  virtual void visit(ImportDeclaration& node) = 0;
  virtual void visit(ImportSpecifier& node) = 0;
  virtual void visit(ImportDefaultSpecifier& node) = 0;
  virtual void visit(ImportNamespaceSpecifier& node) = 0;
  virtual void visit(ExportNamedDeclaration& node) = 0;
  virtual void visit(ExportDefaultDeclaration& node) = 0;
  virtual void visit(ExportAllDeclaration& node) = 0;
  virtual void visit(ExportSpecifier& node) = 0;

  // Patterns
  virtual void visit(PatternNode& node) = 0;
  virtual void visit(AssignmentPattern& node) = 0;
  virtual void visit(ArrayPattern& node) = 0;
  virtual void visit(ObjectPattern& node) = 0;
  virtual void visit(RestElement& node) = 0;

  // Comments (optional, depending on parser configuration)
  // virtual void visit(LineComment& node) = 0;
  // virtual void visit(BlockComment& node) = 0;

  // Private Identifier
  virtual void visit(PrivateIdentifier& node) = 0;
};

}  // namespace aerojs::parser::ast

#endif  // AEROJS_PARSER_AST_AST_VISITOR_H