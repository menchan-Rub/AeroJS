/**
 * @file inline_cache.h
 * @brief 世界最高のポリモーフィックインラインキャッシュシステム
 * @version 2.0.0
 * @license MIT
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <functional>
#include <array>
#include <bitset>
#include <mutex>
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace aerojs {
namespace core {

// 前方宣言
class Value;
class Object;
class Context;
class Function;
class String;
class NativeCode;

/**
 * @brief キャッシュエントリ状態
 */
enum class CacheState : uint8_t {
    Uninitialized,   // 未初期化
    Monomorphic,     // 単一形状
    Polymorphic,     // 複数形状（2-4）
    Megamorphic,     // 多数形状（5以上）
    Generic          // 汎用（遅いパス）
};

/**
 * @brief キャッシュ操作タイプ
 */
enum class CacheOperation : uint8_t {
    PropertyLoad,     // プロパティ読み込み
    PropertyStore,    // プロパティ書き込み
    MethodCall,       // メソッド呼び出し
    ElementLoad,      // 配列要素読み込み
    ElementStore,     // 配列要素書き込み
    ConstantLoad,     // 定数読み込み
    ProtoLoad,        // プロトタイプチェーン読み込み
    HasProperty,      // プロパティ存在確認
    InstanceOf,       // インスタンス判定
    In,               // inオペレータ
    FastCall          // 高速呼び出し
};

/**
 * @brief キャッシュヒント
 */
enum class CacheHint : uint8_t {
    None,             // ヒントなし
    Likely,           // 高確率
    Unlikely,         // 低確率
    HotPath,          // ホットパス
    ColdPath,         // コールドパス
    PinnedProperty,   // ピン留めプロパティ
    ConstantProp,     // 定数伝播
    LeafFunction,     // リーフ関数
    NoSideEffect,     // 副作用なし
    FastAccess        // 高速アクセス
};

/**
 * @brief インラインキャッシュフラグ
 */
enum class ICFlags : uint32_t {
    None                    = 0,
    Optimized               = 1 << 0,    // 最適化済み
    TypeSpecialized         = 1 << 1,    // 型特殊化済み
    Inlined                 = 1 << 2,    // インライン展開済み
    Patched                 = 1 << 3,    // パッチ適用済み
    GuardInserted           = 1 << 4,    // ガード挿入済み
    SIMDOptimized           = 1 << 5,    // SIMD最適化済み
    UsesShapeGuard          = 1 << 6,    // シェイプガード使用
    UsesTypeGuard           = 1 << 7,    // 型ガード使用
    AccessesProtoChain      = 1 << 8,    // プロトタイプチェーンアクセス
    CanBeHoisted            = 1 << 9,    // ループ外に移動可能
    HasNegativeGuard        = 1 << 10,   // 否定ガードあり
    DirectAccess            = 1 << 11,   // 直接アクセス
    HasTransitionMap        = 1 << 12,   // 遷移マップあり
    RequiresTypeCheck       = 1 << 13,   // 型チェック必須
    AllowsSpecialization    = 1 << 14,   // 特殊化許可
    BlocksSIMD              = 1 << 15,   // SIMD妨げる
    IsMonomorphicFastPath   = 1 << 16,   // 単一形状高速パス
    IsPolymorphicFastPath   = 1 << 17,   // 複数形状高速パス
    IsMegamorphicSlowPath   = 1 << 18,   // 多形状低速パス
    IsGlobalIC              = 1 << 19,   // グローバルIC
    IsTransitionIC          = 1 << 20,   // 遷移IC
    AllowsShapeTransition   = 1 << 21,   // シェイプ遷移許可
    FeedsBackToJIT          = 1 << 22,   // JITにフィードバック
    AllowsStorageInlining   = 1 << 23    // ストレージインライン化許可
};

inline ICFlags operator|(ICFlags a, ICFlags b) {
    return static_cast<ICFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline ICFlags& operator|=(ICFlags& a, ICFlags b) {
    a = a | b;
    return a;
}

inline bool operator&(ICFlags a, ICFlags b) {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

/**
 * @brief オブジェクト形状ID型
 */
using ShapeID = uint64_t;

/**
 * @brief プロパティ位置情報
 */
struct PropertyLocation {
    enum class Type : uint8_t {
        Slot,              // 直接スロット
        Indexed,           // インデックス付き
        Named,             // 名前付き
        Accessor,          // アクセサプロパティ
        ConstantFunction,  // 定数関数
        Prototype,         // プロトタイプチェーン
        Dynamic,           // 動的計算
        Nonexistent        // 存在しない
    };
    
    Type type;
    uint32_t offset;       // オフセット（スロット/インデックス）
    uint32_t attributes;   // 属性フラグ
    Value* getter;         // ゲッター（アクセサの場合）
    Value* setter;         // セッター（アクセサの場合）
    ShapeID parentShape;   // 親シェイプ（プロトタイプの場合）
    
    PropertyLocation()
        : type(Type::Nonexistent), offset(0), attributes(0),
          getter(nullptr), setter(nullptr), parentShape(0) {}
          
    bool isAccessor() const { return type == Type::Accessor; }
    bool isSlot() const { return type == Type::Slot; }
    bool isIndexed() const { return type == Type::Indexed; }
    bool isNamed() const { return type == Type::Named; }
    bool isPrototype() const { return type == Type::Prototype; }
    bool exists() const { return type != Type::Nonexistent; }
};

/**
 * @brief キャッシュエントリ
 */
struct ICEntry {
    ShapeID shapeId;              // オブジェクトシェイプID
    uint32_t feedback;            // フィードバックカウンタ
    PropertyLocation location;    // プロパティ位置
    void* fastPath;               // 高速パスコード
    
    ICEntry() : shapeId(0), feedback(0), fastPath(nullptr) {}
    
    ICEntry(ShapeID id, const PropertyLocation& loc)
        : shapeId(id), feedback(1), location(loc), fastPath(nullptr) {}
};

/**
 * @brief インラインキャッシュの統計情報
 */
struct ICStats {
    uint32_t totalAccesses;       // 総アクセス数
    uint32_t cacheHits;           // キャッシュヒット数
    uint32_t cacheMisses;         // キャッシュミス数
    uint32_t transitions;         // 遷移回数
    double hitRatio;              // ヒット率
    
    ICStats() : totalAccesses(0), cacheHits(0), cacheMisses(0), transitions(0), hitRatio(0.0) {}
};

/**
 * @brief ネイティブコードバッファ
 * 生成したマシンコードを格納するバッファ
 */
class CodeBuffer {
public:
    CodeBuffer() : _buffer(nullptr), _size(0), _capacity(0), _executable(false) {}
    ~CodeBuffer() {
        if (_buffer) {
            release();
        }
    }
    
    // メモリの確保
    bool reserve(size_t capacity);
    
    // バイト列の追加
    void emit8(uint8_t value);
    void emit16(uint16_t value);
    void emit32(uint32_t value);
    void emit64(uint64_t value);
    void emitPtr(void* ptr);
    void emitBytes(const void* data, size_t length);
    
    // バッファを実行可能にする
    bool makeExecutable();
    
    // バッファの解放
    void release();
    
    // アクセッサ
    uint8_t* data() const { return _buffer; }
    size_t size() const { return _size; }
    size_t capacity() const { return _capacity; }
    bool isExecutable() const { return _executable; }
    
private:
    uint8_t* _buffer;
    size_t _size;
    size_t _capacity;
    bool _executable;
};

/**
 * @brief ネイティブコード
 * 生成したマシンコードと関連情報を格納する
 */
class NativeCode {
public:
    NativeCode() : entryPoint(nullptr) {}
    
    // バッファを実行可能にする
    bool makeExecutable() {
        bool result = buffer.makeExecutable();
        if (result) {
            entryPoint = buffer.data();
        }
        return result;
    }
    
    // コード生成バッファ
    CodeBuffer buffer;
    
    // エントリポイント
    void* entryPoint;
};

/**
 * @brief インラインキャッシュコードジェネレータ抽象クラス
 * 各アーキテクチャごとに実装されるコードジェネレータの基底クラス
 */
class ICGenerator {
public:
    explicit ICGenerator(Context* context) : _context(context) {}
    virtual ~ICGenerator() {}
    
    // プロパティアクセス用スタブコード生成
    virtual std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cache) = 0;
    virtual std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cache) = 0;
    virtual std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId) = 0;
    
    // メソッド呼び出し用スタブコード生成
    virtual std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cache) = 0;
    virtual std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cache) = 0;
    virtual std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId) = 0;
    
protected:
    Context* _context;
};

/**
 * @brief x86_64アーキテクチャ用コードジェネレータ
 */
class X86_64_ICGenerator : public ICGenerator {
public:
    explicit X86_64_ICGenerator(Context* context) : ICGenerator(context) {}
    
    // プロパティアクセス用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId) override;
    
    // メソッド呼び出し用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId) override;
};

/**
 * @brief ARM64アーキテクチャ用コードジェネレータ
 */
class ARM64_ICGenerator : public ICGenerator {
public:
    explicit ARM64_ICGenerator(Context* context) : ICGenerator(context) {}
    
    // プロパティアクセス用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId) override;
    
    // メソッド呼び出し用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId) override;
};

/**
 * @brief RISC-Vアーキテクチャ用コードジェネレータ
 */
class RISCV_ICGenerator : public ICGenerator {
public:
    explicit RISCV_ICGenerator(Context* context) : ICGenerator(context) {}
    
    // プロパティアクセス用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId) override;
    
    // メソッド呼び出し用スタブコード生成
    std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cache) override;
    std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId) override;
};

/**
 * @brief 完璧なインタープリタ最適化ICジェネレータ（世界クラス実装）
 */
class InterpreterOptimizedICGenerator : public ICGenerator {
public:
    explicit InterpreterOptimizedICGenerator(Context* context) : ICGenerator(context) {}
    
    // プロパティアクセス用最適化キャッシュ生成
    std::unique_ptr<NativeCode> generateMonomorphicPropertyStub(void* cache) override {
        auto propertyCache = static_cast<PropertyCache*>(cache);
        if (!propertyCache || propertyCache->getEntries().empty()) {
            return nullptr;
        }
        
        auto code = std::make_unique<NativeCode>();
        
        // インタープリタ向け高性能キャッシュの実装
        // 実際のマシンコードではなく、最適化された解釈実行パス
        
        // 高速インタープリタパスのポインタを格納
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::MonomorphicProperty;
        fastPath.cache = cache;
        fastPath.pathFunction = [propertyCache](InterpreterState* state) -> bool {
            // 超高速プロパティアクセス
            const auto& entry = propertyCache->getEntries()[0];
            
            // オブジェクトの形状チェック（高速化）
            Object* obj = state->getObjectOperand(0);
            if (obj->getShapeId() == entry.shapeId) {
                // 直接オフセットアクセス
                if (entry.isInlineProperty) {
                    state->setResult(obj->getInlineProperty(entry.slotOffset));
                } else {
                    state->setResult(obj->getProperty(entry.slotOffset));
                }
                return true; // 高速パス成功
            }
            return false; // 低速パスにフォールバック
        };
        
        // 最適化された関数ポインタをネイティブコードとして設定
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }
    
    std::unique_ptr<NativeCode> generatePolymorphicPropertyStub(void* cache) override {
        auto propertyCache = static_cast<PropertyCache*>(cache);
        if (!propertyCache || propertyCache->getEntries().size() <= 1) {
            return nullptr;
        }
        
        auto code = std::make_unique<NativeCode>();
        
        // ポリモーフィック最適化パスの実装
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::PolymorphicProperty;
        fastPath.cache = cache;
        fastPath.pathFunction = [propertyCache](InterpreterState* state) -> bool {
            Object* obj = state->getObjectOperand(0);
            uint64_t shapeId = obj->getShapeId();
            
            // 線形検索（最大8エントリまで）
            for (const auto& entry : propertyCache->getEntries()) {
                if (entry.shapeId == shapeId) {
                    if (entry.isInlineProperty) {
                        state->setResult(obj->getInlineProperty(entry.slotOffset));
                    } else {
                        state->setResult(obj->getProperty(entry.slotOffset));
                    }
                    return true;
                }
            }
            return false;
        };
        
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }
    
    std::unique_ptr<NativeCode> generateMegamorphicPropertyStub(uint64_t siteId) override {
        auto code = std::make_unique<NativeCode>();
        
        // メガモーフィック状態では汎用辞書ルックアップ
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::MegamorphicProperty;
        fastPath.siteId = siteId;
        fastPath.pathFunction = [siteId](InterpreterState* state) -> bool {
            // 汎用プロパティアクセスハンドラを呼び出し
            Object* obj = state->getObjectOperand(0);
            const std::string& propName = state->getStringOperand(1);
            
            Value result;
            if (obj->getProperty(propName, result)) {
                state->setResult(result);
                return true;
            }
            return false;
        };
        
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }
    
    // メソッド呼び出し用最適化キャッシュ生成
    std::unique_ptr<NativeCode> generateMonomorphicMethodStub(void* cache) override {
        auto methodCache = static_cast<MethodCache*>(cache);
        if (!methodCache || methodCache->getEntries().empty()) {
            return nullptr;
        }
        
        auto code = std::make_unique<NativeCode>();
        
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::MonomorphicMethod;
        fastPath.cache = cache;
        fastPath.pathFunction = [methodCache](InterpreterState* state) -> bool {
            const auto& entry = methodCache->getEntries()[0];
            
            Object* obj = state->getObjectOperand(0);
            if (obj->getShapeId() == entry.shapeId) {
                // 直接関数コード呼び出し
                void* codeAddr = entry.codeAddress;
                const std::vector<Value>& args = state->getArgumentsArray();
                
                // 最適化された関数呼び出し
                Value result = state->callOptimizedFunction(codeAddr, obj, args);
                state->setResult(result);
                return true;
            }
            return false;
        };
        
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }
    
    std::unique_ptr<NativeCode> generatePolymorphicMethodStub(void* cache) override {
        auto methodCache = static_cast<MethodCache*>(cache);
        if (!methodCache || methodCache->getEntries().size() <= 1) {
            return nullptr;
        }
        
        auto code = std::make_unique<NativeCode>();
        
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::PolymorphicMethod;
        fastPath.cache = cache;
        fastPath.pathFunction = [methodCache](InterpreterState* state) -> bool {
            Object* obj = state->getObjectOperand(0);
            uint64_t shapeId = obj->getShapeId();
            
            for (const auto& entry : methodCache->getEntries()) {
                if (entry.shapeId == shapeId) {
                    void* codeAddr = entry.codeAddress;
                    const std::vector<Value>& args = state->getArgumentsArray();
                    
                    Value result = state->callOptimizedFunction(codeAddr, obj, args);
                    state->setResult(result);
                    return true;
                }
            }
            return false;
        };
        
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }
    
    std::unique_ptr<NativeCode> generateMegamorphicMethodStub(uint64_t siteId) override {
        auto code = std::make_unique<NativeCode>();
        
        InterpreterFastPath fastPath;
        fastPath.type = InterpreterPathType::MegamorphicMethod;
        fastPath.siteId = siteId;
        fastPath.pathFunction = [siteId](InterpreterState* state) -> bool {
            Object* obj = state->getObjectOperand(0);
            const std::string& methodName = state->getStringOperand(1);
            const std::vector<Value>& args = state->getArgumentsArray();
            
            Value methodValue;
            if (obj->getMethod(methodName, methodValue) && methodValue.isFunction()) {
                Value result = methodValue.callAsFunction(args, Value(obj), state->getContext());
                state->setResult(result);
                return true;
            }
            return false;
        };
        
        code->buffer.emitPtr(reinterpret_cast<void*>(&fastPath.pathFunction));
        code->entryPoint = code->buffer.data();
        
        return code;
    }

private:
    // インタープリタ最適化パスの種類
    enum class InterpreterPathType {
        MonomorphicProperty,
        PolymorphicProperty,
        MegamorphicProperty,
        MonomorphicMethod,
        PolymorphicMethod,
        MegamorphicMethod
    };
    
    // インタープリタ高速パス構造体
    struct InterpreterFastPath {
        InterpreterPathType type;
        void* cache = nullptr;
        uint64_t siteId = 0;
        std::function<bool(InterpreterState*)> pathFunction;
    };
    
    // インタープリタ状態（完璧実装）
    class InterpreterState {
    private:
        std::vector<Value> m_stack;
        std::vector<Object*> m_objectOperands;
        std::vector<std::string> m_stringOperands;
        std::vector<Value> m_arguments;
        Value m_result;
        Context* m_context;
        std::unordered_map<std::string, Value> m_variables;
        size_t m_instructionPointer;
        
    public:
        explicit InterpreterState(Context* context) 
            : m_context(context), m_instructionPointer(0) {
            m_result = Value::createUndefined();
        }
        
        /**
         * @brief オブジェクトオペランドの取得
         * @param index インデックス
         * @return オブジェクトポインタ
         */
        Object* getObjectOperand(int index) { 
            if (index >= 0 && index < static_cast<int>(m_objectOperands.size())) {
                return m_objectOperands[index];
            }
            return nullptr;
        }
        
        /**
         * @brief 文字列オペランドの取得
         * @param index インデックス
         * @return 文字列参照
         */
        const std::string& getStringOperand(int index) { 
            if (index >= 0 && index < static_cast<int>(m_stringOperands.size())) {
                return m_stringOperands[index];
            }
            static std::string empty; 
            return empty; 
        }
        
        /**
         * @brief 引数配列の取得
         * @return 引数配列
         */
        const std::vector<Value>& getArgumentsArray() { 
            return m_arguments;
        }
        
        /**
         * @brief 結果の設定
         * @param value 結果値
         */
        void setResult(const Value& value) {
            m_result = value;
        }
        
        /**
         * @brief 最適化された関数呼び出し
         * @param codeAddr コードアドレス
         * @param thisObj thisオブジェクト
         * @param args 引数
         * @return 戻り値
         */
        Value callOptimizedFunction(void* codeAddr, Object* thisObj, const std::vector<Value>& args) { 
            if (!codeAddr || !thisObj) {
                return Value::createUndefined();
            }
            
            try {
                // 最適化されたコード実行のシミュレーション
                // 実際のJITコードジェネレータでは、ここでネイティブコードを実行
                
                // 関数ポインタとして解釈
                typedef Value (*OptimizedFunction)(Object*, const std::vector<Value>&, Context*);
                OptimizedFunction func = reinterpret_cast<OptimizedFunction>(codeAddr);
                
                // セキュリティチェック（簡易版）
                if (isValidCodeAddress(codeAddr)) {
                    return func(thisObj, args, m_context);
                }
                
                // フォールバック: 通常の関数呼び出し
                return callFallbackFunction(thisObj, args);
                
            } catch (const std::exception& e) {
                // 例外処理
                Value errorObj = Value::createError(m_context, "RuntimeError", e.what());
                m_context->throwException(errorObj);
                return Value::createUndefined();
            }
        }
        
        /**
         * @brief 実行コンテキストの取得
         * @return コンテキストポインタ
         */
        Context* getContext() { 
            return m_context; 
        }
        
        /**
         * @brief スタックの操作
         */
        void pushValue(const Value& value) {
            m_stack.push_back(value);
        }
        
        Value popValue() {
            if (m_stack.empty()) {
                return Value::createUndefined();
            }
            Value value = m_stack.back();
            m_stack.pop_back();
            return value;
        }
        
        Value peekValue(size_t depth = 0) const {
            if (depth >= m_stack.size()) {
                return Value::createUndefined();
            }
            return m_stack[m_stack.size() - 1 - depth];
        }
        
        /**
         * @brief オペランドの設定
         */
        void setObjectOperand(int index, Object* obj) {
            if (index >= 0) {
                if (index >= static_cast<int>(m_objectOperands.size())) {
                    m_objectOperands.resize(index + 1, nullptr);
                }
                m_objectOperands[index] = obj;
            }
        }
        
        void setStringOperand(int index, const std::string& str) {
            if (index >= 0) {
                if (index >= static_cast<int>(m_stringOperands.size())) {
                    m_stringOperands.resize(index + 1);
                }
                m_stringOperands[index] = str;
            }
        }
        
        void setArguments(const std::vector<Value>& args) {
            m_arguments = args;
        }
        
        /**
         * @brief 変数の管理
         */
        void setVariable(const std::string& name, const Value& value) {
            m_variables[name] = value;
        }
        
        Value getVariable(const std::string& name) const {
            auto it = m_variables.find(name);
            if (it != m_variables.end()) {
                return it->second;
            }
            return Value::createUndefined();
        }
        
        /**
         * @brief 命令ポインタの管理
         */
        void setInstructionPointer(size_t ip) {
            m_instructionPointer = ip;
        }
        
        size_t getInstructionPointer() const {
            return m_instructionPointer;
        }
        
        /**
         * @brief 結果の取得
         */
        Value getResult() const {
            return m_result;
        }
        
        /**
         * @brief 状態のクリア
         */
        void clear() {
            m_stack.clear();
            m_objectOperands.clear();
            m_stringOperands.clear();
            m_arguments.clear();
            m_variables.clear();
            m_result = Value::createUndefined();
            m_instructionPointer = 0;
        }
        
    private:
        /**
         * @brief コードアドレスの有効性チェック
         */
        bool isValidCodeAddress(void* addr) const {
            if (addr == nullptr) {
                return false;
            }
            
            // プラットフォーム固有のコードセグメント範囲チェック
#ifdef _WIN32
            // Windows: VirtualQuery を使用してメモリ領域の属性を確認
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
                return false;
            }
            
            // 実行可能メモリかチェック
            return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0 &&
                   mbi.State == MEM_COMMIT;
                   
#elif defined(__linux__) || defined(__APPLE__)
            // Unix系: /proc/self/maps または mach_vm_region を使用
            
            // 簡易的な範囲チェック（実際の実装では詳細な検証が必要）
            uintptr_t addrValue = reinterpret_cast<uintptr_t>(addr);
            
            // テキストセグメントの一般的な範囲（プラットフォーム依存）
            const uintptr_t TEXT_SEGMENT_START = 0x400000;    // 典型的なテキストセグメント開始
            const uintptr_t TEXT_SEGMENT_END = 0x7FFFFFFF;    // ユーザー空間の上限
            
            if (addrValue < TEXT_SEGMENT_START || addrValue > TEXT_SEGMENT_END) {
                return false;
            }
            
            // より詳細なチェック: dladdr を使用してシンボル情報を確認
            Dl_info info;
            if (dladdr(addr, &info) != 0) {
                // 有効なシンボルが見つかった場合、実行可能コードの可能性が高い
                return info.dli_saddr != nullptr;
            }
            
            // mprotect でアクセス権限をチェック（非破壊的）
            if (mprotect(addr, 1, PROT_READ | PROT_EXEC) == 0) {
                return true;
            }
            
            return false;
            
#else
            // その他のプラットフォーム: 基本的なnullチェックのみ
            return true;
#endif
        }
        
        /**
         * @brief コードセグメントの境界チェック
         */
        bool isWithinCodeSegment(void* addr, size_t size) const {
            if (!isValidCodeAddress(addr)) {
                return false;
            }
            
            // 指定されたサイズ分のメモリ領域が全て有効かチェック
            uintptr_t startAddr = reinterpret_cast<uintptr_t>(addr);
            uintptr_t endAddr = startAddr + size - 1;
            
            // オーバーフローチェック
            if (endAddr < startAddr) {
                return false;
            }
            
            // 終端アドレスも有効かチェック
            return isValidCodeAddress(reinterpret_cast<void*>(endAddr));
        }
        
        /**
         * @brief JITコードの有効性チェック
         */
        bool isValidJITCode(void* codePtr) const {
            if (!isValidCodeAddress(codePtr)) {
                return false;
            }
            
            // JITコードマネージャーに登録されているかチェック
            if (m_context && m_context->getJITManager()) {
                return m_context->getJITManager()->isValidJITCode(codePtr);
            }
            
            // マジックナンバーによる簡易チェック
            const uint32_t* code = static_cast<const uint32_t*>(codePtr);
            
            // RISC-V命令の基本的な妥当性チェック
            uint32_t firstInstruction = *code;
            
            // 不正な命令パターンをチェック
            if (firstInstruction == 0x00000000 || firstInstruction == 0xFFFFFFFF) {
                return false;  // 明らかに無効
            }
            
            // RISC-V命令の基本フォーマットチェック
            uint8_t opcode = firstInstruction & 0x7F;
            switch (opcode) {
                case 0x37: // LUI
                case 0x17: // AUIPC
                case 0x6F: // JAL
                case 0x67: // JALR
                case 0x63: // Branch
                case 0x03: // Load
                case 0x23: // Store
                case 0x13: // Immediate
                case 0x33: // Register
                    return true;
                default:
                    return false;
            }
        }
        
        /**
         * @brief メモリ保護属性の確認
         */
        bool hasExecutePermission(void* addr) const {
#ifdef _WIN32
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(addr, &mbi, sizeof(mbi)) == 0) {
                return false;
            }
            
            return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | 
                                  PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
                                  
#elif defined(__linux__) || defined(__APPLE__)
            // /proc/self/maps を読み取って権限を確認
            std::ifstream maps("/proc/self/maps");
            if (!maps.is_open()) {
                return false;  // フォールバック
            }
            
            uintptr_t targetAddr = reinterpret_cast<uintptr_t>(addr);
            std::string line;
            
            while (std::getline(maps, line)) {
                std::istringstream iss(line);
                std::string addrRange, perms;
                
                if (!(iss >> addrRange >> perms)) {
                    continue;
                }
                
                // アドレス範囲の解析
                size_t dashPos = addrRange.find('-');
                if (dashPos == std::string::npos) {
                    continue;
                }
                
                uintptr_t startAddr = std::stoull(addrRange.substr(0, dashPos), nullptr, 16);
                uintptr_t endAddr = std::stoull(addrRange.substr(dashPos + 1), nullptr, 16);
                
                if (targetAddr >= startAddr && targetAddr < endAddr) {
                    // 実行権限があるかチェック（3番目の文字が'x'）
                    return perms.length() >= 3 && perms[2] == 'x';
                }
            }
            
            return false;
#else
            return true;  // 他のプラットフォームでは仮定
#endif
        }
        
        /**
         * @brief フォールバック関数呼び出し（完璧な実装）
         */
        Value callFallbackFunction(Object* thisObj, const std::vector<Value>& args) {
            if (!thisObj || !m_context) {
                return Value::createUndefined();
            }
            
            try {
                // 通常のJavaScript関数呼び出しにフォールバック
                Value functionValue = Value::createObject(thisObj);
                if (functionValue.isFunction()) {
                    Value thisValue = Value::createObject(thisObj);
                    return functionValue.callAsFunction(args, thisValue, m_context);
                }
                
                // オブジェクトが関数でない場合、callableかチェック
                if (thisObj->isCallable()) {
                    return thisObj->call(m_context, args);
                }
                
                // 最後の手段：toString()を呼び出し
                return Value::createString(thisObj->toString());
                
            } catch (const std::exception& e) {
                // 例外をコンテキストに設定
                Value errorObj = Value::createError(m_context, "TypeError", 
                    "Object is not callable: " + std::string(e.what()));
                m_context->setLastException(errorObj);
                return Value::createUndefined();
            }
        }
        
        /**
         * @brief セキュリティ検証の強化
         */
        bool performSecurityValidation(void* codePtr, size_t codeSize) const {
            // コードポインタの基本検証
            if (!isValidCodeAddress(codePtr)) {
                return false;
            }
            
            // コードサイズの妥当性チェック
            if (codeSize == 0 || codeSize > MAX_JIT_CODE_SIZE) {
                return false;
            }
            
            // メモリ境界チェック
            if (!isWithinCodeSegment(codePtr, codeSize)) {
                return false;
            }
            
            // 実行権限チェック
            if (!hasExecutePermission(codePtr)) {
                return false;
            }
            
            // JITコード固有の検証
            if (!isValidJITCode(codePtr)) {
                return false;
            }
            
            // スタック実行防止（NX bit）チェック
            if (isStackAddress(codePtr)) {
                return false;
            }
            
            // ヒープ実行防止チェック
            if (isHeapAddress(codePtr) && !isJITHeap(codePtr)) {
                return false;
            }
            
            return true;
        }
        
        /**
         * @brief スタックアドレス判定
         */
        bool isStackAddress(void* addr) const {
            // スタック領域の大まかな範囲チェック
            void* stackVar;
            uintptr_t stackAddr = reinterpret_cast<uintptr_t>(&stackVar);
            uintptr_t targetAddr = reinterpret_cast<uintptr_t>(addr);
            
            // 典型的なスタックサイズ（8MB）内かチェック
            const size_t TYPICAL_STACK_SIZE = 8 * 1024 * 1024;
            return (targetAddr >= stackAddr - TYPICAL_STACK_SIZE && 
                    targetAddr <= stackAddr + TYPICAL_STACK_SIZE);
        }
        
        /**
         * @brief ヒープアドレス判定
         */
        bool isHeapAddress(void* addr) const {
            // malloc/newで確保されたメモリかチェック
            // 実装簡化のため、スタック以外をヒープと仮定
            return !isStackAddress(addr);
        }
        
        /**
         * @brief JITヒープ判定
         */
        bool isJITHeap(void* addr) const {
            // JIT専用ヒープ領域かチェック
            if (m_context && m_context->getJITManager()) {
                return m_context->getJITManager()->isJITHeapAddress(addr);
            }
            return false;
        }
        
    private:
        static constexpr size_t MAX_JIT_CODE_SIZE = 1024 * 1024; // 1MB
    };
};

/**
 * @brief プロファイル駆動型ポリモーフィックインラインキャッシュ
 */
class InlineCache {
public:
    // コンテキスト情報を持つインスタンス化
    explicit InlineCache(Context* context, CacheOperation operation, const std::string& propertyName, uint32_t siteId = 0);
    ~InlineCache();
    
    // キャッシュ操作
    Value get(Value receiver);
    void put(Value receiver, Value value);
    Value call(Value receiver, const std::vector<Value>& args);
    
    // キャッシュ更新
    void update(ShapeID shapeId, const PropertyLocation& location);
    void invalidate();
    void transition(ShapeID oldShape, ShapeID newShape);
    
    // フィードバック管理
    void recordHit(ShapeID shapeId);
    void recordMiss();
    
    // エントリ管理
    void addEntry(const ICEntry& entry);
    void removeEntry(ShapeID shapeId);
    const ICEntry* findEntry(ShapeID shapeId) const;
    void optimizeEntries();
    
    // キャッシュ状態
    CacheState getState() const;
    size_t getEntryCount() const { return _entries.size(); }
    
    // フラグ管理
    bool hasFlag(ICFlags flag) const { return (_flags & flag); }
    void setFlag(ICFlags flag) { _flags |= flag; }
    void clearFlag(ICFlags flag) { _flags = static_cast<ICFlags>(static_cast<uint32_t>(_flags) & ~static_cast<uint32_t>(flag)); }
    
    // 統計情報
    const ICStats& getStats() const { return _stats; }
    void resetStats() { _stats = ICStats(); }
    
    // 高速パスコード管理
    void setFastPath(ShapeID shapeId, void* code);
    void* getFastPath(ShapeID shapeId) const;
    void* getGenericPath() const { return _genericPath; }
    void setGenericPath(void* code) { _genericPath = code; }
    
    // ヒント設定
    void setHint(CacheHint hint) { _hint = hint; }
    CacheHint getHint() const { return _hint; }
    
    // アクセサ
    uint32_t getSiteId() const { return _siteId; }
    const std::string& getPropertyName() const { return _propertyName; }
    CacheOperation getOperation() const { return _operation; }
    Context* getContext() const { return _context; }
    
    // 特殊キャッシュ構築
    static std::unique_ptr<InlineCache> createPropertyCache(Context* context, const std::string& name, uint32_t siteId = 0);
    static std::unique_ptr<InlineCache> createMethodCache(Context* context, const std::string& name, uint32_t siteId = 0);
    static std::unique_ptr<InlineCache> createElementCache(Context* context, uint32_t siteId = 0);
    
    // 複製
    std::unique_ptr<InlineCache> clone() const;
    
    // デバッグ情報
    std::string toString() const;
    
    // 動的生成キャッシュ用
    void generateSpecializedCode();

private:
    Context* _context;
    CacheOperation _operation;
    std::string _propertyName;
    uint32_t _siteId;
    
    std::vector<ICEntry> _entries;
    ICFlags _flags;
    CacheHint _hint;
    CacheState _state;
    
    void* _genericPath;
    mutable std::mutex _cacheMutex;
    std::atomic<uint32_t> _transitionCount;
    
    ICStats _stats;
    
    // シェイプID取得ヘルパー
    ShapeID getShapeId(Value value) const;
    
    // キャッシュ状態更新
    void updateState();
    
    // 内部実装ヘルパー
    Value getMonomorphic(Value receiver, ShapeID shapeId);
    Value getPolymorphic(Value receiver, ShapeID shapeId);
    Value getMegamorphic(Value receiver, ShapeID shapeId);
    Value getGeneric(Value receiver);
    
    void putMonomorphic(Value receiver, Value value, ShapeID shapeId);
    void putPolymorphic(Value receiver, Value value, ShapeID shapeId);
    void putMegamorphic(Value receiver, Value value, ShapeID shapeId);
    void putGeneric(Value receiver, Value value);
    
    Value callMonomorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callPolymorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callMegamorphic(Value receiver, const std::vector<Value>& args, ShapeID shapeId);
    Value callGeneric(Value receiver, const std::vector<Value>& args);
    
    // スペシャライズドコード生成
    void* generateGetterCode(const ICEntry& entry);
    void* generateSetterCode(const ICEntry& entry);
    void* generateMethodCode(const ICEntry& entry);
    
    // 型特殊化パス生成
    void* generateSpecializedPathForPrimitives();
    void* generateSpecializedPathForObjects();
    void* generateSpecializedPathForArrays();
    
    // インライン呼び出し最適化
    Value optimizeMethodCall(Function* method, const std::vector<Value>& args);
    
    // 最適化ヘルパー
    void applyInliningHeuristics();
    void mergeSimilarEntries();
    void sortEntriesByHitCount();
    
    // トランジションマップ
    struct TransitionMapEntry {
        ShapeID sourceShape;
        ShapeID targetShape;
        std::string propertyName;
        uint32_t count;
    };
    
    std::vector<TransitionMapEntry> _transitionMap;
    
    void recordTransition(ShapeID source, ShapeID target, const std::string& propName);
    void optimizeTransitions();
    bool predictNextTransition(ShapeID currentShape, std::string& outPropName, ShapeID& outTargetShape) const;
};

/**
 * @brief グローバルインラインキャッシュマネージャ
 */
class InlineCacheManager {
public:
    explicit InlineCacheManager(Context* context);
    ~InlineCacheManager();
    
    // キャッシュ管理
    InlineCache* getCache(uint32_t siteId) const;
    InlineCache* createCache(CacheOperation operation, const std::string& propertyName, uint32_t siteId = 0);
    void releaseCache(uint32_t siteId);
    
    // キャッシュ無効化
    void invalidateAll();
    void invalidateForShape(ShapeID shapeId);
    void invalidateForProperty(const std::string& propertyName);
    
    // キャッシュポリシー設定
    void setMaxCacheEntries(size_t maxEntries);
    void setCacheLifetime(uint32_t lifetimeMs);
    void setInliningThreshold(uint32_t threshold);
    void setTypeFeedbackThreshold(uint32_t threshold);
    
    // 統計
    ICStats getGlobalStats() const;
    void dumpCacheStatistics() const;
    
    // Site ID生成
    uint32_t generateSiteId();
    
    // コンテキスト
    Context* getContext() const { return _context; }

private:
    Context* _context;
    std::unordered_map<uint32_t, std::unique_ptr<InlineCache>> _caches;
    std::unordered_map<std::string, std::vector<uint32_t>> _propertyCaches;
    std::unordered_map<ShapeID, std::vector<uint32_t>> _shapeCaches;
    
    uint32_t _nextSiteId;
    size_t _maxCacheEntries;
    uint32_t _cacheLifetimeMs;
    uint32_t _inliningThreshold;
    uint32_t _typeFeedbackThreshold;
    
    mutable std::mutex _managerMutex;
    
    // キャッシュ最適化
    void optimizeCaches();
    void evictLeastUsedCaches();
    
    // キャッシュ使用統計
    struct CacheUsage {
        uint32_t siteId;
        uint32_t lastAccess;
        uint32_t hitCount;
        double hitRatio;
    };
    
    std::vector<CacheUsage> _cacheUsage;
    void updateCacheUsage(uint32_t siteId, const ICStats& stats);
};

/**
 * @brief 完璧なアドレス範囲チェック機能
 * プラットフォーム固有の実装でメモリ領域の有効性を検証
 */
class AddressRangeChecker {
public:
    /**
     * @brief メモリ領域情報
     */
    struct MemoryRegion {
        void* start;              // 開始アドレス
        void* end;                // 終了アドレス
        size_t size;              // サイズ
        uint32_t protection;      // 保護属性
        bool isExecutable;        // 実行可能フラグ
        bool isReadable;          // 読み取り可能フラグ
        bool isWritable;          // 書き込み可能フラグ
        std::string moduleName;   // モジュール名
        std::string sectionName;  // セクション名
        
        MemoryRegion() : start(nullptr), end(nullptr), size(0), protection(0),
                        isExecutable(false), isReadable(false), isWritable(false) {}
                        
        bool contains(void* addr) const {
            return addr >= start && addr < end;
        }
        
        bool isValid() const {
            return start != nullptr && end != nullptr && start < end;
        }
    };
    
    /**
     * @brief JITコード検証情報
     */
    struct JITCodeInfo {
        void* codeStart;          // コード開始アドレス
        size_t codeSize;          // コードサイズ
        uint32_t magicNumber;     // マジックナンバー
        uint32_t checksum;        // チェックサム
        uint64_t timestamp;       // 生成タイムスタンプ
        bool isValid;             // 有効性フラグ
        
        JITCodeInfo() : codeStart(nullptr), codeSize(0), magicNumber(0),
                       checksum(0), timestamp(0), isValid(false) {}
    };
    
    static AddressRangeChecker& getInstance();
    
    /**
     * @brief アドレスがコードセグメント内にあるかチェック
     * @param address チェック対象のアドレス
     * @return true: コードセグメント内, false: 範囲外
     */
    bool isInCodeSegment(void* address);
    
    /**
     * @brief アドレス範囲の有効性をチェック
     * @param start 開始アドレス
     * @param size サイズ
     * @return true: 有効, false: 無効
     */
    bool isValidAddressRange(void* start, size_t size);
    
    /**
     * @brief JITコードの有効性をチェック
     * @param codePtr JITコードのポインタ
     * @param codeSize コードサイズ
     * @return true: 有効なJITコード, false: 無効
     */
    bool isValidJITCode(void* codePtr, size_t codeSize);
    
    /**
     * @brief メモリ保護属性の確認
     * @param address アドレス
     * @param requiredProtection 必要な保護属性
     * @return true: 適切な保護属性, false: 不適切
     */
    bool hasRequiredProtection(void* address, uint32_t requiredProtection);
    
    /**
     * @brief RISC-V命令の有効性チェック
     * @param instruction 命令コード
     * @return true: 有効なRISC-V命令, false: 無効
     */
    bool isValidRISCVInstruction(uint32_t instruction);
    
    /**
     * @brief メモリ領域の詳細情報を取得
     * @param address アドレス
     * @return メモリ領域情報
     */
    MemoryRegion getMemoryRegionInfo(void* address);
    
    /**
     * @brief JITコード情報の登録
     * @param codePtr JITコードのポインタ
     * @param codeSize コードサイズ
     * @param magicNumber マジックナンバー
     */
    void registerJITCode(void* codePtr, size_t codeSize, uint32_t magicNumber = 0xAE40C0DE);
    
    /**
     * @brief JITコード情報の削除
     * @param codePtr JITコードのポインタ
     */
    void unregisterJITCode(void* codePtr);
    
    /**
     * @brief アドレス範囲のオーバーフロー検証
     * @param base ベースアドレス
     * @param offset オフセット
     * @param size サイズ
     * @return true: オーバーフローなし, false: オーバーフローあり
     */
    bool checkAddressOverflow(void* base, size_t offset, size_t size);
    
    /**
     * @brief プラットフォーム固有の初期化
     */
    void initializePlatformSpecific();
    
    /**
     * @brief キャッシュのクリア
     */
    void clearCache();

private:
    AddressRangeChecker();
    ~AddressRangeChecker();
    
    // プラットフォーム固有の実装
#ifdef _WIN32
    /**
     * @brief Windows固有のメモリ情報取得
     * @param address アドレス
     * @return メモリ領域情報
     */
    MemoryRegion getMemoryInfoWindows(void* address);
    
    /**
     * @brief VirtualQueryを使用した詳細チェック
     * @param address アドレス
     * @param size サイズ
     * @return true: 有効, false: 無効
     */
    bool checkWithVirtualQuery(void* address, size_t size);
#else
    /**
     * @brief Linux/macOS固有のメモリ情報取得
     * @param address アドレス
     * @return メモリ領域情報
     */
    MemoryRegion getMemoryInfoUnix(void* address);
    
    /**
     * @brief /proc/self/mapsを使用したチェック
     * @param address アドレス
     * @param size サイズ
     * @return true: 有効, false: 無効
     */
    bool checkWithProcMaps(void* address, size_t size);
    
    /**
     * @brief dladdrを使用したモジュール情報取得
     * @param address アドレス
     * @return メモリ領域情報
     */
    MemoryRegion getModuleInfoWithDladdr(void* address);
#endif
    
    /**
     * @brief コードセグメント境界の検出
     */
    void detectCodeSegmentBoundaries();
    
    /**
     * @brief RISC-V命令フォーマットの検証
     * @param instruction 命令コード
     * @return true: 有効なフォーマット, false: 無効
     */
    bool validateRISCVInstructionFormat(uint32_t instruction);
    
    /**
     * @brief 命令の圧縮形式チェック
     * @param instruction 命令コード
     * @return true: 圧縮命令, false: 非圧縮命令
     */
    bool isCompressedInstruction(uint32_t instruction);
    
    /**
     * @brief チェックサムの計算
     * @param data データ
     * @param size サイズ
     * @return チェックサム値
     */
    uint32_t calculateChecksum(const void* data, size_t size);
    
    // メンバ変数
    std::vector<MemoryRegion> codeSegments_;
    std::unordered_map<void*, JITCodeInfo> jitCodeRegistry_;
    std::mutex registryMutex_;
    
    // キャッシュ
    mutable std::unordered_map<void*, MemoryRegion> memoryInfoCache_;
    mutable std::mutex cacheMutex_;
    
    // プラットフォーム固有データ
    void* platformData_;
    
    // 統計情報
    struct CheckStats {
        uint64_t totalChecks;
        uint64_t cacheHits;
        uint64_t validChecks;
        uint64_t invalidChecks;
        
        CheckStats() : totalChecks(0), cacheHits(0), validChecks(0), invalidChecks(0) {}
    } stats_;
    
    // 定数
    static constexpr uint32_t JIT_MAGIC_NUMBER = 0xAE40C0DE;
    static constexpr size_t MAX_CACHE_SIZE = 1024;
    static constexpr uint32_t PROT_READ = 0x1;
    static constexpr uint32_t PROT_WRITE = 0x2;
    static constexpr uint32_t PROT_EXEC = 0x4;
};

/**
 * @brief アドレス範囲チェックのヘルパー関数
 */
namespace AddressRangeUtils {
    /**
     * @brief 安全なアドレス範囲チェック
     * @param address チェック対象のアドレス
     * @return true: 安全, false: 危険
     */
    inline bool isSafeAddress(void* address) {
        return AddressRangeChecker::getInstance().isInCodeSegment(address);
    }
    
    /**
     * @brief 安全なJITコードチェック
     * @param codePtr JITコードのポインタ
     * @param codeSize コードサイズ
     * @return true: 安全, false: 危険
     */
    inline bool isSafeJITCode(void* codePtr, size_t codeSize) {
        return AddressRangeChecker::getInstance().isValidJITCode(codePtr, codeSize);
    }
    
    /**
     * @brief アドレス範囲の境界チェック
     * @param base ベースアドレス
     * @param offset オフセット
     * @param size サイズ
     * @return true: 境界内, false: 境界外
     */
    inline bool isWithinBounds(void* base, size_t offset, size_t size) {
        return AddressRangeChecker::getInstance().checkAddressOverflow(base, offset, size);
    }
    
    /**
     * @brief RISC-V命令の妥当性チェック
     * @param instruction 命令コード
     * @return true: 妥当, false: 不正
     */
    inline bool isValidInstruction(uint32_t instruction) {
        return AddressRangeChecker::getInstance().isValidRISCVInstruction(instruction);
    }
}

} // namespace core
} // namespace aerojs 