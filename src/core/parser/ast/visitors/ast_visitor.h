/**
 * @file ast_visitor.h
 * @brief AeroJS AST ビジターパターンの基底クラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AeroJSのAST (Abstract Syntax Tree) を走査するための
 * ビジターパターンの基底クラス `AstVisitor` (非const版) および
 * `ConstAstVisitor` (const版) を定義します。
 *
 * ビジターパターンにより、ASTノードクラス自体を変更することなく、
 * ASTに対する新しい操作（例: コード生成、静的解析、最適化）を追加できます。
 *
 * 各具象ビジターは、これらの基底クラスのいずれか（または両方）を継承し、
 * 関心のあるノードタイプに対応する `visit` メソッドをオーバーライドします。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 */

#ifndef AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H
#define AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H

#include <cstdint>

// Forward declarations for all concrete AST node types
// これにより、visitor ヘッダーが node ヘッダーに依存する循環依存を回避します。
// ただし、node ヘッダー側で visitor の前方宣言が必要です (node.h で実施済み)。
namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- Forward Declarations for Node Types --- (Add all node types here)
class Node; // Base node (optional, for default behavior)

// Program Structure
class Program;
class BlockStatement;
class EmptyStatement;

// Declarations
class FunctionDeclaration;
class VariableDeclaration;
class VariableDeclarator;
class ClassDeclaration;
class ClassBody;
class MethodDefinition;
class ImportDeclaration;
class ImportSpecifier;
class ImportDefaultSpecifier;
class ImportNamespaceSpecifier;
class ExportNamedDeclaration;
class ExportDefaultDeclaration;
class ExportAllDeclaration;
class ExportSpecifier;

// Statements
class ExpressionStatement;
class IfStatement;
class SwitchStatement;
class SwitchCase;
class ReturnStatement;
class ThrowStatement;
class TryStatement;
class CatchClause;
class WhileStatement;
class DoWhileStatement;
class ForStatement;
class ForInStatement;
class ForOfStatement;
class BreakStatement;
class ContinueStatement;
class LabeledStatement;
class WithStatement;
class DebuggerStatement;

// Expressions
class Identifier;
class PrivateIdentifier;
class Literal;
class ThisExpression;
class ArrayExpression;
class ObjectExpression;
class Property;
class FunctionExpression;
class ArrowFunctionExpression;
class UnaryExpression;
class UpdateExpression;
class BinaryExpression;
class LogicalExpression;
class AssignmentExpression;
class ConditionalExpression;
class CallExpression;
class NewExpression;
class MemberExpression;
class SequenceExpression;
class YieldExpression;
class AwaitExpression;
class MetaProperty;
class TaggedTemplateExpression;
class TemplateLiteral;
class TemplateElement;
class AssignmentPattern;
class ArrayPattern;
class ObjectPattern;
class RestElement;
class SpreadElement;
class ClassExpression;
class Super;
class ImportExpression;

// JSX (Optional)
#ifdef AEROJS_ENABLE_JSX
class JsxElement;
class JsxOpeningElement;
class JsxClosingElement;
class JsxAttribute;
class JsxSpreadAttribute;
class JsxExpressionContainer;
class JsxFragment;
class JsxText;
#endif

// TypeScript (Optional)
#ifdef AEROJS_ENABLE_TYPESCRIPT
class TsTypeAnnotation;
class TsTypeReference;
class TsParameterProperty;
class TsDeclareFunction;
class TsDeclareMethod;
class TsQualifiedName;
class TsCallSignatureDeclaration;
class TsConstructSignatureDeclaration;
class TsPropertySignature;
class TsMethodSignature;
class TsIndexSignature;
class TsTypePredicate;
class TsNonNullExpression;
class TsAsExpression;
class TsSatisfiesExpression;
class TsTypeAliasDeclaration;
class TsInterfaceDeclaration;
class TsInterfaceBody;
class TsEnumDeclaration;
class TsEnumMember;
class TsModuleDeclaration;
class TsModuleBlock;
class TsImportType;
class TsImportEqualsDeclaration;
class TsExternalModuleReference;
class TsTypeParameterDeclaration;
class TsTypeParameterInstantiation;
class TsTypeParameter;
class TsConditionalType;
class TsInferType;
class TsParenthesizedType;
class TsFunctionType;
class TsConstructorType;
class TsTypeLiteral;
class TsArrayType;
class TsTupleType;
class TsOptionalType;
class TsRestType;
class TsUnionType;
class TsIntersectionType;
class TsIndexedAccessType;
class TsMappedType;
class TsLiteralType;
class TsThisType;
class TsTypeOperator;
class TsTemplateLiteralType;
class TsDecorator;
#endif

// --- End Forward Declarations ---


/**
 * @class AstVisitor
 * @brief AST ノードを訪問するためのビジターインターフェース (非const版)。
 *
 * @details
 * AST を変更する可能性のある操作 (例: トランスフォーマー) のための基底クラスです。
 * 各具象ASTノードクラスに対応する `visitNodeType` メソッドを純粋仮想関数として宣言します。
 * 具象ビジターは、処理したいノードに対応するメソッドをオーバーライドする必要があります。
 *
 * デフォルトの動作:
 * デフォルトでは、各 `visitNodeType` メソッドは処理を行いません。
 * 必要に応じて、共通の処理を `visitNode` (もし定義するなら) や、
 * カテゴリごとの共通メソッド (例: `visitStatement`, `visitExpression`) を
 * 基底クラスで提供し、具象メソッドから呼び出す設計も可能です。
 */
class AstVisitor {
public:
    virtual ~AstVisitor() = default;

    // --- Visit Methods for each Node Type --- (Must be implemented by concrete visitors)

    // Program Structure
    virtual void visitProgram(Program* node) = 0;
    virtual void visitBlockStatement(BlockStatement* node) = 0;
    virtual void visitEmptyStatement(EmptyStatement* node) = 0;

    // Declarations
    virtual void visitVariableDeclaration(VariableDeclaration* node) = 0;
    virtual void visitVariableDeclarator(VariableDeclarator* node) = 0;
    // virtual void visitFunctionDeclaration(FunctionDeclaration* node) = 0; // TODO
    // virtual void visitClassDeclaration(ClassDeclaration* node) = 0; // TODO
    // virtual void visitClassBody(ClassBody* node) = 0; // TODO
    // virtual void visitMethodDefinition(MethodDefinition* node) = 0; // TODO
    // ... other declaration nodes

    // Statements
    virtual void visitExpressionStatement(ExpressionStatement* node) = 0;
    virtual void visitIfStatement(IfStatement* node) = 0;
    // ... other statement nodes

    // Expressions
    virtual void visitLiteral(Literal* node) = 0;
    virtual void visitIdentifier(Identifier* node) = 0;
    virtual void visitPrivateIdentifier(PrivateIdentifier* node) = 0;
    virtual void visitBinaryExpression(BinaryExpression* node) = 0;
    virtual void visitLogicalExpression(LogicalExpression* node) = 0;
    virtual void visitUnaryExpression(UnaryExpression* node) = 0;
    virtual void visitUpdateExpression(UpdateExpression* node) = 0;
    virtual void visitAssignmentExpression(AssignmentExpression* node) = 0;
    virtual void visitCallExpression(CallExpression* node) = 0;
    virtual void visitNewExpression(NewExpression* node) = 0;
    virtual void visitMemberExpression(MemberExpression* node) = 0;
    virtual void visitSequenceExpression(SequenceExpression* node) = 0;
    virtual void visitArrayExpression(ArrayExpression* node) = 0;
    virtual void visitArrowFunctionExpression(ArrowFunctionExpression* node) = 0;
    virtual void visitAwaitExpression(AwaitExpression* node) = 0;
    virtual void visitBinaryExpression(BinaryExpression* node) = 0;
    // ... other expression nodes

    // Add visit methods for all other node types...

    virtual void Visit(Identifier& node) = 0;
    virtual void Visit(PrivateIdentifier& node) = 0;
    virtual void Visit(Literal& node) = 0;
    virtual void Visit(ThisExpression& node) = 0;
    virtual void Visit(ArrayExpression& node) = 0;
    virtual void Visit(ObjectExpression& node) = 0;
    virtual void Visit(Property& node) = 0;
    virtual void Visit(FunctionExpression& node) = 0;
    virtual void Visit(ArrowFunctionExpression& node) = 0;
    virtual void Visit(UnaryExpression& node) = 0;
    virtual void Visit(UpdateExpression& node) = 0;
    virtual void Visit(BinaryExpression& node) = 0;
    virtual void Visit(LogicalExpression& node) = 0;
    virtual void Visit(AssignmentExpression& node) = 0;
    virtual void Visit(ConditionalExpression& node) = 0;
    virtual void Visit(CallExpression& node) = 0;
    virtual void Visit(NewExpression& node) = 0;
    virtual void Visit(MemberExpression& node) = 0;
    virtual void Visit(SequenceExpression& node) = 0;
    virtual void Visit(YieldExpression& node) = 0;
    virtual void Visit(AwaitExpression& node) = 0;
    virtual void Visit(MetaProperty& node) = 0;
    virtual void Visit(TaggedTemplateExpression& node) = 0;
    virtual void Visit(TemplateLiteral& node) = 0;
    virtual void Visit(TemplateElement& node) = 0;
    virtual void Visit(AssignmentPattern& node) = 0;
    virtual void Visit(ArrayPattern& node) = 0;
    virtual void Visit(ObjectPattern& node) = 0;
    virtual void Visit(RestElement& node) = 0;
    virtual void Visit(SpreadElement& node) = 0;
    virtual void Visit(ClassExpression& node) = 0;
    virtual void Visit(Super& node) = 0;
    virtual void Visit(ImportExpression& node) = 0;

protected:
    // Protected constructor to prevent direct instantiation
    AstVisitor() = default;
};

/**
 * @class ConstAstVisitor
 * @brief AST ノードを訪問するためのビジターインターフェース (const版)。
 *
 * @details
 * AST を変更しない操作 (例: 静的解析、検証、読み取り専用の走査) のための基底クラスです。
 * 各 `visitNodeType` メソッドは `const Node*` を引数に取ります。
 * 具象ビジターは、処理したいノードに対応するメソッドをオーバーライドする必要があります。
 */
class ConstAstVisitor {
public:
    virtual ~ConstAstVisitor() = default;

    // --- Visit Methods for each Node Type (const version) ---

    // Program Structure
    virtual void visitProgram(const Program* node) = 0;
    virtual void visitBlockStatement(const BlockStatement* node) = 0;
    virtual void visitEmptyStatement(const EmptyStatement* node) = 0;

    // Declarations
    virtual void visitVariableDeclaration(const VariableDeclaration* node) = 0;
    virtual void visitVariableDeclarator(const VariableDeclarator* node) = 0;
    // virtual void visitFunctionDeclaration(const FunctionDeclaration* node) = 0; // TODO
    // ... other declaration nodes

    // Statements
    virtual void visitExpressionStatement(const ExpressionStatement* node) = 0;
    virtual void visitIfStatement(const IfStatement* node) = 0;
    // ... other statement nodes

    // Expressions
    virtual void visitLiteral(const Literal* node) = 0;
    virtual void visitIdentifier(const Identifier* node) = 0;
    virtual void visitPrivateIdentifier(const PrivateIdentifier* node) = 0;
    virtual void visitBinaryExpression(const BinaryExpression* node) = 0;
    virtual void visitLogicalExpression(const LogicalExpression* node) = 0;
    virtual void visitUnaryExpression(const UnaryExpression* node) = 0;
    virtual void visitUpdateExpression(const UpdateExpression* node) = 0;
    virtual void visitAssignmentExpression(const AssignmentExpression* node) = 0;
    virtual void visitCallExpression(const CallExpression* node) = 0;
    virtual void visitNewExpression(const NewExpression* node) = 0;
    virtual void visitMemberExpression(const MemberExpression* node) = 0;
    virtual void visitSequenceExpression(const SequenceExpression* node) = 0;
    virtual void visitArrayExpression(const ArrayExpression* node) = 0;
    virtual void visitArrowFunctionExpression(const ArrowFunctionExpression* node) = 0;
    virtual void visitAwaitExpression(const AwaitExpression* node) = 0;
    virtual void visitBinaryExpression(const BinaryExpression* node) = 0;
    // ... other expression nodes

    // Add visit methods for all other node types...

    virtual void Visit(const Identifier& node) = 0;
    virtual void Visit(const PrivateIdentifier& node) = 0;
    virtual void Visit(const Literal& node) = 0;
    virtual void Visit(const ThisExpression& node) = 0;
    virtual void Visit(const ArrayExpression& node) = 0;
    virtual void Visit(const ObjectExpression& node) = 0;
    virtual void Visit(const Property& node) = 0;
    virtual void Visit(const FunctionExpression& node) = 0;
    virtual void Visit(const ArrowFunctionExpression& node) = 0;
    virtual void Visit(const UnaryExpression& node) = 0;
    virtual void Visit(const UpdateExpression& node) = 0;
    virtual void Visit(const BinaryExpression& node) = 0;
    virtual void Visit(const LogicalExpression& node) = 0;
    virtual void Visit(const AssignmentExpression& node) = 0;
    virtual void Visit(const ConditionalExpression& node) = 0;
    virtual void Visit(const CallExpression& node) = 0;
    virtual void Visit(const NewExpression& node) = 0;
    virtual void Visit(const MemberExpression& node) = 0;
    virtual void Visit(const SequenceExpression& node) = 0;
    virtual void Visit(const YieldExpression& node) = 0;
    virtual void Visit(const AwaitExpression& node) = 0;
    virtual void Visit(const MetaProperty& node) = 0;
    virtual void Visit(const TaggedTemplateExpression& node) = 0;
    virtual void Visit(const TemplateLiteral& node) = 0;
    virtual void Visit(const TemplateElement& node) = 0;
    virtual void Visit(const AssignmentPattern& node) = 0;
    virtual void Visit(const ArrayPattern& node) = 0;
    virtual void Visit(const ObjectPattern& node) = 0;
    virtual void Visit(const RestElement& node) = 0;
    virtual void Visit(const SpreadElement& node) = 0;
    virtual void Visit(const ClassExpression& node) = 0;
    virtual void Visit(const Super& node) = 0;
    virtual void Visit(const ImportExpression& node) = 0;

protected:
    // Protected constructor to prevent direct instantiation
    ConstAstVisitor() = default;
};


} // namespace ast
} // namespace parser
} // namespace core
} // namespace aerojs

#endif // AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H 