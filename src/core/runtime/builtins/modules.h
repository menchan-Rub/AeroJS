/**
 * @file modules.h
 * @brief 組み込みモジュールの初期化関数の宣言
 * @copyright 2023 AeroJS プロジェクト
 */

#ifndef AERO_MODULES_H
#define AERO_MODULES_H

namespace aero {

class GlobalObject;

// 基本的な組み込みオブジェクト
extern void registerObjectBuiltin(GlobalObject* globalObj);
extern void registerFunctionBuiltin(GlobalObject* globalObj);
extern void registerArrayBuiltin(GlobalObject* globalObj);
extern void registerStringBuiltin(GlobalObject* globalObj);
extern void registerNumberBuiltin(GlobalObject* globalObj);
extern void registerBooleanBuiltin(GlobalObject* globalObj);
extern void registerDateBuiltin(GlobalObject* globalObj);
extern void registerRegExpBuiltin(GlobalObject* globalObj);
extern void registerErrorBuiltin(GlobalObject* globalObj);

// コレクション関連の組み込みオブジェクト
extern void registerSetBuiltin(GlobalObject* globalObj);
extern void registerMapBuiltin(GlobalObject* globalObj);
extern void registerWeakMapBuiltin(GlobalObject* globalObj);
extern void registerWeakSetBuiltin(GlobalObject* globalObj);

// ES6以降の組み込みオブジェクト
extern void registerPromiseBuiltin(GlobalObject* globalObj);
extern void registerSymbolBuiltin(GlobalObject* globalObj);
extern void registerProxyBuiltin(GlobalObject* globalObj);
extern void registerReflectBuiltin(GlobalObject* globalObj);
extern void registerTypedArrayBuiltin(GlobalObject* globalObj);

// ES2021以降の組み込みオブジェクト
extern void registerWeakRefBuiltin(GlobalObject* globalObj);
extern void registerFinalizationRegistryBuiltin(GlobalObject* globalObj);

// ユーティリティオブジェクト
extern void registerJSONBuiltin(GlobalObject* globalObj);
extern void registerMathBuiltin(GlobalObject* globalObj);

} // namespace aero

#endif // AERO_MODULES_H 