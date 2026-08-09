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

## 再生成方法

```powershell
.\tools\GenerateKeywordInc.ps1
```

`sakura_core\types\generated\*.inc` を再生成します(引数省略時はこの `sakura_keyword\` を参照)。
実行後は `git diff sakura_core/types/generated/` で差分を確認してからコミットしてください。

### 注意: 16タイプ分は現時点で未同期

上記のうち **元々ソースに組み込み配列があった16タイプ**(cpp.kwd 〜 rtf.kwd の行)は、
現在の `sakura_core/types/generated/*.inc` が「元の埋め込み配列をそのまま .inc 化しただけ」で、
このフォルダの `.kwd` の内容とは**同期していません**(例: `html5.kwd` は元の埋め込み配列より
キーワードが大幅に増えています)。スクリプトを実行すると .kwd の現在の内容で上書きされ、
`tex1.kwd`(TEX)側にあった手書きコメント(無効化したキーワードの記録など)はソース側から失われます。
同期する場合は事前に差分をよく確認してください。

新しく追加した6タイプ(CSS/JavaScript/PHP/python/Ruby/C#、`css2.1.kwd` 〜 `csharp-context.kwd` の行)は
最初からこのフォルダの内容と同期しています。
