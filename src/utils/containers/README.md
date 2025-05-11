# AeroJS コンテナライブラリ

## 概要

AeroJSエンジン用の高性能コンテナライブラリです。JavaScript実行環境で必要なデータ構造を効率的に実装し、メモリ使用量と実行速度のバランスを最適化しています。

## 主要コンポーネント

### ハッシュマップ

- **HashMap**: 高速なキーバリューマッピング
- **StringMap**: 文字列キー特化のマップ
- **IdentityMap**: アイデンティティ比較による参照マップ
- **StringHasher**: 文字列用高性能ハッシュ関数

### 動的配列

- **Vector**: 伸縮自在な配列
- **SmallVector**: 小さな配列に最適化
- **InlineVector**: スタック割り当て小配列

### 文字列

- **String**: 効率的な文字列表現
- **StringView**: 所有権なしの文字列参照
- **StringBuilder**: 文字列連結最適化
- **StringPool**: 文字列インターニング

### 特殊コンテナ

- **BitSet**: コンパクトなビットフラグ
- **IntrusiveList**: 侵入型リンクリスト
- **LRUCache**: 最近使用されたアイテムキャッシュ
- **OrderedMap**: 挿入順序を保持するマップ

## ハッシュマップの特徴

ハッシュマップは特にJavaScriptオブジェクトの実装に重要で、以下の特性を備えています：

- 低衝突ハッシュ関数
- オープンアドレッシング方式
- ロビンフッドハッシュ法
- サイズ自動調整
- 高速イテレーション

```cpp
// ハッシュマップの例
HashMap<Value, Value> map(allocator);
map.set(Value("key1"), Value(123));
map.set(Value("key2"), Value(456));

// 値の取得
Value result = map.get(Value("key1"));  // 123
```

## 文字列処理の最適化

JavaScript文字列処理を効率化するための特殊な実装：

- UTF-16エンコーディング
- 小さな文字列用のスモールストリング最適化
- ロープ表現による大きな文字列の効率的操作
- COW（Copy-On-Write）セマンティクス
- 文字列インターニングによる重複削減

```cpp
// 文字列の例
String str1 = String::fromUTF8("Hello");
String str2 = String::fromUTF8("World");
String result = StringBuilder()
                .append(str1)
                .append(", ")
                .append(str2)
                .toString();  // "Hello, World"
```

## パフォーマンス特性

AeroJSコンテナは以下の特性を目指して最適化されています：

- **メモリ効率**: 最小限のオーバーヘッドでメモリ使用
- **キャッシュ効率**: CPUキャッシュラインを考慮した設計
- **アロケーション最小化**: 不要なメモリ割り当てを回避
- **イテレーション速度**: 順次アクセスの高速化

## カスタムアロケータサポート

全てのコンテナはカスタムアロケータをサポートし、メモリ管理戦略を柔軟に制御できます：

```cpp
// カスタムアロケータの使用
Vector<Value, CustomAllocator> vec(customAllocator);
vec.pushBack(Value(1));
vec.pushBack(Value(2));
```

## サブディレクトリ

- **`hashmap/`**: ハッシュマップ実装
- **`string/`**: 文字列関連コンテナ
- **`vector/`**: 動的配列実装
- **`list/`**: リスト型コンテナ実装
- **`algorithms/`**: コンテナアルゴリズム 