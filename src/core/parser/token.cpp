/**
 * @file token.cpp
 * @brief JavaScript言語のトークン定義の実装 (高性能拡張版)
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "token.h"

#include <algorithm>  // std::min, std::max のため
#include <array>      // テーブルのため
#include <cassert>
#include <cctype>    // std::is... 関数のため
#include <cerrno>    // errno のため
#include <charconv>  // std::from_chars のため (より安全な数値変換)
#include <chrono>
#include <cmath>    // std::isinf, std::isnan, std::pow のため
#include <codecvt>  // Unicode変換のため (将来的な拡張用)
#include <cstdlib>  // strtod, strtoll のため
#include <cstring>  // memcpy, memcmp, strlen のため
#include <iomanip>
#include <limits>  // std::numeric_limits のため
#include <locale>  // Unicode変換のため (将来的な拡張用)
#include <mutex>   // 将来的な並列化のための std::mutex
#include <sstream>
#include <thread>  // 将来的な並列化のための std::thread
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "parser_error.h"  // エラー情報などを定義する parser_error.h を想定

// SIMDインクルード (プレースホルダー、実際のターゲットアーキテクチャに依存)
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

// Unicode サポートのための仮定義 (完全な実装には ICU などのライブラリが必要)
// ここでは基本的な ASCII 範囲といくつかの代表的な Unicode 文字のみを扱う
namespace Unicode {
// 仮の Unicode ID_Start チェック
bool isIdentifierStart(uint32_t cp) {
  if (cp < 128) {  // ASCII範囲はテーブルでチェック
    return aerojs::core::lexer::Scanner::isIdentifierStart(cp);
  }
  // ここに完全な Unicode ID_Start チェックを実装 (例: '$', '_', または Unicode Letter カテゴリ)
  // 簡略化のため、ここでは基本的な文字のみ許可
  return (cp == '$' || cp == '_');  // '$' と '_' は ASCII テーブルに含まれるが、例として示す
                                    // 完全な実装には Unicode Character Database (UCD) が必要
}

// 仮の Unicode ID_Part チェック
bool isIdentifierPart(uint32_t cp) {
  if (cp < 128) {  // ASCII範囲はテーブルでチェック
    return aerojs::core::lexer::Scanner::isIdentifierPart(cp);
  }
  // ここに完全な Unicode ID_Part チェックを実装 (例: ID_Start 文字、Mn, Mc, Nd, Pc カテゴリ)
  // 簡略化のため、ここでは基本的な文字のみ許可
  return isIdentifierStart(cp) || (cp >= '0' && cp <= '9');  // 数字は ASCII テーブルに含まれるが、例として示す
                                                             // 完全な実装には Unicode Character Database (UCD) が必要
}

// 仮の Unicode Whitespace チェック
bool isWhitespace(uint32_t cp) {
  if (cp < 128) {  // ASCII範囲はテーブルでチェック
    return aerojs::core::lexer::Scanner::isWhitespace(cp);
  }
  // ECMAScript Spec Section 12.3 White Space
  return (cp == 0x0009 || cp == 0x000B || cp == 0x000C || cp == 0x0020 || cp == 0x00A0 || cp == 0xFEFF ||
          // Unicode Category "Zs" (Space Separator) の一部
          cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F || cp == 0x205F || cp == 0x3000);
  // 完全な実装には Unicode Character Database (UCD) が必要
}

// 仮の Unicode Line Terminator チェック
bool isLineTerminator(uint32_t cp) {
  // ECMAScript Spec Section 12.4 Line Terminators
  return cp == 0x000A || cp == 0x000D || cp == 0x2028 || cp == 0x2029;
}
}  // namespace Unicode

namespace aerojs {
namespace core {

//------------------------------------------------------------------------------
// トークンタイプユーティリティの実装
//------------------------------------------------------------------------------

// TokenType から文字列表現へのマップ (デバッグ/エラー用)
// 静的初期化時に構築されるため、実行時オーバーヘッドは小さい
static const std::unordered_map<TokenType, const char*> kTokenTypeStrings = {
    {TokenType::Eof, "EOF"}, {TokenType::Error, "Error"}, {TokenType::Uninitialized, "Uninitialized"}, {TokenType::Identifier, "Identifier"}, {TokenType::PrivateIdentifier, "PrivateIdentifier"}, {TokenType::NumericLiteral, "NumericLiteral"}, {TokenType::StringLiteral, "StringLiteral"}, {TokenType::TemplateHead, "TemplateHead"}, {TokenType::TemplateMiddle, "TemplateMiddle"}, {TokenType::TemplateTail, "TemplateTail"}, {TokenType::RegExpLiteral, "RegExpLiteral"}, {TokenType::BigIntLiteral, "BigIntLiteral"}, {TokenType::NullLiteral, "NullLiteral"}, {TokenType::TrueLiteral, "TrueLiteral"}, {TokenType::FalseLiteral, "FalseLiteral"}, {TokenType::Await, "await"}, {TokenType::Break, "break"}, {TokenType::Case, "case"}, {TokenType::Catch, "catch"}, {TokenType::Class, "class"}, {TokenType::Const, "const"}, {TokenType::Continue, "continue"}, {TokenType::Debugger, "debugger"}, {TokenType::Default, "default"}, {TokenType::Delete, "delete"}, {TokenType::Do, "do"}, {TokenType::Else, "else"}, {TokenType::Enum, "enum"}, {TokenType::Export, "export"}, {TokenType::Extends, "extends"}, {TokenType::Finally, "finally"}, {TokenType::For, "for"}, {TokenType::Function, "function"}, {TokenType::If, "if"}, {TokenType::Import, "import"}, {TokenType::In, "in"}, {TokenType::InstanceOf, "instanceof"}, {TokenType::Let, "let"}, {TokenType::New, "new"}, {TokenType::Return, "return"}, {TokenType::Super, "super"}, {TokenType::Switch, "switch"}, {TokenType::This, "this"}, {TokenType::Throw, "throw"}, {TokenType::Try, "try"}, {TokenType::Typeof, "typeof"}, {TokenType::Var, "var"}, {TokenType::Void, "void"}, {TokenType::While, "while"}, {TokenType::With, "with"}, {TokenType::Yield, "yield"}, {TokenType::Async, "async"}, {TokenType::Get, "get"}, {TokenType::Set, "set"}, {TokenType::Static, "static"}, {TokenType::Of, "of"}, {TokenType::From, "from"}, {TokenType::As, "as"}, {TokenType::Meta, "meta"}, {TokenType::Target, "target"}, {TokenType::Implements, "implements"}, {TokenType::Interface, "interface"}, {TokenType::Package, "package"}, {TokenType::Private, "private"}, {TokenType::Protected, "protected"}, {TokenType::Public, "public"}, {TokenType::LeftParen, "("}, {TokenType::RightParen, ")"}, {TokenType::LeftBracket, "["}, {TokenType::RightBracket, "]"}, {TokenType::LeftBrace, "{"}, {TokenType::RightBrace, "}"}, {TokenType::Colon, ":"}, {TokenType::Semicolon, ";"}, {TokenType::Comma, ","}, {TokenType::Dot, "."}, {TokenType::DotDotDot, "..."}, {TokenType::QuestionMark, "?"}, {TokenType::QuestionDot, "?."}, {TokenType::QuestionQuestion, "??"}, {TokenType::Arrow, "=>"}, {TokenType::Tilde, "~"}, {TokenType::Exclamation, "!"}, {TokenType::Assign, "="}, {TokenType::Equal, "=="}, {TokenType::NotEqual, "!="}, {TokenType::StrictEqual, "==="}, {TokenType::StrictNotEqual, "!=="}, {TokenType::Plus, "+"}, {TokenType::Minus, "-"}, {TokenType::Star, "*"}, {TokenType::Slash, "/"}, {TokenType::Percent, "%"}, {TokenType::StarStar, "**"}, {TokenType::PlusPlus, "++"}, {TokenType::MinusMinus, "--"}, {TokenType::LeftShift, "<<"}, {TokenType::RightShift, ">>"}, {TokenType::UnsignedRightShift, ">>>"}, {TokenType::Ampersand, "&"}, {TokenType::Bar, "|"}, {TokenType::Caret, "^"}, {TokenType::AmpersandAmpersand, "&&"}, {TokenType::BarBar, "||"}, {TokenType::PlusAssign, "+="}, {TokenType::MinusAssign, "-="}, {TokenType::StarAssign, "*="}, {TokenType::SlashAssign, "/="}, {TokenType::PercentAssign, "%="}, {TokenType::StarStarAssign, "**="}, {TokenType::LeftShiftAssign, "<<="}, {TokenType::RightShiftAssign, ">>="}, {TokenType::UnsignedRightShiftAssign, ">>>="}, {TokenType::AmpersandAssign, "&="}, {TokenType::BarAssign, "|="}, {TokenType::CaretAssign, "^="}, {TokenType::AmpersandAmpersandAssign, "&&="}, {TokenType::BarBarAssign, "||="}, {TokenType::QuestionQuestionAssign, "??="}, {TokenType::LessThan, "<"}, {TokenType::GreaterThan, ">"}, {TokenType::LessThanEqual, "<="}, {TokenType::GreaterThanEqual, ">="}, {TokenType::JsxIdentifier, "JsxIdentifier"}, {TokenType::JsxText, "JsxText"}, {TokenType::JsxTagStart, "JsxTagStart"}, {TokenType::JsxTagEnd, "JsxTagEnd"}, {TokenType::JsxClosingTagStart, "JsxClosingTagStart"}, {TokenType::JsxSelfClosingTagEnd, "JsxSelfClosingTagEnd"}, {TokenType::JsxAttributeEquals, "JsxAttributeEquals"}, {TokenType::JsxSpreadAttribute, "JsxSpreadAttribute"},
    // TypeScriptトークンタイプの追加
    {TokenType::TsQuestionMark, "TsQuestionMark"},
    {TokenType::TsColon, "TsColon"},
    {TokenType::TsReadonly, "TsReadonly"},
    {TokenType::TsNumber, "TsNumber"},
    {TokenType::TsString, "TsString"},
    {TokenType::TsBoolean, "TsBoolean"},
    {TokenType::TsVoid, "TsVoid"},
    {TokenType::TsAny, "TsAny"},
    {TokenType::TsUnknown, "TsUnknown"},
    {TokenType::TsNever, "TsNever"},
    {TokenType::TsType, "TsType"},
    {TokenType::TsInterface, "TsInterface"},
    {TokenType::TsImplements, "TsImplements"},
    {TokenType::TsExtends, "TsExtends"},
    {TokenType::TsAbstract, "TsAbstract"},
    {TokenType::TsPublic, "TsPublic"},
    {TokenType::TsPrivate, "TsPrivate"},
    {TokenType::TsProtected, "TsProtected"},
    {TokenType::TsDeclare, "TsDeclare"},
    {TokenType::TsAs, "TsAs"},
    {TokenType::TsSatisfies, "TsSatisfies"},
    {TokenType::TsInfer, "TsInfer"},
    {TokenType::TsKeyof, "TsKeyof"},
    {TokenType::TsTypeof, "TsTypeof"},
    {TokenType::TsNonNullAssertion, "TsNonNullAssertion"},
    {TokenType::TsDecorator, "TsDecorator"},
    // コメント関連
    {TokenType::SingleLineComment, "SingleLineComment"},
    {TokenType::MultiLineComment, "MultiLineComment"},
    {TokenType::HtmlComment, "HtmlComment"},
    // カウント
    {TokenType::Count, "Count"}};

const char* tokenTypeToString(TokenType type) {
  auto it = kTokenTypeStrings.find(type);
  if (it != kTokenTypeStrings.end()) {
    return it->second;
  }
  static char unknown_buffer[64];
  snprintf(unknown_buffer, sizeof(unknown_buffer), "UnknownTokenType(%u)", static_cast<uint16_t>(type));
  return unknown_buffer;
}

// 演算子の優先順位マップ（token.hのマップと完全に一致することを確認）
static const std::unordered_map<TokenType, uint16_t> kOperatorPrecedence = {
    {TokenType::QuestionQuestion, 1}, {TokenType::BarBar, 1},  // 論理OR、Nullish合体演算子
    {TokenType::AmpersandAmpersand, 2},                        // 論理AND
    {TokenType::Bar, 3},                                       // ビット単位OR
    {TokenType::Caret, 4},                                     // ビット単位XOR
    {TokenType::Ampersand, 5},                                 // ビット単位AND
    {TokenType::Equal, 6},
    {TokenType::NotEqual, 6},
    {TokenType::StrictEqual, 6},
    {TokenType::StrictNotEqual, 6},  // 等価性
    {TokenType::LessThan, 7},
    {TokenType::GreaterThan, 7},
    {TokenType::LessThanEqual, 7},
    {TokenType::GreaterThanEqual, 7},
    {TokenType::In, 7},
    {TokenType::InstanceOf, 7},  // 関係演算子、In、InstanceOf
    {TokenType::LeftShift, 8},
    {TokenType::RightShift, 8},
    {TokenType::UnsignedRightShift, 8},  // ビットシフト
    {TokenType::Plus, 9},
    {TokenType::Minus, 9},  // 加算
    {TokenType::Star, 10},
    {TokenType::Slash, 10},
    {TokenType::Percent, 10},   // 乗算
    {TokenType::StarStar, 11},  // べき乗（右結合、パーサーで特別に処理）
};

uint16_t getOperatorPrecedence(TokenType type) {
  auto it = kOperatorPrecedence.find(type);
  return (it != kOperatorPrecedence.end()) ? it->second : 0;
}

//------------------------------------------------------------------------------
// スキャナー実装
//------------------------------------------------------------------------------
namespace lexer {

// キーワード検索テーブル（最適化が必要）
struct KeywordInfo {
  TokenType type;
  uint32_t flags;
};
static const std::unordered_map<std::string, KeywordInfo> kKeywords = {
    // 主要キーワード
    {"await", {TokenType::Await, Token::FlagIsKeyword | Token::FlagIsReservedWord}},  // コンテキスト依存
    {"break", {TokenType::Break, Token::FlagIsKeyword}},
    {"case", {TokenType::Case, Token::FlagIsKeyword}},
    {"catch", {TokenType::Catch, Token::FlagIsKeyword}},
    {"class", {TokenType::Class, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"const", {TokenType::Const, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"continue", {TokenType::Continue, Token::FlagIsKeyword}},
    {"debugger", {TokenType::Debugger, Token::FlagIsKeyword}},
    {"default", {TokenType::Default, Token::FlagIsKeyword}},
    {"delete", {TokenType::Delete, Token::FlagIsKeyword | Token::FlagIsUnaryOperator}},
    {"do", {TokenType::Do, Token::FlagIsKeyword}},
    {"else", {TokenType::Else, Token::FlagIsKeyword}},
    {"enum", {TokenType::Enum, Token::FlagIsReservedWord}},
    {"export", {TokenType::Export, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"extends", {TokenType::Extends, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"finally", {TokenType::Finally, Token::FlagIsKeyword}},
    {"for", {TokenType::For, Token::FlagIsKeyword}},
    {"function", {TokenType::Function, Token::FlagIsKeyword}},
    {"if", {TokenType::If, Token::FlagIsKeyword}},
    {"import", {TokenType::Import, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"in", {TokenType::In, Token::FlagIsKeyword | Token::FlagIsBinaryOperator}},
    {"instanceof", {TokenType::InstanceOf, Token::FlagIsKeyword | Token::FlagIsBinaryOperator}},
    {"let", {TokenType::Let, Token::FlagIsKeyword | Token::FlagIsStrictReservedWord}},
    {"new", {TokenType::New, Token::FlagIsKeyword}},
    {"return", {TokenType::Return, Token::FlagIsKeyword}},
    {"super", {TokenType::Super, Token::FlagIsKeyword | Token::FlagIsReservedWord}},
    {"switch", {TokenType::Switch, Token::FlagIsKeyword}},
    {"this", {TokenType::This, Token::FlagIsKeyword}},
    {"throw", {TokenType::Throw, Token::FlagIsKeyword}},
    {"try", {TokenType::Try, Token::FlagIsKeyword}},
    {"typeof", {TokenType::Typeof, Token::FlagIsKeyword | Token::FlagIsUnaryOperator}},
    {"var", {TokenType::Var, Token::FlagIsKeyword}},
    {"void", {TokenType::Void, Token::FlagIsKeyword | Token::FlagIsUnaryOperator}},
    {"while", {TokenType::While, Token::FlagIsKeyword}},
    {"with", {TokenType::With, Token::FlagIsKeyword}},
    {"yield", {TokenType::Yield, Token::FlagIsKeyword | Token::FlagIsStrictReservedWord}},  // コンテキスト依存
    // コンテキスト依存キーワード
    {"async", {TokenType::Async, Token::FlagIsContextualKeyword}},
    {"get", {TokenType::Get, Token::FlagIsContextualKeyword}},
    {"set", {TokenType::Set, Token::FlagIsContextualKeyword}},
    {"static", {TokenType::Static, Token::FlagIsContextualKeyword | Token::FlagIsStrictReservedWord}},
    {"of", {TokenType::Of, Token::FlagIsContextualKeyword}},
    {"from", {TokenType::From, Token::FlagIsContextualKeyword}},
    {"as", {TokenType::As, Token::FlagIsContextualKeyword}},
    {"meta", {TokenType::Meta, Token::FlagIsContextualKeyword}},
    {"target", {TokenType::Target, Token::FlagIsContextualKeyword}},
    // 予約語
    {"implements", {TokenType::Implements, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"interface", {TokenType::Interface, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"package", {TokenType::Package, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"private", {TokenType::Private, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"protected", {TokenType::Protected, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"public", {TokenType::Public, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    // キーワードとして扱われるリテラル
    {"null", {TokenType::NullLiteral, Token::FlagIsKeyword}},
    {"true", {TokenType::TrueLiteral, Token::FlagIsKeyword}},
    {"false", {TokenType::FalseLiteral, Token::FlagIsKeyword}},
    // TypeScriptキーワード（必要なフラグを追加）
    {"readonly", {TokenType::TsReadonly, 0}},
    {"number", {TokenType::TsNumber, 0}},
    {"string", {TokenType::TsString, 0}},
    {"boolean", {TokenType::TsBoolean, 0}},
    {"any", {TokenType::TsAny, 0}},
    {"unknown", {TokenType::TsUnknown, 0}},
    {"never", {TokenType::TsNever, 0}},
    {"type", {TokenType::TsType, 0}},
    {"declare", {TokenType::TsDeclare, 0}},
    {"satisfies", {TokenType::TsSatisfies, 0}},
    {"infer", {TokenType::TsInfer, 0}},
    {"keyof", {TokenType::TsKeyof, 0}},
    {"abstract", {TokenType::TsAbstract, 0}},
};

// --- 文字分類テーブル/関数 ---
static constexpr uint8_t CP_WS = 1 << 0;   // 空白
static constexpr uint8_t CP_LT = 1 << 1;   // 行終端子
static constexpr uint8_t CP_IDS = 1 << 2;  // 識別子開始
static constexpr uint8_t CP_IDP = 1 << 3;  // 識別子部分
static constexpr uint8_t CP_DEC = 1 << 4;  // 10進数字
static constexpr uint8_t CP_HEX = 1 << 5;  // 16進数字

static const uint8_t kAsciiCharProperties[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, CP_WS, CP_LT, CP_WS, CP_WS, CP_LT, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CP_WS, 0, 0, 0, CP_IDS | CP_IDP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP,
    CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP, CP_DEC | CP_HEX | CP_IDP,
    0, 0, 0, 0, 0, 0,
    0,
    CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    0, CP_IDS | CP_IDP, 0, 0, CP_IDS | CP_IDP, 0,
    CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX, CP_IDS | CP_IDP | CP_HEX,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP, CP_IDS | CP_IDP,
    0, 0, 0, 0, 0};

inline bool isDecimalDigit(uint32_t c) {
  return c < 128 && (kAsciiCharProperties[c] & CP_DEC);
}
inline bool isHexDigit(uint32_t c) {
  return c < 128 && (kAsciiCharProperties[c] & CP_HEX);
}
inline bool isBinaryDigit(uint32_t c) {
  return c == '0' || c == '1';
}
inline bool isOctalDigit(uint32_t c) {
  return c >= '0' && c <= '7';
}

bool Scanner::isIdentifierStart(uint32_t c) {
  if (c < 128) return kAsciiCharProperties[c] & CP_IDS;
  // 完全なUnicodeチェック
  return Unicode::isIdentifierStart(c);
}
bool Scanner::isIdentifierPart(uint32_t c) {
  if (c < 128) return kAsciiCharProperties[c] & CP_IDP;
  // 完全なUnicodeチェック
  return Unicode::isIdentifierPart(c);
}
bool Scanner::isLineTerminator(uint32_t c) {
  return c == 0x0A || c == 0x0D || c == 0x2028 || c == 0x2029;
}
bool Scanner::isWhitespace(uint32_t c) {
  if (c < 128) return kAsciiCharProperties[c] & CP_WS;
  // 完全なUnicodeチェック
  return Unicode::isWhitespace(c);
}

// --- UTF8ユーティリティ実装 ---
namespace utf8 {
int bytesForChar(uint8_t b1) {
  if (b1 < 0x80) return 1;
  if ((b1 & 0xE0) == 0xC0) return 2;
  if ((b1 & 0xF0) == 0xE0) return 3;
  if ((b1 & 0xF8) == 0xF0) return 4;
  return 0;  // 無効なUTF-8シーケンス
}

uint32_t decodeChar(const char*& ptr, const char* end) {
  if (ptr >= end) return 0;
  const uint8_t* uptr = reinterpret_cast<const uint8_t*>(ptr);
  uint8_t b1 = *uptr;
  int len = bytesForChar(b1);

  // 無効なシーケンスまたは境界外の場合
  if (len == 0 || (ptr + len > end)) {
    ptr++;
    return 0xFFFD;  // Unicode置換文字
  }

  uint32_t cp = 0;
  if (len == 0 || (ptr + len > end)) {
    ptr++;
    return 0xFFFD;
  }
  uint32_t cp = 0;
  const uint8_t* nb = uptr + 1;
  bool overlong = false;
  uint32_t min_val = 0;
  switch (len) {
    case 1:
      cp = b1;
      break;
    case 2:
      if ((nb[0] & 0xC0) != 0x80) {
        len = 0;
        break;
      }
      cp = ((b1 & 0x1F) << 6) | (nb[0] & 0x3F);
      min_val = 0x80;
      overlong = (cp < min_val);
      break;
    case 3:
      if ((nb[0] & 0xC0) != 0x80 || (nb[1] & 0xC0) != 0x80) {
        len = 0;
        break;
      }
      cp = ((b1 & 0x0F) << 12) | ((nb[0] & 0x3F) << 6) | (nb[1] & 0x3F);
      min_val = 0x800;
      overlong = (cp < min_val);
      break;
    case 4:
      if ((nb[0] & 0xC0) != 0x80 || (nb[1] & 0xC0) != 0x80 || (nb[2] & 0xC0) != 0x80) {
        len = 0;
        break;
      }
      cp = ((b1 & 0x07) << 18) | ((nb[0] & 0x3F) << 12) | ((nb[1] & 0x3F) << 6) | (nb[2] & 0x3F);
      min_val = 0x10000;
      overlong = (cp < min_val);
      break;
    default:
      len = 0;
      break;
  }
  if (len == 0 || overlong || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
    ptr++;
    return 0xFFFD;
  }
  ptr += len;
  return cp;
}
bool Utf8Utils::isValidUtf8(const char* ptr, size_t length) {
  const char* end = ptr + length;
  while (ptr < end) {
    const char* current = ptr;
    if (decodeChar(ptr, end) == 0xFFFD && ptr == current + 1) return false;
  }
  return true;
}
}  // namespace token_h_filler

// --- スキャナーのコンストラクタ/デストラクタ実装 ---
Scanner::Scanner(const char* source, size_t source_length, int file_id,
                 ScannerErrorHandler* error_handler, ScannerContext initial_context)
    : source_start_(source), source_end_(source + source_length), current_pos_(source), token_start_pos_(source), error_handler_(error_handler), file_id_(file_id), context_(initial_context), simd_enabled_(false), parallel_scan_active_(false) {
  current_location_ = {1, 0, 0, file_id_};
  current_token_.type = TokenType::Uninitialized;
  bytes_scanned_.store(0, std::memory_order_relaxed);
  tokens_emitted_.store(0, std::memory_order_relaxed);
  perf_timers_ = {};
  readUtf8Char();  // 先読み文字を初期化
}

Scanner::~Scanner() {
}

// --- コアスキャン処理の実装 ---
void Scanner::advance(int bytes) {
  if (bytes <= 0) return;
  size_t remaining = source_end_ - current_pos_;
  size_t consumed = std::min(static_cast<size_t>(bytes), remaining);
  current_pos_ += consumed;
  current_location_.offset += consumed;
}

void Scanner::readUtf8Char() {
  const char* next_pos = current_pos_;
  if (next_pos < source_end_) {
    lookahead_char_ = utf8::decodeChar(next_pos, source_end_);
    lookahead_size_ = static_cast<int>(next_pos - current_pos_);
  } else {
    lookahead_char_ = 0;
    lookahead_size_ = 0;
  }
}

void Scanner::consumeChar() {
  if (lookahead_size_ > 0) {
    updateLocation(lookahead_char_);
    advance(lookahead_size_);
    readUtf8Char();
  }
}

void Scanner::updateLocation(uint32_t utf32_char) {
  if (isLineTerminator(utf32_char)) {
    current_location_.line++;
    current_location_.column = 0;
  } else if (utf32_char == '\t') {
    constexpr int TAB_WIDTH = 4;
    current_location_.column = ((current_location_.column / TAB_WIDTH) + 1) * TAB_WIDTH;
  } else {
    current_location_.column++;
  }
}

Token Scanner::nextToken() {
  auto trivia_start_time = std::chrono::high_resolution_clock::now();
  uint32_t previous_flags = current_token_.flags;
  uint16_t trivia_len = skipWhitespaceAndComments();
  auto trivia_end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.whitespaceSkipTime += (trivia_end_time - trivia_start_time);

  token_start_pos_ = current_pos_;
  current_token_.location = current_location_;
  current_token_.trivia_length = trivia_len;
  current_token_.flags = (previous_flags & Token::FlagPrecededByLineTerminator);
  current_token_.value = std::monostate{};
  current_token_.precedence = 0;
  current_token_.raw_lexeme.clear();

  if (current_pos_ >= source_end_) {
    current_token_ = createToken(TokenType::Eof);
    return current_token_;
  }

  auto scan_start_time = std::chrono::high_resolution_clock::now();
  current_token_ = scanNext();
  auto scan_end_time = std::chrono::high_resolution_clock::now();

  auto duration = scan_end_time - scan_start_time;
  // パフォーマンス計測の更新
  if (current_token_.type == TokenType::Identifier)
    perf_timers_.identifierScanTime += duration;
  else if (current_token_.isLiteral())
    perf_timers_.stringScanTime += duration;
  else if (current_token_.isOperator())
    perf_timers_.punctuatorScanTime += duration;

  bytes_scanned_.fetch_add(current_pos_ - token_start_pos_ + trivia_len, std::memory_order_relaxed);
  tokens_emitted_.fetch_add(1, std::memory_order_relaxed);
  return current_token_;
}

// scanNext - メインディスパッチャー（ASCII高速パス + フォールバック）
Token Scanner::scanNext() {
  uint32_t c = lookahead_char_;

  if (c < 128) {
    uint8_t props = kAsciiCharProperties[c];
    if (props & CP_IDENTIFIER_START) return scanIdentifierOrKeyword();
    if (props & CP_DECIMAL_DIGIT) return scanNumericLiteral();
    switch (c) {
      case '\'':
      case '"':
        return scanStringLiteral();
      case '.':
        if (current_pos_ + 1 < source_end_ && isDecimalDigit(static_cast<uint8_t>(current_pos_[1]))) return scanNumericLiteral();
        consumeChar();
        if (lookahead_char_ == '.') {
          consumeChar();
          if (lookahead_char_ == '.') {
            consumeChar();
            return createToken(TokenType::DotDotDot);
          }
          reportError("予期しないトークン '..'");
          return createErrorToken("予期しない '..'");
        }
        return createToken(TokenType::Dot);
      case '`':
        return scanTemplateToken();
      case '#':
        if (current_pos_ + 1 < source_end_ && isIdentifierStart(peekChar(1))) return scanPrivateIdentifier();
        reportError("'#' の後には識別子の開始文字が必要です");
        consumeChar();
        return createErrorToken("無効な '#' の使用");
      case '(':
        consumeChar();
        return createToken(TokenType::LeftParen);
      case ')':
        consumeChar();
        return createToken(TokenType::RightParen);
      case '[':
        consumeChar();
        return createToken(TokenType::LeftBracket);
      case ']':
        consumeChar();
        return createToken(TokenType::RightBracket);
      case '{':
        context_.braceDepth++;
        consumeChar();
        return createToken(TokenType::LeftBrace);
      case '}':
        if (context_.mode == ScannerMode::TemplateLiteral && context_.braceDepth > 0) {
          context_.braceDepth--;
          return scanTemplateToken();
        }
        context_.braceDepth = std::max(0, context_.braceDepth - 1);
        consumeChar();
        return createToken(TokenType::RightBrace);
      case ';':
        consumeChar();
        return createToken(TokenType::Semicolon);
      case ',':
        consumeChar();
        return createToken(TokenType::Comma);
      case ':':
        consumeChar();
        return createToken(TokenType::Colon);
      case '?':
        consumeChar();
        if (lookahead_char_ == '.') {
          consumeChar();
          return createToken(TokenType::QuestionDot);
        }
        if (lookahead_char_ == '?') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::QuestionQuestionAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::QuestionQuestion, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator);
        }
        return createToken(TokenType::QuestionMark);
      case '~':
        consumeChar();
        return createToken(TokenType::Tilde, Token::FlagIsUnaryOperator);
      case '/':
        consumeChar();
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::SlashAssign, Token::FlagIsAssignmentOperator);
        }
        if (lookahead_char_ == '/') {
          skipWhitespaceAndComments();
          return nextToken();
        }
        if (lookahead_char_ == '*') {
          skipWhitespaceAndComments();
          return nextToken();
        }
        if ((context_.mode == ScannerMode::JsxElement || context_.mode == ScannerMode::JsxAttribute) && lookahead_char_ == '>') {
          consumeChar();
          context_.mode = ScannerMode::Normal;
          return createToken(TokenType::JsxSelfClosingTagEnd);
        }
        if (context_.allowRegExp)
          return scanRegExpLiteral();
        else
          return createToken(TokenType::Slash, Token::FlagIsBinaryOperator);
      case '=':
        consumeChar();
        if (lookahead_char_ == '=') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::StrictEqual, Token::FlagIsBinaryOperator);
          }
          return createToken(TokenType::Equal, Token::FlagIsBinaryOperator);
        }
        if (lookahead_char_ == '>') {
          consumeChar();
          return createToken(TokenType::Arrow);
        }
        if (context_.mode == ScannerMode::JsxAttribute) return createToken(TokenType::JsxAttributeEquals);
        return createToken(TokenType::Assign, Token::FlagIsAssignmentOperator);
      case '!':
        consumeChar();
        if (lookahead_char_ == '=') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::StrictNotEqual, Token::FlagIsBinaryOperator);
          }
          return createToken(TokenType::NotEqual, Token::FlagIsBinaryOperator);
        }
        return createToken(TokenType::Exclamation, Token::FlagIsUnaryOperator);
      case '+':
        consumeChar();
        if (lookahead_char_ == '+') {
          consumeChar();
          return createToken(TokenType::PlusPlus, Token::FlagIsUpdateOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::PlusAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Plus, Token::FlagIsBinaryOperator | Token::FlagIsUnaryOperator);
      case '-':
        consumeChar();
        if (lookahead_char_ == '-') {
          consumeChar();
          return createToken(TokenType::MinusMinus, Token::FlagIsUpdateOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::MinusAssign, Token::FlagIsAssignmentOperator);
        }
        if (context_.allowHtmlComment && lookahead_char_ == '-' && current_pos_ + 1 < source_end_ && *(current_pos_ + 1) == '>' && (current_token_.flags & Token::FlagPrecededByLineTerminator)) {
          skipWhitespaceAndComments();
          return nextToken();
        }
        return createToken(TokenType::Minus, Token::FlagIsBinaryOperator | Token::FlagIsUnaryOperator);
      case '*':
        consumeChar();
        if (lookahead_char_ == '*') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::StarStarAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::StarStar, Token::FlagIsBinaryOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::StarAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Star, Token::FlagIsBinaryOperator);
      case '%':
        consumeChar();
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::PercentAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Percent, Token::FlagIsBinaryOperator);
      case '<':
        consumeChar();
        if (context_.allowHtmlComment && lookahead_char_ == '!' && current_pos_ + 2 < source_end_ && current_pos_[1] == '-' && current_pos_[2] == '-') {
          skipWhitespaceAndComments();
          return nextToken();
        }
        if (lookahead_char_ == '<') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::LeftShiftAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::LeftShift, Token::FlagIsBinaryOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::LessThanEqual, Token::FlagIsBinaryOperator);
        }
        if (context_.mode == ScannerMode::Normal && (isIdentifierStart(lookahead_char_) || lookahead_char_ == '/' || lookahead_char_ == '>')) {
          if (lookahead_char_ == '/') {
            consumeChar();
            return createToken(TokenType::JsxClosingTagStart);
          }
          return createToken(TokenType::JsxTagStart);
        }
        return createToken(TokenType::LessThan, Token::FlagIsBinaryOperator);
      case '>':
        consumeChar();
        if (lookahead_char_ == '>') {
          consumeChar();
          if (lookahead_char_ == '>') {
            consumeChar();
            if (lookahead_char_ == '=') {
              consumeChar();
              return createToken(TokenType::UnsignedRightShiftAssign, Token::FlagIsAssignmentOperator);
            }
            return createToken(TokenType::UnsignedRightShift, Token::FlagIsBinaryOperator);
          }
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::RightShiftAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::RightShift, Token::FlagIsBinaryOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::GreaterThanEqual, Token::FlagIsBinaryOperator);
        }
        if (context_.mode == ScannerMode::JsxElement || context_.mode == ScannerMode::JsxAttribute) {
          context_.mode = ScannerMode::JsxElement;
          return createToken(TokenType::JsxTagEnd);
        }
        return createToken(TokenType::GreaterThan, Token::FlagIsBinaryOperator);
      case '&':
        consumeChar();
        if (lookahead_char_ == '&') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::AmpersandAmpersandAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::AmpersandAmpersand, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::AmpersandAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Ampersand, Token::FlagIsBinaryOperator);
      case '|':
        consumeChar();
        if (lookahead_char_ == '|') {
          consumeChar();
          if (lookahead_char_ == '=') {
            consumeChar();
            return createToken(TokenType::BarBarAssign, Token::FlagIsAssignmentOperator);
          }
          return createToken(TokenType::BarBar, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator);
        }
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::BarAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Bar, Token::FlagIsBinaryOperator);
      case '^':
        consumeChar();
        if (lookahead_char_ == '=') {
          consumeChar();
          return createToken(TokenType::CaretAssign, Token::FlagIsAssignmentOperator);
        }
        return createToken(TokenType::Caret, Token::FlagIsBinaryOperator);
      case '@':
        consumeChar();
        return createToken(TokenType::TsDecorator);
        // 未処理のASCII文字のデフォルトフォールスルー（ほとんどないはず）
    }
  }  // ASCII高速パスの終了

  // 非ASCII文字またはフォールスルーの処理
  if (isIdentifierStart(c)) return scanIdentifierOrKeyword();
  // skipWhitespaceで見逃された可能性のあるUnicodeの空白/改行を処理
  if (isWhitespace(c) || isLineTerminator(c)) {
    reportError("内部スキャナーエラー: トリビアがscanNextに到達しました");
    consumeChar();
    return createErrorToken("内部トリビアエラー");
  }

  // 予期しない文字
  std::string error_char_str = "(文字コード " + std::to_string(c) + ")";
  reportError("予期しない文字 " + error_char_str);
  consumeChar();
  return createErrorToken("予期しない文字");
}

// --- トークン作成の実装 ---
Token Scanner::createToken(TokenType type, uint32_t flags) {
  flags |= (current_token_.flags & Token::FlagPrecededByLineTerminator);
  Token token(type, current_token_.location, static_cast<uint32_t>(current_pos_ - token_start_pos_), current_token_.trivia_length, flags);
  token.precedence = getOperatorPrecedence(type);
  if (token.precedence > 0) token.flags |= Token::FlagIsBinaryOperator;
  if (type == TokenType::Assign || (type >= TokenType::PlusAssign && type <= TokenType::QuestionQuestionAssign))
    token.flags |= Token::FlagIsAssignmentOperator;
  else if (type == TokenType::PlusPlus || type == TokenType::MinusMinus) {
    token.flags |= Token::FlagIsUpdateOperator;
    token.flags &= ~Token::FlagIsBinaryOperator;
  } else if (type == TokenType::AmpersandAmpersand || type == TokenType::BarBar || type == TokenType::QuestionQuestion)
    token.flags |= Token::FlagIsLogicalOperator;
  if (type == TokenType::Plus || type == TokenType::Minus || type == TokenType::Tilde || type == TokenType::Exclamation || type == TokenType::Typeof || type == TokenType::Void || type == TokenType::Delete) token.flags |= Token::FlagIsUnaryOperator;
  // 設定されている場合は生のレキシムを保存
  // token.raw_lexeme.assign(token_start_pos_, token.length);
  return token;
}

// --- エラートークン作成の実装 ---
Token Scanner::createErrorToken(const std::string& message) {
  Token token = createToken(TokenType::Error);
  token.value = message;
  if (token.length == 0 && current_pos_ > token_start_pos_) {
    token.length = static_cast<uint32_t>(current_pos_ - token_start_pos_);
    // token.raw_lexeme.assign(token_start_pos_, token.length);
  }
  return token;
}

// --- エラー報告の実装 ---
void Scanner::reportError(const std::string& message) {
  if (error_handler_) {
    SourceLocation error_loc = current_token_.location;
    if (current_pos_ == token_start_pos_) error_loc = current_location_;
    error_handler_->handleError(error_loc, message);
  }
}

// --- 空白とコメントのスキップ実装 ---
bool Scanner::skipWhitespaceAndComments() {
  const char* trivia_start = current_pos_;
  bool preceded = (current_token_.flags & Token::FlagPrecededByLineTerminator);
  while (current_pos_ < source_end_) {
    uint32_t c = lookahead_char_;
    if (c < 128) {
      uint8_t props = kAsciiCharProperties_impl[c];
      if (props & CP_LT_IMPL) {
        preceded = true;
        uint32_t first = c;
        consumeChar();
        if (first == 0x0D && lookahead_char_ == 0x0A) consumeChar();
        continue;
      }
      if (props & CP_WS_IMPL) {
        consumeChar();
        continue;
      }
      if (c == '/') {
        const char* next_p = current_pos_ + lookahead_size_;
        if (next_p >= source_end_) break;
        uint8_t next_b = static_cast<uint8_t>(*next_p);
        if (next_b == '/') {
          consumeChar();
          consumeChar();
          while (current_pos_ < source_end_ && !isLineTerminator(lookahead_char_)) consumeChar();
          continue;
        }
        if (next_b == '*') {
          consumeChar();
          consumeChar();
          bool end = false;
          while (current_pos_ < source_end_) {
            uint32_t cc = lookahead_char_;
            consumeChar();
            if (cc == '*' && lookahead_char_ == '/') {
              consumeChar();
              end = true;
              break;
            }
            if (isLineTerminator(cc)) {
              preceded = true;
              if (cc == 0x0D && lookahead_char_ == 0x0A) consumeChar();
            }
          }
          if (!end) reportError("終了していない複数行コメント");
          continue;
        }
        break;
      }
      if (c == '<' && context_.allowHtmlComment) {
        const char* p = current_pos_ + lookahead_size_;
        if (p + 2 < source_end_ && *p == '!' && *(p + 1) == '-' && *(p + 2) == '-') {
          consumeChar();
          consumeChar();
          consumeChar();
          consumeChar();
          while (current_pos_ < source_end_) {
            if (lookahead_char_ == '-' && current_pos_ + 2 < source_end_ && *(current_pos_ + 1) == '-' && *(current_pos_ + 2) == '>') {
              consumeChar();
              consumeChar();
              consumeChar();
              break;
            }
            if (isLineTerminator(lookahead_char_)) preceded = true;
            consumeChar();
          }
          continue;
        }
        break;
      }
      if (c == '-' && context_.allowHtmlComment && preceded) {
        const char* p = current_pos_ + lookahead_size_;
        if (p + 1 < source_end_ && *p == '-' && *(p + 1) == '>') {
          consumeChar();
          consumeChar();
          consumeChar();
          continue;
        }
        break;
      }
      break;
    } else {
      if (isLineTerminator(c)) {
        preceded = true;
        consumeChar();
        continue;
      }
      if (isWhitespace(c)) {
        consumeChar();
        continue;
      }
      break;
    }
  }
  current_token_.flags = preceded ? Token::FlagPrecededByLineTerminator : 0;
  return static_cast<uint16_t>(current_pos_ - trivia_start);
}

// --- 主要関数の実装 ---

// 識別子またはキーワードのスキャン実装
Token Scanner::scanIdentifierOrKeyword() {
  auto start_time = std::chrono::high_resolution_clock::now();
  const char* start = current_pos_;
  uint32_t flags = 0;
  bool contains_escape = false;
  std::string identifier_value;  // エスケープが存在する場合のみ値を構築
  identifier_value.reserve(32);
  uint32_t first_char_code = 0;

  // 先頭のUnicodeエスケープ \uXXXXまたは\u{XXXXX}の処理
  if (lookahead_char_ == '\\') {
    flags |= Token::FlagContainsEscape;
    contains_escape = true;
    consumeChar();  // '\'を消費
    if (lookahead_char_ != 'u') {
      reportError("識別子は\\u以外のエスケープシーケンスで始まることはできません");
      return createErrorToken("無効な識別子開始エスケープ");
    }
    // エスケープシーケンスを解析するヘルパーを使用
    first_char_code = parseUnicodeEscape(identifier_value);
    if (first_char_code == 0xFFFFFFFF) {
      reportError("識別子開始位置のUnicodeエスケープシーケンスが無効です");
      return createErrorToken("無効な識別子開始エスケープ");
    }
  } else {
    // 通常の開始文字
    first_char_code = lookahead_char_;
    if (!isIdentifierStart(first_char_code)) {
      reportError("無効な識別子開始文字です");
      consumeChar();
      return createErrorToken("無効なID開始");
    }
    // 文字を追加（UTF8処理）
    if (lookahead_size_ == 1) {
      identifier_value += static_cast<char>(lookahead_char_);
    } else {
      identifier_value.append(current_pos_, lookahead_size_);
    }
    consumeChar();
  }

  // （エスケープされた可能性のある）最初の文字が有効なID_Startかを検証
  if (!isIdentifierStart(first_char_code)) {
    reportError("解決された文字は有効な識別子開始文字ではありません");
    return createErrorToken("無効なID開始文字");
  }

  // 識別子部分のスキャン
  while (current_pos_ < source_end_) {
    if (lookahead_char_ == '\\') {
      flags |= Token::FlagContainsEscape;
      contains_escape = true;
      consumeChar();  // '\'
      if (lookahead_char_ != 'u') {
        reportError("識別子ではUnicodeエスケープシーケンス(\\u)のみが許可されています");
        if (lookahead_size_ > 0) consumeChar();
        continue;
      }
      // Unicodeエスケープを解析して識別子値に追加
      uint32_t code_point = parseUnicodeEscape(identifier_value);
      if (code_point == 0xFFFFFFFF) {
        reportError("識別子内のUnicodeエスケープシーケンスが無効です");
        continue;
      }
      if (!isIdentifierPart(code_point)) {
        reportError("エスケープが有効な識別子部分に解決されません");
      }
    } else if (isIdentifierPart(lookahead_char_)) {
      // 通常の文字を追加（UTF8処理）
      if (lookahead_size_ == 1) {
        identifier_value += static_cast<char>(lookahead_char_);
      } else {
        identifier_value.append(current_pos_, lookahead_size_);
      }
      consumeChar();
    } else {
      break;  // 識別子部分の終了
    }
  }

  size_t length = current_pos_ - start;
  std::string lexeme(start, length);  // エスケープを含む生のレキシム

  // キーワード検索：エスケープが存在した場合はデコード後の*値*を使用
  TokenType type = TokenType::Identifier;
  uint32_t keyword_flags = 0;
  const std::string& lookup_key = contains_escape ? identifier_value : lexeme;

  auto it = kKeywords_impl_part1.find(lookup_key);
  if (it != kKeywords_impl_part1.end()) {
    bool is_definite_keyword = true;

    // コンテキスト依存キーワードの処理
    if (it->second.flags & Token::FlagIsContextualKeyword) {
      is_definite_keyword = false;  // コンテキストが証明しない限りIDと仮定

      // コンテキスト依存キーワードの判定
      if (it->second.type == TokenType::Async && peekNextNonTriviaChar() == 'f') {
        // 潜在的な async function
        is_definite_keyword = true;
      } else if (it->second.type == TokenType::Get && isPropertyAccessContext()) {
        // オブジェクトリテラルやクラス内のgetter
        is_definite_keyword = true;
      } else if (it->second.type == TokenType::Set && isPropertyAccessContext()) {
        // オブジェクトリテラルやクラス内のsetter
        is_definite_keyword = true;
      }
      // 他のコンテキスト依存キーワードの判定ロジックを追加
    }

    // 識別子として使用される予約語の処理
    if ((it->second.flags & Token::FlagIsReservedWord) && !(it->second.flags & Token::FlagIsKeyword)) {
      reportError("予期しない予約語 '" + lookup_key + "'");
      type = TokenType::Error;
      is_definite_keyword = false;
    }

    // 厳格モードで識別子として使用される予約語の処理
    if (context_.strictMode && (it->second.flags & Token::FlagIsStrictReservedWord)) {
      reportError("厳格モードでの予期しない予約語 '" + lookup_key + "'");
      type = TokenType::Error;
      is_definite_keyword = false;
    }

    if (is_definite_keyword && type != TokenType::Error) {
      type = it->second.type;
      keyword_flags = it->second.flags;
    }
  }

  // トークンの作成
  Token token = createToken(type, flags | keyword_flags);
  if (type == TokenType::Identifier || contains_escape) {
    // デコードされた可能性のある値を格納
    token.setValue(contains_escape ? identifier_value : lexeme);
  }
  token.raw_lexeme = lexeme;  // 常に生のレキシムを格納

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.identifierScanTime += (end_time - start_time);
  return token;
}

// プライベート識別子のスキャン実装
Token Scanner::scanPrivateIdentifier() {
  auto start_time = std::chrono::high_resolution_clock::now();
  const char* start = current_pos_;
  consumeChar();  // '#'を消費

  const char* name_start = current_pos_;

  // 仕様では名前部分は有効なIdentifierName（キーワードを許可）である必要があるが、
  // エスケープシーケンスを含むことはできない
  if (!isIdentifierStart(lookahead_char_)) {
    reportError("プライベート識別子は識別子文字で始まる名前を持つ必要があります");
    current_pos_ = start + 1;
    readUtf8Char();  // #の後に戻る
    return createErrorToken("無効なプライベート識別子名の開始");
  }
  consumeChar();
  while (current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) {
    // 禁止されているエスケープシーケンス開始の明示的チェック
    if (lookahead_char_ == '\\') {
      reportError("プライベート識別子名ではエスケープシーケンスは許可されていません");
      // エラーの一部として潜在的な識別子の残りを消費
      const char* error_start = current_pos_;
      while (current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) consumeChar();
      Token err_token = createToken(TokenType::Error);
      err_token.length = static_cast<uint32_t>(current_pos_ - start);
      err_token.raw_lexeme.assign(start, err_token.length);
      err_token.value = "無効なプライベート識別子名（エスケープを含む）";
      return err_token;
    }
    consumeChar();
  }
  const char* name_end = current_pos_;

  // 名前部分自体（#の後）は予約語でもよい（例：#class、#let）
  // ここでキーワードチェックは不要、有効なIdentifierName構造であることを確認するだけ

  // トークンの作成
  Token token = createToken(TokenType::PrivateIdentifier);
  token.length = static_cast<uint32_t>(current_pos_ - start);
  token.raw_lexeme.assign(start, token.length);
  // '#'なしの名前部分を値に格納
  token.setValue(std::string(name_start, name_end - name_start));

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.identifierScanTime += (end_time - start_time);
  return token;
}

// コンテキスト依存キーワードチェック用ヘルパー
uint32_t Scanner::peekNextNonTriviaChar() {
  // 現在位置を保存
  const char* saved_pos = current_pos_;
  uint32_t saved_char = lookahead_char_;
  uint32_t saved_size = lookahead_size_;

  // 空白とコメントをスキップ
  consumeChar();  // 現在の文字を消費
  skipWhitespaceAndComments();

  // 次の非空白文字を取得
  uint32_t result = lookahead_char_;

  // 状態を復元
  current_pos_ = saved_pos;
  lookahead_char_ = saved_char;
  lookahead_size_ = saved_size;

  return result;
}

// プロパティアクセスコンテキストかどうかを判定
bool Scanner::isPropertyAccessContext() {
  // 実際の実装では、パーサーの状態を参照する必要がある
  // この簡易実装では、直前のトークンに基づいて判断
  // 例：オブジェクトリテラル内の可能性がある場合は true
  return previous_token_.type == TokenType::LeftBrace ||
         previous_token_.type == TokenType::Comma;
}

// Unicodeエスケープの解析
uint32_t Scanner::parseUnicodeEscape(std::string& output) {
  consumeChar();  // 'u'を消費

  uint32_t code_point = 0;

  // \u{XXXXX} 形式のエスケープ
  if (lookahead_char_ == '{') {
    consumeChar();  // '{'を消費
    int digit_count = 0;

    while (current_pos_ < source_end_ && lookahead_char_ != '}') {
      if (!isHexDigit_impl_part1(lookahead_char_)) {
        reportError("Unicodeエスケープシーケンス内の無効な16進数字");
        return 0xFFFFFFFF;
      }

      uint32_t digit_value = hexDigitValue(lookahead_char_);
      code_point = (code_point << 4) | digit_value;

      if (code_point > 0x10FFFF) {
        reportError("Unicodeエスケープシーケンスの値が大きすぎます");
        return 0xFFFFFFFF;
      }

      consumeChar();
      digit_count++;

      if (digit_count > 6) {
        reportError("Unicodeエスケープシーケンスの桁数が多すぎます");
        return 0xFFFFFFFF;
      }
    }

    if (lookahead_char_ != '}') {
      reportError("閉じる'}'がないUnicodeエスケープシーケンス");
      return 0xFFFFFFFF;
    }

    if (digit_count == 0) {
      reportError("空のUnicodeエスケープシーケンス");
      return 0xFFFFFFFF;
    }

    consumeChar();  // '}'を消費
  }
  // \uXXXX 形式のエスケープ
  else {
    for (int i = 0; i < 4; i++) {
      if (current_pos_ >= source_end_ || !isHexDigit_impl_part1(lookahead_char_)) {
        reportError("Unicodeエスケープシーケンスには4桁の16進数が必要です");
        return 0xFFFFFFFF;
      }

      uint32_t digit_value = hexDigitValue(lookahead_char_);
      code_point = (code_point << 4) | digit_value;
      consumeChar();
    }
  }

  // サロゲートペアの処理
  if (code_point >= 0xD800 && code_point <= 0xDBFF) {
    // 上位サロゲート、下位サロゲートが続く必要がある
    if (lookahead_char_ == '\\' &&
        current_pos_ + 1 < source_end_ &&
        *(current_pos_ + 1) == 'u') {
      const char* surrogate_start = current_pos_;
      uint32_t saved_char = lookahead_char_;
      uint32_t saved_size = lookahead_size_;

      consumeChar();  // '\'を消費
      consumeChar();  // 'u'を消費

      uint32_t low_surrogate = 0;

      // 下位サロゲートの解析
      for (int i = 0; i < 4; i++) {
        if (current_pos_ >= source_end_ || !isHexDigit_impl_part1(lookahead_char_)) {
          // 下位サロゲートが無効、状態を復元
          current_pos_ = surrogate_start;
          lookahead_char_ = saved_char;
          lookahead_size_ = saved_size;
          break;
        }

        uint32_t digit_value = hexDigitValue(lookahead_char_);
        low_surrogate = (low_surrogate << 4) | digit_value;
        consumeChar();
      }

      // 有効な下位サロゲートかチェック
      if (low_surrogate >= 0xDC00 && low_surrogate <= 0xDFFF) {
        // サロゲートペアをUTF-8に変換
        code_point = 0x10000 + ((code_point - 0xD800) << 10) + (low_surrogate - 0xDC00);
      }
    }
  }

  // コードポイントをUTF-8に変換して出力に追加
  appendCodePointToUtf8(output, code_point);

  return code_point;
}

// コードポイントをUTF-8に変換して文字列に追加
void Scanner::appendCodePointToUtf8(std::string& str, uint32_t code_point) {
  if (code_point < 0x80) {
    // 1バイト文字
    str += static_cast<char>(code_point);
  } else if (code_point < 0x800) {
    // 2バイト文字
    str += static_cast<char>(0xC0 | (code_point >> 6));
    str += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point < 0x10000) {
    // 3バイト文字
    str += static_cast<char>(0xE0 | (code_point >> 12));
    str += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    str += static_cast<char>(0x80 | (code_point & 0x3F));
  } else {
    // 4バイト文字
    str += static_cast<char>(0xF0 | (code_point >> 18));
    str += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
    str += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    str += static_cast<char>(0x80 | (code_point & 0x3F));
  }
}

// 16進数字の値を取得
uint32_t Scanner::hexDigitValue(uint32_t c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0;
}

// 数値リテラルのスキャン実装
Token Scanner::scanNumericLiteral() {
  auto start_time = std::chrono::high_resolution_clock::now();
  const char* start = current_pos_;
  uint32_t flags = 0;
  double number_value = 0.0;
  std::string bigint_value;  // BigInt値の文字列表現
  bool is_bigint = false;
  bool is_legacy_octal = false;
  int base = 10;

  // --- プレフィックスの処理 --- (0x, 0b, 0o, レガシー8進数)
  if (lookahead_char_ == '0') {
    const char* after_zero = current_pos_ + 1;
    if (after_zero < source_end_) {
      uint32_t next_char = peekChar(0, 1);  // '0'の後の文字をピーク
      bool consumed_prefix = false;

      if (next_char == 'x' || next_char == 'X') {
        base = 16;
        flags |= Token::FlagIsHex;
        consumeChar();
        consumeChar();  // 0xを消費
        consumed_prefix = true;
        if (!isHexDigit_impl_part1(lookahead_char_)) {
          reportError("16進数リテラルには0xの後に数字が必要です");
          // '0'として扱い回復を試みる
          current_pos_ = start + 1;  // '0'の後に戻る
          readUtf8Char();            // 'x'を再読み込み
          base = 10;
          flags &= ~Token::FlagIsHex;  // フラグをリセット
          consumed_prefix = false;     // プレフィックスは実際には消費されていない
        }
      } else if (next_char == 'b' || next_char == 'B') {
        base = 2;
        flags |= Token::FlagIsBinary;
        consumeChar();
        consumeChar();  // 0bを消費
        consumed_prefix = true;
        if (!isBinaryDigit_impl_part1(lookahead_char_)) {
          reportError("2進数リテラルには0bの後に数字が必要です");
          current_pos_ = start + 1;
          readUtf8Char();
          base = 10;
          flags &= ~Token::FlagIsBinary;
          consumed_prefix = false;
        }
      } else if (next_char == 'o' || next_char == 'O') {
        base = 8;
        flags |= Token::FlagIsOctal;
        consumeChar();
        consumeChar();  // 0oを消費
        consumed_prefix = true;
        if (!isOctalDigit_impl_part1(lookahead_char_)) {
          reportError("8進数リテラルには0oの後に数字が必要です");
          current_pos_ = start + 1;
          readUtf8Char();
          base = 10;
          flags &= ~Token::FlagIsOctal;
          consumed_prefix = false;
        }
      } else if (isOctalDigit_impl_part1(next_char)) {
        // レガシー8進数の可能性
        if (context_.strictMode) {
          reportError("厳格モードではレガシー8進数リテラルは許可されていません");
          // '0'として扱う
          consumeChar();  // '0'のみを消費
        } else {
          // 厳格モードではない、レガシー8進数として解析
          base = 8;
          flags |= Token::FlagIsOctal | Token::FlagIsLegacyOctal;
          is_legacy_octal = true;
          consumeChar();  // '0'を消費
          // 残りの8進数字を消費（注：レガシー8進数はセパレータを含めない）
          while (isOctalDigit_impl_part1(lookahead_char_)) {
            consumeChar();
          }
          // レガシー8進数のスキャンはここで完了。小数点、指数、BigInt接尾辞は持てない。
          goto parse_value;  // 値の解析に直接ジャンプ
        }
      } else if (next_char == '_' || next_char == '.' || next_char == 'e' || next_char == 'E') {
        // '0'の後に10進数であることが明らかなものが続く
        consumeChar();  // '0'を消費
      } else if (!isDecimalDigit_impl_part1(next_char)) {
        // 単なる数字の0
        consumeChar();     // '0'を消費
        goto parse_value;  // 単一の数字'0'として扱う
      } else {
        // '0'の後に10進数字が続く（例：09）- 厳格モードの8進数コンテキストではエラーだが、それ以外では10進数として許可
        if (context_.strictMode) {
          // 暗黙的な8進数のような10進数について警告する可能性？いいえ、仕様では10進数として扱うと言っています。
        }
        consumeChar();  // '0'を消費
      }
    } else {
      // ソースが'0'の後で終了
      consumeChar();  // '0'を消費
      goto parse_value;
    }
  } else if (lookahead_char_ == '.') {
    // '.'で始まる、次の文字が数字かチェック
    if (!(current_pos_ + 1 < source_end_ && isDecimalDigit_impl_part1(peekChar(0, 1)))) {
      // 単なる'.'句読点、数値の開始ではない
      consumeChar();
      return createToken(TokenType::Dot);
    }
    // 数値が小数点で始まる。10進数スキャンの一部として以下で処理される。
    // プレフィックスは消費されず、基数は10のまま。
  }

  // --- 基数に基づく仮数部のスキャン --- (セパレータを処理)
  const char* mantissa_start = current_pos_;
  bool first_digit_in_mantissa = true;
  bool last_was_separator = false;

  while (current_pos_ < source_end_) {
    uint32_t c = lookahead_char_;
    bool is_digit_for_base = false;
    switch (base) {
      case 16:
        is_digit_for_base = isHexDigit_impl_part1(c);
        break;
      case 2:
        is_digit_for_base = isBinaryDigit_impl_part1(c);
        break;
      case 8:
        is_digit_for_base = isOctalDigit_impl_part1(c);
        break;  // 現代的8進数
      default:
        is_digit_for_base = isDecimalDigit_impl_part1(c);
        break;
    }

    if (is_digit_for_base) {
      consumeChar();
      last_was_separator = false;
      first_digit_in_mantissa = false;
    } else if (c == '_') {
      if (first_digit_in_mantissa || last_was_separator) {
        reportError("無効な数値セパレータの位置（先頭または連続はできません）");
        // セパレータを無視して回復？または中断？
        consumeChar();              // '_'を消費して続行、数字を期待
        last_was_separator = true;  // 二重セパレータを捕捉するためにマーク
        continue;
      }
      consumeChar();  // セパレータ'_'を消費
      last_was_separator = true;
      // 先読み：現在の基数の有効な数字が続く必要がある
      uint32_t next_c = lookahead_char_;
      bool next_is_digit = false;
      switch (base) {
        case 16:
          next_is_digit = isHexDigit_impl_part1(next_c);
          break;
        case 2:
          next_is_digit = isBinaryDigit_impl_part1(next_c);
          break;
        case 8:
          next_is_digit = isOctalDigit_impl_part1(next_c);
          break;
        default:
          next_is_digit = isDecimalDigit_impl_part1(next_c);
          break;
      }
      if (!next_is_digit) {
        reportError("数値セパレータの後には同じ基数の数字が必要です");
        // セパレータの前に戻り、数値の終わりとして扱う
        current_pos_--;              // '_'のconsumeCharを元に戻す
        readUtf8Char();              // セパレータを再読み込み
        last_was_separator = false;  // 状態変更を元に戻す
        break;                       // 仮数部スキャンを終了
      }
      // 次の文字は有効な数字、ループを続行
    } else {
      break;  // 仮数部の数字/セパレータの終了
    }
  }

  if (last_was_separator) {
    reportError("数値セパレータは仮数部の最後に現れることはできません");
    // 最後のセパレータの前に戻る
    current_pos_--;
    readUtf8Char();
  }

  // プレフィックス（もしあれば）の*後*に数字が消費されたかチェック
  if (current_pos_ == mantissa_start) {
    // これは0x、0b、0o、または初期の'0'（レガシー8進数でない場合）の後に数字が続かなかったか、'.'で始まり数字がなかったことを意味する
    if (flags & (Token::FlagIsHex | Token::FlagIsBinary | Token::FlagIsOctal)) {
      // プレフィックスチェックですでに処理済み、ただし二重チェック
      reportError("数値リテラルにはプレフィックスの後に数字が必要です");
      // プレフィックス+エラーとして扱う？または単に'0'？
      // プレフィックスチェックが失敗した場合、以前に作成されたエラートークンを使用。
      // ここに到達した場合、プレフィックスは*最初*は問題なかった。
      return createErrorToken("無効な数値形式");
    } else if (*start == '.') {
      reportError("'.'で始まる数値リテラルには少なくとも1つの数字が必要です");
      return createErrorToken("無効な数値形式");
    } else if (*start == '0' && current_pos_ == start + 1) {
      // 単なる数字の'0'、これは有効。
    } else if (base == 10 && *start != '0') {
      // この場合は正しく呼び出された場合は発生しないはず（数字または'.'で始まる必要がある）
      reportError("内部エラー：非数値開始でscanNumericLiteralが呼び出されました");
      return createErrorToken("内部スキャナーエラー");
    }
    // その他：単なる'0'だった。値の解析に進む。
  }
  // This means no digits followed 0x, 0b, 0o, or the initial '0' (if not legacy octal), or it started with '.' and had no digits
  if (flags & (Token::FlagIsHex | Token::FlagIsBinary | Token::FlagIsOctal)) {
    // Already handled by prefix checks, but double-check
    reportError("Number literal missing digits after prefix");
    // Treat as prefix + error? Or just '0'?
    // Let's stick with the error token created earlier if prefix check failed.
    // If we reached here, it means prefix *looked* okay initially.
    return createErrorToken("Invalid numeric format");
  } else if (*start == '.') {
    reportError("Numeric literal starting with '.' must be followed by at least one digit");
    return createErrorToken("Invalid numeric format");
  } else if (*start == '0' && current_pos_ == start + 1) {
    // Just the digit '0', which is valid.
  } else if (base == 10 && *start != '0') {
    // This case shouldn't happen if called correctly (requires starting digit or '.')
    reportError("Internal error: scanNumericLiteral called on non-numeric start");
    return createErrorToken("Internal Scanner Error");
  }
  // Else: It was just '0'. Fall through to value parsing.
}
const char* mantissa_end = current_pos_;

// --- Handle Decimal Point (only for base 10) ---
bool has_decimal = false;
const char* fraction_start = nullptr;
if (base == 10 && lookahead_char_ == '.') {
  has_decimal = true;
  flags |= Token::FlagIsDecimal;  // Mark as potentially floating point
  consumeChar();                  // Consume '.'
  fraction_start = current_pos_;
  last_was_separator = false;
  bool first_digit_in_fraction = true;

  while (current_pos_ < source_end_) {
    uint32_t c = lookahead_char_;
    if (isDecimalDigit_impl_part1(c)) {
      consumeChar();
      last_was_separator = false;
      first_digit_in_fraction = false;
    } else if (c == '_') {
      if (first_digit_in_fraction || last_was_separator) {
        reportError("Invalid numeric separator position in fractional part");
        consumeChar();
        last_was_separator = true;
        continue;  // Recover
      }
      consumeChar();  // Consume '_'
      last_was_separator = true;
      if (!isDecimalDigit_impl_part1(lookahead_char_)) {
        reportError("Separator in fraction must be followed by a decimal digit");
        current_pos_--;
        readUtf8Char();  // Backtrack
        last_was_separator = false;
        break;
      }
      // Next is digit, continue loop
    } else {
      break;  // End of fractional part
    }
  }
  if (last_was_separator) {
    reportError("Numeric separator cannot appear at the end of the fractional part");
    current_pos_--;
    readUtf8Char();
  }
  // Check if any digits were consumed after the decimal point
  if (current_pos_ == fraction_start) {
    // Example: "1." is valid, but "." alone was handled earlier.
    // Example: "1._" is invalid due to trailing separator.
    // Example: "1.e5" is valid.
    // If we got here, it means we had mantissa digits before the '.', so "1." is okay.
    // If we started with '.' and had no fraction digits: ".e5" - this is invalid.
    if (*start == '.') {
      reportError("Numeric literal starting with '.' requires digits after the decimal point if no exponent follows");
      // This error depends on whether an exponent follows.
      // We need to check lookahead_char_ for 'e'/'E'.
      if (lookahead_char_ != 'e' && lookahead_char_ != 'E') {
        return createErrorToken("Invalid numeric format");
      }
      // Allow exponent to follow, e.g. ".e5"
    }
  }
}
const char* fraction_end = current_pos_;

// --- 指数部の処理（10進数のみ） ---
bool has_exponent = false;
const char* exponent_start = nullptr;
if (base == 10 && (lookahead_char_ == 'e' || lookahead_char_ == 'E')) {
  has_exponent = true;
  flags |= Token::FlagIsExponent;  // 指数部フラグを設定
  consumeChar();                   // 'e'または'E'を消費
  const char* sign_pos = current_pos_;
  if (lookahead_char_ == '+' || lookahead_char_ == '-') {
    consumeChar();
  }
  exponent_start = current_pos_;
  last_was_separator = false;
  bool first_digit_in_exponent = true;

  if (!isDecimalDigit_impl_part1(lookahead_char_)) {
    reportError("指数部には少なくとも1つの数字が必要です");
    // 'e'/'E'の前に戻り、そこで数値が終わるものとして扱う
    current_pos_ = sign_pos;  // 符号があれば符号の前に
    if (current_pos_ > start && (*(current_pos_ - 1) == 'e' || *(current_pos_ - 1) == 'E')) {
      current_pos_--;  // 'e'/'E'の前に
    }
    readUtf8Char();
    has_exponent = false;  // 状態を元に戻す
    flags &= ~Token::FlagIsExponent;
    goto parse_value;  // 指数部なしで値を解析
  }

  while (current_pos_ < source_end_) {
    uint32_t c = lookahead_char_;
    if (isDecimalDigit_impl_part1(c)) {
      consumeChar();
      last_was_separator = false;
      first_digit_in_exponent = false;
    } else if (c == '_') {
      if (first_digit_in_exponent || last_was_separator) {
        reportError("指数部での数値セパレータの位置が無効です");
        consumeChar();
        last_was_separator = true;
        continue;  // 回復を試みる
      }
      consumeChar();  // '_'を消費
      last_was_separator = true;
      if (!isDecimalDigit_impl_part1(lookahead_char_)) {
        reportError("指数部のセパレータの後には数字が必要です");
        current_pos_--;
        readUtf8Char();  // 巻き戻し
        last_was_separator = false;
        break;
      }
      // 次は数字なのでループを続ける
    } else {
      break;  // 指数部の終了
    }
  }
  if (last_was_separator) {
    reportError("数値セパレータは指数部の末尾に現れることはできません");
    current_pos_--;
    readUtf8Char();
  }
  // 符号（もしあれば）の後に数字が消費されたかチェック
  if (current_pos_ == exponent_start) {
    // これは最初の数字チェックで捕捉されているはずだが、安全策として
    reportError("指数部には少なくとも1つの数字が必要です");
    // 巻き戻しロジックは複雑なので、初期チェックに依存する
    return createErrorToken("無効な指数形式");
  }
}
const char* exponent_end = current_pos_;

// --- BigInt接尾辞の処理 --- (整数のみ、小数点/指数の後には付けられない)
if (lookahead_char_ == 'n') {
  if (has_decimal || has_exponent || is_legacy_octal) {
    reportError("BigInt接尾辞'n'は小数点、指数、または旧式の8進リテラルの後には使用できません");
    consumeChar();  // 無限ループを避けるために'n'を消費
    return createErrorToken("無効なBigIntリテラル形式");
  } else if (base != 10 && base != 16 && base != 8 && base != 2) {
    // ロジックが正しければ発生しないはずだが、安全策として
    reportError("内部エラー: BigInt接尾辞'n'が無効な基数に適用されました");
    consumeChar();
    return createErrorToken("スキャナー内部エラー");
  } else {
    is_bigint = true;
    flags |= Token::FlagIsBigInt;
    consumeChar();  // 'n'を消費
  }
}

parse_value :
    // --- 値の変換 --- (詳細ヘルパーを使用)
    const char* end = current_pos_;  // リテラル（または'n'）の最後の文字の後の位置
std::string lexeme(start, end - start);
TokenType type;
Scanner::NumberParseResult parse_res = NumberParseResult::Ok;

if (is_bigint) {
  type = TokenType::BigIntLiteral;
  // 基数、セパレータ、潜在的なオーバーフローを処理
  int64_t bigint_value = 0;
  parse_res = parseDetailedBigInt(start, end - 1, bigint_value, flags);  // end-1で'n'を除外
  if (parse_res != NumberParseResult::Ok) {
    // エラーは理想的にはparseDetailedBigIntによって報告されるべき
    reportError("BigInt値の解析に失敗しました", false);  // false = 既に報告されている場合は重複しない
  }
  token_value_.bigint_value = bigint_value;
} else {
  type = TokenType::NumericLiteral;
  // 基数、セパレータ、小数点、指数を処理
  double number_value = 0.0;
  parse_res = parseDetailedNumber(start, end, number_value, flags);
  if (parse_res != NumberParseResult::Ok) {
    reportError("数値の解析に失敗しました", false);
  }
  token_value_.number_value = number_value;
}

// 数値リテラルの直後に識別子が始まる場合のチェック（無効）
if (isIdentifierStart(lookahead_char_)) {
  reportError("識別子が数値リテラルの直後に始まっています");
  // エラーの一部として識別子を消費することを検討？
  // または数値トークンを返して、パーサーにエラー処理を任せる
}

// 解析が失敗した場合でもトークンを作成し、必要に応じてエラーとしてマーク
Token token = createToken(parse_res == NumberParseResult::Ok ? type : TokenType::Error, flags);
token.raw_lexeme = lexeme;

auto end_time = std::chrono::high_resolution_clock::now();
perf_timers_.numberScanTime += (end_time - start_time);
return token;
}

// 文字列リテラルのスキャン実装
Token Scanner::scanStringLiteral() {
  auto start_time = std::chrono::high_resolution_clock::now();

  // 開始引用符を記録
  uint32_t quote = lookahead_char_;
  consumeChar();  // 開始引用符を消費

  std::string value;
  bool has_escape = false;

  while (current_pos_ < source_end_ && lookahead_char_ != quote && lookahead_char_ != '\n') {
    if (lookahead_char_ == '\\') {
      has_escape = true;
      consumeChar();  // バックスラッシュを消費

      if (current_pos_ >= source_end_) {
        reportError("文字列リテラルの終わりに不完全なエスケープシーケンス");
        break;
      }

      switch (lookahead_char_) {
        case 'n':
          value += '\n';
          consumeChar();
          break;
        case 'r':
          value += '\r';
          consumeChar();
          break;
        case 't':
          value += '\t';
          consumeChar();
          break;
        case 'b':
          value += '\b';
          consumeChar();
          break;
        case 'f':
          value += '\f';
          consumeChar();
          break;
        case 'v':
          value += '\v';
          consumeChar();
          break;
        case '\\':
          value += '\\';
          consumeChar();
          break;
        case '\'':
          value += '\'';
          consumeChar();
          break;
        case '\"':
          value += '\"';
          consumeChar();
          break;
        case '0':
          if (current_pos_ + 1 < source_end_ && isDecimalDigit_impl_part1(peekChar(1))) {
            reportError("8進数エスケープシーケンスは使用できません");
            value += '\0';
            consumeChar();
          } else {
            value += '\0';
            consumeChar();
          }
          break;
        case 'x': {
          consumeChar();  // 'x'を消費
          uint32_t hex_value = 0;
          for (int i = 0; i < 2 && current_pos_ < source_end_; i++) {
            uint32_t digit = hexDigitValue(lookahead_char_);
            if (digit == 0xFF) {
              reportError("不完全な16進エスケープシーケンス");
              break;
            }
            hex_value = (hex_value << 4) | digit;
            consumeChar();
          }
          value += static_cast<char>(hex_value);
          break;
        }
        case 'u': {
          consumeChar();  // 'u'を消費
          uint32_t code_point = 0;

          // Unicode エスケープシーケンスの処理
          if (lookahead_char_ == '{') {
            // \u{XXXXXX} 形式
            consumeChar();  // '{'を消費
            int digit_count = 0;

            while (current_pos_ < source_end_ && lookahead_char_ != '}') {
              uint32_t digit = hexDigitValue(lookahead_char_);
              if (digit == 0xFF) {
                reportError("無効なUnicodeエスケープシーケンス");
                break;
              }
              if (digit_count >= 6) {
                reportError("Unicodeエスケープシーケンスが長すぎます");
                break;
              }
              code_point = (code_point << 4) | digit;
              digit_count++;
              consumeChar();
            }

            if (lookahead_char_ != '}') {
              reportError("閉じる'}'がないUnicodeエスケープシーケンス");
            } else {
              consumeChar();  // '}'を消費
            }

            if (digit_count == 0) {
              reportError("空のUnicodeエスケープシーケンス");
              code_point = 0xFFFD;  // 置換文字
            }

            if (code_point > 0x10FFFF) {
              reportError("Unicodeコードポイントが範囲外です");
              code_point = 0xFFFD;  // 置換文字
            }
          } else {
            // \uXXXX 形式
            for (int i = 0; i < 4 && current_pos_ < source_end_; i++) {
              uint32_t digit = hexDigitValue(lookahead_char_);
              if (digit == 0xFF) {
                reportError("不完全なUnicodeエスケープシーケンス");
                break;
              }
              code_point = (code_point << 4) | digit;
              consumeChar();
            }
          }

          // UTF-8エンコーディングでコードポイントを追加
          if (code_point < 0x80) {
            value += static_cast<char>(code_point);
          } else if (code_point < 0x800) {
            value += static_cast<char>(0xC0 | (code_point >> 6));
            value += static_cast<char>(0x80 | (code_point & 0x3F));
          } else if (code_point < 0x10000) {
            value += static_cast<char>(0xE0 | (code_point >> 12));
            value += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            value += static_cast<char>(0x80 | (code_point & 0x3F));
          } else {
            value += static_cast<char>(0xF0 | (code_point >> 18));
            value += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
            value += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
            value += static_cast<char>(0x80 | (code_point & 0x3F));
          }
          break;
        }
        case '\n':
          // 行継続
          consumeChar();
          break;
        case '\r':
          // CRLFの場合は両方消費
          consumeChar();
          if (lookahead_char_ == '\n') {
            consumeChar();
          }
          break;
        default:
          // その他のエスケープシーケンス（例：\a）は文字をそのまま使用
          value += static_cast<char>(lookahead_char_);
          consumeChar();
          break;
      }
    } else {
      // 通常の文字
      const char* char_start = current_pos_;
      uint32_t c = lookahead_char_;
      consumeChar();

      // UTF-8文字をそのまま追加
      value.append(char_start, current_pos_ - char_start);
    }
  }

  // 終了引用符をチェック
  if (lookahead_char_ != quote) {
    reportError("終了引用符がない文字列リテラル");
    // エラートークンを返す
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.stringScanTime += (end_time - start_time);
    return createErrorToken("終了引用符がない文字列リテラル");
  }

  consumeChar();  // 終了引用符を消費

  // トークンを作成
  uint32_t flags = 0;
  if (has_escape) {
    flags |= Token::FlagHasEscape;
  }

  Token token = createToken(TokenType::StringLiteral, flags);
  token.setValue(value);
  token.raw_lexeme = std::string(start_location_.position, current_pos_ - start_location_.position);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.stringScanTime += (end_time - start_time);
  return token;
}

// テンプレートリテラルのスキャン実装
Token Scanner::scanTemplateToken() {
  auto start_time = std::chrono::high_resolution_clock::now();

  const char* start = current_pos_ - 1;  // バッククォートを含める
  bool is_head = true;
  bool is_tail = true;
  bool has_escape = false;
  std::string cooked_value;

  // テンプレートの開始部分を確認
  if (lookahead_char_ == '`') {
    // 空のテンプレート ``
    consumeChar();
    Token token = createToken(TokenType::TemplateLiteral, Token::FlagIsTemplateHead | Token::FlagIsTemplateTail);
    token.setValue(cooked_value);
    token.raw_lexeme = std::string(start, current_pos_ - start);

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.templateScanTime += (end_time - start_time);
    return token;
  }

  // テンプレート内のテキストを処理
  while (current_pos_ < source_end_ && lookahead_char_ != '`' && lookahead_char_ != '$') {
    if (lookahead_char_ == '\\') {
      has_escape = true;
      consumeChar();  // バックスラッシュを消費

      if (current_pos_ >= source_end_) {
        reportError("テンプレートリテラルの終わりに不完全なエスケープシーケンス");
        break;
      }

      // 文字列リテラルと同様のエスケープ処理
      switch (lookahead_char_) {
        case 'n':
          cooked_value += '\n';
          consumeChar();
          break;
        case 'r':
          cooked_value += '\r';
          consumeChar();
          break;
        case 't':
          cooked_value += '\t';
          consumeChar();
          break;
        case 'b':
          cooked_value += '\b';
          consumeChar();
          break;
        case 'f':
          cooked_value += '\f';
          consumeChar();
          break;
        case 'v':
          cooked_value += '\v';
          consumeChar();
          break;
        case '\\':
          cooked_value += '\\';
          consumeChar();
          break;
        case '\'':
          cooked_value += '\'';
          consumeChar();
          break;
        case '\"':
          cooked_value += '\"';
          consumeChar();
          break;
        case '`':
          cooked_value += '`';
          consumeChar();
          break;
        case '$':
          cooked_value += '$';
          consumeChar();
          break;
        // その他のエスケープシーケンスも同様に処理
        default:
          // 文字列リテラルと同様の処理
          cooked_value += static_cast<char>(lookahead_char_);
          consumeChar();
          break;
      }
    } else {
      // 通常の文字
      const char* char_start = current_pos_;
      uint32_t c = lookahead_char_;
      consumeChar();

      // UTF-8文字をそのまま追加
      cooked_value.append(char_start, current_pos_ - char_start);
    }
  }

  // テンプレート式の開始または終了を確認
  if (lookahead_char_ == '$' && peekChar(1) == '{') {
    // 式の開始 ${
    consumeChar();  // '$'を消費
    consumeChar();  // '{'を消費
    is_tail = false;
  } else if (lookahead_char_ == '`') {
    // テンプレートの終了 `
    consumeChar();
    is_head = context_.in_template_middle;
  } else {
    // 予期しない終了
    reportError("テンプレートリテラルが不完全です");
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.templateScanTime += (end_time - start_time);
    return createErrorToken("不完全なテンプレートリテラル");
  }

  // フラグを設定
  uint32_t flags = 0;
  if (is_head) flags |= Token::FlagIsTemplateHead;
  if (is_tail) flags |= Token::FlagIsTemplateTail;
  if (has_escape) flags |= Token::FlagHasEscape;

  // コンテキストを更新
  ScannerContext new_context = context_;
  new_context.in_template_middle = !is_tail;
  setContext(new_context);

  // トークンを作成
  Token token = createToken(TokenType::TemplateLiteral, flags);
  token.setValue(cooked_value);
  token.raw_lexeme = std::string(start, current_pos_ - start);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.templateScanTime += (end_time - start_time);
  return token;
}

// 正規表現リテラルのスキャン実装
Token Scanner::scanRegExpLiteral() {
  auto start_time = std::chrono::high_resolution_clock::now();

  const char* start = current_pos_ - 1;  // スラッシュを含める
  std::string pattern;
  std::string flags_str;

  // パターン部分をスキャン
  while (current_pos_ < source_end_ && lookahead_char_ != '/' && lookahead_char_ != '\n') {
    if (lookahead_char_ == '\\') {
      // エスケープシーケンス
      pattern += static_cast<char>(lookahead_char_);
      consumeChar();

      if (current_pos_ >= source_end_ || lookahead_char_ == '\n') {
        reportError("正規表現リテラルの終わりに不完全なエスケープシーケンス");
        break;
      }

      // エスケープされた文字を追加
      pattern += static_cast<char>(lookahead_char_);
      consumeChar();
    } else {
      // 通常の文字
      pattern += static_cast<char>(lookahead_char_);
      consumeChar();
    }
  }

  // 終了スラッシュをチェック
  if (lookahead_char_ != '/') {
    reportError("終了スラッシュがない正規表現リテラル");
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.regexpScanTime += (end_time - start_time);
    return createErrorToken("終了スラッシュがない正規表現リテラル");
  }

  consumeChar();  // 終了スラッシュを消費

  // フラグ部分をスキャン
  while (current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) {
    flags_str += static_cast<char>(lookahead_char_);
    consumeChar();
  }

  // 正規表現の構文を検証
  if (!validateRegExpSyntax(pattern.c_str(), flags_str.c_str())) {
    reportError("無効な正規表現構文");
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.regexpScanTime += (end_time - start_time);
    return createErrorToken("無効な正規表現構文");
  }

  // トークンを作成
  Token token = createToken(TokenType::RegExpLiteral, 0);
  token.setRegExpValue(pattern, flags_str);
  token.raw_lexeme = std::string(start, current_pos_ - start);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.regexpScanTime += (end_time - start_time);
  return token;
}

// JSXトークンのスキャン実装
Token Scanner::scanJsxToken() {
  auto start_time = std::chrono::high_resolution_clock::now();

  // JSXコンテキストを確認
  if (!context_.in_jsx) {
    reportError("JSXコンテキスト外でJSXトークンをスキャンしようとしました");
    return createErrorToken("無効なJSXコンテキスト");
  }

  // JSXトークンの種類を判断
  if (lookahead_char_ == '<') {
    consumeChar();

    if (lookahead_char_ == '/') {
      // 閉じタグ開始 </
      consumeChar();
      Token token = createToken(TokenType::JsxTagEnd, 0);

      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return token;
    } else {
      // 開始タグ開始 <
      Token token = createToken(TokenType::JsxTagStart, 0);

      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return token;
    }
  } else if (lookahead_char_ == '>') {
    // タグ終了 >
    consumeChar();
    Token token = createToken(TokenType::JsxTagClose, 0);

    // JSXテキストモードに切り替え
    ScannerContext new_context = context_;
    new_context.in_jsx_text = true;
    setContext(new_context);

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.jsxScanTime += (end_time - start_time);
    return token;
  } else if (lookahead_char_ == '/') {
    consumeChar();

    if (lookahead_char_ == '>') {
      // 自己終了タグ />
      consumeChar();
      Token token = createToken(TokenType::JsxTagSelfClose, 0);

      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return token;
    } else {
      // 予期しない文字
      reportError("JSXタグで予期しない文字");
      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return createErrorToken("無効なJSX構文");
    }
  } else if (lookahead_char_ == '{') {
    // JSX式開始 {
    consumeChar();
    Token token = createToken(TokenType::JsxExprStart, 0);

    // JSX式モードを終了
    ScannerContext new_context = context_;
    new_context.in_jsx = false;
    new_context.in_jsx_text = false;
    setContext(new_context);

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.jsxScanTime += (end_time - start_time);
    return token;
  } else if (lookahead_char_ == '}') {
    // JSX式終了 }
    consumeChar();
    Token token = createToken(TokenType::JsxExprEnd, 0);

    // JSXテキストモードに戻る
    ScannerContext new_context = context_;
    new_context.in_jsx = true;
    new_context.in_jsx_text = true;
    setContext(new_context);

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.jsxScanTime += (end_time - start_time);
    return token;
  } else if (isJsxIdentifierStart(lookahead_char_)) {
    // JSX識別子
    return scanJsxIdentifier();
  } else if (context_.in_jsx_text) {
    // JSXテキスト
    return scanJsxText();
  } else {
    // その他のJSXトークン（属性など）
    if (lookahead_char_ == '=') {
      consumeChar();
      Token token = createToken(TokenType::JsxEquals, 0);

      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return token;
    } else if (isWhitespace(lookahead_char_)) {
      // 空白をスキップ
      skipWhitespace();
      return scanJsxToken();
    } else {
      // 予期しない文字
      reportError("JSXで予期しない文字");
      consumeChar();  // エラー回復のため
      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return createErrorToken("無効なJSX構文");
    }
  }
}
// JSX識別子のスキャン実装
Token Scanner::scanJsxIdentifier() {
  auto start_time = std::chrono::high_resolution_clock::now();

  const char* start = current_pos_;
  std::string identifier;

  // 最初の文字
  if (!isJsxIdentifierStart(lookahead_char_)) {
    reportError("JSX識別子が無効な文字で始まっています");
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.jsxScanTime += (end_time - start_time);
    return createErrorToken("無効なJSX識別子");
  }

  // 識別子の最初の部分をスキャン
  identifier += static_cast<char>(lookahead_char_);
  consumeChar();

  // 残りの識別子をスキャン
  while (current_pos_ < source_end_ && isJsxIdentifierPart(lookahead_char_)) {
    identifier += static_cast<char>(lookahead_char_);
    consumeChar();
  }

  // 名前空間プレフィックス（例：svg:rect の svg:）をチェック
  if (lookahead_char_ == ':') {
    identifier += ':';
    consumeChar();

    // コロンの後に有効な識別子が続く必要がある
    if (!isJsxIdentifierStart(lookahead_char_)) {
      reportError("JSX名前空間プレフィックスの後に有効な識別子が必要です");
      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.jsxScanTime += (end_time - start_time);
      return createErrorToken("無効なJSX名前空間");
    }

    // 名前空間の後の識別子をスキャン
    identifier += static_cast<char>(lookahead_char_);
    consumeChar();

    while (current_pos_ < source_end_ && isJsxIdentifierPart(lookahead_char_)) {
      identifier += static_cast<char>(lookahead_char_);
      consumeChar();
    }
  }

  // トークンを作成
  Token token = createToken(TokenType::JsxIdentifier, 0);
  token.setStringValue(identifier);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.jsxScanTime += (end_time - start_time);
  return token;
}

// 正規表現リテラルのスキャン
Token Scanner::scanRegExpLiteral() {
  auto start_time = std::chrono::high_resolution_clock::now();

  // 開始スラッシュはすでに消費されているはず
  const char* pattern_start = current_pos_;
  std::string pattern;

  // パターン部分をスキャン
  bool in_char_class = false;
  bool escaped = false;

  while (current_pos_ < source_end_) {
    if (lookahead_char_ == '/' && !escaped && !in_char_class) {
      // パターン終了
      break;
    } else if (lookahead_char_ == '[' && !escaped) {
      in_char_class = true;
    } else if (lookahead_char_ == ']' && !escaped) {
      in_char_class = false;
    } else if (lookahead_char_ == '\\' && !escaped) {
      escaped = true;
      pattern += static_cast<char>(lookahead_char_);
      consumeChar();
      continue;
    } else if (lookahead_char_ == '\n' && !escaped) {
      // 改行は正規表現パターン内では許可されない
      reportError("正規表現パターン内に改行があります");
      auto end_time = std::chrono::high_resolution_clock::now();
      perf_timers_.regExpScanTime += (end_time - start_time);
      return createErrorToken("無効な正規表現");
    } else {
      escaped = false;
    }

    pattern += static_cast<char>(lookahead_char_);
    consumeChar();
  }

  if (current_pos_ >= source_end_ || lookahead_char_ != '/') {
    reportError("正規表現パターンが閉じられていません");
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.regExpScanTime += (end_time - start_time);
    return createErrorToken("閉じられていない正規表現");
  }

  // 終了スラッシュを消費
  consumeChar();

  // フラグをスキャン
  std::string flags;
  while (current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) {
    flags += static_cast<char>(lookahead_char_);
    consumeChar();
  }

  // 正規表現の構文を検証
  if (!validateRegExpSyntax(pattern.c_str(), flags.c_str())) {
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.regExpScanTime += (end_time - start_time);
    return createErrorToken("無効な正規表現構文");
  }

  // トークンを作成
  Token token = createToken(TokenType::RegExpLiteral, 0);
  token.setRegExpValue(pattern, flags);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.regExpScanTime += (end_time - start_time);
  return token;
}

// JSXテキストのスキャン
Token Scanner::scanJsxText() {
  auto start_time = std::chrono::high_resolution_clock::now();

  const char* start = current_pos_;
  std::string text;
  bool has_non_whitespace = false;

  while (current_pos_ < source_end_) {
    if (lookahead_char_ == '<' || lookahead_char_ == '{') {
      // JSXタグまたは式の開始
      break;
    }

    if (!isWhitespace(lookahead_char_)) {
      has_non_whitespace = true;
    }

    // テキストに文字を追加
    text += static_cast<char>(lookahead_char_);
    consumeChar();
  }

  // 空白のみのテキストは無視する（オプション）
  if (!has_non_whitespace && context_.jsx_preserve_whitespace == false) {
    // 次のトークンを再帰的にスキャン
    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.jsxScanTime += (end_time - start_time);
    return scanJsxToken();
  }

  // トークンを作成
  Token token = createToken(TokenType::JsxText, 0);
  token.setStringValue(text);

  auto end_time = std::chrono::high_resolution_clock::now();
  perf_timers_.jsxScanTime += (end_time - start_time);
  return token;
}

// 詳細な数値解析の実装
Scanner::NumberParseResult Scanner::parseDetailedNumber(const char* start, const char* end, double& out, uint32_t& flags) {
  // セパレータを除去した数値文字列を作成
  std::string cleaned_num;
  cleaned_num.reserve(end - start);

  // 基数を決定（10進数がデフォルト）
  int base = 10;
  const char* num_start = start;

  // プレフィックスに基づいて基数を設定
  if ((flags & Token::FlagIsHex) && end - start > 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
    base = 16;
    num_start = start + 2;
  } else if ((flags & Token::FlagIsBinary) && end - start > 2 && start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) {
    base = 2;
    num_start = start + 2;
  } else if ((flags & Token::FlagIsOctal) && !(flags & Token::FlagIsLegacyOctal) && end - start > 2 &&
             start[0] == '0' && (start[1] == 'o' || start[1] == 'O')) {
    base = 8;
    num_start = start + 2;
  } else if (flags & Token::FlagIsLegacyOctal) {
    base = 8;
    num_start = start + 1;  // 先頭の'0'をスキップ
  }

  // セパレータを除去して数値文字列を構築
  for (const char* p = num_start; p < end; ++p) {
    if (*p != '_') {
      cleaned_num += *p;
    }
  }

  // 基数に応じた解析
  if (base == 10) {
    // 10進数の場合、小数点や指数も処理
    char* endptr;
    errno = 0;
    out = std::strtod(cleaned_num.c_str(), &endptr);

    if (*endptr != '\0' || cleaned_num.empty()) {
      return NumberParseResult::InvalidFormat;
    }

    if (errno == ERANGE) {
      if (out == 0.0) {
        return NumberParseResult::Underflow;
      } else {
        return NumberParseResult::Overflow;
      }
    }
  } else {
    // 16進数、8進数、2進数の場合
    char* endptr;
    errno = 0;
    unsigned long long int_value = std::strtoull(cleaned_num.c_str(), &endptr, base);

    if (*endptr != '\0' || cleaned_num.empty()) {
      return NumberParseResult::InvalidFormat;
    }

    if (errno == ERANGE) {
      return NumberParseResult::Overflow;
    }

    out = static_cast<double>(int_value);
  }

  return NumberParseResult::Ok;
}

// BigInt詳細解析の実装
Scanner::NumberParseResult Scanner::parseDetailedBigInt(const char* start, const char* end, int64_t& out, uint32_t& flags) {
  // セパレータを除去した数値文字列を作成
  std::string cleaned_num;
  cleaned_num.reserve(end - start);

  // 基数を決定（10進数がデフォルト）
  int base = 10;
  const char* num_start = start;

  // プレフィックスに基づいて基数を設定
  if ((flags & Token::FlagIsHex) && end - start > 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
    base = 16;
    num_start = start + 2;
  } else if ((flags & Token::FlagIsBinary) && end - start > 2 && start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) {
    base = 2;
    num_start = start + 2;
  } else if ((flags & Token::FlagIsOctal) && !(flags & Token::FlagIsLegacyOctal) && end - start > 2 &&
             start[0] == '0' && (start[1] == 'o' || start[1] == 'O')) {
    base = 8;
    num_start = start + 2;
  } else if (flags & Token::FlagIsLegacyOctal) {
    base = 8;
    num_start = start + 1;  // 先頭の'0'をスキップ
  }

  // セパレータを除去して数値文字列を構築
  for (const char* p = num_start; p < end; ++p) {
    if (*p != '_') {
      cleaned_num += *p;
    }
  }

  // 基数に応じた解析
  char* endptr;
  errno = 0;
  out = std::strtoll(cleaned_num.c_str(), &endptr, base);

  if (*endptr != '\0' || cleaned_num.empty()) {
    return NumberParseResult::InvalidFormat;
  }

  if (errno == ERANGE) {
    return NumberParseResult::Overflow;
  }

  return NumberParseResult::Ok;
}

// 複雑なエスケープシーケンスの解析
Scanner::EscapeParseResult Scanner::parseComplexEscape(const char*& p, std::string& out_val) {
  if (p >= source_end_) {
    return EscapeParseResult::InvalidUnicodeEscape;
  }

  uint32_t c = static_cast<uint8_t>(*p++);

  // 単純なASCII文字の場合
  if (c < 0x80) {
    out_val += static_cast<char>(c);
    return EscapeParseResult::Ok;
  }

  // UTF-8エンコードされた文字の処理
  int remaining_bytes = 0;
  uint32_t code_point = 0;

  // UTF-8バイト数を判定
  if ((c & 0xE0) == 0xC0) {
    // 2バイトシーケンス
    remaining_bytes = 1;
    code_point = c & 0x1F;
  } else if ((c & 0xF0) == 0xE0) {
    // 3バイトシーケンス
    remaining_bytes = 2;
    code_point = c & 0x0F;
  } else if ((c & 0xF8) == 0xF0) {
    // 4バイトシーケンス
    remaining_bytes = 3;
    code_point = c & 0x07;
  } else {
    // 無効なUTF-8シーケンス
    return EscapeParseResult::InvalidUnicodeEscape;
  }

  // 残りのバイトを処理
  for (int i = 0; i < remaining_bytes; i++) {
    if (p >= source_end_ || (*p & 0xC0) != 0x80) {
      return EscapeParseResult::InvalidUnicodeEscape;
    }

    code_point = (code_point << 6) | (*p & 0x3F);
    p++;
  }

  // コードポイントをUTF-8でエンコードして出力に追加
  if (code_point < 0x80) {
    out_val += static_cast<char>(code_point);
  } else if (code_point < 0x800) {
    out_val += static_cast<char>(0xC0 | (code_point >> 6));
    out_val += static_cast<char>(0x80 | (code_point & 0x3F));
  } else if (code_point < 0x10000) {
    out_val += static_cast<char>(0xE0 | (code_point >> 12));
    out_val += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    out_val += static_cast<char>(0x80 | (code_point & 0x3F));
  } else {
    out_val += static_cast<char>(0xF0 | (code_point >> 18));
    out_val += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
    out_val += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
    out_val += static_cast<char>(0x80 | (code_point & 0x3F));
  }

  return EscapeParseResult::Ok;
}

// 正規表現構文の検証
bool Scanner::validateRegExpSyntax(const char* pattern, const char* flags) {
  // フラグの重複チェック
  std::set<char> unique_flags;
  for (const char* f = flags; *f; ++f) {
    if (unique_flags.find(*f) != unique_flags.end()) {
      reportError("正規表現フラグが重複しています");
      return false;
    }

    // 有効なフラグかチェック
    if (*f != 'g' && *f != 'i' && *f != 'm' && *f != 's' && *f != 'u' && *f != 'y') {
      reportError("無効な正規表現フラグです");
      return false;
    }

    unique_flags.insert(*f);
  }

  // パターンの基本的な構文チェック
  bool in_char_class = false;
  bool escaped = false;
  int group_depth = 0;

  for (const char* p = pattern; *p; ++p) {
    if (escaped) {
      escaped = false;
      continue;
    }

    switch (*p) {
      case '\\':
        escaped = true;
        break;

      case '[':
        if (!in_char_class) {
          in_char_class = true;
        }
        break;

      case ']':
        if (in_char_class) {
          in_char_class = false;
        }
        break;

      case '(':
        if (!in_char_class) {
          group_depth++;
        }
        break;

      case ')':
        if (!in_char_class) {
          group_depth--;
          if (group_depth < 0) {
            reportError("正規表現の括弧が閉じられていません");
            return false;
          }
        }
        break;

      case '+':
      case '*':
        if (!in_char_class && p == pattern) {
          reportError("正規表現の量指定子が無効な位置にあります");
          return false;
        }
        break;
    }
  }

  // 閉じられていない文字クラスや括弧をチェック
  if (in_char_class) {
    reportError("正規表現の文字クラスが閉じられていません");
    return false;
  }

  if (group_depth > 0) {
    reportError("正規表現の括弧が閉じられていません");
    return false;
  }

  return true;
}

// 識別子の種類を判別
TokenType Scanner::disambiguateIdentifier(const char* str, size_t length) {
  // キーワードかどうかをチェック
  TokenType keyword_type = lookupKeyword(str, length);
  if (keyword_type != TokenType::Identifier) {
    return keyword_type;
  }

  // 予約語かどうかをチェック
  static const std::unordered_map<std::string_view, TokenType> reserved_words = {
      {"await", TokenType::Await},
      {"async", TokenType::Async},
      {"yield", TokenType::Yield},
      {"let", TokenType::Let},
      {"static", TokenType::Static},
      {"get", TokenType::Get},
      {"set", TokenType::Set},
      {"of", TokenType::Of},
      {"from", TokenType::From}};

  std::string_view sv(str, length);
  auto it = reserved_words.find(sv);
  if (it != reserved_words.end()) {
    // コンテキストに応じて予約語かどうかを判断
    if (context_.in_strict_mode ||
        (it->second == TokenType::Await && context_.in_async_function) ||
        (it->second == TokenType::Yield && context_.in_generator)) {
      return it->second;
    }
  }

  return TokenType::Identifier;
}

// キーワードの検索
TokenType Scanner::lookupKeyword(const char* str, size_t length) {
  static const std::unordered_map<std::string_view, TokenType> keywords = {
      {"break", TokenType::Break},
      {"case", TokenType::Case},
      {"catch", TokenType::Catch},
      {"class", TokenType::Class},
      {"const", TokenType::Const},
      {"continue", TokenType::Continue},
      {"debugger", TokenType::Debugger},
      {"default", TokenType::Default},
      {"delete", TokenType::Delete},
      {"do", TokenType::Do},
      {"else", TokenType::Else},
      {"export", TokenType::Export},
      {"extends", TokenType::Extends},
      {"false", TokenType::False},
      {"finally", TokenType::Finally},
      {"for", TokenType::For},
      {"function", TokenType::Function},
      {"if", TokenType::If},
      {"import", TokenType::Import},
      {"in", TokenType::In},
      {"instanceof", TokenType::InstanceOf},
      {"new", TokenType::New},
      {"null", TokenType::Null},
      {"return", TokenType::Return},
      {"super", TokenType::Super},
      {"switch", TokenType::Switch},
      {"this", TokenType::This},
      {"throw", TokenType::Throw},
      {"true", TokenType::True},
      {"try", TokenType::Try},
      {"typeof", TokenType::TypeOf},
      {"var", TokenType::Var},
      {"void", TokenType::Void},
      {"while", TokenType::While},
      {"with", TokenType::With}};

  std::string_view sv(str, length);
  auto it = keywords.find(sv);
  if (it != keywords.end()) {
    return it->second;
  }

  return TokenType::Identifier;
}

// 現在のトークンを取得
const Token& Scanner::currentToken() const {
  return current_token_;
}

// スキャナーをリセット
void Scanner::reset(size_t position, const ScannerContext& context) {
  // 位置を設定
  current_pos_ = source_start_ + position;

  // コンテキストを設定
  context_ = context;

  // 位置が有効範囲内かチェック
  if (current_pos_ > source_end_) {
    current_pos_ = source_end_;
  }

  // 次の文字を読み込む
  readUtf8Char();

  // トークン開始位置を更新
  token_start_pos_ = current_pos_;

  // 現在の行と列を再計算（オプション）
  recalculateLineAndColumn();
}

// 現在のコンテキストを取得
const ScannerContext& Scanner::getContext() const {
  return context_;
}

// コンテキストを設定
void Scanner::setContext(const ScannerContext& c) {
  context_ = c;
}

// 現在の位置を取得
size_t Scanner::getCurrentPosition() const {
  return current_pos_ - source_start_;
}

// チェックポイントを作成
ScannerCheckpoint Scanner::createCheckpoint() {
  return ScannerCheckpoint{
      current_pos_,
      current_token_,
      current_location_,
      context_};
}

// チェックポイントを復元
void Scanner::restoreCheckpoint(const ScannerCheckpoint& cp) {
  current_pos_ = cp.position;
  current_token_ = cp.token;
  current_location_ = cp.location;
  context_ = cp.context;

  // 次の文字を読み込む
  readUtf8Char();
}

// SIMD最適化を有効化
void Scanner::enableSimdOptimization(bool enable) {
  simd_enabled_ = enable;

  // プラットフォーム固有のSIMD機能を初期化
  if (enable) {
    initializeSimdSupport();
  }
}

// 並列スキャンを試行
bool Scanner::tryParallelScan(int thread_count) {
  // 並列スキャンが可能かチェック
  if (source_end_ - current_pos_ < 1024 || thread_count <= 1) {
    return false;  // 小さすぎるか、スレッド数が不十分
  }

  // 並列スキャンがアクティブであることを記録
  parallel_scan_active_ = true;

  // 実際の並列スキャン処理はここに実装
  // （この実装は簡略化されています）

  return true;
}

// 指定オフセット先の文字をピーク
uint32_t Scanner::peekChar(int offset, ptrdiff_t relative_offset) {
  const char* p = current_pos_ + relative_offset;
  const char* end = source_end_;

  // 指定されたオフセットまでスキップ
  for (int i = 0; i < offset && p < end; ++i) {
    const char* cur = p;
    token_h_filler::Utf8Utils::decodeChar(p, end);
    if (p == cur) {
      p++;  // デコードに失敗した場合は1バイト進める
    }
  }

  if (p >= end) {
    return 0;  // ファイル終端
  }

  const char* fp = p;
  return token_h_filler::Utf8Utils::decodeChar(fp, end);
}

// 指定オフセット先の文字サイズを取得
int Scanner::peekCharSize(int offset, ptrdiff_t relative_offset) {
  const char* p = current_pos_ + relative_offset;
  const char* end = source_end_;

  // 指定されたオフセットまでスキップ
  for (int i = 0; i < offset && p < end; ++i) {
    const char* cur = p;
    token_h_filler::Utf8Utils::decodeChar(p, end);
    if (p == cur) {
      p++;  // デコードに失敗した場合は1バイト進める
    }
  }

  if (p >= end) {
    return 0;  // ファイル終端
  }

  const char* sd = p;
  token_h_filler::Utf8Utils::decodeChar(p, end);
  return static_cast<int>(p - sd);
}

// 行と列の再計算（内部ヘルパー）
void Scanner::recalculateLineAndColumn() {
  // ファイルの先頭から現在位置までスキャンして行と列を計算
  const char* p = source_start_;
  int line = 1;
  int column = 0;

  while (p < current_pos_) {
    if (*p == '\n') {
      line++;
      column = 0;
    } else {
      column++;
    }
    p++;
  }

  current_location_.line = line;
  current_location_.column = column;
  current_location_.offset = current_pos_ - source_start_;
}

// SIMD機能の初期化（内部ヘルパー）
void Scanner::initializeSimdSupport() {
// プラットフォーム固有のSIMD機能を検出・初期化
#if defined(__AVX2__)
  simd_features_ |= SIMD_AVX2;
#endif

#if defined(__SSE4_2__)
  simd_features_ |= SIMD_SSE4_2;
#endif

#if defined(__ARM_NEON)
  simd_features_ |= SIMD_NEON;
#endif
}

}  // namespace lexer
}  // namespace core
}  // namespace aerojs
}  // namespace aerojs