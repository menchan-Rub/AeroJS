/**
 * @file array.cpp
 * @brief JavaScript Arrayクラスの実装
 * @version 0.1.0
 * @license MIT
 */

#include "array.h"
#include "value.h"
#include "string.h"

#include <algorithm>
#include <sstream>
#include <limits>

namespace aerojs {
namespace core {

// コンストラクタ
Array::Array() : Object(), length_(0) {
}

Array::Array(uint32_t length) : Object(), length_(length) {
  // 適切なサイズに予約
  if (length <= 100) {
    elements_.resize(length, Value::createUndefined());
  }
  // 大きなサイズの場合はスパース配列として処理
}

Array::Array(std::initializer_list<Value*> elements) : Object(), length_(static_cast<uint32_t>(elements.size())) {
  elements_.reserve(elements.size());
  for (Value* element : elements) {
    elements_.push_back(element);
    if (element) {
      element->ref();
    }
  }
}

Array::~Array() {
  // 要素の参照を解放
  for (Value* element : elements_) {
    if (element) {
      element->unref();
    }
  }
  
  for (auto& pair : sparse_) {
    if (pair.second) {
      pair.second->unref();
    }
  }
}

Object::Type Array::getType() const {
  return Type::Array;
}

void Array::setLength(uint32_t newLength) {
  if (newLength < length_) {
    // 長さを縮める場合、余分な要素を削除
    for (uint32_t i = newLength; i < length_; i++) {
      deleteElement(i);
    }
    
    if (newLength < elements_.size()) {
      // 密な部分も調整
      for (uint32_t i = newLength; i < elements_.size(); i++) {
        if (elements_[i]) {
          elements_[i]->unref();
        }
      }
      elements_.resize(newLength);
    }
  }
  
  length_ = newLength;
}

Value* Array::getElement(uint32_t index) const {
  if (index >= length_) {
    return Value::createUndefined();
  }
  
  // 密な部分をチェック
  if (index < elements_.size()) {
    return elements_[index] ? elements_[index] : Value::createUndefined();
  }
  
  // スパース部分をチェック
  auto it = sparse_.find(index);
  if (it != sparse_.end()) {
    return it->second;
  }
  
  return Value::createUndefined();
}

bool Array::setElement(uint32_t index, Value* value) {
  // lengthを超える場合は拡張
  if (index >= length_) {
    length_ = index + 1;
  }
  
  // 密な部分に設定可能かチェック
  if (index < 1000 && index < elements_.size() + 100) {
    // 密な配列として管理
    if (index >= elements_.size()) {
      elements_.resize(index + 1, nullptr);
    }
    
    if (elements_[index]) {
      elements_[index]->unref();
    }
    
    elements_[index] = value;
    if (value) {
      value->ref();
    }
  } else {
    // スパース配列として管理
    auto it = sparse_.find(index);
    if (it != sparse_.end() && it->second) {
      it->second->unref();
    }
    
    sparse_[index] = value;
    if (value) {
      value->ref();
    }
  }
  
  return true;
}

bool Array::deleteElement(uint32_t index) {
  if (index >= length_) {
    return true;
  }
  
  // 密な部分から削除
  if (index < elements_.size() && elements_[index]) {
    elements_[index]->unref();
    elements_[index] = nullptr;
  }
  
  // スパース部分から削除
  auto it = sparse_.find(index);
  if (it != sparse_.end()) {
    if (it->second) {
      it->second->unref();
    }
    sparse_.erase(it);
  }
  
  return true;
}

bool Array::hasElement(uint32_t index) const {
  if (index >= length_) {
    return false;
  }
  
  // 密な部分をチェック
  if (index < elements_.size() && elements_[index]) {
    return true;
  }
  
  // スパース部分をチェック
  return sparse_.find(index) != sparse_.end();
}

uint32_t Array::push(Value* value) {
  setElement(length_, value);
  return length_;
}

Value* Array::pop() {
  if (length_ == 0) {
    return Value::createUndefined();
  }
  
  Value* result = getElement(length_ - 1);
  deleteElement(length_ - 1);
  length_--;
  
  // 結果の参照を追加（削除時に解放されるため）
  if (result) {
    result->ref();
  }
  
  return result;
}

uint32_t Array::unshift(Value* value) {
  // すべての要素を1つ右にシフト
  for (int32_t i = static_cast<int32_t>(length_) - 1; i >= 0; i--) {
    Value* element = getElement(static_cast<uint32_t>(i));
    setElement(static_cast<uint32_t>(i + 1), element);
  }
  
  setElement(0, value);
  return length_;
}

Value* Array::shift() {
  if (length_ == 0) {
    return Value::createUndefined();
  }
  
  Value* result = getElement(0);
  
  // すべての要素を1つ左にシフト
  for (uint32_t i = 1; i < length_; i++) {
    Value* element = getElement(i);
    setElement(i - 1, element);
  }
  
  deleteElement(length_ - 1);
  length_--;
  
  // 結果の参照を追加
  if (result) {
    result->ref();
  }
  
  return result;
}

Array* Array::splice(int32_t start, uint32_t deleteCount, const std::vector<Value*>& items) {
  // 開始位置を正規化
  int32_t actualStart = normalizeIndex(start, length_);
  
  // 削除する要素数を調整
  uint32_t actualDeleteCount = std::min(deleteCount, length_ - static_cast<uint32_t>(actualStart));
  
  // 削除された要素を保存する新しい配列
  Array* deletedElements = new Array();
  
  // 削除される要素を新しい配列に追加
  for (uint32_t i = 0; i < actualDeleteCount; i++) {
    Value* element = getElement(static_cast<uint32_t>(actualStart) + i);
    deletedElements->push(element);
  }
  
  // 新しい長さを計算
  uint32_t newLength = length_ - actualDeleteCount + static_cast<uint32_t>(items.size());
  
  // 要素をシフト
  if (items.size() != actualDeleteCount) {
    int32_t shift = static_cast<int32_t>(items.size()) - static_cast<int32_t>(actualDeleteCount);
    
    if (shift > 0) {
      // 右にシフト（挿入）
      for (int32_t i = static_cast<int32_t>(length_) - 1; i >= actualStart + static_cast<int32_t>(actualDeleteCount); i--) {
        Value* element = getElement(static_cast<uint32_t>(i));
        setElement(static_cast<uint32_t>(i + shift), element);
      }
    } else if (shift < 0) {
      // 左にシフト（削除）
      for (uint32_t i = static_cast<uint32_t>(actualStart + static_cast<int32_t>(actualDeleteCount)); i < length_; i++) {
        Value* element = getElement(i);
        setElement(static_cast<uint32_t>(static_cast<int32_t>(i) + shift), element);
      }
    }
  }
  
  // 新しい要素を挿入
  for (size_t i = 0; i < items.size(); i++) {
    setElement(static_cast<uint32_t>(actualStart) + static_cast<uint32_t>(i), items[i]);
  }
  
  // 長さを更新
  setLength(newLength);
  
  return deletedElements;
}

Array* Array::slice(int32_t start, int32_t end) const {
  // インデックスを正規化
  int32_t actualStart = normalizeIndex(start, length_);
  int32_t actualEnd = (end == -1) ? static_cast<int32_t>(length_) : normalizeIndex(end, length_);
  
  if (actualStart >= actualEnd) {
    return new Array();
  }
  
  Array* result = new Array();
  for (int32_t i = actualStart; i < actualEnd; i++) {
    Value* element = getElement(static_cast<uint32_t>(i));
    result->push(element);
  }
  
  return result;
}

Array* Array::concat(const std::vector<Value*>& values) const {
  Array* result = new Array();
  
  // 自身の要素をコピー
  for (uint32_t i = 0; i < length_; i++) {
    Value* element = getElement(i);
    result->push(element);
  }
  
  // 追加の値を結合
  for (Value* value : values) {
    if (value && value->isArray()) {
      // 配列の場合は要素を展開
      Array* arr = static_cast<Array*>(value->asObject());
      for (uint32_t i = 0; i < arr->length(); i++) {
        Value* element = arr->getElement(i);
        result->push(element);
      }
    } else {
      // 単一の値として追加
      result->push(value);
    }
  }
  
  return result;
}

std::string Array::join(const std::string& separator) const {
  std::ostringstream oss;
  bool first = true;
  
  for (uint32_t i = 0; i < length_; i++) {
    if (!first) {
      oss << separator;
    }
    first = false;
    
    Value* element = getElement(i);
    if (element && !element->isUndefined() && !element->isNull()) {
      oss << element->toString();
    }
  }
  
  return oss.str();
}

Array* Array::reverse() {
  uint32_t half = length_ / 2;
  
  for (uint32_t i = 0; i < half; i++) {
    uint32_t j = length_ - 1 - i;
    
    Value* temp = getElement(i);
    setElement(i, getElement(j));
    setElement(j, temp);
  }
  
  return this;
}

Array* Array::sort(std::function<int(Value*, Value*)> compareFn) {
  if (!compareFn) {
    compareFn = defaultCompare;
  }
  
  // 有効な要素のインデックスを取得
  std::vector<uint32_t> indices = getValidIndices();
  
  // インデックスに対応する値を取得
  std::vector<std::pair<uint32_t, Value*>> indexValuePairs;
  for (uint32_t index : indices) {
    Value* value = getElement(index);
    indexValuePairs.emplace_back(index, value);
  }
  
  // ソート
  std::sort(indexValuePairs.begin(), indexValuePairs.end(),
    [&compareFn](const std::pair<uint32_t, Value*>& a, const std::pair<uint32_t, Value*>& b) {
      return compareFn(a.second, b.second) < 0;
    });
  
  // 元の位置をクリア
  for (uint32_t index : indices) {
    deleteElement(index);
  }
  
  // ソートされた順序で再配置
  for (size_t i = 0; i < indexValuePairs.size(); i++) {
    setElement(static_cast<uint32_t>(i), indexValuePairs[i].second);
  }
  
  return this;
}

int32_t Array::indexOf(Value* searchElement, int32_t fromIndex) const {
  int32_t start = normalizeIndex(fromIndex, length_);
  
  for (uint32_t i = static_cast<uint32_t>(start); i < length_; i++) {
    Value* element = getElement(i);
    if (element && searchElement && element->strictEquals(searchElement)) {
      return static_cast<int32_t>(i);
    }
  }
  
  return -1;
}

int32_t Array::lastIndexOf(Value* searchElement, int32_t fromIndex) const {
  int32_t start = (fromIndex == -1) ? static_cast<int32_t>(length_) - 1 : normalizeIndex(fromIndex, length_);
  
  for (int32_t i = start; i >= 0; i--) {
    Value* element = getElement(static_cast<uint32_t>(i));
    if (element && searchElement && element->strictEquals(searchElement)) {
      return i;
    }
  }
  
  return -1;
}

bool Array::some(std::function<bool(Value*, uint32_t, Array*)> predicate) const {
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      if (predicate(element, i, const_cast<Array*>(this))) {
        return true;
      }
    }
  }
  return false;
}

bool Array::every(std::function<bool(Value*, uint32_t, Array*)> predicate) const {
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      if (!predicate(element, i, const_cast<Array*>(this))) {
        return false;
      }
    }
  }
  return true;
}

void Array::forEach(std::function<void(Value*, uint32_t, Array*)> callback) const {
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      callback(element, i, const_cast<Array*>(this));
    }
  }
}

Array* Array::map(std::function<Value*(Value*, uint32_t, Array*)> callback) const {
  Array* result = new Array(length_);
  
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      Value* mappedValue = callback(element, i, const_cast<Array*>(this));
      result->setElement(i, mappedValue);
    }
  }
  
  return result;
}

Array* Array::filter(std::function<bool(Value*, uint32_t, Array*)> predicate) const {
  Array* result = new Array();
  
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      if (predicate(element, i, const_cast<Array*>(this))) {
        result->push(element);
      }
    }
  }
  
  return result;
}

Value* Array::reduce(std::function<Value*(Value*, Value*, uint32_t, Array*)> callback, Value* initialValue) const {
  uint32_t start = 0;
  Value* accumulator = initialValue;
  
  if (!accumulator) {
    // 初期値が提供されない場合、最初の要素を使用
    for (uint32_t i = 0; i < length_; i++) {
      if (hasElement(i)) {
        accumulator = getElement(i);
        start = i + 1;
        break;
      }
    }
    
    if (!accumulator) {
      // 空の配列で初期値なしの場合はTypeError
      return Value::createUndefined();
    }
  }
  
  for (uint32_t i = start; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      accumulator = callback(accumulator, element, i, const_cast<Array*>(this));
    }
  }
  
  return accumulator;
}

Value* Array::reduceRight(std::function<Value*(Value*, Value*, uint32_t, Array*)> callback, Value* initialValue) const {
  int32_t start = static_cast<int32_t>(length_) - 1;
  Value* accumulator = initialValue;
  
  if (!accumulator) {
    // 初期値が提供されない場合、最後の要素を使用
    for (int32_t i = start; i >= 0; i--) {
      if (hasElement(static_cast<uint32_t>(i))) {
        accumulator = getElement(static_cast<uint32_t>(i));
        start = i - 1;
        break;
      }
    }
    
    if (!accumulator) {
      // 空の配列で初期値なしの場合はTypeError
      return Value::createUndefined();
    }
  }
  
  for (int32_t i = start; i >= 0; i--) {
    if (hasElement(static_cast<uint32_t>(i))) {
      Value* element = getElement(static_cast<uint32_t>(i));
      accumulator = callback(accumulator, element, static_cast<uint32_t>(i), const_cast<Array*>(this));
    }
  }
  
  return accumulator;
}

Value* Array::find(std::function<bool(Value*, uint32_t, Array*)> predicate) const {
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      if (predicate(element, i, const_cast<Array*>(this))) {
        return element;
      }
    }
  }
  return Value::createUndefined();
}

int32_t Array::findIndex(std::function<bool(Value*, uint32_t, Array*)> predicate) const {
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      if (predicate(element, i, const_cast<Array*>(this))) {
        return static_cast<int32_t>(i);
      }
    }
  }
  return -1;
}

bool Array::includes(Value* searchElement, int32_t fromIndex) const {
  return indexOf(searchElement, fromIndex) != -1;
}

Array* Array::flat(uint32_t depth) const {
  Array* result = new Array();
  
  for (uint32_t i = 0; i < length_; i++) {
    if (hasElement(i)) {
      Value* element = getElement(i);
      flattenHelper(result, element, depth);
    }
  }
  
  return result;
}

Array* Array::flatMap(std::function<Value*(Value*, uint32_t, Array*)> callback, uint32_t depth) const {
  Array* mapped = map(callback);
  Array* result = mapped->flat(depth);
  delete mapped;
  return result;
}

std::vector<uint32_t> Array::getValidIndices() const {
  std::vector<uint32_t> indices;
  
  // 密な部分のインデックス
  for (uint32_t i = 0; i < elements_.size() && i < length_; i++) {
    if (elements_[i]) {
      indices.push_back(i);
    }
  }
  
  // スパース部分のインデックス
  for (const auto& pair : sparse_) {
    if (pair.first < length_) {
      indices.push_back(pair.first);
    }
  }
  
  // ソートして重複を除去
  std::sort(indices.begin(), indices.end());
  indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
  
  return indices;
}

bool Array::isDense() const {
  return sparse_.empty() && elements_.size() == length_;
}

std::string Array::toString() const {
  return "[object Array]";
}

// ヘルパー関数の実装
void Array::ensureCapacity(uint32_t minCapacity) {
  if (elements_.capacity() < minCapacity) {
    elements_.reserve(std::max(minCapacity, static_cast<uint32_t>(elements_.capacity() * 2)));
  }
}

void Array::compactIfNeeded() {
  // スパース配列を密な配列に変換する条件をチェック
  if (sparse_.size() > 0 && sparse_.size() < length_ / 4) {
    // スパース要素が少ない場合は密な配列に変換
    for (const auto& pair : sparse_) {
      if (pair.first < 1000) {  // 適度なサイズ制限
        setElement(pair.first, pair.second);
      }
    }
  }
}

bool Array::isValidArrayIndex(uint32_t index) const {
  return index < std::numeric_limits<uint32_t>::max() - 1;
}

int32_t Array::normalizeIndex(int32_t index, uint32_t length) const {
  if (index < 0) {
    index += static_cast<int32_t>(length);
  }
  return std::max(0, std::min(index, static_cast<int32_t>(length)));
}

void Array::flattenHelper(Array* result, Value* element, uint32_t depth) const {
  if (depth > 0 && element && element->isArray()) {
    Array* arr = static_cast<Array*>(element->asObject());
    for (uint32_t i = 0; i < arr->length(); i++) {
      if (arr->hasElement(i)) {
        Value* subElement = arr->getElement(i);
        flattenHelper(result, subElement, depth - 1);
      }
    }
  } else {
    result->push(element);
  }
}

int Array::defaultCompare(Value* a, Value* b) {
  if (!a && !b) return 0;
  if (!a) return -1;
  if (!b) return 1;
  
  std::string strA = a->toString();
  std::string strB = b->toString();
  
  if (strA < strB) return -1;
  if (strA > strB) return 1;
  return 0;
}

// ファクトリ関数
Array* Array::create() {
  return new Array();
}

Array* Array::create(uint32_t length) {
  return new Array(length);
}

Array* Array::from(const std::vector<Value*>& elements) {
  Array* result = new Array();
  for (Value* element : elements) {
    result->push(element);
  }
  return result;
}

}  // namespace core
}  // namespace aerojs 