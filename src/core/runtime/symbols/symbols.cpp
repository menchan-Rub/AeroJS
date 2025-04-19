/**
 * @file symbols.cpp
 * @brief JavaScript のシンボル型の実装
 * @copyright Copyright (c) 2022 AeroJS Project
 * @license MIT License
 */

#include "symbols.h"
#include <sstream>
#include "../error/error.h"
#include "../context/context.h"

namespace aero {

// 静的メンバ変数の初期化
std::unordered_map<std::string, Symbol*> Symbol::s_registry;
std::mutex Symbol::s_registryMutex;
std::atomic<uint64_t> Symbol::s_nextId(1);

// Well-Known Symbolsの初期化
Symbol* Symbol::s_hasInstance = nullptr;
Symbol* Symbol::s_isConcatSpreadable = nullptr;
Symbol* Symbol::s_iterator = nullptr;
Symbol* Symbol::s_match = nullptr;
Symbol* Symbol::s_matchAll = nullptr;
Symbol* Symbol::s_replace = nullptr;
Symbol* Symbol::s_search = nullptr;
Symbol* Symbol::s_species = nullptr;
Symbol* Symbol::s_split = nullptr;
Symbol* Symbol::s_toPrimitive = nullptr;
Symbol* Symbol::s_toStringTag = nullptr;
Symbol* Symbol::s_unscopables = nullptr;
Symbol* Symbol::s_asyncIterator = nullptr;

Symbol::Symbol(const std::string& description, uint64_t id, bool isGlobal, const std::string& key)
    : m_description(description), 
      m_isGlobal(isGlobal), 
      m_key(key) {
  // IDが指定されていない場合は自動生成
  m_id = (id == 0) ? s_nextId.fetch_add(1) : id;
}

Symbol* Symbol::create(const std::string& description) {
  return new Symbol(description);
}

Symbol* Symbol::for_(const std::string& key) {
  // すでに存在するキーを確認
  Symbol* symbol = getFromRegistry(key);
  if (symbol) {
    return symbol;
  }
  
  // 新しいグローバルシンボルを作成して登録
  symbol = new Symbol(key, 0, true, key);
  registerToRegistry(key, symbol);
  
  return symbol;
}

Value Symbol::keyFor(Symbol* symbol) {
  if (!symbol) {
    return Value::createUndefined();
  }
  
  // グローバルシンボルでない場合はundefinedを返す
  if (!symbol->m_isGlobal) {
    return Value::createUndefined();
  }
  
  return Value::createString(symbol->m_key);
}

bool Symbol::equals(const Symbol* other) const {
  if (!other) {
    return false;
  }
  
  // シンボルの等価性はIDによって決定される
  return m_id == other->m_id;
}

std::string Symbol::toString() const {
  std::stringstream ss;
  ss << "Symbol(";
  if (!m_description.empty()) {
    ss << m_description;
  }
  ss << ")";
  return ss.str();
}

Symbol* Symbol::getFromRegistry(const std::string& key) {
  std::lock_guard<std::mutex> lock(s_registryMutex);
  auto it = s_registry.find(key);
  if (it != s_registry.end()) {
    return it->second;
  }
  return nullptr;
}

void Symbol::registerToRegistry(const std::string& key, Symbol* symbol) {
  std::lock_guard<std::mutex> lock(s_registryMutex);
  s_registry[key] = symbol;
}

// Well-Known Symbols の実装

Symbol* Symbol::hasInstance() {
  if (!s_hasInstance) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_hasInstance) {
      s_hasInstance = new Symbol("Symbol.hasInstance");
    }
  }
  return s_hasInstance;
}

Symbol* Symbol::isConcatSpreadable() {
  if (!s_isConcatSpreadable) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_isConcatSpreadable) {
      s_isConcatSpreadable = new Symbol("Symbol.isConcatSpreadable");
    }
  }
  return s_isConcatSpreadable;
}

Symbol* Symbol::iterator() {
  if (!s_iterator) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_iterator) {
      s_iterator = new Symbol("Symbol.iterator");
    }
  }
  return s_iterator;
}

Symbol* Symbol::match() {
  if (!s_match) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_match) {
      s_match = new Symbol("Symbol.match");
    }
  }
  return s_match;
}

Symbol* Symbol::matchAll() {
  if (!s_matchAll) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_matchAll) {
      s_matchAll = new Symbol("Symbol.matchAll");
    }
  }
  return s_matchAll;
}

Symbol* Symbol::replace() {
  if (!s_replace) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_replace) {
      s_replace = new Symbol("Symbol.replace");
    }
  }
  return s_replace;
}

Symbol* Symbol::search() {
  if (!s_search) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_search) {
      s_search = new Symbol("Symbol.search");
    }
  }
  return s_search;
}

Symbol* Symbol::species() {
  if (!s_species) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_species) {
      s_species = new Symbol("Symbol.species");
    }
  }
  return s_species;
}

Symbol* Symbol::split() {
  if (!s_split) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_split) {
      s_split = new Symbol("Symbol.split");
    }
  }
  return s_split;
}

Symbol* Symbol::toPrimitive() {
  if (!s_toPrimitive) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_toPrimitive) {
      s_toPrimitive = new Symbol("Symbol.toPrimitive");
    }
  }
  return s_toPrimitive;
}

Symbol* Symbol::toStringTag() {
  if (!s_toStringTag) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_toStringTag) {
      s_toStringTag = new Symbol("Symbol.toStringTag");
    }
  }
  return s_toStringTag;
}

Symbol* Symbol::unscopables() {
  if (!s_unscopables) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_unscopables) {
      s_unscopables = new Symbol("Symbol.unscopables");
    }
  }
  return s_unscopables;
}

Symbol* Symbol::asyncIterator() {
  if (!s_asyncIterator) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    if (!s_asyncIterator) {
      s_asyncIterator = new Symbol("Symbol.asyncIterator");
    }
  }
  return s_asyncIterator;
}

//-----------------------------------------------------------------------------
// Symbol プロトタイプとコンストラクタの初期化
//-----------------------------------------------------------------------------

// Symbolプロトタイプメソッド

/**
 * @brief Symbol.prototype.toString
 */
static Value symbolToString(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisがSymbolオブジェクトの場合は内部のSymbol値を取得
  Symbol* symbol = nullptr;
  
  if (thisValue.isSymbol()) {
    symbol = thisValue.asSymbol();
  } else if (thisValue.isObject() && thisValue.asObject()->isSymbolObject(ctx)) {
    // Symbol objectからSymbol値を取り出す
    symbol = thisValue.asObject()->getInternalSymbol();
  } else {
    ctx->throwError(Error::createTypeError(ctx, "Symbol.prototype.toString called on non-symbol"));
    return Value::createUndefined();
  }
  
  return Value::createString(symbol->toString());
}

/**
 * @brief Symbol.prototype.valueOf
 */
static Value symbolValueOf(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisがSymbolオブジェクトの場合は内部のSymbol値を取得
  Symbol* symbol = nullptr;
  
  if (thisValue.isSymbol()) {
    return thisValue;
  } else if (thisValue.isObject() && thisValue.asObject()->isSymbolObject(ctx)) {
    // Symbol objectからSymbol値を取り出す
    symbol = thisValue.asObject()->getInternalSymbol();
    return Value(symbol);
  } else {
    ctx->throwError(Error::createTypeError(ctx, "Symbol.prototype.valueOf called on non-symbol"));
    return Value::createUndefined();
  }
}

/**
 * @brief Symbol.prototype[Symbol.toPrimitive]
 */
static Value symbolToPrimitive(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // 変換のヒントは無視される（Symbol型として変換する）
  return symbolValueOf(ctx, thisValue, args);
}

/**
 * @brief Symbol.prototype[Symbol.toStringTag]
 */
static Value symbolToStringTag(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  return Value::createString("Symbol");
}

/**
 * @brief Symbol.prototype.description getter
 */
static Value symbolDescriptionGetter(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // thisがSymbolオブジェクトの場合は内部のSymbol値を取得
  Symbol* symbol = nullptr;
  
  if (thisValue.isSymbol()) {
    symbol = thisValue.asSymbol();
  } else if (thisValue.isObject() && thisValue.asObject()->isSymbolObject(ctx)) {
    // Symbol objectからSymbol値を取り出す
    symbol = thisValue.asObject()->getInternalSymbol();
  } else {
    ctx->throwError(Error::createTypeError(ctx, "Symbol.prototype.description getter called on non-symbol"));
    return Value::createUndefined();
  }
  
  const std::string& desc = symbol->description();
  if (desc.empty()) {
    return Value::createUndefined();
  }
  
  return Value::createString(desc);
}

void initializeSymbolPrototype(ExecutionContext* ctx, Object* globalObj) {
  // Symbolプロトタイプオブジェクトを作成
  Object* symbolPrototype = Object::create(ctx);
  symbolPrototype->setPrototype(ctx->getObjectPrototype());
  
  // プロトタイプメソッドを定義
  symbolPrototype->defineProperty(ctx, "toString", 
      Value::createFunction(ctx, symbolToString, 0, "toString"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  symbolPrototype->defineProperty(ctx, "valueOf", 
      Value::createFunction(ctx, symbolValueOf, 0, "valueOf"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // descriptionプロパティのゲッターを定義
  Object* descriptionDescriptor = Object::create(ctx);
  descriptionDescriptor->defineProperty(ctx, "get", 
      Value::createFunction(ctx, symbolDescriptionGetter, 0, "get description"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Enumerable | 
          PropertyDescriptor::Configurable
      )
  );
  
  symbolPrototype->defineProperty(ctx, "description", Value::createUndefined(), 
      PropertyDescriptor::createAccessorDescriptorFlags(
          PropertyDescriptor::Configurable
      ),
      Value::createFunction(ctx, symbolDescriptionGetter, 0, "get description"),
      Value::createUndefined()
  );
  
  // Well-known Symbolを使用したメソッドを定義
  symbolPrototype->defineProperty(ctx, Symbol::toPrimitive(), 
      Value::createFunction(ctx, symbolToPrimitive, 1, "[Symbol.toPrimitive]"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  symbolPrototype->defineProperty(ctx, Symbol::toStringTag(), 
      Value::createString("Symbol"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Configurable
      )
  );
  
  // Symbolプロトタイプをグローバルオブジェクトに保存
  globalObj->defineProperty(ctx, "Symbol.prototype", Value(symbolPrototype), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
}

// Symbolコンストラクタメソッド

/**
 * @brief Symbol コンストラクタ実装
 */
static Value symbolConstructor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  // newで呼び出された場合はTypeErrorをスロー
  if (thisValue.isObject() && ctx->isConstructorCall()) {
    ctx->throwError(Error::createTypeError(ctx, "Symbol is not a constructor"));
    return Value::createUndefined();
  }
  
  // 引数から説明を取得
  std::string description;
  if (!args.empty() && !args[0].isUndefined() && !args[0].isNull()) {
    description = args[0].toString(ctx);
  }
  
  // 新しいシンボルを作成
  Symbol* symbol = Symbol::create(description);
  return Value(symbol);
}

/**
 * @brief Symbol.for 実装
 */
static Value symbolFor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty()) {
    ctx->throwError(Error::createTypeError(ctx, "Symbol.for requires an argument"));
    return Value::createUndefined();
  }
  
  // キーを文字列に変換
  std::string key = args[0].toString(ctx);
  
  // グローバルレジストリからシンボルを取得または作成
  Symbol* symbol = Symbol::for_(key);
  return Value(symbol);
}

/**
 * @brief Symbol.keyFor 実装
 */
static Value symbolKeyFor(ExecutionContext* ctx, Value thisValue, const std::vector<Value>& args) {
  if (args.empty() || !args[0].isSymbol()) {
    ctx->throwError(Error::createTypeError(ctx, "Symbol.keyFor requires a Symbol argument"));
    return Value::createUndefined();
  }
  
  // シンボルのキーを取得
  return Symbol::keyFor(args[0].asSymbol());
}

Function* initializeSymbolConstructor(ExecutionContext* ctx, Object* globalObj, Object* prototype) {
  // Symbolコンストラクタ関数を作成
  Function* symbolConstructorFunc = Function::create(ctx, symbolConstructor, 0, "Symbol");
  
  // コンストラクタのprototypeプロパティを設定
  symbolConstructorFunc->defineProperty(ctx, "prototype", Value(prototype), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  // 静的メソッドを定義
  symbolConstructorFunc->defineProperty(ctx, "for", 
      Value::createFunction(ctx, symbolFor, 1, "for"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "keyFor", 
      Value::createFunction(ctx, symbolKeyFor, 1, "keyFor"),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  // Well-Known Symbolsをプロパティとして定義
  symbolConstructorFunc->defineProperty(ctx, "hasInstance", 
      Value(Symbol::hasInstance()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "isConcatSpreadable", 
      Value(Symbol::isConcatSpreadable()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "iterator", 
      Value(Symbol::iterator()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "match", 
      Value(Symbol::match()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "matchAll", 
      Value(Symbol::matchAll()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "replace", 
      Value(Symbol::replace()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "search", 
      Value(Symbol::search()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "species", 
      Value(Symbol::species()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "split", 
      Value(Symbol::split()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "toPrimitive", 
      Value(Symbol::toPrimitive()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "toStringTag", 
      Value(Symbol::toStringTag()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "unscopables", 
      Value(Symbol::unscopables()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  symbolConstructorFunc->defineProperty(ctx, "asyncIterator", 
      Value(Symbol::asyncIterator()),
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::None
      )
  );
  
  // Symbolコンストラクタをグローバルオブジェクトに設定
  globalObj->defineProperty(ctx, "Symbol", Value(symbolConstructorFunc), 
      PropertyDescriptor::createDataDescriptorFlags(
          PropertyDescriptor::Writable | 
          PropertyDescriptor::Configurable
      )
  );
  
  return symbolConstructorFunc;
}

} // namespace aero 