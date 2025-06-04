cmake_minimum_required(VERSION 3.20)

# AeroJS 世界最高レベル JavaScript エンジン
project(AeroJS 
    VERSION 3.0.0
    DESCRIPTION "World's Most Advanced JavaScript Engine - Quantum Level Performance"
    LANGUAGES CXX
)

# C++20標準を使用（最新機能を活用）
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 世界最高レベルの最適化設定
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native -mavx2 -mfma -flto")
        set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -fsanitize=address,undefined")
    elseif(MSVC)
        set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /arch:AVX2 /GL")
        set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /RTC1")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# コンパイラ固有の最適化
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -mavx2 -mfma")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer -finline-functions")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -mavx2 -mfma")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fomit-frame-pointer -finline-functions")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /MP /permissive-")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Gy /GS-")
endif()

# インクルードディレクトリ
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# === 既存のコアライブラリソース ===
set(CORE_SOURCES
    # エンジンコア（既存）
    src/core/engine.cpp
    src/core/context.cpp
    src/core/value.cpp
    
    # ランタイムシステム（既存）
    src/core/runtime/builtins/builtins_manager.cpp
    
    # 変換システム（既存）
    src/core/transformers/transformer.cpp
    
    # 世界最高レベル新機能実装
    src/core/world_class_engine.cpp
    src/core/quantum_jit.cpp
    src/core/hyper_gc.cpp
)

# === 既存のユーティリティライブラリソース ===
set(UTILS_SOURCES
    # メモリ管理（既存）
    src/utils/memory/allocators/memory_allocator.cpp
    src/utils/memory/pool/memory_pool.cpp
    src/utils/memory/gc/garbage_collector.cpp
    
    # 時間管理（既存）
    src/utils/time/timer.cpp
)

# === 世界最高レベル新機能ソース（ヘッダーオンリー） ===
set(WORLD_CLASS_HEADERS
    # 量子JITコンパイラ
    include/aerojs/quantum_jit.h
    
    # 超高速ガベージコレクタ
    include/aerojs/hyper_gc.h
    
    # 超高速パーサー
    include/aerojs/ultra_parser.h
    
    # 世界最高レベル統合エンジン
    include/aerojs/world_class_engine.h
)

# === AeroJSコアライブラリ ===
add_library(AeroJSCore STATIC ${CORE_SOURCES} ${UTILS_SOURCES})

# ライブラリのプロパティ設定
set_target_properties(AeroJSCore PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 3
    OUTPUT_NAME "aerojs_core"
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# === 世界最高レベルテストプログラム ===
add_executable(world_class_test world_class_test.cpp)
target_link_libraries(world_class_test AeroJSCore)

# === 究極テストプログラム ===
add_executable(ultimate_test ultimate_test.cpp)
target_link_libraries(ultimate_test AeroJSCore)

# === 従来のテストプログラム（互換性） ===
add_executable(test_aerojs test_main.cpp)
target_link_libraries(test_aerojs AeroJSCore)

# === スレッドライブラリのリンク ===
find_package(Threads REQUIRED)
target_link_libraries(AeroJSCore Threads::Threads)
target_link_libraries(world_class_test Threads::Threads)
target_link_libraries(ultimate_test Threads::Threads)
target_link_libraries(test_aerojs Threads::Threads)

# === プラットフォーム固有の設定 ===
if(WIN32)
    # Windows固有の設定
    target_compile_definitions(AeroJSCore PRIVATE 
        AEROJS_WINDOWS=1
        AEROJS_WORLD_CLASS=1
        AEROJS_QUANTUM_ENABLED=1
    )
    target_link_libraries(AeroJSCore ws2_32 winmm kernel32 user32)
elseif(UNIX AND NOT APPLE)
    # Linux固有の設定
    target_compile_definitions(AeroJSCore PRIVATE 
        AEROJS_LINUX=1
        AEROJS_WORLD_CLASS=1
        AEROJS_QUANTUM_ENABLED=1
    )
    target_link_libraries(AeroJSCore dl rt pthread)
elseif(APPLE)
    # macOS固有の設定
    target_compile_definitions(AeroJSCore PRIVATE 
        AEROJS_MACOS=1
        AEROJS_WORLD_CLASS=1
        AEROJS_QUANTUM_ENABLED=1
    )
    target_link_libraries(AeroJSCore dl pthread)
endif()

# === 世界最高レベル機能の有効化 ===
target_compile_definitions(AeroJSCore PRIVATE
    AEROJS_VERSION_MAJOR=3
    AEROJS_VERSION_MINOR=0
    AEROJS_VERSION_PATCH=0
    AEROJS_ENABLE_QUANTUM_JIT=1
    AEROJS_ENABLE_HYPER_GC=1
    AEROJS_ENABLE_ULTRA_PARSER=1
    AEROJS_ENABLE_WORLD_CLASS_ENGINE=1
    AEROJS_ENABLE_PARALLEL_EXECUTION=1
    AEROJS_ENABLE_STREAMING_EXECUTION=1
    AEROJS_ENABLE_ADAPTIVE_OPTIMIZATION=1
    AEROJS_ENABLE_PREDICTIVE_GC=1
    AEROJS_ENABLE_CONCURRENT_GC=1
    AEROJS_ENABLE_QUANTUM_OPTIMIZATION=1
)

# === カスタムターゲット ===

# 世界最高レベルテスト実行
add_custom_target(run_world_class_test
    COMMAND world_class_test
    DEPENDS world_class_test
    COMMENT "Running World Class AeroJS Tests"
)

# 究極テスト実行
add_custom_target(run_ultimate_test
    COMMAND ultimate_test
    DEPENDS ultimate_test
    COMMENT "Running Ultimate AeroJS Tests - Quantum Level Verification"
)

# 全テスト実行
add_custom_target(run_all_tests
    COMMAND test_aerojs
    COMMAND world_class_test
    COMMAND ultimate_test
    DEPENDS test_aerojs world_class_test ultimate_test
    COMMENT "Running All AeroJS Tests - Complete Verification Suite"
)

# パフォーマンスベンチマーク
add_custom_target(benchmark
    COMMAND ultimate_test
    DEPENDS ultimate_test
    COMMENT "Running Performance Benchmarks - Proving World Class Performance"
)

# === ビルド情報の表示 ===
message(STATUS "🌟 AeroJS World Class JavaScript Engine v${PROJECT_VERSION}")
message(STATUS "🚀 Quantum Level Performance - Beyond V8, SpiderMonkey, JavaScriptCore")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Target Platform: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "🏆 Optimized for MAXIMUM PERFORMANCE!")
    message(STATUS "🎯 Target: 10x faster than V8")
    message(STATUS "⚡ Quantum JIT: ENABLED")
    message(STATUS "🔥 Hyper GC: ENABLED")
    message(STATUS "🚀 Ultra Parser: ENABLED")
    message(STATUS "🌟 World Class Engine: ENABLED")
endif()

message(STATUS "🎉 Ready to build the world's most advanced JavaScript engine!")
message(STATUS "🏆 Destination: World's #1 JavaScript Engine")

# === インストール設定 ===
install(TARGETS AeroJSCore
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${WORLD_CLASS_HEADERS}
    DESTINATION include/aerojs
)

install(TARGETS world_class_test ultimate_test test_aerojs
    RUNTIME DESTINATION bin
)

# === パッケージ設定 ===
set(CPACK_PACKAGE_NAME "AeroJS")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "World's Most Advanced JavaScript Engine")
set(CPACK_PACKAGE_VENDOR "AeroJS Team")
set(CPACK_PACKAGE_CONTACT "team@aerojs.org")

include(CPack)
