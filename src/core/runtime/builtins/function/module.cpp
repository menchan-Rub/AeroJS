/**
 * @file module.cpp
 * @brief JavaScript の Function モジュール登録
 * @copyright 2023 AeroJS Project
 */

#include "function.h"
#include "core/runtime/global_object.h"

namespace aero {

void registerFunctionModule(GlobalObject* global) {
    // Function 組み込みオブジェクトを登録
    registerFunctionBuiltin(global);
}

} // namespace aero 