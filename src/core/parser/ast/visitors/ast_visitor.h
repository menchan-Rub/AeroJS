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
 */

#ifndef AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H
#define AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H

#include <cstdint>

// 全ての具象ASTノード型の前方宣言
// これにより、visitor ヘッダーが node ヘッダーに依存する循環依存を回避します
namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// --- ノード型の前方宣言 ---
class Node;  // 基底ノード

// プログラム構造
class Program;
class BlockStatement;
class EmptyStatement;

// 宣言
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

// 文
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

// 式
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

// JSX (オプション)
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

// TypeScript (オプション)
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

// --- 前方宣言終了 ---

/**
 * @class AstVisitor
 * @brief AST ノードを訪問するためのビジターインターフェース (非const版)
 *
 * @details
 * AST を変更する可能性のある操作 (例: トランスフォーマー) のための基底クラスです。
 * 各具象ASTノードクラスに対応する `visitNodeType` メソッドを純粋仮想関数として宣言します。
 * 具象ビジターは、処理したいノードに対応するメソッドをオーバーライドする必要があります。
 */
class AstVisitor {
 public:
  virtual ~AstVisitor() = default;

  // --- 各ノード型に対する訪問メソッド ---

  // プログラム構造
  virtual void visitProgram(Program* node) = 0;
  virtual void visitBlockStatement(BlockStatement* node) = 0;
  virtual void visitEmptyStatement(EmptyStatement* node) = 0;

  // 宣言
  virtual void visitVariableDeclaration(VariableDeclaration* node) = 0;
  virtual void visitVariableDeclarator(VariableDeclarator* node) = 0;
  virtual void visitFunctionDeclaration(FunctionDeclaration* node) = 0;
  virtual void visitClassDeclaration(ClassDeclaration* node) = 0;
  virtual void visitClassBody(ClassBody* node) = 0;
  virtual void visitMethodDefinition(MethodDefinition* node) = 0;
  virtual void visitImportDeclaration(ImportDeclaration* node) = 0;
  virtual void visitImportSpecifier(ImportSpecifier* node) = 0;
  virtual void visitImportDefaultSpecifier(ImportDefaultSpecifier* node) = 0;
  virtual void visitImportNamespaceSpecifier(ImportNamespaceSpecifier* node) = 0;
  virtual void visitExportNamedDeclaration(ExportNamedDeclaration* node) = 0;
  virtual void visitExportDefaultDeclaration(ExportDefaultDeclaration* node) = 0;
  virtual void visitExportAllDeclaration(ExportAllDeclaration* node) = 0;
  virtual void visitExportSpecifier(ExportSpecifier* node) = 0;

  // 文
  virtual void visitExpressionStatement(ExpressionStatement* node) = 0;
  virtual void visitIfStatement(IfStatement* node) = 0;
  virtual void visitSwitchStatement(SwitchStatement* node) = 0;
  virtual void visitSwitchCase(SwitchCase* node) = 0;
  virtual void visitReturnStatement(ReturnStatement* node) = 0;
  virtual void visitThrowStatement(ThrowStatement* node) = 0;
  virtual void visitTryStatement(TryStatement* node) = 0;
  virtual void visitCatchClause(CatchClause* node) = 0;
  virtual void visitWhileStatement(WhileStatement* node) = 0;
  virtual void visitDoWhileStatement(DoWhileStatement* node) = 0;
  virtual void visitForStatement(ForStatement* node) = 0;
  virtual void visitForInStatement(ForInStatement* node) = 0;
  virtual void visitForOfStatement(ForOfStatement* node) = 0;
  virtual void visitBreakStatement(BreakStatement* node) = 0;
  virtual void visitContinueStatement(ContinueStatement* node) = 0;
  virtual void visitLabeledStatement(LabeledStatement* node) = 0;
  virtual void visitWithStatement(WithStatement* node) = 0;
  virtual void visitDebuggerStatement(DebuggerStatement* node) = 0;

  // 式
  virtual void visitIdentifier(Identifier* node) = 0;
  virtual void visitPrivateIdentifier(PrivateIdentifier* node) = 0;
  virtual void visitLiteral(Literal* node) = 0;
  virtual void visitThisExpression(ThisExpression* node) = 0;
  virtual void visitArrayExpression(ArrayExpression* node) = 0;
  virtual void visitObjectExpression(ObjectExpression* node) = 0;
  virtual void visitProperty(Property* node) = 0;
  virtual void visitFunctionExpression(FunctionExpression* node) = 0;
  virtual void visitArrowFunctionExpression(ArrowFunctionExpression* node) = 0;
  virtual void visitUnaryExpression(UnaryExpression* node) = 0;
  virtual void visitUpdateExpression(UpdateExpression* node) = 0;
  virtual void visitBinaryExpression(BinaryExpression* node) = 0;
  virtual void visitLogicalExpression(LogicalExpression* node) = 0;
  virtual void visitAssignmentExpression(AssignmentExpression* node) = 0;
  virtual void visitConditionalExpression(ConditionalExpression* node) = 0;
  virtual void visitCallExpression(CallExpression* node) = 0;
  virtual void visitNewExpression(NewExpression* node) = 0;
  virtual void visitMemberExpression(MemberExpression* node) = 0;
  virtual void visitSequenceExpression(SequenceExpression* node) = 0;
  virtual void visitYieldExpression(YieldExpression* node) = 0;
  virtual void visitAwaitExpression(AwaitExpression* node) = 0;
  virtual void visitMetaProperty(MetaProperty* node) = 0;
  virtual void visitTaggedTemplateExpression(TaggedTemplateExpression* node) = 0;
  virtual void visitTemplateLiteral(TemplateLiteral* node) = 0;
  virtual void visitTemplateElement(TemplateElement* node) = 0;
  virtual void visitAssignmentPattern(AssignmentPattern* node) = 0;
  virtual void visitArrayPattern(ArrayPattern* node) = 0;
  virtual void visitObjectPattern(ObjectPattern* node) = 0;
  virtual void visitRestElement(RestElement* node) = 0;
  virtual void visitSpreadElement(SpreadElement* node) = 0;
  virtual void visitClassExpression(ClassExpression* node) = 0;
  virtual void visitSuper(Super* node) = 0;
  virtual void visitImportExpression(ImportExpression* node) = 0;

  // 参照渡し版訪問メソッド
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

  // 宣言の参照渡し版訪問メソッド
  virtual void Visit(FunctionDeclaration& node) = 0;
  virtual void Visit(VariableDeclaration& node) = 0;
  virtual void Visit(VariableDeclarator& node) = 0;
  virtual void Visit(ClassDeclaration& node) = 0;
  virtual void Visit(ClassBody& node) = 0;
  virtual void Visit(MethodDefinition& node) = 0;
  virtual void Visit(ImportDeclaration& node) = 0;
  virtual void Visit(ImportSpecifier& node) = 0;
  virtual void Visit(ImportDefaultSpecifier& node) = 0;
  virtual void Visit(ImportNamespaceSpecifier& node) = 0;
  virtual void Visit(ExportNamedDeclaration& node) = 0;
  virtual void Visit(ExportDefaultDeclaration& node) = 0;
  virtual void Visit(ExportAllDeclaration& node) = 0;
  virtual void Visit(ExportSpecifier& node) = 0;

  // 文の参照渡し版訪問メソッド
  virtual void Visit(Program& node) = 0;
  virtual void Visit(BlockStatement& node) = 0;
  virtual void Visit(EmptyStatement& node) = 0;
  virtual void Visit(ExpressionStatement& node) = 0;
  virtual void Visit(IfStatement& node) = 0;
  virtual void Visit(SwitchStatement& node) = 0;
  virtual void Visit(SwitchCase& node) = 0;
  virtual void Visit(ReturnStatement& node) = 0;
  virtual void Visit(ThrowStatement& node) = 0;
  virtual void Visit(TryStatement& node) = 0;
  virtual void Visit(CatchClause& node) = 0;
  virtual void Visit(WhileStatement& node) = 0;
  virtual void Visit(DoWhileStatement& node) = 0;
  virtual void Visit(ForStatement& node) = 0;
  virtual void Visit(ForInStatement& node) = 0;
  virtual void Visit(ForOfStatement& node) = 0;
  virtual void Visit(BreakStatement& node) = 0;
  virtual void Visit(ContinueStatement& node) = 0;
  virtual void Visit(LabeledStatement& node) = 0;
  virtual void Visit(WithStatement& node) = 0;
  virtual void Visit(DebuggerStatement& node) = 0;

 protected:
  // 直接インスタンス化を防ぐためのprotectedコンストラクタ
  AstVisitor() = default;
};

/**
 * @class ConstAstVisitor
 * @brief AST ノードを訪問するためのビジターインターフェース (const版)
 *
 * @details
 * AST を変更しない操作 (例: 静的解析、検証、読み取り専用の走査) のための基底クラスです。
 * 各 `visitNodeType` メソッドは `const Node*` を引数に取ります。
 * 具象ビジターは、処理したいノードに対応するメソッドをオーバーライドする必要があります。
 */
class ConstAstVisitor {
 public:
  virtual ~ConstAstVisitor() = default;

  // --- 各ノード型に対する訪問メソッド (const版) ---

  // プログラム構造
  virtual void visitProgram(const Program* node) = 0;
  virtual void visitBlockStatement(const BlockStatement* node) = 0;
  virtual void visitEmptyStatement(const EmptyStatement* node) = 0;

  // 宣言
  virtual void visitVariableDeclaration(const VariableDeclaration* node) = 0;
  virtual void visitVariableDeclarator(const VariableDeclarator* node) = 0;
  virtual void visitFunctionDeclaration(const FunctionDeclaration* node) = 0;
  virtual void visitClassDeclaration(const ClassDeclaration* node) = 0;
  virtual void visitClassBody(const ClassBody* node) = 0;
  virtual void visitMethodDefinition(const MethodDefinition* node) = 0;
  virtual void visitImportDeclaration(const ImportDeclaration* node) = 0;
  virtual void visitImportSpecifier(const ImportSpecifier* node) = 0;
  virtual void visitImportDefaultSpecifier(const ImportDefaultSpecifier* node) = 0;
  virtual void visitImportNamespaceSpecifier(const ImportNamespaceSpecifier* node) = 0;
  virtual void visitExportNamedDeclaration(const ExportNamedDeclaration* node) = 0;
  virtual void visitExportDefaultDeclaration(const ExportDefaultDeclaration* node) = 0;
  virtual void visitExportAllDeclaration(const ExportAllDeclaration* node) = 0;
  virtual void visitExportSpecifier(const ExportSpecifier* node) = 0;

  // 文
  virtual void visitExpressionStatement(const ExpressionStatement* node) = 0;
  virtual void visitIfStatement(const IfStatement* node) = 0;
  virtual void visitSwitchStatement(const SwitchStatement* node) = 0;
  virtual void visitSwitchCase(const SwitchCase* node) = 0;
  virtual void visitReturnStatement(const ReturnStatement* node) = 0;
  virtual void visitThrowStatement(const ThrowStatement* node) = 0;
  virtual void visitTryStatement(const TryStatement* node) = 0;
  virtual void visitCatchClause(const CatchClause* node) = 0;
  virtual void visitWhileStatement(const WhileStatement* node) = 0;
  virtual void visitDoWhileStatement(const DoWhileStatement* node) = 0;
  virtual void visitForStatement(const ForStatement* node) = 0;
  virtual void visitForInStatement(const ForInStatement* node) = 0;
  virtual void visitForOfStatement(const ForOfStatement* node) = 0;
  virtual void visitBreakStatement(const BreakStatement* node) = 0;
  virtual void visitContinueStatement(const ContinueStatement* node) = 0;
  virtual void visitLabeledStatement(const LabeledStatement* node) = 0;
  virtual void visitWithStatement(const WithStatement* node) = 0;
  virtual void visitDebuggerStatement(const DebuggerStatement* node) = 0;

  // 式
  virtual void visitIdentifier(const Identifier* node) = 0;
  virtual void visitPrivateIdentifier(const PrivateIdentifier* node) = 0;
  virtual void visitLiteral(const Literal* node) = 0;
  virtual void visitThisExpression(const ThisExpression* node) = 0;
  virtual void visitArrayExpression(const ArrayExpression* node) = 0;
  virtual void visitObjectExpression(const ObjectExpression* node) = 0;
  virtual void visitProperty(const Property* node) = 0;
  virtual void visitFunctionExpression(const FunctionExpression* node) = 0;
  virtual void visitArrowFunctionExpression(const ArrowFunctionExpression* node) = 0;
  virtual void visitUnaryExpression(const UnaryExpression* node) = 0;
  virtual void visitUpdateExpression(const UpdateExpression* node) = 0;
  virtual void visitBinaryExpression(const BinaryExpression* node) = 0;
  virtual void visitLogicalExpression(const LogicalExpression* node) = 0;
  virtual void visitAssignmentExpression(const AssignmentExpression* node) = 0;
  virtual void visitConditionalExpression(const ConditionalExpression* node) = 0;
  virtual void visitCallExpression(const CallExpression* node) = 0;
  virtual void visitNewExpression(const NewExpression* node) = 0;
  virtual void visitMemberExpression(const MemberExpression* node) = 0;
  virtual void visitSequenceExpression(const SequenceExpression* node) = 0;
  virtual void visitYieldExpression(const YieldExpression* node) = 0;
  virtual void visitAwaitExpression(const AwaitExpression* node) = 0;
  virtual void visitMetaProperty(const MetaProperty* node) = 0;
  virtual void visitTaggedTemplateExpression(const TaggedTemplateExpression* node) = 0;
  virtual void visitTemplateLiteral(const TemplateLiteral* node) = 0;
  virtual void visitTemplateElement(const TemplateElement* node) = 0;
  virtual void visitAssignmentPattern(const AssignmentPattern* node) = 0;
  virtual void visitArrayPattern(const ArrayPattern* node) = 0;
  virtual void visitObjectPattern(const ObjectPattern* node) = 0;
  virtual void visitRestElement(const RestElement* node) = 0;
  virtual void visitSpreadElement(const SpreadElement* node) = 0;
  virtual void visitClassExpression(const ClassExpression* node) = 0;
  virtual void visitSuper(const Super* node) = 0;
  virtual void visitImportExpression(const ImportExpression* node) = 0;

  // 参照渡し版訪問メソッド (const版)
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

  // 宣言の参照渡し版訪問メソッド (const版)
  virtual void Visit(const FunctionDeclaration& node) = 0;
  virtual void Visit(const VariableDeclaration& node) = 0;
  virtual void Visit(const VariableDeclarator& node) = 0;
  virtual void Visit(const ClassDeclaration& node) = 0;
  virtual void Visit(const ClassBody& node) = 0;
  virtual void Visit(const MethodDefinition& node) = 0;
  virtual void Visit(const ImportDeclaration& node) = 0;
  virtual void Visit(const ImportSpecifier& node) = 0;
  virtual void Visit(const ImportDefaultSpecifier& node) = 0;
  virtual void Visit(const ImportNamespaceSpecifier& node) = 0;
  virtual void Visit(const ExportNamedDeclaration& node) = 0;
  virtual void Visit(const ExportDefaultDeclaration& node) = 0;
  virtual void Visit(const ExportAllDeclaration& node) = 0;
  virtual void Visit(const ExportSpecifier& node) = 0;

  // 文の参照渡し版訪問メソッド (const版)
  virtual void Visit(const Program& node) = 0;
  virtual void Visit(const BlockStatement& node) = 0;
  virtual void Visit(const EmptyStatement& node) = 0;
  virtual void Visit(const ExpressionStatement& node) = 0;
  virtual void Visit(const IfStatement& node) = 0;
  virtual void Visit(const SwitchStatement& node) = 0;
  virtual void Visit(const SwitchCase& node) = 0;
  virtual void Visit(const ReturnStatement& node) = 0;
  virtual void Visit(const ThrowStatement& node) = 0;
  virtual void Visit(const TryStatement& node) = 0;
  virtual void Visit(const CatchClause& node) = 0;
  virtual void Visit(const WhileStatement& node) = 0;
  virtual void Visit(const DoWhileStatement& node) = 0;
  virtual void Visit(const ForStatement& node) = 0;
  virtual void Visit(const ForInStatement& node) = 0;
  virtual void Visit(const ForOfStatement& node) = 0;
  virtual void Visit(const BreakStatement& node) = 0;
  virtual void Visit(const ContinueStatement& node) = 0;
  virtual void Visit(const LabeledStatement& node) = 0;
  virtual void Visit(const WithStatement& node) = 0;
  virtual void Visit(const DebuggerStatement& node) = 0;

 protected:
  // 直接インスタンス化を防ぐためのprotectedコンストラクタ
  ConstAstVisitor() = default;
};

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_PARSER_AST_VISITORS_AST_VISITOR_H