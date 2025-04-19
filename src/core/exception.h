/**
 * @file exception.h
 * @brief AeroJS JavaScript エンジンの例外クラスの定義
 * @version 0.1.0
 * @license MIT
 */

#ifndef AEROJS_CORE_EXCEPTION_H
#define AEROJS_CORE_EXCEPTION_H

#include <string>
#include <vector>
#include "../utils/memory/smart_ptr/ref_counted.h"

namespace aerojs {
namespace core {

// 前方宣言
class Context;
class Value;

/**
 * @brief エラー種別を定義する列挙型
 */
enum class ErrorType {
    Error,              // 一般的なエラー
    EvalError,          // eval()に関連するエラー
    RangeError,         // 値が特定の範囲外の場合
    ReferenceError,     // 無効な参照を使用した場合
    SyntaxError,        // 構文エラー
    TypeError,          // 型エラー
    URIError,           // encodeURI()などのURI関連関数のエラー
    AggregateError,     // 複数のエラーを表現するエラー
    InternalError       // エンジン内部エラー
};

/**
 * @brief スタックトレース要素を表す構造体
 */
struct StackTraceElement {
    std::string functionName; // 関数名
    std::string fileName;     // ファイル名
    int lineNumber;           // 行番号
    int columnNumber;         // 列番号
    
    StackTraceElement(
        const std::string& func = "",
        const std::string& file = "",
        int line = -1,
        int column = -1
    ) : functionName(func), fileName(file), lineNumber(line), columnNumber(column) {}
};

/**
 * @brief JavaScript実行時の例外を表すクラス
 */
class Exception : public utils::memory::RefCounted {
public:
    /**
     * @brief エラーメッセージから例外を作成
     * @param ctx コンテキスト
     * @param message エラーメッセージ
     * @param type エラー種別
     */
    static Exception* create(Context* ctx, const std::string& message, ErrorType type = ErrorType::Error);
    
    /**
     * @brief エラー値から例外を作成
     * @param ctx コンテキスト
     * @param errorValue エラーオブジェクト
     */
    static Exception* fromErrorObject(Context* ctx, Value* errorValue);
    
    /**
     * @brief 例外からエラーオブジェクトを作成
     * @return エラーオブジェクト
     */
    Value* toErrorObject() const;
    
    /**
     * @brief エラーメッセージを取得
     * @return エラーメッセージ
     */
    const std::string& getMessage() const { return m_message; }
    
    /**
     * @brief エラー種別を取得
     * @return エラー種別
     */
    ErrorType getType() const { return m_type; }
    
    /**
     * @brief スタックトレースを取得
     * @return スタックトレース
     */
    const std::vector<StackTraceElement>& getStackTrace() const { return m_stackTrace; }
    
    /**
     * @brief スタックトレースを設定
     * @param stackTrace スタックトレース
     */
    void setStackTrace(const std::vector<StackTraceElement>& stackTrace) {
        m_stackTrace = stackTrace;
    }
    
    /**
     * @brief スタックトレース要素を追加
     * @param element スタックトレース要素
     */
    void addStackTraceElement(const StackTraceElement& element) {
        m_stackTrace.push_back(element);
    }
    
    /**
     * @brief 完全なエラーメッセージとスタックトレースを文字列として取得
     * @return フォーマットされたエラー文字列
     */
    std::string toString() const;

    /**
     * @brief デストラクタ
     */
    virtual ~Exception() = default;

private:
    Context* m_context;                       // コンテキスト
    std::string m_message;                    // エラーメッセージ
    ErrorType m_type;                         // エラー種別
    std::vector<StackTraceElement> m_stackTrace; // スタックトレース
    
    /**
     * @brief コンストラクタ
     * @param ctx コンテキスト
     * @param message エラーメッセージ
     * @param type エラー種別
     */
    Exception(Context* ctx, const std::string& message, ErrorType type);
    
    /**
     * @brief エラー種別に対応するコンストラクタ名を取得
     * @param type エラー種別
     * @return コンストラクタ名
     */
    static std::string getErrorConstructorName(ErrorType type);
};

// 例外用のスマートポインタ型の定義
using ExceptionPtr = utils::memory::RefPtr<Exception>;

} // namespace core
} // namespace aerojs

#endif // AEROJS_CORE_EXCEPTION_H 