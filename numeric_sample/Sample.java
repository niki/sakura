// Java 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
public class Sample {
    public static void main(String[] args) {
        int a1 = 123;               // 10進数
        int a2 = 0xFF;               // 16進数
        double a3 = 3.14;            // 浮動小数点

        int b1 = 0b1010;             // 2進数リテラル(Java 7+) ← 新規対応
        int b2 = 0B1111_0000;        // 2進数 + 桁区切り記号
        long b3 = 1_000_000;         // 桁区切り記号(10進) ← 新規対応
        long b4 = 0xFF_FF_FF_FFL;    // 桁区切り記号(16進) + Lサフィックス

        long c1 = 100L;              // Lサフィックス(既存)
        long c2 = 0b1010L;           // 2進数の後にLサフィックス ← 新規対応
        double c3 = 3.14D;           // Dサフィックス ← 新規対応
        double c4 = 3.14d;           // dサフィックス

        System.out.println(a1 + a2 + a3 + b1 + b2 + b3 + b4 + c1 + c2 + c3 + c4);
    }
}
