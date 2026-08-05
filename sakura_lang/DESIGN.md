# sakura_lang 多言語化 設計メモ（枠組み・移行準備）

このディレクトリは、.rc の多言語対応を「文字列マクロ分離＋共通本体」方式に段階移行するための
受け皿です。今回のコミットは **枠組みと雛形のみ**（実移行はまだ行っていません）。

## 現状（as-is）

- 実行ファイル用: `sakura_core/sakura_rc.rc`（日本語、`LANGUAGE LANG_JAPANESE`、`sakura.exe` に直接コンパイル）
- 英語サテライトDLL用: `sakura_lang_en_US/sakura_lang_rc.rc`（英語、`sakura_lang_en_US.dll` に別ソリューションでコンパイル）
- 上記2ファイルはダイアログ構造（66個）がほぼ並行だが、キャプション・コントロール文字列が
  インラインリテラルのため **ファイルまるごと複製**して保守している（各5000行超）。
- 実行時言語切替の仕組み（`CSelectLang`）は既存かつ健全。`sakura_lang_*.dll` をファイル名パターンで
  自動検出し、`HINSTANCE` を差し替えて `LoadString` する。無い文字列は exe 本体（日本語）にフォールバック。
- STRINGTABLE（メニュー等）は元々 `LS()` 経由の間接参照なので、言語別ファイルに分かれていること自体は
  設計上問題ない。問題は **DIALOGEX 内の直書きテキスト**。

### 実例で見える言語間の食い違い（IDD_JUMP を比較して判明）

- フォント: 日本語側は `FONT 9, NKMM_RES_FONT_NAME`（`my_config.h` で `"MS Shell Dlg"` に展開）、
  英語側は `FONT 9, "Tahoma"` とハードコード。→ 本体を共有するなら **フォントも言語マクロ化が必要**。
- レイアウト微調整: `IDC_RADIO_LINENUM_CRLF` の x 座標が日本語版 `94`、英語版 `102`
  （英語の "Use Layout(&R)" ラベルが長いため右にずらしてある）。
  → 完全共有本体にすると、こうした**言語別の微調整が効かなくなる**。v1では許容し、
  はみ出しが出た箇所だけ個別対応する方針（前回提案の合意事項）。

## 目標（to-be）

「言語依存の文字列・フォント」と「言語非依存のレイアウト（ダイアログ配置／コントロールID／
STRINGTABLE構造）」を分離する。

```
sakura_lang/
  DESIGN.md              ← このファイル
  common/
    sakura_rc_body.rc.sample   ← 共通本体の書き方サンプル（1ダイアログ分、未結線）
  jp-ja/
    lang_strings.h.sample      ← 日本語文字列マクロのサンプル
  en-us/
    lang_strings.h.sample      ← 英語文字列マクロのサンプル
```

- `sakura_core/sakura_rc.rc` の**位置は変更しない**（このリポジトリの既存ビルドに影響を与えない）。
- ディレクトリ名は要望どおり `jp-ja/` `en-us/`（小文字ハイフン区切り）とする。
  ※ BCP47的には `ja-JP` / `en-US` が一般的だが、本プロジェクトの表記に合わせて `jp-ja` / `en-us` を採用。
- DLLサテライト方式（`sakura_lang_en_US` プロジェクト）は将来的に廃止予定。
  ただし**代替のビルド／実行時切替方式は今回は未確定**。次のいずれか、あるいは他案を後日決定する。
  1. ビルド時に言語ヘッダを選び、言語ごとに exe / リソースDLL を生成する現行方式を維持しつつ、
     .rc ソースだけを本体＋言語ヘッダに再編する（`CSelectLang` の仕組みはそのまま活かせる）。
  2. 実行時切替は維持しつつ、DLLではなく別形式（リソースオンリーEXE等）に変更する。
  3. その他（未検討）。
  → **この決定は次フェーズで行う。今回のスコープ外。**

## 文字列マクロ化パターン（サンプルの読み方）

`common/sakura_rc_body.rc.sample` は `IDD_JUMP` ダイアログ1つだけを題材に、
キャプション・コントロール文字列・フォントをすべて `LANG_*` マクロに置き換えた例。
実際のビルドには組み込んでいない（`.sample` 拡張子でプレースホルダであることを明示）。

各言語の `lang_strings.h.sample` が `LANG_*` マクロの実体（リテラル文字列）を定義し、
最終的には以下のように束ねる想定（未実装）:

```rc
// sakura_core/sakura_rc.rc（既存ファイル、将来的に本体を #include する形に変更）
#include "../sakura_lang/jp-ja/lang_strings.h"
#include "../sakura_lang/common/sakura_rc_body.rc"
```

```rc
// sakura_lang_en_US/sakura_lang_rc.rc（または後継の何か）
#include "../sakura_lang/en-us/lang_strings.h"
#include "../sakura_lang/common/sakura_rc_body.rc"
```

## 移行方法メモ（次フェーズでやること）

1. `sakura_core/sakura_rc.rc` と `sakura_lang_en_US/sakura_lang_rc.rc` はダイアログ/コントロールIDが
   並行しているため、ID単位で突き合わせて差分（文字列リテラル）を自動抽出するスクリプトを書く。
2. 抽出した文字列に `LANG_xxx` マクロ名を機械的に採番し、`jp-ja/`・`en-us/` 配下のヘッダへ出力。
3. 本体（レイアウト）は日本語版をベースに、リテラルをマクロ参照へ置換して `common/` 配下に生成。
4. フォント・言語ごとの座標微調整（上記 IDD_JUMP の例）は棚卸しして個別対応リストを作る。
5. 生成結果を実ビルドし、両言語でダイアログが従来と同一に見えるか目視確認する。
6. DLLサテライト方式の後継（上記「未確定」の3択）を決定し、ビルド配線を行う。

## 今回のコミットでやったこと / やっていないこと

- やったこと: `sakura_lang/` ディレクトリと本ドキュメント、および `IDD_JUMP` 1件分の
  雛形サンプル（`.sample` 拡張子、ビルド未結線）を追加。
- やっていないこと: `sakura_core/sakura_rc.rc` ・`sakura_lang_en_US/sakura_lang_rc.rc` の変更、
  vcxproj/sln の変更、実際のマクロ移行、DLL方式の後継決定。既存ビルドへの影響はゼロ。
