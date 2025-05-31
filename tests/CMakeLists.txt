cmake_minimum_required(VERSION 3.24)

# テストライブラリの設定
find_package(GTest QUIET)
if(NOT GTest_FOUND)
    # Google Testが見つからない場合は、FetchContentで取得
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/03597a01ee50f33f9142a332e68d798063b2dee5.zip
    )
    # Windows用の設定
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# テスト用のインクルードディレクトリ
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/tests
)

# コアテスト
add_executable(test_core
    core/test_engine.cpp
    core/test_context.cpp
    core/test_value.cpp
)

target_link_libraries(test_core
    PRIVATE
    AeroJSCore
    gtest_main
    gtest
)

# メモリ管理テスト
add_executable(test_memory
    core/test_memory_allocator.cpp
    core/test_memory_pool.cpp
    core/test_garbage_collector.cpp
)

target_link_libraries(test_memory
    PRIVATE
    AeroJSCore
    gtest_main
    gtest
)

# ユーティリティテスト
add_executable(test_utils
    utils/test_timer.cpp
    utils/test_containers.cpp
)

target_link_libraries(test_utils
    PRIVATE
    AeroJSCore
    gtest_main
    gtest
)

# パフォーマンステスト
add_executable(test_performance
    performance/benchmark_engine.cpp
    performance/benchmark_memory.cpp
    performance/benchmark_gc.cpp
)

target_link_libraries(test_performance
    PRIVATE
    AeroJSCore
    gtest_main
    gtest
)

# 統合テスト
add_executable(test_integration
    integration/test_full_engine.cpp
    integration/test_javascript_execution.cpp
)

target_link_libraries(test_integration
    PRIVATE
    AeroJSCore
    gtest_main
    gtest
)

# テストの登録
add_test(NAME CoreTests COMMAND test_core)
add_test(NAME MemoryTests COMMAND test_memory)
add_test(NAME UtilsTests COMMAND test_utils)
add_test(NAME PerformanceTests COMMAND test_performance)
add_test(NAME IntegrationTests COMMAND test_integration)

# テストのプロパティ設定
set_tests_properties(CoreTests PROPERTIES TIMEOUT 30)
set_tests_properties(MemoryTests PROPERTIES TIMEOUT 60)
set_tests_properties(UtilsTests PROPERTIES TIMEOUT 30)
set_tests_properties(PerformanceTests PROPERTIES TIMEOUT 120)
set_tests_properties(IntegrationTests PROPERTIES TIMEOUT 180)

# カバレッジ設定（GCC/Clangの場合）
if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
    target_compile_options(test_core PRIVATE --coverage)
    target_link_options(test_core PRIVATE --coverage)
    
    target_compile_options(test_memory PRIVATE --coverage)
    target_link_options(test_memory PRIVATE --coverage)
    
    target_compile_options(test_utils PRIVATE --coverage)
    target_link_options(test_utils PRIVATE --coverage)
endif()

# カスタムテストターゲット
add_custom_target(run_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_core test_memory test_utils test_performance test_integration
    COMMENT "Running all tests"
)

# メモリリークチェック用ターゲット（Valgrind）
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    add_custom_target(test_memcheck
        COMMAND ${VALGRIND_PROGRAM} --tool=memcheck --leak-check=full --show-leak-kinds=all 
                --track-origins=yes --verbose $<TARGET_FILE:test_core>
        DEPENDS test_core
        COMMENT "Running memory leak check"
    )
endif()

# ベンチマーク用ターゲット
add_custom_target(benchmark
    COMMAND $<TARGET_FILE:test_performance>
    DEPENDS test_performance
    COMMENT "Running performance benchmarks"
) 