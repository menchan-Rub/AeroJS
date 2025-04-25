#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../ir/ir.h"

namespace aerojs {
namespace core {

/**
 * @brief 検証エラーの種類を表す列挙型
 */
enum class ValidationErrorType {
  kInvalidOpcode,               // 無効なオペコード
  kInvalidOperandCount,         // 無効なオペランド数
  kInvalidRegister,             // 無効なレジスタ
  kUndefinedRegister,           // 未定義のレジスタ使用
  kInvalidJumpTarget,           // 無効なジャンプターゲット
  kUndefinedLabel,              // 未定義のラベル使用
  kInvalidConstantIndex,        // 無効な定数インデックス
  kInvalidFunctionIndex,        // 無効な関数インデックス
  kInvalidInstructionSequence,  // 無効な命令シーケンス
  kDuplicateLabel,              // ラベルの重複定義
  kUnreachableCode,             // 到達不能コード
  kIncompatibleTypes,           // 型の互換性エラー
  kStackImbalance,              // スタック不均衡
  kMaxRegistersExceeded,        // レジスタ数超過
  kCyclicDependency,            // 循環依存関係
  kOther                        // その他のエラー
};

/**
 * @brief 検証エラーを表す構造体
 */
struct ValidationError {
  ValidationErrorType type;      // エラーの種類
  size_t instructionIndex;       // エラーが発生した命令のインデックス
  std::string message;           // エラーメッセージ
  
  ValidationError(ValidationErrorType type, size_t index, const std::string& msg)
      : type(type), instructionIndex(index), message(msg) {}
};

/**
 * @brief IR検証オプションを表す構造体
 */
struct IRValidationOptions {
  bool checkUndefinedRegisters;    // 未定義レジスタの使用をチェック
  bool checkUnreachableCode;       // 到達不能コードをチェック
  bool checkTypes;                 // 型の互換性をチェック
  bool checkStackBalance;          // スタックバランスをチェック
  uint32_t maxAllowedRegisters;    // 許可される最大レジスタ数
  
  IRValidationOptions()
      : checkUndefinedRegisters(true)
      , checkUnreachableCode(true)
      , checkTypes(false)  // デフォルトは型チェックなし
      , checkStackBalance(true)
      , maxAllowedRegisters(256) {}
};

/**
 * @brief IR検証器クラス
 * 
 * IR関数の整合性を検証し、コンパイル前にエラーを検出する責務を持ちます。
 */
class IRValidator {
public:
  /**
   * @brief コンストラクタ
   * @param options 検証オプション
   */
  explicit IRValidator(const IRValidationOptions& options = IRValidationOptions()) noexcept;
  
  /**
   * @brief デストラクタ
   */
  ~IRValidator() noexcept;
  
  /**
   * @brief IR関数を検証する
   * @param function 検証対象のIR関数
   * @return エラーが検出されなかった場合true
   */
  bool Validate(const IRFunction& function) noexcept;
  
  /**
   * @brief 検出されたエラーを取得する
   * @return 検証エラーのリスト
   */
  const std::vector<ValidationError>& GetErrors() const noexcept;
  
  /**
   * @brief 検証オプションを設定する
   * @param options 設定する検証オプション
   */
  void SetOptions(const IRValidationOptions& options) noexcept;
  
  /**
   * @brief 最後のエラーメッセージを取得する
   * @return フォーマットされたエラーメッセージ
   */
  std::string GetErrorMessage() const noexcept;
  
  /**
   * @brief 検証器をリセットする
   */
  void Reset() noexcept;

private:
  /**
   * @brief 命令の基本的な整合性をチェックする
   * @param inst 検証対象の命令
   * @param index 命令のインデックス
   * @return エラーが検出されなかった場合true
   */
  bool ValidateInstruction(const IRInstruction& inst, size_t index) noexcept;
  
  /**
   * @brief レジスタの定義-使用関係を確認する
   * @param function 検証対象のIR関数
   * @return エラーが検出されなかった場合true
   */
  bool ValidateRegisterUsage(const IRFunction& function) noexcept;
  
  /**
   * @brief ジャンプターゲットの有効性を確認する
   * @param function 検証対象のIR関数
   * @param labels 定義されたラベルのマップ
   * @return エラーが検出されなかった場合true
   */
  bool ValidateJumpTargets(const IRFunction& function, 
                          const std::unordered_map<std::string, size_t>& labels) noexcept;
  
  /**
   * @brief コードの到達可能性を確認する
   * @param function 検証対象のIR関数
   * @param labels 定義されたラベルのマップ
   * @return エラーが検出されなかった場合true
   */
  bool ValidateReachability(const IRFunction& function,
                           const std::unordered_map<std::string, size_t>& labels) noexcept;
  
  /**
   * @brief スタックバランスをチェックする
   * @param function 検証対象のIR関数
   * @return エラーが検出されなかった場合true
   */
  bool ValidateStackBalance(const IRFunction& function) noexcept;
  
  /**
   * @brief エラーを追加する
   * @param type エラーの種類
   * @param index 命令のインデックス
   * @param message エラーメッセージ
   */
  void AddError(ValidationErrorType type, size_t index, const std::string& message) noexcept;
  
  /**
   * @brief レジスタの最大数をチェックする
   * @param function 検証対象のIR関数
   * @return エラーが検出されなかった場合true
   */
  bool ValidateRegisterCount(const IRFunction& function) noexcept;

private:
  // 検証オプション
  IRValidationOptions m_options;
  
  // 検出されたエラーのリスト
  std::vector<ValidationError> m_errors;
};

/**
 * @brief 検証エラーの種類を文字列に変換する
 * @param type 変換する検証エラーの種類
 * @return エラー種類の文字列表現
 */
std::string ValidationErrorTypeToString(ValidationErrorType type);

} // namespace core
} // namespace aerojs 