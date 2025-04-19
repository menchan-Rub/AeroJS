cmake_minimum_required(VERSION 3.14)
project(AeroJSTests)

# GoogleTestをダウンロードしてビルドする
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# テスト用の実行ファイルを生成
add_executable(
  symbol_tests
  core/runtime/values/symbol_test.cpp
)

# リンクするライブラリを指定
target_link_libraries(
  symbol_tests
  GTest::gtest_main
  AeroJSCore  # AeroJSのメインプロジェクトのライブラリ名（実際のプロジェクト設定に合わせて変更してください）
)

# テストを自動的に登録
include(GoogleTest)
gtest_discover_tests(symbol_tests)

# テスト実行時のオプション
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 