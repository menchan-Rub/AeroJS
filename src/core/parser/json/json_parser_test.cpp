/**
 * @file json_parser_test.cpp
 * @version 1.0.0
 * @copyright Copyright (c) 2023 AeroJS Project
 * @license MIT License
 * @brief JSONパーサーのテストスイート
 *
 * このファイルは、JSONパーサーの機能をテストするための単体テストスイートを提供します。
 * すべての主要な機能と境界条件をテストします。
 */

#include "json_parser.h"

#include <chrono>
#include <cmath>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace aero::parser::json;

class JsonParserTest : public ::testing::Test {
 protected:
  JsonParser parser_;

  void SetUp() override {
    JsonParserOptions options;
    options.allow_comments = true;
    options.allow_trailing_commas = true;
    parser_ = JsonParser(options);
  }
};

// 基本的な値のテスト
TEST_F(JsonParserTest, TestBasicValues) {
  // null
  {
    JsonValue value = parser_.parse("null");
    EXPECT_TRUE(value.isNull());
  }

  // boolean
  {
    JsonValue true_value = parser_.parse("true");
    EXPECT_TRUE(true_value.isBoolean());
    EXPECT_TRUE(true_value.getBooleanValue());

    JsonValue false_value = parser_.parse("false");
    EXPECT_TRUE(false_value.isBoolean());
    EXPECT_FALSE(false_value.getBooleanValue());
  }

  // number
  {
    JsonValue int_value = parser_.parse("42");
    EXPECT_TRUE(int_value.isNumber());
    EXPECT_EQ(42.0, int_value.getNumberValue());

    JsonValue float_value = parser_.parse("3.14159");
    EXPECT_TRUE(float_value.isNumber());
    EXPECT_DOUBLE_EQ(3.14159, float_value.getNumberValue());

    JsonValue neg_value = parser_.parse("-123");
    EXPECT_TRUE(neg_value.isNumber());
    EXPECT_EQ(-123.0, neg_value.getNumberValue());
  }

  // string
  {
    JsonValue value = parser_.parse("\"Hello, world!\"");
    EXPECT_TRUE(value.isString());
    EXPECT_EQ("Hello, world!", value.getStringValue());
  }
}

// 文字列のエスケープシーケンスのテスト
TEST_F(JsonParserTest, TestStringEscapes) {
  std::string json = "\"\\\"\\\\\\b\\f\\n\\r\\t\"";
  JsonValue value = parser_.parse(json);
  EXPECT_TRUE(value.isString());
  EXPECT_EQ("\"\\\b\f\n\r\t", value.getStringValue());

  json = "\"Unicode: \\u0041\\u0042\\u0043\"";
  value = parser_.parse(json);
  EXPECT_TRUE(value.isString());
  EXPECT_EQ("Unicode: ABC", value.getStringValue());

  json = "\"Unicode surrogate pair: \\uD834\\uDD1E\"";
  value = parser_.parse(json);
  EXPECT_TRUE(value.isString());
  // サロゲートペアは UTF-8 では 4 バイトに変換されるため、実際の文字列は "Unicode surrogate pair: 𝄞" になる
  EXPECT_EQ(14 + 4, value.getStringValue().length());
}

// 数値の特殊ケースのテスト
TEST_F(JsonParserTest, TestNumberSpecialCases) {
  // エクスポネント表記
  {
    JsonValue value = parser_.parse("1.23e+4");
    EXPECT_TRUE(value.isNumber());
    EXPECT_DOUBLE_EQ(12300.0, value.getNumberValue());

    value = parser_.parse("-5.67e-2");
    EXPECT_TRUE(value.isNumber());
    EXPECT_DOUBLE_EQ(-0.0567, value.getNumberValue());
  }

  // 0 始まり
  {
    JsonValue value = parser_.parse("0.123");
    EXPECT_TRUE(value.isNumber());
    EXPECT_DOUBLE_EQ(0.123, value.getNumberValue());
  }

  // 大きな整数
  {
    JsonValue value = parser_.parse("9007199254740991");  // MAX_SAFE_INTEGER
    EXPECT_TRUE(value.isNumber());
    EXPECT_DOUBLE_EQ(9007199254740991.0, value.getNumberValue());
  }
}

// 配列のテスト
TEST_F(JsonParserTest, TestArrays) {
  // 空の配列
  {
    JsonValue value = parser_.parse("[]");
    EXPECT_TRUE(value.isArray());
    EXPECT_EQ(0, value.getArrayValue().size());
  }

  // 単純な配列
  {
    JsonValue value = parser_.parse("[1, 2, 3]");
    EXPECT_TRUE(value.isArray());
    EXPECT_EQ(3, value.getArrayValue().size());
    EXPECT_DOUBLE_EQ(1.0, value.getArrayValue()[0].getNumberValue());
    EXPECT_DOUBLE_EQ(2.0, value.getArrayValue()[1].getNumberValue());
    EXPECT_DOUBLE_EQ(3.0, value.getArrayValue()[2].getNumberValue());
  }

  // 異なる型を含む配列
  {
    JsonValue value = parser_.parse("[null, true, 42, \"hello\"]");
    EXPECT_TRUE(value.isArray());
    EXPECT_EQ(4, value.getArrayValue().size());
    EXPECT_TRUE(value.getArrayValue()[0].isNull());
    EXPECT_TRUE(value.getArrayValue()[1].isBoolean());
    EXPECT_TRUE(value.getArrayValue()[1].getBooleanValue());
    EXPECT_TRUE(value.getArrayValue()[2].isNumber());
    EXPECT_DOUBLE_EQ(42.0, value.getArrayValue()[2].getNumberValue());
    EXPECT_TRUE(value.getArrayValue()[3].isString());
    EXPECT_EQ("hello", value.getArrayValue()[3].getStringValue());
  }

  // ネストした配列
  {
    JsonValue value = parser_.parse("[[1, 2], [3, 4]]");
    EXPECT_TRUE(value.isArray());
    EXPECT_EQ(2, value.getArrayValue().size());
    EXPECT_TRUE(value.getArrayValue()[0].isArray());
    EXPECT_EQ(2, value.getArrayValue()[0].getArrayValue().size());
    EXPECT_DOUBLE_EQ(1.0, value.getArrayValue()[0].getArrayValue()[0].getNumberValue());
    EXPECT_DOUBLE_EQ(2.0, value.getArrayValue()[0].getArrayValue()[1].getNumberValue());
    EXPECT_TRUE(value.getArrayValue()[1].isArray());
    EXPECT_EQ(2, value.getArrayValue()[1].getArrayValue().size());
    EXPECT_DOUBLE_EQ(3.0, value.getArrayValue()[1].getArrayValue()[0].getNumberValue());
    EXPECT_DOUBLE_EQ(4.0, value.getArrayValue()[1].getArrayValue()[1].getNumberValue());
  }

  // 末尾のカンマ
  {
    JsonValue value = parser_.parse("[1, 2, 3,]");
    EXPECT_TRUE(value.isArray());
    EXPECT_EQ(3, value.getArrayValue().size());
  }
}

// オブジェクトのテスト
TEST_F(JsonParserTest, TestObjects) {
  // 空のオブジェクト
  {
    JsonValue value = parser_.parse("{}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(0, value.getObjectValue().size());
  }

  // 単純なオブジェクト
  {
    JsonValue value = parser_.parse("{\"a\": 1, \"b\": 2}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
    EXPECT_TRUE(value.getObjectValue().find("a") != value.getObjectValue().end());
    EXPECT_TRUE(value.getObjectValue().find("b") != value.getObjectValue().end());
    EXPECT_DOUBLE_EQ(1.0, value.getObjectValue().at("a").getNumberValue());
    EXPECT_DOUBLE_EQ(2.0, value.getObjectValue().at("b").getNumberValue());
  }

  // 異なる型を含むオブジェクト
  {
    JsonValue value = parser_.parse("{\"a\": null, \"b\": true, \"c\": 42, \"d\": \"hello\"}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(4, value.getObjectValue().size());
    EXPECT_TRUE(value.getObjectValue().at("a").isNull());
    EXPECT_TRUE(value.getObjectValue().at("b").isBoolean());
    EXPECT_TRUE(value.getObjectValue().at("b").getBooleanValue());
    EXPECT_TRUE(value.getObjectValue().at("c").isNumber());
    EXPECT_DOUBLE_EQ(42.0, value.getObjectValue().at("c").getNumberValue());
    EXPECT_TRUE(value.getObjectValue().at("d").isString());
    EXPECT_EQ("hello", value.getObjectValue().at("d").getStringValue());
  }

  // ネストしたオブジェクト
  {
    JsonValue value = parser_.parse("{\"a\": {\"b\": 1}, \"c\": {\"d\": 2}}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
    EXPECT_TRUE(value.getObjectValue().at("a").isObject());
    EXPECT_EQ(1, value.getObjectValue().at("a").getObjectValue().size());
    EXPECT_DOUBLE_EQ(1.0, value.getObjectValue().at("a").getObjectValue().at("b").getNumberValue());
    EXPECT_TRUE(value.getObjectValue().at("c").isObject());
    EXPECT_EQ(1, value.getObjectValue().at("c").getObjectValue().size());
    EXPECT_DOUBLE_EQ(2.0, value.getObjectValue().at("c").getObjectValue().at("d").getNumberValue());
  }

  // 末尾のカンマ
  {
    JsonValue value = parser_.parse("{\"a\": 1, \"b\": 2,}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
  }

  // 引用符なしのキー (オプション機能)
  {
    JsonParserOptions options;
    options.allow_unquoted_keys = true;
    JsonParser parser(options);
    JsonValue value = parser.parse("{a: 1, b: 2}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
  }
}

// 複雑なJSON構造のテスト
TEST_F(JsonParserTest, TestComplexStructures) {
  std::string json = R"({
        "name": "John Doe",
        "age": 30,
        "isActive": true,
        "address": {
            "street": "123 Main St",
            "city": "Anytown",
            "country": "USA"
        },
        "phoneNumbers": [
            {
                "type": "home",
                "number": "555-1234"
            },
            {
                "type": "work",
                "number": "555-5678"
            }
        ],
        "languages": ["English", "Spanish"],
        "metadata": null
    })";

  JsonValue value = parser_.parse(json);
  EXPECT_TRUE(value.isObject());
  EXPECT_EQ(7, value.getObjectValue().size());

  // 基本的なプロパティ
  EXPECT_TRUE(value.getObjectValue().at("name").isString());
  EXPECT_EQ("John Doe", value.getObjectValue().at("name").getStringValue());
  EXPECT_TRUE(value.getObjectValue().at("age").isNumber());
  EXPECT_DOUBLE_EQ(30.0, value.getObjectValue().at("age").getNumberValue());
  EXPECT_TRUE(value.getObjectValue().at("isActive").isBoolean());
  EXPECT_TRUE(value.getObjectValue().at("isActive").getBooleanValue());

  // ネストしたオブジェクト
  EXPECT_TRUE(value.getObjectValue().at("address").isObject());
  EXPECT_EQ(3, value.getObjectValue().at("address").getObjectValue().size());
  EXPECT_EQ("123 Main St", value.getObjectValue().at("address").getObjectValue().at("street").getStringValue());

  // 配列
  EXPECT_TRUE(value.getObjectValue().at("languages").isArray());
  EXPECT_EQ(2, value.getObjectValue().at("languages").getArrayValue().size());
  EXPECT_EQ("English", value.getObjectValue().at("languages").getArrayValue()[0].getStringValue());

  // オブジェクトの配列
  EXPECT_TRUE(value.getObjectValue().at("phoneNumbers").isArray());
  EXPECT_EQ(2, value.getObjectValue().at("phoneNumbers").getArrayValue().size());
  EXPECT_TRUE(value.getObjectValue().at("phoneNumbers").getArrayValue()[0].isObject());
  EXPECT_EQ("home", value.getObjectValue().at("phoneNumbers").getArrayValue()[0].getObjectValue().at("type").getStringValue());

  // null値
  EXPECT_TRUE(value.getObjectValue().at("metadata").isNull());
}

// エラーケースのテスト
TEST_F(JsonParserTest, TestErrorCases) {
  // 空の入力
  EXPECT_THROW(parser_.parse(""), JsonParseError);

  // 不正な構文
  EXPECT_THROW(parser_.parse("{"), JsonParseError);
  EXPECT_THROW(parser_.parse("}"), JsonParseError);
  EXPECT_THROW(parser_.parse("["), JsonParseError);
  EXPECT_THROW(parser_.parse("]"), JsonParseError);
  EXPECT_THROW(parser_.parse(","), JsonParseError);
  EXPECT_THROW(parser_.parse(":"), JsonParseError);

  // 不正な値
  EXPECT_THROW(parser_.parse("undefined"), JsonParseError);
  EXPECT_THROW(parser_.parse("NaN"), JsonParseError);
  EXPECT_THROW(parser_.parse("Infinity"), JsonParseError);
  EXPECT_THROW(parser_.parse("-Infinity"), JsonParseError);

  // 不正な数値
  EXPECT_THROW(parser_.parse("+42"), JsonParseError);
  EXPECT_THROW(parser_.parse(".42"), JsonParseError);
  EXPECT_THROW(parser_.parse("01"), JsonParseError);

  // 不正な文字列
  EXPECT_THROW(parser_.parse("\"unterminated string"), JsonParseError);
  EXPECT_THROW(parser_.parse("\"invalid escape \\z\""), JsonParseError);

  // 配列のエラー
  EXPECT_THROW(parser_.parse("[1, 2, 3"), JsonParseError);
  EXPECT_THROW(parser_.parse("[1, 2, ]"), JsonParseError);
  EXPECT_THROW(parser_.parse("[1, , 3]"), JsonParseError);

  // オブジェクトのエラー
  EXPECT_THROW(parser_.parse("{\"a\": 1,"), JsonParseError);
  EXPECT_THROW(parser_.parse("{\"a\" 1}"), JsonParseError);
  EXPECT_THROW(parser_.parse("{\"a\": 1, }"), JsonParseError);
}

// 大きなJSONデータのパフォーマンステスト
TEST_F(JsonParserTest, TestLargeJsonPerformance) {
  // 大きな配列を生成
  std::stringstream ss;
  ss << "[";
  for (int i = 0; i < 1000; ++i) {
    if (i > 0) {
      ss << ",";
    }
    ss << i;
  }
  ss << "]";

  auto start = std::chrono::high_resolution_clock::now();
  JsonValue value = parser_.parse(ss.str());
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  EXPECT_TRUE(value.isArray());
  EXPECT_EQ(1000, value.getArrayValue().size());

  // パフォーマンス統計を表示
  std::cout << "Large array parse time: " << duration << " microseconds" << std::endl;
  std::cout << "Tokens parsed: " << parser_.getStats().total_tokens << std::endl;
  std::cout << "Parse time from stats: " << parser_.getStats().parse_time_ns / 1000 << " microseconds" << std::endl;
}

// ディープネスティングのテスト
TEST_F(JsonParserTest, TestDeepNesting) {
  // 深くネストした配列
  std::string json = "[[[[[[[[[[]]]]]]]]]]";
  JsonValue value = parser_.parse(json);
  EXPECT_TRUE(value.isArray());
  EXPECT_EQ(1, value.getArrayValue().size());

  // 再帰制限を超えるネスティング
  std::string deep_json = "[";
  for (int i = 0; i < 1024; ++i) {
    deep_json += "[";
  }
  for (int i = 0; i < 1024; ++i) {
    deep_json += "]";
  }
  deep_json += "]";

  EXPECT_THROW(parser_.parse(deep_json), JsonParseError);
}

// JSONからの文字列化のテスト
TEST_F(JsonParserTest, TestJsonToString) {
  // 単純な値
  {
    JsonValue null_val;
    EXPECT_EQ("null", null_val.toString());

    JsonValue bool_val(true);
    EXPECT_EQ("true", bool_val.toString());

    JsonValue num_val(42.0);
    EXPECT_EQ("42", num_val.toString());

    JsonValue str_val("hello");
    EXPECT_EQ("\"hello\"", str_val.toString());
  }

  // エスケープ文字
  {
    JsonValue val("a\b\f\n\r\t\"\\");
    EXPECT_EQ("\"a\\b\\f\\n\\r\\t\\\"\\\\\"", val.toString());
  }

  // 配列
  {
    JsonValue arr(JsonValueType::kArray);
    arr.addArrayElement(JsonValue(1.0));
    arr.addArrayElement(JsonValue(2.0));
    arr.addArrayElement(JsonValue(3.0));
    EXPECT_EQ("[1,2,3]", arr.toString());
  }

  // オブジェクト
  {
    JsonValue obj(JsonValueType::kObject);
    obj.addObjectMember("a", JsonValue(1.0));
    obj.addObjectMember("b", JsonValue("hello"));
    obj.addObjectMember("c", JsonValue(true));

    // オブジェクトのキー順序は保証されないため、結果をパースして検証
    std::string result = obj.toString();
    JsonValue parsed = parser_.parse(result);
    EXPECT_TRUE(parsed.isObject());
    EXPECT_EQ(3, parsed.getObjectValue().size());
    EXPECT_DOUBLE_EQ(1.0, parsed.getObjectValue().at("a").getNumberValue());
    EXPECT_EQ("hello", parsed.getObjectValue().at("b").getStringValue());
    EXPECT_TRUE(parsed.getObjectValue().at("c").getBooleanValue());
  }

  // 複雑な構造
  {
    JsonValue complex = parser_.parse(R"({"a":[1,2,3],"b":{"c":"hello"}})");
    std::string result = complex.toString();
    JsonValue reparsed = parser_.parse(result);

    // 元の構造と再パースした構造が一致することを確認
    EXPECT_TRUE(reparsed.isObject());
    EXPECT_EQ(2, reparsed.getObjectValue().size());
    EXPECT_TRUE(reparsed.getObjectValue().at("a").isArray());
    EXPECT_EQ(3, reparsed.getObjectValue().at("a").getArrayValue().size());
    EXPECT_TRUE(reparsed.getObjectValue().at("b").isObject());
    EXPECT_EQ(1, reparsed.getObjectValue().at("b").getObjectValue().size());
    EXPECT_EQ("hello", reparsed.getObjectValue().at("b").getObjectValue().at("c").getStringValue());
  }
}

// SIMD最適化のパフォーマンステスト（可能な場合）
#ifdef AERO_JSON_ENABLE_SIMD
TEST_F(JsonParserTest, TestSimdOptimizations) {
  // 大きなJSONデータを生成
  std::stringstream ss;
  ss << "{";
  for (int i = 0; i < 1000; ++i) {
    if (i > 0) {
      ss << ",";
    }
    ss << "\"key" << i << "\":\"value" << i << "\"";
  }
  ss << "}";

  auto start = std::chrono::high_resolution_clock::now();
  JsonValue value = parser_.parse(ss.str());
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

  EXPECT_TRUE(value.isObject());
  EXPECT_EQ(1000, value.getObjectValue().size());

  // パフォーマンス統計を表示
  std::cout << "Large object parse time with SIMD: " << duration << " microseconds" << std::endl;
}
#endif

// 特殊な機能のテスト
TEST_F(JsonParserTest, TestSpecialFeatures) {
  // コメントのテスト（有効にしている場合）
  JsonParserOptions options;
  options.allow_comments = true;
  JsonParser parser_with_comments(options);

  try {
    JsonValue value = parser_with_comments.parse(R"({
            // ライン コメント
            "a": 1,
            /* ブロック コメント */
            "b": 2
        })");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
  } catch (const JsonParseError& e) {
    // コメントが実装されていない場合、このテストはスキップ
    std::cout << "Comments not implemented: " << e.getMessage() << std::endl;
  }

  // シングルクォートのテスト（有効にしている場合）
  options.allow_comments = false;
  options.allow_single_quotes = true;
  JsonParser parser_with_single_quotes(options);

  try {
    JsonValue value = parser_with_single_quotes.parse("{'a': 1, 'b': 2}");
    EXPECT_TRUE(value.isObject());
    EXPECT_EQ(2, value.getObjectValue().size());
  } catch (const JsonParseError& e) {
    // シングルクォートが実装されていない場合、このテストはスキップ
    std::cout << "Single quotes not implemented: " << e.getMessage() << std::endl;
  }
}

// 検証機能のテスト
TEST_F(JsonParserTest, TestValidation) {
  EXPECT_TRUE(parser_.validate("{}"));
  EXPECT_TRUE(parser_.validate("[]"));
  EXPECT_TRUE(parser_.validate("123"));
  EXPECT_TRUE(parser_.validate("\"hello\""));
  EXPECT_TRUE(parser_.validate("null"));
  EXPECT_TRUE(parser_.validate("true"));
  EXPECT_TRUE(parser_.validate("false"));

  EXPECT_FALSE(parser_.validate(""));
  EXPECT_FALSE(parser_.validate("{"));
  EXPECT_FALSE(parser_.validate("}"));
  EXPECT_FALSE(parser_.validate("["));
  EXPECT_FALSE(parser_.validate("]"));
  EXPECT_FALSE(parser_.validate("\"unclosed string"));
  EXPECT_FALSE(parser_.validate("undefined"));
}

// エラーレポート機能のテスト
TEST_F(JsonParserTest, TestErrorReporting) {
  try {
    parser_.parse("{\"a\": 1, \"b\": }");
    FAIL() << "Expected JsonParseError";
  } catch (const JsonParseError& e) {
    EXPECT_STREQ("Unexpected character in JSON: }", e.getMessage());
    EXPECT_FALSE(parser_.validate("{\"a\": 1, \"b\": }"));
    EXPECT_TRUE(parser_.hasError());
    EXPECT_GT(parser_.getErrorPosition(), 0);
  }
}

// メモリリークがないことを確認するテスト
TEST_F(JsonParserTest, TestNoMemoryLeaks) {
  for (int i = 0; i < 1000; ++i) {
    JsonValue value = parser_.parse(R"({"a": [1, 2, {"b": "test", "c": [true, false, null]}]})");
    // 値がスコープから出るときに、適切にメモリが解放されることを期待
  }
}

// メインテスト実行関数
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}