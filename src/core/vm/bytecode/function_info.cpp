/**
 * @file function_info.cpp
 * @brief 関数情報の実装ファイル
 * 
 * このファイルは、バイトコードモジュール内の関数に関する情報を保持するFunctionInfoクラスを実装します。
 * 関数名、引数の数、関数本体のバイトコード位置などの操作メソッドを提供します。
 */

#include "function_info.h"
#include <sstream>
#include <algorithm>
#include <cassert>

namespace aerojs {
namespace core {

/**
 * @brief 関数の完全な名前を取得
 * 
 * @return 修飾された関数名
 */
std::string FunctionInfo::getFullName() const {
    std::string prefix;
    
    if (is_async) {
        prefix += "async ";
    }
    
    if (is_generator) {
        prefix += "* ";
    }
    
    std::string result = prefix + (name.empty() ? "<anonymous>" : name);
    return result;
}

/**
 * @brief 関数のデバッグ情報を文字列形式で取得
 * 
 * @param verbose 詳細な情報を含めるかどうか
 * @return 関数のデバッグ情報
 */
std::string FunctionInfo::getDebugString(bool verbose) const {
    std::stringstream ss;
    
    ss << getSignature();
    
    if (verbose) {
        ss << "\n  コード位置: " << code_offset << "-" << (code_offset + code_length - 1);
        ss << "\n  引数の数: " << arity;
        ss << "\n  厳格モード: " << (is_strict ? "はい" : "いいえ");
        ss << "\n  アロー関数: " << (is_arrow_function ? "はい" : "いいえ");
        ss << "\n  ジェネレーター: " << (is_generator ? "はい" : "いいえ");
        ss << "\n  非同期関数: " << (is_async ? "はい" : "いいえ");
        ss << "\n  レスト引数: " << (has_rest_parameter ? "あり" : "なし");
        ss << "\n  デフォルト引数: " << (has_default_parameters ? "あり" : "なし");
        
        ss << "\n  引数名: ";
        for (size_t i = 0; i < parameter_names.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << parameter_names[i];
        }
        
        ss << "\n  ローカル変数: ";
        for (size_t i = 0; i < local_names.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << local_names[i];
        }
        
        if (!captured_variables.empty()) {
            ss << "\n  キャプチャ変数: ";
            bool first = true;
            for (const auto& [name, index] : captured_variables) {
                if (!first) ss << ", ";
                ss << name << " (idx:" << index << ")";
                first = false;
            }
        }
        
        ss << "\n  ソース位置: " << source_location_start << "-" << source_location_end;
        
        if (parent_function_index != static_cast<uint32_t>(-1)) {
            ss << "\n  親関数インデックス: " << parent_function_index;
        }
    }
    
    return ss.str();
}

/**
 * @brief 関数シグネチャの比較
 * 
 * @param other 比較する関数情報
 * @return 等しい場合はtrue、そうでない場合はfalse
 */
bool FunctionInfo::hasSameSignature(const FunctionInfo& other) const {
    if (name != other.name) return false;
    if (arity != other.arity) return false;
    if (parameter_names.size() != other.parameter_names.size()) return false;
    if (is_strict != other.is_strict) return false;
    if (is_arrow_function != other.is_arrow_function) return false;
    if (is_generator != other.is_generator) return false;
    if (is_async != other.is_async) return false;
    if (has_rest_parameter != other.has_rest_parameter) return false;
    if (has_default_parameters != other.has_default_parameters) return false;
    
    for (size_t i = 0; i < parameter_names.size(); ++i) {
        if (parameter_names[i] != other.parameter_names[i]) return false;
    }
    
    return true;
}

/**
 * @brief 関数情報をシリアライズしてバイナリデータに変換
 * 
 * @return シリアライズされたバイナリデータ
 */
std::vector<uint8_t> FunctionInfo::serialize() const {
    // この実装は基本的なもので、実際の環境では圧縮や最適化が必要かもしれません
    std::vector<uint8_t> result;
    
    // 名前をシリアライズ
    serializeString(result, name);
    
    // 基本プロパティをシリアライズ
    serializeUint32(result, arity);
    serializeUint32(result, code_offset);
    serializeUint32(result, code_length);
    
    // フラグをバイトにパック
    uint8_t flags = 0;
    if (is_strict) flags |= 0x01;
    if (is_arrow_function) flags |= 0x02;
    if (is_generator) flags |= 0x04;
    if (is_async) flags |= 0x08;
    if (has_rest_parameter) flags |= 0x10;
    if (has_default_parameters) flags |= 0x20;
    result.push_back(flags);
    
    // 引数名をシリアライズ
    serializeStringVector(result, parameter_names);
    
    // ローカル変数名をシリアライズ
    serializeStringVector(result, local_names);
    
    // 位置情報をシリアライズ
    serializeUint32(result, source_location_start);
    serializeUint32(result, source_location_end);
    serializeUint32(result, parent_function_index);
    serializeUint32(result, scope_index);
    
    // キャプチャされた変数をシリアライズ
    serializeUint32(result, static_cast<uint32_t>(captured_variables.size()));
    for (const auto& [name, index] : captured_variables) {
        serializeString(result, name);
        serializeUint32(result, index);
    }
    
    return result;
}

/**
 * @brief バイナリデータから関数情報をデシリアライズ
 * 
 * @param data デシリアライズするバイナリデータ
 * @param pos データ内の現在位置（入出力パラメータ）
 * @return デシリアライズされた関数情報
 */
FunctionInfo FunctionInfo::deserialize(const std::vector<uint8_t>& data, size_t& pos) {
    FunctionInfo info;
    
    // 名前をデシリアライズ
    info.name = deserializeString(data, pos);
    
    // 基本プロパティをデシリアライズ
    info.arity = deserializeUint32(data, pos);
    info.code_offset = deserializeUint32(data, pos);
    info.code_length = deserializeUint32(data, pos);
    
    // フラグをデシリアライズ
    uint8_t flags = data[pos++];
    info.is_strict = (flags & 0x01) != 0;
    info.is_arrow_function = (flags & 0x02) != 0;
    info.is_generator = (flags & 0x04) != 0;
    info.is_async = (flags & 0x08) != 0;
    info.has_rest_parameter = (flags & 0x10) != 0;
    info.has_default_parameters = (flags & 0x20) != 0;
    
    // 引数名をデシリアライズ
    info.parameter_names = deserializeStringVector(data, pos);
    
    // ローカル変数名をデシリアライズ
    info.local_names = deserializeStringVector(data, pos);
    
    // 位置情報をデシリアライズ
    info.source_location_start = deserializeUint32(data, pos);
    info.source_location_end = deserializeUint32(data, pos);
    info.parent_function_index = deserializeUint32(data, pos);
    info.scope_index = deserializeUint32(data, pos);
    
    // キャプチャされた変数をデシリアライズ
    uint32_t captured_count = deserializeUint32(data, pos);
    for (uint32_t i = 0; i < captured_count; ++i) {
        std::string var_name = deserializeString(data, pos);
        uint32_t var_index = deserializeUint32(data, pos);
        info.captured_variables[var_name] = var_index;
    }
    
    return info;
}

// プライベートヘルパーメソッド

/**
 * @brief 文字列をバイナリデータにシリアライズ
 * 
 * @param out 出力バイナリデータ
 * @param str シリアライズする文字列
 */
void FunctionInfo::serializeString(std::vector<uint8_t>& out, const std::string& str) const {
    serializeUint32(out, static_cast<uint32_t>(str.length()));
    out.insert(out.end(), str.begin(), str.end());
}

/**
 * @brief 文字列ベクターをバイナリデータにシリアライズ
 * 
 * @param out 出力バイナリデータ
 * @param strings シリアライズする文字列ベクター
 */
void FunctionInfo::serializeStringVector(std::vector<uint8_t>& out, const std::vector<std::string>& strings) const {
    serializeUint32(out, static_cast<uint32_t>(strings.size()));
    for (const auto& str : strings) {
        serializeString(out, str);
    }
}

/**
 * @brief 32ビット符号なし整数をバイナリデータにシリアライズ
 * 
 * @param out 出力バイナリデータ
 * @param value シリアライズする整数
 */
void FunctionInfo::serializeUint32(std::vector<uint8_t>& out, uint32_t value) const {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

/**
 * @brief バイナリデータから文字列をデシリアライズ
 * 
 * @param data デシリアライズするバイナリデータ
 * @param pos データ内の現在位置（入出力パラメータ）
 * @return デシリアライズされた文字列
 */
std::string FunctionInfo::deserializeString(const std::vector<uint8_t>& data, size_t& pos) {
    uint32_t length = deserializeUint32(data, pos);
    std::string result(data.begin() + pos, data.begin() + pos + length);
    pos += length;
    return result;
}

/**
 * @brief バイナリデータから文字列ベクターをデシリアライズ
 * 
 * @param data デシリアライズするバイナリデータ
 * @param pos データ内の現在位置（入出力パラメータ）
 * @return デシリアライズされた文字列ベクター
 */
std::vector<std::string> FunctionInfo::deserializeStringVector(const std::vector<uint8_t>& data, size_t& pos) {
    uint32_t count = deserializeUint32(data, pos);
    std::vector<std::string> result;
    result.reserve(count);
    
    for (uint32_t i = 0; i < count; ++i) {
        result.push_back(deserializeString(data, pos));
    }
    
    return result;
}

/**
 * @brief バイナリデータから32ビット符号なし整数をデシリアライズ
 * 
 * @param data デシリアライズするバイナリデータ
 * @param pos データ内の現在位置（入出力パラメータ）
 * @return デシリアライズされた整数
 */
uint32_t FunctionInfo::deserializeUint32(const std::vector<uint8_t>& data, size_t& pos) {
    assert(pos + 4 <= data.size());
    uint32_t result = 
        static_cast<uint32_t>(data[pos]) |
        (static_cast<uint32_t>(data[pos + 1]) << 8) |
        (static_cast<uint32_t>(data[pos + 2]) << 16) |
        (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    return result;
}

} // namespace core
} // namespace aerojs 