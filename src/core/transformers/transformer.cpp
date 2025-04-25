/**
 * @file transformer.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief AST変換を行うトランスフォーマーの基本実装
 */

#include "transformer.h"

#include <cassert>

namespace aero {
namespace transformers {

Transformer::Transformer(std::string name, std::string description)
    : m_name(std::move(name)), m_description(std::move(description)), m_changed(false) {
}

TransformResult Transformer::transform(ast::NodePtr node) {
  if (!node) {
    return TransformResult::unchanged(nullptr);
  }

  m_result = node;
  m_changed = false;

  // ノードの型に応じたvisitメソッドを呼び出す
  node->accept(this);

  return TransformResult{m_result, m_changed, false};
}

std::string Transformer::getName() const {
  return m_name;
}

std::string Transformer::getDescription() const {
  return m_description;
}

TransformResult Transformer::transformNode(ast::NodePtr node) {
  if (!node) {
    return TransformResult::unchanged(nullptr);
  }

  ast::NodePtr oldNode = m_result;
  bool oldChanged = m_changed;

  m_result = node;
  m_changed = false;

  // ノードの型に応じたvisitメソッドを呼び出す
  node->accept(this);

  ast::NodePtr result = m_result;
  bool changed = m_changed;

  m_result = oldNode;
  m_changed = oldChanged || changed;

  return TransformResult{result, changed, false};
}

// AST Visitor メソッドの実装

void Transformer::visitProgram(ast::Program* node) {
  bool localChanged = false;

  // 子ノードを再帰的に変換
  for (size_t i = 0; i < node->body.size(); ++i) {
    auto result = transformNode(node->body[i]);
    if (result.changed) {
      localChanged = true;
      node->body[i] = std::move(result.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitExpressionStatement(ast::ExpressionStatement* node) {
  auto result = transformNode(node->expression);
  if (result.changed) {
    m_changed = true;
    node->expression = std::move(result.node);
  }
}

void Transformer::visitBlockStatement(ast::BlockStatement* node) {
  bool localChanged = false;

  // 子ノードを再帰的に変換
  for (size_t i = 0; i < node->body.size(); ++i) {
    auto result = transformNode(node->body[i]);
    if (result.changed) {
      localChanged = true;
      node->body[i] = std::move(result.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitEmptyStatement(ast::EmptyStatement* node) {
  // 変換なし
}

void Transformer::visitIfStatement(ast::IfStatement* node) {
  bool localChanged = false;

  // 条件式を変換
  auto testResult = transformNode(node->test);
  if (testResult.changed) {
    localChanged = true;
    node->test = std::move(testResult.node);
  }

  // Then節を変換
  auto consequentResult = transformNode(node->consequent);
  if (consequentResult.changed) {
    localChanged = true;
    node->consequent = std::move(consequentResult.node);
  }

  // Else節があれば変換
  if (node->alternate) {
    auto alternateResult = transformNode(node->alternate);
    if (alternateResult.changed) {
      localChanged = true;
      node->alternate = std::move(alternateResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitSwitchStatement(ast::SwitchStatement* node) {
  bool localChanged = false;

  // 判別式を変換
  auto discriminantResult = transformNode(node->discriminant);
  if (discriminantResult.changed) {
    localChanged = true;
    node->discriminant = std::move(discriminantResult.node);
  }

  // ケース節を変換
  for (size_t i = 0; i < node->cases.size(); ++i) {
    auto caseResult = transformNode(node->cases[i]);
    if (caseResult.changed) {
      localChanged = true;
      node->cases[i] = std::move(caseResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitCaseClause(ast::CaseClause* node) {
  bool localChanged = false;

  // ケース値を変換
  if (node->test) {
    auto testResult = transformNode(node->test);
    if (testResult.changed) {
      localChanged = true;
      node->test = std::move(testResult.node);
    }
  }

  // 本体を変換
  for (size_t i = 0; i < node->consequent.size(); ++i) {
    auto stmtResult = transformNode(node->consequent[i]);
    if (stmtResult.changed) {
      localChanged = true;
      node->consequent[i] = std::move(stmtResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitWhileStatement(ast::WhileStatement* node) {
  bool localChanged = false;

  // 条件式を変換
  auto testResult = transformNode(node->test);
  if (testResult.changed) {
    localChanged = true;
    node->test = std::move(testResult.node);
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitDoWhileStatement(ast::DoWhileStatement* node) {
  bool localChanged = false;

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  // 条件式を変換
  auto testResult = transformNode(node->test);
  if (testResult.changed) {
    localChanged = true;
    node->test = std::move(testResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitForStatement(ast::ForStatement* node) {
  bool localChanged = false;

  // 初期化式を変換
  if (node->init) {
    auto initResult = transformNode(node->init);
    if (initResult.changed) {
      localChanged = true;
      node->init = std::move(initResult.node);
    }
  }

  // 条件式を変換
  if (node->test) {
    auto testResult = transformNode(node->test);
    if (testResult.changed) {
      localChanged = true;
      node->test = std::move(testResult.node);
    }
  }

  // 更新式を変換
  if (node->update) {
    auto updateResult = transformNode(node->update);
    if (updateResult.changed) {
      localChanged = true;
      node->update = std::move(updateResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitForInStatement(ast::ForInStatement* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitForOfStatement(ast::ForOfStatement* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitContinueStatement(ast::ContinueStatement* node) {
  // 変換なし（ラベルはプリミティブなので変換しない）
}

void Transformer::visitBreakStatement(ast::BreakStatement* node) {
  // 変換なし（ラベルはプリミティブなので変換しない）
}

void Transformer::visitReturnStatement(ast::ReturnStatement* node) {
  if (node->argument) {
    auto argumentResult = transformNode(node->argument);
    if (argumentResult.changed) {
      m_changed = true;
      node->argument = std::move(argumentResult.node);
    }
  }
}

void Transformer::visitWithStatement(ast::WithStatement* node) {
  bool localChanged = false;

  // オブジェクト式を変換
  auto objectResult = transformNode(node->object);
  if (objectResult.changed) {
    localChanged = true;
    node->object = std::move(objectResult.node);
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitLabeledStatement(ast::LabeledStatement* node) {
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    m_changed = true;
    node->body = std::move(bodyResult.node);
  }
}

void Transformer::visitTryStatement(ast::TryStatement* node) {
  bool localChanged = false;

  // try節を変換
  auto blockResult = transformNode(node->block);
  if (blockResult.changed) {
    localChanged = true;
    node->block = std::move(blockResult.node);
  }

  // catch節を変換
  if (node->handler) {
    auto handlerResult = transformNode(node->handler);
    if (handlerResult.changed) {
      localChanged = true;
      node->handler = std::move(handlerResult.node);
    }
  }

  // finally節を変換
  if (node->finalizer) {
    auto finalizerResult = transformNode(node->finalizer);
    if (finalizerResult.changed) {
      localChanged = true;
      node->finalizer = std::move(finalizerResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitCatchClause(ast::CatchClause* node) {
  bool localChanged = false;

  // パラメータを変換
  if (node->param) {
    auto paramResult = transformNode(node->param);
    if (paramResult.changed) {
      localChanged = true;
      node->param = std::move(paramResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitThrowStatement(ast::ThrowStatement* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitDebuggerStatement(ast::DebuggerStatement* node) {
  // 変換なし
}

void Transformer::visitVariableDeclaration(ast::VariableDeclaration* node) {
  bool localChanged = false;

  // 変数宣言子を変換
  for (size_t i = 0; i < node->declarations.size(); ++i) {
    auto declarationResult = transformNode(node->declarations[i]);
    if (declarationResult.changed) {
      localChanged = true;
      node->declarations[i] = std::move(declarationResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitVariableDeclarator(ast::VariableDeclarator* node) {
  bool localChanged = false;

  // 変数識別子を変換
  auto idResult = transformNode(node->id);
  if (idResult.changed) {
    localChanged = true;
    node->id = std::move(idResult.node);
  }

  // 初期化式を変換
  if (node->init) {
    auto initResult = transformNode(node->init);
    if (initResult.changed) {
      localChanged = true;
      node->init = std::move(initResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitIdentifier(ast::Identifier* node) {
  // 基本的には変換なし（サブクラスが名前の変更などを行う場合がある）
}

void Transformer::visitLiteral(ast::Literal* node) {
  // 基本的には変換なし（サブクラスがリテラルの最適化などを行う場合がある）
}

void Transformer::visitFunctionDeclaration(ast::FunctionDeclaration* node) {
  bool localChanged = false;

  // 関数名を変換
  auto idResult = transformNode(node->id);
  if (idResult.changed) {
    localChanged = true;
    node->id = std::move(idResult.node);
  }

  // パラメータを変換
  for (size_t i = 0; i < node->params.size(); ++i) {
    auto paramResult = transformNode(node->params[i]);
    if (paramResult.changed) {
      localChanged = true;
      node->params[i] = std::move(paramResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitFunctionExpression(ast::FunctionExpression* node) {
  bool localChanged = false;

  // 関数名があれば変換
  if (node->id) {
    auto idResult = transformNode(node->id);
    if (idResult.changed) {
      localChanged = true;
      node->id = std::move(idResult.node);
    }
  }

  // パラメータを変換
  for (size_t i = 0; i < node->params.size(); ++i) {
    auto paramResult = transformNode(node->params[i]);
    if (paramResult.changed) {
      localChanged = true;
      node->params[i] = std::move(paramResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitArrowFunctionExpression(ast::ArrowFunctionExpression* node) {
  bool localChanged = false;

  // パラメータを変換
  for (size_t i = 0; i < node->params.size(); ++i) {
    auto paramResult = transformNode(node->params[i]);
    if (paramResult.changed) {
      localChanged = true;
      node->params[i] = std::move(paramResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitClassDeclaration(ast::ClassDeclaration* node) {
  bool localChanged = false;

  // クラス名を変換
  if (node->id) {
    auto idResult = transformNode(node->id);
    if (idResult.changed) {
      localChanged = true;
      node->id = std::move(idResult.node);
    }
  }

  // 親クラスを変換
  if (node->superClass) {
    auto superResult = transformNode(node->superClass);
    if (superResult.changed) {
      localChanged = true;
      node->superClass = std::move(superResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitClassExpression(ast::ClassExpression* node) {
  bool localChanged = false;

  // クラス名を変換
  if (node->id) {
    auto idResult = transformNode(node->id);
    if (idResult.changed) {
      localChanged = true;
      node->id = std::move(idResult.node);
    }
  }

  // 親クラスを変換
  if (node->superClass) {
    auto superResult = transformNode(node->superClass);
    if (superResult.changed) {
      localChanged = true;
      node->superClass = std::move(superResult.node);
    }
  }

  // 本体を変換
  auto bodyResult = transformNode(node->body);
  if (bodyResult.changed) {
    localChanged = true;
    node->body = std::move(bodyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitMethodDefinition(ast::MethodDefinition* node) {
  bool localChanged = false;

  // キーを変換
  auto keyResult = transformNode(node->key);
  if (keyResult.changed) {
    localChanged = true;
    node->key = std::move(keyResult.node);
  }

  // 値を変換
  auto valueResult = transformNode(node->value);
  if (valueResult.changed) {
    localChanged = true;
    node->value = std::move(valueResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitClassProperty(ast::ClassProperty* node) {
  bool localChanged = false;

  // キーを変換
  auto keyResult = transformNode(node->key);
  if (keyResult.changed) {
    localChanged = true;
    node->key = std::move(keyResult.node);
  }

  // 値があれば変換
  if (node->value) {
    auto valueResult = transformNode(node->value);
    if (valueResult.changed) {
      localChanged = true;
      node->value = std::move(valueResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitImportDeclaration(ast::ImportDeclaration* node) {
  bool localChanged = false;

  // インポート指定子を変換
  for (size_t i = 0; i < node->specifiers.size(); ++i) {
    auto specifierResult = transformNode(node->specifiers[i]);
    if (specifierResult.changed) {
      localChanged = true;
      node->specifiers[i] = std::move(specifierResult.node);
    }
  }

  // ソースを変換
  auto sourceResult = transformNode(node->source);
  if (sourceResult.changed) {
    localChanged = true;
    node->source = std::move(sourceResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitExportNamedDeclaration(ast::ExportNamedDeclaration* node) {
  bool localChanged = false;

  // 宣言を変換
  if (node->declaration) {
    auto declarationResult = transformNode(node->declaration);
    if (declarationResult.changed) {
      localChanged = true;
      node->declaration = std::move(declarationResult.node);
    }
  }

  // エクスポート指定子を変換
  for (size_t i = 0; i < node->specifiers.size(); ++i) {
    auto specifierResult = transformNode(node->specifiers[i]);
    if (specifierResult.changed) {
      localChanged = true;
      node->specifiers[i] = std::move(specifierResult.node);
    }
  }

  // ソースを変換
  if (node->source) {
    auto sourceResult = transformNode(node->source);
    if (sourceResult.changed) {
      localChanged = true;
      node->source = std::move(sourceResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitExportDefaultDeclaration(ast::ExportDefaultDeclaration* node) {
  auto declarationResult = transformNode(node->declaration);
  if (declarationResult.changed) {
    m_changed = true;
    node->declaration = std::move(declarationResult.node);
  }
}

void Transformer::visitExportAllDeclaration(ast::ExportAllDeclaration* node) {
  auto sourceResult = transformNode(node->source);
  if (sourceResult.changed) {
    m_changed = true;
    node->source = std::move(sourceResult.node);
  }
}

void Transformer::visitImportSpecifier(ast::ImportSpecifier* node) {
  bool localChanged = false;

  // インポート名を変換
  auto importedResult = transformNode(node->imported);
  if (importedResult.changed) {
    localChanged = true;
    node->imported = std::move(importedResult.node);
  }

  // ローカル名を変換
  auto localResult = transformNode(node->local);
  if (localResult.changed) {
    localChanged = true;
    node->local = std::move(localResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitExportSpecifier(ast::ExportSpecifier* node) {
  bool localChanged = false;

  // エクスポート名を変換
  auto exportedResult = transformNode(node->exported);
  if (exportedResult.changed) {
    localChanged = true;
    node->exported = std::move(exportedResult.node);
  }

  // ローカル名を変換
  auto localResult = transformNode(node->local);
  if (localResult.changed) {
    localChanged = true;
    node->local = std::move(localResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitObjectPattern(ast::ObjectPattern* node) {
  bool localChanged = false;

  // プロパティを変換
  for (size_t i = 0; i < node->properties.size(); ++i) {
    auto propertyResult = transformNode(node->properties[i]);
    if (propertyResult.changed) {
      localChanged = true;
      node->properties[i] = std::move(propertyResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitArrayPattern(ast::ArrayPattern* node) {
  bool localChanged = false;

  // 要素を変換
  for (size_t i = 0; i < node->elements.size(); ++i) {
    if (node->elements[i]) {
      auto elementResult = transformNode(node->elements[i]);
      if (elementResult.changed) {
        localChanged = true;
        node->elements[i] = std::move(elementResult.node);
      }
    }
  }

  m_changed = localChanged;
}

void Transformer::visitAssignmentPattern(ast::AssignmentPattern* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitRestElement(ast::RestElement* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitSpreadElement(ast::SpreadElement* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitTemplateElement(ast::TemplateElement* node) {
  // 変換なし
}

void Transformer::visitTemplateLiteral(ast::TemplateLiteral* node) {
  bool localChanged = false;

  // クォートを変換
  for (size_t i = 0; i < node->quasis.size(); ++i) {
    auto quasisResult = transformNode(node->quasis[i]);
    if (quasisResult.changed) {
      localChanged = true;
      node->quasis[i] = std::move(quasisResult.node);
    }
  }

  // 式を変換
  for (size_t i = 0; i < node->expressions.size(); ++i) {
    auto expressionResult = transformNode(node->expressions[i]);
    if (expressionResult.changed) {
      localChanged = true;
      node->expressions[i] = std::move(expressionResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitTaggedTemplateExpression(ast::TaggedTemplateExpression* node) {
  bool localChanged = false;

  // タグを変換
  auto tagResult = transformNode(node->tag);
  if (tagResult.changed) {
    localChanged = true;
    node->tag = std::move(tagResult.node);
  }

  // テンプレートを変換
  auto quasisResult = transformNode(node->quasi);
  if (quasisResult.changed) {
    localChanged = true;
    node->quasi = std::move(quasisResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitObjectExpression(ast::ObjectExpression* node) {
  bool localChanged = false;

  // プロパティを変換
  for (size_t i = 0; i < node->properties.size(); ++i) {
    auto propertyResult = transformNode(node->properties[i]);
    if (propertyResult.changed) {
      localChanged = true;
      node->properties[i] = std::move(propertyResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitProperty(ast::Property* node) {
  bool localChanged = false;

  // キーを変換
  auto keyResult = transformNode(node->key);
  if (keyResult.changed) {
    localChanged = true;
    node->key = std::move(keyResult.node);
  }

  // 値を変換
  auto valueResult = transformNode(node->value);
  if (valueResult.changed) {
    localChanged = true;
    node->value = std::move(valueResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitArrayExpression(ast::ArrayExpression* node) {
  bool localChanged = false;

  // 要素を変換
  for (size_t i = 0; i < node->elements.size(); ++i) {
    if (node->elements[i]) {
      auto elementResult = transformNode(node->elements[i]);
      if (elementResult.changed) {
        localChanged = true;
        node->elements[i] = std::move(elementResult.node);
      }
    }
  }

  m_changed = localChanged;
}

void Transformer::visitUnaryExpression(ast::UnaryExpression* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitUpdateExpression(ast::UpdateExpression* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitBinaryExpression(ast::BinaryExpression* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitAssignmentExpression(ast::AssignmentExpression* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitLogicalExpression(ast::LogicalExpression* node) {
  bool localChanged = false;

  // 左辺を変換
  auto leftResult = transformNode(node->left);
  if (leftResult.changed) {
    localChanged = true;
    node->left = std::move(leftResult.node);
  }

  // 右辺を変換
  auto rightResult = transformNode(node->right);
  if (rightResult.changed) {
    localChanged = true;
    node->right = std::move(rightResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitMemberExpression(ast::MemberExpression* node) {
  bool localChanged = false;

  // オブジェクトを変換
  auto objectResult = transformNode(node->object);
  if (objectResult.changed) {
    localChanged = true;
    node->object = std::move(objectResult.node);
  }

  // プロパティを変換
  auto propertyResult = transformNode(node->property);
  if (propertyResult.changed) {
    localChanged = true;
    node->property = std::move(propertyResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitConditionalExpression(ast::ConditionalExpression* node) {
  bool localChanged = false;

  // 条件式を変換
  auto testResult = transformNode(node->test);
  if (testResult.changed) {
    localChanged = true;
    node->test = std::move(testResult.node);
  }

  // 真の場合の式を変換
  auto consequentResult = transformNode(node->consequent);
  if (consequentResult.changed) {
    localChanged = true;
    node->consequent = std::move(consequentResult.node);
  }

  // 偽の場合の式を変換
  auto alternateResult = transformNode(node->alternate);
  if (alternateResult.changed) {
    localChanged = true;
    node->alternate = std::move(alternateResult.node);
  }

  m_changed = localChanged;
}

void Transformer::visitCallExpression(ast::CallExpression* node) {
  bool localChanged = false;

  // 呼び出し対象を変換
  auto calleeResult = transformNode(node->callee);
  if (calleeResult.changed) {
    localChanged = true;
    node->callee = std::move(calleeResult.node);
  }

  // 引数を変換
  for (size_t i = 0; i < node->arguments.size(); ++i) {
    auto argumentResult = transformNode(node->arguments[i]);
    if (argumentResult.changed) {
      localChanged = true;
      node->arguments[i] = std::move(argumentResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitNewExpression(ast::NewExpression* node) {
  bool localChanged = false;

  // 呼び出し対象を変換
  auto calleeResult = transformNode(node->callee);
  if (calleeResult.changed) {
    localChanged = true;
    node->callee = std::move(calleeResult.node);
  }

  // 引数を変換
  for (size_t i = 0; i < node->arguments.size(); ++i) {
    auto argumentResult = transformNode(node->arguments[i]);
    if (argumentResult.changed) {
      localChanged = true;
      node->arguments[i] = std::move(argumentResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitSequenceExpression(ast::SequenceExpression* node) {
  bool localChanged = false;

  // 式を変換
  for (size_t i = 0; i < node->expressions.size(); ++i) {
    auto expressionResult = transformNode(node->expressions[i]);
    if (expressionResult.changed) {
      localChanged = true;
      node->expressions[i] = std::move(expressionResult.node);
    }
  }

  m_changed = localChanged;
}

void Transformer::visitAwaitExpression(ast::AwaitExpression* node) {
  auto argumentResult = transformNode(node->argument);
  if (argumentResult.changed) {
    m_changed = true;
    node->argument = std::move(argumentResult.node);
  }
}

void Transformer::visitYieldExpression(ast::YieldExpression* node) {
  if (node->argument) {
    auto argumentResult = transformNode(node->argument);
    if (argumentResult.changed) {
      m_changed = true;
      node->argument = std::move(argumentResult.node);
    }
  }
}

}  // namespace transformers
}  // namespace aero
