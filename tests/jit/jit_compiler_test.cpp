#include <gtest/gtest.h>
#include "../../src/core/jit/jit_compiler.h"
#include "../../src/core/jit/baseline/baseline_jit.h"
#include <vector>
#include <cstdint>

using namespace aerojs::core;

// 空のバイトコード列を渡した場合、nullptr が返り、サイズは0になることを検証
TEST(JITCompilerTest, BaselineCompileEmpty) {
  BaselineJIT jit;
  size_t outSize = 0;
  auto code = jit.Compile({}, outSize);
  EXPECT_EQ(code, nullptr);
  EXPECT_EQ(outSize, 0);
}

// 非空のバイトコード列を渡した場合、同一内容のマシンコードが返ることを検証
TEST(JITCompilerTest, BaselineCompileNonEmpty) {
  BaselineJIT jit;
  std::vector<uint8_t> bytecodes = {1, 2, 3, 4, 5};
  size_t outSize = 0;
  auto code = jit.Compile(bytecodes, outSize);
  ASSERT_NE(code, nullptr);
  EXPECT_EQ(outSize, bytecodes.size());
  for (size_t i = 0; i < outSize; ++i) {
    EXPECT_EQ(code.get()[i], bytecodes[i]);
  }
} 