/**
 * @file type_info_test.cpp
 * @brief 型プロファイリングシステムのテスト
 * @version 1.0.0
 * @license MIT
 */

#include "../../../src/core/jit/profiler/type_info.h"
#include "../../../src/core/runtime/values/value.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>

using namespace aerojs::core;
using namespace aerojs::core::jit::profiler;
using namespace aerojs::core::runtime;

// テストフィクスチャ
class TypeInfoTest : public ::testing::Test {
protected:
  void SetUp() override {
    // 各テスト前に型情報を新規作成
    typeInfo = std::make_unique<TypeInfo>();
  }
  
  void TearDown() override {
    typeInfo.reset();
  }
  
  std::unique_ptr<TypeInfo> typeInfo;
};

// 基本的な型情報記録テスト
TEST_F(TypeInfoTest, BasicTypeRecording) {
  // 初期状態確認
  EXPECT_EQ(TypeCategory::Uninitialized, typeInfo->getCategory());
  EXPECT_EQ(0, typeInfo->getTypeCount());
  
  // 数値型を記録
  typeInfo->recordType(Value::createNumber(42.0));
  
  // 記録後の検証
  EXPECT_EQ(TypeCategory::Monomorphic, typeInfo->getCategory());
  EXPECT_EQ(1, typeInfo->getTypeCount());
  EXPECT_EQ(ValueType::Number, typeInfo->getMostCommonType());
  EXPECT_FLOAT_EQ(1.0, typeInfo->getTypeRatio(ValueType::Number));
  
  // 別の型（ブール値）を記録
  typeInfo->recordType(Value::createBoolean(true));
  
  // ポリモーフィック状態への移行を検証
  EXPECT_EQ(TypeCategory::Polymorphic, typeInfo->getCategory());
  EXPECT_EQ(2, typeInfo->getTypeCount());
  
  // 最も一般的な型は依然として数値型のはず（1回目の記録）
  EXPECT_EQ(ValueType::Number, typeInfo->getMostCommonType());
  
  // 型の割合を検証
  EXPECT_FLOAT_EQ(0.5, typeInfo->getTypeRatio(ValueType::Number));
  EXPECT_FLOAT_EQ(0.5, typeInfo->getTypeRatio(ValueType::Boolean));
  EXPECT_FLOAT_EQ(0.0, typeInfo->getTypeRatio(ValueType::String));
}

// モノモーフィック/ポリモーフィック判定テスト
TEST_F(TypeInfoTest, TypeCategoryDetection) {
  // 空の状態
  EXPECT_EQ(TypeCategory::Uninitialized, typeInfo->getCategory());
  
  // 単一型（モノモーフィック）
  typeInfo->recordType(Value::createNumber(1.0));
  EXPECT_EQ(TypeCategory::Monomorphic, typeInfo->getCategory());
  EXPECT_TRUE(typeInfo->isMonomorphic());
  EXPECT_FALSE(typeInfo->isPolymorphic());
  EXPECT_FALSE(typeInfo->isMegamorphic());
  
  // 2種類の型（ポリモーフィック）
  typeInfo->recordType(Value::createBoolean(true));
  EXPECT_EQ(TypeCategory::Polymorphic, typeInfo->getCategory());
  EXPECT_FALSE(typeInfo->isMonomorphic());
  EXPECT_TRUE(typeInfo->isPolymorphic());
  EXPECT_FALSE(typeInfo->isMegamorphic());
  
  // 3種類の型（ポリモーフィック）
  typeInfo->recordType(Value::createNull());
  EXPECT_EQ(TypeCategory::Polymorphic, typeInfo->getCategory());
  EXPECT_FALSE(typeInfo->isMonomorphic());
  EXPECT_TRUE(typeInfo->isPolymorphic());
  EXPECT_FALSE(typeInfo->isMegamorphic());
  
  // 4種類の型（ポリモーフィック）
  typeInfo->recordType(Value::createUndefined());
  EXPECT_EQ(TypeCategory::Polymorphic, typeInfo->getCategory());
  
  // 5種類の型（メガモーフィック）
  typeInfo->recordType(ValueType::String);
  EXPECT_EQ(TypeCategory::MegaMorphic, typeInfo->getCategory());
  EXPECT_FALSE(typeInfo->isMonomorphic());
  EXPECT_FALSE(typeInfo->isPolymorphic());
  EXPECT_TRUE(typeInfo->isMegamorphic());
}

// 型特化チェックテスト
TEST_F(TypeInfoTest, TypeSpecializationChecks) {
  // 常に整数型
  for (int i = 0; i < 10; ++i) {
    typeInfo->recordType(Value::createNumber(static_cast<double>(i)));
  }
  
  EXPECT_TRUE(typeInfo->isAlwaysNumber());
  EXPECT_TRUE(typeInfo->isAlwaysInt32());
  EXPECT_TRUE(typeInfo->isMostlyNumber());
  EXPECT_TRUE(typeInfo->isMostlyInt32());
  
  // 95%整数型、5%ブール型
  for (int i = 0; i < 190; ++i) {
    typeInfo->recordType(Value::createNumber(static_cast<double>(i)));
  }
  
  for (int i = 0; i < 10; ++i) {
    typeInfo->recordType(Value::createBoolean(i % 2 == 0));
  }
  
  EXPECT_FALSE(typeInfo->isAlwaysNumber());
  EXPECT_FALSE(typeInfo->isAlwaysInt32());
  EXPECT_TRUE(typeInfo->isMostlyNumber());  // 95%は数値型
  EXPECT_TRUE(typeInfo->isMostlyInt32());   // 95%は整数型
  
  // 50%整数型、50%浮動小数点型
  typeInfo = std::make_unique<TypeInfo>();
  
  for (int i = 0; i < 50; ++i) {
    typeInfo->recordType(Value::createNumber(static_cast<double>(i)));  // 整数
  }
  
  for (int i = 0; i < 50; ++i) {
    typeInfo->recordType(Value::createNumber(i + 0.5));  // 浮動小数点
  }
  
  EXPECT_TRUE(typeInfo->isAlwaysNumber());   // すべて数値型
  EXPECT_FALSE(typeInfo->isAlwaysInt32());   // 整数型だけではない
  EXPECT_TRUE(typeInfo->isMostlyNumber());   // すべて数値型
  EXPECT_FALSE(typeInfo->isMostlyInt32());   // 整数型は50%のみ
}

// 型安定性テスト
TEST_F(TypeInfoTest, TypeStability) {
  // 安定した型（常に同じ型）
  for (int i = 0; i < 100; ++i) {
    typeInfo->recordType(Value::createNumber(static_cast<double>(i)));
  }
  
  // 高い安定性を期待
  EXPECT_GT(typeInfo->getStability(), 0.9f);
  
  // 不安定な型（交互に異なる型）
  typeInfo = std::make_unique<TypeInfo>();
  
  for (int i = 0; i < 100; ++i) {
    if (i % 2 == 0) {
      typeInfo->recordType(Value::createNumber(static_cast<double>(i)));
    } else {
      typeInfo->recordType(Value::createBoolean(i % 4 == 1));
    }
  }
  
  // 低い安定性を期待
  EXPECT_LT(typeInfo->getStability(), 0.5f);
}

// 信頼度テスト
TEST_F(TypeInfoTest, ConfidenceTest) {
  // 少数のサンプル
  typeInfo->recordType(Value::createNumber(1.0));
  typeInfo->recordType(Value::createNumber(2.0));
  
  // 低い信頼度を期待
  EXPECT_LT(typeInfo->getConfidence(), 0.5f);
  
  // 多数のサンプル
  for (int i = 0; i < 98; ++i) {
    typeInfo->recordType(Value::createNumber(static_cast<double>(i)));
  }
  
  // 高い信頼度を期待
  EXPECT_GT(typeInfo->getConfidence(), 0.9f);
}

// オブジェクト形状テスト
TEST_F(ObjectShapeTest, BasicShapeOperations) {
  ObjectShape shape;
  shape.setId(1);
  
  // プロパティ追加
  ObjectShape::Property prop1("x", ValueType::Number, false);
  ObjectShape::Property prop2("y", ValueType::Number, false);
  ObjectShape::Property prop3("name", ValueType::String, true);
  
  shape.addProperty(prop1);
  shape.addProperty(prop2);
  shape.addProperty(prop3);
  
  // プロパティ存在確認
  EXPECT_TRUE(shape.hasProperty("x"));
  EXPECT_TRUE(shape.hasProperty("y"));
  EXPECT_TRUE(shape.hasProperty("name"));
  EXPECT_FALSE(shape.hasProperty("z"));
  
  // プロパティ情報取得
  const auto* xProp = shape.getProperty("x");
  EXPECT_NE(nullptr, xProp);
  EXPECT_EQ("x", xProp->name);
  EXPECT_EQ(ValueType::Number, xProp->type);
  EXPECT_FALSE(xProp->isConstant);
  
  const auto* nameProp = shape.getProperty("name");
  EXPECT_NE(nullptr, nameProp);
  EXPECT_EQ("name", nameProp->name);
  EXPECT_EQ(ValueType::String, nameProp->type);
  EXPECT_TRUE(nameProp->isConstant);
  
  // フラグ設定と確認
  EXPECT_FALSE(shape.hasFlag(ObjectShape::IsArray));
  shape.setFlag(ObjectShape::IsArray, true);
  EXPECT_TRUE(shape.hasFlag(ObjectShape::IsArray));
  
  shape.setFlag(ObjectShape::HasNamedProps, true);
  EXPECT_TRUE(shape.hasFlag(ObjectShape::HasNamedProps));
  
  shape.setFlag(ObjectShape::HasNamedProps, false);
  EXPECT_FALSE(shape.hasFlag(ObjectShape::HasNamedProps));
}

// 形状互換性テスト
TEST_F(ObjectShapeTest, ShapeCompatibility) {
  ObjectShape shape1;
  shape1.setId(1);
  shape1.addProperty({"x", ValueType::Number});
  shape1.addProperty({"y", ValueType::Number});
  
  ObjectShape shape2;
  shape2.setId(2);
  shape2.addProperty({"x", ValueType::Number});
  shape2.addProperty({"y", ValueType::Number});
  
  // 同じプロパティを持つ形状は互換性がある
  EXPECT_TRUE(shape1.isCompatibleWith(shape2));
  EXPECT_TRUE(shape2.isCompatibleWith(shape1));
  
  // 異なるプロパティを追加
  shape2.addProperty({"z", ValueType::Number});
  
  // shape1はshape2と互換性があるが、shape2はshape1と互換性がない
  EXPECT_TRUE(shape1.isCompatibleWith(shape2));  // shape1のすべてのプロパティはshape2にある
  EXPECT_FALSE(shape2.isCompatibleWith(shape1)); // shape2の'z'プロパティはshape1にない
  
  // 型が異なるプロパティを追加
  ObjectShape shape3;
  shape3.setId(3);
  shape3.addProperty({"x", ValueType::String});  // 異なる型
  shape3.addProperty({"y", ValueType::Number});
  
  // 型が異なるので互換性がない
  EXPECT_FALSE(shape1.isCompatibleWith(shape3));
  EXPECT_FALSE(shape3.isCompatibleWith(shape1));
  
  // 類似度チェック
  EXPECT_FLOAT_EQ(1.0f, shape1.similarityWith(shape1));  // 自身との類似度は1.0
  EXPECT_LT(0.5f, shape1.similarityWith(shape2));        // 部分的に類似
  EXPECT_GT(0.5f, shape1.similarityWith(shape3));        // あまり類似していない
}

// 呼び出しサイト型情報テスト
TEST_F(CallSiteTypeInfoTest, CallSiteRecording) {
  CallSiteTypeInfo callSiteInfo;
  
  // 初期状態確認
  EXPECT_EQ(0u, callSiteInfo.getCallCount());
  EXPECT_FALSE(callSiteInfo.isHot());
  EXPECT_FLOAT_EQ(0.0f, callSiteInfo.getSuccessRatio());
  EXPECT_FLOAT_EQ(0.0f, callSiteInfo.getExceptionRatio());
  
  // 引数型と戻り値型の記録
  std::vector<Value> args = {
    Value::createNumber(42.0),
    Value::createString(nullptr), // スタブ: 実際はStringオブジェクトが必要
    Value::createBoolean(true)
  };
  
  callSiteInfo.recordArgTypes(args);
  callSiteInfo.recordReturnType(Value::createNumber(84.0));
  callSiteInfo.recordSuccess();
  
  // 成功した呼び出しの記録を検証
  EXPECT_EQ(1u, callSiteInfo.getCallCount());
  EXPECT_FLOAT_EQ(1.0f, callSiteInfo.getSuccessRatio());
  EXPECT_FLOAT_EQ(0.0f, callSiteInfo.getExceptionRatio());
  
  // 引数型情報を検証
  const auto& argTypeInfos = callSiteInfo.getArgTypeInfos();
  EXPECT_EQ(3, argTypeInfos.size());
  EXPECT_EQ(ValueType::Number, argTypeInfos[0].getMostCommonType());
  EXPECT_EQ(ValueType::String, argTypeInfos[1].getMostCommonType());
  EXPECT_EQ(ValueType::Boolean, argTypeInfos[2].getMostCommonType());
  
  // 戻り値型情報を検証
  const auto& returnTypeInfo = callSiteInfo.getReturnTypeInfo();
  EXPECT_EQ(ValueType::Number, returnTypeInfo.getMostCommonType());
  
  // 例外を含む追加の呼び出しを記録
  callSiteInfo.recordArgTypes(args);
  callSiteInfo.recordException();
  
  // 例外を含む呼び出し統計を検証
  EXPECT_EQ(2u, callSiteInfo.getCallCount());
  EXPECT_FLOAT_EQ(0.5f, callSiteInfo.getSuccessRatio());
  EXPECT_FLOAT_EQ(0.5f, callSiteInfo.getExceptionRatio());
  
  // ホットかどうかの判定（閾値は実装依存）
  EXPECT_FALSE(callSiteInfo.isHot());  // デフォルト閾値は10
  
  // さらに呼び出しを記録してホットにする
  for (int i = 0; i < 10; ++i) {
    callSiteInfo.recordArgTypes(args);
    callSiteInfo.recordReturnType(Value::createNumber(i * 2.0));
    callSiteInfo.recordSuccess();
  }
  
  EXPECT_TRUE(callSiteInfo.isHot());  // 呼び出し回数が閾値を超えた
}

// 型プロファイラテスト
TEST_F(TypeProfilerTest, BasicProfilerOperations) {
  TypeProfiler profiler;
  
  // 初期状態
  EXPECT_FALSE(profiler.isEnabled());
  EXPECT_EQ(0u, profiler.getTotalTypeObservations());
  EXPECT_EQ(0u, profiler.getShapeCount());
  
  // プロファイラを有効化
  profiler.enable();
  EXPECT_TRUE(profiler.isEnabled());
  
  // オブジェクト形状の記録
  auto shape = std::make_unique<ObjectShape>();
  shape->setId(1);
  shape->addProperty({"x", ValueType::Number});
  shape->addProperty({"y", ValueType::Number});
  
  profiler.recordObjectShape(1, *shape);
  
  // 形状の取得と検証
  const auto* retrievedShape = profiler.getObjectShape(1);
  EXPECT_NE(nullptr, retrievedShape);
  EXPECT_EQ(1u, retrievedShape->getId());
  EXPECT_TRUE(retrievedShape->hasProperty("x"));
  EXPECT_TRUE(retrievedShape->hasProperty("y"));
  
  // 存在しない形状IDでの取得
  EXPECT_EQ(nullptr, profiler.getObjectShape(999));
  
  // 変数型情報
  auto* varTypeInfo = profiler.getOrCreateVarTypeInfo(100, 0);
  EXPECT_NE(nullptr, varTypeInfo);
  
  varTypeInfo->recordType(Value::createNumber(42.0));
  
  // 変数型情報の取得と検証
  const auto* retrievedVarTypeInfo = profiler.getVarTypeInfo(100, 0);
  EXPECT_NE(nullptr, retrievedVarTypeInfo);
  EXPECT_EQ(ValueType::Number, retrievedVarTypeInfo->getMostCommonType());
  
  // プロパティ型情報
  auto* propTypeInfo = profiler.getOrCreatePropertyTypeInfo(1, "x");
  EXPECT_NE(nullptr, propTypeInfo);
  
  propTypeInfo->recordType(Value::createNumber(42.0));
  
  // プロパティ型情報の取得と検証
  const auto* retrievedPropTypeInfo = profiler.getPropertyTypeInfo(1, "x");
  EXPECT_NE(nullptr, retrievedPropTypeInfo);
  EXPECT_EQ(ValueType::Number, retrievedPropTypeInfo->getMostCommonType());
  
  // 呼び出しサイト型情報
  auto* callSiteInfo = profiler.getOrCreateCallSiteTypeInfo(100, 50);
  EXPECT_NE(nullptr, callSiteInfo);
  
  std::vector<Value> args = {Value::createNumber(42.0)};
  callSiteInfo->recordArgTypes(args);
  callSiteInfo->recordReturnType(Value::createNumber(84.0));
  callSiteInfo->recordSuccess();
  
  // 呼び出しサイト型情報の取得と検証
  const auto* retrievedCallSiteInfo = profiler.getCallSiteTypeInfo(100, 50);
  EXPECT_NE(nullptr, retrievedCallSiteInfo);
  EXPECT_EQ(1u, retrievedCallSiteInfo->getCallCount());
  EXPECT_EQ(ValueType::Number, retrievedCallSiteInfo->getArgTypeInfos()[0].getMostCommonType());
  
  // コレクションサイズ予測
  profiler.recordCollectionSize(100, 60, 5);
  profiler.recordCollectionSize(100, 60, 7);
  profiler.recordCollectionSize(100, 60, 6);
  
  uint32_t predictedSize = profiler.predictCollectionSize(100, 60);
  EXPECT_GE(predictedSize, 5u);
  EXPECT_LE(predictedSize, 7u);
  
  // ホット関数判定
  EXPECT_FALSE(profiler.isHotFunction(100)); // 呼び出し回数が少ない関数
  
  // プロファイル情報のエクスポート/インポート
  std::string profileData = profiler.exportTypeProfile();
  EXPECT_FALSE(profileData.empty());
  
  TypeProfiler newProfiler;
  bool imported = newProfiler.importTypeProfile(profileData);
  EXPECT_TRUE(imported);
  
  // プロファイルデータのクリア
  profiler.clearFunction(100);
  EXPECT_EQ(nullptr, profiler.getVarTypeInfo(100, 0)); // クリア後はnullptrが返るはず
  
  profiler.clearAll();
  EXPECT_EQ(nullptr, profiler.getObjectShape(1)); // すべてクリアされたので形状も取得できないはず
}

// メインテスト実行
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
} 