// C/C++ 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
#include <cstdio>

int main()
{
    int a1 = 123;              // 10進数
    int a2 = 0123;             // 8進数(先頭ゼロ、そのまま10進数風に色付け)
    int a3 = 0xFF;             // 16進数 (IsNumber()の既存機能)
    int a4 = -123;             // マイナス符号付き
    double a5 = 3.14;          // 浮動小数点
    double a6 = 1.2e+3;        // 指数表記(+)
    double a7 = 1.2e-3;        // 指数表記(-)

    int b1 = 0b1010;           // 2進数リテラル(C++14) ← 新規対応
    int b2 = 0B1111'0000;      // 2進数 + 桁区切り記号
    long b3 = 36'000'000;      // 桁区切り記号(10進) ← 新規対応
    long b4 = 0x12'34'56'78;   // 桁区切り記号(16進)

    unsigned int c1 = 328u;    // uサフィックス ← 新規対応
    unsigned int c2 = 328U;    // Uサフィックス
    long c3 = 108L;            // Lサフィックス(既存)
    long long c4 = 108LL;      // LLサフィックス ← 新規対応(2文字目)
    unsigned long long c5 = 0x8000000000000000ULL; // ULLサフィックス ← 新規対応

    long double d1 = 1.5L;     // long doubleのLサフィックス(小数点あり) ← 新規対応

    printf("%d %d %d %d %f %f %f %d %d %ld %ld %u %u %ld %lld %llu %Lf\n",
        a1,a2,a3,a4,(int)a5,(int)a6,(int)a7,b1,b2,b3,b4,c1,c2,c3,c4,c5,(double)d1);
    return 0;
}
