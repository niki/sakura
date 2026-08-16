// ============================================================
// Pascal風マクロサンプル (拡張子 .pas)
//
// NKMM_FIX_PASCAL_MACRO(CPasToJsTranspiler経由でQuickJSが実行する)の
// 動作確認用。var宣言・for/while/repeatループ・if/elseと、
// 引数付き/無しの関数呼び出しを一通り含む。
//
// 1〜10の合計をカーソル位置へ挿入し(InsText)、
// 結果をメッセージボックスで表示する(InfoMsg)。
// ============================================================
var
  i, total: Integer;
  msg: string;
begin
  total := 0;
  for i := 1 to 10 do
  begin
    total := total + i;
  end;

  i := 0;
  while i < 3 do
  begin
    i := i + 1;
  end;

  repeat
    i := i - 1;
  until i = 0;

  if total > 0 then
  begin
    msg := 'Sum 1..10 = ' + total;
  end
  else
  begin
    msg := 'unexpected';
  end;

  InsText(msg);
  InfoMsg(msg);
end.
