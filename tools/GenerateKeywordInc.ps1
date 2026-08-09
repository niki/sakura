<#
.SYNOPSIS
	sakura_keyword\*.kwd から、組み込みキーワード配列の中身(sakura_core\types\generated\*.inc)を再生成する。

.DESCRIPTION
	sakura.keywordset.csv が参照する外部キーワードファイル(実行時はKeyword\配下)が見つからない
	場合のフォールバックとして、対応する各CType_*.cppは、あらかじめ埋め込んだ配列を使う(20260809)。
	その配列の中身は、コンパイル時に丸ごと#includeするだけの .inc ファイルとして
	sakura_core\types\generated\ に置いてあり、sakura_keyword\*.kwd の内容を更新したときは、
	このスクリプトを実行して .inc を再生成し、コミットし直す。

	各 .kwd ファイルは空行・"//"で始まるコメント行を除いた各行を
	L"キーワード", の形式で1行ずつ書き出す。ダブルクォート・バックスラッシュを含む行や
	MAX_KEYWORDLEN(63文字。CKeyWordSetMgr.h参照)を超える行はエラーにして中断する
	(CKeyWordSetMgr::SetKeyWordArr()はキーワード長のチェックをしないため、
	 埋め込み側で63文字を超えないことを保証する必要がある)。

	対象はPHP2(php.kwd)を除く全22タイプ・27キーワードセット。PHP2はPHP組み込み関数一覧で
	1万語超と大きく、全キーワードセット共有の格納領域(MAX_KEYWORDNUM=15000)を圧迫するため
	埋め込み対象外(対象一覧に含めない)。

	なお、元々ソースに組み込み配列があった16タイプ(CPP/HTML/PLSQL/COBOL/JAVA/CORBA_IDL/AWK/
	BAT/PASCAL/TEX/TEX2/PERL/PERL2/VB/VB2/RTF)は、現時点の.incが「元の埋め込み配列をそのまま
	.inc化しただけ」で、sakura_keyword\*.kwdの現在の内容とは同期していない可能性がある
	(html5.kwd等は元の配列よりキーワードが大幅に増えている)。このスクリプトを実行すると
	.kwdの現在の内容で上書きされ、TEX等にあった手書きコメント(無効化したキーワードの記録など)
	は失われる。同期する場合は事前にgit diffで変更点を確認すること。

.PARAMETER KeywordDir
	*.kwd が置かれているディレクトリ。省略時はリポジトリ直下の sakura_keyword\
	(sakura_lang\と同階層。実行時にKeyword\として配布されるファイル一式のマスター)。

.EXAMPLE
	.\tools\GenerateKeywordInc.ps1
.EXAMPLE
	.\tools\GenerateKeywordInc.ps1 -KeywordDir 'D:\github.niki\sakura\sakura\Release64\keyword'
#>
[CmdletBinding()]
param(
	[string]$KeywordDir
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$OutDir   = Join-Path $RepoRoot 'sakura_core\types\generated'

if (-not $KeywordDir) {
	$KeywordDir = Join-Path $RepoRoot 'sakura_keyword'
}

if (-not (Test-Path $KeywordDir)) {
	throw "KeywordDir が見つかりません: $KeywordDir"
}
if (-not (Test-Path $OutDir)) {
	New-Item -ItemType Directory -Path $OutDir | Out-Null
}

# アリ名 : (元ファイル, 出力.inc)
# CSV(sakura.keywordset.csv)側の name/filename と対応させること
$Targets = @(
	@{ Name = 'CSS';      File = 'css2.1.kwd' }
	@{ Name = 'JS';       File = 'ecmascript_sys.kwd' }
	@{ Name = 'JS2';      File = 'javascript.kwd' }
	@{ Name = 'PHP';      File = 'php_reserved.kwd' }	# PHP2(php.kwd)は対象外
	@{ Name = 'PYTHON';   File = 'python_2.5.kwd' }
	@{ Name = 'RUBY';     File = 'ruby1.kwd' }
	@{ Name = 'RUBY2';    File = 'ruby2.kwd' }
	@{ Name = 'RUBY3';    File = 'ruby3.kwd' }
	@{ Name = 'RUBY4';    File = 'ruby4.kwd' }
	@{ Name = 'CSHARP';   File = 'csharp.kwd' }
	@{ Name = 'CSHARP2';  File = 'csharp-context.kwd' }

	# 元々ソースに組み込み配列があった16タイプ 20260809
	# 注意: これらの.incは現時点では「元の埋め込み配列をそのまま.inc化しただけ」で、
	# 現在のKeyword\*.kwdの内容とは同期していない(html5.kwd等は元の配列より
	# キーワードが大幅に増えている)。このスクリプトを実行すると.kwdの現在の内容で
	# 上書きされ、TEX等にあった手書きコメント(無効化したキーワードの記録など)は
	# 失われる。同期する場合は事前にgit diffで変更点を確認すること。
	@{ Name = 'CPP';        File = 'cpp.kwd' }
	@{ Name = 'HTML';       File = 'html5.kwd' }
	@{ Name = 'PLSQL';      File = 'plsql.kwd' }
	@{ Name = 'COBOL';      File = 'COBOL.kwd' }
	@{ Name = 'JAVA';       File = 'java.kwd' }
	@{ Name = 'CORBA_IDL';  File = 'corba.kwd' }
	@{ Name = 'AWK';        File = 'awk.kwd' }
	@{ Name = 'BAT';        File = 'batch.kwd' }
	@{ Name = 'PASCAL';     File = 'pascal.kwd' }
	@{ Name = 'TEX';        File = 'tex1.kwd' }
	@{ Name = 'TEX2';       File = 'tex2.kwd' }
	@{ Name = 'PERL';       File = 'perl.kwd' }
	@{ Name = 'PERL2';      File = 'perlvar.kwd' }
	@{ Name = 'VB';         File = 'vb.kwd' }
	@{ Name = 'VB2';        File = 'vb2.kwd' }
	@{ Name = 'RTF';        File = 'rtf.kwd' }
)

$MaxKeywordLen = 63

foreach ($t in $Targets) {
	$srcPath = Join-Path $KeywordDir $t.File
	$keywords = New-Object System.Collections.Generic.List[string]

	if (-not (Test-Path $srcPath)) {
		# 20260809 対応する.kwdが無いターゲットがあっても生成全体を止めない(1件のギャップで
		# 残り26件の再生成が巻き添えを食わないように)。
		# 空の配列 `T arr[] = {};` はMSVCではC2466(サイズ0の配列は宣言不可)になるため、
		# 生成を丸ごとスキップする(既存の.incがあってもそのまま残す)。#includeで参照する側の
		# コンパイルエラーが「.kwdを用意してこのスクリプトを実行してください」の合図になる。
		Write-Warning "$($t.File) が見つかりません。$($t.Name)の生成をスキップします: $srcPath"
		continue
	}

	# .kwd はShift-JIS想定(コメント行に日本語を含むことがある)。
	# キーワード自体はASCIIのみを許可する(埋め込み配列は他言語コメントを保持する必要が無いため)。
	$lines = Get-Content -LiteralPath $srcPath -Encoding Default

	foreach ($line in $lines) {
		$s = $line.Trim("`r", "`n")
		if ($s.Length -eq 0) { continue }
		if ($s.StartsWith('//')) { continue }
		if ($s.Length -gt $MaxKeywordLen) {
			throw "$($t.File): MAX_KEYWORDLEN($MaxKeywordLen)を超える行があります: '$s' ($($s.Length)文字)"
		}
		if ($s -match '[^\x00-\x7F]') {
			throw "$($t.File): ASCII範囲外の文字を含む行があります: '$s'"
		}
		# C++文字列リテラルとして安全になるようエスケープ(TeXの\AAやRTF制御語の\ansi等、
		# バックスラッシュを含むキーワードがあるため単純拒否ではなくエスケープする)。
		# 先にバックスラッシュ、次にダブルクォートの順でエスケープすること。
		$escaped = $s.Replace('\', '\\').Replace('"', '\"')
		$keywords.Add($escaped)
	}

	$outPath = Join-Path $OutDir ($t.Name.ToLower() + '_keywords.inc')
	$body = ($keywords | ForEach-Object { "`tL`"$_`"," }) -join "`r`n"
	# generated/*.inc はCRLFで統一(このリポジトリの大半のソースがCRLFのため)
	[System.IO.File]::WriteAllText($outPath, $body + "`r`n", (New-Object System.Text.UTF8Encoding($true)))

	Write-Host "$($t.Name): $($keywords.Count) keywords -> $outPath"
}

Write-Host "`n完了。git diff で sakura_core\types\generated\*.inc の差分を確認してからコミットしてください。"
