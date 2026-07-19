# NKMM_FIX_REGEXP_FALLBACK 実装レポート

対象フラグ: `NKMM_FIX_REGEXP_FALLBACK`（新規）
対象ファイル(主なもの):

- `libs/pcre2/`（新規、PCRE2 10.47をvendor。BSD-3-Clause）
- `sakura_core/extmodule/CRegexFallback.h` / `.cpp`（新規、フォールバック実装本体）
- `sakura_core/extmodule/CBregexpDll2.h` / `.cpp`
- `sakura_core/extmodule/CBregexp.cpp`
- `sakura_core/CRegexKeyword.cpp`
- `sakura_core/view/CEditView.cpp`
- `sakura_core/my_config.h`
- `sakura/sakura.vcxproj` / `.vcxproj.filters`

---

## 背景

サクラエディタは検索/置換/Grep/マクロ/タイプ別設定の正規表現キーワード強調のすべてで
`bregonig.dll`（Perl5互換, BREGEXP.DLL系）を`LoadLibrary`で動的ロードして使っている。
このDLLが見つからない環境では、正規表現機能が丸ごと無効化される（チェックボックスが
グレーアウト、または操作がエラーで中断）だけで、代替手段が無かった。

「正規表現ライブラリ（DLL）が見つからない場合は標準ライブラリの正規表現にフォールバック
する」という要望を受け、DLLが無い環境でも正規表現機能が動作し続けるようにした。

## 実装の経緯（2段階）

### 第1段階: std::regexによるフォールバック

まず`std::wregex`(ECMAScript文法)を使ったフォールバックエンジンを実装した。低レベルAPI
(`CBregexpDll2`の`BMatch`/`BSubst`/`BMatchEx`/`BSubstEx`/`BRegfree`/`BRegexpVersion`)
を委譲先として差し替える設計とすることで、これを直接呼ぶ`CBregexp`(検索/置換/Grep/
マクロ)と`CRegexKeyword`(構文強調)の両方に無改修に近い形で波及させた。

DLLが「見つからない」場合(`DLL_LOADFAILURE`)のみフォールバックを発動し、DLLはあるが
エクスポート不整合(壊れている)の場合は今まで通りエラーにする、という区別を設けた
(誤って壊れたDLLの存在を隠蔽しないため)。

しかし`std::regex`のECMAScript文法には次の制約があり、BREGEXPが元々サポートしていた
構文の一部が「正規表現エラー」になってしまう問題が残った。

- ルックビハインド `(?<=...)` `(?<!...)` 非対応
  → `CBregexp::MakePatternAlternate()`が生成するCR/LF補正パターン自体がルックビハインド
    を使うため、フォールバック時はこの補正を丸ごとスキップするという妥協策が必要だった
- POSIX文字クラス `[[:alpha:]]` 等が非対応
- Unicode文字プロパティ(`\p{...}`)非対応、`\w`等もASCII基準

### 第2段階: PCRE2への置き換え

上記の制約を解消するため、フォールバックエンジンを**PCRE2**(Perl Compatible Regular
Expressions, BSD-3-Clauseライセンス)に置き換えた。PCRE2はルックビハインド・POSIX
文字クラス・Unicode文字プロパティ・拡張構文(`/x`)・複数行モード(`/m`)をすべて
ネイティブサポートしており、BREGEXP(Oniguruma系)の挙動にstd::regexよりずっと近い。

**依存の取り込み方法**: PCRE2ソース一式を`libs/pcre2/`に直接vendorし、静的に
sakura.exeへ組み込む方式にした（`libs/silica`と同じ考え方）。フォールバック機能は
そもそも「DLLが無い環境で動く」ことが目的のため、フォールバックの実装自体が新たな
外部DLL依存を持ってしまっては本末転倒である。PCRE2は`configure`/CMakeを使わずに
手動ビルドする手順が公式にサポートされている(`NON-AUTOTOOLS-BUILD`)ため、これに
従って`config.h.generic`→`config.h`、`pcre2.h.generic`→`pcre2.h`、
`pcre2_chartables.c.dist`→`pcre2_chartables.c`をリネームし、標準ライブラリ構成に
必要な`.c`一式(約30ファイル)を追加した。`PCRE2_CODE_UNIT_WIDTH=16`を指定し、
`wchar_t`(UTF-16)とそのまま噛み合う16bit版を使うことで文字コード変換を不要にした。

`CRegexFallback.cpp`の内部実装(コマンド文字列パース以外)をPCRE2 C APIベースに
全面書き換えした。`CBregexpDll2`・`CBregexp`・`CRegexKeyword`側の呼び出し規約は
一切変更していない。

- コンパイル: `pcre2_compile_16()`。`i`→`PCRE2_CASELESS`、`x`→`PCRE2_EXTENDED`
  (ネイティブ対応のため手書きの空白/コメント除去処理が不要になり削除)、
  `m`→`PCRE2_MULTILINE`、Unicode対応として`PCRE2_UTF | PCRE2_UCP`を既定で有効化。
- 検索: `pcre2_match_16()`。`targetbeg`からの本当のオフセットとして検索開始位置を
  渡せるため、std::regexで必要だった`match_prev_avail`の代用策が不要になり、
  ルックビハインドや`\b`が`targetbeg`基準で正しく効くようになった。
- 置換: `pcre2_substitute_16()`を使用。`PCRE2_SUBSTITUTE_MATCHED`で事前の
  `pcre2_match_16()`結果を渡すことで二重マッチングを避けつつ、置換後も
  `GetIndex()`/`GetMatchLen()`が最初の一致位置を指すBREGEXP仕様
  (`CDocOutline.cpp`のコメント実例 `GetString() = "ABC123456DEF"` で確認)を
  満たしている。戻り値がそのまま置換件数になるため、std::regex版で必要だった
  手書きのイテレータループ・0幅マッチ対策・`$1`置換テンプレート整形コードは
  まるごと削除できた(PCRE2の置換構文はPerl/BREGEXPの`$1`とほぼ互換)。

`CBregexp::Compile()`に第1段階で入れていた「フォールバック時はルックビハインドを
含むCR/LF補正をスキップする」という特別扱いは、PCRE2がルックビハインドをネイティブ
サポートするため不要になり撤去した(DLL版・フォールバック版とも同じ経路)。

## ビルド設定の注意点（vcxproj）

- PCRE2の`.c`ファイル一式には`PCRE2_CODE_UNIT_WIDTH=16;PCRE2_STATIC;HAVE_CONFIG_H`を
  個別に指定し、`PrecompiledHeader`は`NotUsing`にした(StdAfx.hをincludeしないため)。
- プロジェクト全体の`ForcedIncludeFiles`(`my.h`)は、C++専用のヘッダオンリーライブラリ
  `silica`を`#include`しており、Cとしてコンパイルされるこれらのファイルに適用すると
  `yvals_core.h`関連のコンパイルエラー(STL1003)になる。PCRE2の`.c`ファイルでは
  `ForcedIncludeFiles`を空に上書きして無効化した。
- `CRegexFallback.cpp`には`PCRE2_CODE_UNIT_WIDTH=16;PCRE2_STATIC`を追加(`pcre2.h`を
  includeするために必要)。

## 既知の制限

- それでもPCRE2が非対応の構文(PCRE2独自の一部拡張の欠如など、ごく僅か)は
  「正規表現エラー」として通知される(新規実装ではなく既存のエラー表示経路を流用)。
- `\w`/`\d`等の文字クラスはUnicode基準(`PCRE2_UCP`)で解釈され、BREGEXPの
  Unicode/ASCII/Locale切り替えオプション(`u`/`a`/`d`/`l`)はno-op。
- `BTrans`/`BSplit`(コードベース内で未使用)はフォールバック未対応。

## 動作確認について

このサンドボックスビルド環境ではプロジェクト全体のフルビルドが、今回の変更とは無関係な
既存の不具合(`CStrictInteger`まわりの型変換エラー、`git stash`で変更前でも再現することを
確認済み)により最後までは通らない。加えてMSVCの既定動作では、あるビルドバッチが
1つでも失敗すると同一ターゲット内の後続バッチ(PrecompiledHeader不使用扱いのPCRE2一式が
該当)が丸ごとスキップされてしまうため、通しビルドのログだけでは新規追加分の成否を
確認できない。

そのため`msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`によるファイル単位ビルド
で、PCRE2の`.c`31ファイル全部と`CRegexFallback.cpp`・`CBregexp.cpp`を個別にコンパイル
し、すべて0警告0エラーであることを確認した。実機での動作確認(検索/置換/Grep/構文強調の
実操作、ルックビハインドパターンが実際にマッチすることの確認等)は未実施。
