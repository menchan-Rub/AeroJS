/**
 * @file dummy.cpp
 * @brief AeroJS ダミーファイル
 * @version 0.1.0
 * @license MIT
 */

#include "simple_value.h"
#include "simple_engine.h"

// 最小限のダミー実装
namespace aerojs {
namespace core {

void dummy_function() {
    // 基本的な機能のテスト
    SimpleEngine engine;
    
    // 基本的な値の作成
    SimpleValue val1 = SimpleValue::fromNumber(42.0);
    SimpleValue val2 = SimpleValue::fromString("Hello, AeroJS!");
    SimpleValue val3 = SimpleValue::fromBoolean(true);
    
    // 変数の設定
    engine.setVariable("x", val1);
    engine.setVariable("message", val2);
    engine.setVariable("flag", val3);
    
    // 簡単な評価
    SimpleValue result = engine.evaluate("42");
}

} // namespace core
} // namespace aerojs 