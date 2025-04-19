/**
 * @file date.cpp
 * @brief JavaScriptのDateオブジェクトの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "date.h"
#include "../../error.h"
#include "../../global_object.h"
#include "../../value.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>

namespace aero {

// 静的変数の初期化
Object* DateObject::s_prototype = nullptr;

// ユーティリティ関数
namespace {
    // tm構造体をミリ秒値に変換
    double tmToMilliseconds(const std::tm& tm) {
        std::time_t time = std::mktime(const_cast<std::tm*>(&tm));
        return static_cast<double>(time) * 1000;
    }

    // ミリ秒値からtm構造体へ変換
    std::tm millisecondsToTm(double timeValue) {
        std::time_t time = static_cast<std::time_t>(timeValue / 1000);
        return *std::gmtime(&time);
    }

    // 日付が有効かどうかをチェック
    bool isValidDate(double timeValue) {
        return !std::isnan(timeValue) && std::isfinite(timeValue);
    }

    // 引数からDateオブジェクトを取得（型チェック付き）
    DateObject* getDateObject(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
        if (!thisObj || !thisObj->isDateObject()) {
            throwError(ErrorType::TypeError, "Date.prototype.method called on incompatible receiver", globalObj);
            return nullptr;
        }
        return static_cast<DateObject*>(thisObj);
    }
}

DateObject::DateObject()
    : Object(ObjectType::DateObject)
{
    m_time = std::chrono::system_clock::now();
    setPrototype(s_prototype);
}

DateObject::DateObject(double timeValue)
    : Object(ObjectType::DateObject)
{
    setTime(timeValue);
    setPrototype(s_prototype);
}

DateObject::DateObject(int year, int month, int day, int hour, int minute, int second, int millisecond)
    : Object(ObjectType::DateObject)
{
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    
    std::time_t time = std::mktime(&tm);
    auto timePoint = std::chrono::system_clock::from_time_t(time);
    timePoint += std::chrono::milliseconds(millisecond);
    
    m_time = timePoint;
    setPrototype(s_prototype);
}

DateObject::~DateObject() = default;

Value DateObject::valueOf() const {
    return Value(getTime());
}

std::string DateObject::toISOString() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    
    // ミリ秒の計算
    auto epoch = m_time.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(epoch - seconds);
    
    std::ostringstream ss;
    ss << std::setfill('0') 
       << std::setw(4) << (tm.tm_year + 1900) << '-' 
       << std::setw(2) << (tm.tm_mon + 1) << '-'
       << std::setw(2) << tm.tm_mday << 'T'
       << std::setw(2) << tm.tm_hour << ':'
       << std::setw(2) << tm.tm_min << ':'
       << std::setw(2) << tm.tm_sec << '.'
       << std::setw(3) << milliseconds.count() << 'Z';
    
    return ss.str();
}

std::string DateObject::toString() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    
    const char* weekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    
    std::ostringstream ss;
    ss << weekDays[tm.tm_wday] << " " 
       << months[tm.tm_mon] << " "
       << std::setw(2) << tm.tm_mday << " "
       << std::setw(4) << (tm.tm_year + 1900) << " "
       << std::setfill('0')
       << std::setw(2) << tm.tm_hour << ":"
       << std::setw(2) << tm.tm_min << ":"
       << std::setw(2) << tm.tm_sec << " GMT";
    
    return ss.str();
}

double DateObject::getTime() const {
    auto duration = m_time.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return static_cast<double>(milliseconds.count());
}

void DateObject::setTime(double timeValue) {
    if (std::isnan(timeValue)) {
        // NaN値の場合は無効な日付として設定
        m_time = std::chrono::system_clock::from_time_t(0);
        m_time -= m_time.time_since_epoch(); // エポック時間の0に設定
    } else {
        auto milliseconds = std::chrono::milliseconds(static_cast<int64_t>(timeValue));
        m_time = std::chrono::system_clock::time_point(milliseconds);
    }
}

int DateObject::getFullYear() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_year + 1900;
}

void DateObject::setFullYear(int year) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_year = year - 1900;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getMonth() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_mon;
}

void DateObject::setMonth(int month) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_mon = month;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getDate() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_mday;
}

void DateObject::setDate(int date) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_mday = date;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getHours() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_hour;
}

void DateObject::setHours(int hours) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_hour = hours;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getMinutes() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_min;
}

void DateObject::setMinutes(int minutes) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_min = minutes;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getSeconds() const {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    return tm.tm_sec;
}

void DateObject::setSeconds(int seconds) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    tm.tm_sec = seconds;
    
    std::time_t newTime = std::mktime(&tm);
    m_time = std::chrono::system_clock::from_time_t(newTime);
}

int DateObject::getMilliseconds() const {
    auto duration = m_time.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration - seconds);
    return static_cast<int>(milliseconds.count());
}

void DateObject::setMilliseconds(int milliseconds) {
    auto time = std::chrono::system_clock::to_time_t(m_time);
    std::tm tm = *std::gmtime(&time);
    
    std::time_t newTime = std::mktime(&tm);
    auto timePoint = std::chrono::system_clock::from_time_t(newTime);
    timePoint += std::chrono::milliseconds(milliseconds);
    
    m_time = timePoint;
}

// Dateコンストラクタの実装
Value dateConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    if (!thisObj || thisObj == globalObj->getGlobalObject()) {
        // Dateを文字列として呼び出した場合
        // 現在の日時を文字列で返す
        DateObject tempDate;
        return Value(tempDate.toString());
    }
    
    if (args.empty()) {
        // 引数なしの場合は現在の日時
        return Value(new DateObject());
    }
    
    if (args.size() == 1) {
        // 1つの引数の場合
        if (args[0].isString()) {
            // 文字列の場合はパースして日時を設定
            double timeValue = dateParse(args[0].toString());
            return Value(new DateObject(timeValue));
        } else if (args[0].isNumber()) {
            // 数値の場合は1970年1月1日からのミリ秒として解釈
            return Value(new DateObject(args[0].toNumber()));
        } else if (args[0].isObject() && args[0].asObject()->isDateObject()) {
            // Dateオブジェクトの場合はそのミリ秒値を使用
            DateObject* dateObj = static_cast<DateObject*>(args[0].asObject());
            return Value(new DateObject(dateObj->getTime()));
        }
    }
    
    // 複数の引数がある場合は年月日時分秒として解釈
    int year = args.size() > 0 ? args[0].toInt32() : 0;
    int month = args.size() > 1 ? args[1].toInt32() : 0;
    int day = args.size() > 2 ? args[2].toInt32() : 1;
    int hour = args.size() > 3 ? args[3].toInt32() : 0;
    int minute = args.size() > 4 ? args[4].toInt32() : 0;
    int second = args.size() > 5 ? args[5].toInt32() : 0;
    int millisecond = args.size() > 6 ? args[6].toInt32() : 0;
    
    return Value(new DateObject(year, month, day, hour, minute, second, millisecond));
}

// Date.now 静的メソッドの実装
Value dateNow(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return Value(static_cast<double>(milliseconds.count()));
}

// Date.parse 静的メソッドの実装
double dateParse(const std::string& dateString) {
    // ISO 8601形式のパターン
    std::regex isoPattern(R"(^(\d{4})-(\d{2})-(\d{2})(?:T(\d{2}):(\d{2}):(\d{2})(?:\.(\d{3}))?Z?)?$)");
    std::smatch matches;
    
    if (std::regex_match(dateString, matches, isoPattern)) {
        std::tm tm = {};
        tm.tm_year = std::stoi(matches[1]) - 1900;
        tm.tm_mon = std::stoi(matches[2]) - 1;
        tm.tm_mday = std::stoi(matches[3]);
        
        if (matches[4].matched) {
            tm.tm_hour = std::stoi(matches[4]);
        }
        if (matches[5].matched) {
            tm.tm_min = std::stoi(matches[5]);
        }
        if (matches[6].matched) {
            tm.tm_sec = std::stoi(matches[6]);
        }
        
        std::time_t time = std::mktime(&tm);
        double milliseconds = static_cast<double>(time) * 1000;
        
        if (matches[7].matched) {
            milliseconds += std::stoi(matches[7]);
        }
        
        return milliseconds;
    }
    
    // 他の形式のパース（RFC 2822など）も実装可能
    // より完全な実装のためには、外部ライブラリの使用も検討する
    
    return std::numeric_limits<double>::quiet_NaN(); // パース失敗時はNaN
}

// Date.prototype.toString メソッドの実装
Value dateToString(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    return Value(dateObj->toString());
}

// Date.prototype.valueOf メソッドの実装
Value dateValueOf(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    return dateObj->valueOf();
}

// Date.prototype.getTime メソッドの実装
Value dateGetTime(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    return Value(dateObj->getTime());
}

// Date.prototype.setTime メソッドの実装
Value dateSetTime(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    double timeValue = args[0].toNumber();
    dateObj->setTime(timeValue);
    return Value(dateObj->getTime());
}

// Date.prototype.toISOString メソッドの実装
Value dateToISOString(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        throwError(ErrorType::RangeError, "Invalid date", globalObj);
        return Value::undefined();
    }
    
    return Value(dateObj->toISOString());
}

// Date.prototype.getFullYear メソッドの実装
Value dateGetFullYear(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getFullYear());
}

// Date.prototype.setFullYear メソッドの実装
Value dateSetFullYear(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int year = args[0].toInt32();
    int month = dateObj->getMonth();
    int day = dateObj->getDate();
    
    // 追加の引数がある場合は月と日も設定
    if (args.size() > 1 && args[1].isNumber()) {
        month = args[1].toInt32();
    }
    if (args.size() > 2 && args[2].isNumber()) {
        day = args[2].toInt32();
    }
    
    // 時間部分は変更せず
    int hour = dateObj->getHours();
    int minute = dateObj->getMinutes();
    int second = dateObj->getSeconds();
    int millisecond = dateObj->getMilliseconds();
    
    // 新しいDateオブジェクトの作成
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon = month;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    
    std::time_t time = std::mktime(&tm);
    auto timePoint = std::chrono::system_clock::from_time_t(time);
    timePoint += std::chrono::milliseconds(millisecond);
    
    dateObj->m_time = timePoint;
    return Value(dateObj->getTime());
}

// Date.prototype.getMonth メソッドの実装
Value dateGetMonth(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getMonth());
}

// Date.prototype.setMonth メソッドの実装
Value dateSetMonth(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int month = args[0].toInt32();
    int day = dateObj->getDate();
    
    // 追加の引数がある場合は日も設定
    if (args.size() > 1 && args[1].isNumber()) {
        day = args[1].toInt32();
    }
    
    dateObj->setMonth(month);
    dateObj->setDate(day);
    
    return Value(dateObj->getTime());
}

// Date.prototype.getDate メソッドの実装
Value dateGetDate(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getDate());
}

// Date.prototype.setDate メソッドの実装
Value dateSetDate(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int day = args[0].toInt32();
    dateObj->setDate(day);
    
    return Value(dateObj->getTime());
}

// Date.prototype.getHours メソッドの実装
Value dateGetHours(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getHours());
}

// Date.prototype.setHours メソッドの実装
Value dateSetHours(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int hours = args[0].toInt32();
    int minutes = dateObj->getMinutes();
    int seconds = dateObj->getSeconds();
    int milliseconds = dateObj->getMilliseconds();
    
    // 追加の引数がある場合は分、秒、ミリ秒も設定
    if (args.size() > 1 && args[1].isNumber()) {
        minutes = args[1].toInt32();
    }
    if (args.size() > 2 && args[2].isNumber()) {
        seconds = args[2].toInt32();
    }
    if (args.size() > 3 && args[3].isNumber()) {
        milliseconds = args[3].toInt32();
    }
    
    dateObj->setHours(hours);
    dateObj->setMinutes(minutes);
    dateObj->setSeconds(seconds);
    dateObj->setMilliseconds(milliseconds);
    
    return Value(dateObj->getTime());
}

// Date.prototype.getMinutes メソッドの実装
Value dateGetMinutes(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getMinutes());
}

// Date.prototype.setMinutes メソッドの実装
Value dateSetMinutes(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int minutes = args[0].toInt32();
    int seconds = dateObj->getSeconds();
    int milliseconds = dateObj->getMilliseconds();
    
    // 追加の引数がある場合は秒、ミリ秒も設定
    if (args.size() > 1 && args[1].isNumber()) {
        seconds = args[1].toInt32();
    }
    if (args.size() > 2 && args[2].isNumber()) {
        milliseconds = args[2].toInt32();
    }
    
    dateObj->setMinutes(minutes);
    dateObj->setSeconds(seconds);
    dateObj->setMilliseconds(milliseconds);
    
    return Value(dateObj->getTime());
}

// Date.prototype.getSeconds メソッドの実装
Value dateGetSeconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getSeconds());
}

// Date.prototype.setSeconds メソッドの実装
Value dateSetSeconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int seconds = args[0].toInt32();
    int milliseconds = dateObj->getMilliseconds();
    
    // 追加の引数がある場合はミリ秒も設定
    if (args.size() > 1 && args[1].isNumber()) {
        milliseconds = args[1].toInt32();
    }
    
    dateObj->setSeconds(seconds);
    dateObj->setMilliseconds(milliseconds);
    
    return Value(dateObj->getTime());
}

// Date.prototype.getMilliseconds メソッドの実装
Value dateGetMilliseconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (!isValidDate(dateObj->getTime())) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    return Value(dateObj->getMilliseconds());
}

// Date.prototype.setMilliseconds メソッドの実装
Value dateSetMilliseconds(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
    DateObject* dateObj = getDateObject(args, thisObj, globalObj);
    if (!dateObj) {
        return Value::undefined();
    }
    
    if (args.empty() || !args[0].isNumber()) {
        return Value(std::numeric_limits<double>::quiet_NaN());
    }
    
    int milliseconds = args[0].toInt32();
    dateObj->setMilliseconds(milliseconds);
    
    return Value(dateObj->getTime());
}

// Dateプロトタイプの初期化
void initDatePrototype(GlobalObject* globalObj) {
    DateObject::s_prototype = new Object(ObjectType::Object);
    DateObject::s_prototype->setPrototype(globalObj->getObjectPrototype());
    
    // プロトタイプメソッドの登録
    DateObject::s_prototype->defineNativeFunction("toString", dateToString, 0);
    DateObject::s_prototype->defineNativeFunction("valueOf", dateValueOf, 0);
    DateObject::s_prototype->defineNativeFunction("getTime", dateGetTime, 0);
    DateObject::s_prototype->defineNativeFunction("setTime", dateSetTime, 1);
    DateObject::s_prototype->defineNativeFunction("toISOString", dateToISOString, 0);
    DateObject::s_prototype->defineNativeFunction("getFullYear", dateGetFullYear, 0);
    DateObject::s_prototype->defineNativeFunction("setFullYear", dateSetFullYear, 1);
    DateObject::s_prototype->defineNativeFunction("getMonth", dateGetMonth, 0);
    DateObject::s_prototype->defineNativeFunction("setMonth", dateSetMonth, 1);
    DateObject::s_prototype->defineNativeFunction("getDate", dateGetDate, 0);
    DateObject::s_prototype->defineNativeFunction("setDate", dateSetDate, 1);
    DateObject::s_prototype->defineNativeFunction("getHours", dateGetHours, 0);
    DateObject::s_prototype->defineNativeFunction("setHours", dateSetHours, 1);
    DateObject::s_prototype->defineNativeFunction("getMinutes", dateGetMinutes, 0);
    DateObject::s_prototype->defineNativeFunction("setMinutes", dateSetMinutes, 1);
    DateObject::s_prototype->defineNativeFunction("getSeconds", dateGetSeconds, 0);
    DateObject::s_prototype->defineNativeFunction("setSeconds", dateSetSeconds, 1);
    DateObject::s_prototype->defineNativeFunction("getMilliseconds", dateGetMilliseconds, 0);
    DateObject::s_prototype->defineNativeFunction("setMilliseconds", dateSetMilliseconds, 1);
    
    // ECMAScript標準メソッドのエイリアスを追加
    DateObject::s_prototype->defineNativeFunction("toJSON", dateToISOString, 0);
}

// Dateオブジェクトの初期化
void initDateObject(GlobalObject* globalObj) {
    // Dateプロトタイプの初期化
    initDatePrototype(globalObj);
    
    // Dateコンストラクタオブジェクトの作成
    Object* dateConstructorObj = new Object(ObjectType::Function);
    dateConstructorObj->defineNativeFunction("constructor", dateConstructor, 7);
    
    // 静的メソッドの登録
    dateConstructorObj->defineNativeFunction("now", dateNow, 0);
    dateConstructorObj->defineNativeFunction("parse", [globalObj](const std::vector<Value>& args, Object* thisObj, GlobalObject*) -> Value {
        if (args.empty() || !args[0].isString()) {
            return Value(std::numeric_limits<double>::quiet_NaN());
        }
        return Value(dateParse(args[0].toString()));
    }, 1);
    
    // プロトタイプの設定
    dateConstructorObj->defineProperty("prototype", Value(DateObject::s_prototype), PropertyAttribute::None);
    
    // グローバルオブジェクトにDateコンストラクタを登録
    globalObj->defineProperty("Date", Value(dateConstructorObj));
}

} // namespace aero 