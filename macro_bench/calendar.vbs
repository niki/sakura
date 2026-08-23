' ある月のカレンダーを表示するマクロ(calendar.vbs)
' macro_bench/calendar.pas (Pascal風マクロ版)をCVbsToJsTranspilerの動作確認用に
' VBScript風へ移植したもの。アルゴリズム・変数構成は元のPascal版を忠実に踏襲する。
'
' 作成者: 堀内悟 作成時期: 2004/02 (2/26 v0.9発表、原典はPPAマクロ)
' 任意の日の曜日判定に下の書籍紹介のアルゴリズムを利用
' 参照文献：気賀康夫著「電卓に強くなる」(講談社ブルーバックス)
' 有効範囲：1900/03--2100/02
'
' (このファイルはUTF-8 BOM付きで保存すること。CTextInputStreamがBOM無しを
'  Shift-JISとして読み込むため、BOMが無いと日本語部分が文字化けする)

Dim YM          ' 任意の年月
Dim Y, M, D     ' 西暦年・月・日
Dim MDays       ' 月間日数
Dim Mk          ' 月係数
Dim preN, N     ' 修正(前,後)の数字
Dim id          ' 曜日判定の元になる数字
Dim idw         ' 曜日指数(日=0, 月=1,...)
Dim WD          ' 曜日('日', '月'...)
Dim Date        ' カレンダーの日付
Dim kari        ' 汎用変数
Dim CRLF        ' 文字列変数 CRLF を用意
Dim i, j        ' ループカウンタ
Dim SPC, SPCx1, SPCx3  ' スペース
Dim Head        ' カレンダーのヘッダ: 年─月
Dim Head2       ' カレンダーのヘッダ: 曜日
Dim Line_1      ' カレンダーの行: 日付第１行
Dim Line_2      ' カレンダーの行: 日付第２行以降
Dim MSG         ' ダイアログメッセージ

' ＜処理の流れ＞
' 1. 月初日の曜日を求める
' 2. 年月と曜日を表示するヘッダ２行分を設定
' 3. 1.の結果に基づいて日付部分の１行目を生成
' 4. 繰り返し命令で２行目以降を生成
' 5. すべての行を結合させてダイアログに表示

CRLF = vbCrLf
SPCx1 = " "
SPCx3 = "   "

' 任意の年-月の入力(VBScriptのInputBoxは Prompt, Title, Default の順)
YM = InputBox("例: 200402 有効範囲: 190003-210002", "年月の入力", "")
Y = CInt(Mid(YM, 1, 4))
M = CInt(Mid(YM, 5, 2))
D = 1   ' 月初日

' 月間日数を求める
If M = 1 Then MDays = 31
If M = 3 Then MDays = 31
If M = 5 Then MDays = 31
If M = 7 Then MDays = 31
If M = 8 Then MDays = 31
If M = 10 Then MDays = 31
If M = 12 Then MDays = 31
If M = 4 Then MDays = 30
If M = 6 Then MDays = 30
If M = 9 Then MDays = 30
If M = 11 Then MDays = 30

' うるう年かどうか判定
' 100の倍数は通常年だが、400の倍数ならうるう年
If M = 2 Then
	If Y Mod 4 <> 0 Then
		MDays = 28   ' まったき通常年
	ElseIf Y Mod 100 = 0 Then
		If Y Mod 400 = 0 Then
			MDays = 29
		Else
			MDays = 28
		End If
	Else
		MDays = 29
	End If
End If

' 月係数の代入
If M = 1 Then Mk = 5
If M = 2 Then Mk = 8
If M = 3 Then Mk = 8
If M = 4 Then Mk = 4
If M = 5 Then Mk = 6
If M = 6 Then Mk = 9
If M = 7 Then Mk = 4
If M = 8 Then Mk = 7
If M = 9 Then Mk = 3
If M = 10 Then Mk = 5
If M = 11 Then Mk = 1
If M = 12 Then Mk = 3

preN = Y / 0.8 + Mk + D
preN = Int(preN)            ' 小数部を切り捨てる
If M < 3 Then N = preN - 1
If M > 2 Then N = preN
N = (N / 7) - Int(N / 7)    ' Frac(N/7)相当(N/7は常に非負のためInt=切り捨てでよい)
kari = CStr(N)
kari = Mid(kari, 3, 1)
id = CInt(kari)

If id = 1 Then WD = "月"
If id = 2 Then WD = "火"
If id = 4 Then WD = "水"
If id = 5 Then WD = "木"
If id = 7 Then WD = "金"
If id = 8 Then WD = "土"
If id = 0 Then WD = "日"

' ヘッダ設定および日付部分を表わす変数の初期化
Head = "       " & CStr(Y) & "-" & CStr(M) & "      "
Head2 = "日 月 火 水 木 金 土"
Line_1 = ""
Line_2 = ""
MSG = ""

' 曜日を表わす指数を設定する
' 日 月 火 水 木 金 土
'  0  1  2  3  4  5  6 = idw
If WD = "日" Then idw = 0
If WD = "月" Then idw = 1
If WD = "火" Then idw = 2
If WD = "水" Then idw = 3
If WD = "木" Then idw = 4
If WD = "金" Then idw = 5
If WD = "土" Then idw = 6

' 月初日の曜日を基に日付部分第１行の左インデント幅を設定
If WD = "日" Then Line_1 = ""
If WD = "月" Then Line_1 = SPCx3
If WD = "火" Then Line_1 = SPCx3 & SPCx3
If WD = "水" Then Line_1 = SPCx3 & SPCx3 & SPCx3
If WD = "木" Then Line_1 = SPCx3 & SPCx3 & SPCx3 & SPCx3
If WD = "金" Then Line_1 = SPCx3 & SPCx3 & SPCx3 & SPCx3 & SPCx3
If WD = "土" Then Line_1 = SPCx3 & SPCx3 & SPCx3 & SPCx3 & SPCx3 & SPCx3

' 日付１行目 (Line_1) に表示される日付は 1 から (7-idw) まで
i = 0
Do While i < (7 - idw)
	i = i + 1
	Line_1 = Line_1 & " " & CStr(i) & " "
Loop

' このルーチン開始時点で i = １行目最後の日付
Date = i
Do While Date < MDays
	For j = 1 To 7   ' １週間分の日付を並べる
		' VBScriptにはContinueが無いため、Date=MDaysのときは
		' 本体をスキップする形(条件反転)で元のContinueを表現する
		If Date <> MDays Then
			Date = Date + 1
			If Date < 10 Then
				SPC = SPCx1
			Else
				SPC = ""
			End If
			Line_2 = Line_2 & SPC & CStr(Date) & SPCx1
		End If
	Next
	Line_2 = Line_2 & CRLF
Loop

' ダイアログメッセージ生成
MSG = Head & CRLF & Head2 & CRLF & Line_1 & CRLF & Line_2
Call MsgBox(MSG, 0, "カレンダー")
