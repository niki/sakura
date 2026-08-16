program PPALangCheck;

var
  testStr : String;
  subStr  : String;
  strLen  : Integer;
  foundPos: Integer;

begin
  // ==========================================
  // 1. 出力関数のテスト (Write / Writeln)
  // ==========================================
  Writeln('--- PPA Runtime Functions Check Start ---');
  Write('This is Write (No newline). ');
  Writeln('This is Writeln (With newline).');
  Writeln('');

  // ==========================================
  // 2. 文字列操作関数のテスト (Length / Copy / Pos)
  // ==========================================
  testStr := 'Hello, Poor-Pascal!';
  Writeln('Target String: ' + testStr);

  // Length: 文字列のバイト数を取得
  strLen := Length(testStr);
  Writeln('1. Length Test -> ' + testStr + ' Length is:');
  Writeln(strLen);

  // Copy: 文字列の一部を切り出し (1から始まる位置, 文字数)
  // 'Poor-Pascal' を抽出
  subStr := Copy(testStr, 8, 11);
  Writeln('2. Copy Test (Index 8, Len 11) -> ' + subStr);

  // Pos: 部分文字列の登場位置を検索 (見つかると1以上の整数, ないと0)
  foundPos := Pos('Pascal', testStr);
  Writeln('3. Pos Test ("Pascal") -> Found at position:');
  Writeln(foundPos);
  
  foundPos := Pos('Delphi', testStr);
  Writeln('4. Pos Test ("Delphi" not exist) -> Found at position:');
  Writeln(foundPos);
  Writeln('');

  // ==========================================
  // 3. 特殊エラー変数 (UserErrorMes) のテスト
  // ==========================================
  // ※以下のコメントアウトを解除して実行すると、
  //   PPAマクロがダイアログを出して異常終了する挙動を確認できます。
  
  // UserErrorMes := 'PPAの全関数チェック中に意図的なエラーが発生しました。';
  
  Writeln('--- PPA Runtime Functions Check End ---');
end.
