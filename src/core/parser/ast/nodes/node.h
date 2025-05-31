/**
 * @file node.h
 * @brief AeroJS 抽象構文木 (AST) の基底ノードクラス定義
 * @author AeroJS Developers
 * @copyright Copyright (c) 2024 AeroJS Project
 * @license MIT License
 *
 * @details
 * このファイルは、AeroJSのJavaScriptコードを表現するASTの全てのノードタイプの
 * 基底となる抽象クラス `Node` を定義します。
 * 各ASTノードは、ソースコードの位置情報、ノードタイプ、親ノードへのポインタ、
 * そして子ノードのリスト（オプション）を保持します。
 * また、ビジターパターンのための `accept` メソッドと、シリアライズや
 * デバッグのための基本的な機能を提供します。
 *
 * コーディング規約: AeroJS コーディング規約 version 1.2
 * - C++20 標準
 * - インデント: スペース2文字
 * - 命名規則: クラス名=PascalCase, メソッド名=camelCase, メンバ変数=m_camelCase
 * - コメント: Doxygenスタイル
 * - エラーハンドリング: 例外ではなく、エラーコード/状態を使用
 * - パフォーマンス: 仮想関数の使用は最小限に、メモリ効率を考慮
 */

#pragma once

#include <cstdint>
#include <memory>                 // std::unique_ptr, std::shared_ptr (将来的に必要であれば) のため
#include <nlohmann/json_fwd.hpp>  // JSONシリアライズのための前方宣言
#include <optional>
#include <stdexcept>  // 必要であれば std::bad_alloc のような特定の例外のため
#include <string>
#include <variant>  // 値や型のための潜在的な使用
#include <vector>

#include "../../token.h"  // SourceLocation のため

namespace aerojs {
namespace core {
namespace parser {
namespace ast {

// 前方宣言
class AstVisitor;       // ASTビジターの基底クラス
class ConstAstVisitor;  // const ASTビジターの基底クラス
class StatementNode;
class ExpressionNode;
class DeclarationNode;
class PatternNode;

/**
 * @enum NodeType
 * @brief ASTノードのタイプを示す列挙型
 *
 * @details
 * この列挙型は、AST内に存在しうる全てのノードの種類を定義します。
 * 各ノードタイプは、JavaScriptの構文要素に対応します。
 * 将来の拡張性を考慮し、予約済みの範囲も含まれています。
 * `NodeType::Count` は常に最後の要素とし、ノードタイプの総数を表します。
 * これは主に統計やテーブルサイズ定義に使用されます。
 */
enum class NodeType : uint16_t {
  // --- センチネル ---
  Uninitialized = 0,  ///< 未初期化状態

  // --- プログラム構造 ---
  Program,         ///< プログラム全体 (スクリプトまたはモジュール)
  BlockStatement,  ///< ブロック文 ({ ... })
  EmptyStatement,  ///< 空文 (;)

  // --- 宣言 ---
  FunctionDeclaration,       ///< 関数宣言 (function foo() {})
  VariableDeclaration,       ///< 変数宣言 (var, let, const)
  VariableDeclarator,        ///< 変数宣言子 (x = 1 in var x = 1)
  ClassDeclaration,          ///< クラス宣言 (class Foo {})
  ClassBody,                 ///< クラス本体 ({ ... } in class)
  MethodDefinition,          ///< メソッド定義 (constructor, method, get, set)
  ImportDeclaration,         ///< インポート宣言 (import ... from ...)
  ImportSpecifier,           ///< デフォルト/名前付きインポート指定子 (foo, { bar })
  ImportDefaultSpecifier,    ///< デフォルトインポート指定子 (foo in import foo from ...)
  ImportNamespaceSpecifier,  ///< 名前空間インポート指定子 (* as foo)
  ExportNamedDeclaration,    ///< 名前付きエクスポート宣言 (export { foo })
  ExportDefaultDeclaration,  ///< デフォルトエクスポート宣言 (export default ...)
  ExportAllDeclaration,      ///< 全てエクスポート宣言 (export * from ...)
  ExportSpecifier,           ///< エクスポート指定子 ({ foo as bar })

  // --- 文 ---
  ExpressionStatement,  ///< 式文 (例: console.log("hello");)
  IfStatement,          ///< if文
  SwitchStatement,      ///< switch文
  SwitchCase,           ///< case / default 節
  ReturnStatement,      ///< return文
  ThrowStatement,       ///< throw文
  TryStatement,         ///< try文
  CatchClause,          ///< catch節
  WhileStatement,       ///< whileループ
  DoWhileStatement,     ///< do-whileループ
  ForStatement,         ///< forループ (Cスタイル)
  ForInStatement,       ///< for-inループ
  ForOfStatement,       ///< for-ofループ
  BreakStatement,       ///< break文
  ContinueStatement,    ///< continue文
  LabeledStatement,     ///< ラベル付き文 (label: ...)
  WithStatement,        ///< with文 (非推奨)
  DebuggerStatement,    ///< debugger文

  // --- 式 ---
  Identifier,                ///< 識別子 (変数名、関数名)
  PrivateIdentifier,         ///< プライベート識別子 (#field)
  Literal,                   ///< リテラル (文字列, 数値, 真偽値, null, 正規表現, bigint)
  ThisExpression,            ///< this式
  ArrayExpression,           ///< 配列リテラル ([1, 2])
  ObjectExpression,          ///< オブジェクトリテラル ({ a: 1 })
  Property,                  ///< オブジェクトプロパティ (key: value)
  FunctionExpression,        ///< 関数式 (function() {})
  ArrowFunctionExpression,   ///< アロー関数式 (() => ...)
  UnaryExpression,           ///< 単項演算子式 (!, +, -, ~, typeof, void, delete)
  UpdateExpression,          ///< 更新式 (++, --)
  BinaryExpression,          ///< 二項演算子式 (+, -, *, /, ==, ===, in, instanceof, など)
  LogicalExpression,         ///< 論理演算子式 (&&, ||, ??)
  AssignmentExpression,      ///< 代入式 (=, +=, -=, など)
  ConditionalExpression,     ///< 条件 (三項) 演算子式 (cond ? then : else)
  CallExpression,            ///< 関数呼び出し式 (func(arg))
  NewExpression,             ///< new式 (new Date())
  MemberExpression,          ///< メンバアクセス式 (obj.prop, obj["prop"])
  SequenceExpression,        ///< カンマ演算子式 (a, b, c)
  YieldExpression,           ///< yield式 (yield value)
  AwaitExpression,           ///< await式 (await promise)
  MetaProperty,              ///< メタプロパティ (new.target, import.meta)
  TaggedTemplateExpression,  ///< タグ付きテンプレートリテラル (tag`template`)
  TemplateLiteral,           ///< テンプレートリテラル (`string ${expr}`)
  TemplateElement,           ///< テンプレートリテラル要素 (文字列部分, ${} 部分)
  AssignmentPattern,         ///< 分割代入デフォルト値 ( {a = 1} = {} )
  ArrayPattern,              ///< 配列分割代入パターン ([a, b])
  ObjectPattern,             ///< オブジェクト分割代入パターン ({a, b})
  RestElement,               ///< 残余要素/スプレッド構文 (...args)
  SpreadElement,             ///< スプレッド構文 (...array)
  ClassExpression,           ///< クラス式 (const C = class {})
  Super,                     ///< superキーワード
  ImportExpression,          ///< 動的インポート (import())

  // --- JSX (有効な場合) ---
  JsxElement,              ///< JSX要素 (<div />)
  JsxOpeningElement,       ///< JSX開始タグ (<div>)
  JsxClosingElement,       ///< JSX終了タグ (</div>)
  JsxAttribute,            ///< JSX属性 (attr="value")
  JsxSpreadAttribute,      ///< JSXスプレッド属性 ({...props})
  JsxExpressionContainer,  ///< JSX式コンテナ ({expr})
  JsxFragment,             ///< JSXフラグメント (<>...</>)
  JsxText,                 ///< JSXテキストノード

  // --- TypeScript (有効な場合) ---
  TsTypeAnnotation,                 ///< 型アノテーション (: string)
  TsTypeReference,                  ///< 型参照 (Array<string>)
  TsParameterProperty,              ///< パラメータプロパティ (constructor(public name: string))
  TsDeclareFunction,                ///< declare function
  TsDeclareMethod,                  ///< declare method
  TsQualifiedName,                  ///< 修飾名 (A.B)
  TsCallSignatureDeclaration,       ///< 呼び出しシグネチャ宣言
  TsConstructSignatureDeclaration,  ///< コンストラクトシグネチャ宣言
  TsPropertySignature,              ///< プロパティシグネチャ
  TsMethodSignature,                ///< メソッドシグネチャ
  TsIndexSignature,                 ///< インデックスシグネチャ
  TsTypePredicate,                  ///< 型述語 (arg is string)
  TsNonNullExpression,              ///< Non-null アサーション (expr!)
  TsAsExpression,                   ///< As式 (expr as Type)
  TsSatisfiesExpression,            ///< Satisfies式 (expr satisfies Type)
  TsTypeAliasDeclaration,           ///< 型エイリアス宣言 (type T = ...)
  TsInterfaceDeclaration,           ///< インターフェース宣言 (interface I { ... })
  TsInterfaceBody,                  ///< インターフェース本体
  TsEnumDeclaration,                ///< Enum宣言 (enum E { ... })
  TsEnumMember,                     ///< Enumメンバ
  TsModuleDeclaration,              ///< モジュール/名前空間宣言 (module/namespace M { ... })
  TsModuleBlock,                    ///< モジュール/名前空間ブロック
  TsImportType,                     ///< import type
  TsImportEqualsDeclaration,        ///< import = require()
  TsExternalModuleReference,        ///< 外部モジュール参照 (require())
  TsTypeParameterDeclaration,       ///< 型パラメータ宣言 (<T>)
  TsTypeParameterInstantiation,     ///< 型パラメータインスタンス化 (<string>)
  TsTypeParameter,                  ///< 型パラメータ (T extends K)
  TsConditionalType,                ///< 条件型 (T extends U ? X : Y)
  TsInferType,                      ///< 推論型 (infer R)
  TsParenthesizedType,              ///< 括弧付き型 ((string | number))
  TsFunctionType,                   ///< 関数型 (() => void)
  TsConstructorType,                ///< コンストラクタ型 (new () => T)
  TsTypeLiteral,                    ///< 型リテラル ({ a: string })
  TsArrayType,                      ///< 配列型 (string[])
  TsTupleType,                      ///< タプル型 ([string, number])
  TsOptionalType,                   ///< オプショナル型 (T?)
  TsRestType,                       ///< 残余型 (...T[])
  TsUnionType,                      ///< ユニオン型 (string | number)
  TsIntersectionType,               ///< 交差型 (A & B)
  TsIndexedAccessType,              ///< インデックスアクセス型 (T[K])
  TsMappedType,                     ///< マップ型 ({ [P in K]: T })
  TsLiteralType,                    ///< リテラル型 ('hello', 123)
  TsThisType,                       ///< this型 (this)
  TsTypeOperator,                   ///< 型演算子 (keyof, typeof, readonly, unique)
  TsTemplateLiteralType,            ///< テンプレートリテラル型 (`a${T}b`)
  TsDecorator,                      ///< デコレーター (@decorator)

  // --- カウント ---
  Count  ///< ノードタイプの総数 (必ず最後に配置)
};

/**
 * @brief NodeType を文字列に変換します。
 * @param type 変換するノードタイプ。
 * @return ノードタイプを表す文字列。不明な場合は "Unknown"。
 */
const char* nodeTypeToString(NodeType type);

// NodeType カテゴリ判定ヘルパー関数
inline bool isStatement(NodeType type);
inline bool isExpression(NodeType type);
inline bool isDeclaration(NodeType type);
inline bool isPattern(NodeType type);

/**
 * @class Node
 * @brief ASTの全てのノードの基底となる抽象クラス。
 *
 * @details
 * このクラスは、ASTノードに共通の機能を提供します。
 * - ノードタイプ (`NodeType`)
 * - ソースコード上の位置情報 (`SourceLocation`)
 * - 親ノードへのポインタ (オプション)
 * - 子ノードのリスト (オプション、具象クラスで管理)
 * - ビジターパターンのための `accept` メソッド
 *
 * このクラスは抽象クラスであり、直接インスタンス化することはできません。
 * 全ての具象ASTノードクラスは、このクラスまたは適切な中間基底クラス
 * (StatementNode, ExpressionNode など) を継承する必要があります。
 *
 * メモリ管理:
 * Nodeオブジェクトの所有権は、通常 `std::unique_ptr<Node>` を使用して管理されます。
 * 親ノードへのポインタ (`m_parent`) は非所有ポインタです。
 *
 * スレッド安全性:
 * Nodeオブジェクト自体はスレッドセーフではありません。
 * ASTの構築や変更は、単一のスレッドで行われることを想定しています。
 * ASTの読み取りは、変更が行われていない限り複数のスレッドから安全に行えます。
 */
class Node {
 public:
  /**
   * @brief デフォルトコンストラクタ (禁止)。
   * @details ノードタイプと位置情報は必須のため、デフォルトコンストラクタは削除されます。
   */
  Node() = delete;

  /**
   * @brief Nodeのコンストラクタ。
   * @param type このノードのタイプ。
   * @param location ソースコード内のこのノードの位置情報。
   * @param parent このノードの親ノードへのポインタ (オプション)。デフォルトはnullptr。
   *               親ノードへのポインタは所有権を持ちません。
   */
  explicit Node(NodeType type,
                const SourceLocation& location,
                Node* parent = nullptr);

  /**
   * @brief デストラクタ (仮想)。
   * @details 継承されるクラスのため、デストラクタは仮想である必要があります。
   */
  virtual ~Node() = default;

  /**
   * @brief コピーコンストラクタ (禁止)。
   * @details ASTノードは通常ユニークであり、コピーセマンティクスは複雑になるため禁止します。
   *          必要な場合は明示的なクローンメソッドを実装します。
   */
  Node(const Node&) = delete;

  /**
   * @brief コピー代入演算子 (禁止)。
   */
  Node& operator=(const Node&) = delete;

  /**
   * @brief ムーブコンストラクタ (デフォルト)。
   */
  Node(Node&&) noexcept = default;

  /**
   * @brief ムーブ代入演算子 (デフォルト)。
   */
  Node& operator=(Node&&) noexcept = default;

  /**
   * @brief このノードのタイプを取得します。
   * @return ノードタイプ (`NodeType`)。
   */
  NodeType getType() const noexcept;

  /**
   * @brief このノードのソースコード上の位置情報を取得します。
   * @return 位置情報 (`SourceLocation`)。
   */
  const SourceLocation& getLocation() const noexcept;

  /**
   * @brief このノードのソースコード上の開始オフセットを取得します。
   * @return 開始オフセット。
   */
  size_t getStartOffset() const noexcept;

  /**
   * @brief このノードのソースコード上の終了オフセットを取得します。
   * @return 終了オフセット。
   */
  size_t getEndOffset() const noexcept;

  /**
   * @brief このノードの親ノードへのポインタを取得します。
   * @return 親ノードへのポインタ。親が存在しない場合はnullptr。
   *         返されるポインタは所有権を持ちません。
   */
  Node* getParent() const noexcept;

  /**
   * @brief このノードの親ノードを設定します (内部使用推奨)。
   * @param parent 新しい親ノードへのポインタ。
   * @warning このメソッドはAST構築時に内部的に使用されることを想定しています。
   *          所有権は移譲されません。
   */
  void setParent(Node* parent) noexcept;

  /**
   * @brief ビジターを受け入れ、適切なVisitメソッドを呼び出します。
   * @param visitor ASTビジターのインスタンス。
   * @note 純粋仮想関数であり、全ての具象ノードクラスで実装される必要があります。
   */
  virtual void accept(AstVisitor& visitor) = 0;

  /**
   * @brief const ビジターを受け入れ、適切なVisitメソッドを呼び出します。
   * @param visitor const ASTビジターのインスタンス。
   * @note 純粋仮想関数であり、全ての具象ノードクラスで実装される必要があります。
   */
  virtual void accept(ConstAstVisitor& visitor) const = 0;

  /**
   * @brief このノードの子ノードのリストを取得します (非所有ポインタ)。
   * @return 子ノードのベクター。子を持たない場合は空のベクター。
   * @note 具象クラスでオーバーライドされる必要があります。
   *       返されるポインタは所有権を持ちません。デフォルト実装は空のベクターを返します。
   */
  virtual std::vector<Node*> getChildren();

  /**
   * @brief このノードの子ノードのリストを取得します (const 非所有ポインタ)。
   * @return const 子ノードのベクター。子を持たない場合は空のベクター。
   * @note 具象クラスでオーバーライドされる必要があります。
   *       返されるポインタは所有権を持ちません。デフォルト実装は空のベクターを返します。
   */
  virtual std::vector<const Node*> getChildren() const;

  /**
   * @brief このノードをJSONオブジェクトに変換します。
   * @param pretty trueの場合、整形されたJSONを出力します。
   * @return ノードを表す nlohmann::json オブジェクト。
   * @note 具象クラスはこのメソッドをオーバーライドして、
   *       自身のプロパティを追加する必要があります。基底クラスの実装は
   *       `type` と `loc` を含む基本的なJSONオブジェクトを生成します。
   */
  virtual nlohmann::json toJson(bool pretty = false) const;

  /**
   * @brief ノードの基本的な文字列表現を取得します (デバッグ用)。
   * @return ノードの文字列表現。例: "Identifier[0:5]"
   */
  virtual std::string toString() const;

 protected:
  NodeType m_type;            ///< このノードのタイプ
  SourceLocation m_location;  ///< ソースコード内の位置情報
  Node* m_parent;             ///< 親ノードへの非所有ポインタ (nullptrの場合あり)

  // ヘルパー関数: JSONシリアライズ用
  void baseJson(nlohmann::json& jsonNode) const;
};

/**
 * @class StatementNode
 * @brief 全ての文ノードの基底クラス。
 */
class StatementNode : public Node {
 public:
  using Node::Node;  // 親クラスのコンストラクタを継承
  StatementNode(NodeType type, const SourceLocation& location, Node* parent = nullptr);
};

/**
 * @class ExpressionNode
 * @brief 全ての式ノードの基底クラス。
 */
class ExpressionNode : public Node {
 public:
  using Node::Node;  // 親クラスのコンストラクタを継承
  ExpressionNode(NodeType type, const SourceLocation& location, Node* parent = nullptr);
};

/**
 * @class DeclarationNode
 * @brief 全ての宣言ノードの基底クラス。
 */
class DeclarationNode : public Node {
 public:
  using Node::Node;  // 親クラスのコンストラクタを継承
  DeclarationNode(NodeType type, const SourceLocation& location, Node* parent = nullptr);
};

/**
 * @class PatternNode
 * @brief 全てのパターンノード (分割代入など) の基底クラス。
 */
class PatternNode : public Node {
 public:
  using Node::Node;  // 親クラスのコンストラクタを継承
  PatternNode(NodeType type, const SourceLocation& location, Node* parent = nullptr);
};

// スマートポインタ型のエイリアス
using NodePtr = std::unique_ptr<Node>;
// const版のunique_ptr。unique_ptrはconst伝播するため、通常 const unique_ptr<Node> で十分なことが多い。
using ConstNodePtr = std::unique_ptr<const Node>;

// --- カテゴリ判定関数のインライン実装 ---

inline bool isStatement(NodeType type) {
  // NodeType::BlockStatement から NodeType::DebuggerStatement までの範囲が文に対応
  return type >= NodeType::BlockStatement && type <= NodeType::DebuggerStatement;
}

inline bool isExpression(NodeType type) {
  // Identifier と PrivateIdentifier は文脈によって式にもパターンにもなりうるが、
  // ここでは便宜上 Expression に含める (パターンチェックは isPattern で行う)
  // NodeType::Identifier から NodeType::ImportExpression までの範囲が式に対応
  return type >= NodeType::Identifier && type <= NodeType::ImportExpression;
}

inline bool isDeclaration(NodeType type) {
  // NodeType::FunctionDeclaration から NodeType::ExportSpecifier までの範囲が宣言に対応
  return type >= NodeType::FunctionDeclaration && type <= NodeType::ExportSpecifier;
}

inline bool isPattern(NodeType type) {
  // Identifier はパターンにもなりうる
  // SpreadElement は式の一部であり、パターンではない
  return type == NodeType::Identifier ||
         type == NodeType::AssignmentPattern ||
         type == NodeType::ArrayPattern ||
         type == NodeType::ObjectPattern ||
         type == NodeType::RestElement;
}

}  // namespace ast
}  // namespace parser
}  // namespace core
}  // namespace aerojs