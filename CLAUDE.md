## 日付と時刻
- 日付や時刻を扱う際は、常に日本時間（JST / UTC+9）を基準としてください。

## sakura_core/sakura_rc.rc のエンコード
- このファイルは **UTF-16LE (codepage 1200, BOM付き)** です。通常のUTF-8前提の
  Read/Editツールで直接開いて保存すると、エンコードが壊れて文字化けし、
  rc.exeのビルドエラー（例: `RC2104: undefined keyword or key name: MS`）や
  リソース破損の原因になります。
- 経緯: 元々CP932(Shift-JIS)だったが、rc.exeのプリプロセッサがCP932前提で
  バイト単位スキャンするため、UTF-8+`#pragma code_page(65001)`に変換しても
  DBCSの誤認識でクォート対応がズレて失敗した。UTF-16LEに変換したところ
  正しくビルドできることを確認し、以後この方式を採用している。
- 日本語文字列の追加・変更が必要な場合は、Pythonなどでファイルをbytesとして
  読み込み、`utf-16` (BOM付きは`utf-16`、無しなら`utf-16-le`)でデコード/エンコード
  してから書き戻すこと。ASCII範囲のみの変更であっても、この方式を踏襲すること。
- `.gitattributes`で`working-tree-encoding=UTF-16`を設定済みのため、
  作業ツリー上はUTF-16LEのまま、`git diff`等はUTF-8に変換されて読める。