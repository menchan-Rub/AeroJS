/**
 * @file globals_object.cpp
 * @brief JavaScriptのグローバルオブジェクトの実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "globals_object.h"
#include "globals.h"
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>
#include "../builtins/array/array.h"
#include "../builtins/boolean/boolean.h"
#include "../builtins/date/date.h"
#include "../builtins/error/error.h"
#include "../builtins/function/function.h"
#include "../builtins/map/map.h"
#include "../builtins/number/number.h"
#include "../builtins/object/object.h"
#include "../builtins/promise/promise.h"
#include "../builtins/regexp/regexp.h"
#include "../builtins/set/set.h"
#include "../builtins/string/string.h"
#include "../builtins/symbol/symbol.h"
#include "../builtins/weakmap/weakmap.h"
#include "../builtins/weakset/weakset.h"

namespace aero {

GlobalsObject* GlobalsObject::create(ExecutionContext* ctx) {
  return new GlobalsObject(ctx);
}

GlobalsObject::GlobalsObject(ExecutionContext* ctx)
  : m_context(ctx)
  , m_globalObject(Object::create(ctx))
  , m_isInitialized(false) {
}

GlobalsObject::~GlobalsObject() {
  // グローバルオブジェクトはコンテキストによって管理されるため、
  // ここでは解放しません
}

void GlobalsObject::initialize() {
  std::lock_guard<std::mutex> lock(m_mutex);
  
  if (m_isInitialized) {
    return;
  }
  
  // グローバル関数の初期化
  initializeGlobalFunctions(m_context, m_globalObject);
  
  // プロトタイプオブジェクトの初期化
  initializePrototypes();
  
  // 組み込みコンストラクタの初期化
  initializeConstructors();
  
  // 組み込みオブジェクトの初期化
  initializeBuiltins();
  
  // グローバルプロパティの設定
  initializeProperties();
  
  m_isInitialized = true;
}

void GlobalsObject::initializePrototypes() {
  // オブジェクトプロトタイプの初期化
  m_objectPrototype = builtins::object::initializePrototype(m_context, m_globalObject);
  
  // 関数プロトタイプの初期化
  m_functionPrototype = builtins::function::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 配列プロトタイプの初期化
  m_arrayPrototype = builtins::array::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 文字列プロトタイプの初期化
  m_stringPrototype = builtins::string::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 数値プロトタイプの初期化
  m_numberPrototype = builtins::number::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 真偽値プロトタイプの初期化
  m_booleanPrototype = builtins::boolean::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 日付プロトタイプの初期化
  m_datePrototype = builtins::date::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // 正規表現プロトタイプの初期化
  m_regexpPrototype = builtins::regexp::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // エラープロトタイプの初期化
  m_errorPrototypes = builtins::error::initializePrototypes(m_context, m_globalObject, m_objectPrototype);
  
  // シンボルプロトタイプの初期化
  m_symbolPrototype = builtins::symbol::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // プロミスプロトタイプの初期化
  m_promisePrototype = builtins::promise::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // Mapプロトタイプの初期化
  m_mapPrototype = builtins::map::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // Setプロトタイプの初期化
  m_setPrototype = builtins::set::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // WeakMapプロトタイプの初期化
  m_weakMapPrototype = builtins::weakmap::initializePrototype(m_context, m_globalObject, m_objectPrototype);
  
  // WeakSetプロトタイプの初期化
  m_weakSetPrototype = builtins::weakset::initializePrototype(m_context, m_globalObject, m_objectPrototype);
}

void GlobalsObject::initializeConstructors() {
  // Objectコンストラクタの初期化
  m_objectConstructor = builtins::object::initializeConstructor(m_context, m_globalObject, m_objectPrototype, m_functionPrototype);
  defineObject("Object", m_objectConstructor);
  
  // Functionコンストラクタの初期化
  m_functionConstructor = builtins::function::initializeConstructor(m_context, m_globalObject, m_functionPrototype, m_functionPrototype);
  defineObject("Function", m_functionConstructor);
  
  // Arrayコンストラクタの初期化
  m_arrayConstructor = builtins::array::initializeConstructor(m_context, m_globalObject, m_arrayPrototype, m_functionPrototype);
  defineObject("Array", m_arrayConstructor);
  
  // Stringコンストラクタの初期化
  m_stringConstructor = builtins::string::initializeConstructor(m_context, m_globalObject, m_stringPrototype, m_functionPrototype);
  defineObject("String", m_stringConstructor);
  
  // Numberコンストラクタの初期化
  m_numberConstructor = builtins::number::initializeConstructor(m_context, m_globalObject, m_numberPrototype, m_functionPrototype);
  defineObject("Number", m_numberConstructor);
  
  // Booleanコンストラクタの初期化
  m_booleanConstructor = builtins::boolean::initializeConstructor(m_context, m_globalObject, m_booleanPrototype, m_functionPrototype);
  defineObject("Boolean", m_booleanConstructor);
  
  // Dateコンストラクタの初期化
  m_dateConstructor = builtins::date::initializeConstructor(m_context, m_globalObject, m_datePrototype, m_functionPrototype);
  defineObject("Date", m_dateConstructor);
  
  // RegExpコンストラクタの初期化
  m_regexpConstructor = builtins::regexp::initializeConstructor(m_context, m_globalObject, m_regexpPrototype, m_functionPrototype);
  defineObject("RegExp", m_regexpConstructor);
  
  // Errorコンストラクタの初期化
  m_errorConstructors = builtins::error::initializeConstructors(m_context, m_globalObject, m_errorPrototypes, m_functionPrototype);
  for (const auto& [name, constructor] : m_errorConstructors) {
    defineObject(name, constructor);
  }
  
  // Symbolコンストラクタの初期化
  m_symbolConstructor = builtins::symbol::initializeConstructor(m_context, m_globalObject, m_symbolPrototype, m_functionPrototype);
  defineObject("Symbol", m_symbolConstructor);
  
  // Promiseコンストラクタの初期化
  m_promiseConstructor = builtins::promise::initializeConstructor(m_context, m_globalObject, m_promisePrototype, m_functionPrototype);
  defineObject("Promise", m_promiseConstructor);
  
  // Mapコンストラクタの初期化
  m_mapConstructor = builtins::map::initializeConstructor(m_context, m_globalObject, m_mapPrototype, m_functionPrototype);
  defineObject("Map", m_mapConstructor);
  
  // Setコンストラクタの初期化
  m_setConstructor = builtins::set::initializeConstructor(m_context, m_globalObject, m_setPrototype, m_functionPrototype);
  defineObject("Set", m_setConstructor);
  
  // WeakMapコンストラクタの初期化
  m_weakMapConstructor = builtins::weakmap::initializeConstructor(m_context, m_globalObject, m_weakMapPrototype, m_functionPrototype);
  defineObject("WeakMap", m_weakMapConstructor);
  
  // WeakSetコンストラクタの初期化
  m_weakSetConstructor = builtins::weakset::initializeConstructor(m_context, m_globalObject, m_weakSetPrototype, m_functionPrototype);
  defineObject("WeakSet", m_weakSetConstructor);
}

void GlobalsObject::initializeBuiltins() {
  // すでに初期化されたMath、JSON、Reflectオブジェクトは
  // initializeGlobalFunctionsで設定されています
}

void GlobalsObject::initializeProperties() {
  // グローバルプロパティの設定
  setGlobal("undefined", Value::createUndefined(), false, false, false);
  setGlobal("NaN", Value::createNaN(), false, false, false);
  setGlobal("Infinity", Value::createNumber(std::numeric_limits<double>::infinity()), false, false, false);
  
  // グローバルオブジェクト自身への参照
  setGlobal("globalThis", Value(m_globalObject), true, false, true);
}

void GlobalsObject::setGlobal(const std::string& name, Value value, 
                             bool writable, bool enumerable, bool configurable) {
  uint8_t attributes = 0;
  if (writable) attributes |= PropertyDescriptor::Writable;
  if (enumerable) attributes |= PropertyDescriptor::Enumerable;
  if (configurable) attributes |= PropertyDescriptor::Configurable;
  
  m_globalObject->defineProperty(m_context, name, value, 
    PropertyDescriptor::createDataDescriptorFlags(attributes));
}

Value GlobalsObject::getGlobal(const std::string& name) {
  return m_globalObject->get(m_context, name);
}

void GlobalsObject::defineFunction(const std::string& name, FunctionCallback callback, uint32_t length) {
  m_globalObject->defineProperty(m_context, name, 
    Value::createFunction(m_context, callback, length, name),
    PropertyDescriptor::createDataDescriptorFlags(
      PropertyDescriptor::Writable | 
      PropertyDescriptor::Configurable
    ));
}

void GlobalsObject::defineObject(const std::string& name, Object* obj) {
  m_globalObject->defineProperty(m_context, name, Value(obj),
    PropertyDescriptor::createDataDescriptorFlags(
      PropertyDescriptor::Writable | 
      PropertyDescriptor::Configurable
    ));
}

} // namespace aero 