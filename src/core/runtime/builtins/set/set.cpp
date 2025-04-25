/**
 * @file set.cpp
 * @brief JavaScriptのSet組み込みオブジェクトの実装
 * @copyright Copyright (c) 2023 AeroJS project authors
 */

#include "set.h"

#include <algorithm>
#include <stdexcept>

#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/function.h"
#include "core/runtime/global_object.h"
#include "core/runtime/symbol.h"
#include "core/runtime/iterator.h"

namespace aero {

// ValueHashの実装
size_t SetObject::ValueHash::operator()(const Value& value) const {
  if (value.isString()) {
    return std::hash<std::string>{}(value.toString());
  } else if (value.isNumber()) {
    return std::hash<double>{}(value.toNumber());
  } else if (value.isBoolean()) {
    return std::hash<bool>{}(value.toBoolean());
  } else if (value.isNull()) {
    return std::hash<std::nullptr_t>{}(nullptr);
  } else if (value.isUndefined()) {
    return 0;
  } else if (value.isObject()) {
    // オブジェクトの場合はポインタのハッシュ値を使用
    return std::hash<Object*>{}(value.asObject());
  } else if (value.isSymbol()) {
    // シンボルの場合はIDをハッシュ値として使用
    return std::hash<int>{}(value.asSymbol()->id());
  }

  // その他の型の場合はデフォルトのハッシュ
  return 0;
}

// ValueEqualの実装
bool SetObject::ValueEqual::operator()(const Value& lhs, const Value& rhs) const {
  // 型が異なる場合は常に不一致
  if (lhs.type() != rhs.type()) {
    return false;
  }

  // 型ごとの比較
  switch (lhs.type()) {
    case Value::Type::Undefined:
    case Value::Type::Null:
      return true;  // 同じ型のundefinedまたはnullは常に等しい

    case Value::Type::Boolean:
      return lhs.asBoolean() == rhs.asBoolean();

    case Value::Type::Number: {
      double lnum = lhs.asNumber();
      double rnum = rhs.asNumber();

      // NaNはNaNと等しい (通常のJavaScriptとは異なる、Setの仕様)
      if (std::isnan(lnum) && std::isnan(rnum)) {
        return true;
      }

      // +0と-0は同一とみなす
      if (lnum == 0 && rnum == 0) {
        return true;
      }

      return lnum == rnum;
    }

    case Value::Type::String:
      return lhs.asString()->value() == rhs.asString()->value();

    case Value::Type::Symbol:
      return lhs.asSymbol() == rhs.asSymbol();

    case Value::Type::Object:
      // オブジェクトは参照比較
      return lhs.asObject() == rhs.asObject();

    case Value::Type::BigInt:
      // BigIntの比較
      return lhs.asBigInt().equals(rhs.asBigInt());

    default:
      return false;
  }
}

// SetObjectの実装
SetObject::SetObject(Context* context, Object* prototype)
    : Object(context, prototype), m_values(), m_iterators() {
}

SetObject::~SetObject() {
  // イテレータを無効化
  for (auto* iterator : m_iterators) {
    if (iterator) {
      iterator->invalidate();
    }
  }
}

Value SetObject::add(const Value& value) {
  // 既に存在する値を上書き
  m_values[value] = value;
  return Value(this);  // メソッドチェーン用にthisを返す
}

bool SetObject::has(const Value& value) const {
  return m_values.find(value) != m_values.end();
}

bool SetObject::remove(const Value& value) {
  return m_values.erase(value) > 0;
}

void SetObject::clear() {
  m_values.clear();
  
  // イテレータを無効化
  for (auto* iterator : m_iterators) {
    if (iterator) {
      iterator->setDone(true);
    }
  }
}

size_t SetObject::size() const {
  return m_values.size();
}

void SetObject::registerIterator(SetIterator* iterator) {
  if (iterator) {
    m_iterators.insert(iterator);
  }
}

void SetObject::unregisterIterator(SetIterator* iterator) {
  if (iterator) {
    m_iterators.erase(iterator);
  }
}

// SetIteratorの実装
SetIterator::SetIterator(SetObject* set, IterationType type)
    : m_set(set), m_type(type), m_index(0), m_done(false), m_values() {
  // 値のスナップショットを作成
  if (set) {
    for (const auto& pair : set->m_values) {
      m_values.push_back(pair.second);
    }
  }
}

SetIterator::~SetIterator() {
  if (m_set) {
    m_set->unregisterIterator(this);
  }
}

void SetIterator::invalidate() {
  m_set = nullptr;
  m_done = true;
}

void SetIterator::setDone(bool done) {
  m_done = done;
}

Value SetIterator::next(Context* context) {
  if (m_done || !m_set || m_index >= m_values.size()) {
    m_done = true;
    // イテレーション終了を示すオブジェクトを返す
    Object* result = context->createObject();
    result->set("done", Value(true));
    result->set("value", Value::undefined());
    return Value(result);
  }

  // 現在の値を取得
  Value currentValue = m_values[m_index++];
  
  // 結果オブジェクトを作成
  Object* result = context->createObject();
  result->set("done", Value(false));
  
  // イテレーションタイプに応じた値を設定
  switch (m_type) {
    case IterationType::Values:
      result->set("value", currentValue);
      break;
      
    case IterationType::KeysAndValues:
      // [値, 値]のペアを作成
      Object* entry = context->createArray(2);
      entry->set("0", currentValue);
      entry->set("1", currentValue);
      entry->set("length", Value(2.0));
      result->set("value", Value(entry));
      break;
  }
  
  return Value(result);
}

// Setコンストラクタ関数の実装
Value setConstructor(Context* context, Value thisValue, const std::vector<Value>& args) {
  // new演算子なしで呼ばれた場合はエラー
  if (!thisValue.isConstructorCall()) {
    context->throwTypeError("Set constructor must be called with new");
    return Value();
  }

  // Setオブジェクトを作成
  SetObject* set = new SetObject(context, context->globalObject()->objectPrototype());

  // イテラブルな引数があれば、その要素を追加
  if (!args.empty() && !args[0].isUndefined() && !args[0].isNull()) {
    Value iterable = args[0];
    
    // イテレータを取得
    Value iteratorMethod = context->getIteratorMethod(iterable);
    if (!iteratorMethod.isUndefined()) {
      // イテレータを取得して要素を追加
      Value iterator = context->getIterator(iterable, iteratorMethod);
      if (!iterator.isUndefined() && iterator.isObject()) {
        // イテレータを使って要素を追加
        while (true) {
          Value next = context->iteratorNext(iterator);
          if (context->iteratorComplete(next)) {
            break;
          }
          
          Value value = context->iteratorValue(next);
          set->add(value);
          
          // 例外が発生したら中断
          if (context->hasException()) {
            return Value();
          }
        }
      }
    } else if (iterable.isObject()) {
      // 後方互換性のため、配列ライクなオブジェクトのサポート
      Object* obj = iterable.asObject();
      if (obj->has("length")) {
        Value lengthValue = obj->get("length");
        if (lengthValue.isNumber()) {
          size_t length = static_cast<size_t>(lengthValue.asNumber());
          
          for (size_t i = 0; i < length; ++i) {
            Value element = obj->get(std::to_string(i));
            set->add(element);
          }
        }
      }
    }
  }

  return Value(set);
}

// Set.prototype.add実装
Value setAdd(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.add called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  // 値が与えられていなければundefinedを追加
  if (args.empty()) {
    return set->add(Value::undefined());
  }

  // 値を追加
  return set->add(args[0]);
}

// Set.prototype.clear実装
Value setClear(Context* context, Value thisValue, const std::vector<Value>&) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.clear called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());
  set->clear();

  return Value::undefined();
}

// Set.prototype.delete実装
Value setDelete(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.delete called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  // 値が与えられていなければundefinedを削除する
  if (args.empty()) {
    return Value(set->remove(Value::undefined()));
  }

  // 値を削除
  return Value(set->remove(args[0]));
}

// Set.prototype.has実装
Value setHas(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.has called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  // 値が与えられていなければundefinedを確認
  if (args.empty()) {
    return Value(set->has(Value::undefined()));
  }

  // 値を確認
  return Value(set->has(args[0]));
}

// Set.prototype.forEach実装
Value setForEach(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.forEach called on non-Set object");
    return Value();
  }

  // コールバック関数が提供されていることを確認
  if (args.empty() || !args[0].isFunction()) {
    context->throwTypeError("Set.prototype.forEach requires a callback function");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());
  Value callback = args[0];
  Value thisArg = args.size() > 1 ? args[1] : Value::undefined();

  // すべての値に対してコールバックを実行
  std::vector<Value> callbackArgs(3);
  callbackArgs[2] = thisValue;  // 第3引数は常にSetオブジェクト自身

  for (const auto& pair : set->m_values) {
    callbackArgs[0] = pair.second;  // 値
    callbackArgs[1] = pair.second;  // 値と同じ（Mapと違い）

    // コールバック関数を実行
    context->callFunction(callback.asObject()->asFunction(), thisArg, callbackArgs);

    // 例外が発生したら即座に停止
    if (context->hasException()) {
      return Value();
    }
  }

  return Value::undefined();
}

// Set.prototype.values実装
Value setValues(Context* context, Value thisValue, const std::vector<Value>&) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.values called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  // SetIteratorオブジェクトを作成して返す
  Object* iteratorObj = context->createObject(context->getSetIteratorPrototype());
  SetIterator* iterator = new SetIterator(set, SetIterator::IterationType::Values);
  
  // イテレータの状態を設定
  iteratorObj->setInternalSlot("iterator", Value(iterator));
  iteratorObj->setInternalSlot("iteratedObject", thisValue);
  iteratorObj->setInternalSlot("iterationKind", Value(static_cast<int>(SetIterator::IterationType::Values)));
  
  // イテレータの状態を追跡するためのセットへの参照を保持
  set->registerIterator(iterator);
  
  // next メソッドを追加
  iteratorObj->defineOwnProperty(context->staticStrings()->next,
                                Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, 
                                  [](Context* ctx, Value thisVal, const std::vector<Value>&) -> Value {
                                    if (!thisVal.isObject()) {
                                      ctx->throwTypeError("SetIterator.prototype.next called on non-object");
                                      return Value();
                                    }
                                    
                                    Object* iterObj = thisVal.asObject();
                                    Value iteratorValue = iterObj->getInternalSlot("iterator");
                                    
                                    if (iteratorValue.isUndefined() || !iteratorValue.isPointer()) {
                                      ctx->throwTypeError("Invalid SetIterator");
                                      return Value();
                                    }
                                    
                                    SetIterator* iter = static_cast<SetIterator*>(iteratorValue.asPointer());
                                    return iter->next(ctx);
                                  }, 0, context->staticStrings()->next),
                                  Object::PropertyDescriptor::Writable |
                                  Object::PropertyDescriptor::Configurable));
  
  // @@toStringTag プロパティを設定
  iteratorObj->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
                                Object::PropertyDescriptor(context->staticStrings()->SetIterator,
                                                          Object::PropertyDescriptor::Configurable));

  return Value(iteratorObj);
}

// Set.prototype.keys実装（valuesと同じ）
Value setKeys(Context* context, Value thisValue, const std::vector<Value>& args) {
  // keys()はvalues()と同じ動作
  return setValues(context, thisValue, args);
}

// Set.prototype.entries実装
Value setEntries(Context* context, Value thisValue, const std::vector<Value>&) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.entries called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  // SetIteratorオブジェクトを作成して返す
  Object* iteratorObj = context->createObject(context->getSetIteratorPrototype());
  SetIterator* iterator = new SetIterator(set, SetIterator::IterationType::KeysAndValues);
  
  // イテレータの状態を設定
  iteratorObj->setInternalSlot("iterator", Value(iterator));
  iteratorObj->setInternalSlot("iteratedObject", thisValue);
  iteratorObj->setInternalSlot("iterationKind", Value(static_cast<int>(SetIterator::IterationType::KeysAndValues)));
  
  // イテレータの状態を追跡するためのセットへの参照を保持
  set->registerIterator(iterator);
  
  // next メソッドを追加
  iteratorObj->defineOwnProperty(context->staticStrings()->next,
                                Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, 
                                  [](Context* ctx, Value thisVal, const std::vector<Value>&) -> Value {
                                    if (!thisVal.isObject()) {
                                      ctx->throwTypeError("SetIterator.prototype.next called on non-object");
                                      return Value();
                                    }
                                    
                                    Object* iterObj = thisVal.asObject();
                                    Value iteratorValue = iterObj->getInternalSlot("iterator");
                                    
                                    if (iteratorValue.isUndefined() || !iteratorValue.isPointer()) {
                                      ctx->throwTypeError("Invalid SetIterator");
                                      return Value();
                                    }
                                    
                                    SetIterator* iter = static_cast<SetIterator*>(iteratorValue.asPointer());
                                    return iter->next(ctx);
                                  }, 0, context->staticStrings()->next),
                                  Object::PropertyDescriptor::Writable |
                                  Object::PropertyDescriptor::Configurable));
  
  // @@toStringTag プロパティを設定
  iteratorObj->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
                                Object::PropertyDescriptor(context->staticStrings()->SetIterator,
                                                          Object::PropertyDescriptor::Configurable));

  return Value(iteratorObj);
}

// Set.prototype[Symbol.iterator]実装（valuesと同じ）
Value setIterator(Context* context, Value thisValue, const std::vector<Value>& args) {
  // [Symbol.iterator]()はvalues()と同じ動作
  return setValues(context, thisValue, args);
}

// Set.prototype.size取得関数
Value setSize(Context* context, Value thisValue, const std::vector<Value>&) {
  // thisがSetオブジェクトか確認
  if (!thisValue.isObject() || !thisValue.asObject()->isSetObject()) {
    context->throwTypeError("Set.prototype.size called on non-Set object");
    return Value();
  }

  SetObject* set = static_cast<SetObject*>(thisValue.asObject());

  return Value(static_cast<double>(set->size()));
}

// SetIteratorプロトタイプの初期化
Value initializeSetIteratorPrototype(Context* context) {
  // プロトタイプオブジェクトを作成
  Object* prototype = new Object(context->iteratorPrototype());
  
  // @@toStringTag プロパティを設定
  prototype->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
                              Object::PropertyDescriptor(context->staticStrings()->SetIterator,
                                                        Object::PropertyDescriptor::Configurable));
  
  // プロトタイプをコンテキストに保存
  context->setSetIteratorPrototype(prototype);
  
  return Value(prototype);
}

// Setプロトタイプの初期化
Value SetObject::initializePrototype(Context* context) {
  // SetIteratorプロトタイプを初期化
  initializeSetIteratorPrototype(context);
  
  // プロトタイプオブジェクトを作成
  Object* prototype = new Object(context->objectPrototype());

  // Setコンストラクタを作成
  FunctionObject* constructor = new NativeFunctionObject(context, prototype, setConstructor, 0, context->staticStrings()->Set);

  // プロトタイプにメソッドを追加
  prototype->defineOwnProperty(context->staticStrings()->constructor,
                               Object::PropertyDescriptor(Value(constructor),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // add メソッド
  prototype->defineOwnProperty(context->staticStrings()->add,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setAdd, 1,
                                                                                   context->staticStrings()->add),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // clear メソッド
  prototype->defineOwnProperty(context->staticStrings()->clear,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setClear, 0,
                                                                                   context->staticStrings()->clear),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // delete メソッド
  prototype->defineOwnProperty(context->staticStrings()->delete_,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setDelete, 1,
                                                                                   context->staticStrings()->delete_),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // has メソッド
  prototype->defineOwnProperty(context->staticStrings()->has,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setHas, 1,
                                                                                   context->staticStrings()->has),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // forEach メソッド
  prototype->defineOwnProperty(context->staticStrings()->forEach,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setForEach, 1,
                                                                                   context->staticStrings()->forEach),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // values メソッド
  prototype->defineOwnProperty(context->staticStrings()->values,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setValues, 0,
                                                                                   context->staticStrings()->values),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // keys メソッド
  prototype->defineOwnProperty(context->staticStrings()->keys,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setKeys, 0,
                                                                                   context->staticStrings()->keys),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // entries メソッド
  prototype->defineOwnProperty(context->staticStrings()->entries,
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setEntries, 0,
                                                                                   context->staticStrings()->entries),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // [Symbol.iterator] メソッド
  prototype->defineOwnProperty(context->getSymbolFor("iterator"),
                               Object::PropertyDescriptor(new NativeFunctionObject(context, nullptr, setIterator, 0,
                                                                                   context->staticStrings()->iterator),
                                                          Object::PropertyDescriptor::Writable |
                                                              Object::PropertyDescriptor::Configurable));

  // size プロパティ (getter)
  Object* sizeGetter = new NativeFunctionObject(context, nullptr, setSize, 0, context->staticStrings()->getSize);
  prototype->defineOwnProperty(context->staticStrings()->size,
                               Object::PropertyDescriptor(nullptr, sizeGetter, nullptr,
                                                          Object::PropertyDescriptor::Configurable));

  // constructor.prototype
  constructor->defineOwnProperty(context->staticStrings()->prototype,
                                 Object::PropertyDescriptor(Value(prototype), Object::PropertyDescriptor::None));

  // Set.prototype[@@toStringTag]
  prototype->defineOwnProperty(Symbol::wellKnown(context)->toStringTag,
                               Object::PropertyDescriptor(context->staticStrings()->Set,
                                                          Object::PropertyDescriptor::Configurable));

  return Value(constructor);
}

// Setオブジェクトの初期化
Value initializeSet(Context* context) {
  return SetObject::initializePrototype(context);
}

// Set組み込みオブジェクトの登録
void registerSetBuiltin(GlobalObject* global) {
  if (!global) {
    return;
  }

  Context* context = global->context();
  if (!context) {
    return;
  }

  // Setコンストラクタの初期化と登録
  Value setConstructor = initializeSet(context);

  global->defineOwnProperty(context->staticStrings()->Set,
                            Object::PropertyDescriptor(setConstructor,
                                                       Object::PropertyDescriptor::Writable |
                                                           Object::PropertyDescriptor::Configurable));
}

}  // namespace aero