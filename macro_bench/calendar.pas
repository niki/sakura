//ある月のカレンダーを表示するマクロ(calendar.ppa)
//作成者: 堀内悟 作成時期: 2004/02 (2/26 v0.9発表)
//任意の日の曜日判定に下の書籍紹介のアルゴリズムを利用
//参照文献：気賀康夫著「電卓に強くなる」(講談社ブルーバックス)
//有効範囲：1900/03--2100/02

var
  YM: String;        //任意の年月
  Y,M,D: Integer;    //西暦年・月・日
  MDays: Integer;    //月間日数
  Mk: Integer;       //月係数
  preN,N: Real;      //修正(前,後)の数字
  id: Integer;       //曜日判定の元になる数字
  idw:Integer;       //曜日指数(日=0, 月=1,...)
  WD: String;        //曜日('日', '月'...)
  Date: Integer;     //カレンダーの日付
  kari: String;      //汎用変数
  CRLF: String;      // 文字列変数 CRLF を用意
  i, j: Integer;     //ループカウンタ
  SPC,SPCx1,SPCx3:String;  //スペース
  Head: String;      //カレンダーのヘッダ: 年─月
  Head2: String;     //カレンダーのヘッダ: 曜日
  Line_1: String;    //カレンダーの行: 日付第１行
  Line_2: String;    //カレンダーの行: 日付第２行以降
  MSG: String;       //ダイアログメッセージ

// ＜処理の流れ＞
// 1. 月初日の曜日を求める
// 2. 年月と曜日を表示するヘッダ２行分を設定
// 3. 1.の結果に基づいて日付部分の１行目を生成
// 4. 繰り返し命令で２行目以降を生成
// 5. すべての行を結合させてダイアログに表示

begin
  CRLF := '
';
  SPCx1 := ' ';
  SPCx3 := '   ';

//任意の年-月の入力
  YM := InputBox('年月の入力', '例: 200402 有効範囲: 190003-210002', '');
  Y := StrToInt( Copy(YM, 1, 4) );
  M := StrToInt( Copy(YM, 5, 2) );
  D := 1;	//月初日

//月間日数を求める
if M= 1 then MDays := 31;
if M= 3 then MDays := 31;
if M= 5 then MDays := 31;
if M= 7 then MDays := 31;
if M= 8 then MDays := 31;
if M=10 then MDays := 31;
if M=12 then MDays := 31;
if M= 4 then MDays := 30;
if M= 6 then MDays := 30;
if M= 9 then MDays := 30;
if M=11 then MDays := 30;
//うるう年かどうか判定
// 100の倍数は通常年だが、400の倍数ならうるう年
if M=2 then 
begin
  if Y mod 4 <> 0 then MDays := 28 else //else前＝まったき通常年
  begin
    if Y mod 100 = 0 then
        begin
          if Y mod 400 = 0 then MDays := 29 else
          MDays := 28;
        end
    else 
    MDays := 29;
  end
end;

//月係数の代入
if M= 1 then Mk := 5;
if M= 2 then Mk := 8;
if M= 3 then Mk := 8;
if M= 4 then Mk := 4;
if M= 5 then Mk := 6;
if M= 6 then Mk := 9;
if M= 7 then Mk := 4;
if M= 8 then Mk := 7;
if M= 9 then Mk := 3;
if M=10 then Mk := 5;
if M=11 then Mk := 1;
if M=12 then Mk := 3;

  preN := Y/0.8 + Mk + D; //(Y/0.8)だと"右括弧がありません"エラー!
  preN := Trunc(preN);    //小数部を切り捨ててInt64型の値にする
  if M<3 then N := preN-1;
  if M>2 then N := preN;
  N := Frac(N/7);
  kari := FloatToStr(N);
  kari := Copy(kari, 3, 1);
  id := StrToInt(kari);

if id= 1 then WD := '月';
if id= 2 then WD := '火';
if id= 4 then WD := '水';
if id= 5 then WD := '木';
if id= 7 then WD := '金';
if id= 8 then WD := '土';
if id= 0 then WD := '日';

//ヘッダ設定および日付部分を表わす変数の初期化
Head := '       ' + IntToStr(Y) + '-' + IntToStr(M) + '      ';
Head2 := '日 月 火 水 木 金 土';
Line_1 := '';
Line_2 := '';
MSG := '';

//曜日を表わす指数を設定する
//日 月 火 水 木 金 土
// 0  1  2  3  4  5  6 = idw
if WD = '日' then idw := 0;
if WD = '月' then idw := 1;
if WD = '火' then idw := 2;
if WD = '水' then idw := 3;
if WD = '木' then idw := 4;
if WD = '金' then idw := 5;
if WD = '土' then idw := 6;
//月初日の曜日を基に日付部分第１行の左インデント幅を設定
if WD = '日' then Line_1 := '';
if WD = '月' then Line_1 := SPCx3;
if WD = '火' then Line_1 := SPCx3+SPCx3;
if WD = '水' then Line_1 := SPCx3+SPCx3+SPCx3;
if WD = '木' then Line_1 := SPCx3+SPCx3+SPCx3+SPCx3;
if WD = '金' then Line_1 := SPCx3+SPCx3+SPCx3+SPCx3+SPCx3;
if WD = '土' then Line_1 := SPCx3+SPCx3+SPCx3+SPCx3+SPCx3+SPCx3;

//日付１行目 (Line_1) に表示される日付は 1 から (7-idw) まで
i := 0;
while i < (7-idw) do
begin
	i := i + 1;
	Line_1 := Line_1 + ' ' + IntToStr(i)+ ' ';
end;

//このルーチン開始時点で i = １行目最後の日付
Date := i;
while (Date < MDays) do
begin
  for j := 1 to 7 do    //１週間分の日付を並べる
  begin
    if Date = MDays then Continue;
    Date := Date + 1;
    if Date < 10 then SPC := SPCx1 else SPC := '';
    Line_2 := Line_2 + SPC + IntToStr(Date) + SPCx1;
  end;
Line_2 := Line_2 + CRLF;
end;

//ダイアログメッセージ生成
MSG := Head + CRLF + Head2 + CRLF + Line_1 + CRLF + Line_2;
MessageBox(MSG, 'カレンダー', 0);
end
