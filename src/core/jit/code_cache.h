#pragma once

#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <tsl/robin_map.h>
#include <vector>

namespace aerojs {
namespace core {

/**
 * @brief コンパイル済みコードバッファのキャッシュ
 *
 * バイトコードシーケンスから生成された機械語バイトバッファを
 * ハッシュキーで保持し、再コンパイルを回避します。
 */
class CodeCache {
 public:
  static CodeCache& Instance() noexcept {
    static CodeCache instance;
    return instance;
  }

  /**
   * @brief キャッシュ検索
   * @param key バイトコードハッシュキー
   * @return キャッシュに存在すればバイナリバッファ, なければ std::nullopt
   */
  std::optional<std::vector<uint8_t>> Lookup(size_t key) const noexcept {
    std::shared_lock lock(mutex_);
    auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  /**
   * @brief キャッシュ挿入
   * @param key ハッシュキー
   * @param codeBuf 機械語バッファ
   */
  void Insert(size_t key, const std::vector<uint8_t>& codeBuf) noexcept {
    std::unique_lock lock(mutex_);
    cache_[key] = codeBuf;
  }

 private:
  CodeCache() = default;
  ~CodeCache() = default;
  CodeCache(const CodeCache&) = delete;
  CodeCache& operator=(const CodeCache&) = delete;

  mutable std::shared_mutex mutex_;
  tsl::robin_map<size_t, std::vector<uint8_t>> cache_;
};

}  // namespace core
}  // namespace aerojs