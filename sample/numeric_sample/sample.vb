' Visual Basic 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL)
Dim a1 As Integer = 123        ' 10進数
Dim a2 As Double = 3.14        ' 浮動小数点

Dim b1 As Integer = &HFF       ' 16進数(&H) ← 新規対応
Dim b2 As Integer = &Hff
Dim b3 As Integer = &O17       ' 8進数(&O) ← 新規対応
Dim b4 As Integer = &B1010     ' 2進数(&B, VB.NET) ← 新規対応

Dim c1 As Integer = 100%       ' 型宣言文字 Integer ← 新規対応
Dim c2 As Long = 100&          ' 型宣言文字 Long ← 新規対応
Dim c3 As Single = 3.14!       ' 型宣言文字 Single ← 新規対応
Dim c4 As Double = 3.14#       ' 型宣言文字 Double ← 新規対応
Dim c5 As Decimal = 3.14@      ' 型宣言文字 Decimal ← 新規対応

Sub Main()
    Console.WriteLine(a1 & a2 & b1 & b2 & b3 & b4 & c1 & c2 & c3 & c4 & c5)
End Sub
