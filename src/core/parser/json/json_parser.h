/**
 * @file json_parser.h
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief 高性能JSONパーサーの実装
 *
 * このファイルは、AeroJS JavaScriptエンジンのためのJSONパーサーを定義します。
 * ECMAScript標準に準拠しており、オプションで拡張機能をサポートします。
 * - JSONオブジェクトとJSONプリミティブの解析
 * - 厳格な標準準拠モードと拡張モード（コメント、後続カンマなど）
 * - SIMD最適化によるパフォーマンス向上
 * - エラー回復と詳細なエラー報告
 * - メモリ使用量の最適化
 */

#ifndef AERO_JSON_PARSER_H
#define AERO_JSON_PARSER_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace aero {
namespace parser {
namespace json {

/**
 * @brief JSONパーサーのオプション構造体
 */
struct JsonParserOptions {
  /**
   * @brief コメントを許可するかどうか（// および /* */
  形式）
  * /
      bool allow_comments = false;

  /**
   * @brief 配列とオブジェクトの末尾のカンマを許可するかどうか
   */
  bool allow_trailing_commas = false;

  /**
   * @brief シングルクォートを文字列で許可するかどうか
   */
  bool allow_single_quotes = false;

  /**
   * @brief オブジェクトキーの引用符なしを許可するかどうか
   */
  bool allow_unquoted_keys = false;

  /**
   * @brief SIMD命令を使用してパフォーマンスを最適化するかどうか
   */
  bool use_simd = true;

  /**
   * @brief 並列処理を可能な場合に使用するかどうか
   */
  bool use_parallel = false;

  /**
   * @brief エラー時にできるだけ解析を続行してエラー報告を最大化するかどうか
   */
  bool error_recovery = false;

  /**
   * @brief 最大許容ネスト深度
   */
  uint32_t max_depth = 1000;

  /**
   * @brief 文字列バッファの初期サイズ（パフォーマンスチューニング用）
   */
  uint32_t string_buffer_initial_size = 1024;

  /**
   * @brief パース中に処理できる最大要素数（DoS保護用）
   */
  uint32_t max_elements = 1000000;
};

/**
 * @brief JSONパーサーの統計情報
 */
struct JsonParserStats {
  uint64_t total_tokens = 0;        ///< 処理された合計トークン数
  uint64_t total_objects = 0;       ///< パースされた合計オブジェクト数
  uint64_t total_arrays = 0;        ///< パースされた合計配列数
  uint64_t total_strings = 0;       ///< パースされた合計文字列数
  uint64_t total_numbers = 0;       ///< パースされた合計数値数
  uint64_t total_booleans = 0;      ///< パースされた合計ブール値数
  uint64_t total_nulls = 0;         ///< パースされた合計null値数
  uint64_t max_depth_reached = 0;   ///< 到達した最大ネスト深度
  uint64_t parse_time_ns = 0;       ///< パース処理時間（ナノ秒）
  uint64_t total_string_bytes = 0;  ///< パースされた文字列の合計バイト数

  /**
   * @brief 統計情報をリセットする
   */
  void reset() {
    total_tokens = 0;
    total_objects = 0;
    total_arrays = 0;
    total_strings = 0;
    total_numbers = 0;
    total_booleans = 0;
    total_nulls = 0;
    max_depth_reached = 0;
    parse_time_ns = 0;
    total_string_bytes = 0;
  }
};

/**
 * @brief JSONエラーの位置情報
 */
struct JsonErrorPosition {
  size_t line = 1;    ///< エラーが発生した行
  size_t column = 1;  ///< エラーが発生した列
  size_t offset = 0;  ///< 入力文字列内のオフセット

  /**
   * @brief 位置情報を更新する
   * @param c 処理された文字
   */
  void update(char c) {
    offset++;
    if (c == '\n') {
      line++;
      column = 1;
    } else {
      column++;
    }
  }

  /**
   * @brief 位置情報をリセットする
   */
  void reset() {
    line = 1;
    column = 1;
    offset = 0;
  }
};

/**
 * @brief JSONパース例外クラス
 */
class JsonParseError : public std::runtime_error {
 public:
  /**
   * @brief JSON解析エラーを構築する
   * @param message エラーメッセージ
   * @param position エラーの位置情報
   */
  JsonParseError(const std::string& message, const JsonErrorPosition& position)
      : std::runtime_error(message), position_(position) {
  }

  /**
   * @brief エラーの位置情報を取得する
   * @return エラーの位置情報
   */
  const JsonErrorPosition& position() const {
    return position_;
  }

  /**
   * @brief 書式付きエラーメッセージを取得する
   * @return 行と列を含む書式付きエラーメッセージ
   */
  std::string formattedMessage() const {
    return what() + std::string(" at line ") + std::to_string(position_.line) +
           ", column " + std::to_string(position_.column);
  }

 private:
  JsonErrorPosition position_;  ///< エラーの位置情報
};

// 前方宣言
class JsonValue;
class JsonObject;
class JsonArray;

/**
 * @brief JSONの値型を表す列挙型
 */
enum class JsonValueType {
  Null,     ///< JSON null値
  Boolean,  ///< JSON真偽値（true/false）
  Number,   ///< JSON数値（整数または浮動小数点数）
  String,   ///< JSON文字列
  Array,    ///< JSON配列
  Object    ///< JSONオブジェクト
};

/**
 * @brief JSON値クラス
 *
 * このクラスはJSONの任意の値を表現するためのクラスです。
 * 値の型に応じて異なる表現を保持します。
 */
class JsonValue {
 public:
  /**
   * @brief null値のJSONValueを構築する
   */
  JsonValue()
      : type_(JsonValueType::Null) {
  }

  /**
   * @brief ブール値のJSONValueを構築する
   * @param value ブール値
   */
  JsonValue(bool value)
      : type_(JsonValueType::Boolean), bool_value_(value) {
  }

  /**
   * @brief 整数値のJSONValueを構築する
   * @param value 整数値
   */
  JsonValue(int value)
      : type_(JsonValueType::Number), number_value_(static_cast<double>(value)), is_integer_(true) {
  }

  /**
   * @brief 64ビット整数値のJSONValueを構築する
   * @param value 64ビット整数値
   */
  JsonValue(int64_t value)
      : type_(JsonValueType::Number), number_value_(static_cast<double>(value)), is_integer_(true) {
  }

  /**
   * @brief 浮動小数点値のJSONValueを構築する
   * @param value 浮動小数点値
   */
  JsonValue(double value)
      : type_(JsonValueType::Number), number_value_(value), is_integer_(false) {
  }

  /**
   * @brief 文字列のJSONValueを構築する
   * @param value 文字列値
   */
  JsonValue(const std::string& value)
      : type_(JsonValueType::String), string_value_(value) {
  }

  /**
   * @brief 文字列のJSONValueを構築する（右辺値参照）
   * @param value 文字列値
   */
  JsonValue(std::string&& value)
      : type_(JsonValueType::String), string_value_(std::move(value)) {
  }

  /**
   * @brief 配列のJSONValueを構築する
   * @param array 配列値
   */
  JsonValue(const std::vector<JsonValue>& array);

  /**
   * @brief 配列のJSONValueを構築する（右辺値参照）
   * @param array 配列値
   */
  JsonValue(std::vector<JsonValue>&& array);

  /**
   * @brief オブジェクトのJSONValueを構築する
   * @param object オブジェクト値
   */
  JsonValue(const std::unordered_map<std::string, JsonValue>& object);

  /**
   * @brief オブジェクトのJSONValueを構築する（右辺値参照）
   * @param object オブジェクト値
   */
  JsonValue(std::unordered_map<std::string, JsonValue>&& object);

  /**
   * @brief コピーコンストラクタ
   * @param other コピー元のJsonValue
   */
  JsonValue(const JsonValue& other) = default;

  /**
   * @brief ムーブコンストラクタ
   * @param other ムーブ元のJsonValue
   */
  JsonValue(JsonValue&& other) = default;

  /**
   * @brief コピー代入演算子
   * @param other コピー元のJsonValue
   * @return 自身への参照
   */
  JsonValue& operator=(const JsonValue& other) = default;

  /**
   * @brief ムーブ代入演算子
   * @param other ムーブ元のJsonValue
   * @return 自身への参照
   */
  JsonValue& operator=(JsonValue&& other) = default;

  /**
   * @brief デストラクタ
   */
  ~JsonValue() = default;

  /**
   * @brief 値の型を取得する
   * @return JSON値の型
   */
  JsonValueType type() const {
    return type_;
  }

  /**
   * @brief 値がnullかどうかを確認する
   * @return nullの場合はtrue、それ以外はfalse
   */
  bool isNull() const {
    return type_ == JsonValueType::Null;
  }

  /**
   * @brief 値がブール値かどうかを確認する
   * @return ブール値の場合はtrue、それ以外はfalse
   */
  bool isBool() const {
    return type_ == JsonValueType::Boolean;
  }

  /**
   * @brief 値が数値かどうかを確認する
   * @return 数値の場合はtrue、それ以外はfalse
   */
  bool isNumber() const {
    return type_ == JsonValueType::Number;
  }

  /**
   * @brief 値が整数値かどうかを確認する
   * @return 整数値の場合はtrue、それ以外はfalse
   */
  bool isInteger() const {
    return type_ == JsonValueType::Number && is_integer_;
  }

  /**
   * @brief 値が文字列かどうかを確認する
   * @return 文字列の場合はtrue、それ以外はfalse
   */
  bool isString() const {
    return type_ == JsonValueType::String;
  }

  /**
   * @brief 値が配列かどうかを確認する
   * @return 配列の場合はtrue、それ以外はfalse
   */
  bool isArray() const {
    return type_ == JsonValueType::Array;
  }

  /**
   * @brief 値がオブジェクトかどうかを確認する
   * @return オブジェクトの場合はtrue、それ以外はfalse
   */
  bool isObject() const {
    return type_ == JsonValueType::Object;
  }

  /**
   * @brief ブール値を取得する
   * @return ブール値
   * @throws std::runtime_error 値がブール値でない場合
   */
  bool asBool() const;

  /**
   * @brief 整数値を取得する
   * @return 整数値
   * @throws std::runtime_error 値が数値でない場合
   */
  int64_t asInt() const;

  /**
   * @brief 浮動小数点値を取得する
   * @return 浮動小数点値
   * @throws std::runtime_error 値が数値でない場合
   */
  double asDouble() const;

  /**
   * @brief 文字列を取得する
   * @return 文字列
   * @throws std::runtime_error 値が文字列でない場合
   */
  const std::string& asString() const;

  /**
   * @brief 配列を取得する
   * @return 配列
   * @throws std::runtime_error 値が配列でない場合
   */
  const std::vector<JsonValue>& asArray() const;

  /**
   * @brief 配列を取得する（変更可能）
   * @return 配列への参照
   * @throws std::runtime_error 値が配列でない場合
   */
  std::vector<JsonValue>& asArray();

  /**
   * @brief オブジェクトを取得する
   * @return オブジェクト
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  const std::unordered_map<std::string, JsonValue>& asObject() const;

  /**
   * @brief オブジェクトを取得する（変更可能）
   * @return オブジェクトへの参照
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  std::unordered_map<std::string, JsonValue>& asObject();

  /**
   * @brief インデックスを使用して配列の要素にアクセスする
   * @param index アクセスするインデックス
   * @return 要素への参照
   * @throws std::runtime_error 値が配列でない場合、またはインデックスが範囲外の場合
   */
  const JsonValue& operator[](size_t index) const;

  /**
   * @brief インデックスを使用して配列の要素にアクセスする（変更可能）
   * @param index アクセスするインデックス
   * @return 要素への参照
   * @throws std::runtime_error 値が配列でない場合、またはインデックスが範囲外の場合
   */
  JsonValue& operator[](size_t index);

  /**
   * @brief キーを使用してオブジェクトの要素にアクセスする
   * @param key アクセスするキー
   * @return 要素への参照
   * @throws std::runtime_error 値がオブジェクトでない場合、またはキーが存在しない場合
   */
  const JsonValue& operator[](const std::string& key) const;

  /**
   * @brief キーを使用してオブジェクトの要素にアクセスする（変更可能）
   * @param key アクセスするキー
   * @return 要素への参照
   * @throws std::runtime_error 値がオブジェクトでない場合
   * @note キーが存在しない場合は新しいnull値が作成される
   */
  JsonValue& operator[](const std::string& key);

  /**
   * @brief オブジェクトにキーが存在するかどうかを確認する
   * @param key 確認するキー
   * @return キーが存在する場合はtrue、それ以外の場合はfalse
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  bool hasKey(const std::string& key) const;

  /**
   * @brief キーを使用してオブジェクトから値を取得する
   * @param key 取得するキー
   * @return キーに関連付けられた値
   * @throws std::runtime_error 値がオブジェクトでない場合、またはキーが存在しない場合
   */
  const JsonValue& get(const std::string& key) const;

  /**
   * @brief キーを使用してオブジェクトから値を取得する（オプショナル）
   * @param key 取得するキー
   * @return キーに関連付けられた値、またはキーが存在しない場合はstd::nullopt
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  std::optional<std::reference_wrapper<const JsonValue>> getOptional(const std::string& key) const;

  /**
   * @brief インデックスを使用して配列から値を取得する
   * @param index 取得するインデックス
   * @return インデックスに関連付けられた値
   * @throws std::runtime_error 値が配列でない場合、またはインデックスが範囲外の場合
   */
  const JsonValue& get(size_t index) const;

  /**
   * @brief インデックスを使用して配列から値を取得する（オプショナル）
   * @param index 取得するインデックス
   * @return インデックスに関連付けられた値、またはインデックスが範囲外の場合はstd::nullopt
   * @throws std::runtime_error 値が配列でない場合
   */
  std::optional<std::reference_wrapper<const JsonValue>> getOptional(size_t index) const;

  /**
   * @brief 配列のサイズを取得する
   * @return 配列のサイズ
   * @throws std::runtime_error 値が配列でない場合
   */
  size_t size() const;

  /**
   * @brief JSONオブジェクトのキー数を取得する
   * @return オブジェクトのキー数
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  size_t count() const;

  /**
   * @brief JSONオブジェクトのすべてのキーを取得する
   * @return キーのリスト
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  std::vector<std::string> keys() const;

  /**
   * @brief JSONオブジェクトのすべての値を取得する
   * @return 値のリスト
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  std::vector<JsonValue> values() const;

  /**
   * @brief 値をJSON文字列に変換する
   * @param pretty 整形出力するかどうか
   * @param indent インデントレベル（整形出力の場合に使用）
   * @return JSON文字列
   */
  std::string toString(bool pretty = false, int indent = 0) const;

  /**
   * @brief 値が等しいかどうかを比較する
   * @param other 比較対象の値
   * @return 等しい場合はtrue、それ以外はfalse
   */
  bool operator==(const JsonValue& other) const;

  /**
   * @brief 値が等しくないかどうかを比較する
   * @param other 比較対象の値
   * @return 等しくない場合はtrue、それ以外はfalse
   */
  bool operator!=(const JsonValue& other) const {
    return !(*this == other);
  }

  /**
   * @brief 配列に値を追加する
   * @param value 追加する値
   * @throws std::runtime_error 値が配列でない場合
   */
  void push(const JsonValue& value);

  /**
   * @brief 配列に値を追加する（右辺値参照）
   * @param value 追加する値
   * @throws std::runtime_error 値が配列でない場合
   */
  void push(JsonValue&& value);

  /**
   * @brief オブジェクトに値を設定する
   * @param key 設定するキー
   * @param value 設定する値
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  void set(const std::string& key, const JsonValue& value);

  /**
   * @brief オブジェクトに値を設定する（右辺値参照）
   * @param key 設定するキー
   * @param value 設定する値
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  void set(const std::string& key, JsonValue&& value);

  /**
   * @brief オブジェクトからキーを削除する
   * @param key 削除するキー
   * @return 削除されたかどうか（キーが存在した場合はtrue）
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  bool remove(const std::string& key);

  /**
   * @brief 配列からインデックスの要素を削除する
   * @param index 削除するインデックス
   * @throws std::runtime_error 値が配列でない場合、またはインデックスが範囲外の場合
   */
  void removeAt(size_t index);

  /**
   * @brief 配列をクリアする
   * @throws std::runtime_error 値が配列でない場合
   */
  void clearArray();

  /**
   * @brief オブジェクトをクリアする
   * @throws std::runtime_error 値がオブジェクトでない場合
   */
  void clearObject();

 private:
  JsonValueType type_ = JsonValueType::Null;  ///< 値の型

  // 値のストレージ（型に応じて使用される）
  union {
    bool bool_value_;      ///< ブール値
    double number_value_;  ///< 数値
  };

  bool is_integer_ = false;                                  ///< 数値が整数かどうか
  std::string string_value_;                                 ///< 文字列値
  std::vector<JsonValue> array_value_;                       ///< 配列値
  std::unordered_map<std::string, JsonValue> object_value_;  ///< オブジェクト値
};

/**
 * @brief JSONパーサークラス
 *
 * このクラスはJSON文字列を解析してJsonValue構造を生成します。
 */
class JsonParser {
 public:
  /**
   * @brief デフォルトオプションでJSONパーサーを構築する
   */
  JsonParser()
      : options_(JsonParserOptions()) {
  }

  /**
   * @brief 指定されたオプションでJSONパーサーを構築する
   * @param options パーサーオプション
   */
  explicit JsonParser(const JsonParserOptions& options)
      : options_(options) {
  }

  /**
   * @brief JSON文字列を解析する
   * @param json 解析するJSON文字列
   * @return 解析されたJsonValue
   * @throws JsonParseError 解析エラーが発生した場合
   */
  JsonValue parse(const std::string& json);

  /**
   * @brief JSON文字列を解析する（非スローバージョン）
   * @param json 解析するJSON文字列
   * @param success 成功フラグ（出力）
   * @return 解析されたJsonValue、エラー時はnull値
   */
  JsonValue tryParse(const std::string& json, bool& success);

  /**
   * @brief JSON文字列を検証する
   * @param json 検証するJSON文字列
   * @return 有効な場合はtrue、それ以外はfalse
   */
  bool validate(const std::string& json);

  /**
   * @brief パーサー統計情報を取得する
   * @return パーサー統計情報
   */
  const JsonParserStats& getStats() const {
    return stats_;
  }

  /**
   * @brief パーサー統計情報をリセットする
   */
  void resetStats() {
    stats_.reset();
  }

  /**
   * @brief 値をJSON文字列に変換する
   * @param value 変換する値
   * @param pretty 整形出力するかどうか
   * @return JSON文字列
   */
  static std::string stringify(const JsonValue& value, bool pretty = false);

  /**
   * @brief 値をJSONファイルに書き込む
   * @param value 書き込む値
   * @param filename 書き込むファイル名
   * @param pretty 整形出力するかどうか
   * @return 成功した場合はtrue、それ以外はfalse
   */
  static bool writeToFile(const JsonValue& value, const std::string& filename, bool pretty = false);

  /**
   * @brief JSONファイルから値を読み込む
   * @param filename 読み込むファイル名
   * @param options パーサーオプション
   * @return 読み込まれた値
   * @throws JsonParseError 解析エラーが発生した場合
   * @throws std::runtime_error ファイルが開けない場合
   */
  static JsonValue readFromFile(const std::string& filename, const JsonParserOptions& options = JsonParserOptions());

  /**
   * @brief JSONファイルから値を読み込む（非スローバージョン）
   * @param filename 読み込むファイル名
   * @param success 成功フラグ（出力）
   * @param options パーサーオプション
   * @return 読み込まれた値、エラー時はnull値
   */
  static JsonValue tryReadFromFile(const std::string& filename, bool& success, const JsonParserOptions& options = JsonParserOptions());

 private:
  JsonParserOptions options_;   ///< パーサーオプション
  JsonParserStats stats_;       ///< パーサー統計情報
  JsonErrorPosition position_;  ///< 現在の解析位置
  std::string current_json_;    ///< 現在処理中のJSON文字列
  size_t current_pos_ = 0;      ///< 現在の解析位置
  size_t json_length_ = 0;      ///< JSON文字列の長さ
  uint32_t current_depth_ = 0;  ///< 現在のネスト深度

  /**
   * @brief 解析状態をリセットする
   */
  void reset();

  /**
   * @brief 空白文字をスキップする
   */
  void skipWhitespace();

  /**
   * @brief コメントをスキップする
   * @return コメントがスキップされた場合はtrue、それ以外はfalse
   */
  bool skipComment();

  /**
   * @brief JSONの値を解析する
   * @return 解析された値
   */
  JsonValue parseValue();

  /**
   * @brief nullリテラルを解析する
   * @return null値
   */
  JsonValue parseNull();

  /**
   * @brief 真偽値リテラルを解析する
   * @return 真偽値
   */
  JsonValue parseBoolean();

  /**
   * @brief 数値リテラルを解析する
   * @return 数値
   */
  JsonValue parseNumber();

  /**
   * @brief 文字列リテラルを解析する
   * @return 文字列
   */
  JsonValue parseString();

  /**
   * @brief 配列リテラルを解析する
   * @return 配列
   */
  JsonValue parseArray();

  /**
   * @brief オブジェクトリテラルを解析する
   * @return オブジェクト
   */
  JsonValue parseObject();

  /**
   * @brief 文字列リテラルを解析する
   * @param is_object_key オブジェクトキーかどうか
   * @return 解析された文字列
   */
  std::string parseStringLiteral(bool is_object_key = false);

  /**
   * @brief エスケープシーケンスを解析する
   * @param result 結果文字列
   */
  void parseEscapeSequence(std::string& result);

  /**
   * @brief ユニコードエスケープシーケンスを解析する
   * @param result 結果文字列
   */
  void parseUnicodeEscape(std::string& result);

  /**
   * @brief 次の文字を確認する
   * @return 次の文字（EOFの場合は'\0'）
   */
  char peek() const;

  /**
   * @brief 現在の文字を取得して、次の文字に進む
   * @return 現在の文字
   */
  char advance();

  /**
   * @brief 特定の文字が次の文字かどうかを確認し、一致すれば進める
   * @param expected 期待する文字
   * @return 一致した場合はtrue、それ以外はfalse
   */
  bool match(char expected);

  /**
   * @brief 現在の位置から特定の文字列が続くかどうかを確認し、一致すれば進める
   * @param expected 期待する文字列
   * @return 一致した場合はtrue、それ以外はfalse
   */
  bool matchString(const std::string& expected);

  /**
   * @brief 解析エラーを生成する
   * @param message エラーメッセージ
   * @throws JsonParseError 常にスロー
   */
  [[noreturn]] void error(const std::string& message);

  /**
   * @brief EOFに達したかどうかを確認する
   * @return EOFに達した場合はtrue、それ以外はfalse
   */
  bool isAtEnd() const;

  /**
   * @brief 深度カウンタを増加させる
   * @throws JsonParseError 最大深度を超えた場合
   */
  void incrementDepth();

  /**
   * @brief 深度カウンタを減少させる
   */
  void decrementDepth();

  /**
   * @brief 現在の文字が数字かどうかを確認する
   * @return 数字の場合はtrue、それ以外はfalse
   */
  bool isDigit(char c) const;

  /**
   * @brief 現在の文字が16進数かどうかを確認する
   * @return 16進数の場合はtrue、それ以外はfalse
   */
  bool isHexDigit(char c) const;
};

}  // namespace json
}  // namespace parser
}  // namespace aero

#endif  // AERO_JSON_PARSER_H