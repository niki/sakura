// C# 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
class Sample
{
    static void Main()
    {
        int a1 = 123;                 // 10進数
        int a2 = 0xFF;                 // 16進数
        double a3 = 3.14;              // 浮動小数点

        int b1 = 0b1010;               // 2進数リテラル(C# 7.0+) ← 新規対応
        int b2 = 0B1111_0000;          // 2進数 + 桁区切り記号
        long b3 = 1_000_000;           // 桁区切り記号(10進) ← 新規対応

        uint c1 = 328u;                // uサフィックス
        long c2 = 100L;                // Lサフィックス
        ulong c3 = 100UL;              // ULサフィックス(組み合わせ) ← 新規対応
        ulong c4 = 0b1010UL;           // 2進数 + ULサフィックス ← 新規対応

        double d1 = 3.14d;             // dサフィックス ← 新規対応
        decimal d2 = 3.14m;            // mサフィックス(decimal) ← 新規対応
        float d3 = 3.14f;              // fサフィックス(既存)

        System.Console.WriteLine(a1 + a2 + a3 + b1 + b2 + b3 + c1 + c2 + c3 + c4 + d1 + (double)d2 + d3);
    }
}
