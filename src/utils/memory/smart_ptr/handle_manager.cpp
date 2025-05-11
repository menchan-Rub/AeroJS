/**
 * @file handle_manager.cpp
 * @brief WeakHandleの管理を行うHandleManagerクラスの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "handle_manager.h"
#include "../../../core/runtime/object.h"

namespace aero {

// HandleManagerの非テンプレートメソッドの実装

// 将来的な拡張のためのダミー実装
void HandleManager::registerWeakHandle(void* handle) {
  // 将来的にはWeakHandleごとの個別登録を実装する可能性がある
  // 現在は何もしない
}

} // namespace aero 