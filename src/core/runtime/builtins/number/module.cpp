/**
 * @file module.cpp
 * @brief JavaScript の Number 組み込みオブジェクトのモジュール登録
 */

#include "number.h"
#include "../../global_object.h"

namespace aerojs {
namespace core {

/**
 * @brief Number 組み込みオブジェクトをグローバルオブジェクトに登録
 * 
 * @param global グローバルオブジェクト
 */
void registerNumberBuiltin(GlobalObject* global) {
    if (!global) {
        return;
    }
    
    // Number コンストラクタをグローバルオブジェクトに登録
    global->set("Number", Number::getConstructor(), PropertyAttribute::Writable | PropertyAttribute::Configurable);
}

} // namespace core
} // namespace aerojs 