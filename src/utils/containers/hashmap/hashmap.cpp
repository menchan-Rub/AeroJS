/**
 * @file hashmap.cpp
 * @brief ハッシュマップの実装
 * 
 * テンプレートクラスなので基本的にヘッダーに実装を含めていますが、
 * このファイルにはベンチマーク用の関数と明示的テンプレートのインスタンス化を含みます。
 * 
 * @author AeroJS Team
 * @version 2.0.0
 * @copyright MIT License
 */

#include "hashmap.h"
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>

namespace aerojs {
namespace utils {
namespace containers {

/**
 * @brief 明示的なテンプレートのインスタンス化
 * 
 * 最も一般的に使用される型のインスタンスを事前に生成します。
 * これにより、コンパイル時間が短縮され、バイナリサイズが最適化されます。
 */
template class HashMap<int, int>;
template class HashMap<int, double>;
template class HashMap<int, std::string>;
template class HashMap<std::string, int>;
template class HashMap<std::string, double>;
template class HashMap<std::string, std::string>;
template class HashMap<const char*, std::string>;

/**
 * @brief パフォーマンスベンチマーク関数
 * 
 * AeroJSのハッシュマップと標準ライブラリのunordered_mapを比較します。
 * @param test_size テストするエントリ数
 */
void run_hashmap_benchmark(size_t test_size = 1000000) {
    std::cout << "AeroJSハッシュマップベンチマーク開始（テストサイズ: " << test_size << "）\n";
    
    // 乱数生成器を初期化
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, std::numeric_limits<int>::max());
    
    // テストデータを生成
    std::vector<int> keys(test_size);
    std::vector<int> values(test_size);
    for (size_t i = 0; i < test_size; ++i) {
        keys[i] = dist(gen);
        values[i] = dist(gen);
    }
    
    // 挿入テスト - AeroJS HashMap
    {
        HashMap<int, int> hashmap;
        hashmap.reserve(test_size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            hashmap.insert(keys[i], values[i]);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "AeroJS HashMap 挿入時間: " << duration.count() << " ms\n";
    }
    
    // 挿入テスト - std::unordered_map
    {
        std::unordered_map<int, int> stdmap;
        stdmap.reserve(test_size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            stdmap.insert({keys[i], values[i]});
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::unordered_map 挿入時間: " << duration.count() << " ms\n";
    }
    
    // 参照前にハッシュマップを再構築
    HashMap<int, int> aero_map;
    std::unordered_map<int, int> std_map;
    
    // 両方のマップに要素を挿入
    for (size_t i = 0; i < test_size; ++i) {
        aero_map.insert(keys[i], values[i]);
        std_map.insert({keys[i], values[i]});
    }
    
    // 検索テスト - AeroJS HashMap
    {
        // 検索順序をシャッフル
        std::shuffle(keys.begin(), keys.end(), gen);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t result_sum = 0;
        for (size_t i = 0; i < test_size; ++i) {
            auto it = aero_map.get(keys[i]);
            if (it) {
                result_sum += it->get();
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "AeroJS HashMap 検索時間: " << duration.count() << " ms";
        std::cout << " (チェックサム: " << (result_sum & 0xFFFFFFFF) << ")\n";
    }
    
    // 検索テスト - std::unordered_map
    {
        // 同じ検索順序を使用
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t result_sum = 0;
        for (size_t i = 0; i < test_size; ++i) {
            auto it = std_map.find(keys[i]);
            if (it != std_map.end()) {
                result_sum += it->second;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::unordered_map 検索時間: " << duration.count() << " ms";
        std::cout << " (チェックサム: " << (result_sum & 0xFFFFFFFF) << ")\n";
    }
    
    // 削除テスト - AeroJS HashMap
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            aero_map.erase(keys[i]);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "AeroJS HashMap 削除時間: " << duration.count() << " ms\n";
    }
    
    // 削除テスト - std::unordered_map
    {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            std_map.erase(keys[i]);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::unordered_map 削除時間: " << duration.count() << " ms\n";
    }
    
    std::cout << "ベンチマーク完了\n";
}

/**
 * @brief 文字列キー用のパフォーマンステスト
 */
void run_string_hashmap_benchmark(size_t test_size = 500000) {
    std::cout << "文字列キーハッシュマップベンチマーク開始（テストサイズ: " << test_size << "）\n";
    
    // 乱数生成器を初期化
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> len_dist(5, 20);
    std::uniform_int_distribution<int> char_dist(97, 122); // a-z
    
    // テストデータを生成
    std::vector<std::string> keys(test_size);
    std::vector<int> values(test_size);
    
    for (size_t i = 0; i < test_size; ++i) {
        // ランダムな文字列を生成
        int len = len_dist(gen);
        std::string key;
        key.reserve(len);
        for (int j = 0; j < len; ++j) {
            key.push_back(static_cast<char>(char_dist(gen)));
        }
        keys[i] = key;
        values[i] = static_cast<int>(i);
    }
    
    // 挿入テスト - AeroJS HashMap
    {
        HashMap<std::string, int> hashmap;
        hashmap.reserve(test_size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            hashmap.insert(keys[i], values[i]);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "AeroJS HashMap<string> 挿入時間: " << duration.count() << " ms\n";
    }
    
    // 挿入テスト - std::unordered_map
    {
        std::unordered_map<std::string, int> stdmap;
        stdmap.reserve(test_size);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < test_size; ++i) {
            stdmap.insert({keys[i], values[i]});
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::unordered_map<string> 挿入時間: " << duration.count() << " ms\n";
    }
    
    // 参照前にハッシュマップを再構築
    HashMap<std::string, int> aero_map;
    std::unordered_map<std::string, int> std_map;
    
    // 両方のマップに要素を挿入
    for (size_t i = 0; i < test_size; ++i) {
        aero_map.insert(keys[i], values[i]);
        std_map.insert({keys[i], values[i]});
    }
    
    // 検索テスト - AeroJS HashMap
    {
        // 検索順序をシャッフル
        std::shuffle(keys.begin(), keys.end(), gen);
        
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t result_sum = 0;
        for (size_t i = 0; i < test_size; ++i) {
            auto it = aero_map.get(keys[i]);
            if (it) {
                result_sum += it->get();
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "AeroJS HashMap<string> 検索時間: " << duration.count() << " ms";
        std::cout << " (チェックサム: " << (result_sum & 0xFFFFFFFF) << ")\n";
    }
    
    // 検索テスト - std::unordered_map
    {
        // 同じ検索順序を使用
        auto start = std::chrono::high_resolution_clock::now();
        
        size_t result_sum = 0;
        for (size_t i = 0; i < test_size; ++i) {
            auto it = std_map.find(keys[i]);
            if (it != std_map.end()) {
                result_sum += it->second;
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "std::unordered_map<string> 検索時間: " << duration.count() << " ms";
        std::cout << " (チェックサム: " << (result_sum & 0xFFFFFFFF) << ")\n";
    }
    
    std::cout << "文字列ベンチマーク完了\n";
}

// コンパイル時のテスト用
void dummy_hashmap_test() {
    // 基本的な操作のテスト
    HashMap<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    
    // イテレーションのテスト
    for (const auto& [key, value] : map) {
        (void)key;   // 未使用警告を抑制
        (void)value; // 未使用警告を抑制
    }
    
    // 各種操作のテスト
    map.erase(2);
    map.contains(1);
    map[4] = "four";
    map.at(3);
    map.clear();
    map.empty();
    map.size();
    map.capacity();
    map.reserve(100);
    map.rehash(64);
    map.load_factor();
    map.max_load_factor();
    map.max_load_factor(0.8f);
}

} // namespace containers
} // namespace utils
} // namespace aerojs 