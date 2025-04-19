/**
 * @file function_info.h
 * @brief 関数情報のヘッダーファイル
 * 
 * このファイルは、バイトコードモジュール内の関数に関する情報を保持するクラスを定義します。
 * 関数名、引数の数、関数本体のバイトコード位置などの情報を含みます。
 */

#ifndef AEROJS_CORE_VM_BYTECODE_FUNCTION_INFO_H_
#define AEROJS_CORE_VM_BYTECODE_FUNCTION_INFO_H_

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace aerojs {
namespace core {

/**
 * @brief 関数情報クラス
 * 
 * JavaScriptの関数に関する情報を保持します。
 */
struct FunctionInfo {
    std::string name;                        ///< 関数名（無名関数の場合は空文字列）
    uint32_t arity;                          ///< 引数の数
    uint32_t code_offset;                    ///< 関数本体のバイトコード位置
    uint32_t code_length;                    ///< 関数本体のバイトコード長
    std::vector<std::string> parameter_names;///< 引数名のリスト
    std::vector<std::string> local_names;    ///< ローカル変数名のリスト
    bool is_strict;                          ///< 厳格モードかどうか
    bool is_arrow_function;                  ///< アロー関数かどうか
    bool is_generator;                       ///< ジェネレーター関数かどうか
    bool is_async;                           ///< 非同期関数かどうか
    bool has_rest_parameter;                 ///< レスト引数を持つかどうか
    bool has_default_parameters;             ///< デフォルト引数を持つかどうか
    uint32_t source_location_start;          ///< ソースコード内の開始位置
    uint32_t source_location_end;            ///< ソースコード内の終了位置
    uint32_t parent_function_index;          ///< 親関数のインデックス（トップレベルの場合は-1）
    uint32_t scope_index;                    ///< この関数が定義されたスコープのインデックス
    std::unordered_map<std::string, uint32_t> captured_variables; ///< キャプチャされた変数とそのインデックス
    
    /**
     * @brief コンストラクタ
     * 
     * @param name 関数名
     * @param arity 引数の数
     * @param code_offset 関数本体のバイトコード位置
     * @param code_length 関数本体のバイトコード長
     */
    FunctionInfo(const std::string& name = "",
                uint32_t arity = 0,
                uint32_t code_offset = 0,
                uint32_t code_length = 0)
        : name(name),
          arity(arity),
          code_offset(code_offset),
          code_length(code_length),
          is_strict(false),
          is_arrow_function(false),
          is_generator(false),
          is_async(false),
          has_rest_parameter(false),
          has_default_parameters(false),
          source_location_start(0),
          source_location_end(0),
          parent_function_index(static_cast<uint32_t>(-1)),
          scope_index(0) {}
    
    /**
     * @brief 関数のシグネチャを取得
     * 
     * @return 関数のシグネチャ文字列
     */
    std::string getSignature() const {
        std::string signature = name.empty() ? "<anonymous>" : name;
        signature += "(";
        
        for (size_t i = 0; i < parameter_names.size(); ++i) {
            if (i > 0) {
                signature += ", ";
            }
            
            signature += parameter_names[i];
            
            if (i == parameter_names.size() - 1 && has_rest_parameter) {
                signature += "...";
            }
        }
        
        signature += ")";
        
        if (is_generator) {
            signature = "*" + signature;
        }
        
        if (is_async) {
            signature = "async " + signature;
        }
        
        return signature;
    }
    
    /**
     * @brief 関数が名前付き関数かどうかを確認
     * 
     * @return 名前付き関数の場合はtrue、無名関数の場合はfalse
     */
    bool isNamed() const {
        return !name.empty();
    }
    
    /**
     * @brief 引数名を追加
     * 
     * @param name 追加する引数名
     */
    void addParameterName(const std::string& name) {
        parameter_names.push_back(name);
    }
    
    /**
     * @brief ローカル変数名を追加
     * 
     * @param name 追加するローカル変数名
     */
    void addLocalName(const std::string& name) {
        local_names.push_back(name);
    }
};

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_VM_BYTECODE_FUNCTION_INFO_H_ 