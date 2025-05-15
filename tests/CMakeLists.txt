cmake_minimum_required(VERSION 3.14)
project(AeroJSTests)

# GoogleTestをダウンロードしてビルドする
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# 共通設定
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# コア機能テスト
add_executable(
  core_value_tests
  core/runtime/values/value_test.cpp
  core/runtime/values/symbol_test.cpp
)

# JIT基本機能テスト
add_executable(
  jit_compiler_tests
  jit/jit_compiler_test.cpp
)

# 最適化JITテスト
add_executable(
  optimizing_jit_tests
  jit/optimizing_jit_test.cpp
)

# IRグラフテスト
add_executable(
  ir_graph_tests
  jit/ir/ir_graph_test.cpp
)

# 型プロファイルテスト
add_executable(
  type_info_tests
  jit/profiler/type_info_test.cpp
)

# メタトレーシングJITテスト
add_executable(
  metatracing_jit_tests
  jit/metatracing/trace_recorder_test.cpp
)

# 統合テスト
add_executable(
  integration_tests
  integration/engine_integration_test.cpp
)

# パフォーマンステスト
add_executable(
  performance_tests
  performance/jit_performance_test.cpp
  performance/value_performance_test.cpp
)

# リンクするライブラリを指定
target_link_libraries(
  core_value_tests
  GTest::gtest_main
  AeroJSCore
)

target_link_libraries(
  jit_compiler_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  optimizing_jit_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  ir_graph_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  type_info_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  metatracing_jit_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  integration_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

target_link_libraries(
  performance_tests
  GTest::gtest_main
  AeroJSCore
  AeroJSJIT
)

# テスト自動発見
include(GoogleTest)
gtest_discover_tests(core_value_tests)
gtest_discover_tests(jit_compiler_tests)
gtest_discover_tests(optimizing_jit_tests)
gtest_discover_tests(ir_graph_tests)
gtest_discover_tests(type_info_tests)
gtest_discover_tests(metatracing_jit_tests)
gtest_discover_tests(integration_tests)
gtest_discover_tests(performance_tests)

# カスタムターゲット: すべてのテスト実行
add_custom_target(run_all_tests
  COMMAND core_value_tests
  COMMAND jit_compiler_tests
  COMMAND optimizing_jit_tests
  COMMAND ir_graph_tests
  COMMAND type_info_tests
  COMMAND metatracing_jit_tests
  COMMAND integration_tests
  COMMAND performance_tests
  DEPENDS 
    core_value_tests
    jit_compiler_tests
    optimizing_jit_tests
    ir_graph_tests
    type_info_tests
    metatracing_jit_tests
    integration_tests
    performance_tests
  COMMENT "Run all tests in sequence"
)

# テストカバレッジ設定（オプション）
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)
if(ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # GCCまたはClangでカバレッジを有効化
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    
    # カバレッジレポート生成用のターゲット
    find_program(GCOVR_PATH gcovr)
    if(GCOVR_PATH)
      add_custom_target(coverage
        COMMAND ${GCOVR_PATH} --xml-pretty --exclude-unreachable-branches --print-summary -o coverage.xml --root ${CMAKE_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating coverage report"
        DEPENDS run_all_tests
      )
    endif()
  endif()
endif() 