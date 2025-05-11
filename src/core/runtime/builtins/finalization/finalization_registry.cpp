/**
 * @file finalization_registry.cpp
 * @brief JavaScriptのFinalizationRegistryオブジェクトの実装
 * @copyright 2023 AeroJS プロジェクト
 */

#include "finalization_registry.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
#include "../../exception/exception.h"
#include "../../global_object.h"
#include "../../object.h"
#include "../../value.h"
#include "../function/function.h"
#include "../../../utils/memory/gc/garbage_collector.h"
#include "../../../utils/memory/smart_ptr/handle_manager.h"

namespace aero {

// 静的メンバ変数の初期化
Object* FinalizationRegistryObject::s_prototype = nullptr;

// FinalizationRegistryObjectの実装
FinalizationRegistryObject::FinalizationRegistryObject(Value cleanupCallback, GlobalObject* globalObj)
    : Object(globalObj->finalizationRegistryPrototype()),
      m_cleanupCallback(cleanupCallback),
      m_globalObject(globalObj),
      m_isCleanupInProgress(false) {
  // cleanupCallbackの検証はコンストラクタ関数で行われるため、
  // ここでは特に何もしない
}

FinalizationRegistryObject::~FinalizationRegistryObject() {
  // WeakHandleはGCによって自動的に管理されるため、
  // 明示的なクリーンアップは不要
}

// オブジェクトを登録する
bool FinalizationRegistryObject::register_(Value target, Value heldValue, Value unregisterToken) {
  // ターゲットはオブジェクトでなければならない
  if (!target.isObject()) {
    return false;
  }

  // ターゲットとheldValueが同じオブジェクトの場合は登録しない
  // （このチェックはregisterメソッドでも行われているが、APIの安全性のために再度チェック）
  if (target.isObject() && heldValue.isObject() && target.asObject() == heldValue.asObject()) {
    return false;
  }

  // エントリの追加はロックが必要
  std::unique_lock<std::shared_mutex> lock(m_mutex);

  // ObjectからWeakHandleを取得
  Object* obj = target.asObject();
  
  // HandleManagerからWeakHandleを取得
  HandleManager* handleManager = HandleManager::getInstance();
  
  // 新しいエントリを作成
  RegistryEntry entry;
  entry.target = handleManager->createWeakHandle(obj);
  entry.heldValue = heldValue;
  entry.unregisterToken = unregisterToken;
  entry.isTargetAlive.store(true, std::memory_order_release);
  
  // エントリをリストに追加
  size_t index = m_entries.size();
  m_entries.push_back(std::move(entry));
  
  // unregisterTokenが指定されている場合、マップに追加
  if (!unregisterToken.isUndefined()) {
    m_tokenMap[unregisterToken] = index;
  }
  
  return true;
}

// 登録を解除する
bool FinalizationRegistryObject::unregister(Value unregisterToken) {
  // unregisterTokenがundefinedの場合は失敗
  if (unregisterToken.isUndefined()) {
    return false;
  }

  // 読み取り用ロックでトークンの存在確認
  {
    std::shared_lock<std::shared_mutex> readLock(m_mutex);
    if (m_tokenMap.find(unregisterToken) == m_tokenMap.end()) {
      return false;
    }
  }
  
  // 書き込み用ロックで実際の削除を行う
  std::unique_lock<std::shared_mutex> writeLock(m_mutex);
  
  // トークンがマップに存在するか再確認（ロック解除中に変更された可能性があるため）
  auto it = m_tokenMap.find(unregisterToken);
  if (it == m_tokenMap.end()) {
    return false;
  }
  
  // エントリのインデックスを取得
  size_t index = it->second;
  
  // インデックスが範囲外の場合はエラー
  if (index >= m_entries.size()) {
    m_tokenMap.erase(it);
    return false;
  }
  
  // エントリを安全に削除
  safeRemoveEntry(index);
  
  return true;
}

// エントリを安全に削除
void FinalizationRegistryObject::safeRemoveEntry(size_t index) {
  // 最後のエントリを現在位置に移動して、末尾を削除（効率的な削除方法）
  if (index < m_entries.size() - 1) {
    m_entries[index] = std::move(m_entries.back());
    
    // トークンマップを更新
    const Value& movedToken = m_entries[index].unregisterToken;
    if (!movedToken.isUndefined()) {
      m_tokenMap[movedToken] = index;
    }
  }
  
  // 削除対象のエントリのトークンをマップから削除
  const Value& token = m_entries[index].unregisterToken;
  if (!token.isUndefined()) {
    m_tokenMap.erase(token);
  }
  
  // ベクターから最後の要素を削除
  m_entries.pop_back();
}

// クリーンアップキューにエントリを追加
void FinalizationRegistryObject::addToCleanupQueue(size_t index) {
  if (index < m_entries.size()) {
    m_cleanupQueue.push(index);
  }
}

// クリーンアップ処理を実行する
void FinalizationRegistryObject::cleanupSome(Value callback) {
  // 再入を防止（他のスレッドやJSコールバックからのクリーンアップ呼び出しを防ぐ）
  bool expected = false;
  if (!m_isCleanupInProgress.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
    return;  // 既に処理中の場合は何もしない
  }
  
  try {
    // クリーンアップキューが空の場合は無効なエントリを確認
    if (m_cleanupQueue.empty()) {
      std::unique_lock<std::shared_mutex> lock(m_mutex);
      
      // 無効なエントリをチェックして、クリーンアップキューに追加
      for (size_t i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].isTargetAlive.load(std::memory_order_acquire)) {
          m_cleanupQueue.push(i);
        }
      }
    }
    
    // クリーンアップキューが空の場合は終了
    if (m_cleanupQueue.empty()) {
      m_isCleanupInProgress.store(false, std::memory_order_release);
      return;
    }
    
    // キューから最初のエントリを取得
    size_t index;
    {
      std::shared_lock<std::shared_mutex> lock(m_mutex);
      if (m_cleanupQueue.empty()) {
        m_isCleanupInProgress.store(false, std::memory_order_release);
        return;
      }
      
      index = m_cleanupQueue.front();
      m_cleanupQueue.pop();
      
      // インデックスが範囲外の場合は無視
      if (index >= m_entries.size()) {
        m_isCleanupInProgress.store(false, std::memory_order_release);
        return;
      }
      
      // エントリが既に復活している場合は無視
      if (m_entries[index].isTargetAlive.load(std::memory_order_acquire)) {
        m_isCleanupInProgress.store(false, std::memory_order_release);
        return;
      }
    }
    
    // heldValueを取得
    Value heldValue;
    {
      std::shared_lock<std::shared_mutex> lock(m_mutex);
      if (index < m_entries.size()) {
        heldValue = m_entries[index].heldValue;
      }
    }
    
    // コールバックを呼び出す（ロック外で呼び出して再入を防止）
    try {
      if (callback.isFunction()) {
        // cleanupSomeに渡されたコールバック関数を使用
        std::vector<Value> args = {heldValue};
        callback.call(Value::undefined(), args, m_globalObject);
      } else {
        // デフォルトのクリーンアップコールバックを使用
        std::vector<Value> args = {heldValue};
        m_cleanupCallback.call(Value::undefined(), args, m_globalObject);
      }
    } catch (const Exception& e) {
      // コールバック内でエラーが発生した場合は無視
      // 仕様によると、このエラーはHostに報告されるべきだが、コールバックチェーンは中断されない
      if (m_globalObject && m_globalObject->context()) {
        m_globalObject->context()->reportError("Error in FinalizationRegistry cleanup callback", e.getValue());
      }
    }
    
    // エントリを削除
    {
      std::unique_lock<std::shared_mutex> lock(m_mutex);
      if (index < m_entries.size() && !m_entries[index].isTargetAlive.load(std::memory_order_acquire)) {
        safeRemoveEntry(index);
      }
    }
    
  } catch (...) {
    // 予期せぬ例外が発生した場合でもフラグをリセット
  }
  
  // クリーンアップ処理完了のフラグをリセット
  m_isCleanupInProgress.store(false, std::memory_order_release);
}

// 監視しているターゲットのいずれかがGCされたか確認
bool FinalizationRegistryObject::hasDeadTargets() const {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  
  return !m_cleanupQueue.empty() || std::any_of(m_entries.begin(), m_entries.end(), 
      [](const RegistryEntry& entry) {
        return !entry.isTargetAlive.load(std::memory_order_acquire);
      });
}

// クリーンアップコールバックを取得する
Value FinalizationRegistryObject::getCleanupCallback() const {
  return m_cleanupCallback;
}

// ガベージコレクション開始前の処理
void FinalizationRegistryObject::preGCCallback(GarbageCollector* gc) {
  std::shared_lock<std::shared_mutex> lock(m_mutex);
  
  // 各エントリのWeakHandleをGCに登録
  for (auto& entry : m_entries) {
    gc->registerWeakHandle(&entry.target);
  }
}

// ガベージコレクション終了後の処理
void FinalizationRegistryObject::postGCCallback(GarbageCollector* gc) {
  std::unique_lock<std::shared_mutex> lock(m_mutex);
  
  // 各エントリの生存状態を更新し、クリーンアップキューに追加
  for (size_t i = 0; i < m_entries.size(); ++i) {
    RegistryEntry& entry = m_entries[i];
    bool isAlive = entry.target.get() != nullptr;
    
    // 状態が変化した場合のみ処理
    bool wasAlive = entry.isTargetAlive.exchange(isAlive, std::memory_order_acq_rel);
    if (wasAlive && !isAlive) {
      // オブジェクトが死亡した場合、クリーンアップキューに追加
      m_cleanupQueue.push(i);
    }
  }
}

// 無効なエントリをリストから削除する
size_t FinalizationRegistryObject::removeInvalidEntries() {
  size_t originalSize = m_entries.size();
  
  // 削除対象のインデックスを収集
  std::vector<size_t> indicesToRemove;
  for (size_t i = 0; i < m_entries.size(); ++i) {
    if (!m_entries[i].isTargetAlive.load(std::memory_order_acquire)) {
      indicesToRemove.push_back(i);
    }
  }
  
  // 大きいインデックスから順に削除（インデックスのずれを防ぐ）
  std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<size_t>());
  for (size_t index : indicesToRemove) {
    safeRemoveEntry(index);
  }
  
  return originalSize - m_entries.size();
}

// FinalizationRegistryコンストラクタ関数
Value finalizationRegistryConstructor(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがFinalizationRegistryコンストラクタでない場合はエラー
  if (thisObj == nullptr || !thisObj->isConstructor()) {
    throw TypeException("FinalizationRegistry constructor requires new");
  }
  
  // 引数が与えられていない場合はエラー
  if (args.empty()) {
    throw TypeException("FinalizationRegistry constructor requires a cleanup callback function");
  }
  
  // 引数が関数でない場合はエラー
  if (!args[0].isFunction()) {
    throw TypeException("FinalizationRegistry constructor requires a function argument");
  }
  
  // 新しいFinalizationRegistryオブジェクトを作成
  FinalizationRegistryObject* registry = new FinalizationRegistryObject(args[0], globalObj);
  return Value(registry);
}

// FinalizationRegistry.prototype.register メソッド
Value finalizationRegistryRegister(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがFinalizationRegistryオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isFinalizationRegistry()) {
    throw TypeException("FinalizationRegistry.prototype.register called on an object that is not a FinalizationRegistry");
  }
  
  FinalizationRegistryObject* registry = static_cast<FinalizationRegistryObject*>(thisObj);
  
  // 引数が足りない場合はエラー
  if (args.size() < 2) {
    throw TypeException("FinalizationRegistry.prototype.register requires at least 2 arguments");
  }
  
  // ターゲットとheldValueを取得
  Value target = args[0];
  Value heldValue = args[1];
  
  // unregisterTokenは省略可能
  Value unregisterToken = args.size() > 2 ? args[2] : Value::undefined();
  
  // ターゲットがオブジェクトでない場合はエラー
  if (!target.isObject()) {
    throw TypeException("Target must be an object");
  }
  
  // ターゲットとheldValueが同じオブジェクトの場合はエラー
  if (target.isObject() && heldValue.isObject() && target.asObject() == heldValue.asObject()) {
    throw TypeException("target and holdings cannot be the same object");
  }
  
  // オブジェクトを登録
  registry->register_(target, heldValue, unregisterToken);
  
  return Value::undefined();
}

// FinalizationRegistry.prototype.unregister メソッド
Value finalizationRegistryUnregister(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがFinalizationRegistryオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isFinalizationRegistry()) {
    throw TypeException("FinalizationRegistry.prototype.unregister called on an object that is not a FinalizationRegistry");
  }
  
  FinalizationRegistryObject* registry = static_cast<FinalizationRegistryObject*>(thisObj);
  
  // 引数が足りない場合はエラー
  if (args.empty()) {
    throw TypeException("FinalizationRegistry.prototype.unregister requires an unregister token argument");
  }
  
  // unregisterTokenを取得
  Value unregisterToken = args[0];
  
  // 登録を解除
  bool result = registry->unregister(unregisterToken);
  
  return Value(result);
}

// FinalizationRegistry.prototype.cleanupSome メソッド
Value finalizationRegistryCleanupSome(const std::vector<Value>& args, Object* thisObj, GlobalObject* globalObj) {
  // thisがFinalizationRegistryオブジェクトでない場合はエラー
  if (thisObj == nullptr || !thisObj->isFinalizationRegistry()) {
    throw TypeException("FinalizationRegistry.prototype.cleanupSome called on an object that is not a FinalizationRegistry");
  }
  
  FinalizationRegistryObject* registry = static_cast<FinalizationRegistryObject*>(thisObj);
  
  // オプションのコールバックを取得
  Value callback = args.size() > 0 ? args[0] : Value::undefined();
  
  // コールバックが指定されている場合は関数であることを確認
  if (!callback.isUndefined() && !callback.isFunction()) {
    throw TypeException("The callback argument must be a function");
  }
  
  // クリーンアップ処理を実行
  registry->cleanupSome(callback);
  
  return Value::undefined();
}

// FinalizationRegistryプロトタイプを初期化
void initFinalizationRegistryPrototype(GlobalObject* globalObj) {
  // FinalizationRegistryプロトタイプオブジェクトがまだ存在しない場合、作成
  if (FinalizationRegistryObject::s_prototype == nullptr) {
    FinalizationRegistryObject::s_prototype = new Object(globalObj->objectPrototype());
    
    // プロトタイプメソッドを設定
    FinalizationRegistryObject::s_prototype->defineNativeFunction("register", finalizationRegistryRegister, 2);
    FinalizationRegistryObject::s_prototype->defineNativeFunction("unregister", finalizationRegistryUnregister, 1);
    FinalizationRegistryObject::s_prototype->defineNativeFunction("cleanupSome", finalizationRegistryCleanupSome, 0);
    
    // プロトタイプにクラス名を設定
    FinalizationRegistryObject::s_prototype->defineProperty(
        "constructor",
        PropertyDescriptor(Value(globalObj->finalizationRegistryConstructor()), nullptr, false, false, true));
    
    // toStringTagを設定
    FinalizationRegistryObject::s_prototype->defineProperty(
        globalObj->getSymbolRegistry()->getSymbol("toStringTag"),
        PropertyDescriptor(Value("FinalizationRegistry"), nullptr, false, false, true));
  }
}

// FinalizationRegistryオブジェクトを初期化
void initFinalizationRegistryObject(GlobalObject* globalObj) {
  // FinalizationRegistryプロトタイプを初期化
  initFinalizationRegistryPrototype(globalObj);
  
  // FinalizationRegistryコンストラクタを作成
  Object* finalizationRegistryConstructorObj = new Object(globalObj->functionPrototype());
  finalizationRegistryConstructorObj->setIsConstructor(true);
  
  // プロトタイプを設定
  finalizationRegistryConstructorObj->defineProperty(
      "prototype",
      PropertyDescriptor(Value(FinalizationRegistryObject::s_prototype), nullptr, false, false, false));
  
  // グローバルオブジェクトにFinalizationRegistryコンストラクタを設定
  globalObj->defineProperty(
      "FinalizationRegistry",
      PropertyDescriptor(Value(finalizationRegistryConstructorObj), nullptr, false, false, true));
  
  // グローバルオブジェクトのFinalizationRegistryコンストラクタを設定
  globalObj->setFinalizationRegistryConstructor(static_cast<FunctionObject*>(finalizationRegistryConstructorObj));
}

} // namespace aero