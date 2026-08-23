' checkall.vbs - CVbsToJsTranspilerの動作確認用サンプル
' サクラエディタのキーボードマクロとして実行すると、対応構文と組み込み関数を
' ひととおり検証し、結果をカーソル位置へテキストとして挿入する。
' (このファイルはUTF-8 BOM付きで保存すること。CTextInputStreamがBOM無しを
'  Shift-JISとして読み込むため、BOMが無いと日本語部分が文字化けする)

Option Explicit

Const APP_NAME = "VBS Transpiler Check"
Const MIN_SCORE = 0, MAX_SCORE = 100

Dim CRLF
CRLF = vbCrLf

' On Error Resume Next はこのエンジンでは無視されるだけ(実際のエラー握り
' つぶし動作までは再現しない)。構文として受理されることの確認を兼ねる。
On Error Resume Next

' ==========================================
' 出力用ヘルパー (Subの定義・呼び出しの確認)
' ==========================================
Sub WriteLine(s)
	S_InsText(s & CRLF)
End Sub

Sub WriteLabelValue(label, val)
	Call WriteLine(label & " -> " & CStr(val))
End Sub

' ==========================================
' Functionの確認 (再帰呼び出し、Exit Function、戻り値の代入)
' ==========================================
Function Factorial(n)
	If n <= 1 Then
		Factorial = 1
		Exit Function
	End If
	Factorial = n * Factorial(n - 1)
End Function

Function Max2(a, b)
	If a > b Then
		Max2 = a
	Else
		Max2 = b
	End If
End Function

' ==========================================
' メイン処理
' ==========================================
Dim i, j
Dim total
Dim s1
Dim score
Dim arr(4)
Dim grid(1, 2)
Dim list
Dim sample

WriteLine("--- " & APP_NAME & " Start ---")
WriteLine("")

' --- If / ElseIf / Else (ブロック形式) ---
score = 72
If score >= 90 Then
	WriteLine("Grade: A")
ElseIf score >= 70 Then
	WriteLine("Grade: B")
Else
	WriteLine("Grade: C")
End If

' --- If (単一行形式) ---
If score >= MIN_SCORE And score <= MAX_SCORE Then WriteLine("Score in range") Else WriteLine("Score out of range")

' --- For ... Next (Stepの確認) ---
total = 0
For i = 1 To 10 Step 2
	total = total + i
Next
Call WriteLabelValue("Sum of 1,3,5,7,9 (For Step 2)", total)

' --- Do While ... Loop (前置While) ---
i = 1 : total = 0
Do While i <= 5
	total = total + i
	i = i + 1
Loop
Call WriteLabelValue("Sum 1..5 (Do While)", total)

' --- Do ... Loop Until (後置Until) ---
i = 1 : total = 0
Do
	total = total + i
	i = i + 1
Loop Until i > 5
Call WriteLabelValue("Sum 1..5 (Do Loop Until)", total)

' --- While ... Wend ---
i = 1 : total = 0
While i <= 5
	total = total + i
	i = i + 1
Wend
Call WriteLabelValue("Sum 1..5 (While Wend)", total)

' --- 行継続(_)の確認 ---
total = 1 + 2 + _
        3 + 4
Call WriteLabelValue("Line continuation (1+2+3+4)", total)

Rem Remによるコメントの確認(ここは実行結果に影響しない)

' --- 1次元配列 (Dim / For Each) ---
For i = 0 To 4
	arr(i) = i * i
Next
s1 = ""
For Each j In arr
	s1 = s1 & j & " "
Next
Call WriteLabelValue("Array squares (For Each)", s1)

' --- 2次元配列 ---
For i = 0 To 1
	For j = 0 To 2
		grid(i, j) = i * 10 + j
	Next
Next
Call WriteLabelValue("grid(1,2)", grid(1, 2))

' --- ReDim / ReDim Preserve ---
ReDim list(2)
list(0) = "a"
list(1) = "b"
list(2) = "c"
ReDim Preserve list(4)
list(3) = "d"
list(4) = "e"
s1 = ""
For i = 0 To 4
	s1 = s1 & list(i)
Next
Call WriteLabelValue("ReDim Preserve", s1)

' --- 演算子の確認 ---
Call WriteLabelValue("7 \ 2 (整数除算)", 7 \ 2)
Call WriteLabelValue("7 Mod 2", 7 Mod 2)
Call WriteLabelValue("2 ^ 10 (べき乗)", 2 ^ 10)
Call WriteLabelValue("-2 ^ 2 (単項より優先順位が低い)", -2 ^ 2)
Call WriteLabelValue("True And False", True And False)
Call WriteLabelValue("True Or False", True Or False)
Call WriteLabelValue("True Xor True", True Xor True)
Call WriteLabelValue("Not True", Not True)
Call WriteLabelValue("Nothing Is Nothing", Nothing Is Nothing)
Call WriteLabelValue("IsEmpty(Empty)", IsEmpty(Empty))
Call WriteLabelValue("IsNull(Null)", IsNull(Null))

' --- Sub/Function呼び出し ---
Call WriteLabelValue("Factorial(5)", Factorial(5))
Call WriteLabelValue("Max2(3, 9)", Max2(3, 9))

' --- 文字列関数 ---
sample = "  Hello, VBScript World!  "
Call WriteLabelValue("Len(sample)", Len(sample))
Call WriteLabelValue("Trim(sample)", Trim(sample))
Call WriteLabelValue("UCase(Trim(sample))", UCase(Trim(sample)))
Call WriteLabelValue("LCase(Trim(sample))", LCase(Trim(sample)))
Call WriteLabelValue("Left(Trim(sample), 5)", Left(Trim(sample), 5))
Call WriteLabelValue("Right(Trim(sample), 6)", Right(Trim(sample), 6))
Call WriteLabelValue("Mid(Trim(sample), 8, 9)", Mid(Trim(sample), 8, 9))
Call WriteLabelValue("InStr(sample, ""VBScript"")", InStr(sample, "VBScript"))
Call WriteLabelValue("Replace(Trim(sample), ""VBScript"", ""VBS"")", Replace(Trim(sample), "VBScript", "VBS"))
Call WriteLabelValue("Chr(65) & Chr(66) & Chr(67)", Chr(65) & Chr(66) & Chr(67))
Call WriteLabelValue("Asc(""A"")", Asc("A"))

' --- 型変換・型判定関数 ---
Call WriteLabelValue("CStr(123)", CStr(123))
Call WriteLabelValue("CInt(""42"")", CInt("42"))
Call WriteLabelValue("CBool(""True"")", CBool("True"))
Call WriteLabelValue("IsNumeric(""3.14"")", IsNumeric("3.14"))
Call WriteLabelValue("IsNumeric(""abc"")", IsNumeric("abc"))
Call WriteLabelValue("TypeName(123)", TypeName(123))
Call WriteLabelValue("TypeName(""abc"")", TypeName("abc"))
Call WriteLabelValue("IsArray(arr)", IsArray(arr))

' --- 数値関数 ---
Call WriteLabelValue("Abs(-5)", Abs(-5))
Call WriteLabelValue("Int(3.7)", Int(3.7))
Call WriteLabelValue("Fix(-3.7)", Fix(-3.7))
Call WriteLabelValue("Sgn(-9)", Sgn(-9))
Call WriteLabelValue("Sqr(16)", Sqr(16))

WriteLine("")
WriteLine("--- " & APP_NAME & " End ---")
