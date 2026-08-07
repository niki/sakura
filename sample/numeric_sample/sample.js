// JavaScript 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
const a1 = 123;               // 10進数
const a2 = 0xFF;               // 16進数
const a3 = 3.14;               // 浮動小数点

const b1 = 0b1010;             // 2進数リテラル(ES2015+) ← 新規対応
const b2 = 0B1111_0000;        // 2進数 + 桁区切り記号
const b3 = 0o17;               // 8進数リテラル(ES2015+) ← 新規対応
const b4 = 0O17;
const b5 = 1_000_000;          // 桁区切り記号(10進) ← 新規対応
const b6 = 0xFF_FF;            // 桁区切り記号(16進)

const c1 = 123n;               // BigIntサフィックス(10進) ← 新規対応
const c2 = 0x1Fn;              // BigIntサフィックス(16進)
const c3 = 0b101n;             // BigIntサフィックス(2進)

console.log(a1, a2, a3, b1, b2, b3, b4, b5, b6, c1, c2, c3);
