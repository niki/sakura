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
- `sakura_core/view/colors/CColor_Numeric.cpp`（追記、REGEX_MODE==3として任意利用可能に）

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

## 追記: CColor_Numeric.cpp からの直接利用 (REGEX_MODE==3)

`NKMM_FIX_NUMERIC_COLOR`(数値の色付け判定)は元々`REGEX_MODE`マクロで
`std::regex`/`boost::regex`/BREGEXP(bregonig.dll)を切り替えられる作りになって
いた(`CColor_Numeric.cpp`)。ここに`REGEX_MODE==3`として、`CBregexp`経由ではなく
`RegexFallback`名前空間(本フラグの実装本体)を直接呼び出すモードを追加した。

```cpp
#elif REGEX_MODE == 3  // PCRE2
	using _regex = std::wstring;
	using _match = BREGEXP_W*;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            RegexFallback::BMatch(pt.c_str(), p, q, &(match), msg)
	#define _REG_FREE(p)        if (p) { RegexFallback::BRegfree(p); }
	...
```

既存のBREGEXPモード(`REGEX_MODE==2`)が`m_CurRegexp.BMatch()`/`BRegfree()`
(bregonig.dllが無ければ機能しない)を呼んでいるのに対し、`RegexFallback::BMatch`/
`BRegfree`はPCRE2が静的にリンクされているため**bregonig.dllの有無に関わらず常時
利用可能**(`_REG_IS_AVAILABLE()`を`(1)`固定にできる)。コマンド文字列書式
(`"/pattern/k"`)が共通のため、`PREFIX`/`SUFIX`はBREGEXPモードと同じものを流用できた。

`REGEX_MODE==3`を選択するには`NKMM_FIX_REGEXP_FALLBACK`の定義が必須で、未定義
のまま選択すると`#error`でビルド時に気づけるようにしてある。既定値は引き続き
`REGEX_MODE (0)`(std::regex)のままで、この追加は選択肢を増やしただけであり
デフォルト動作は変更していない。

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

## 追記: PCRE2のJIT化 20260728

対象ファイル(追加分):

- `libs/deps/sljit/`（新規、sljit(PCRE2のJITバックエンド)をvendor。BSD、
  zherczeg/sljit、コミット`45f910b`。PCRE2 10.47が`.gitmodules`/`deps/sljit`で
  参照しているのと同一コミット）
- `libs/pcre2/config.h`（`SUPPORT_JIT`を有効化）
- `sakura_core/extmodule/CRegexFallback.cpp`（`CompilePattern`に
  `pcre2_jit_compile_16`呼び出しを追加）
- `sakura/sakura.vcxproj` / `.vcxproj.filters`（sljitソースをIDE表示用に追加）

### 背景

`CBregexp`/`CRegexKeyword`は`BMatch(str, ...)`で一度パターンをコンパイルした後、
同じ`rxp`(コンパイル済みインスタンス)を使い回して`BMatch(NULL, ...)`を繰り返し呼ぶ
設計になっている(`CBregexp.cpp`の`ExistBMatchEx()`分岐、`CColor_Numeric.cpp`の数値
ハイライト等)。数値ハイライトのように画面内の全行に対して毎スクロール・毎入力で
同一パターンをマッチさせ続ける箇所があり、これは「コンパイル済みパターンを繰り返し
マッチさせる」というJITが最も効果を発揮するアクセスパターンに合致する。

ところが従来は`pcre2_compile_16()`でコンパイルするだけでJIT化しておらず、
`pcre2_match_16`/`pcre2_substitute_16`は常にバイトコードインタプリタで実行されていた。

### 実装

`CompilePattern()`内、`pcre2_compile_16()`成功後に`pcre2_jit_compile_16(outCode,
PCRE2_JIT_COMPLETE)`を呼ぶ一行を追加しただけ。戻り値は無視する(非対応パターン・
非対応環境では単に失敗するだけで、それ以降は今まで通りインタプリタでマッチする
ため、呼び出し側の分岐は不要)。PCRE2は一度JITコンパイルに成功したcodeに対しては、
通常の`pcre2_match_16`/`pcre2_substitute_16`呼び出しが自動的にJIT実行に切り替わる
仕様のため、`DoMatch`/`DoSubst`側の呼び出しコードは無改造で恩恵を受ける。

### sljitのvendoringについて

`pcre2_jit_compile.c`は内部で`../deps/sljit/sljit_src/sljitLir.c`を`#include`する
構成になっており(単一翻訳単位ビルド)、`SUPPORT_JIT`を有効にしただけではこの
sljit本体が存在せずビルドが壊れることが判明した。PCRE2本体のvendoring時には
sljit(gitサブモジュール)が含まれていなかったため、今回`libs/deps/sljit/`として
新規にvendorした。バージョンはPCRE2 10.47の`.gitmodules`が指す
`https://github.com/zherczeg/sljit.git`のコミット`45f910b78c6605ebf5b53d3ec7cb00f2312fe417`
と完全に一致させている(pcre2_jit_compile.c側のAPI前提とズレないようにするため)。
`sljit_src/`と`sljit_src/allocator_src/`をまるごとvendorし(ARM/MIPS/PPC/RISCV/S390X等、
このプロジェクトでは使わないアーキテクチャの`.c`も含む。プリプロセッサで自動的に
x86/x64のみが選択されるため無害)、vcxprojの`ClCompile`は追加していない
(`pcre2_jit_compile.c`の既存の1エントリが`#include`経由でまとめてビルドする)。

### 動作確認について

`msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`によるファイル単位ビルドで、
`pcre2_jit_compile.c`をDebug/Release×Win32/x64の4構成全てで0警告0エラーを確認した。
また`SUPPORT_JIT`有効化の影響を受ける`pcre2_compile.c`/`pcre2_match.c`/
`pcre2_context.c`/`pcre2_config.c`も(x64 Debugで)問題なくコンパイルできることを
確認した。実機でのJIT有効化による速度計測(数値ハイライトの体感速度、検索/置換の
実行時間比較等)は未実施。

## 追記: ReplaceAllの前置文字列二重挿入バグ(PCRE2フォールバック限定) 20260810

対象ファイル: `sakura_core/extmodule/CRegexFallback.cpp`(`DoSubst()`/`BSubstEx()`/
`BREGEXP_W_Fallback`)。原因調査後、下記「修正」の通り実装・検証済み。

### 症状

正規表現ReplaceAllを実行すると、**マッチが行頭以外の位置にある行すべて**で、
「行頭から最初のマッチ位置までの前置文字列」が結果に1回余分に挿入される。

例: `ReplaceAll("([0-9]+)", "$1", ...)` (数字を数字自身に置換するだけの、
実質no-opのはずの置換)を1回実行しただけで、

```
置換前: lorem ipsum dolor sit amet va=0 vb=31 vc=262 vd=393 ve=524 vf=655
置換後: lorem ipsum dolor sit amet va=lorem ipsum dolor sit amet va=0 vb=31 vc=262 vd=393 ve=524 vf=655
                                      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ここが余分に増える
```

マッチした数字自体(`$1`が指す内容)や、2箇所目以降のマッチは正しい。壊れるのは
「1行の中で最初に見つかったマッチより前の部分」だけ、1行につきちょうど1回。

### 再現条件

- 検索/置換ダイアログの「すべて置換」ボタン、またはマクロの`ReplaceAll()`。
  どちらも内部的に同じ`CViewCommander::Command_REPLACE_ALL()`
  ([CViewCommander_Search.cpp:926](../sakura_core/cmd/CViewCommander_Search.cpp#L926))
  を通る。
- 正規表現ON。**バックリファレンス使用の有無、繰り返し回数は無関係**。1回の
  `ReplaceAll`実行で、行頭ではない位置にマッチするパターンであれば再現する。
- **`bregonig.dll`/`bregonig64.dll`が見つからず、PCRE2フォールバックが有効な
  環境限定**(下記「挙動の違い」参照)。

### 挙動の違い: 実bregonig.dll vs PCRE2フォールバック

実物の`bregonig64.dll`(`Publish/bregonig190130.zip`に同梱)を`sakura.exe`と
同じディレクトリに置いて同一条件で検証し、フォールバック時の結果と比較した。

| パターン | 実`bregonig64.dll` | PCRE2フォールバック |
|---|---|---|
| `([0-9]+)` → `$1` | 完全にクリーン(置換なし版とバイト単位で一致) | **前置文字列が二重挿入される(本バグ)** |
| `\b(?:lorem\|ipsum\|dolor\|sit\|amet)\b` → `$0` | `$0`をリテラル文字列として置換してしまう(別問題、下記) | クリーン(`$0`を全体一致のバックリファレンスとして正しく解釈) |

前者(`$1`)が今回の本題。後者(`$0`)は逆にPCRE2フォールバックの方が「正しく」
見える結果になっているが、これは**PCRE2とbregonig(Oniguruma系)で`$0`の
構文サポートに違いがある**ことによる非互換であり、本バグとは別種の問題
(修正はスコープ外、記録のみ)。

### 根本原因

`CBregexp::Replace()`([CBregexp.cpp:566-570](../sakura_core/extmodule/CBregexp.cpp#L566-L570))は、
`ExistBSubstEx()`が真のとき`BSubstEx(NULL, szTarget, szTarget+nStart, szTarget+nLen, ...)`
を呼ぶ。`targetbeg`に行頭(`szTarget`)を渡すのは、戻り読み(lookbehind)などの
正規表現機能が「検索開始位置より前の文脈」を参照できるようにするための設計。

`ExistBSubstEx()`([CBregexpDll2.h:94](../sakura_core/extmodule/CBregexpDll2.h#L94))は
`m_bFallback || m_BSubstEx!=NULL`——**フォールバック時は無条件に`true`**を返すため、
フォールバック中は必ず`BSubstEx`経路(下記のバグを持つ経路)を通る。

PCRE2フォールバックの`DoSubst()`([CRegexFallback.cpp:251-324](../sakura_core/extmodule/CRegexFallback.cpp#L251-L324))は、
この`targetbeg`(行頭)を**そのまま`pcre2_substitute_16()`の`subject`引数として渡している**。

```cpp
PCRE2_SPTR16 subject = reinterpret_cast<PCRE2_SPTR16>(targetbeg);       // 行頭
PCRE2_SIZE subjectLen = static_cast<PCRE2_SIZE>(targetendp - targetbeg);
PCRE2_SIZE startOffset = static_cast<PCRE2_SIZE>(target - targetbeg);   // マッチ検索開始位置
...
pcre2_substitute_16(code, subject, subjectLen, startOffset, options, ...)
```

`options`に`PCRE2_SUBSTITUTE_REPLACEMENT_ONLY`を指定していないため、PCRE2は
仕様通り「`subject`全体(行頭からマッチ前の未変更部分も含む)を、マッチ部分だけ
置換後文字列に差し替えて返す」。**PCRE2はドキュメント通りに正しく動作している。**

一方、呼び出し元の`Command_REPLACE_ALL`
([CViewCommander_Search.cpp:1553](../sakura_core/cmd/CViewCommander_Search.cpp#L1553))は、
`cRegexp.GetString()`が「マッチ位置から始まる(前置文字列を含まない)文字列」で
あることを前提に、その一部を`Command_INSTEXT`で挿入している。ドキュメント側に
既にある前置文字列はそのまま残っているところに、戻り値に含まれる同じ前置文字列を
もう一度挿入する形になるため、二重になる。

### まとめ: どちらの規約も単体では正しいが、噛み合っていない

- **PCRE2**: 「`subject`全体を対象に、`REPLACEMENT_ONLY`無指定なら前置文字列
  込みで返す」という自身のAPI契約通りに動いている。
- **`Command_REPLACE_ALL`側の前提**: 「`GetString()`はマッチ位置から始まる」
  という、恐らく実`bregonig.dll`の`BSubstEx`の実際の挙動(前置文字列を出力に
  含めない)に基づいて書かれたコード。
- **`DoSubst()`**: 上記2つの間を取り持つはずが、`targetbeg`(lookbehind用の
  文脈)と`subject`(出力に反映される範囲)を区別せず同じポインタとして
  PCRE2に渡してしまったため、両者の前提のズレがそのまま症状として表面化した。

修正の方向性としては、`DoSubst()`側で`PCRE2_SUBSTITUTE_REPLACEMENT_ONLY`を
使うか、出力から前置文字列(`target - targetbeg`分)を切り落としてから
`BREGEXP_W_Fallback`に格納する、のいずれかが妥当と考えられる。
→ 後者の方針で修正した(下記「修正」参照)。

### 修正

`BREGEXP_W_Fallback`のコンストラクタに`skip`引数(既定値0)を追加し、
`outp`(`GetString()`の起点)をバッファ先頭から`skip`文字分ずらせるようにした。
バッファ自体(`outHeap`)は引き続き全体を所有するため、追加コピーは発生しない。

```cpp
explicit BREGEXP_W_Fallback(std::unique_ptr<wchar_t[]> buf = nullptr, size_t len = 0, size_t skip = 0)
    : BREGEXP_W(MakeBaseW(buf.get() + skip, buf.get() + len))
    , outHeap(std::move(buf))
{
}
```

`DoSubst()`の呼び出し箇所で、`pcre2_substitute_16()`に渡した`startOffset`
(検索開始位置。`targetbeg`起点の未変更プレフィックス長そのもの)をそのまま
`skip`として渡すだけでよい。

```cpp
BREGEXP_W_Fallback* result = new BREGEXP_W_Fallback(std::move(buf), outLen, startOffset);
```

`vStartp`/`vEndp`(`GetIndex()`/`GetMatchLen()`が参照するマッチ位置情報、
`m_szTarget`=`targetbeg`起点の絶対ポインタが前提)は元々の計算のまま変更して
いない——今回のバグは出力文字列(`GetString()`)側だけの問題で、マッチ位置の
報告自体は最初から正しかったため。

`BSubst()`(Exなし、`targetbeg==target`で呼ばれる)は`startOffset`が常に0に
なるため、`skip=0`でこれまで通り無変更。前述の「実bregonig.dllでは再現しない
`$0`のリテラル置換問題」はbregonig側の構文差異であり、この修正の対象外
(今回は未対応のまま)。

### 修正後の動作確認

`msbuild /t:sakura:ClCompile /p:SelectedFiles=..\sakura_core\extmodule\CRegexFallback.cpp`
によるファイル単位ビルド(Release x64)で0エラー・0警告を確認後、フルビルド
(リンクまで)して`sakura.exe`を再生成した(`bregonig64.dll`は置かず、PCRE2
フォールバックを強制した状態)。

- 1プロセス=1回の`FileNew()`+1回の`ReplaceAll()`という最小構成の検証:
  `ReplaceAll("([0-9]+)", "$1", ...)`・`ReplaceAll("\\b(?:lorem|ipsum|dolor|sit|amet)\\b", "$0", ...)`
  いずれも、**置換なしのbaselineとバイト単位で完全に一致**(前置文字列の
  二重挿入が解消)。
- `macro_bench/BenchmarkRegex.qjs`相当(Simple/Alt各5回、計10回の
  `ReplaceAll`)を再実行したところ、結果ドキュメントのサイズは
  1,435,395バイト(baseline 1,434,781バイト+ログ分)で**膨張なし**。
  修正前は同条件で約20MBまで膨れ上がっていた。
  タイミングも副次的に改善した: Simple total=1636ms/avg=327.20ms、
  Alt total=1442ms/avg=288.40ms(修正前の同条件はSimple total=3188ms、
  Alt total=5690ms程度)。これは壊れたドキュメントがパスを重ねるごとに
  肥大化し、後続の`ReplaceAll`の対象データ量そのものが増えていたことの
  裏返しと考えられる。

### 発覚の経緯

`NKMM_FIX_EDITVIEW_SCRBAR`(スクロールバーマーカーのスレッドプール化)の
回帰確認で`macro_bench/BenchmarkRegex.qjs`を実行した際に偶然発見。
その変更(ScrBarMarkerのキャッシュ処理)とは無関係であることは`git stash`に
よるA/B比較で確認済み。詳細な調査過程は
`changelog/NKMM_FIX_EDITVIEW_SCRBAR_THREADPOOL.md`の「追記: macro_bench/
BenchmarkRegex.qjsによる回帰確認」を参照(本追記はその内容を整理・確定させた
最終レポート)。

### 動作確認について

`msbuild /t:sakura:ClCompile`によるファイル単位ビルドは行っていない(調査のみ、
コード変更なし)。フルビルド(Release x64)した`sakura.exe`に対し、非対話マクロ
(`InfoMsg`をファイル書き込みに置き換えたもの)で1プロセス=1回の`FileNew()`+
1回の`ReplaceAll()`という最小構成の検証を、PCRE2フォールバック時と実
`bregonig64.dll`使用時それぞれで実施し、上記の表の内容を確認した。

## 追記: CColor_Numeric.cpp側のREGEX_MODE==3利用は削除 20260806

上記「追記: CColor_Numeric.cpp からの直接利用 (REGEX_MODE==3)」節が指す
`NKMM_FIX_NUMERIC_COLOR`は、呼び出しパターン起因のコンパイル漏れ(パターンごとに
毎回ゼロからコンパイルし直す)・リークが解消不能と判明したため無効化の上、
コード自体を削除した。本フラグ(`NKMM_FIX_REGEXP_FALLBACK`)自体や
`RegexFallback`名前空間は検索/置換/Grep/構文強調キーワードのフォールバックとして
引き続き使われており影響はない。削除の経緯・削除前の実装全文は
`changelog/NKMM_FIX_NUMERIC_COLOR.md`を参照。
