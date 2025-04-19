/**
 * @file date.h
 * @brief JavaScriptのDateオブジェクトの定義
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_DATE_H
#define AERO_DATE_H

#include "../../object.h"
#include "../../value.h"
#include <chrono>
#include <string>
#include <vector>

namespace aero {

class GlobalObject;

/**
 * @class DateObject
 * @brief JavaScriptのDateオブジェクトを表現するクラス
 * 
 * ECMAScript仕様に基づき、日付と時刻を処理するための機能を提供します。
 * 内部的にはstd::chrono::system_clockを使用して時刻を管理します。
 */
class DateObject : public Object {
public:
    /**
     * @brief 現在の日時でDateオブジェクトを作成するコンストラクタ
     */
    DateObject();
    
    /**
     * @brief 指定されたミリ秒値でDateオブジェクトを作成するコンストラクタ
     * @param timeValue UNIX時間（1970年1月1日からのミリ秒）
     */
    explicit DateObject(double timeValue);
    
    /**
     * @brief 指定された年月日時分秒でDateオブジェクトを作成するコンストラクタ
     * @param year 年（例: 2023）
     * @param month 月（0〜11）
     * @param day 日（1〜31）
     * @param hour 時（0〜23）
     * @param minute 分（0〜59）
     * @param second 秒（0〜59）
     * @param millisecond ミリ秒（0〜999）
     */
    DateObject(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int millisecond = 0);
    
    /**
     * @brief デストラクタ
     */
    ~DateObject();
    
    /**
     * @brief オブジェクトがDateオブジェクトかどうかを判定
     * @return 常にtrue
     */
    bool isDateObject() const override { return true; }
    
    /**
     * @brief Dateオブジェクトのプリミティブ値（ミリ秒値）を取得
     * @return Dateオブジェクトのプリミティブ値を表すValue
     */
    Value valueOf() const;
    
    /**
     * @brief ISO 8601形式の文字列表現を取得
     * @return ISO 8601形式の文字列（例: "2023-01-01T12:00:00.000Z"）
     */
    std::string toISOString() const;
    
    /**
     * @brief 日付を文字列表現で取得
     * @return 日付の文字列表現
     */
    std::string toString() const;
    
    /**
     * @brief 1970年1月1日からのミリ秒数を取得
     * @return ミリ秒数
     */
    double getTime() const;
    
    /**
     * @brief 1970年1月1日からのミリ秒数を設定
     * @param timeValue ミリ秒数
     */
    void setTime(double timeValue);
    
    /**
     * @brief 年を取得
     * @return 年（例: 2023）
     */
    int getFullYear() const;
    
    /**
     * @brief 年を設定
     * @param year 年（例: 2023）
     */
    void setFullYear(int year);
    
    /**
     * @brief 月を取得
     * @return 月（0〜11）
     */
    int getMonth() const;
    
    /**
     * @brief 月を設定
     * @param month 月（0〜11）
     */
    void setMonth(int month);
    
    /**
     * @brief 日を取得
     * @return 日（1〜31）
     */
    int getDate() const;
    
    /**
     * @brief 日を設定
     * @param date 日（1〜31）
     */
    void setDate(int date);
    
    /**
     * @brief 時を取得
     * @return 時（0〜23）
     */
    int getHours() const;
    
    /**
     * @brief 時を設定
     * @param hours 時（0〜23）
     */
    void setHours(int hours);
    
    /**
     * @brief 分を取得
     * @return 分（0〜59）
     */
    int getMinutes() const;
    
    /**
     * @brief 分を設定
     * @param minutes 分（0〜59）
     */
    void setMinutes(int minutes);
    
    /**
     * @brief 秒を取得
     * @return 秒（0〜59）
     */
    int getSeconds() const;
    
    /**
     * @brief 秒を設定
     * @param seconds 秒（0〜59）
     */
    void setSeconds(int seconds);
    
    /**
     * @brief ミリ秒を取得
     * @return ミリ秒（0〜999）
     */
    int getMilliseconds() const;
    
    /**
     * @brief ミリ秒を設定
     * @param milliseconds ミリ秒（0〜999）
     */
    void setMilliseconds(int milliseconds);
    
    /**
     * @brief Dateプロトタイプオブジェクト
     */
    static Object* s_prototype;
    
private:
    /**
     * @brief 内部的な時間表現
     */
    std::chrono::system_clock::time_point m_time;
};

/**
 * @brief Dateコンストラクタ関数
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 新しく作成されたDateオブジェクト
 */
Value dateConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.now 静的メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 現在時刻（1970年1月1日からのミリ秒数）
 */
Value dateNow(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief 日付文字列をパースしてミリ秒値に変換
 * @param dateString 日付を表す文字列
 * @return 1970年1月1日からのミリ秒数（パース失敗時はNaN）
 */
double dateParse(const std::string& dateString);

/**
 * @brief Date.prototype.toString メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 日付の文字列表現
 */
Value dateToString(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.valueOf メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 日付のプリミティブ値（ミリ秒数）
 */
Value dateValueOf(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getTime メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 1970年1月1日からのミリ秒数
 */
Value dateGetTime(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setTime メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された1970年1月1日からのミリ秒数
 */
Value dateSetTime(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.toISOString メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return ISO 8601形式の文字列
 */
Value dateToISOString(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getFullYear メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 年（例: 2023）
 */
Value dateGetFullYear(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setFullYear メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetFullYear(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getMonth メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 月（0〜11）
 */
Value dateGetMonth(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setMonth メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetMonth(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getDate メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 日（1〜31）
 */
Value dateGetDate(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setDate メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetDate(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getHours メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 時（0〜23）
 */
Value dateGetHours(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setHours メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetHours(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getMinutes メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 分（0〜59）
 */
Value dateGetMinutes(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setMinutes メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetMinutes(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getSeconds メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 秒（0〜59）
 */
Value dateGetSeconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setSeconds メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetSeconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.getMilliseconds メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return ミリ秒（0〜999）
 */
Value dateGetMilliseconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Date.prototype.setMilliseconds メソッド
 * @param args 引数リスト
 * @param thisObj thisオブジェクト
 * @param globalObj グローバルオブジェクト
 * @return 設定された日時の1970年1月1日からのミリ秒数
 */
Value dateSetMilliseconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj);

/**
 * @brief Dateプロトタイプの初期化
 * @param globalObj グローバルオブジェクト
 */
void initDatePrototype(GlobalObject* globalObj);

/**
 * @brief Dateオブジェクトの初期化
 * @param globalObj グローバルオブジェクト
 */
void initDateObject(GlobalObject* globalObj);

} // namespace aero

#endif // AERO_DATE_H 