cmake_minimum_required(VERSION 3.14)
project(AeroJS VERSION 0.1.0 LANGUAGES CXX)

# C++17を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
