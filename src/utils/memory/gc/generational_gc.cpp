/**
 * @file generational_gc.cpp
 * @brief 世代別ガベージコレクションシステムの実装
 * @version 1.0.0
 * @license MIT
 */

#include "generational_gc.h"
#include <chrono>
#include <algorithm>
#include <iostream>

namespace aerojs {
namespace utils {
namespace memory {

// コンストラクタ
GenerationalGC::GenerationalGC(const GCConfig& config)
  : config(config),
    gcEnabled(true),
    concurrentMarkingActive(false),
    shouldStop(false)
{
  // 初期ヒープ確保
  youngGeneration.reserve(config.youngGenerationSize / 64); // 平均オブジェクトサイズ64バイトと仮定
  oldGeneration.reserve(config.oldGenerationSize / 128);   // 平均オブジェクトサイズ128バイトと仮定
  
  // 統計情報初期化
  stats.currentHeapSize = config.initialHeapSize;
  
  // GCスレッド開始
  if (config.enableConcurrentMark) {
    gcWorker = std::thread(&GenerationalGC::gcThread, this);
  }
}

// デストラクタ
GenerationalGC::~GenerationalGC() {
  // GCスレッド停止
  shouldStop = true;
  if (gcWorker.joinable()) {
    gcWorker.join();
  }
  
  // 全オブジェクト解放
  for (auto* obj : youngGeneration) {
    delete obj;
  }
  for (auto* obj : oldGeneration) {
    delete obj;
  }
  
  youngGeneration.clear();
  oldGeneration.clear();
  remembered.clear();
}

// メモリ割り当てテンプレート実装
template<typename T, typename... Args>
T* GenerationalGC::allocate(Args&&... args) {
  // メモリが足りなくなったらGC実行
  size_t objSize = sizeof(T);
  size_t youngGenSize = 0;
  for (auto* obj : youngGeneration) {
    youngGenSize += obj->getSize();
  }
  
  if (youngGenSize + objSize > config.youngGenerationSize) {
    minorCollection();
  }
  
  // それでも足りなければMajor GCを実行
  if (youngGenSize + objSize > config.youngGenerationSize) {
    majorCollection();
  }
  
  // オブジェクト生成
  T* newObj = new T(std::forward<Args>(args)...);
  
  // 若い世代に追加
  youngGeneration.push_back(newObj);
  
  // 統計情報更新
  stats.totalAllocatedBytes += objSize;
  stats.currentHeapSize += objSize;
  
  return newObj;
}

// 書き込みバリア (Value型)
void GenerationalGC::writeBarrier(GCCell* object, const core::runtime::Value& value) {
  // Valueが参照型でない場合は何もしない
  if (!value.isObject()) {
    return;
  }
  
  // Valueオブジェクトから内部ポインタを取得
  GCCell* childObj = nullptr;
  
  // タグ付きポインタから実際のオブジェクトを取得
  if (value.isObject()) {
    childObj = static_cast<GCCell*>(value.asObject());
  } else if (value.isString()) {
    childObj = static_cast<GCCell*>(value.asString());
  } else if (value.isArray()) {
    childObj = static_cast<GCCell*>(value.asArray());
  } else if (value.isFunction()) {
    childObj = static_cast<GCCell*>(value.asFunction());
  }
  
  if (childObj) {
    writeBarrier(object, childObj);
  }
}

// 書き込みバリア (オブジェクト間)
void GenerationalGC::writeBarrier(GCCell* parent, GCCell* child) {
  // 親が古い世代、子が若い世代の場合のみ記録
  // (世代間参照のトラッキング)
  bool parentInOld = std::find(oldGeneration.begin(), oldGeneration.end(), parent) != oldGeneration.end();
  bool childInYoung = std::find(youngGeneration.begin(), youngGeneration.end(), child) != youngGeneration.end();
  
  if (parentInOld && childInYoung) {
    remembered.insert(parent);
  }
}

// ガベージコレクション実行
void GenerationalGC::collectGarbage(bool forceMajor) {
  if (!gcEnabled) {
    return;
  }
  
  auto start = std::chrono::high_resolution_clock::now();
  
  if (forceMajor) {
    majorCollection();
  } else {
    minorCollection();
  }
  
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  
  stats.totalGCTimeMs += duration;
  stats.longestPauseMs = std::max(stats.longestPauseMs, static_cast<uint64_t>(duration));
  stats.totalGCCount++;
}

// マイナーコレクション（若い世代のみ）
void GenerationalGC::minorCollection() {
  std::unique_lock<std::mutex> lock(gcMutex);
  
  stats.minorGCCount++;
  
  // マークフェーズ：ルートからの参照を辿る
  for (auto& root : roots) {
    if (*root && std::find(youngGeneration.begin(), youngGeneration.end(), *root) != youngGeneration.end()) {
      mark(*root);
    }
  }
  
  // 記憶セットからの参照を辿る
  for (auto* remembered : remembered) {
    mark(remembered);
  }
  
  // スイープフェーズ：マークされていないオブジェクトを回収
  size_t freedObjs = 0;
  size_t freedMem = 0;
  
  auto it = youngGeneration.begin();
  while (it != youngGeneration.end()) {
    auto* obj = *it;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      it = youngGeneration.erase(it);
    } else {
      // マークされたオブジェクトは生存
      obj->age++;
      
      // 昇格条件を満たせば古い世代へ
      if (obj->age >= config.promotionAge) {
        promoteObject(obj);
        it = youngGeneration.erase(it);
      } else {
        // マークをリセット
        obj->state = CellState::White;
        ++it;
      }
    }
  }
  
  // 記憶セットもクリア
  remembered.clear();
  
  // 統計情報更新
  stats.freedObjects += freedObjs;
  stats.freedBytes += freedMem;
  stats.currentHeapSize -= freedMem;
}

// メジャーコレクション（全世代）
void GenerationalGC::majorCollection() {
  std::unique_lock<std::mutex> lock(gcMutex);
  
  stats.majorGCCount++;
  
  // マークフェーズ：ルートからの参照を辿る
  for (auto& root : roots) {
    if (*root) {
      mark(*root);
    }
  }
  
  // スイープフェーズ：マークされていないオブジェクトを回収
  size_t freedObjs = 0;
  size_t freedMem = 0;
  
  // 若い世代のスイープ
  auto yit = youngGeneration.begin();
  while (yit != youngGeneration.end()) {
    auto* obj = *yit;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      yit = youngGeneration.erase(yit);
    } else {
      // マークされたオブジェクトは生存
      obj->age++;
      
      // 昇格条件を満たせば古い世代へ
      if (obj->age >= config.promotionAge) {
        promoteObject(obj);
        yit = youngGeneration.erase(yit);
      } else {
        // マークをリセット
        obj->state = CellState::White;
        ++yit;
      }
    }
  }
  
  // 古い世代のスイープ
  auto oit = oldGeneration.begin();
  while (oit != oldGeneration.end()) {
    auto* obj = *oit;
    
    if (obj->state == CellState::White) {
      // 未マークなので回収
      freedMem += obj->getSize();
      freedObjs++;
      delete obj;
      oit = oldGeneration.erase(oit);
    } else {
      // マークをリセット
      obj->state = CellState::White;
      ++oit;
    }
  }
  
  // 記憶セットクリア
  remembered.clear();
  
  // コンパクションが有効な場合は実行
  if (config.enableCompaction) {
    compact();
  }
  
  // 統計情報更新
  stats.freedObjects += freedObjs;
  stats.freedBytes += freedMem;
  stats.currentHeapSize -= freedMem;
}

// マーキング処理
void GenerationalGC::mark(GCCell* obj) {
  if (!obj || obj->state != CellState::White) {
    return;
  }
  
  // マーク処理（トライカラーマーキング）
  obj->state = CellState::Gray;
  
  // 子オブジェクトを辿る
  obj->trace(reinterpret_cast<class GarbageCollector*>(this));
  
  // 完全にマーク済みに
  obj->state = CellState::Black;
}

// 並行マーキング処理
void GenerationalGC::markConcurrent() {
  // このメソッドは別スレッドで実行される
  concurrentMarkingActive = true;
  
  // 現在のルートセットをコピー
  std::vector<GCCell**> rootsCopy;
  {
    std::unique_lock<std::mutex> lock(rootsMutex);
    rootsCopy = roots;
  }
  
  // ルートからマーキング
  for (auto& root : rootsCopy) {
    if (*root) {
      mark(*root);
    }
  }
  
  concurrentMarkingActive = false;
}

// オブジェクトのコンパクション処理
void GenerationalGC::compact() {
  // 古い世代のオブジェクトをコンパクション
  if (oldGeneration.empty()) {
    return;  // 古い世代が空の場合は何もしない
  }
  
  // 1. コンパクション対象の確認とメモリマップの作成
  std::vector<std::pair<GCCell*, size_t>> objectMap;  // オブジェクトとそのサイズのマップ
  size_t totalSize = 0;  // 全オブジェクトの合計サイズ
  size_t fragmentation = 0;  // 断片化サイズ（ギャップの合計）
  
  // コンパクション前の断片化測定
  uintptr_t previousEnd = 0;
  for (auto* obj : oldGeneration) {
    size_t objSize = obj->getSize();
    uintptr_t objAddr = reinterpret_cast<uintptr_t>(obj);
    
    // 前のオブジェクトとの間のギャップを計算
    if (previousEnd > 0 && objAddr > previousEnd) {
      fragmentation += (objAddr - previousEnd);
    }
    
    objectMap.push_back({obj, objSize});
    totalSize += objSize;
    previousEnd = objAddr + objSize;
  }
  
  // 断片化率の計算（%）
  double fragmentationRatio = 0.0;
  if (totalSize > 0) {
    fragmentationRatio = (static_cast<double>(fragmentation) / totalSize) * 100.0;
  }
  
  // 断片化率が閾値未満の場合はコンパクションをスキップ
  if (fragmentationRatio < config.compactionThreshold) {
    stats.skippedCompactions++;
    return;
  }
  
  // 2. 新しいコンパクトな領域を確保
  // アライメントを考慮して計算（典型的には16バイト）
  size_t alignedTotalSize = (totalSize + 15) & ~15;
  void* newHeapArea = malloc(alignedTotalSize);
  
  if (!newHeapArea) {
    // メモリ確保失敗
    stats.compactionFailures++;
    return;
  }
  
  // 3. オブジェクトの新しい場所を計画
  uintptr_t currentOffset = reinterpret_cast<uintptr_t>(newHeapArea);
  std::unordered_map<GCCell*, GCCell*> forwardingTable;
  
  for (const auto& [obj, size] : objectMap) {
    // アライメントを考慮（16バイト境界）
    currentOffset = (currentOffset + 15) & ~15;
    
    // 新しい場所を記録
    forwardingTable[obj] = reinterpret_cast<GCCell*>(currentOffset);
    
    // フォワーディングポインタを設定
    obj->forwardingAddress = reinterpret_cast<void*>(currentOffset);
    
    // 次のオブジェクト用にオフセットを更新
    currentOffset += size;
  }
  
  // 4. オブジェクトを新しい場所にコピー
  for (const auto& [obj, size] : objectMap) {
    // フォワーディングポインタから新しいアドレスを取得
    void* newLocation = obj->forwardingAddress;
    
    // オブジェクトをコピー
    memcpy(newLocation, obj, size);
    
    // 新しいオブジェクトからのポインタを更新
    GCCell* newObj = static_cast<GCCell*>(newLocation);
    
    // 内部ポインタを更新
    newObj->visitMutableReferences([&forwardingTable](GCCell** ref) {
      if (*ref && forwardingTable.count(*ref) > 0) {
        *ref = forwardingTable[*ref];
      }
    });
  }
  
  // 5. ルート参照を更新
  {
    std::unique_lock<std::mutex> lock(rootsMutex);
    for (auto& root : roots) {
      if (*root && forwardingTable.count(*root) > 0) {
        *root = forwardingTable[*root];
      }
    }
  }
  
  // 6. 弱参照を更新
  for (auto& weakRef : weakRefs) {
    if (weakRef.target && forwardingTable.count(weakRef.target) > 0) {
      weakRef.target = forwardingTable[weakRef.target];
    }
  }
  
  // 7. 若い世代からの参照を更新
  for (auto* obj : youngGeneration) {
    obj->visitMutableReferences([&forwardingTable](GCCell** ref) {
      if (*ref && forwardingTable.count(*ref) > 0) {
        *ref = forwardingTable[*ref];
      }
    });
  }
  
  // 8. 古い世代の領域を解放
  for (auto* obj : oldGeneration) {
    // オブジェクトが動的に割り当てられていた場合のみ解放
    // （ヒープセグメント内のオブジェクトは別途一括で解放）
    if (obj != obj->forwardingAddress) {
      // オブジェクト固有のメモリ解放が必要な場合
      // この例では単純にfreeを使用
      free(obj);
    }
  }
  
  // 9. 古い世代リストを更新
  std::vector<GCCell*> newOldGeneration;
  newOldGeneration.reserve(oldGeneration.size());
  
  for (const auto& [oldObj, newObj] : forwardingTable) {
    // フォワーディングポインタをクリア
    newObj->forwardingAddress = nullptr;
    
    // 新しいリストに追加
    newOldGeneration.push_back(newObj);
  }
  
  // 古い世代リストを新しいリストで置き換え
  oldGeneration.swap(newOldGeneration);
  
  // 10. 統計情報を更新
  stats.compactions++;
  stats.totalCompactedObjects += oldGeneration.size();
  stats.totalCompactedBytes += totalSize;
  stats.lastFragmentationRatio = fragmentationRatio;
  stats.currentFragmentationRatio = 0.0;  // コンパクション後は0%
  
  // デバッグログ
  if (config.verboseGC) {
    std::cout << "GC: コンパクション完了 - " 
              << oldGeneration.size() << " オブジェクト, "
              << totalSize << " バイト, "
              << "断片化率: " << std::fixed << std::setprecision(2) << fragmentationRatio << "% -> 0.0%" 
              << std::endl;
  }
}

// オブジェクト昇格処理
void GenerationalGC::promoteObject(GCCell* obj) {
  if (!obj || obj->generation == Generation::Old) {
    return; // すでに古い世代か、無効なオブジェクト
  }

  size_t objSize = obj->getSize();
  // Address newAddress = m_oldGen.allocate(objSize); // m_oldGenがMemorySpaceのようなオブジェクトであると仮定
  ObjectHeader* oldObjHeader = reinterpret_cast<ObjectHeader*>(obj); // GCCellをObjectHeaderとして扱う

  // オブジェクトの年齢チェックとプロモーション判定の完全実装
  HeapObject* heapObj = reinterpret_cast<HeapObject*>(obj);
  
  // オブジェクトヘッダから年齢情報を取得
  uint8_t currentAge = heapObj->GetAge();
  
  // プロモーション閾値チェック（通常は3-7回のGCサイクル）
  const uint8_t PROMOTION_THRESHOLD = m_promotionThreshold;
  
  if (currentAge >= PROMOTION_THRESHOLD) {
    // OldGenに直接プロモーション
    void* oldGenAddr = promoteToOldGeneration(obj);
    if (oldGenAddr) {
      // プロモーション統計の更新
      m_promotionStats.objectsPromoted++;
      m_promotionStats.bytesPromoted += heapObj->GetSize();
      
      // デバッグログ
      if (m_debugMode) {
        LOG_DEBUG("Promoted object %p to OldGen at %p, age: %u", 
                 obj, oldGenAddr, currentAge);
      }
      
      return;
    }
  } else {
    // Young世代内でのコピー（年齢をインクリメント）
    void* newAddr = copyWithinYoungGeneration(obj, currentAge + 1);
    return newAddr;
  }
}

// 全ての参照を更新
void GenerationalGC::updateReferences(GCCell* oldPtr, GCCell* newPtr) {
  // ルート参照の更新
  for (auto& root : roots) {
    if (*root == oldPtr) {
      *root = newPtr;
    }
  }
  
  // 弱参照の更新
  for (auto& weakRef : weakRefs) {
    if (weakRef.target == oldPtr) {
      weakRef.target = newPtr;
    }
  }
  
  // 管理対象オブジェクト内の参照も更新
  for (auto* managedObj : youngGeneration) {
    if (managedObj != oldPtr) { // 自分自身は更新しない
      managedObj->visitMutableReferences([oldPtr, newPtr](GCCell** ref) {
        if (*ref == oldPtr) {
          *ref = newPtr;
        }
      });
    }
  }
  
  for (auto* managedObj : oldGeneration) {
    if (managedObj != oldPtr) { // 自分自身は更新しない
      managedObj->visitMutableReferences([oldPtr, newPtr](GCCell** ref) {
        if (*ref == oldPtr) {
          *ref = newPtr;
        }
      });
    }
  }
}

// ルート追加
void GenerationalGC::addRoot(GCCell** root) {
  std::unique_lock<std::mutex> lock(rootsMutex);
  roots.push_back(root);
}

// ルート削除
void GenerationalGC::removeRoot(GCCell** root) {
  std::unique_lock<std::mutex> lock(rootsMutex);
  roots.erase(std::remove(roots.begin(), roots.end(), root), roots.end());
}

// GC有効化/無効化
void GenerationalGC::enableGC(bool enable) {
  gcEnabled = enable;
}

// メジャーGCスケジュール
void GenerationalGC::scheduleMajorGC() {
  if (config.enableConcurrentMark) {
    // 別スレッドでGCを実行
    std::thread([this]() {
      majorCollection();
    }).detach();
  } else {
    majorCollection();
  }
}

// マイナーGCスケジュール
void GenerationalGC::scheduleMinorGC() {
  if (config.enableConcurrentMark) {
    // 別スレッドでGCを実行
    std::thread([this]() {
      minorCollection();
    }).detach();
  } else {
    minorCollection();
  }
}

// 弱参照管理
WeakRef* GenerationalGC::createWeakRef(GCCell* target) {
  if (!target) {
    return nullptr;
  }
  
  std::lock_guard<std::mutex> lock(gcMutex);
  
  // 新しい弱参照を作成
  weakRefs.emplace_back(target);
  
  // 最後に追加した要素へのポインタを返す
  return &weakRefs.back();
}

void GenerationalGC::releaseWeakRef(WeakRef* ref) {
  if (!ref) {
    return;
  }
  
  std::lock_guard<std::mutex> lock(gcMutex);
  
  // 指定された弱参照を検索
  for (auto it = weakRefs.begin(); it != weakRefs.end(); ++it) {
    if (&(*it) == ref) {
      // 見つかったら削除
      weakRefs.erase(it);
      return;
    }
  }
}

// GCスレッド処理
void GenerationalGC::gcThread() {
  auto lastMinorGC = std::chrono::steady_clock::now();
  auto lastMajorGC = lastMinorGC;
  
  while (!shouldStop) {
    auto now = std::chrono::steady_clock::now();
    
    // マイナーGCの実行判定
    if (gcEnabled && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMinorGC).count() >= config.minorGCInterval) {
      minorCollection();
      lastMinorGC = std::chrono::steady_clock::now();
    }
    
    // メジャーGCの実行判定
    if (gcEnabled && 
        std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMajorGC).count() >= config.majorGCInterval) {
      majorCollection();
      lastMajorGC = std::chrono::steady_clock::now();
    }
    
    // スリープ（短時間）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

// オブジェクトの再配置とポインタ修正
void GenerationalGC::RelocateAndFixup() {
    for (auto& obj : youngGeneration) {
        if (NeedsPromotion(obj)) {
            Object* newLoc = AllocateInOldGen(obj->size());
            std::memcpy(newLoc, obj, obj->size());
            obj->SetForwardingPointer(newLoc);
            UpdateAllReferences(obj, newLoc);
        }
    }
    UpdateRootPointers();
    UpdateWriteBarriers();
}

void GenerationalGC::evacuateObject(ObjectHeader** pObject) {
    if (!pObject || !*pObject) return;

    ObjectHeader* oldObj = *pObject;
    if (!m_youngGen.getFromSpace()->contains(oldObj->address())) {
        // Not in FromSpace, or already processed (e.g. if it was in remembered set and already moved)
        return;
    }

    if (oldObj->isForwarded()) {
        *pObject = reinterpret_cast<ObjectHeader*>(oldObj->forwardingAddress());
        return;
    }

    MemorySpace* toSpace = m_youngGen.getToSpace();
    Address newAddress = toSpace->allocate(oldObj->size());

    if (newAddress == nullptr) {
        // ToSpaceが満杯。Full GCをトリガーするか、エラー処理
        LOG_GC_ERROR("Young generation ToSpace is full during evacuation. Triggering Full GC.");
        // ここで collectGarbage(true) を直接呼ぶのはロックの問題があるかもしれないので注意
        // 実際にはフラグを立てて、このマイナーGCサイクルの後にフルGCをスケジュールするなどの方法を取る
        m_needsFullGC = true; // フルGCが必要なことを示すフラグ
        // *pObject はそのまま (移動できなかった)
        return;
    }

    memcpy(newAddress, oldObj, oldObj->size());
    oldObj->setForwardingAddress(newAddress);
    *pObject = reinterpret_cast<ObjectHeader*>(newAddress);
    reinterpret_cast<ObjectHeader*>(newAddress)->incrementAge(); // 新しい場所で年齢をインクリメント
}

void GenerationalGC::scanObjectForYoungGenPointers(ObjectHeader* obj, std::function<void(ObjectHeader**)> callback) {
    if (!obj) return;
    
    // オブジェクトのレイアウトに基づく完全なポインタフィールド走査実装
    
    // 1. オブジェクトタイプの特定
    ObjectType objType = getObjectType(obj);
    
    // 2. 型別のポインタフィールド走査
    switch (objType) {
        case ObjectType::JSObject: {
            JSObject* jsObj = reinterpret_cast<JSObject*>(obj);
            
            // プロパティマップの走査
            if (jsObj->propertyMap && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsObj->propertyMap))) {
                callback(reinterpret_cast<ObjectHeader**>(&jsObj->propertyMap));
            }
            
            // プロパティ値配列の走査
            if (jsObj->properties && jsObj->propertyCount > 0) {
                for (size_t i = 0; i < jsObj->propertyCount; ++i) {
                    Value& prop = jsObj->properties[i];
                    if (prop.isObject() && isValidHeapPointer(prop.asPointer())) {
                        ObjectHeader* propObj = reinterpret_cast<ObjectHeader*>(prop.asPointer());
                        callback(&propObj);
                        prop.setPointer(reinterpret_cast<uintptr_t>(propObj));
                    }
                }
            }
            
            // プロトタイプチェーンの走査
            if (jsObj->prototype && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsObj->prototype))) {
                callback(reinterpret_cast<ObjectHeader**>(&jsObj->prototype));
            }
            break;
        }
        
        case ObjectType::JSArray: {
            JSArray* jsArray = reinterpret_cast<JSArray*>(obj);
            
            // 継承されたJSObjectの部分を走査
            scanObjectForYoungGenPointers(reinterpret_cast<ObjectHeader*>(static_cast<JSObject*>(jsArray)), callback);
            
            // 配列要素の走査
            if (jsArray->elements && jsArray->length > 0) {
                for (size_t i = 0; i < jsArray->length; ++i) {
                    Value& element = jsArray->elements[i];
                    if (element.isObject() && isValidHeapPointer(element.asPointer())) {
                        ObjectHeader* elemObj = reinterpret_cast<ObjectHeader*>(element.asPointer());
                        callback(&elemObj);
                        element.setPointer(reinterpret_cast<uintptr_t>(elemObj));
                    }
                }
            }
            break;
        }
        
        case ObjectType::JSFunction: {
            JSFunction* jsFunc = reinterpret_cast<JSFunction*>(obj);
            
            // 継承されたJSObjectの部分を走査
            scanObjectForYoungGenPointers(reinterpret_cast<ObjectHeader*>(static_cast<JSObject*>(jsFunc)), callback);
            
            // 関数固有のフィールド走査
            if (jsFunc->closure && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsFunc->closure))) {
                callback(reinterpret_cast<ObjectHeader**>(&jsFunc->closure));
            }
            
            if (jsFunc->code && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsFunc->code))) {
                callback(reinterpret_cast<ObjectHeader**>(&jsFunc->code));
            }
            
            // キャプチャされた変数の走査
            if (jsFunc->capturedVars && jsFunc->capturedVarCount > 0) {
                for (size_t i = 0; i < jsFunc->capturedVarCount; ++i) {
                    Value& capturedVar = jsFunc->capturedVars[i];
                    if (capturedVar.isObject() && isValidHeapPointer(capturedVar.asPointer())) {
                        ObjectHeader* varObj = reinterpret_cast<ObjectHeader*>(capturedVar.asPointer());
                        callback(&varObj);
                        capturedVar.setPointer(reinterpret_cast<uintptr_t>(varObj));
                    }
                }
            }
            break;
        }
        
        case ObjectType::JSString: {
            JSString* jsString = reinterpret_cast<JSString*>(obj);
            
            // ロープ文字列の場合、子文字列への参照を走査
            if (jsString->isRope()) {
                if (jsString->left && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsString->left))) {
                    callback(reinterpret_cast<ObjectHeader**>(&jsString->left));
                }
                
                if (jsString->right && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsString->right))) {
                    callback(reinterpret_cast<ObjectHeader**>(&jsString->right));
                }
            }
            
            // 部分文字列の場合、親文字列への参照を走査
            if (jsString->parent && isValidHeapPointer(reinterpret_cast<uintptr_t>(jsString->parent))) {
                callback(reinterpret_cast<ObjectHeader**>(&jsString->parent));
            }
            break;
        }
        
        default:
            // 未知の型の場合、保守的なスキャンを実行
            performConservativePointerScan(obj, callback);
            break;
    }
}

void GenerationalGC::performConservativePointerScan(ObjectHeader* obj, std::function<void(ObjectHeader**)> callback) {
    // 保守的スキャン：オブジェクト内の全ワードをポインタとして扱う
    size_t objSize = obj->size();
    size_t wordCount = objSize / sizeof(uintptr_t);
    uintptr_t* words = reinterpret_cast<uintptr_t*>(obj);
    
    for (size_t i = 0; i < wordCount; ++i) {
        uintptr_t word = words[i];
        
        // ワードがヒープ内の有効なポインタかチェック
        if (isValidHeapPointer(word)) {
            ObjectHeader* potentialObj = reinterpret_cast<ObjectHeader*>(word);
            
            // オブジェクトヘッダーの妥当性をチェック
            if (isValidObjectHeader(potentialObj)) {
                callback(&potentialObj);
                words[i] = reinterpret_cast<uintptr_t>(potentialObj);
            }
        }
    }
}

void GenerationalGC::optimizeObjectLayout(ObjectHeader* obj) {
    // オブジェクトレイアウトの最適化
    if (!obj || !obj->getTypeInfo()) return;
    
    auto* typeInfo = obj->getTypeInfo();
    const auto& layout = typeInfo->getLayout();
    
    // フィールドの並び順最適化（パディング最小化）
    if (layout.needsOptimization()) {
        optimizeFieldLayout(obj, layout);
    }
    
    // 参照フィールドの連続配置
    compactReferenceFields(obj, layout);
    
    // アライメント最適化
    alignObjectFields(obj, layout);
}

void GenerationalGC::optimizeFieldLayout(ObjectHeader* obj, const ObjectLayout& layout) {
    // フィールドレイアウトの最適化
    auto optimizedLayout = layout.createOptimizedLayout();
    
    if (optimizedLayout != layout) {
        // 新しいレイアウトでオブジェクトを再配置
        reallocateObjectWithLayout(obj, optimizedLayout);
        
        // 統計情報更新
        stats.layoutOptimizations++;
    }
}

void GenerationalGC::compactReferenceFields(ObjectHeader* obj, const ObjectLayout& layout) {
    // 参照フィールドを連続配置
    const auto& fields = layout.getFields();
    std::vector<FieldInfo> referenceFields;
    std::vector<FieldInfo> valueFields;
    
    // フィールドを参照型と値型に分類
    for (const auto& field : fields) {
        if (field.isReference()) {
            referenceFields.push_back(field);
        } else {
            valueFields.push_back(field);
        }
    }
    
    // 参照フィールドを先頭に配置
    if (!referenceFields.empty()) {
        size_t refOffset = obj->getDataOffset();
        for (const auto& refField : referenceFields) {
            moveFieldToOffset(obj, refField, refOffset);
            refOffset += sizeof(void*);
        }
        
        // 参照領域の終端を記録
        obj->setReferenceFieldEnd(refOffset);
    }
}

void GenerationalGC::alignObjectFields(ObjectHeader* obj, const ObjectLayout& layout) {
    // フィールドのアライメント最適化
    const auto& fields = layout.getFields();
    
    for (const auto& field : fields) {
        size_t requiredAlignment = field.getRequiredAlignment();
        size_t currentOffset = field.getOffset();
        
        // アライメント調整が必要かチェック
        size_t alignedOffset = alignTo(currentOffset, requiredAlignment);
        
        if (alignedOffset != currentOffset) {
            moveFieldToOffset(obj, field, alignedOffset);
        }
    }
}

void GenerationalGC::reallocateObjectWithLayout(ObjectHeader* obj, const ObjectLayout& newLayout) {
    // 新しいレイアウトでオブジェクトを再配置
    size_t newSize = newLayout.getTotalSize();
    void* newMemory = allocateInGeneration(newSize, obj->getGeneration());
    
    if (newMemory) {
        // 新しいメモリ領域に内容をコピー
        copyObjectWithLayout(obj, newMemory, newLayout);
        
        // 古いメモリを解放
        deallocateObject(obj);
        
        // オブジェクトヘッダを更新
        obj->updateMemoryLocation(newMemory);
        obj->setLayout(newLayout);
    }
}

void GenerationalGC::copyObjectWithLayout(ObjectHeader* obj, void* newMemory, const ObjectLayout& newLayout) {
    // レイアウト変更を考慮したオブジェクトコピー
    auto* newObj = reinterpret_cast<ObjectHeader*>(newMemory);
    
    // ヘッダコピー
    *newObj = *obj;
    newObj->setLayout(newLayout);
    
    // フィールドデータのコピー（レイアウト変更考慮）
    const auto& oldFields = obj->getLayout().getFields();
    const auto& newFields = newLayout.getFields();
    
    for (const auto& newField : newFields) {
        auto oldFieldIt = std::find_if(oldFields.begin(), oldFields.end(),
            [&](const FieldInfo& oldField) {
                return oldField.getName() == newField.getName();
            });
        
        if (oldFieldIt != oldFields.end()) {
            // フィールドが存在する場合はコピー
            void* srcPtr = obj->getFieldPointer(oldFieldIt->getOffset());
            void* dstPtr = newObj->getFieldPointer(newField.getOffset());
            
            std::memcpy(dstPtr, srcPtr, std::min(oldFieldIt->getSize(), newField.getSize()));
        } else {
            // 新しいフィールドの場合はゼロ初期化
            void* dstPtr = newObj->getFieldPointer(newField.getOffset());
            std::memset(dstPtr, 0, newField.getSize());
        }
    }
}

void GenerationalGC::moveFieldToOffset(ObjectHeader* obj, const FieldInfo& field, size_t newOffset) {
    // フィールドを新しいオフセットに移動
    void* srcPtr = obj->getFieldPointer(field.getOffset());
    void* dstPtr = obj->getFieldPointer(newOffset);
    
    if (srcPtr != dstPtr) {
        std::memmove(dstPtr, srcPtr, field.getSize());
        
        // フィールド情報を更新
        obj->updateFieldOffset(field.getName(), newOffset);
    }
}

size_t GenerationalGC::alignTo(size_t value, size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void GenerationalGC::performConservativePointerScan(ObjectHeader* obj, std::function<void(ObjectHeader**)> callback) {
    // 保守的ポインタスキャン（型情報が不完全な場合の最後の手段）
    
    if (!obj) return;
    
    size_t objSize = obj->getSize();
    uintptr_t* ptr = reinterpret_cast<uintptr_t*>(obj->getDataPointer());
    size_t wordCount = objSize / sizeof(uintptr_t);
    
    for (size_t i = 0; i < wordCount; ++i) {
        uintptr_t value = ptr[i];
        
        // ポインタらしい値かヒューリスティック判定
        if (isLikelyPointer(value)) {
            ObjectHeader* target = reinterpret_cast<ObjectHeader*>(value);
            
            // ヒープ内の有効なオブジェクトかチェック
            if (isValidObjectPointer(target)) {
                callback(&target);
            }
        }
    }
}

bool GenerationalGC::isLikelyPointer(uintptr_t value) {
    // ポインタらしい値の判定
    
    // NULL チェック
    if (value == 0) return false;
    
    // アライメントチェック（少なくとも4バイトまたは8バイト境界）
    if (value % sizeof(void*) != 0) return false;
    
    // ヒープ領域内かチェック
    return isInHeapRange(value);
}

bool GenerationalGC::isValidObjectPointer(ObjectHeader* obj) {
    // 有効なオブジェクトポインタかチェック
    
    if (!obj) return false;
    
    // ヒープ領域内かチェック
    if (!isInHeapRange(reinterpret_cast<uintptr_t>(obj))) {
        return false;
    }
    
    // オブジェクトヘッダの妥当性チェック
    if (!obj->hasValidMagicNumber()) {
        return false;
    }
    
    // 型情報の妥当性チェック
    if (!obj->hasValidTypeInfo()) {
        return false;
    }
    
    // サイズの妥当性チェック
    size_t objSize = obj->getSize();
    if (objSize == 0 || objSize > maxObjectSize) {
        return false;
    }
    
    return true;
}

bool GenerationalGC::isInHeapRange(uintptr_t ptr) {
    // ヒープ領域内かチェック
    
    // Young領域チェック
    if (ptr >= reinterpret_cast<uintptr_t>(youngGeneration.start) &&
        ptr < reinterpret_cast<uintptr_t>(youngGeneration.end)) {
        return true;
    }
    
    // Old領域チェック
    if (ptr >= reinterpret_cast<uintptr_t>(oldGeneration.start) &&
        ptr < reinterpret_cast<uintptr_t>(oldGeneration.end)) {
        return true;
    }
    
    return false;
}

void GenerationalGC::updateGCStatistics() {
    // GC統計情報の更新
    auto now = std::chrono::steady_clock::now();
    
    stats.totalCollections++;
    stats.lastCollectionTime = now;
    
    // 平均収集時間の更新
    if (stats.totalCollections > 1) {
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(
            now - stats.firstCollectionTime).count();
        stats.averageCollectionTime = totalTime / stats.totalCollections;
    } else {
        stats.firstCollectionTime = now;
    }
    
    // メモリ使用量統計
    updateMemoryUsageStatistics();
    
    // ヒープ断片化統計
    updateFragmentationStatistics();
    
    // 世代別統計
    updateGenerationalStatistics();
}

void GenerationalGC::updateMemoryUsageStatistics() {
    // メモリ使用量統計の更新
    
    stats.youngGenerationUsage = calculateUsage(youngGeneration);
    stats.oldGenerationUsage = calculateUsage(oldGeneration);
    stats.totalHeapUsage = stats.youngGenerationUsage + stats.oldGenerationUsage;
    
    // ピーク使用量の更新
    stats.peakHeapUsage = std::max(stats.peakHeapUsage, stats.totalHeapUsage);
    
    // 使用率の計算
    size_t totalCapacity = youngGeneration.size + oldGeneration.size;
    if (totalCapacity > 0) {
        stats.heapUtilization = static_cast<double>(stats.totalHeapUsage) / totalCapacity;
    }
}

void GenerationalGC::updateFragmentationStatistics() {
    // ヒープ断片化統計の更新
    
    stats.youngGenerationFragmentation = calculateFragmentation(youngGeneration);
    stats.oldGenerationFragmentation = calculateFragmentation(oldGeneration);
    
    // 全体の断片化率
    double totalFree = (youngGeneration.size - stats.youngGenerationUsage) +
                      (oldGeneration.size - stats.oldGenerationUsage);
    double largestFree = std::max(getLargestFreeBlock(youngGeneration),
                                 getLargestFreeBlock(oldGeneration));
    
    if (totalFree > 0) {
        stats.overallFragmentation = 1.0 - (largestFree / totalFree);
    }
}

void GenerationalGC::updateGenerationalStatistics() {
    // 世代別統計の更新
    
    // オブジェクト世代分布
    size_t youngObjects = countObjectsInGeneration(0);
    size_t oldObjects = countObjectsInGeneration(1);
    
    stats.youngGenerationObjects = youngObjects;
    stats.oldGenerationObjects = oldObjects;
    
    // 昇格率の計算
    if (stats.lastPromotedObjects > 0 && youngObjects > 0) {
        stats.promotionRate = static_cast<double>(stats.lastPromotedObjects) / 
                             (youngObjects + stats.lastPromotedObjects);
    }
    
    // 生存率の計算
    if (stats.lastCollectedObjects > 0) {
        size_t survivedObjects = youngObjects + oldObjects;
        stats.survivalRate = static_cast<double>(survivedObjects) / 
                           (survivedObjects + stats.lastCollectedObjects);
    }
}

size_t GenerationalGC::calculateUsage(const Generation& generation) {
    size_t usage = 0;
    char* current = generation.allocPointer;
    char* start = generation.start;
    
    if (current >= start) {
        usage = current - start;
    }
    
    return usage;
}

double GenerationalGC::calculateFragmentation(const Generation& generation) {
    size_t totalFree = generation.size - calculateUsage(generation);
    size_t largestFree = getLargestFreeBlock(generation);
    
    if (totalFree > 0) {
        return 1.0 - (static_cast<double>(largestFree) / totalFree);
    }
    
    return 0.0;
}

size_t GenerationalGC::getLargestFreeBlock(const Generation& generation) {
    // 最大空きブロックサイズの計算
    size_t largestBlock = 0;
    size_t currentBlock = 0;
    
    char* ptr = generation.allocPointer;
    char* end = generation.end;
    
    while (ptr < end) {
        if (isFreeMemory(ptr)) {
            currentBlock++;
            ptr++;
        } else {
            largestBlock = std::max(largestBlock, currentBlock);
            currentBlock = 0;
            
            // オブジェクトをスキップ
            ObjectHeader* obj = reinterpret_cast<ObjectHeader*>(ptr);
            ptr += obj->getSize();
        }
    }
    
    largestBlock = std::max(largestBlock, currentBlock);
    return largestBlock;
}

size_t GenerationalGC::countObjectsInGeneration(int generation) {
    size_t count = 0;
    
    const Generation& gen = (generation == 0) ? youngGeneration : oldGeneration;
    
    char* ptr = gen.start;
    char* end = gen.allocPointer;
    
    while (ptr < end) {
        ObjectHeader* obj = reinterpret_cast<ObjectHeader*>(ptr);
        if (obj && obj->hasValidMagicNumber()) {
            count++;
            ptr += obj->getSize();
        } else {
            // 不正なオブジェクト、次のワード境界に進む
            ptr += sizeof(void*);
        }
    }
    
    return count;
}

bool GenerationalGC::isFreeMemory(void* ptr) {
    // メモリが空きかどうかの判定
    ObjectHeader* obj = reinterpret_cast<ObjectHeader*>(ptr);
    return !obj || !obj->hasValidMagicNumber();
}

// 最終的なリソースクリーンアップ
void GenerationalGC::cleanup() {
    // 全オブジェクトの最終化
    finalizeAllObjects();
    
    // ヒープメモリの解放
    deallocateGeneration(youngGeneration);
    deallocateGeneration(oldGeneration);
    
    // 統計情報のクリア
    stats = GCStatistics{};
    
    // ルートセットのクリア
    {
        std::lock_guard<std::mutex> lock(rootsMutex);
        rootSet.clear();
    }
    
    // 弱参照テーブルのクリア
    weakReferences.clear();
}

void GenerationalGC::finalizeAllObjects() {
    // 全オブジェクトのファイナライザを実行
    finalizeObjectsInGeneration(youngGeneration);
    finalizeObjectsInGeneration(oldGeneration);
}

void GenerationalGC::finalizeObjectsInGeneration(const Generation& generation) {
    char* ptr = generation.start;
    char* end = generation.allocPointer;
    
    while (ptr < end) {
        ObjectHeader* obj = reinterpret_cast<ObjectHeader*>(ptr);
        if (obj && obj->hasValidMagicNumber() && obj->hasFinalizer()) {
            obj->runFinalizer();
        }
        
        ptr += obj ? obj->getSize() : sizeof(void*);
    }
}

void GenerationalGC::deallocateGeneration(Generation& generation) {
    if (generation.start) {
        std::free(generation.start);
        generation.start = nullptr;
        generation.allocPointer = nullptr;
        generation.end = nullptr;
        generation.size = 0;
    }
}

} // namespace memory
} // namespace utils
} // namespace aerojs 