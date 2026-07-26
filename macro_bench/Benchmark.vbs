' ============================================================
' VBScript版ベンチマークマクロ (WSHエンジン, 拡張子 .vbs)
'
' Benchmark.js / Benchmark.vbs / Benchmark.qjs は同じ処理内容
' (算術ループ・文字列連結・バブルソート)を行い、実行時間を
' InfoMsg で表示する。3エンジンの実行速度を比較するためのもの。
' ============================================================

Dim ARITH_N, STR_N, SORT_N
ARITH_N = 2000000	' 算術ループの回数
STR_N   = 20000		' 文字列連結の回数
SORT_N  = 2000		' ソート対象配列の要素数

' 算術演算ループ
Function BenchArith()
	Dim total, i, x
	total = 0
	For i = 1 To ARITH_N
		x = i * 3 - 7
		If (x Mod 2) = 0 Then
			total = total + x
		Else
			total = total - x
		End If
	Next
	BenchArith = total
End Function

' 文字列連結
Function BenchString()
	Dim s, j
	s = ""
	For j = 1 To STR_N
		s = s & "x"
	Next
	BenchString = Len(s)
End Function

' バブルソート(降順配列を昇順に)
Function BenchSort()
	Dim arr(), k, a, b, tmp
	ReDim arr(SORT_N - 1)
	For k = 0 To SORT_N - 1
		arr(k) = SORT_N - k
	Next
	For a = 0 To SORT_N - 2
		For b = 0 To SORT_N - 2 - a
			If arr(b) > arr(b + 1) Then
				tmp = arr(b)
				arr(b) = arr(b + 1)
				arr(b + 1) = tmp
			End If
		Next
	Next
	BenchSort = arr(0)
End Function

Dim t0, t1, t2, t3
Dim rArith, rString, rSort

t0 = Timer
rArith = BenchArith()
t1 = Timer
rString = BenchString()
t2 = Timer
rSort = BenchSort()
t3 = Timer

Dim msg
msg = "[VBScript]" & vbCrLf & _
	"Arith : " & Int((t1 - t0) * 1000) & " ms (result=" & rArith & ")" & vbCrLf & _
	"String: " & Int((t2 - t1) * 1000) & " ms (len=" & rString & ")" & vbCrLf & _
	"Sort  : " & Int((t3 - t2) * 1000) & " ms (arr(0)=" & rSort & ")" & vbCrLf & _
	"Total : " & Int((t3 - t0) * 1000) & " ms"

InfoMsg msg
