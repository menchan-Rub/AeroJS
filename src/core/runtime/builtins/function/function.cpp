/**
 * @file function.cpp
 * @brief JavaScript の Function オブジェクトの実装
 * @copyright 2023 AeroJS Project
 */

#include "core/runtime/builtins/function/function.h"

#include <sstream>

#include "core/runtime/context.h"
#include "core/runtime/error.h"
#include "core/runtime/global_object.h"
#include "core/runtime/property_descriptor.h"

namespace aero {

// FunctionObjectの実装

FunctionObject::FunctionObject(Object* proto, NativeFunction func, int length, const std::string& name)
    : Object(proto), m_nativeFunction(func), m_length(length), m_name(name), m_prototype(nullptr), m_body(""), m_scope(nullptr), m_isNative(true) {
  // プロトタイプオブジェクトを作成
  auto* context = Context::current();
  m_prototype = context->objectConstructor()->newObject();
  m_prototype->defineProperty("constructor", PropertyDescriptor(Value(this), true, true, true));

  // 関数オブジェクトのプロパティを設定
  defineProperty("length", PropertyDescriptor(Value(m_length), true, false, false));
  defineProperty("name", PropertyDescriptor(Value(m_name), true, false, true));
  defineProperty("prototype", PropertyDescriptor(Value(m_prototype), true, false, true));
}

FunctionObject::FunctionObject(Object* proto, const std::string& name, const std::vector<std::string>& parameterList, const std::string& body, Object* scope)
    : Object(proto), m_nativeFunction(nullptr), m_length(static_cast<int>(parameterList.size())), m_name(name), m_prototype(nullptr), m_parameterList(parameterList), m_body(body), m_scope(scope), m_isNative(false) {
  // プロトタイプオブジェクトを作成
  auto* context = Context::current();
  m_prototype = context->objectConstructor()->newObject();
  m_prototype->defineProperty("constructor", PropertyDescriptor(Value(this), true, true, true));

  // 関数オブジェクトのプロパティを設定
  defineProperty("length", PropertyDescriptor(Value(m_length), true, false, false));
  defineProperty("name", PropertyDescriptor(Value(m_name), true, false, true));
  defineProperty("prototype", PropertyDescriptor(Value(m_prototype), true, false, true));
}

FunctionObject::~FunctionObject() {
  // プロトタイプオブジェクトの参照を解放
  if (m_prototype) {
    m_prototype = nullptr;
  }
}

Value FunctionObject::call(Context* context, Value thisValue, const std::vector<Value>& args) {
  if (m_isNative) {
    // ネイティブ関数の場合は、関数ポインタを呼び出す
    if (m_nativeFunction) {
      return m_nativeFunction(context, thisValue, args);
    }
    return Value::undefined();
  } else {
    // ユーザー定義関数の場合は、新しい実行コンテキストを作成して関数を実行する
    // 注: 実際の実装では、関数のボディをパースして実行するコードが必要
    // ここではスタブ実装として未定義を返す
    return Value::undefined();
  }
}

Value FunctionObject::construct(Context* context, const std::vector<Value>& args) {
  // 新しいオブジェクトを作成
  Object* obj = context->objectConstructor()->newObject();

  // プロトタイプを設定
  if (m_prototype) {
    obj->setPrototype(m_prototype);
  }

  // コンストラクタとして関数を呼び出す
  Value result = call(context, Value(obj), args);

  // 戻り値がオブジェクトの場合はそれを返し、そうでない場合は新しいオブジェクトを返す
  if (result.isObject()) {
    return result;
  }
  return Value(obj);
}

// Function コンストラクタ
Value functionConstructor(Context* context, Value thisValue, const std::vector<Value>& args) {
  // パラメータと関数ボディを準備
  std::vector<std::string> parameterList;
  std::string body;

  if (args.empty()) {
    // 引数がない場合は、空の関数を作成
    body = "";
  } else if (args.size() == 1) {
    // 引数が1つの場合は、それを関数ボディとして扱う
    body = args[0].toString()->value();
  } else {
    // 引数が複数ある場合は、最後の引数を関数ボディとして扱い、他の引数をパラメータとして扱う
    for (size_t i = 0; i < args.size() - 1; ++i) {
      parameterList.push_back(args[i].toString()->value());
    }
    body = args.back().toString()->value();
  }

  // 新しい関数オブジェクトを作成
  Object* functionProto = context->functionPrototype();
  auto* func = new FunctionObject(functionProto, "", parameterList, body, context->globalObject());

  return Value(func);
}

// Function.prototype.toString
Value functionToString(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueが関数オブジェクトでない場合はエラー
  if (!thisValue.isFunction()) {
    return context->throwTypeError("Function.prototype.toString called on incompatible receiver");
  }

  auto* func = static_cast<FunctionObject*>(thisValue.asObject());

  std::ostringstream oss;
  if (func->isNative) {
    // ネイティブ関数の場合
    oss << "function " << func->name() << "() { [native code] }";
  } else {
    // ユーザー定義関数の場合
    oss << "function " << func->name() << "(";

    // パラメータリストを文字列化
    for (size_t i = 0; i < func->m_parameterList.size(); ++i) {
      if (i > 0) {
        oss << ", ";
      }
      oss << func->m_parameterList[i];
    }

    oss << ") {" << std::endl
        << func->m_body << std::endl
        << "}";
  }

  return Value(context->newString(oss.str()));
}

// Function.prototype.apply
Value functionApply(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueが関数オブジェクトでない場合はエラー
  if (!thisValue.isFunction()) {
    return context->throwTypeError("Function.prototype.apply called on incompatible receiver");
  }

  auto* func = static_cast<FunctionObject*>(thisValue.asObject());

  // thisArgを取得
  Value thisArg = args.size() > 0 ? args[0] : Value::undefined();

  // argsArrayを取得
  std::vector<Value> callArgs;
  if (args.size() > 1 && !args[1].isNullOrUndefined()) {
    // argsArrayからパラメータを取得
    Value argsArray = args[1];
    if (!argsArray.isObject()) {
      return context->throwTypeError("CreateListFromArrayLike called on non-object");
    }

    // lengthプロパティを取得
    Value lengthValue = argsArray.asObject()->get("length");
    uint32_t length = lengthValue.toUint32();

    // パラメータを配列から取得
    for (uint32_t i = 0; i < length; ++i) {
      std::string indexStr = std::to_string(i);
      Value arg = argsArray.asObject()->get(indexStr);
      callArgs.push_back(arg);
    }
  }

  // 関数を呼び出す
  return func->call(context, thisArg, callArgs);
}

// Function.prototype.call
Value functionCall(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueが関数オブジェクトでない場合はエラー
  if (!thisValue.isFunction()) {
    return context->throwTypeError("Function.prototype.call called on incompatible receiver");
  }

  auto* func = static_cast<FunctionObject*>(thisValue.asObject());

  // thisArgを取得
  Value thisArg = args.size() > 0 ? args[0] : Value::undefined();

  // パラメータを取得（最初の引数はthisArgなので除外）
  std::vector<Value> callArgs;
  if (args.size() > 1) {
    callArgs.assign(args.begin() + 1, args.end());
  }

  // 関数を呼び出す
  return func->call(context, thisArg, callArgs);
}

// Function.prototype.bind
Value functionBind(Context* context, Value thisValue, const std::vector<Value>& args) {
  // thisValueが関数オブジェクトでない場合はエラー
  if (!thisValue.isFunction()) {
    return context->throwTypeError("Function.prototype.bind called on incompatible receiver");
  }

  auto* targetFunc = static_cast<FunctionObject*>(thisValue.asObject());

  // thisArgを取得
  Value thisArg = args.size() > 0 ? args[0] : Value::undefined();

  // バインドする引数を取得（最初の引数はthisArgなので除外）
  std::vector<Value> boundArgs;
  if (args.size() > 1) {
    boundArgs.assign(args.begin() + 1, args.end());
  }

  // バインド関数を作成する
  class BoundFunctionObject : public FunctionObject {
   public:
    BoundFunctionObject(Object* proto, FunctionObject* targetFunc, Value thisArg, const std::vector<Value>& boundArgs)
        : FunctionObject(proto, nullptr, std::max(0, targetFunc->length() - static_cast<int>(boundArgs.size())), "bound " + targetFunc->name()), m_targetFunc(targetFunc), m_thisArg(thisArg), m_boundArgs(boundArgs) {
      // バインド関数は、ターゲット関数のprototypeプロパティを持たない
      defineProperty("prototype", PropertyDescriptor(Value::undefined(), false, false, false));
    }

    Value call(Context* context, Value /* thisValue */, const std::vector<Value>& args) override {
      // バインドされた引数と呼び出し時の引数を結合
      std::vector<Value> combinedArgs = m_boundArgs;
      combinedArgs.insert(combinedArgs.end(), args.begin(), args.end());

      // ターゲット関数を呼び出す
      return m_targetFunc->call(context, m_thisArg, combinedArgs);
    }

    Value construct(Context* context, const std::vector<Value>& args) override {
      // バインドされた引数と呼び出し時の引数を結合
      std::vector<Value> combinedArgs = m_boundArgs;
      combinedArgs.insert(combinedArgs.end(), args.begin(), args.end());

      // ターゲット関数をコンストラクタとして呼び出す
      return m_targetFunc->construct(context, combinedArgs);
    }

   private:
    FunctionObject* m_targetFunc;
    Value m_thisArg;
    std::vector<Value> m_boundArgs;
  };

  // 新しいバインド関数オブジェクトを作成
  auto* boundFunc = new BoundFunctionObject(context->functionPrototype(), targetFunc, thisArg, boundArgs);

  return Value(boundFunc);
}

// Function プロトタイプの初期化
void initFunctionPrototype(Context* context, Object* proto) {
  // prototype プロパティを設定
  proto->defineProperty("constructor", PropertyDescriptor(Value::undefined(), true, false, true));
  proto->defineProperty("length", PropertyDescriptor(Value(0), true, false, false));
  proto->defineProperty("name", PropertyDescriptor(Value(""), true, false, false));

  // メソッドを追加
  proto->defineProperty("toString", PropertyDescriptor(
                                        Value(new FunctionObject(context->functionPrototype(), functionToString, 0, "toString")),
                                        true, false, true));

  proto->defineProperty("apply", PropertyDescriptor(
                                     Value(new FunctionObject(context->functionPrototype(), functionApply, 2, "apply")),
                                     true, false, true));

  proto->defineProperty("call", PropertyDescriptor(
                                    Value(new FunctionObject(context->functionPrototype(), functionCall, 1, "call")),
                                    true, false, true));

  proto->defineProperty("bind", PropertyDescriptor(
                                    Value(new FunctionObject(context->functionPrototype(), functionBind, 1, "bind")),
                                    true, false, true));
}

// Function コンストラクタの初期化
void initFunctionConstructor(Context* context, Object* constructor, Object* proto) {
  // constructorのプロパティを設定
  constructor->defineProperty("prototype", PropertyDescriptor(Value(proto), false, false, false));
  constructor->defineProperty("length", PropertyDescriptor(Value(1), true, false, false));

  // prototypeのconstructorプロパティを設定
  proto->defineProperty("constructor", PropertyDescriptor(Value(constructor), true, false, true));
}

// Function 組み込みオブジェクトの登録
void registerFunctionBuiltin(GlobalObject* global) {
  Context* context = Context::current();

  // Function.prototypeを作成
  Object* functionProto = new Object(context->objectPrototype());

  // Function コンストラクタを作成
  auto* functionConstructorObj = new FunctionObject(context->functionPrototype(), functionConstructor, 1, "Function");

  // プロトタイプとコンストラクタを初期化
  initFunctionPrototype(context, functionProto);
  initFunctionConstructor(context, functionConstructorObj, functionProto);

  // グローバルオブジェクトに登録
  global->defineProperty("Function", PropertyDescriptor(Value(functionConstructorObj), true, false, true));

  // コンテキストに保存
  context->setFunctionPrototype(functionProto);
  context->setFunctionConstructor(functionConstructorObj);
}

}  // namespace aero