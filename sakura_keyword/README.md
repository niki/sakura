# sakura_keyword

強調キーワードファイル(`*.kwd`)のマスターです。`sakura_lang/` と同じ階層に置いています。

## これは何か

`sakura.keywordset.csv` が参照するキーワードセットのうち、実装済みタイプ分については
`Keyword\*.kwd` が実行時に見つからなくても強調表示できるよう、対応するキーワード配列を
`sakura_core/types/CType_*.cpp` に `#include` の形でソースへ組み込んでいます(20260809)。

このフォルダの `*.kwd` は、その組み込み配列の**生成元**です。

- 実行時に配布・参照される `Keyword\*.kwd`(`Publish\keyword_pack*.zip` 等で配布、exe と同じ階層に
  展開して使う。git管理外)とは別物です。こちらは**ビルド時にソースへ変換するための素材**として
  git管理します。
- 中身は同じ形式の `.kwd` ファイルです。内容を更新したい場合は、このフォルダのファイルを直接編集するか、
  最新の配布用 `Keyword\` から該当ファイルを上書きしてください。

## 対象ファイル一覧(27ファイル、PHP2を除く)

| ファイル | セット名 | 対応する CType_*.cpp |
|---|---|---|
| cpp.kwd | C/C++ | CType_Cpp.cpp |
| html5.kwd | HTML | CType_Html.cpp |
| plsql.kwd | PL/SQL | CType_Sql.cpp |
| COBOL.kwd | COBOL | CType_Cobol.cpp |
| java.kwd | Java | CType_Java.cpp |
| corba.kwd | CORBA IDL | CType_CorbaIdl.cpp |
| awk.kwd | AWK | CType_Awk.cpp |
| batch.kwd | MS-DOS batch | CType_Dos.cpp |
| pascal.kwd | Pascal | CType_Pascal.cpp |
| tex1.kwd | TeX | CType_Tex.cpp |
| tex2.kwd | TeX2 | CType_Tex.cpp |
| perl.kwd | Perl | CType_Perl.cpp |
| perlvar.kwd | Perl2 | CType_Perl.cpp |
| vb.kwd | Visual Basic | CType_Vb.cpp |
| vb2.kwd | Visual Basic2 | CType_Vb.cpp |
| rtf.kwd | Rich Text | CType_Rich.cpp |
| css2.1.kwd | CSS | CType_Css.cpp |
| ecmascript_sys.kwd | JavaScript | CType_JavaScript.cpp |
| javascript.kwd | JavaScript2 | CType_JavaScript.cpp |
| php_reserved.kwd | PHP | CType_Php.cpp |
| python_2.5.kwd | python | CType_Python.cpp |
| ruby1.kwd | Ruby1 | CType_Ruby.cpp |
| ruby2.kwd | Ruby2 | CType_Ruby.cpp |
| ruby3.kwd | Ruby3 | CType_Ruby.cpp |
| ruby4.kwd | Ruby4 | CType_Ruby.cpp |
| csharp.kwd | C# | CType_Csharp.cpp |
| csharp-context.kwd | C# content | CType_Csharp.cpp |

**PHP2(`php.kwd`)は対象外**です。PHP組み込み関数一覧で1万語超と大きく、全キーワードセット共有の
格納領域(`CKeyWordSetMgr.h` の `MAX_KEYWORDNUM=15000`)を圧迫するため埋め込みを見送っています。
実行時に `Keyword\php.kwd` が無い場合、PHP2セットは空のままです。

## 正規表現キーワード(`*.rkw`)

`*.kwd`(強調キーワード、`CKeyWordSetMgr`経由)とは別に、`*.rkw`(正規表現キーワード、
`STypeConfig::m_RegexKeywordArr`/`m_RegexKeywordList`経由)も同じ`sakura_keyword\`・同じ
`tools\GenerateKeywordInc.ps1`で扱う(20260810)。ただし消費のされ方が異なる点に注意:

- `.kwd`は`sakura.keywordset.csv`経由で**起動のたびに自動読み込み**され、ファイルが無ければ
  組み込み配列にフォールバックする(詳細は「読み込み優先順位」章参照)。
- `.rkw`はタイプ別設定「正規表現キーワード」タブの**手動インポートでのみ**読み込まれる仕組みで、
  起動時の自動読み込み・フォールバックの機構自体が存在しない。そのため`.rkw`の組み込みは、
  該当`CType_*.cpp`の`InitTypeConfigImp()`に直接`RegexAdd()`呼び出し列を`#include`する形を取る
  (既存のJavaScriptタイプの手書き`RegexAdd()`と同じやり方を自動生成しているだけで、実行時の
  フォールバック判定は無い)。

| ファイル | 対応する CType_*.cpp | 生成物 |
|---|---|---|
| cpp.rkw | CType_Cpp.cpp | generated/cpp_regex.inc |
| perl.rkw | CType_Perl.cpp | generated/perl_regex.inc |
| Ruby.rkw | CType_Ruby.cpp | generated/ruby_regex.inc |

**Ruby.rkwは注意**: CType_Ruby.cppには元々このファイルと同内容の`RegexAdd()`手書き列があった
(このリポジトリでは元からRubyタイプの正規表現キーワードは実装済みだった)。1件だけ食い違いが
あり(`&`始まりのブロック引数パターンの色が手書き側`COLORIDX_REGEX2`、当時のRuby.rkw側は
`COLORIDX_REGEX3`)、手書き側(=実際に長年使われてきた挙動)を正として採用し、Ruby.rkwを
その内容に合わせて上書きした上で`#include`に置き換えた。今後Ruby.rkwを編集する際は
この経緯を踏まえること。

## アウトライン解析ルール(`*.rule`)

`*.kwd`/`*.rkw`とはさらに別物。`CDocOutline::ReadRuleFile()`(`key1,key2 /// GroupName,Lv=1`や
`;Mode=Regex`等の書式)が読む、アウトライン解析用のルールファイル(20260811)。この形式は
`CDocOutline`自身がテキスト全体を都度パースする作りのため、`.kwd`/`.rkw`のように1行ずつ
解析してデータ配列や関数呼び出し列に変換する必要が無く、**ファイル内容をそのままC++11の
raw文字列リテラルとして埋め込むだけ**(エスケープ不要)。

| ファイル | 対応する CType_*.cpp(`m_szOutlineRuleFilename`) | 生成物 |
|---|---|---|
| JavaScript.rule | CType_JavaScript.cpp | generated/js_rule.inc |
| Ruby.rule | CType_Ruby.cpp | generated/ruby_rule.inc |
| php.rule | CType_Php.cpp | generated/php_rule.inc |

**`.rule`にも`.kwd`のような自動読み込みの起点自体が無い**(`.rkw`と同様)。`CDocOutline::ReadRuleFile()`
はアウトライン表示のたびに`Keyword\*.rule`を都度ディスクから読みに行く実装で、これをキャッシュ・
一括ロードする仕組みは元から存在しない。そのため`.rkw`と違い**CType_*.cpp側の変更は不要**で、
`CDocOutline.cpp`(`sakura_core/doc/`)側に埋め込み文字列3つと`GetEmbeddedOutlineRule()`を追加し、
`ReadRuleFile()`が実ファイルを開けなかったときだけそちらにフォールバックするよう1箇所だけ改修した
(ファイル名は`m_szOutlineRuleFilename`のディレクトリ部分を除いた末尾で照合する)。

`sakura_keyword/lua.rule`も置かれているが、このリポジトリには`CType_Lua.cpp`(Luaタイプ)が
存在しないため対応先が無く、対象外。

## 再生成方法

```powershell
.\tools\GenerateKeywordInc.ps1
```

`sakura_core\types\generated\*.inc` を再生成します(引数省略時はこの `sakura_keyword\` を参照)。
`generated\*.inc` は `.gitignore` 対象のため `git diff` では差分が見えない。生成後は
`generated\*.inc` の中身を目視するか、`sakura.sln` をビルドしてエラーが無いことで確認すること。

### 注意: 16タイプ分は現時点で未同期

上記のうち **元々ソースに組み込み配列があった16タイプ**(cpp.kwd 〜 rtf.kwd の行)は、
現在の `sakura_core/types/generated/*.inc` が「元の埋め込み配列をそのまま .inc 化しただけ」で、
このフォルダの `.kwd` の内容とは**同期していません**(例: `html5.kwd` は元の埋め込み配列より
キーワードが大幅に増えています)。スクリプトを実行すると .kwd の現在の内容で上書きされ、
`tex1.kwd`(TEX)側にあった手書きコメント(無効化したキーワードの記録など)はソース側から失われます。
同期する場合は事前に差分をよく確認してください。

新しく追加した6タイプ(CSS/JavaScript/PHP/python/Ruby/C#、`css2.1.kwd` 〜 `csharp-context.kwd` の行)は
最初からこのフォルダの内容と同期しています。
