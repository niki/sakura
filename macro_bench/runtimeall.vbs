' runtimeall.vbs - CVbsToJsTranspilerのランタイム関数(CVbsMacroMgr.cppの
' VBS_RUNTIME_PRELUDE)を一通り確認するためのリファレンス兼マニュアルスクリプト。
' checkall.vbsが構文(Dim/If/For/Sub/Function等)を中心に確認するのに対し、
' こちらは組み込み関数の網羅を目的とする。各関数の呼び出し例と戻り値を
' カーソル位置へ挿入する。
' MsgBox/InputBoxはユーザー操作が必要なため対象外(calendar.vbs参照)。
'
' (このファイルはUTF-8 BOM付きで保存すること。CTextInputStreamがBOM無しを
'  Shift-JISとして読み込むため、BOMが無いと日本語部分が文字化けする)

Dim CRLF
CRLF = vbCrLf

Dim s, csv, parts()
Dim nums(4)
Dim i, total, r

Sub WriteLine(s)
	S_InsText(s & CRLF)
End Sub

Sub WriteLabelValue(label, val)
	Call WriteLine(label & " -> " & CStr(val))
End Sub

Sub WriteHeader(title)
	Call WriteLine("")
	Call WriteLine("=== " & title & " ===")
End Sub

WriteLine("--- VBScript Runtime Functions Check Start ---")

' ==========================================
' 文字列変換
' ==========================================
Call WriteHeader("String Conversion")
Call WriteLabelValue("Chr(65)", Chr(65))
Call WriteLabelValue("Asc(""A"")", Asc("A"))
Call WriteLabelValue("CStr(123.45)", CStr(123.45))
Call WriteLabelValue("StrReverse(""abcde"")", StrReverse("abcde"))
Call WriteLabelValue("Hex(255)", Hex(255))
Call WriteLabelValue("Oct(8)", Oct(8))
' "String"はJSのグローバルと衝突するため__vbsStringRepeatへ読み替えられる
Call WriteLabelValue("String(5, ""*"")", String(5, "*"))

' ==========================================
' 数値変換
' ==========================================
Call WriteHeader("Numeric Conversion")
Call WriteLabelValue("CInt(""42.6"")", CInt("42.6"))
Call WriteLabelValue("CLng(""1000"")", CLng("1000"))
Call WriteLabelValue("CDbl(""3.14"")", CDbl("3.14"))
Call WriteLabelValue("CSng(""2.5"")", CSng("2.5"))
Call WriteLabelValue("CBool(""True"")", CBool("True"))
Call WriteLabelValue("CBool(0)", CBool(0))
Call WriteLabelValue("CByte(300)", CByte(300))
Call WriteLabelValue("CByte(-10)", CByte(-10))

' ==========================================
' 文字列操作
' ==========================================
Call WriteHeader("String Manipulation")
s = "Hello, VBScript!"
Call WriteLabelValue("Len(s)", Len(s))
Call WriteLabelValue("Left(s, 5)", Left(s, 5))
Call WriteLabelValue("Right(s, 6)", Right(s, 6))
Call WriteLabelValue("Mid(s, 8, 9)", Mid(s, 8, 9))
Call WriteLabelValue("InStr(s, ""VBScript"")", InStr(s, "VBScript"))
Call WriteLabelValue("InStr(2, s, ""l"")", InStr(2, s, "l"))
Call WriteLabelValue("InStrRev(s, ""l"")", InStrRev(s, "l"))
Call WriteLabelValue("Replace(s, ""VBScript"", ""World"")", Replace(s, "VBScript", "World"))
Call WriteLabelValue("UCase(s)", UCase(s))
Call WriteLabelValue("LCase(s)", LCase(s))
Call WriteLabelValue("Trim(""  padded  "")", "[" & Trim("  padded  ") & "]")
Call WriteLabelValue("LTrim(""  padded  "")", "[" & LTrim("  padded  ") & "]")
Call WriteLabelValue("RTrim(""  padded  "")", "[" & RTrim("  padded  ") & "]")
Call WriteLabelValue("Space(5) & ""|""", Space(5) & "|")
Call WriteLabelValue("StrComp(""abc"", ""abd"")", StrComp("abc", "abd"))
Call WriteLabelValue("StrComp(""ABC"", ""abc"", 1)", StrComp("ABC", "abc", 1))

' ==========================================
' 配列操作
' ==========================================
Call WriteHeader("Array Functions")
csv = "1,2,3,4,5"
' Split/Joinの戻り値は静的な配列名追跡の対象外のため、添字アクセスしたい
' 場合は"Dim parts()"のように括弧付きで宣言してから代入するのが安全
parts = Split(csv, ",")
Call WriteLabelValue("Split(csv, "","")(3)", parts(3))
Call WriteLabelValue("Join(Array(""a"",""b"",""c""), ""-"")", Join(Array("a", "b", "c"), "-"))
Call WriteLabelValue("LBound(parts)", LBound(parts))
Call WriteLabelValue("UBound(parts)", UBound(parts))

For i = 0 To 4
	nums(i) = i * i
Next
Call WriteLabelValue("UBound(nums) (Dim nums(4))", UBound(nums))

total = 0
For Each i In Array(10, 20, 30)
	total = total + i
Next
Call WriteLabelValue("Sum of Array(10,20,30) via For Each", total)

' ==========================================
' 型判定
' ==========================================
Call WriteHeader("Type Checking")
Call WriteLabelValue("IsEmpty(Empty)", IsEmpty(Empty))
Call WriteLabelValue("IsNull(Null)", IsNull(Null))
Call WriteLabelValue("IsArray(nums)", IsArray(nums))
Call WriteLabelValue("IsArray(s)", IsArray(s))
Call WriteLabelValue("IsNumeric(""123"")", IsNumeric("123"))
Call WriteLabelValue("IsNumeric(""abc"")", IsNumeric("abc"))
Call WriteLabelValue("IsDate(""2024-01-01"")", IsDate("2024-01-01"))
Call WriteLabelValue("IsDate(""abc"")", IsDate("abc"))
Call WriteLabelValue("IsObject(nums)", IsObject(nums))
Call WriteLabelValue("TypeName(123)", TypeName(123))
Call WriteLabelValue("TypeName(1.5)", TypeName(1.5))
Call WriteLabelValue("TypeName(""abc"")", TypeName("abc"))
Call WriteLabelValue("TypeName(True)", TypeName(True))
Call WriteLabelValue("TypeName(nums)", TypeName(nums))
Call WriteLabelValue("TypeName(Null)", TypeName(Null))
Call WriteLabelValue("TypeName(Empty)", TypeName(Empty))

' ==========================================
' 数値関数
' ==========================================
Call WriteHeader("Numeric Functions")
Call WriteLabelValue("Abs(-9.5)", Abs(-9.5))
Call WriteLabelValue("Int(3.9)", Int(3.9))
Call WriteLabelValue("Int(-3.1)", Int(-3.1))
Call WriteLabelValue("Fix(3.9)", Fix(3.9))
Call WriteLabelValue("Fix(-3.9)", Fix(-3.9))
Call WriteLabelValue("Sgn(-42)", Sgn(-42))
Call WriteLabelValue("Sgn(0)", Sgn(0))
Call WriteLabelValue("Sqr(81)", Sqr(81))
Call WriteLabelValue("Exp(1)", Exp(1))
Call WriteLabelValue("Log(Exp(1))", Log(Exp(1)))
Call WriteLabelValue("Sin(0)", Sin(0))
Call WriteLabelValue("Cos(0)", Cos(0))
Call WriteLabelValue("Tan(0)", Tan(0))
Call WriteLabelValue("Atn(1) * 4 (Pi)", Atn(1) * 4)

' ==========================================
' 乱数・時間
' ==========================================
Call WriteHeader("Random / Time")
Randomize
r = Rnd()
Call WriteLabelValue("Rnd() is in [0,1)", (r >= 0 And r < 1))
Call WriteLabelValue("Timer() is numeric", IsNumeric(Timer))

WriteLine("")
WriteLine("--- VBScript Runtime Functions Check End ---")
