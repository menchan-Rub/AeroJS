cmake_minimum_required(VERSION 3.24)
project(AeroJS VERSION 0.1.0 LANGUAGES CXX)

## C++20を使用
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# CMakePresets 対応
if(EXISTS ${CMAKE_SOURCE_DIR}/CMakePresets.json)
  include(CMakePresets)
endif()
#
# Sanitizer ビルドターゲット
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
if(ENABLE_SANITIZERS)
  if(NOT MSVC)
    add_compile_options(-fsanitize=address,undefined,thread -fno-omit-frame-pointer)
    link_libraries(-fsanitize=address,undefined,thread)
  endif()
endif()

# ビルドタイプが設定されていない場合はリリースとする
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# コンパイルオプション
if(MSVC)
  # MSVCの場合の警告レベル設定
  add_compile_options(/W4 /WX)
else()
  # GCC/Clangの場合の警告レベル設定
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ライブラリとヘッダーのディレクトリ
include_directories(
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src
)

# AeroJSのコアライブラリ
add_library(AeroJSCore
  src/core/runtime/values/symbol.cpp
  # 他のソースファイルをここに追加
)

#------------------------------------------------------------------------------
# AsmJit (AArch64 コード生成ライブラリ) の外部プロジェクトとして追加
#------------------------------------------------------------------------------
include(ExternalProject)
ExternalProject_Add(
  asmjit
  GIT_REPOSITORY https://github.com/asmjit/asmjit.git
  GIT_TAG master
  PREFIX ${CMAKE_BINARY_DIR}/third_party/asmjit
  CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/asmjit_install
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
)
ExternalProject_Get_Property(asmjit install_dir)
set(ASMJIT_INCLUDE_DIR ${install_dir}/include)
set(ASMJIT_LIB_DIR ${install_dir}/lib)
# AsmJit インターフェースターゲット
add_library(AsmJit INTERFACE)
target_include_directories(AsmJit INTERFACE ${ASMJIT_INCLUDE_DIR})
target_link_libraries(AsmJit INTERFACE ${ASMJIT_LIB_DIR}/libasmjit.a)
# AeroJSJIT を asmjit ビルドに依存させ、リンクする
add_dependencies(AeroJSJIT asmjit)
target_link_libraries(AeroJSJIT PUBLIC AsmJit)

#------------------------------------------------------------------------------
# JIT ライブラリの追加
#------------------------------------------------------------------------------
file(GLOB_RECURSE JIT_SOURCE_FILES
  "${CMAKE_SOURCE_DIR}/src/core/jit/*.cpp"
)
add_library(AeroJSJIT STATIC ${JIT_SOURCE_FILES})
target_include_directories(AeroJSJIT PUBLIC ${CMAKE_SOURCE_DIR}/src/core/jit)
target_link_libraries(AeroJSJIT PUBLIC AeroJSCore)

# テストのビルドオプション
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# ドキュメント生成オプション
option(BUILD_DOCS "Build documentation" OFF)
if(BUILD_DOCS)
  find_package(Doxygen)
  if(DOXYGEN_FOUND)
    add_subdirectory(docs)
  endif()
endif()

# インストール設定
install(TARGETS AeroJSCore
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

# ヘッダーファイルのインストール
install(DIRECTORY include/ DESTINATION include/aerojs)

# パッケージ設定
include(CPack)
set(CPACK_PACKAGE_VENDOR "AeroJS Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "AeroJS JavaScript Engine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

#------------------------------------------------------------------------------
# clang-formatによるJITコード整形ターゲット (常に定義)
#------------------------------------------------------------------------------
add_custom_target(format-jit
  COMMAND find ${CMAKE_SOURCE_DIR}/src/core/jit -type f \( -name "*.cpp" -o -name "*.h" \) -print0 |
          xargs -0 clang-format -i
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Run clang-format on all JIT source files"
  VERBATIM
)
