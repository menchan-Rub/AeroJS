#ifndef AEROJS_CORE_JIT_IC_INLINE_CACHE_H
#define AEROJS_CORE_JIT_IC_INLINE_CACHE_H

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aerojs {
namespace core {

// 前方宣言
class JSObject;
class Shape;
class JSValue;

// インラインキャッシュのステータス
enum class ICStatus : uint8_t {
  Uninitialized,  // 未初期化状態
  Monomorphic,    // 単一形状（最も効率的）
  Polymorphic,    // 少数形状（2-4程度）
  Megamorphic     // 多数形状（汎用処理に切り替え）
};

// ICエントリの基底クラス
class ICEntry {
public:
  ICEntry() = default;
  virtual ~ICEntry() = default;
  
  // 形状IDの取得
  virtual uint32_t GetShapeId() const noexcept = 0;
  
  // キャッシュが有効かどうかのチェック
  virtual bool IsValid() const noexcept = 0;
  
  // エントリの無効化
  virtual void Invalidate() noexcept = 0;
};

// プロパティアクセス用ICエントリ
class PropertyICEntry : public ICEntry {
public:
  PropertyICEntry(uint32_t shapeId, uint32_t offset, bool isInline) noexcept;
  ~PropertyICEntry() override = default;
  
  // 形状IDの取得
  uint32_t GetShapeId() const noexcept override { return m_shapeId; }
  
  // プロパティのオフセット取得
  uint32_t GetOffset() const noexcept { return m_offset; }
  
  // インラインプロパティかどうか
  bool IsInlineProperty() const noexcept { return m_isInline; }
  
  // キャッシュが有効かどうかのチェック
  bool IsValid() const noexcept override { return m_isValid; }
  
  // エントリの無効化
  void Invalidate() noexcept override { m_isValid = false; }

private:
  uint32_t m_shapeId;  // オブジェクト形状ID
  uint32_t m_offset;   // プロパティのオフセット
  bool m_isInline;     // インラインプロパティかどうか
  bool m_isValid;      // 有効性フラグ
};

// メソッド呼び出し用ICエントリ
class MethodICEntry : public ICEntry {
public:
  MethodICEntry(uint32_t shapeId, uint32_t methodId, void* nativeCode) noexcept;
  ~MethodICEntry() override = default;
  
  // 形状IDの取得
  uint32_t GetShapeId() const noexcept override { return m_shapeId; }
  
  // メソッドIDの取得
  uint32_t GetMethodId() const noexcept { return m_methodId; }
  
  // ネイティブコードポインタの取得
  void* GetNativeCode() const noexcept { return m_nativeCode; }
  
  // キャッシュが有効かどうかのチェック
  bool IsValid() const noexcept override { return m_isValid; }
  
  // エントリの無効化
  void Invalidate() noexcept override { m_isValid = false; }

private:
  uint32_t m_shapeId;   // オブジェクト形状ID
  uint32_t m_methodId;  // メソッドID
  void* m_nativeCode;   // ネイティブコードへのポインタ
  bool m_isValid;       // 有効性フラグ
};

// プロトタイプチェーン用ICエントリ
class ProtoICEntry : public ICEntry {
public:
  ProtoICEntry(uint32_t shapeId, uint32_t protoShapeId, uint32_t offset) noexcept;
  ~ProtoICEntry() override = default;
  
  // 形状IDの取得
  uint32_t GetShapeId() const noexcept override { return m_shapeId; }
  
  // プロトタイプの形状IDの取得
  uint32_t GetProtoShapeId() const noexcept { return m_protoShapeId; }
  
  // プロパティのオフセット取得
  uint32_t GetOffset() const noexcept { return m_offset; }
  
  // キャッシュが有効かどうかのチェック
  bool IsValid() const noexcept override { return m_isValid; }
  
  // エントリの無効化
  void Invalidate() noexcept override { m_isValid = false; }

private:
  uint32_t m_shapeId;      // オブジェクト形状ID
  uint32_t m_protoShapeId; // プロトタイプオブジェクト形状ID
  uint32_t m_offset;       // プロパティのオフセット
  bool m_isValid;          // 有効性フラグ
};

// モノモーフィック(単一形状)インラインキャッシュ
class MonomorphicIC {
public:
  MonomorphicIC() noexcept;
  ~MonomorphicIC() = default;
  
  // プロパティアクセス用ICエントリの設定
  void Set(std::unique_ptr<PropertyICEntry> entry) noexcept;
  
  // メソッド呼び出し用ICエントリの設定
  void Set(std::unique_ptr<MethodICEntry> entry) noexcept;
  
  // プロトタイプチェーン用ICエントリの設定
  void Set(std::unique_ptr<ProtoICEntry> entry) noexcept;
  
  // ICエントリの取得
  ICEntry* Get() const noexcept { return m_entry.get(); }
  
  // ステータスの取得
  ICStatus GetStatus() const noexcept;
  
  // キャッシュの無効化
  void Invalidate() noexcept;
  
  // キャッシュのリセット
  void Reset() noexcept;

private:
  std::unique_ptr<ICEntry> m_entry;
};

// ポリモーフィック(少数形状)インラインキャッシュ
class PolymorphicIC {
public:
  static constexpr size_t kMaxEntries = 4;
  
  PolymorphicIC() noexcept;
  ~PolymorphicIC() = default;
  
  // プロパティアクセス用ICエントリの追加
  bool Add(std::unique_ptr<PropertyICEntry> entry) noexcept;
  
  // メソッド呼び出し用ICエントリの追加
  bool Add(std::unique_ptr<MethodICEntry> entry) noexcept;
  
  // プロトタイプチェーン用ICエントリの追加
  bool Add(std::unique_ptr<ProtoICEntry> entry) noexcept;
  
  // 形状IDに対応するICエントリの検索
  ICEntry* Find(uint32_t shapeId) const noexcept;
  
  // エントリ数の取得
  size_t GetEntryCount() const noexcept { return m_entries.size(); }
  
  // 空き容量の確認
  bool HasSpace() const noexcept { return m_entries.size() < kMaxEntries; }
  
  // ステータスの取得
  ICStatus GetStatus() const noexcept;
  
  // キャッシュの無効化
  void Invalidate() noexcept;
  
  // キャッシュのリセット
  void Reset() noexcept;

private:
  std::vector<std::unique_ptr<ICEntry>> m_entries;
};

// インラインキャッシュマネージャ
class InlineCacheManager {
public:
  InlineCacheManager() noexcept;
  ~InlineCacheManager() = default;
  
  // シングルトンインスタンスの取得
  static InlineCacheManager& Instance() noexcept;
  
  // プロパティアクセス用キャッシュの作成
  uint32_t CreatePropertyCache(const std::string& propertyName) noexcept;
  
  // メソッド呼び出し用キャッシュの作成
  uint32_t CreateMethodCache(const std::string& methodName) noexcept;
  
  // プロパティアクセス用キャッシュの取得
  MonomorphicIC* GetPropertyCache(uint32_t cacheId) noexcept;
  
  // メソッド呼び出し用キャッシュの取得
  MonomorphicIC* GetMethodCache(uint32_t cacheId) noexcept;
  
  // プロパティアクセス用キャッシュのアップグレード（モノモーフィック→ポリモーフィック）
  PolymorphicIC* UpgradePropertyCache(uint32_t cacheId) noexcept;
  
  // メソッド呼び出し用キャッシュのアップグレード（モノモーフィック→ポリモーフィック）
  PolymorphicIC* UpgradeMethodCache(uint32_t cacheId) noexcept;
  
  // オブジェクト形状変更時のキャッシュ無効化
  void InvalidateCachesForShape(uint32_t shapeId) noexcept;
  
  // 全キャッシュのリセット
  void Reset() noexcept;

private:
  // キャッシュIDの生成
  uint32_t GenerateCacheId() noexcept { return m_nextCacheId++; }

private:
  uint32_t m_nextCacheId;
  std::unordered_map<uint32_t, MonomorphicIC> m_propertyCaches;
  std::unordered_map<uint32_t, MonomorphicIC> m_methodCaches;
  std::unordered_map<uint32_t, PolymorphicIC> m_polyPropertyCaches;
  std::unordered_map<uint32_t, PolymorphicIC> m_polyMethodCaches;
  
  // 形状IDからキャッシュIDへのマッピング（無効化のため）
  std::unordered_map<uint32_t, std::vector<uint32_t>> m_shapeToCaches;
};

// プロパティアクセスのための補助関数
JSValue GetPropertyCached(JSObject* obj, uint32_t cacheId, const std::string& propertyName);
void SetPropertyCached(JSObject* obj, uint32_t cacheId, const std::string& propertyName, const JSValue& value);

// メソッド呼び出しのための補助関数
JSValue CallMethodCached(JSObject* obj, uint32_t cacheId, const std::string& methodName, const std::vector<JSValue>& args);

}  // namespace core
}  // namespace aerojs

#endif // AEROJS_CORE_JIT_IC_INLINE_CACHE_H 