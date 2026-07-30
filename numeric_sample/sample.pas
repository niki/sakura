{ Pascal 数値リテラル サンプル (NKMM_FIX_NUMERIC_LANG_LITERAL) }
program Sample;
var
  a1: Integer;
  a2: Double;
  b1, b2, b3: Integer;
  c1: LongInt;
begin
  a1 := 123;              { 10進数 }
  a2 := 3.14;              { 浮動小数点 }

  b1 := $FF;               { 16進数($) ← 新規対応 }
  b2 := &17;                { 8進数(&, FreePascal拡張) ← 新規対応 }
  b3 := %1010;              { 2進数(%, FreePascal拡張) ← 新規対応 }

  c1 := 1_000_000;         { 桁区切り記号(FreePascal拡張) ← 新規対応 }

  WriteLn(a1, a2, b1, b2, b3, c1);
end.
