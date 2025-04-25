# AeroJS 超最適化 JIT コンパイラ 完全ドキュメント

> **バージョン**: 1.0.0
> **最終更新日**: 2024-06-15
> **著者**: AeroJS Team

---

## 目次

1. [はじめに](#1-はじめに)
2. [ビルドおよび導入方法](#2-ビルドおよび導入方法)
3. [ディレクトリ構成](#3-ディレクトリ構成)
4. [クラス階層とアーキテクチャ](#4-クラス階層とアーキテクチャ)
5. [バイトコード命令セット](#5-バイトコード命令セット)
6. [中間表現 (IR) の設計](#6-中間表現-ir-の設計)
7. [最適化パイプライン](#7-最適化パイプライン)
8. [x86_64 コード生成の詳細](#8-x86_64-コード生成の詳細)
9. [ARM64 コード生成の詳細](#9-arm64-コード生成の詳細)
10. [RISC‑V コード生成の詳細](#10-risc-v-コード生成の詳細)
11. [ローカル変数とスタックフレーム](#11-ローカル変数とスタックフレーム)
12. [関数呼び出しABI](#12-関数呼び出しabi)
13. [サンプルコードと使用例](#13-サンプルコードと使用例)
14. [テストとベンチマーク](#14-テストとベンチマーク)
15. [デバッグとトラブルシューティング](#15-デバッグとトラブルシューティング)
16. [APIリファレンス](#16-apiリファレンス)
17. [拡張ガイド](#17-拡張ガイド)
18. [FAQ](#18-faq)
19. [ライセンス](#19-ライセンス)

---

## 1. はじめに

AeroJS の超最適化 JIT コンパイラは、JavaScript バイトコードや AeroJS 独自バイトコードを動的にネイティブ機械語へ変換し、最高の実行性能を実現することを目的としています。V8 を凌駕する性能を目指し、以下の特徴を備えます：

- **多段最適化パイプライン**：定数畳み込み、ループ展開、インライン化、デッドコード除去など、複数パスを組み合わせた最適化を実施
- **マルチアーキテクチャ対応**：x86_64、ARM64、RISC‑V 向けの専用バックエンドを提供
- **高いテストカバレッジ**：単体テスト、統合テスト、ベンチマークテストを CI/CD で常時検証
- **豊富なドキュメント**：設計ガイド、API リファレンス、チュートリアル、拡張ガイドなどを完備

---

## 2. ビルドおよび導入方法

### 必須依存

- CMake ≥ 3.14
- Ninja (または Make)
- C++17 対応コンパイラ (GCC ≥11, Clang ≥14 等)
- Doxygen（オプション、ドキュメント生成用）

### クローン・ビルド手順

```bash
# リポジトリをクローン
git clone https://github.com/aerojs/aerojs.git
cd aerojs

# ビルドディレクトリ作成
mkdir build && cd build

# CMake 設定 (テスト・ドキュメントも有効化)
cmake .. -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DBUILD_DOCS=ON

# ビルド実行
ninja -j$(nproc)
```

### テスト実行

```bash
cd build
ctest --output-on-failure -j$(nproc)
```

### ドキュメント生成

```bash
# Doxygen がインストールされている場合
cd build
cmake --build . --target doc
# HTML ドキュメントは build/html 配下へ出力
```

---

## 3. ディレクトリ構成

```plaintext
aerojs/
├─ include/                      # 公開ヘッダー
├─ src/
│  ├─ core/
│  │  ├─ jit/
│  │  │  ├─ ir/                  # 中間表現 (IR) モジュール
│  │  │  ├─ backend/
│  │  │  │  ├─ x86_64/           # x86_64 バックエンド
│  │  │  │  ├─ arm64/            # ARM64 バックエンド
│  │  │  │  └─ riscv/            # RISC‑V バックエンド (予定)
│  │  │  ├─ baseline/            # BaselineJIT (スタブ実装)
│  │  │  ├─ optimizing/          # OptimizingJIT 骨子
│  │  │  └─ super_optimizing_jit.{h,cpp} # 超最適化JITパイプライン
│  │  └─ ...                     
│  └─ ...                        
├─ tests/                        # テストコード
│  └─ jit/jit_compiler_test.cpp  
└─ docs/                         # ドキュメント
   ├─ api/jit.md                # このマニュアル
   ├─ design/                   # 設計ガイド
   └─ examples/                 # サンプルコード
```

---

## 4. クラス階層とアーキテクチャ

```plaintext
JITCompiler (抽象)
├─ BaselineJIT            # バイトコードをそのままマシンコードと見做す最小実装
├─ OptimizingJIT          # 多段最適化パスの骨子を提供
└─ SuperOptimizingJIT     # IR管理 + 最適化パイプライン + バックエンド呼び出し
```

### JITCompiler

- `virtual ~JITCompiler() noexcept`
- `virtual std::unique_ptr<uint8_t[]> Compile(const std::vector<uint8_t>& bytecodes, size_t& outSize) noexcept = 0`
- `virtual void Reset() noexcept = 0`

### BaselineJIT

- bytecodes をそのまま返却するデバッグ用スタブ

### OptimizingJIT

- `Compile`→`Reset` の骨子を定義

### SuperOptimizingJIT

1. `BuildIR()`：バイトコードから IR 生成
2. `RunOptimizationPasses()`：最適化パス実行
3. `GenerateMachineCode()`：バックエンドによる機械語生成

---

## 5. バイトコード命令セット

| バイト値      | Opcode       | オペランド             | 説明                                            |
|-------------|-------------|------------------------|-------------------------------------------------|
| `0x00…0x7F` | LoadConst   | imm8                   | 即値 (0–127) をロードしスタックにプッシュ        |
| `0x80`      | Add         | —                      | pop 2→加算→push                                  |
| `0x81`      | Sub         | —                      | pop 2→減算→push                                  |
| `0x82`      | Mul         | —                      | pop 2→乗算→push                                  |
| `0x83`      | Div         | —                      | pop 2→除算→push                                  |
| `0x84`      | LoadVar     | varIndex (次バイト)    | ローカル変数をスタックにロード                  |
| `0x85`      | StoreVar    | varIndex (次バイト)    | pop→ローカル変数にストア                        |
| `0xF0`      | Call        | funcIndex (次バイト)   | pop→関数テーブル[funcIndex]を間接 call→push     |
| `0xFF`      | Return      | —                      | leave; ret (関数終了)                           |

### 命令エンコード例

```text
# バイト列: [0x10,0x20,0x80,0xFF]
# 解釈:
LoadConst 16   # [16]
LoadConst 32   # [16,32]
Add            # [48]
Return         # 関数終了 → RAX/X0 に 48 を返却
```

---

## 6. 中間表現 (IR) の設計

```cpp
enum class Opcode {
  Nop, LoadConst, Add, Sub, Mul, Div,
  LoadVar, StoreVar, Call, Return
};

struct IRInstruction {
  Opcode opcode;
  std::vector<int32_t> args;  // 各命令の引数
};

class IRFunction {
public:
  void AddInstruction(const IRInstruction& inst) noexcept;
  const std::vector<IRInstruction>& GetInstructions() const noexcept;
  void Clear() noexcept;
private:
  std::vector<IRInstruction> instructions_;
};
```

- **IRFunction** は命令列を保持し、最適化パスやコード生成に渡されます。
- **BuildIR** で bytecodes → IRInstruction のリストへ変換し、末尾に Return を追加。
- **RunOptimizationPasses** で`instructions_`を書き換え。

---

## 7. 最適化パイプライン

+0. **バイトコード検証 (Bytecode Validation)**
+   - `SuperOptimizingJIT::ValidateBytecode` によって、不正命令やオペランド欠落を事前に検査し、安全性を担保します。
+   - 検証不合格時は `Compile()` が即座に `nullptr` を返却します。
 1. **定数畳み込み (Constant Folding)**
    - `LoadConst a`, `LoadConst b`, `Add` → `LoadConst(a+b)` に置換
 2. **デッドコード除去 (Dead Code Elimination)**
    - 使用されない LoadConst 命令を削除
 3. **ループ展開 (Loop Unrolling)**
    - 小ループを展開しジャンプを削減
 4. **関数インライン化 (Function Inlining)**
    - 呼び出し先小関数を呼び出し元に展開しオーバーヘッドを低減
 5. **型プロモーション (Type Promotion)**
    - 実行時プロファイルに基づき型を限定し、より高速な命令を選択

---

## 8. x86_64 コード生成の詳細

### プロローグ

```asm
push rbp
mov  rbp, rsp
sub  rsp, <stack_size>
```

### 命令エミット例

| Opcode      | バイナリシーケンス                  | 説明                                      |
|-------------|-------------------------------------|-------------------------------------------|
| LoadConst   | 48 C7 C0 ii ii ii ii   50           | mov rax, imm32; push rax                  |
| Add         | 58 5B 48 01 D8 50                   | pop rax; pop rbx; add rax,rbx; push rax   |
| Sub         | 5B 58 48 29 D8 50                   | pop rbx; pop rax; sub rax,rbx; push rax   |
| Mul         | 5B 58 48 0F AF C3 50                | pop rbx; pop rax; imul rax,rbx; push rax  |
| Div         | 5B 58 48 99 48 F7 FB 50             | pop rbx; pop rax; cqo; idiv rbx; push rax |
| LoadVar     | 48 8B 85 dd dd dd dd 50             | mov rax,[rbp-dispx]; push rax             |
| StoreVar    | 58 48 89 85 dd dd dd dd             | pop rax; mov [rbp-dispx],rax              |
| Call        | 58 FF D0 50                         | pop rax; call rax; push rax               |
| Return      | C9 C3                               | leave; ret                                |

### エピローグ

```asm
add rsp, <stack_size>
leave
ret
```

---

## 9. ARM64 コード生成の詳細

### プロローグ

```asm
stp x29,x30,[sp,#-16]!  # push fp,lr
mov x29, sp
sub sp, sp, #<stack_size>
```

### 命令エミット例 (暫定)

| Opcode      | 命令列                                  | 説明            |
|-------------|----------------------------------------|----------------|
| LoadConst   | mov w0,#imm; str w0,[sp,#-4]!         | 即値ロード→プッシュ |
| Add         | ldr w1,[sp]; ldr w0,[sp,#4]; add w0,w0,w1; str w0,[sp,#-4]! | 加算 |

### エピローグ

```asm
add sp, #<stack_size>
ldp x29,x30,[sp],#16
ret
```

---

## 10. RISC‑V コード生成の詳細

（RV64I 標準命令セット対応）

### プロローグ

```asm
addi sp,sp,-<stack_size>
sd ra,0(sp)
sd fp,8(sp)
addi fp,sp,<offset>
```

### 命令エミット例

| Opcode      | 命令列                             | 説明          |
|-------------|----------------------------------|--------------|
| LoadConst   | li t0,imm; sd t0,-8(sp); addi sp,sp,-8 | 即値ロード→プッシュ |
| Add         | ld t1,0(sp); ld t0,8(sp); add t0,t0,t1; sd t0,-8(sp); addi sp,sp,-8 | 加算 |

### エピローグ

```asm
ld fp,8(sp)
ld ra,0(sp)
addi sp,sp,<stack_size>
jalr ra
```

---

## 11. ローカル変数とスタックフレーム

- スタックフレーム内の変数オフセット: `rbp - 8*(index+1)` / `fp - 8*(index+1)`
- 引数受け渡しとローカル変数用のアライメント (16byte 境界)
- 64bit/32bit 値の混在サポート

---

## 12. 関数呼び出しABI

| Arch       | 引数レジスタ                   | 保存レジスタ           | 戻り値レジスタ    |
|------------|-------------------------------|-----------------------|------------------|
| x86_64     | RDI, RSI, RDX, RCX, R8, R9    | RBX, RBP, R12–R15     | RAX              |
| ARM64      | X0–X7                         | X19–X28, FP, LR       | X0               |
| RISC‑V     | a0–a7                         | s0–s11, ra            | a0               |

---

## 13. サンプルコードと使用例

```cpp
#include <iostream>
#include "super_optimizing_jit.h"

int main() {
  using namespace aerojs::core;
  SuperOptimizingJIT jit;
  // 例: 10 + 20 を実行
  std::vector<uint8_t> bytecode = {10, 20, 0x80};
  size_t codeSize;
  auto code = jit.Compile(bytecode, codeSize);
  // 実行可能メモリにコピー後関数ポインタとして呼び出し
  auto func = reinterpret_cast<int(*)()>(allocate_executable_memory(code.get(), codeSize));
  int result = func();
  std::cout << "10 + 20 = " << result << "\n";
  return 0;
}
```

---

## 14. テストとベンチマーク

- **単体テスト**: `tests/jit/jit_compiler_test.cpp`
- **統合テスト**: CI で ctest により全テスト実行
- **ベンチマーク**: `benchmarks/jit_benchmark.cpp`
  ```bash
  cd build
  ./benchmarks/jit_benchmark --iterations=1e6
  ```

---

## 15. デバッグとトラブルシューティング

- 環境変数 `AEROJS_JIT_LOG=[ERROR|WARN|INFO|DEBUG]` でログレベル制御
- `ulimit -c unlimited` でコアダンプ有効化
- よくあるエラー: `nullptr` 戻却 (バイトコード不正)、`segfault` (実行メモリ保護不足)

---

## 16. APIリファレンス

### メモリマネージャ

以下のAPIで、生成した機械語バイト列を実行可能メモリ領域へコピーし、実行権限を設定／解放します。

```cpp
namespace aerojs::core {
  void* allocate_executable_memory(const void* code, size_t size) noexcept;
  void  free_executable_memory(void* ptr, size_t size) noexcept;
}
```

### class `aerojs::core::JITCompiler`

| メソッド                                                | 説明                          |
|--------------------------------------------------------|-------------------------------|
| `virtual ~JITCompiler() noexcept`                      | デストラクタ                 |
| `virtual Compile(const vector<uint8_t>&, size_t&)`    | バイトコード→機械語           |
| `virtual Reset() noexcept`                             | 内部状態リセット             |

### class `aerojs::core::SuperOptimizingJIT`

| メソッド                                                                          | 説明                                |
|----------------------------------------------------------------------------------|-------------------------------------|
| `Compile(const vector<uint8_t>&, size_t&) noexcept`                              | メインエントリ                     |
| `Reset() noexcept`                                                               | IR / プロファイル情報リセット       |
| `BuildIR(const vector<uint8_t>&) noexcept`                                      | バイトコード→IR                     |
| `RunOptimizationPasses() noexcept`                                               | 最適化パス                           |
| `GenerateMachineCode(vector<uint8_t>&) noexcept`                                | 機械語生成                           |

### class `aerojs::core::X86_64CodeGenerator`

| メソッド                                            | 説明                      |
|----------------------------------------------------|---------------------------|
| `Generate(const IRFunction&, vector<uint8_t>&)`    | バイナリ生成メイン       |
| `EmitPrologue(vector<uint8_t>&)`                   | プロローグ emit           |
| `EmitInstruction(const IRInstruction&, vector<uint8_t>&)` | 命令 emit         |
| `EmitEpilogue(vector<uint8_t>&)`                   | エピローグ emit           |

(ARM64/RISC‑V も同様の API 構造)

---

## 17. 拡張ガイド

- **IR 最適化パス追加**: `src/core/jit/optimizing` 内のパスを実装
- **新規バックエンド追加**: `src/core/jit/backend/<arch>` 配下に実装し `GenerateMachineCode` に分岐追加
- **組み込み関数対応**: バイト値 0xE0〜を `BuiltinCall` に割り当て

---

## 18. FAQ

**Q1. バイトコード検証は？**
A1. 現在未実装。次フェーズでバイトコードバリデータを追加予定。

**Q2. 例外は投げる？**
A2. JIT内部では例外を投げず、エラーコードとログで検知。

**Q3. メモリ保護は？**
A3. `mprotect` で実行/書込権限を分離。

---

## 19. ライセンス

AeroJS JIT コンパイラは **MIT License** のもとで公開。
詳細はプロジェクトルートの `LICENSE` を参照。

---

*© AeroJS Team* 