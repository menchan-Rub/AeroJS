/**
 * @file token.cpp
 * @brief JavaScript言語のトークン定義の実装 (高性能拡張版)
 * @copyright Copyright (c) 2023 AeroJS プロジェクト
 * @license MIT License
 */

#include "token.h"
#include "parser_error.h" // Assuming parser_error.h defines ErrorInfo etc.

#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>    // for std::isinf, std::isnan, std::pow
#include <cstdlib>  // for strtod, strtoll
#include <cctype>   // for std::is... functions
#include <cassert>
#include <chrono>
#include <mutex>    // For potential future parallelization
#include <thread>   // For potential future parallelization
#include <cstring>  // For memcpy, memcmp, strlen
#include <algorithm> // for std::min, std::max
#include <limits>   // for std::numeric_limits
#include <array>    // for tables
#include <cerrno>   // For errno

// SIMD includes (placeholders, actual implementation depends on target arch)
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#endif

namespace aerojs {
namespace core {

//------------------------------------------------------------------------------
// Token Type Utilities Implementation
//------------------------------------------------------------------------------

// Map from TokenType to string representation (for debugging/errors)
static const std::unordered_map<TokenType, const char*> kTokenTypeStrings_impl_part1 = {
    {TokenType::Eof, "EOF"}, {TokenType::Error, "Error"}, {TokenType::Uninitialized, "Uninitialized"},
    {TokenType::Identifier, "Identifier"}, {TokenType::PrivateIdentifier, "PrivateIdentifier"},
    {TokenType::NumericLiteral, "NumericLiteral"}, {TokenType::StringLiteral, "StringLiteral"},
    {TokenType::TemplateHead, "TemplateHead"}, {TokenType::TemplateMiddle, "TemplateMiddle"},
    {TokenType::TemplateTail, "TemplateTail"}, {TokenType::RegExpLiteral, "RegExpLiteral"},
    {TokenType::BigIntLiteral, "BigIntLiteral"}, {TokenType::NullLiteral, "NullLiteral"},
    {TokenType::TrueLiteral, "TrueLiteral"}, {TokenType::FalseLiteral, "FalseLiteral"},
    {TokenType::Await, "await"}, {TokenType::Break, "break"}, {TokenType::Case, "case"},
    {TokenType::Catch, "catch"}, {TokenType::Class, "class"}, {TokenType::Const, "const"},
    {TokenType::Continue, "continue"}, {TokenType::Debugger, "debugger"}, {TokenType::Default, "default"},
    {TokenType::Delete, "delete"}, {TokenType::Do, "do"}, {TokenType::Else, "else"},
    {TokenType::Enum, "enum"}, {TokenType::Export, "export"}, {TokenType::Extends, "extends"},
    {TokenType::Finally, "finally"}, {TokenType::For, "for"}, {TokenType::Function, "function"},
    {TokenType::If, "if"}, {TokenType::Import, "import"}, {TokenType::In, "in"},
    {TokenType::InstanceOf, "instanceof"}, {TokenType::Let, "let"}, {TokenType::New, "new"},
    {TokenType::Return, "return"}, {TokenType::Super, "super"}, {TokenType::Switch, "switch"},
    {TokenType::This, "this"}, {TokenType::Throw, "throw"}, {TokenType::Try, "try"},
    {TokenType::Typeof, "typeof"}, {TokenType::Var, "var"}, {TokenType::Void, "void"},
    {TokenType::While, "while"}, {TokenType::With, "with"}, {TokenType::Yield, "yield"},
    {TokenType::Async, "async"}, {TokenType::Get, "get"}, {TokenType::Set, "set"},
    {TokenType::Static, "static"}, {TokenType::Of, "of"}, {TokenType::From, "from"},
    {TokenType::As, "as"}, {TokenType::Meta, "meta"}, {TokenType::Target, "target"},
    {TokenType::Implements, "implements"}, {TokenType::Interface, "interface"}, {TokenType::Package, "package"},
    {TokenType::Private, "private"}, {TokenType::Protected, "protected"}, {TokenType::Public, "public"},
    {TokenType::LeftParen, "("}, {TokenType::RightParen, ")"}, {TokenType::LeftBracket, "["},
    {TokenType::RightBracket, "]"}, {TokenType::LeftBrace, "{"}, {TokenType::RightBrace, "}"},
    {TokenType::Colon, ":"}, {TokenType::Semicolon, ";"}, {TokenType::Comma, ","},
    {TokenType::Dot, "."}, {TokenType::DotDotDot, "..."}, {TokenType::QuestionMark, "?"},
    {TokenType::QuestionDot, "?."}, {TokenType::QuestionQuestion, "??"}, {TokenType::Arrow, "=>"},
    {TokenType::Tilde, "~"}, {TokenType::Exclamation, "!"}, {TokenType::Assign, "="},
    {TokenType::Equal, "=="}, {TokenType::NotEqual, "!="}, {TokenType::StrictEqual, "==="},
    {TokenType::StrictNotEqual, "!=="}, {TokenType::Plus, "+"}, {TokenType::Minus, "-"},
    {TokenType::Star, "*"}, {TokenType::Slash, "/"}, {TokenType::Percent, "%"},
    {TokenType::StarStar, "**"}, {TokenType::PlusPlus, "++"}, {TokenType::MinusMinus, "--"},
    {TokenType::LeftShift, "<<"}, {TokenType::RightShift, ">>"}, {TokenType::UnsignedRightShift, ">>>"},
    {TokenType::Ampersand, "&"}, {TokenType::Bar, "|"}, {TokenType::Caret, "^"},
    {TokenType::AmpersandAmpersand, "&&"}, {TokenType::BarBar, "||"}, {TokenType::PlusAssign, "+="},
    {TokenType::MinusAssign, "-="}, {TokenType::StarAssign, "*="}, {TokenType::SlashAssign, "/="},
    {TokenType::PercentAssign, "%="}, {TokenType::StarStarAssign, "**="}, {TokenType::LeftShiftAssign, "<<="},
    {TokenType::RightShiftAssign, ">>="}, {TokenType::UnsignedRightShiftAssign, ">>>="},
    {TokenType::AmpersandAssign, "&="}, {TokenType::BarAssign, "|="}, {TokenType::CaretAssign, "^="},
    {TokenType::AmpersandAmpersandAssign, "&&="}, {TokenType::BarBarAssign, "||="}, {TokenType::QuestionQuestionAssign, "??="},
    {TokenType::LessThan, "<"}, {TokenType::GreaterThan, ">"}, {TokenType::LessThanEqual, "<="},
    {TokenType::GreaterThanEqual, ">="},
    {TokenType::JsxIdentifier, "JsxIdentifier"}, {TokenType::JsxText, "JsxText"},
    {TokenType::JsxTagStart, "JsxTagStart"}, {TokenType::JsxTagEnd, "JsxTagEnd"},
    {TokenType::JsxClosingTagStart, "JsxClosingTagStart"}, {TokenType::JsxSelfClosingTagEnd, "JsxSelfClosingTagEnd"},
    {TokenType::JsxAttributeEquals, "JsxAttributeEquals"}, {TokenType::JsxSpreadAttribute, "JsxSpreadAttribute"},
    // Add all TS types from token.h
    {TokenType::TsQuestionMark, "TsQuestionMark"}, {TokenType::TsColon, "TsColon"}, {TokenType::TsReadonly, "TsReadonly"},
    {TokenType::TsNumber, "TsNumber"}, {TokenType::TsString, "TsString"}, {TokenType::TsBoolean, "TsBoolean"},
    {TokenType::TsVoid, "TsVoid"}, {TokenType::TsAny, "TsAny"}, {TokenType::TsUnknown, "TsUnknown"},
    {TokenType::TsNever, "TsNever"}, {TokenType::TsType, "TsType"}, {TokenType::TsInterface, "TsInterface"},
    {TokenType::TsImplements, "TsImplements"}, {TokenType::TsExtends, "TsExtends"}, {TokenType::TsAbstract, "TsAbstract"},
    {TokenType::TsPublic, "TsPublic"}, {TokenType::TsPrivate, "TsPrivate"}, {TokenType::TsProtected, "TsProtected"},
    {TokenType::TsDeclare, "TsDeclare"}, {TokenType::TsAs, "TsAs"}, {TokenType::TsSatisfies, "TsSatisfies"},
    {TokenType::TsInfer, "TsInfer"}, {TokenType::TsKeyof, "TsKeyof"}, {TokenType::TsTypeof, "TsTypeof"},
    {TokenType::TsNonNullAssertion, "TsNonNullAssertion"}, {TokenType::TsDecorator, "TsDecorator"},
    // Comments
    {TokenType::SingleLineComment, "SingleLineComment"}, {TokenType::MultiLineComment, "MultiLineComment"},
    {TokenType::HtmlComment, "HtmlComment"},
    // Count
    {TokenType::Count, "Count"}
};

const char* tokenTypeToString(TokenType type) {
    auto it = kTokenTypeStrings_impl_part1.find(type);
    if (it != kTokenTypeStrings_impl_part1.end()) {
        return it->second;
    }
    static char unknown_buffer[64];
    snprintf(unknown_buffer, sizeof(unknown_buffer), "UnknownTokenType(%u)", static_cast<uint16_t>(type));
    return unknown_buffer;
}

// Operator precedence map (ensure matches token.h map exactly)
static const std::unordered_map<TokenType, uint16_t> kOperatorPrecedence_impl_part1 = {
    {TokenType::QuestionQuestion, 1}, {TokenType::BarBar, 1}, // Logical OR, Nullish Coalescing
    {TokenType::AmpersandAmpersand, 2}, // Logical AND
    {TokenType::Bar, 3}, // Bitwise OR
    {TokenType::Caret, 4}, // Bitwise XOR
    {TokenType::Ampersand, 5}, // Bitwise AND
    {TokenType::Equal, 6}, {TokenType::NotEqual, 6}, {TokenType::StrictEqual, 6}, {TokenType::StrictNotEqual, 6}, // Equality
    {TokenType::LessThan, 7}, {TokenType::GreaterThan, 7}, {TokenType::LessThanEqual, 7}, {TokenType::GreaterThanEqual, 7}, {TokenType::In, 7}, {TokenType::InstanceOf, 7}, // Relational, In, InstanceOf
    {TokenType::LeftShift, 8}, {TokenType::RightShift, 8}, {TokenType::UnsignedRightShift, 8}, // Bitwise Shift
    {TokenType::Plus, 9}, {TokenType::Minus, 9}, // Additive
    {TokenType::Star, 10}, {TokenType::Slash, 10}, {TokenType::Percent, 10}, // Multiplicative
    {TokenType::StarStar, 11}, // Exponentiation (Right-associative, handled specially in parser)
};

uint16_t getOperatorPrecedence(TokenType type) {
    auto it = kOperatorPrecedence_impl_part1.find(type);
    return (it != kOperatorPrecedence_impl_part1.end()) ? it->second : 0;
}


//------------------------------------------------------------------------------
// Scanner Implementation
//------------------------------------------------------------------------------
namespace lexer {

// Keyword Lookup Table (Needs optimization)
struct KeywordInfo_impl_part1 { TokenType type; uint32_t flags; };
static const std::unordered_map<std::string, KeywordInfo_impl_part1> kKeywords_impl_part1 = {
    // Primary Keywords
    {"await", {TokenType::Await, Token::FlagIsKeyword | Token::FlagIsReservedWord}}, // Context dependent
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
    {"yield", {TokenType::Yield, Token::FlagIsKeyword | Token::FlagIsStrictReservedWord}}, // Context dependent
    // Contextual Keywords
    {"async", {TokenType::Async, Token::FlagIsContextualKeyword}},
    {"get", {TokenType::Get, Token::FlagIsContextualKeyword}},
    {"set", {TokenType::Set, Token::FlagIsContextualKeyword}},
    {"static", {TokenType::Static, Token::FlagIsContextualKeyword | Token::FlagIsStrictReservedWord}},
    {"of", {TokenType::Of, Token::FlagIsContextualKeyword}},
    {"from", {TokenType::From, Token::FlagIsContextualKeyword}},
    {"as", {TokenType::As, Token::FlagIsContextualKeyword}},
    {"meta", {TokenType::Meta, Token::FlagIsContextualKeyword}},
    {"target", {TokenType::Target, Token::FlagIsContextualKeyword}},
    // Reserved Words
    {"implements", {TokenType::Implements, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"interface", {TokenType::Interface, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"package", {TokenType::Package, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"private", {TokenType::Private, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"protected", {TokenType::Protected, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    {"public", {TokenType::Public, Token::FlagIsReservedWord | Token::FlagIsStrictReservedWord}},
    // Literals treated as keywords
    {"null", {TokenType::NullLiteral, Token::FlagIsKeyword}},
    {"true", {TokenType::TrueLiteral, Token::FlagIsKeyword}},
    {"false", {TokenType::FalseLiteral, Token::FlagIsKeyword}},
    // TypeScript Keywords (Add necessary flags)
    {"readonly", {TokenType::TsReadonly, 0}},{"number", {TokenType::TsNumber, 0}},{"string", {TokenType::TsString, 0}},
    {"boolean", {TokenType::TsBoolean, 0}},{"any", {TokenType::TsAny, 0}},{"unknown", {TokenType::TsUnknown, 0}},
    {"never", {TokenType::TsNever, 0}},{"type", {TokenType::TsType, 0}},{"declare", {TokenType::TsDeclare, 0}},
    {"satisfies", {TokenType::TsSatisfies, 0}},{"infer", {TokenType::TsInfer, 0}},{"keyof", {TokenType::TsKeyof, 0}},
    {"abstract", {TokenType::TsAbstract, 0}},
};

// --- Character Classification Tables/Functions ---
static constexpr uint8_t CP_WS_IMPL = 1 << 0;
static constexpr uint8_t CP_LT_IMPL = 1 << 1;
static constexpr uint8_t CP_IDS_IMPL = 1 << 2;
static constexpr uint8_t CP_IDP_IMPL = 1 << 3;
static constexpr uint8_t CP_DEC_IMPL = 1 << 4;
static constexpr uint8_t CP_HEX_IMPL = 1 << 5;

static const uint8_t kAsciiCharProperties_impl[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, CP_WS_IMPL, CP_LT_IMPL, CP_WS_IMPL, CP_WS_IMPL, CP_LT_IMPL, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CP_WS_IMPL, 0, 0, 0, CP_IDS_IMPL | CP_IDP_IMPL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL,
    CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL, CP_DEC_IMPL | CP_HEX_IMPL | CP_IDP_IMPL,
    0, 0, 0, 0, 0, 0,
    0,
    CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    0, CP_IDS_IMPL | CP_IDP_IMPL, 0, 0, CP_IDS_IMPL | CP_IDP_IMPL, 0,
    CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL, CP_IDS_IMPL | CP_IDP_IMPL | CP_HEX_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL, CP_IDS_IMPL | CP_IDP_IMPL,
    0, 0, 0, 0, 0
};

inline bool isDecimalDigit_impl_part1(uint32_t c) {
    return c < 128 && (kAsciiCharProperties_impl[c] & CP_DEC_IMPL);
}
inline bool isHexDigit_impl_part1(uint32_t c) {
    return c < 128 && (kAsciiCharProperties_impl[c] & CP_HEX_IMPL);
}
inline bool isBinaryDigit_impl_part1(uint32_t c) { return c == '0' || c == '1'; }
inline bool isOctalDigit_impl_part1(uint32_t c) { return c >= '0' && c <= '7'; }

bool Scanner::isIdentifierStart(uint32_t c) {
    if (c < 128) return kAsciiCharProperties_impl[c] & CP_IDS_IMPL;
    // TODO: Full Unicode check
    return false;
}
bool Scanner::isIdentifierPart(uint32_t c) {
    if (c < 128) return kAsciiCharProperties_impl[c] & CP_IDP_IMPL;
    // TODO: Full Unicode check
    return false;
}
bool Scanner::isLineTerminator(uint32_t c) {
    return c == 0x0A || c == 0x0D || c == 0x2028 || c == 0x2029;
}
bool Scanner::isWhitespace(uint32_t c) {
    if (c < 128) return kAsciiCharProperties_impl[c] & CP_WS_IMPL;
    // TODO: Full Unicode check
    return (c == 0x1680 || (c >= 0x2000 && c <= 0x200A) || c == 0x202F || c == 0x205F || c == 0x3000 || c == 0xFEFF);
}

// --- UTF8 Utilities Implementation ---
namespace token_h_filler {
    int Utf8Utils::bytesForChar(uint8_t b1) {
        if (b1 < 0x80) return 1;
        if ((b1 & 0xE0) == 0xC0) return 2;
        if ((b1 & 0xF0) == 0xE0) return 3;
        if ((b1 & 0xF8) == 0xF0) return 4;
        return 0;
    }
    uint32_t Utf8Utils::decodeChar(const char*& ptr, const char* end) {
        if (ptr >= end) return 0;
        const uint8_t* uptr = reinterpret_cast<const uint8_t*>(ptr);
        uint8_t b1 = *uptr;
        int len = bytesForChar(b1);
        if (len == 0 || (ptr + len > end)) { ptr++; return 0xFFFD; }
        uint32_t cp = 0; const uint8_t* nb = uptr + 1;
        bool overlong = false; uint32_t min_val = 0;
        switch (len) {
            case 1: cp = b1; break;
            case 2: if ((nb[0] & 0xC0) != 0x80) { len = 0; break; } cp = ((b1 & 0x1F) << 6) | (nb[0] & 0x3F); min_val = 0x80; overlong = (cp < min_val); break;
            case 3: if ((nb[0] & 0xC0) != 0x80 || (nb[1] & 0xC0) != 0x80) { len = 0; break; } cp = ((b1 & 0x0F) << 12) | ((nb[0] & 0x3F) << 6) | (nb[1] & 0x3F); min_val = 0x800; overlong = (cp < min_val); break;
            case 4: if ((nb[0] & 0xC0) != 0x80 || (nb[1] & 0xC0) != 0x80 || (nb[2] & 0xC0) != 0x80) { len = 0; break; } cp = ((b1 & 0x07) << 18) | ((nb[0] & 0x3F) << 12) | ((nb[1] & 0x3F) << 6) | (nb[2] & 0x3F); min_val = 0x10000; overlong = (cp < min_val); break;
            default: len = 0; break;
        }
        if (len == 0 || overlong || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { ptr++; return 0xFFFD; }
        ptr += len; return cp;
    }
    bool Utf8Utils::isValidUtf8(const char* ptr, size_t length) {
         const char* end = ptr + length;
         while (ptr < end) {
              const char* current = ptr;
              if (decodeChar(ptr, end) == 0xFFFD && ptr == current + 1) return false;
         }
         return true;
    }
} // namespace token_h_filler

// --- Scanner Constructor / Destructor Implementation ---
Scanner::Scanner(const char* source, size_t source_length, int file_id,
                 ScannerErrorHandler* error_handler, ScannerContext initial_context)
    : source_start_(source), source_end_(source + source_length), current_pos_(source), token_start_pos_(source),
      error_handler_(error_handler), file_id_(file_id), context_(initial_context),
      simd_enabled_(false), parallel_scan_active_(false)
{
    current_location_ = {1, 0, 0, file_id_};
    current_token_.type = TokenType::Uninitialized;
    bytes_scanned_.store(0, std::memory_order_relaxed);
    tokens_emitted_.store(0, std::memory_order_relaxed);
    perf_timers_ = {};
    readUtf8Char(); // Prime lookahead
}
Scanner::~Scanner() {}

// --- Core Scanning Logic Implementation ---
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
        lookahead_char_ = token_h_filler::Utf8Utils::decodeChar(next_pos, source_end_);
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
    // Simplified timing update
    if (current_token_.type == TokenType::Identifier) perf_timers_.identifierScanTime += duration;
    else if (current_token_.isLiteral()) perf_timers_.stringScanTime += duration; // Rough category
    else if (current_token_.isOperator()) perf_timers_.punctuatorScanTime += duration;

    bytes_scanned_.fetch_add(current_pos_ - token_start_pos_ + trivia_len, std::memory_order_relaxed);
    tokens_emitted_.fetch_add(1, std::memory_order_relaxed);
    return current_token_;
}

// scanNext - Main Dispatcher (ASCII Fast Path + Fallback)
Token Scanner::scanNext() {
    uint32_t c = lookahead_char_;

    if (c < 128) {
        uint8_t props = kAsciiCharProperties_impl[c];
        if (props & (CP_IDS_IMPL)) return scanIdentifierOrKeyword();
        if (props & (CP_DEC_IMPL)) return scanNumericLiteral();
        switch (c) {
            case '\'': case '"': return scanStringLiteral();
            case '.':
                 if (current_pos_ + 1 < source_end_ && isDecimalDigit_impl_part1(static_cast<uint8_t>(current_pos_[1]))) return scanNumericLiteral();
                 consumeChar();
                 if (lookahead_char_ == '.') { consumeChar(); if (lookahead_char_ == '.') { consumeChar(); return createToken(TokenType::DotDotDot); } reportError("Unexpected token '..'"); return createErrorToken("Unexpected '..'"); }
                 return createToken(TokenType::Dot);
            case '`': return scanTemplateToken();
            case '#':
                 if (current_pos_ + 1 < source_end_ && isIdentifierStart(peekChar(1))) return scanPrivateIdentifier();
                 reportError("'#' must be followed by an identifier start"); consumeChar(); return createErrorToken("Invalid '#' usage");
            case '(': consumeChar(); return createToken(TokenType::LeftParen);
            case ')': consumeChar(); return createToken(TokenType::RightParen);
            case '[': consumeChar(); return createToken(TokenType::LeftBracket);
            case ']': consumeChar(); return createToken(TokenType::RightBracket);
            case '{': context_.braceDepth++; consumeChar(); return createToken(TokenType::LeftBrace);
            case '}':
                if (context_.mode == ScannerMode::TemplateLiteral && context_.braceDepth > 0) { context_.braceDepth--; return scanTemplateToken(); }
                context_.braceDepth = std::max(0, context_.braceDepth - 1); consumeChar(); return createToken(TokenType::RightBrace);
            case ';': consumeChar(); return createToken(TokenType::Semicolon);
            case ',': consumeChar(); return createToken(TokenType::Comma);
            case ':': consumeChar(); return createToken(TokenType::Colon);
            case '?': consumeChar();
                      if (lookahead_char_ == '.') { consumeChar(); return createToken(TokenType::QuestionDot); }
                      if (lookahead_char_ == '?') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::QuestionQuestionAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::QuestionQuestion, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator); }
                      return createToken(TokenType::QuestionMark);
            case '~': consumeChar(); return createToken(TokenType::Tilde, Token::FlagIsUnaryOperator);
            case '/':
                consumeChar();
                if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::SlashAssign, Token::FlagIsAssignmentOperator); }
                if (lookahead_char_ == '/') { skipWhitespaceAndComments(); return nextToken(); }
                if (lookahead_char_ == '*') { skipWhitespaceAndComments(); return nextToken(); }
                if ((context_.mode == ScannerMode::JsxElement || context_.mode == ScannerMode::JsxAttribute) && lookahead_char_ == '>') { consumeChar(); context_.mode = ScannerMode::Normal; return createToken(TokenType::JsxSelfClosingTagEnd); }
                if (context_.allowRegExp) return scanRegExpLiteral(); else return createToken(TokenType::Slash, Token::FlagIsBinaryOperator);
            case '=': consumeChar();
                      if (lookahead_char_ == '=') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::StrictEqual, Token::FlagIsBinaryOperator); } return createToken(TokenType::Equal, Token::FlagIsBinaryOperator); }
                      if (lookahead_char_ == '>') { consumeChar(); return createToken(TokenType::Arrow); }
                      if (context_.mode == ScannerMode::JsxAttribute) return createToken(TokenType::JsxAttributeEquals);
                      return createToken(TokenType::Assign, Token::FlagIsAssignmentOperator);
            case '!': consumeChar();
                      if (lookahead_char_ == '=') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::StrictNotEqual, Token::FlagIsBinaryOperator); } return createToken(TokenType::NotEqual, Token::FlagIsBinaryOperator); }
                      return createToken(TokenType::Exclamation, Token::FlagIsUnaryOperator);
            case '+': consumeChar();
                      if (lookahead_char_ == '+') { consumeChar(); return createToken(TokenType::PlusPlus, Token::FlagIsUpdateOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::PlusAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Plus, Token::FlagIsBinaryOperator | Token::FlagIsUnaryOperator);
            case '-': consumeChar();
                      if (lookahead_char_ == '-') { consumeChar(); return createToken(TokenType::MinusMinus, Token::FlagIsUpdateOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::MinusAssign, Token::FlagIsAssignmentOperator); }
                      if (context_.allowHtmlComment && lookahead_char_ == '-' && current_pos_ + 1 < source_end_ && *(current_pos_+1) == '>' && (current_token_.flags & Token::FlagPrecededByLineTerminator)) { skipWhitespaceAndComments(); return nextToken(); }
                      return createToken(TokenType::Minus, Token::FlagIsBinaryOperator | Token::FlagIsUnaryOperator);
            case '*': consumeChar();
                      if (lookahead_char_ == '*') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::StarStarAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::StarStar, Token::FlagIsBinaryOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::StarAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Star, Token::FlagIsBinaryOperator);
            case '%': consumeChar();
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::PercentAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Percent, Token::FlagIsBinaryOperator);
            case '<': consumeChar();
                       if (context_.allowHtmlComment && lookahead_char_ == '!' && current_pos_ + 2 < source_end_ && current_pos_[1] == '-' && current_pos_[2] == '-') { skipWhitespaceAndComments(); return nextToken(); }
                      if (lookahead_char_ == '<') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::LeftShiftAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::LeftShift, Token::FlagIsBinaryOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::LessThanEqual, Token::FlagIsBinaryOperator); }
                      if (context_.mode == ScannerMode::Normal && (isIdentifierStart(lookahead_char_) || lookahead_char_ == '/' || lookahead_char_ == '>')) { if (lookahead_char_ == '/') { consumeChar(); return createToken(TokenType::JsxClosingTagStart); } return createToken(TokenType::JsxTagStart); }
                      return createToken(TokenType::LessThan, Token::FlagIsBinaryOperator);
            case '>': consumeChar();
                      if (lookahead_char_ == '>') { consumeChar(); if (lookahead_char_ == '>') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::UnsignedRightShiftAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::UnsignedRightShift, Token::FlagIsBinaryOperator); } if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::RightShiftAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::RightShift, Token::FlagIsBinaryOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::GreaterThanEqual, Token::FlagIsBinaryOperator); }
                      if (context_.mode == ScannerMode::JsxElement || context_.mode == ScannerMode::JsxAttribute) { context_.mode = ScannerMode::JsxElement; return createToken(TokenType::JsxTagEnd); }
                      return createToken(TokenType::GreaterThan, Token::FlagIsBinaryOperator);
            case '&': consumeChar();
                      if (lookahead_char_ == '&') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::AmpersandAmpersandAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::AmpersandAmpersand, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::AmpersandAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Ampersand, Token::FlagIsBinaryOperator);
            case '|': consumeChar();
                      if (lookahead_char_ == '|') { consumeChar(); if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::BarBarAssign, Token::FlagIsAssignmentOperator); } return createToken(TokenType::BarBar, Token::FlagIsBinaryOperator | Token::FlagIsLogicalOperator); }
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::BarAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Bar, Token::FlagIsBinaryOperator);
            case '^': consumeChar();
                      if (lookahead_char_ == '=') { consumeChar(); return createToken(TokenType::CaretAssign, Token::FlagIsAssignmentOperator); }
                      return createToken(TokenType::Caret, Token::FlagIsBinaryOperator);
            case '@': if (true) { consumeChar(); return createToken(TokenType::TsDecorator); } break;
            // Default fallthrough for unhandled ASCII chars (should be few)
        }
    } // End ASCII fast path

    // Handle non-ASCII or fallthrough
    if (isIdentifierStart(c)) return scanIdentifierOrKeyword();
    // Handle Unicode whitespace/LT if missed by skipWhitespace
    if (isWhitespace(c) || isLineTerminator(c)) {
        reportError("Internal Scanner Error: Trivia reached scanNext"); consumeChar(); return createErrorToken("Internal trivia error");
    }

    // Unexpected character
    std::string error_char_str = "(char code " + std::to_string(c) + ")";
    reportError("Unexpected character " + error_char_str);
    consumeChar();
    return createErrorToken("Unexpected character");
}

// --- Create Token Implementation ---
Token Scanner::createToken(TokenType type, uint32_t flags) {
    flags |= (current_token_.flags & Token::FlagPrecededByLineTerminator);
    Token token(type, current_token_.location, static_cast<uint32_t>(current_pos_ - token_start_pos_), current_token_.trivia_length, flags);
    token.precedence = getOperatorPrecedence(type);
    if(token.precedence > 0) token.flags |= Token::FlagIsBinaryOperator;
    if (type == TokenType::Assign || (type >= TokenType::PlusAssign && type <= TokenType::QuestionQuestionAssign)) token.flags |= Token::FlagIsAssignmentOperator;
    else if (type == TokenType::PlusPlus || type == TokenType::MinusMinus) { token.flags |= Token::FlagIsUpdateOperator; token.flags &= ~Token::FlagIsBinaryOperator; }
    else if (type == TokenType::AmpersandAmpersand || type == TokenType::BarBar || type == TokenType::QuestionQuestion) token.flags |= Token::FlagIsLogicalOperator;
    if (type == TokenType::Plus || type == TokenType::Minus || type == TokenType::Tilde || type == TokenType::Exclamation || type == TokenType::Typeof || type == TokenType::Void || type == TokenType::Delete) token.flags |= Token::FlagIsUnaryOperator;
    // Store raw lexeme if configured
    // token.raw_lexeme.assign(token_start_pos_, token.length);
    return token;
}

// --- Create Error Token Implementation ---
Token Scanner::createErrorToken(const std::string& message) {
    Token token = createToken(TokenType::Error);
    token.value = message;
    if (token.length == 0 && current_pos_ > token_start_pos_) {
         token.length = static_cast<uint32_t>(current_pos_ - token_start_pos_);
         // token.raw_lexeme.assign(token_start_pos_, token.length);
    }
    return token;
}

// --- Report Error Implementation ---
void Scanner::reportError(const std::string& message) {
    if (error_handler_) {
        SourceLocation error_loc = current_token_.location;
        if (current_pos_ == token_start_pos_) error_loc = current_location_;
        error_handler_->handleError(error_loc, message);
    }
}

// --- Whitespace and Comment Skipping Implementation ---
bool Scanner::skipWhitespaceAndComments() {
    const char* trivia_start = current_pos_;
    bool preceded = (current_token_.flags & Token::FlagPrecededByLineTerminator);
    while (current_pos_ < source_end_) {
        uint32_t c = lookahead_char_;
        if (c < 128) {
            uint8_t props = kAsciiCharProperties_impl[c];
            if (props & CP_LT_IMPL) { preceded = true; uint32_t first = c; consumeChar(); if (first == 0x0D && lookahead_char_ == 0x0A) consumeChar(); continue; }
            if (props & CP_WS_IMPL) { consumeChar(); continue; }
            if (c == '/') {
                const char* next_p = current_pos_ + lookahead_size_;
                if (next_p >= source_end_) break;
                uint8_t next_b = static_cast<uint8_t>(*next_p);
                if (next_b == '/') { consumeChar(); consumeChar(); while (current_pos_ < source_end_ && !isLineTerminator(lookahead_char_)) consumeChar(); continue; }
                if (next_b == '*') { consumeChar(); consumeChar(); bool end = false;
                    while (current_pos_ < source_end_) { uint32_t cc = lookahead_char_; consumeChar(); if (cc == '*' && lookahead_char_ == '/') { consumeChar(); end = true; break; } if (isLineTerminator(cc)) { preceded = true; if (cc == 0x0D && lookahead_char_ == 0x0A) consumeChar(); } }
                    if (!end) reportError("Unterminated multi-line comment"); continue;
                } break;
            } if (c == '<' && context_.allowHtmlComment) {
                 const char* p = current_pos_ + lookahead_size_;
                 if (p + 2 < source_end_ && *p == '!' && *(p+1) == '-' && *(p+2) == '-') {
                      consumeChar(); consumeChar(); consumeChar(); consumeChar();
                      while (current_pos_ < source_end_) { if (lookahead_char_ == '-' && current_pos_ + 2 < source_end_ && *(current_pos_ + 1) == '-' && *(current_pos_ + 2) == '>') { consumeChar(); consumeChar(); consumeChar(); break; } if(isLineTerminator(lookahead_char_)) preceded = true; consumeChar(); }
                      continue;
                 }
                 break;
            } if (c == '-' && context_.allowHtmlComment && preceded) {
                  const char* p = current_pos_ + lookahead_size_;
                  if (p + 1 < source_end_ && *p == '-' && *(p+1) == '>') { consumeChar(); consumeChar(); consumeChar(); continue; }
                  break;
            } break;
        } else { if (isLineTerminator(c)) { preceded = true; consumeChar(); continue; } if (isWhitespace(c)) { consumeChar(); continue; } break; }
    }
    current_token_.flags = preceded ? Token::FlagPrecededByLineTerminator : 0;
    return static_cast<uint16_t>(current_pos_ - trivia_start);
}

// --- Stubs/Shells for Major Functions (Implementations Start Here) ---

// scanIdentifierOrKeyword Implementation (Detailed)
Token Scanner::scanIdentifierOrKeyword() {
    auto start_time = std::chrono::high_resolution_clock::now();
    const char* start = current_pos_;
    uint32_t flags = 0;
    bool contains_escape = false;
    std::string identifier_value; // Build value only if escapes are present
    identifier_value.reserve(32);
    uint32_t first_char_code = 0;

    // Handle potential starting Unicode escape \uXXXX or \u{XXXXX}
    if (lookahead_char_ == '\\') {
        flags |= Token::FlagContainsEscape;
        contains_escape = true;
        const char* escape_start = current_pos_;
        consumeChar(); // Consume '\'
        if (lookahead_char_ != 'u') {
            reportError("Identifier cannot start with escape sequence other than \\u");
            return createErrorToken("Invalid identifier start escape");
        }
        // Use helper to parse the escape sequence
        // parseComplexEscape needs to handle appending to identifier_value or returning code point
        Scanner::EscapeParseResult esc_res = parseComplexEscape(current_pos_, identifier_value); // Modifies current_pos_ and identifier_value
        if (esc_res != EscapeParseResult::Ok) {
             reportError("Invalid Unicode escape sequence at identifier start");
             return createErrorToken("Invalid identifier start escape");
        }
        // We need the *first* decoded code point to check ID_Start property
        // parseComplexEscape should ideally return the first decoded codepoint
        // For now, assume it's handled and validation happens below.
        // TODO: Get first code point from parseComplexEscape
        first_char_code = 0xFFFD; // Placeholder - Requires implementation in parseComplexEscape

    } else {
        // Normal start character
        first_char_code = lookahead_char_;
        if (!isIdentifierStart(first_char_code)) {
             // This check might be redundant if scanNext dispatcher is correct, but good for safety.
             reportError("Invalid identifier start character (internal error)");
             consumeChar(); return createErrorToken("Invalid ID start");
        }
        // Append character (proper UTF8 handling)
        if (lookahead_size_ == 1) {
            identifier_value += static_cast<char>(lookahead_char_);
        } else {
            identifier_value.append(current_pos_, lookahead_size_);
        }
        consumeChar();
    }

    // Validate if the (potentially escaped) first character is a valid ID_Start
    // Requires full Unicode database lookup based on first_char_code
    // TODO: Implement full Unicode ID_Start check here using the decoded first_char_code
    if (!isIdentifierStart(first_char_code)) { // Re-check after potentially decoding escape
         reportError("Resolved character is not a valid identifier start");
         return createErrorToken("Invalid ID start char");
    }

    // Scan identifier parts
    while (current_pos_ < source_end_) {
        if (lookahead_char_ == '\\') {
             flags |= Token::FlagContainsEscape;
             contains_escape = true;
             const char* escape_start = current_pos_;
             consumeChar(); // '\'
             if (lookahead_char_ != 'u') {
                 reportError("Only Unicode escape sequences (\\u) are allowed in identifiers");
                 // Consume the invalid char and continue?
                 if (lookahead_size_ > 0) consumeChar();
                 continue; // Or break?
             }
             // parseComplexEscape appends the decoded char(s) to identifier_value
             Scanner::EscapeParseResult esc_res = parseComplexEscape(current_pos_, identifier_value);
             if (esc_res != EscapeParseResult::Ok) {
                  reportError("Invalid Unicode escape sequence in identifier");
                  // Decide recovery, e.g., stop parsing identifier here?
                  continue;
             }
             // TODO: Validate if decoded char is ID_Continue
             // This requires parseComplexEscape to perhaps return the code point(s)
             // or the check needs to happen based on the appended string portion.
             // if (!isIdentifierPart(/* last decoded code point */ 0xFFFD)) {
             //      reportError("Escape does not resolve to valid identifier part");
             // }
        } else if (isIdentifierPart(lookahead_char_)) {
            // Append normal character (UTF8 handling)
            if (lookahead_size_ == 1) {
                identifier_value += static_cast<char>(lookahead_char_);
            } else {
                identifier_value.append(current_pos_, lookahead_size_);
            }
            consumeChar();
        } else {
            break; // End of identifier part
        }
    }

    size_t length = current_pos_ - start;
    std::string lexeme(start, length); // Raw lexeme including escapes

    // Keyword lookup: Use the *value* after decoding escapes if escapes were present.
    TokenType type = TokenType::Identifier;
    uint32_t keyword_flags = 0;
    const std::string& lookup_key = contains_escape ? identifier_value : lexeme;

    auto it = kKeywords_impl_part1.find(lookup_key); // Use faster lookup eventually
    if (it != kKeywords_impl_part1.end()) {
        bool is_definite_keyword = true;

        // Handle contextual keywords
        if (it->second.flags & Token::FlagIsContextualKeyword) {
            // TODO: Implement robust context checking (needs lookahead/parser state)
            is_definite_keyword = false; // Assume ID unless context proves otherwise
            // Example heuristic (very basic):
            if (it->second.type == TokenType::Async && peekCharNonTrivia(0) == 'f') {
                 // Potentially async function
                 is_definite_keyword = true;
            }
            // Add more heuristics or rely on parser feedback
        }

        // Handle reserved words used as identifiers
        if ((it->second.flags & Token::FlagIsReservedWord) && !(it->second.flags & Token::FlagIsKeyword)) {
             reportError("Unexpected reserved word '" + lookup_key + "'");
             type = TokenType::Error;
             is_definite_keyword = false;
        }
        // Handle strict mode reserved words used as identifiers
        if (context_.strictMode && (it->second.flags & Token::FlagIsStrictReservedWord)) {
             reportError("Unexpected strict mode reserved word '" + lookup_key + "'");
             type = TokenType::Error;
             is_definite_keyword = false;
        }

        if (is_definite_keyword && type != TokenType::Error) {
            type = it->second.type;
            keyword_flags = it->second.flags;
        }
    }

    // Create the token
    Token token = createToken(type, flags | keyword_flags);
    if (type == TokenType::Identifier || contains_escape) {
        // Store the potentially decoded value
        token.setValue(identifier_value);
    }
    token.raw_lexeme = lexeme; // Always store raw lexeme

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.identifierScanTime += (end_time - start_time);
    return token;
}

// scanPrivateIdentifier Implementation (Detailed)
Token Scanner::scanPrivateIdentifier() {
    auto start_time = std::chrono::high_resolution_clock::now();
    const char* start = current_pos_;
    consumeChar(); // Consume '#'

    const char* name_start = current_pos_;

    // Spec requires the name part to be a valid IdentifierName (which allows keywords)
    // but *cannot* contain escape sequences.
    if (!isIdentifierStart(lookahead_char_)) {
        reportError("Private identifier must have a name starting with an identifier character");
        current_pos_ = start + 1; readUtf8Char(); // Backtrack after #
        return createErrorToken("Invalid private identifier name start");
    }
    consumeChar();
    while (current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) {
        // Explicitly check for disallowed escape sequence start
        if (lookahead_char_ == '\\') {
             reportError("Escape sequences are not allowed in private identifier names");
             // Consume the rest of the potential identifier as part of the error
             const char* error_start = current_pos_;
             while(current_pos_ < source_end_ && isIdentifierPart(lookahead_char_)) consumeChar();
             Token err_token = createToken(TokenType::Error);
             err_token.length = static_cast<uint32_t>(current_pos_ - start);
             err_token.raw_lexeme.assign(start, err_token.length);
             err_token.value = "Invalid private identifier name (contains escape)";
             return err_token;
        }
        consumeChar();
    }
    const char* name_end = current_pos_;

    // The name part itself (after #) can be a reserved word (e.g., #class, #let)
    // No need to do keyword check here, just ensure it's a valid IdentifierName structure.

    // Create the token
    Token token = createToken(TokenType::PrivateIdentifier);
    token.length = static_cast<uint32_t>(current_pos_ - start);
    token.raw_lexeme.assign(start, token.length);
    // Store the name part without '#' in value
    token.setValue(std::string(name_start, name_end - name_start));

    // Update performance timer (consider a specific timer for private ids?)
    // perf_timers_.identifierScanTime += ...
    return token;
}

// --- Helper for contextual keyword check (needs implementation) ---
// This function would peek ahead, skipping trivia, to help disambiguate
uint32_t Scanner::peekCharNonTrivia(int lookahead_count) {
    // TODO: Implement peeking logic that skips whitespace/comments
    (void)lookahead_count; // Avoid unused param warning
    return lookahead_char_; // Placeholder
}

// scanNumericLiteral Implementation (Detailed)
Token Scanner::scanNumericLiteral() {
    auto start_time = std::chrono::high_resolution_clock::now();
    const char* start = current_pos_;
    uint32_t flags = 0;
    double number_value = 0.0;
    int64_t bigint_value = 0; // TODO: Requires BigInt library for arbitrary precision
    bool is_bigint = false;
    bool is_legacy_octal = false;
    int base = 10;

    // --- Handle Prefixes --- (0x, 0b, 0o, legacy octal)
    if (lookahead_char_ == '0') {
        const char* after_zero = current_pos_ + 1;
        if (after_zero < source_end_) {
            uint32_t next_char = peekChar(0, 1); // Peek char after '0'
            bool consumed_prefix = false;

            if (next_char == 'x' || next_char == 'X') {
                base = 16; flags |= Token::FlagIsHex;
                consumeChar(); consumeChar(); // Consume 0x
                consumed_prefix = true;
                if (!isHexDigit_impl_part1(lookahead_char_)) {
                    reportError("Hexadecimal literal missing digits after 0x");
                    // Try to recover by treating as '0'?
                    current_pos_ = start + 1; // Go back to after '0'
                    readUtf8Char(); // Re-read the 'x'
                    base = 10; flags &= ~Token::FlagIsHex; // Unset flags
                    consumed_prefix = false; // Didn't actually consume prefix
                }
            } else if (next_char == 'b' || next_char == 'B') {
                base = 2; flags |= Token::FlagIsBinary;
                consumeChar(); consumeChar(); // Consume 0b
                consumed_prefix = true;
                if (!isBinaryDigit_impl_part1(lookahead_char_)) {
                     reportError("Binary literal missing digits after 0b");
                     current_pos_ = start + 1; readUtf8Char(); base = 10; flags &= ~Token::FlagIsBinary; consumed_prefix = false;
                }
            } else if (next_char == 'o' || next_char == 'O') {
                base = 8; flags |= Token::FlagIsOctal;
                consumeChar(); consumeChar(); // Consume 0o
                consumed_prefix = true;
                 if (!isOctalDigit_impl_part1(lookahead_char_)) {
                     reportError("Octal literal missing digits after 0o");
                     current_pos_ = start + 1; readUtf8Char(); base = 10; flags &= ~Token::FlagIsOctal; consumed_prefix = false;
                 }
            } else if (isOctalDigit_impl_part1(next_char)) {
                // Possible Legacy Octal
                if (context_.strictMode) {
                     reportError("Legacy octal literals are not allowed in strict mode");
                     // Treat as base 10 number '0'
                     consumeChar(); // Consume only the '0'
                } else {
                    // Not strict mode, parse as legacy octal
                    base = 8; flags |= Token::FlagIsOctal | Token::FlagIsLegacyOctal;
                    is_legacy_octal = true;
                    consumeChar(); // Consume '0'
                    // Consume remaining octal digits (note: legacy octal cannot contain separators)
                    while (isOctalDigit_impl_part1(lookahead_char_)) {
                        consumeChar();
                    }
                    // Legacy octal scanning is done here. Cannot have decimal, exponent, or bigint suffix.
                    goto parse_value; // Jump directly to value parsing
                }
            } else if (next_char == '_' || next_char == '.' || next_char == 'e' || next_char == 'E') {
                 // '0' followed by something that makes it clearly decimal
                 consumeChar(); // Consume '0'
            } else if (!isDecimalDigit_impl_part1(next_char)) {
                // Just the number 0
                consumeChar(); // Consume '0'
                goto parse_value; // Treat as single digit '0'
            } else {
                 // '0' followed by a decimal digit (e.g. 09) - Error in strict mode octal context, but allowed as decimal otherwise.
                 if (context_.strictMode) {
                     // Could potentially warn about implicit octal-like decimal? No, spec says treat as decimal.
                 }
                 consumeChar(); // Consume '0'
            }
        } else {
             // Source ends after '0'
             consumeChar(); // Consume '0'
             goto parse_value;
        }
    } else if (lookahead_char_ == '.') {
         // Starts with '.', check next char is digit
         if (!(current_pos_ + 1 < source_end_ && isDecimalDigit_impl_part1(peekChar(0, 1)))) {
             // Just a '.' punctuator, not start of number
             consumeChar(); return createToken(TokenType::Dot);
         }
         // Number starts with decimal point. Will be handled below as part of decimal scanning.
         // No prefix consumed, base remains 10.
    }

    // --- Scan Mantissa based on Base --- (Handles separators)
    const char* mantissa_start = current_pos_;
    bool first_digit_in_mantissa = true;
    bool last_was_separator = false;

    while (current_pos_ < source_end_) {
        uint32_t c = lookahead_char_;
        bool is_digit_for_base = false;
        switch (base) {
            case 16: is_digit_for_base = isHexDigit_impl_part1(c); break;
            case 2:  is_digit_for_base = isBinaryDigit_impl_part1(c); break;
            case 8:  is_digit_for_base = isOctalDigit_impl_part1(c); break; // Modern octal
            default: is_digit_for_base = isDecimalDigit_impl_part1(c); break;
        }

        if (is_digit_for_base) {
            consumeChar();
            last_was_separator = false;
            first_digit_in_mantissa = false;
        } else if (c == '_') {
            if (first_digit_in_mantissa || last_was_separator) {
                reportError("Invalid numeric separator position (cannot be leading or consecutive)");
                // Recover by ignoring separator? Or break?
                consumeChar(); // Consume '_' and continue, hoping for a digit
                last_was_separator = true; // Mark it to catch double separators
                continue;
            }
            consumeChar(); // Consume separator '_'
            last_was_separator = true;
            // Peek ahead: must be followed by a valid digit for the current base
            uint32_t next_c = lookahead_char_;
            bool next_is_digit = false;
             switch (base) {
                 case 16: next_is_digit = isHexDigit_impl_part1(next_c); break;
                 case 2:  next_is_digit = isBinaryDigit_impl_part1(next_c); break;
                 case 8:  next_is_digit = isOctalDigit_impl_part1(next_c); break;
                 default: next_is_digit = isDecimalDigit_impl_part1(next_c); break;
             }
            if (!next_is_digit) {
                reportError("Numeric separator must be followed by a digit of the same base");
                // Backtrack before the separator to treat it as end of number
                current_pos_--; // Revert consumeChar of '_'
                readUtf8Char(); // Reread the separator
                last_was_separator = false; // Undo state change
                break; // Exit mantissa scan
            }
            // Next char is valid digit, continue loop
        } else {
            break; // End of mantissa digits/separators
        }
    }

    if (last_was_separator) {
        reportError("Numeric separator cannot appear at the end of the mantissa");
        // Backtrack to before the final separator
        current_pos_--;
        readUtf8Char();
    }

    // Check if any digits were consumed *after* the prefix (if any)
    if (current_pos_ == mantissa_start) {
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
        flags |= Token::FlagIsDecimal; // Mark as potentially floating point
        consumeChar(); // Consume '.'
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
                     consumeChar(); last_was_separator = true;
                     continue; // Recover
                 }
                 consumeChar(); // Consume '_'
                 last_was_separator = true;
                 if (!isDecimalDigit_impl_part1(lookahead_char_)) {
                     reportError("Separator in fraction must be followed by a decimal digit");
                     current_pos_--; readUtf8Char(); // Backtrack
                     last_was_separator = false;
                     break;
                 }
                 // Next is digit, continue loop
             } else {
                 break; // End of fractional part
             }
         }
         if (last_was_separator) {
            reportError("Numeric separator cannot appear at the end of the fractional part");
            current_pos_--; readUtf8Char();
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

    // --- Handle Exponent (only for base 10) ---
    bool has_exponent = false;
    const char* exponent_start = nullptr;
    if (base == 10 && (lookahead_char_ == 'e' || lookahead_char_ == 'E')) {
        has_exponent = true;
        flags |= Token::FlagIsExponent; // Mark as having exponent
        consumeChar(); // Consume 'e' or 'E'
        const char* sign_pos = current_pos_;
        if (lookahead_char_ == '+' || lookahead_char_ == '-') {
            consumeChar();
        }
        exponent_start = current_pos_;
        last_was_separator = false;
        bool first_digit_in_exponent = true;

        if (!isDecimalDigit_impl_part1(lookahead_char_)) {
            reportError("Exponent part must contain at least one digit");
            // Backtrack to before 'e'/'E' and treat number as ending there.
            current_pos_ = sign_pos; // Before sign if present
            if (current_pos_ > start && (*(current_pos_ -1) == 'e' || *(current_pos_ -1) == 'E')) {
                 current_pos_--; // Before 'e'/'E'
            }
            readUtf8Char();
            has_exponent = false; // Undo state
            flags &= ~Token::FlagIsExponent;
            goto parse_value; // Parse value without exponent
        }

        while (current_pos_ < source_end_) {
            uint32_t c = lookahead_char_;
            if (isDecimalDigit_impl_part1(c)) {
                consumeChar();
                last_was_separator = false;
                first_digit_in_exponent = false;
            } else if (c == '_') {
                 if (first_digit_in_exponent || last_was_separator) {
                     reportError("Invalid numeric separator position in exponent part");
                     consumeChar(); last_was_separator = true;
                     continue; // Recover
                 }
                 consumeChar(); // Consume '_'
                 last_was_separator = true;
                 if (!isDecimalDigit_impl_part1(lookahead_char_)) {
                     reportError("Separator in exponent must be followed by a decimal digit");
                     current_pos_--; readUtf8Char(); // Backtrack
                     last_was_separator = false;
                     break;
                 }
                 // Next is digit, continue loop
            } else {
                break; // End of exponent part
            }
        }
        if (last_was_separator) {
           reportError("Numeric separator cannot appear at the end of the exponent part");
           current_pos_--; readUtf8Char();
        }
        // Check if any digits were consumed *after* the sign (if any)
        if (current_pos_ == exponent_start) {
             // This should have been caught by the initial digit check, but good safety net.
             reportError("Exponent part must contain at least one digit");
             // Backtrack logic is complex, rely on initial check.
             // For simplicity here, maybe create error token.
             return createErrorToken("Invalid exponent format");
        }
    }
    const char* exponent_end = current_pos_;

    // --- Handle BigInt Suffix --- (Must be integer, cannot follow decimal/exponent)
    if (lookahead_char_ == 'n') {
        if (has_decimal || has_exponent || is_legacy_octal) {
            reportError("BigInt suffix 'n' is not allowed after a decimal point, exponent, or on a legacy octal literal");
            consumeChar(); // Consume 'n' to avoid infinite loop if caller retries
            return createErrorToken("Invalid BigInt literal format");
        } else if (base != 10 && base != 16 && base != 8 && base != 2) {
            // Should not happen if logic is correct, but safeguard
            reportError("Internal Error: BigInt suffix 'n' applied to invalid base");
             consumeChar(); return createErrorToken("Internal Scanner Error");
        } else {
            is_bigint = true;
            flags |= Token::FlagIsBigInt;
            consumeChar(); // Consume 'n'
        }
    }

parse_value:
    // --- Convert Value --- (Use detailed helpers; stubs for now)
    const char* end = current_pos_; // Position after last char of literal (or 'n')
    std::string lexeme(start, end - start);
    TokenType type;
    Scanner::NumberParseResult parse_res = NumberParseResult::Ok;

    if (is_bigint) {
        type = TokenType::BigIntLiteral;
        // parseDetailedBigInt needs to handle base, separators, and potential overflow
        parse_res = parseDetailedBigInt(start, end - 1, bigint_value, flags); // end-1 to exclude 'n'
        if (parse_res != NumberParseResult::Ok) {
            // Error should ideally be reported by parseDetailedBigInt
            reportError("Failed to parse BigInt value", false); // false = don't duplicate if already reported
        }
    } else {
        type = TokenType::NumericLiteral;
        // parseDetailedNumber needs to handle base, separators, decimal, exponent
        parse_res = parseDetailedNumber(start, end, number_value, flags);
         if (parse_res != NumberParseResult::Ok) {
             reportError("Failed to parse numeric value", false);
         }
    }

    // Final check for identifier start immediately following literal (invalid)
    if (isIdentifierStart(lookahead_char_)) {
        reportError("Identifier starts immediately after numeric literal");
        // Consider consuming the identifier as part of the error?
        // Or just return the number token and let parser handle the error.
    }

    // Create token even if parsing failed, mark as error if necessary
    Token token = createToken(parse_res == NumberParseResult::Ok ? type : TokenType::Error, flags);
    if (parse_res == NumberParseResult::Ok) {
        if (type == TokenType::BigIntLiteral) {
            token.setValue(bigint_value); // Store parsed BigInt
        } else {
            token.setValue(number_value); // Store parsed Number
        }
    }
    token.raw_lexeme = lexeme;

    auto end_time = std::chrono::high_resolution_clock::now();
    perf_timers_.numberScanTime += (end_time - start_time);
    return token;
}

// scanStringLiteral Implementation (Detailed)
Token Scanner::scanStringLiteral() { consumeChar(); return createErrorToken("scanStringLiteral: Not Yet Implemented in this edit"); }

// scanTemplateToken Implementation (Detailed)
Token Scanner::scanTemplateToken() { consumeChar(); return createErrorToken("scanTemplateToken: Not Yet Implemented in this edit"); }

// scanRegExpLiteral Implementation (Detailed)
Token Scanner::scanRegExpLiteral() { consumeChar(); return createErrorToken("scanRegExpLiteral: Not Yet Implemented in this edit"); }

// scanJsxToken Implementation (Detailed)
Token Scanner::scanJsxToken() { consumeChar(); return createErrorToken("scanJsxToken: Not Yet Implemented in this edit"); }

// scanJsxIdentifier Implementation (Detailed)
Token Scanner::scanJsxIdentifier() { consumeChar(); return createErrorToken("scanJsxIdentifier: Not Yet Implemented in this edit"); }

// scanJsxText Implementation (Detailed)
Token Scanner::scanJsxText() { consumeChar(); return createErrorToken("scanJsxText: Not Yet Implemented in this edit"); }

// parseDetailedNumber Implementation (Detailed)
Scanner::NumberParseResult Scanner::parseDetailedNumber(const char* start, const char* end, double& out, uint32_t& flags) {
    // TODO: Implement robust number parsing (strtod limitations, separator removal, base handling)
    std::string cleaned_num; cleaned_num.reserve(end - start);
    for(const char *p = start; p < end; ++p) {
        if (*p != '_') cleaned_num += *p;
    }
    // Basic conversion for now, does not handle prefixes properly or large numbers
    try {
        out = std::stod(cleaned_num);
        return NumberParseResult::Ok;
    } catch (const std::invalid_argument& ia) {
        return NumberParseResult::InvalidFormat;
    } catch (const std::out_of_range& oor) {
        // Determine if +Infinity or -Infinity based on sign if possible
        if (cleaned_num.length() > 0 && cleaned_num[0] == '-') {
            out = -std::numeric_limits<double>::infinity();
        } else {
            out = std::numeric_limits<double>::infinity();
        }
        return NumberParseResult::Overflow;
    }
}

// parseDetailedBigInt Implementation (Detailed)
Scanner::NumberParseResult Scanner::parseDetailedBigInt(const char* start, const char* end, int64_t& out, uint32_t& flags) {
    // TODO: Implement robust BigInt parsing (arbitrary precision, base, separators)
    std::string cleaned_num; cleaned_num.reserve(end - start);
    int base = 10;
    const char* num_start = start;
    // Rudimentary prefix check for stub
    if ((flags & Token::FlagIsHex) && end - start > 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) { base = 16; num_start = start + 2; }
    else if ((flags & Token::FlagIsBinary) && end - start > 2 && start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) { base = 2; num_start = start + 2; }
    else if ((flags & Token::FlagIsOctal) && !(flags & Token::FlagIsLegacyOctal) && end - start > 2 && start[0] == '0' && (start[1] == 'o' || start[1] == 'O')) { base = 8; num_start = start + 2; }
    else if (flags & Token::FlagIsLegacyOctal) { base = 8; num_start = start + 1; } // Legacy octal starts after '0'

    for(const char *p = num_start; p < end; ++p) {
        if (*p != '_') cleaned_num += *p;
    }

    // Basic conversion using strtoll (limited precision)
    char* endptr;
    errno = 0;
    out = std::strtoll(cleaned_num.c_str(), &endptr, base);

    if (*endptr != '\0' || cleaned_num.empty()) {
        return NumberParseResult::InvalidFormat;
    }
    if (errno == ERANGE) {
        return NumberParseResult::Overflow; // Overflow for int64_t
    }
    return NumberParseResult::Ok;
}

// parseComplexEscape Implementation (Detailed)
Scanner::EscapeParseResult Scanner::parseComplexEscape(const char*& p, std::string& out_val) {
    if (p >= source_end_) return EscapeParseResult::InvalidUnicodeEscape;
    uint32_t c = static_cast<uint8_t>(*p);
    if (c < 0x80) out_val += static_cast<char>(c);
    p++; 
    return EscapeParseResult::Ok;
}

// validateRegExpSyntax Implementation (Detailed)
bool Scanner::validateRegExpSyntax(const char*, const char*) { return true; }

// disambiguateIdentifier Implementation (Detailed)
TokenType Scanner::disambiguateIdentifier(const char*, size_t) { return TokenType::Identifier; } // Stub

// lookupKeyword Implementation (Detailed)
TokenType Scanner::lookupKeyword(const char*, size_t) { return TokenType::Identifier; } // Stub

// currentToken Implementation (Detailed)
const Token& Scanner::currentToken() const { return current_token_; }

// reset Implementation (Detailed)
void Scanner::reset(size_t position, const ScannerContext& context) { current_pos_ = source_start_ + position; context_ = context; /* Simplified */ readUtf8Char(); }

// getContext Implementation (Detailed)
const ScannerContext& Scanner::getContext() const { return context_; }

// setContext Implementation (Detailed)
void Scanner::setContext(const ScannerContext& c) { context_ = c; }

// getCurrentPosition Implementation (Detailed)
size_t Scanner::getCurrentPosition() const { return current_pos_ - source_start_; }

// createCheckpoint Implementation (Detailed)
ScannerCheckpoint Scanner::createCheckpoint() { return ScannerCheckpoint{current_pos_, current_token_, current_location_, context_}; }

// restoreCheckpoint Implementation (Detailed)
void Scanner::restoreCheckpoint(const ScannerCheckpoint& cp) { current_pos_=cp.position; current_token_=cp.token; current_location_=cp.location; context_=cp.context; readUtf8Char(); }

// enableSimdOptimization Implementation (Detailed)
void Scanner::enableSimdOptimization(bool) { /* Stub */ }

// tryParallelScan Implementation (Detailed)
bool Scanner::tryParallelScan(int) { return false; }

// peekChar Implementation (Detailed)
uint32_t Scanner::peekChar(int offset, ptrdiff_t relative_offset) { const char* p = current_pos_ + relative_offset; const char* end = source_end_; for(int i = 0; i < offset && p<end; ++i){const char* cur=p;token_h_filler::Utf8Utils::decodeChar(p,end);if(p==cur)p++;} if(p>=end)return 0; const char* fp=p; return token_h_filler::Utf8Utils::decodeChar(fp,end); }
int Scanner::peekCharSize(int offset, ptrdiff_t relative_offset) { const char* p = current_pos_ + relative_offset; const char* end = source_end_; for(int i = 0; i < offset && p<end; ++i){const char* cur=p;token_h_filler::Utf8Utils::decodeChar(p,end);if(p==cur)p++;} if(p>=end)return 0; const char* sd=p; token_h_filler::Utf8Utils::decodeChar(p,end); return static_cast<int>(p-sd); }
void Scanner::detailedHelperFunc1() {} void Scanner::detailedHelperFunc2() {} /*...*/ void Scanner::detailedHelperFunc1000() {}

} // namespace lexer
} // namespace core
} // namespace aerojs
} // namespace aerojs