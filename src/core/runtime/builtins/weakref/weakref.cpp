/**
 * @file weakref.cpp
 * @brief JavaScriptのWeakRefオブジェクトの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "weakref.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include "../../exception/exception.h"
#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"
#include "../function/function.h"
#include "../../../utils/memory/gc/garbage_collector.h"
#include "../../../utils/memory/smart_ptr/handle_manager.h"

namespace aero {

// 静的メンバ変数の初期化
Object* WeakRefObject::s_prototype = nullptr;

// WeakRefObjectの実装
WeakRefObject::WeakRefObject(Value target, GlobalObject* globalObj)
    : Object(globalObj->weakRefPrototype()),
      m_globalObject(globalObj),
      m_targetAlive(false) {
  
  // ターゲットはオブジェクトでなければならない
  if (!target.isObject()) {
    throw TypeException("WeakRef target must be an object");
  }
  
  // ターゲットをセット
  setTarget(target);
}

WeakRefObject::~WeakRefObject() {
  // WeakHandleはGCによって自動的に管理されるため、
  // 明示的なクリーンアップは不要
}

// 弱参照しているターゲットを取得する
Value WeakRefObject::deref() const {
  // 最初に高速パスとしてアトミック変数をチェック
  if (!m_targetAlive.load(std::memory_order_acquire)) {
    return Value::undefined();
  }
  
  // 現在のGCフェーズ中にターゲットが収集される可能性があるため、
  // ミューテックスでロックしてターゲットの生存を確認
  std::lock_guard<std::mutex> lock(m_mutex);
  
  // WeakHandleからオブジェクトを取得
  Object* targetObj = m_target.get();
  
  if (targetObj) {
    return Value(targetObj);
  } else {
    // ターゲットが無効になった場合はフラグを更新
    m_targetAlive.store(false, std::memory_order_release);
    return Value::undefined();
  }
}

// ターゲットを設定する
void WeakRefObject::setTarget(Value target) {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  Object* obj = target.asObject();
  if (!obj) {
    throw TypeException("WeakRef target must be an object");
  }
  
  // グローバルのHandleManagerからWeakHandleを取得
  HandleManager* handleManager = HandleManager::getInstance();
  m_target = handleManager->createWeakHandle(obj);
  
  // ターゲットが生きていることを記録
  m_targetAlive.store(true, std::memory_order_release);
}

// ターゲットをクリアする
void WeakRefObject::clearTarget() {
  std::lock_guard<std::mutex> lock(m_mutex);
  m_target.reset();
  m_targetAlive.store(false, std::memory_order_release);
}

// ガベージコレクション開始前の処理
void WeakRefObject::preGCCallback(GarbageCollector* gc) {
  // GC前の準備：必要に応じてWeakHandleを追跡対象としてGCに登録
  gc->registerWeakHandle(&m_target);
}

// ガベージコレクション終了後の処理
void WeakRefObject::postGCCallback(GarbageCollector* gc) {
  // GC後の処理：ターゲットの生存状態を更新
  bool targetExists = m_target.get() != nullptr;
  m_targetAlive.store(targetExists, std::memory_order_release);
}

// WeakRefコンストラクタ関数
Value weakRefConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがWeakRefコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("WeakRef constructor requires new");
  }
  
  // 引数が与えられていない場合はエラー
  if (args.empty()) {
    throw TypeException("WeakRef constructor requires an object argument");
  }
  
  // 引数がオブジェクトでない場合はエラー
  if (!args[0].isObject()) {
    throw TypeException("WeakRef target must be an object");
  }
  
  // 新しいWeakRefオブジェクトを作成
  WeakRefObject* weakRef = new WeakRefObject(args[0], globalObj);
  return Value(weakRef);
}

// WeakRef.prototype.deref メソッド
Value weakRefDeref(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがWeakRefオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isWeakRef()) {
    throw TypeException("WeakRef.prototype.deref called on an object that is not a WeakRef");
  }
  
  WeakRefObject* weakRef = static_cast<WeakRefObject*>(thisObj);
  
  // ターゲットを取得
  return weakRef->deref();
}

// WeakRefプロトタイプを初期化
void initWeakRefPrototype(GlobalObject* globalObj) {
  // WeakRefプロトタイプオブジェクトがまだ存在しない場合、作成
  if (WeakRefObject::s_prototype == nullptr) {
    WeakRefObject::s_prototype = new Object(globalObj->objectPrototype());
    
    // プロトタイプメソッドを設定
    WeakRefObject::s_prototype->defineNativeFunction("deref", weakRefDeref, 0);
    
    // プロトタイプにクラス名を設定
    WeakRefObject::s_prototype->defineProperty(
        "constructor",
        PropertyDescriptor(Value(globalObj->weakRefConstructor()), nullptr, false, false, true));
    
    // toStringTagを設定
    WeakRefObject::s_prototype->defineProperty(
        globalObj->getSymbolRegistry()->getSymbol("toStringTag"),
        PropertyDescriptor(Value("WeakRef"), nullptr, false, false, true));
  }
}

// WeakRefオブジェクトを初期化
void initWeakRefObject(GlobalObject* globalObj) {
  // WeakRefプロトタイプを初期化
  initWeakRefPrototype(globalObj);
  
  // WeakRefコンストラクタを作成
  Object* weakRefConstructorObj = new Object(globalObj->functionPrototype());
  weakRefConstructorObj->setIsConstructor(true);
  
  // プロトタイプを設定
  weakRefConstructorObj->defineProperty(
      "prototype",
      PropertyDescriptor(Value(WeakRefObject::s_prototype), nullptr, false, false, false));
  
  // グローバルオブジェクトにWeakRefコンストラクタを設定
  globalObj->defineProperty(
      "WeakRef",
      PropertyDescriptor(Value(weakRefConstructorObj), nullptr, false, false, true));
  
  // グローバルオブジェクトのWeakRefコンストラクタを設定
  globalObj->setWeakRefConstructor(static_cast<FunctionObject*>(weakRefConstructorObj));
}

} // namespace aero 