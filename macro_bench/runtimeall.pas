// runtimeall.pas - CPasToJsTranspilerのランタイム関数(CPasMacroMgr.cppの
// PAS_RUNTIME_PRELUDE)を一通り確認するためのリファレンス兼マニュアルスクリプト。
// checkall.pasが主にWrite/Writeln/Length/Copy/Posを確認するのに対し、
// こちらはStrToInt/IntToStr/Trunc/Frac/FloatToStrに加え、UpperCase/LowerCase/
// Trim系/StringReplace/CompareStr/Round/Sqr/Sqrt/Odd/Chr/Ord/Randomまで、
// PAS_RUNTIME_PRELUDEの全関数を網羅する。
// InputBox/MessageBoxはユーザー操作が必要なため対象外(calendar.pas参照)。
program RuntimeAllCheck;

var
  s: String;
  n: Integer;
  x: Real;

begin
  Writeln('--- Pascal Runtime Functions Check Start ---');
  Writeln('');

  // ==========================================
  // 文字列 <-> 数値変換 (StrToInt / IntToStr / FloatToStr)
  // ==========================================
  n := StrToInt('42');
  Writeln('StrToInt(''42'') -> ');
  Writeln(n);

  s := IntToStr(12345);
  Writeln('IntToStr(12345) -> ' + s);

  s := FloatToStr(3.14);
  Writeln('FloatToStr(3.14) -> ' + s);

  Writeln('');

  // ==========================================
  // 数値の切り捨て・小数部 (Trunc / Frac)
  // ==========================================
  x := 7.8;
  n := Trunc(x);
  Writeln('Trunc(7.8) -> ');
  Writeln(n);

  x := Frac(7.8);
  s := FloatToStr(x);
  Writeln('Frac(7.8) -> ' + s);

  Writeln('');

  // ==========================================
  // 文字列操作 (Length / Copy / Pos)
  // ==========================================
  s := 'Hello, Pascal World!';
  Writeln('Target string: ' + s);

  Writeln('Length(s) -> ');
  Writeln(Length(s));

  s := Copy('Hello, Pascal World!', 8, 6);
  Writeln('Copy(s, 8, 6) -> ' + s);

  n := Pos('Pascal', 'Hello, Pascal World!');
  Writeln('Pos(''Pascal'', s) -> ');
  Writeln(n);

  n := Pos('Delphi', 'Hello, Pascal World!');
  Writeln('Pos(''Delphi'', s) (not found) -> ');
  Writeln(n);

  Writeln('');

  // ==========================================
  // 文字列操作(大文字小文字・トリム・置換・比較)
  // ==========================================
  s := UpperCase('Hello Pascal');
  Writeln('UpperCase(''Hello Pascal'') -> ' + s);

  s := LowerCase('Hello Pascal');
  Writeln('LowerCase(''Hello Pascal'') -> ' + s);

  s := Trim('   padded   ');
  Writeln('Trim(''   padded   '') -> [' + s + ']');

  s := TrimLeft('   padded   ');
  Writeln('TrimLeft(''   padded   '') -> [' + s + ']');

  s := TrimRight('   padded   ');
  Writeln('TrimRight(''   padded   '') -> [' + s + ']');

  s := StringReplace('Hello, Pascal World!', 'Pascal', 'Delphi');
  Writeln('StringReplace(s, ''Pascal'', ''Delphi'') -> ' + s);

  n := CompareStr('abc', 'abd');
  Writeln('CompareStr(''abc'', ''abd'') -> ');
  Writeln(n);

  Writeln('');

  // ==========================================
  // 数値関数 (Round / Sqr / Sqrt / Odd / Chr / Ord / Random)
  // ==========================================
  n := Round(3.6);
  Writeln('Round(3.6) -> ');
  Writeln(n);

  n := Sqr(5);
  Writeln('Sqr(5) (2乗) -> ');
  Writeln(n);

  x := Sqrt(81);
  s := FloatToStr(x);
  Writeln('Sqrt(81) (平方根) -> ' + s);

  Writeln('Odd(7) -> ');
  Writeln(Odd(7));

  s := Chr(65);
  Writeln('Chr(65) -> ' + s);

  n := Ord('A');
  Writeln('Ord(''A'') -> ');
  Writeln(n);

  Randomize;
  n := Random(100);
  Writeln('Random(100) is in [0,100) -> ');
  Writeln(n);

  Writeln('');

  // ==========================================
  // 出力関数 (Write / Writeln、改行の有無)
  // ==========================================
  Write('Write (改行なし). ');
  Writeln('Writeln (改行あり).');

  Writeln('');
  Writeln('--- Pascal Runtime Functions Check End ---');
end.
