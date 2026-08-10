<#
.SYNOPSIS
	sakura_keyword\*.kwd から、組み込みキーワード配列の中身(sakura_core\types\generated\*.inc)を再生成する。

.DESCRIPTION
	sakura.keywordset.csv が参照する外部キーワードファイル(実行時はKeyword\配下)が見つからない
	場合のフォールバックとして、対応する各CType_*.cppは、あらかじめ埋め込んだ配列を使う(20260809)。
	その配列の中身は、コンパイル時に丸ごと#includeするだけの .inc ファイルとして
	sakura_core\types\generated\ に置いてあり、sakura_keyword\*.kwd の内容を更新したときは、
	このスクリプトを実行して .inc を再生成し、コミットし直す。

	$Targets の各要素は File(.kwd) か Regex(.rkw) のどちらか一方を持つ。

	[File(.kwd)の場合]
	空行・"//"で始まるコメント行を除いた各行を L"キーワード", の形式で1行ずつ書き出し、
	出力先は generated\<name>_keywords.inc。ダブルクォート・バックスラッシュを含む行や
	MAX_KEYWORDLEN(63文字。CKeyWordSetMgr.h参照)を超える行はエラーにして中断する
	(CKeyWordSetMgr::SetKeyWordArr()はキーワード長のチェックをしないため、
	 埋め込み側で63文字を超えないことを保証する必要がある)。
	消費側は `const wchar_t* g_ppszKeywordsXXX[] = { #include "..." }; ` という
	データ配列の初期化子として#includeする。

	[Regex(.rkw)の場合]
	正規表現キーワード(タイプ別設定「正規表現キーワード」タブ・CImpExpRegex::Import()と同じ
	`RxKey[999]=ColorName,/pattern/flags` 形式)を1行ずつ解析し、
	`RegexAdd( pType, keywordPos, idx++, COLORIDX_XXX, L"/pattern/flags" );` という
	関数呼び出し文の並びとして generated\<name>_regex.inc に書き出す(データ配列ではない)。
	消費側は該当CType_*.cppのInitTypeConfigImp()内、
	`int keywordPos = 0; int idx = 0; pType->m_bUseRegexKeyword = true;` の直後で
	文として#includeする(既存のCType_JavaScript.cppの手書きRegexAdd()呼び出しと同じ形)。
	ColorNameは3文字コード(KW1〜KWA=COLORIDX_KEYWORD1〜10、RK1〜RKA=COLORIDX_REGEX1〜10、
	その他CColorStrategy.cppのg_ColorAttributeArrに準拠)のみ対応(日本語名フォールバックは
	CImpExpRegex::Import()側の機能でスクリプトでは非対応)。

	[Rule(.rule)の場合]
	アウトライン解析ルールファイル(CDocOutline::ReadRuleFile()が読む、
	`key1,key2 /// GroupName,Lv=1`や`;Mode=Regex`等の書式)。この形式は.kwd/.rkwと違い
	1行ずつパースする必要が無い(CDocOutline::ReadRuleFile()自身がテキスト全体をパースする)ため、
	ファイル内容をそのままC++11生raw文字列リテラル`LR"RULEDATA(...)RULEDATA"`として
	generated\<name>_rule.incに書き出すだけ(エスケープ一切不要)。ファイル内容に区切り文字列
	`)RULEDATA"`が含まれる場合はエラーにする。
	消費側はsakura_core\doc\CDocOutline.cpp内で
	`const wchar_t* g_pszOutlineRuleXXX = #include "../types/generated/<name>_rule.inc";`
	として直接埋め込み、ReadRuleFile()が実ファイルを開けなかったときのフォールバックとして使う
	(doc\とtypes\は別ディレクトリなので相対パスに../types/を挟む点に注意)。
	(ファイル名の去掉ディレクトリ部分で対応表引きする。CType_*.cpp側の変更は不要、
	ReadRuleFile()側だけで完結する)。

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

	# 正規表現キーワード(.rkw)。File方式とは別物なのでRegexで指定する。出力は
	# <name>_regex.inc(File側の<name>_keywords.incとは別ファイルなので同じNameを使ってよい) 20260810
	@{ Name = 'CPP';        Regex = 'cpp.rkw' }
	@{ Name = 'PERL';       Regex = 'perl.rkw' }
	@{ Name = 'RUBY';       Regex = 'ruby.rkw' }
	@{ Name = 'JS';         Regex = 'JavaScript.rkw' }
	@{ Name = 'TEXT';       Regex = 'Text.rkw' }
	@{ Name = 'XML';        Regex = 'Xml.rkw' }

	# アウトライン解析ルールファイル(.rule)。出力は<name>_rule.inc 20260811
	@{ Name = 'JS';         Rule = 'JavaScript.rule' }
	@{ Name = 'RUBY';       Rule = 'Ruby.rule' }
	@{ Name = 'PHP';        Rule = 'php.rule' }
)

# ColorName(3文字コード) -> COLORIDX_XXX。CColorStrategy.cppのg_ColorAttributeArrと
# EColorIndexType.hのenum定義を突き合わせた対応表(順序が対応しているため、片方だけ
# 増減すると対応がずれる点に注意)。
$ColorNameMap = @{
	'TXT'='COLORIDX_TEXT'; 'RUL'='COLORIDX_RULER'; 'CAR'='COLORIDX_CARET'; 'IME'='COLORIDX_CARET_IME';
	'CBK'='COLORIDX_CARETLINEBG'; 'UND'='COLORIDX_UNDERLINE'; 'CVL'='COLORIDX_CURSORVLINE'; 'NOT'='COLORIDX_NOTELINE';
	'LNO'='COLORIDX_GYOU'; 'MOD'='COLORIDX_GYOU_MOD'; 'EBK'='COLORIDX_EVENLINEBG'; 'TAB'='COLORIDX_TAB';
	'SPC'='COLORIDX_SPACE'; 'ZEN'='COLORIDX_ZENSPACE'; 'CTL'='COLORIDX_CTRLCODE'; 'EOL'='COLORIDX_EOL';
	'RAP'='COLORIDX_WRAP'; 'VER'='COLORIDX_VERTLINE'; 'EOF'='COLORIDX_EOF'; 'NUM'='COLORIDX_DIGIT';
	'BRC'='COLORIDX_BRACKET_PAIR'; 'SEL'='COLORIDX_SELECT'; 'FND'='COLORIDX_SEARCH'; 'FN2'='COLORIDX_SEARCH2';
	'FN3'='COLORIDX_SEARCH3'; 'FN4'='COLORIDX_SEARCH4'; 'FN5'='COLORIDX_SEARCH5'; 'CMT'='COLORIDX_COMMENT';
	'SQT'='COLORIDX_SSTRING'; 'WQT'='COLORIDX_WSTRING'; 'HDC'='COLORIDX_HEREDOC'; 'URL'='COLORIDX_URL';
	'KW1'='COLORIDX_KEYWORD1'; 'KW2'='COLORIDX_KEYWORD2'; 'KW3'='COLORIDX_KEYWORD3'; 'KW4'='COLORIDX_KEYWORD4';
	'KW5'='COLORIDX_KEYWORD5'; 'KW6'='COLORIDX_KEYWORD6'; 'KW7'='COLORIDX_KEYWORD7'; 'KW8'='COLORIDX_KEYWORD8';
	'KW9'='COLORIDX_KEYWORD9'; 'KWA'='COLORIDX_KEYWORD10';
	'RK1'='COLORIDX_REGEX1'; 'RK2'='COLORIDX_REGEX2'; 'RK3'='COLORIDX_REGEX3'; 'RK4'='COLORIDX_REGEX4';
	'RK5'='COLORIDX_REGEX5'; 'RK6'='COLORIDX_REGEX6'; 'RK7'='COLORIDX_REGEX7'; 'RK8'='COLORIDX_REGEX8';
	'RK9'='COLORIDX_REGEX9'; 'RKA'='COLORIDX_REGEX10';
	'DFA'='COLORIDX_DIFF_APPEND'; 'DFC'='COLORIDX_DIFF_CHANGE'; 'DFD'='COLORIDX_DIFF_DELETE'; 'MRK'='COLORIDX_MARK';
}

$MaxKeywordLen = 63
$MaxRegexKeywordLen = 1000	# CKeyWordSetMgr.hのMAX_KEYWORDLENに対応するconfig/maxdata.hのMAX_REGEX_KEYWORDLEN

foreach ($t in $Targets) {
	$isRegex = $t.ContainsKey('Regex')
	$isRule = $t.ContainsKey('Rule')
	$srcFile = if ($isRegex) { $t.Regex } elseif ($isRule) { $t.Rule } else { $t.File }
	$srcPath = Join-Path $KeywordDir $srcFile

	if (-not (Test-Path $srcPath)) {
		# 20260809 対応するファイルが無いターゲットがあっても生成全体を止めない(1件のギャップで
		# 他の再生成が巻き添えを食わないように)。
		# 空の配列 `T arr[] = {};` はMSVCではC2466(サイズ0の配列は宣言不可)になるため、
		# 生成を丸ごとスキップする(既存の.incがあってもそのまま残す)。#includeで参照する側の
		# コンパイルエラーが「ファイルを用意してこのスクリプトを実行してください」の合図になる。
		Write-Warning "$srcFile が見つかりません。$($t.Name)の生成をスキップします: $srcPath"
		continue
	}

	if ($isRule) {
		# [Rule(.rule)] CDocOutline::ReadRuleFile()が読む書式。1行ずつのパースは不要なので、
		# ファイル内容をそのままraw文字列リテラルとして書き出す。
		$rawText = [System.IO.File]::ReadAllText($srcPath, [System.Text.Encoding]::GetEncoding(932))
		$delim = 'RULEDATA'
		if ($rawText.Contains(")$delim`"")) {
			throw "${srcFile}: raw文字列の区切り ')$delim`"' を含むため埋め込めません(スクリプトの`$delimを変更してください)"
		}
		$outPath = Join-Path $OutDir ($t.Name.ToLower() + '_rule.inc')
		$body = "LR`"$delim($rawText)$delim`""
		[System.IO.File]::WriteAllText($outPath, $body, (New-Object System.Text.UTF8Encoding($true)))

		Write-Host "$($t.Name): $($rawText.Length) chars (rule) -> $outPath"
		continue
	}

	# .kwd/.rkw はShift-JIS想定(コメント行に日本語を含むことがある)。
	$lines = Get-Content -LiteralPath $srcPath -Encoding Default

	if ($isRegex) {
		# [Regex(.rkw)] RxKey[999]=ColorName,/pattern/flags 形式。
		# CImpExpRegex::Import()(CImpExpManager.cpp)と同じ判定条件で解析する。
		$regexLines = New-Object System.Collections.Generic.List[string]
		foreach ($line in $lines) {
			$s = $line.Trim("`r", "`n")
			if ($s.Length -lt 12) { continue }
			if ($s.Substring(0, 6) -ne 'RxKey[') { continue }
			if ($s.Substring(9, 2) -ne ']=') { continue }
			$rest = $s.Substring(11)
			$commaIdx = $rest.IndexOf(',')
			if ($commaIdx -lt 0) { continue }
			$colorName = $rest.Substring(0, $commaIdx)
			$pattern = $rest.Substring($commaIdx + 1)
			if ($pattern.Length -eq 0) { continue }
			if (-not $ColorNameMap.ContainsKey($colorName)) {
				throw "${srcFile}: 未知のColorName '$colorName' です(スクリプト内のColorNameMapに追加してください): '$s'"
			}
			if ($pattern.Length -gt $MaxRegexKeywordLen) {
				throw "${srcFile}: MAX_REGEX_KEYWORDLEN($MaxRegexKeywordLen)を超えるパターンがあります: '$s'"
			}
			if ($pattern -match '[^\x00-\x7F]') {
				throw "${srcFile}: ASCII範囲外の文字を含むパターンがあります: '$s'"
			}
			# C++文字列リテラルとして安全になるようエスケープ(先にバックスラッシュ、次にダブルクォート)。
			$escaped = $pattern.Replace('\', '\\').Replace('"', '\"')
			$regexLines.Add("`tRegexAdd( pType, keywordPos, idx++, $($ColorNameMap[$colorName]), L`"$escaped`" );")
		}

		$outPath = Join-Path $OutDir ($t.Name.ToLower() + '_regex.inc')
		$body = ($regexLines -join "`r`n")
		[System.IO.File]::WriteAllText($outPath, $body + "`r`n", (New-Object System.Text.UTF8Encoding($true)))

		Write-Host "$($t.Name): $($regexLines.Count) regex keywords -> $outPath"
	}
	else {
		# [File(.kwd)] 空行・"//"コメントを除いた各行をL"...",として書き出す。
		# キーワード自体はASCIIのみを許可する(埋め込み配列は他言語コメントを保持する必要が無いため)。
		$keywords = New-Object System.Collections.Generic.List[string]
		foreach ($line in $lines) {
			$s = $line.Trim("`r", "`n")
			if ($s.Length -eq 0) { continue }
			if ($s.StartsWith('//')) { continue }
			if ($s.Length -gt $MaxKeywordLen) {
				throw "${srcFile}: MAX_KEYWORDLEN($MaxKeywordLen)を超える行があります: '$s' ($($s.Length)文字)"
			}
			if ($s -match '[^\x00-\x7F]') {
				throw "${srcFile}: ASCII範囲外の文字を含む行があります: '$s'"
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
}

Write-Host "`n完了。git diff で sakura_core\types\generated\*.inc の差分を確認してからコミットしてください。"
