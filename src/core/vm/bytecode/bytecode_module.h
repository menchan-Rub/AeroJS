/**
 * @file bytecode_module.h
 * @brief バイトコードモジュールのヘッダーファイル
 *
 * このファイルは、VMが実行するバイトコードモジュールクラスを定義します。
 * バイトコードモジュールは、生成されたバイトコード命令列、定数プール、
 * デバッグ情報などを含みます。
 */

#ifndef AEROJS_CORE_VM_BYTECODE_BYTECODE_MODULE_H_
#define AEROJS_CORE_VM_BYTECODE_BYTECODE_MODULE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../interpreter/bytecode_instruction.h"
#include "constant_pool.h"
#include "function_info.h"
#include "sourcemap_entry.h"

namespace aerojs {
namespace core {

/**
 * @brief バイトコードモジュールのメタデータ
 */
struct BytecodeModuleMetadata {
  std::string source_file;  ///< ソースファイル名
  std::string module_name;  ///< モジュール名
  std::string version;      ///< バイトコードバージョン
  bool is_module;           ///< ESモジュールかどうか
  bool strict_mode;         ///< 厳格モードかどうか
  int timestamp;            ///< 生成タイムスタンプ

  BytecodeModuleMetadata()
      : is_module(false), strict_mode(false), timestamp(0) {
  }
};

/**
 * @brief バイトコードモジュールクラス
 *
 * このクラスは、VMによって実行可能なバイトコードモジュールを表します。
 * 命令列、定数プール、関数情報、ソースマップなどを含みます。
 */
class BytecodeModule {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param metadata モジュールのメタデータ
   */
  explicit BytecodeModule(const BytecodeModuleMetadata& metadata = BytecodeModuleMetadata());

  /**
   * @brief デストラクタ
   */
  ~BytecodeModule();

  /**
   * @brief バイトコード命令を追加
   *
   * @param instruction 追加する命令
   * @return 命令のインデックス
   */
  uint32_t addInstruction(const BytecodeInstruction& instruction);

  /**
   * @brief 命令列を設定
   *
   * @param instructions 設定する命令列
   */
  void setInstructions(const std::vector<BytecodeInstruction>& instructions);

  /**
   * @brief 定数プールを設定
   *
   * @param constant_pool 設定する定数プール
   */
  void setConstantPool(std::shared_ptr<ConstantPool> constant_pool);

  /**
   * @brief 関数情報を追加
   *
   * @param function_info 追加する関数情報
   * @return 関数のインデックス
   */
  uint32_t addFunctionInfo(const FunctionInfo& function_info);

  /**
   * @brief ソースマップエントリを追加
   *
   * @param entry 追加するソースマップエントリ
   */
  void addSourceMapEntry(const SourceMapEntry& entry);

  /**
   * @brief 文字列テーブルに文字列を追加
   *
   * @param str 追加する文字列
   * @return 文字列のインデックス
   */
  uint32_t addString(const std::string& str);

  /**
   * @brief メタデータを取得
   *
   * @return モジュールのメタデータ
   */
  const BytecodeModuleMetadata& getMetadata() const;

  /**
   * @brief メタデータを設定
   *
   * @param metadata 設定するメタデータ
   */
  void setMetadata(const BytecodeModuleMetadata& metadata);

  /**
   * @brief 命令列を取得
   *
   * @return 命令列
   */
  const std::vector<BytecodeInstruction>& getInstructions() const;

  /**
   * @brief 指定インデックスの命令を取得
   *
   * @param index 取得する命令のインデックス
   * @return 命令
   */
  const BytecodeInstruction& getInstruction(uint32_t index) const;

  /**
   * @brief 命令列の長さを取得
   *
   * @return 命令列の長さ
   */
  size_t getInstructionCount() const;

  /**
   * @brief 定数プールを取得
   *
   * @return 定数プール
   */
  std::shared_ptr<ConstantPool> getConstantPool() const;

  /**
   * @brief 関数情報のリストを取得
   *
   * @return 関数情報のリスト
   */
  const std::vector<FunctionInfo>& getFunctionInfos() const;

  /**
   * @brief 指定インデックスの関数情報を取得
   *
   * @param index 取得する関数情報のインデックス
   * @return 関数情報
   */
  const FunctionInfo& getFunctionInfo(uint32_t index) const;

  /**
   * @brief ソースマップを取得
   *
   * @return ソースマップ
   */
  const std::vector<SourceMapEntry>& getSourceMap() const;

  /**
   * @brief 文字列テーブルを取得
   *
   * @return 文字列テーブル
   */
  const std::vector<std::string>& getStringTable() const;

  /**
   * @brief 指定インデックスの文字列を取得
   *
   * @param index 取得する文字列のインデックス
   * @return 文字列
   */
  const std::string& getString(uint32_t index) const;

  /**
   * @brief バイトコードモジュールをバイナリ形式でシリアライズ
   *
   * @return シリアライズされたバイナリデータ
   */
  std::vector<uint8_t> serialize() const;

  /**
   * @brief バイナリデータからバイトコードモジュールをデシリアライズ
   *
   * @param data デシリアライズするバイナリデータ
   * @return デシリアライズされたバイトコードモジュール
   */
  static std::unique_ptr<BytecodeModule> deserialize(const std::vector<uint8_t>& data);

  /**
   * @brief バイトコードモジュールの内容をダンプ（デバッグ用）
   *
   * @param output 出力ストリーム
   * @param verbose 詳細な出力を行うかどうか
   */
  void dump(std::ostream& output, bool verbose = false) const;

 private:
  BytecodeModuleMetadata m_metadata;                ///< モジュールのメタデータ
  std::vector<BytecodeInstruction> m_instructions;  ///< 命令列
  std::shared_ptr<ConstantPool> m_constantPool;     ///< 定数プール
  std::vector<FunctionInfo> m_functionInfos;        ///< 関数情報のリスト
  std::vector<SourceMapEntry> m_sourceMap;          ///< ソースマップ
  std::vector<std::string> m_stringTable;           ///< 文字列テーブル
};

}  // namespace core
}  // namespace aerojs

#endif  // AEROJS_CORE_VM_BYTECODE_BYTECODE_MODULE_H_