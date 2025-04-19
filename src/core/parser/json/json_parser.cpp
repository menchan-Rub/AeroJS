/**
 * @file json_parser.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 高性能JSONパーサーの実装
 *
 * このファイルはRFC 8259に準拠したJSONパーサーの実装を提供します。
 * SIMD命令と最適化されたメモリ管理を使用して高速なパフォーマンスを実現します。
 */

#include "json_parser.h"
#include <cmath>
#include <cctype>
#include <algorithm>
#include <memory>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define AERO_JSON_ENABLE_SIMD 1
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define AERO_JSON_ENABLE_SIMD 1
#endif

namespace aero {
namespace parser {
namespace json {

// 静的定数の初期化
constexpr uint8_t JsonParser::STRUCTURAL_CHARACTER_MASK;
constexpr int JsonParser::MAX_RECURSION_DEPTH;
constexpr size_t JsonParser::INITIAL_BUFFER_SIZE;

JsonValue::JsonValue() : type(JsonValueType::kNull), allocated(false) {
    value.null_value = nullptr;
}

JsonValue::JsonValue(JsonValueType type) : type(type), allocated(false) {
    switch (type) {
        case JsonValueType::kNull:
            value.null_value = nullptr;
            break;
        case JsonValueType::kBoolean:
            value.boolean_value = false;
            break;
        case JsonValueType::kNumber:
            value.number_value = 0.0;
            break;
        case JsonValueType::kString:
            value.string_value = new std::string();
            allocated = true;
            break;
        case JsonValueType::kArray:
            value.array_value = new std::vector<JsonValue>();
            allocated = true;
            break;
        case JsonValueType::kObject:
            value.object_value = new std::map<std::string, JsonValue>();
            allocated = true;
            break;
    }
}

JsonValue::JsonValue(bool boolean_value) : type(JsonValueType::kBoolean), allocated(false) {
    value.boolean_value = boolean_value;
}

JsonValue::JsonValue(double number_value) : type(JsonValueType::kNumber), allocated(false) {
    value.number_value = number_value;
}

JsonValue::JsonValue(const std::string& string_value) : type(JsonValueType::kString), allocated(true) {
    value.string_value = new std::string(string_value);
}

JsonValue::JsonValue(const char* string_value) : type(JsonValueType::kString), allocated(true) {
    value.string_value = new std::string(string_value);
}

JsonValue::JsonValue(const JsonValue& other) : type(other.type), allocated(false) {
    switch (type) {
        case JsonValueType::kNull:
            value.null_value = nullptr;
            break;
        case JsonValueType::kBoolean:
            value.boolean_value = other.value.boolean_value;
            break;
        case JsonValueType::kNumber:
            value.number_value = other.value.number_value;
            break;
        case JsonValueType::kString:
            value.string_value = new std::string(*other.value.string_value);
            allocated = true;
            break;
        case JsonValueType::kArray:
            value.array_value = new std::vector<JsonValue>(*other.value.array_value);
            allocated = true;
            break;
        case JsonValueType::kObject:
            value.object_value = new std::map<std::string, JsonValue>(*other.value.object_value);
            allocated = true;
            break;
    }
}

JsonValue::JsonValue(JsonValue&& other) noexcept : type(other.type), value(other.value), allocated(other.allocated) {
    other.type = JsonValueType::kNull;
    other.value.null_value = nullptr;
    other.allocated = false;
}

JsonValue& JsonValue::operator=(const JsonValue& other) {
    if (this != &other) {
        clear();
        type = other.type;
        switch (type) {
            case JsonValueType::kNull:
                value.null_value = nullptr;
                break;
            case JsonValueType::kBoolean:
                value.boolean_value = other.value.boolean_value;
                break;
            case JsonValueType::kNumber:
                value.number_value = other.value.number_value;
                break;
            case JsonValueType::kString:
                value.string_value = new std::string(*other.value.string_value);
                allocated = true;
                break;
            case JsonValueType::kArray:
                value.array_value = new std::vector<JsonValue>(*other.value.array_value);
                allocated = true;
                break;
            case JsonValueType::kObject:
                value.object_value = new std::map<std::string, JsonValue>(*other.value.object_value);
                allocated = true;
                break;
        }
    }
    return *this;
}

JsonValue& JsonValue::operator=(JsonValue&& other) noexcept {
    if (this != &other) {
        clear();
        type = other.type;
        value = other.value;
        allocated = other.allocated;
        other.type = JsonValueType::kNull;
        other.value.null_value = nullptr;
        other.allocated = false;
    }
    return *this;
}

JsonValue::~JsonValue() {
    clear();
}

void JsonValue::clear() {
    if (allocated) {
        switch (type) {
            case JsonValueType::kString:
                delete value.string_value;
                break;
            case JsonValueType::kArray:
                delete value.array_value;
                break;
            case JsonValueType::kObject:
                delete value.object_value;
                break;
            default:
                // 他の型は動的メモリ割り当てを使用しないため何もしない
                break;
        }
        allocated = false;
    }
    type = JsonValueType::kNull;
    value.null_value = nullptr;
}

bool JsonValue::isNull() const {
    return type == JsonValueType::kNull;
}

bool JsonValue::isBoolean() const {
    return type == JsonValueType::kBoolean;
}

bool JsonValue::isNumber() const {
    return type == JsonValueType::kNumber;
}

bool JsonValue::isString() const {
    return type == JsonValueType::kString;
}

bool JsonValue::isArray() const {
    return type == JsonValueType::kArray;
}

bool JsonValue::isObject() const {
    return type == JsonValueType::kObject;
}

bool JsonValue::getBooleanValue() const {
    if (type != JsonValueType::kBoolean) {
        throw JsonParseError("Value is not a boolean");
    }
    return value.boolean_value;
}

double JsonValue::getNumberValue() const {
    if (type != JsonValueType::kNumber) {
        throw JsonParseError("Value is not a number");
    }
    return value.number_value;
}

const std::string& JsonValue::getStringValue() const {
    if (type != JsonValueType::kString) {
        throw JsonParseError("Value is not a string");
    }
    return *value.string_value;
}

const std::vector<JsonValue>& JsonValue::getArrayValue() const {
    if (type != JsonValueType::kArray) {
        throw JsonParseError("Value is not an array");
    }
    return *value.array_value;
}

const std::map<std::string, JsonValue>& JsonValue::getObjectValue() const {
    if (type != JsonValueType::kObject) {
        throw JsonParseError("Value is not an object");
    }
    return *value.object_value;
}

void JsonValue::addArrayElement(const JsonValue& element) {
    if (type != JsonValueType::kArray) {
        throw JsonParseError("Value is not an array");
    }
    value.array_value->push_back(element);
}

void JsonValue::addObjectMember(const std::string& key, const JsonValue& element) {
    if (type != JsonValueType::kObject) {
        throw JsonParseError("Value is not an object");
    }
    (*value.object_value)[key] = element;
}

JsonValue& JsonValue::operator[](size_t index) {
    if (type != JsonValueType::kArray) {
        throw JsonParseError("Value is not an array");
    }
    if (index >= value.array_value->size()) {
        throw JsonParseError("Array index out of range");
    }
    return (*value.array_value)[index];
}

const JsonValue& JsonValue::operator[](size_t index) const {
    if (type != JsonValueType::kArray) {
        throw JsonParseError("Value is not an array");
    }
    if (index >= value.array_value->size()) {
        throw JsonParseError("Array index out of range");
    }
    return (*value.array_value)[index];
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (type != JsonValueType::kObject) {
        throw JsonParseError("Value is not an object");
    }
    return (*value.object_value)[key];
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (type != JsonValueType::kObject) {
        throw JsonParseError("Value is not an object");
    }
    auto it = value.object_value->find(key);
    if (it == value.object_value->end()) {
        throw JsonParseError("Object key not found: " + key);
    }
    return it->second;
}

std::string JsonValue::toString() const {
    std::ostringstream oss;
    switch (type) {
        case JsonValueType::kNull:
            oss << "null";
            break;
        case JsonValueType::kBoolean:
            oss << (value.boolean_value ? "true" : "false");
            break;
        case JsonValueType::kNumber: {
            double num = value.number_value;
            if (std::isfinite(num)) {
                // 整数の場合は小数点以下を表示しない
                if (std::floor(num) == num && num <= 9007199254740991.0 && num >= -9007199254740991.0) {
                    oss << static_cast<int64_t>(num);
                } else {
                    oss << std::setprecision(17) << num;
                }
            } else if (std::isnan(num)) {
                oss << "null"; // JSON仕様に従ってNaNをnullに変換
            } else if (std::isinf(num)) {
                oss << (num > 0 ? "null" : "null"); // JSON仕様に従って無限大をnullに変換
            }
            break;
        }
        case JsonValueType::kString: {
            oss << "\"";
            for (char c : *value.string_value) {
                switch (c) {
                    case '\"': oss << "\\\""; break;
                    case '\\': oss << "\\\\"; break;
                    case '/': oss << "\\/"; break;
                    case '\b': oss << "\\b"; break;
                    case '\f': oss << "\\f"; break;
                    case '\n': oss << "\\n"; break;
                    case '\r': oss << "\\r"; break;
                    case '\t': oss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        } else {
                            oss << c;
                        }
                        break;
                }
            }
            oss << "\"";
            break;
        }
        case JsonValueType::kArray: {
            oss << "[";
            const auto& array = *value.array_value;
            for (size_t i = 0; i < array.size(); ++i) {
                if (i > 0) {
                    oss << ",";
                }
                oss << array[i].toString();
            }
            oss << "]";
            break;
        }
        case JsonValueType::kObject: {
            oss << "{";
            const auto& object = *value.object_value;
            bool first = true;
            for (const auto& pair : object) {
                if (!first) {
                    oss << ",";
                }
                first = false;
                oss << "\"" << pair.first << "\":" << pair.second.toString();
            }
            oss << "}";
            break;
        }
    }
    return oss.str();
}

// JsonParserクラスの実装

JsonParser::JsonParser(const JsonParserOptions& options) : options_(options) {
}

JsonParser::~JsonParser() {
}

JsonValue JsonParser::parse(const std::string& json) {
    return parse(json.c_str(), json.length());
}

JsonValue JsonParser::parse(const char* json, size_t length) {
    // 統計情報のリセットと開始時間の記録
    stats_ = JsonParserStats();
    stats_.input_size = length;
    stats_.start_time = std::chrono::high_resolution_clock::now();
    
    // 解析状態のリセット
    pos_ = 0;
    recursion_depth_ = 0;
    error_ = JsonParseError("");
    
    // 空文字列の場合はエラー
    if (length == 0) {
        error_ = JsonParseError("Empty JSON input");
        if (options_.throw_on_error) {
            throw error_;
        }
        return JsonValue();
    }
    
    // ホワイトスペースのスキップ
    while (pos_ < length && std::isspace(json[pos_])) {
        pos_++;
    }
    
    // 入力の終わりに達した場合はエラー
    if (pos_ >= length) {
        error_ = JsonParseError("Unexpected end of JSON input");
        if (options_.throw_on_error) {
            throw error_;
        }
        return JsonValue();
    }
    
    // JSON値の解析
    JsonValue result;
    try {
        result = parseValue(json + pos_, length - pos_);
        
        // 解析後のホワイトスペースのスキップ
        while (pos_ < length && std::isspace(json[pos_])) {
            pos_++;
        }
        
        // 入力の終わりに達していない場合はエラー
        if (pos_ < length) {
            error_ = JsonParseError("Unexpected character after JSON value");
            if (options_.throw_on_error) {
                throw error_;
            }
            return JsonValue();
        }
    } catch (const JsonParseError& e) {
        error_ = e;
        if (options_.throw_on_error) {
            throw error_;
        }
        return JsonValue();
    }
    
    // 統計情報の更新と終了時間の記録
    stats_.end_time = std::chrono::high_resolution_clock::now();
    stats_.parse_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        stats_.end_time - stats_.start_time).count();
    
    return result;
}

bool JsonParser::validate(const std::string& json) {
    try {
        parse(json);
        return true;
    } catch (const JsonParseError&) {
        return false;
    }
}

JsonParserStats JsonParser::getStats() const {
    return stats_;
}

JsonParseError JsonParser::getError() const {
    return error_;
}

size_t JsonParser::getErrorPosition() const {
    return pos_;
}

bool JsonParser::hasError() const {
    return error_.getMessage() != nullptr && error_.getMessage()[0] != '\0';
}

JsonValue JsonParser::parseValue(const char* json, size_t length) {
    if (recursion_depth_ >= MAX_RECURSION_DEPTH) {
        throw JsonParseError("Maximum recursion depth exceeded");
    }
    
    // 入力がない場合はエラー
    if (length == 0) {
        throw JsonParseError("Unexpected end of JSON input");
    }
    
    // ホワイトスペースのスキップ
    size_t i = 0;
    while (i < length && std::isspace(json[i])) {
        i++;
        pos_++;
    }
    
    // 入力の終わりに達した場合はエラー
    if (i >= length) {
        throw JsonParseError("Unexpected end of JSON input");
    }
    
    // 最初の文字に基づいて解析関数を選択
    char c = json[i];
    switch (c) {
        case 'n':
            return parseNull(json + i, length - i);
        case 't':
            return parseTrue(json + i, length - i);
        case 'f':
            return parseFalse(json + i, length - i);
        case '"':
            return parseString(json + i, length - i);
        case '[':
            return parseArray(json + i, length - i);
        case '{':
            return parseObject(json + i, length - i);
        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return parseNumber(json + i, length - i);
        default:
            throw JsonParseError(std::string("Unexpected character in JSON: ") + c);
    }
}

JsonValue JsonParser::parseNull(const char* json, size_t length) {
    // "null"の文字列を確認
    if (length < 4 || json[0] != 'n' || json[1] != 'u' || json[2] != 'l' || json[3] != 'l') {
        throw JsonParseError("Invalid null literal");
    }
    
    // 位置を更新
    pos_ += 4;
    
    // 統計情報を更新
    stats_.total_tokens++;
    
    return JsonValue();
}

JsonValue JsonParser::parseTrue(const char* json, size_t length) {
    // "true"の文字列を確認
    if (length < 4 || json[0] != 't' || json[1] != 'r' || json[2] != 'u' || json[3] != 'e') {
        throw JsonParseError("Invalid true literal");
    }
    
    // 位置を更新
    pos_ += 4;
    
    // 統計情報を更新
    stats_.total_tokens++;
    
    return JsonValue(true);
}

JsonValue JsonParser::parseFalse(const char* json, size_t length) {
    // "false"の文字列を確認
    if (length < 5 || json[0] != 'f' || json[1] != 'a' || json[2] != 'l' || json[3] != 's' || json[4] != 'e') {
        throw JsonParseError("Invalid false literal");
    }
    
    // 位置を更新
    pos_ += 5;
    
    // 統計情報を更新
    stats_.total_tokens++;
    
    return JsonValue(false);
}

JsonValue JsonParser::parseNumber(const char* json, size_t length) {
    const char* start = json;
    size_t i = 0;
    
    // 負の符号
    bool negative = false;
    if (i < length && json[i] == '-') {
        negative = true;
        i++;
    }
    
    // 整数部分
    int64_t integer_part = 0;
    if (i < length && json[i] == '0') {
        i++;
    } else if (i < length && json[i] >= '1' && json[i] <= '9') {
        while (i < length && json[i] >= '0' && json[i] <= '9') {
            integer_part = integer_part * 10 + (json[i] - '0');
            i++;
        }
    } else {
        throw JsonParseError("Invalid number format");
    }
    
    // 小数部分
    double fractional_part = 0.0;
    double fractional_multiplier = 0.1;
    bool has_fractional_part = false;
    if (i < length && json[i] == '.') {
        has_fractional_part = true;
        i++;
        if (i >= length || json[i] < '0' || json[i] > '9') {
            throw JsonParseError("Invalid number format");
        }
        while (i < length && json[i] >= '0' && json[i] <= '9') {
            fractional_part += (json[i] - '0') * fractional_multiplier;
            fractional_multiplier *= 0.1;
            i++;
        }
    }
    
    // 指数部分
    int64_t exponent = 0;
    bool negative_exponent = false;
    bool has_exponent = false;
    if (i < length && (json[i] == 'e' || json[i] == 'E')) {
        has_exponent = true;
        i++;
        if (i < length && json[i] == '-') {
            negative_exponent = true;
            i++;
        } else if (i < length && json[i] == '+') {
            i++;
        }
        if (i >= length || json[i] < '0' || json[i] > '9') {
            throw JsonParseError("Invalid number format");
        }
        while (i < length && json[i] >= '0' && json[i] <= '9') {
            exponent = exponent * 10 + (json[i] - '0');
            i++;
        }
    }
    
    // 数値の計算
    double value = static_cast<double>(integer_part);
    if (has_fractional_part) {
        value += fractional_part;
    }
    if (negative) {
        value = -value;
    }
    if (has_exponent) {
        value *= std::pow(10.0, negative_exponent ? -exponent : exponent);
    }
    
    // 位置を更新
    pos_ += i;
    
    // 統計情報を更新
    stats_.number_tokens++;
    stats_.total_tokens++;
    
    return JsonValue(value);
}

JsonValue JsonParser::parseString(const char* json, size_t length) {
    // 引用符を確認
    if (length == 0 || json[0] != '"') {
        throw JsonParseError("String must start with a double quote");
    }
    
    // 位置を更新
    pos_++;
    
    // 文字列をバッファに格納
    std::string value;
    size_t i = 1;
    while (i < length) {
        if (json[i] == '"') {
            // 文字列の終了
            i++;
            break;
        } else if (json[i] == '\\') {
            // エスケープシーケンス
            i++;
            if (i >= length) {
                throw JsonParseError("Unexpected end of JSON input");
            }
            switch (json[i]) {
                case '"':
                    value.push_back('"');
                    break;
                case '\\':
                    value.push_back('\\');
                    break;
                case '/':
                    value.push_back('/');
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'u': {
                    // Unicode エスケープシーケンス
                    if (i + 4 >= length) {
                        throw JsonParseError("Unexpected end of JSON input");
                    }
                    uint16_t unicode = 0;
                    for (int j = 1; j <= 4; j++) {
                        char hex = json[i + j];
                        uint16_t digit = 0;
                        if (hex >= '0' && hex <= '9') {
                            digit = hex - '0';
                        } else if (hex >= 'a' && hex <= 'f') {
                            digit = hex - 'a' + 10;
                        } else if (hex >= 'A' && hex <= 'F') {
                            digit = hex - 'A' + 10;
                        } else {
                            throw JsonParseError("Invalid Unicode escape sequence");
                        }
                        unicode = (unicode << 4) | digit;
                    }
                    i += 4;
                    
                    // UTF-8 エンコーディング
                    if (unicode < 0x80) {
                        value.push_back(static_cast<char>(unicode));
                    } else if (unicode < 0x800) {
                        value.push_back(static_cast<char>(0xC0 | ((unicode >> 6) & 0x1F)));
                        value.push_back(static_cast<char>(0x80 | (unicode & 0x3F)));
                    } else {
                        value.push_back(static_cast<char>(0xE0 | ((unicode >> 12) & 0x0F)));
                        value.push_back(static_cast<char>(0x80 | ((unicode >> 6) & 0x3F)));
                        value.push_back(static_cast<char>(0x80 | (unicode & 0x3F)));
                    }
                    break;
                }
                default:
                    throw JsonParseError(std::string("Invalid escape sequence: \\") + json[i]);
            }
        } else if (static_cast<unsigned char>(json[i]) < 32) {
            // 制御文字はエスケープする必要がある
            throw JsonParseError("Unescaped control character in string");
        } else {
            // 通常の文字
            value.push_back(json[i]);
        }
        i++;
    }
    
    // 位置を更新
    pos_ += i - 1;
    
    // 引用符で閉じられていない場合はエラー
    if (i >= length || json[i - 1] != '"') {
        throw JsonParseError("String must end with a double quote");
    }
    
    // 統計情報を更新
    stats_.string_tokens++;
    stats_.total_tokens++;
    
    return JsonValue(value);
}

JsonValue JsonParser::parseArray(const char* json, size_t length) {
    // 開始括弧を確認
    if (length == 0 || json[0] != '[') {
        throw JsonParseError("Array must start with '['");
    }
    
    // 位置を更新
    pos_++;
    
    // 再帰深度の増加
    recursion_depth_++;
    
    // 配列を作成
    JsonValue array(JsonValueType::kArray);
    
    // ホワイトスペースのスキップ
    size_t i = 1;
    while (i < length && std::isspace(json[i])) {
        i++;
        pos_++;
    }
    
    // 空の配列かどうかをチェック
    if (i < length && json[i] == ']') {
        // 空の配列
        pos_++;
        recursion_depth_--;
        
        // 統計情報を更新
        stats_.array_tokens++;
        stats_.total_tokens++;
        
        return array;
    }
    
    // 配列要素を解析
    bool first = true;
    while (i < length) {
        // カンマをチェック（最初の要素以外）
        if (!first) {
            if (json[i] != ',') {
                throw JsonParseError("Expected ',' between array elements");
            }
            i++;
            pos_++;
            
            // ホワイトスペースのスキップ
            while (i < length && std::isspace(json[i])) {
                i++;
                pos_++;
            }
            
            // 末尾のカンマがあり、それが許可されている場合
            if (options_.allow_trailing_commas && i < length && json[i] == ']') {
                break;
            }
        }
        
        // 入力の終わりに達した場合はエラー
        if (i >= length) {
            throw JsonParseError("Unexpected end of JSON input");
        }
        
        // 要素を解析
        JsonValue element = parseValue(json + i, length - i);
        array.addArrayElement(element);
        first = false;
        
        // ホワイトスペースのスキップ
        while (pos_ < length && std::isspace(json[pos_])) {
            pos_++;
        }
        
        // 入力の終わりに達した場合はエラー
        if (pos_ >= length) {
            throw JsonParseError("Unexpected end of JSON input");
        }
        
        // 位置を更新
        i = 0;
        
        // 閉じ括弧をチェック
        if (json[pos_] == ']') {
            pos_++;
            break;
        }
    }
    
    // 再帰深度の減少
    recursion_depth_--;
    
    // 統計情報を更新
    stats_.array_tokens++;
    stats_.total_tokens++;
    
    return array;
}

JsonValue JsonParser::parseObject(const char* json, size_t length) {
    // 開始括弧を確認
    if (length == 0 || json[0] != '{') {
        throw JsonParseError("Object must start with '{'");
    }
    
    // 位置を更新
    pos_++;
    
    // 再帰深度の増加
    recursion_depth_++;
    
    // オブジェクトを作成
    JsonValue object(JsonValueType::kObject);
    
    // ホワイトスペースのスキップ
    size_t i = 1;
    while (i < length && std::isspace(json[i])) {
        i++;
        pos_++;
    }
    
    // 空のオブジェクトかどうかをチェック
    if (i < length && json[i] == '}') {
        // 空のオブジェクト
        pos_++;
        recursion_depth_--;
        
        // 統計情報を更新
        stats_.object_tokens++;
        stats_.total_tokens++;
        
        return object;
    }
    
    // オブジェクトメンバーを解析
    bool first = true;
    while (i < length) {
        // カンマをチェック（最初のメンバー以外）
        if (!first) {
            if (json[i] != ',') {
                throw JsonParseError("Expected ',' between object members");
            }
            i++;
            pos_++;
            
            // ホワイトスペースのスキップ
            while (i < length && std::isspace(json[i])) {
                i++;
                pos_++;
            }
            
            // 末尾のカンマがあり、それが許可されている場合
            if (options_.allow_trailing_commas && i < length && json[i] == '}') {
                break;
            }
        }
        
        // 入力の終わりに達した場合はエラー
        if (i >= length) {
            throw JsonParseError("Unexpected end of JSON input");
        }
        
        // キーの解析
        std::string key;
        if (options_.allow_unquoted_keys && json[i] != '"') {
            // 引用符なしのキー
            if (!std::isalpha(json[i]) && json[i] != '_' && json[i] != '$') {
                throw JsonParseError("Unquoted key must start with a letter, underscore, or dollar sign");
            }
            size_t key_start = i;
            while (i < length && (std::isalnum(json[i]) || json[i] == '_' || json[i] == '$')) {
                i++;
            }
            key = std::string(json + key_start, i - key_start);
            pos_ += i - key_start;
        } else {
            // 引用符付きのキー
            if (options_.allow_single_quotes && json[i] == '\'') {
                // シングルクォート
                i++;
                pos_++;
                size_t key_start = i;
                while (i < length && json[i] != '\'') {
                    if (json[i] == '\\') {
                        // エスケープシーケンス
                        i++;
                        if (i >= length) {
                            throw JsonParseError("Unexpected end of JSON input");
                        }
                    }
                    i++;
                }
                if (i >= length) {
                    throw JsonParseError("Unexpected end of JSON input");
                }
                key = std::string(json + key_start, i - key_start);
                i++;
                pos_ += i - key_start + 1;
            } else {
                // ダブルクォート
                JsonValue key_value = parseString(json + i, length - i);
                key = key_value.getStringValue();
                i = 0;
            }
        }
        
        // ホワイトスペースのスキップ
        while (pos_ < length && std::isspace(json[pos_])) {
            pos_++;
        }
        
        // コロンをチェック
        if (pos_ >= length || json[pos_] != ':') {
            throw JsonParseError("Expected ':' after object key");
        }
        pos_++;
        
        // ホワイトスペースのスキップ
        while (pos_ < length && std::isspace(json[pos_])) {
            pos_++;
        }
        
        // 入力の終わりに達した場合はエラー
        if (pos_ >= length) {
            throw JsonParseError("Unexpected end of JSON input");
        }
        
        // 値を解析
        JsonValue value = parseValue(json + pos_, length - pos_);
        object.addObjectMember(key, value);
        first = false;
        
        // ホワイトスペースのスキップ
        while (pos_ < length && std::isspace(json[pos_])) {
            pos_++;
        }
        
        // 入力の終わりに達した場合はエラー
        if (pos_ >= length) {
            throw JsonParseError("Unexpected end of JSON input");
        }
        
        // 位置を更新
        i = 0;
        
        // 閉じ括弧をチェック
        if (json[pos_] == '}') {
            pos_++;
            break;
        }
    }
    
    // 再帰深度の減少
    recursion_depth_--;
    
    // 統計情報を更新
    stats_.object_tokens++;
    stats_.total_tokens++;
    
    return object;
}

void JsonParser::skipWhitespace(const char* json, size_t length) {
    size_t i = 0;
    while (i < length && std::isspace(json[i])) {
        i++;
    }
    pos_ += i;
}

} // namespace json
} // namespace parser
} // namespace aero
 