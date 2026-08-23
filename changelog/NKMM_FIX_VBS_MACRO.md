# NKMM_FIX_VBS_MACRO 実装レポート

対象フラグ: `NKMM_FIX_VBS_MACRO`(新規、`NKMM_FIX_QUICKJS_MACRO`と合わせて定義する前提)

> **登録順について**: `CSMacroMgr.cpp`のコンストラクタでは常に
> `CWSHMacroManager::declare()`の後に`CVbsMacroMgr::declare()`を呼ぶ、下記
> 「拡張子の奪い合いを避ける設計」の順序を維持している(単なる登録順の
> 入れ替えによる一時しのぎのテストは廃止した)。「WSHが無い場合」の動作を
> 確認したいときは、下記の`NKMM_USE_WSH`フラグを使うこと。

> **`NKMM_USE_WSH`フラグ(2026-08-23追加、`NKMM_USE_PPA`と同じ位置付け)**:
> `my_config.h`に新設。既定で有効(`#define NKMM_USE_WSH`)。単に登録順を
> 入れ替えるだけの以前のテスト方法(WSHは実際には残ったまま、優先順位だけ
> 変える)と違い、このフラグをコメントアウトすると`NKMM_USE_PPA`と全く同じ
> 要領で**コンパイル自体**から次のファイルが除外される(内容全体が
> `#ifdef NKMM_USE_WSH`で囲まれ、フラグ無効時は空の翻訳単位になる)。
> - `sakura_core/macro/CWSHManager.h` / `.cpp`(`CWSHMacroManager`本体)
> - `sakura_core/plugin/CWSHPlugin.h` / `.cpp`(`CWSHPlugin`/`CWSHPlug`。
>   `CWSHMacroManager`に直接依存するプラグイン側ラッパー)
>
> `sakura_core/plugin/CPluginManager.cpp`の`GetPlugin()`(プラグイン種別を
> 判定してインスタンス化するif/elseの連なり)は、`wsh`分岐を
> `#ifdef NKMM_USE_WSH`で囲んだ上で、どの組み合わせのフラグでも構文が
> 崩れないよう`if( false ){ ... }else if(...)...`という「絶対に成立しない
> if を先頭に置く」定石で書き換えた(`qjs`分岐の既存の`#ifdef NKMM_FIX_QUICKJS_MACRO`
> と同じ形に揃えている)。
>
> **`CWSHIfObj`/`CWSH`(`sakura_core/macro/CWSHIfObj.h,cpp`・`CWSH.h,cpp`)は
> 対象外**。名前に"WSH"を含むが、実体はプラグイン/マクロにエディタの機能を
> 公開するための共有基盤クラスで、`CQuickJSIfObjBinder`・`CDllPlugin`・
> `COutlineIfObj`・`CComplementIfObj`・`CSmartIndentIfObj`・`CEditorIfObj`他、
> `CWSH`と無関係な多数のファイルから参照されている。ここまで
> `NKMM_USE_WSH`でゲートすると他エンジン・他機能が軒並みビルドできなく
> なるため、意図的に対象から外した。
>
> `NKMM_USE_WSH`を無効にした状態でも`NKMM_FIX_VBS_MACRO`が有効であれば
> `.vbs`はそのまま`CVbsMacroMgr`が処理する。

> **実機検証で見つけた不具合の修正(2026-08-23)**: `macro_bench/checkall.vbs`・
> `macro_bench/calendar.pas`移植版(`calendar.vbs`)に続き、既存の実運用寄りの
> サンプル`macro_bench/Benchmark.vbs`(WSH用に書かれた実在のVBScript)を
> `CVbsToJsTranspiler`にかけたところ、以下3件の不具合が見つかり修正した。
> 1. **`Dim arr()`(境界未指定の動的配列)が`ParseError`になる** —
>    `BuildNewArrayExpr`が次元数0を考慮しておらず、常に「3次元以上は非対応」
>    のエラーに落ちていた。次元数0のときは`[]`(空配列。後続の`ReDim`で
>    実サイズが決まる前提)を返すよう修正。
> 2. **`Timer`(括弧無しの組み込み関数参照)が変数参照として誤変換される** —
>    `t0 = Timer`のような、VBScriptで括弧を付けずに呼び出せる組み込み関数を
>    他の識別子と同様の「ただの変数参照」として扱っていたため、関数を呼ばずに
>    関数オブジェクト自体を代入してしまっていた(結果、`t1 - t0`が`NaN`になる)。
>    `parsePrimary`に`Timer`専用の特別扱いを追加し、`Timer()`という呼び出しへ
>    変換するよう修正(`Now`/`Date`/`Time`はJSのグローバル`Date`と名前が衝突する
>    ため今回は対象外。必要になったら`Array`/`String`と同様の別名ランタイム
>    関数への読み替えが必要)。ランタイム側に`Timer()`の実装を追加。
> 3. **`InfoMsg msg`のような、括弧無し・引数ありの手続き呼び出しが誤変換される** —
>    従来は識別子の直後が`(`でなければ常に「引数無しの呼び出し」
>    (`InfoMsg();`)として変換しており、後続の`msg`トークンが未消費のまま
>    次の文として再パースされ、`msg();`という無関係な呼び出しが生成される
>    不具合があった(エラーにはならず、静かに誤ったコードを生成する類の
>    不具合だった点で特に注意が必要)。`parseStatement`に
>    `isStatementBoundary`判定を追加し、識別子の直後が改行・`:`・
>    `Else`/`End`/`Next`/`Loop`/`Wend`等の「文の終わり」を示すトークン
>    でなければ、`(`を伴わないカンマ区切りの引数列とみなして読み進める
>    よう修正した。

## 背景

サクラエディタは`CWSHMacroManager`経由でWSH(Windows Script Host)のVBScript
エンジンを使い、拡張子`.vbs`のマクロを実行できる。ただしWSHはOS登録済みの
外部コンポーネントで、無効化・削除された環境(WSHはセキュリティ上の理由で
無効化されることがある、また将来のWindowsで削除される可能性が取り沙汰されて
いる)では`.vbs`マクロがそもそも動かない。

[[NKMM_FIX_PASCAL_MACRO]](`changelog/NKMM_FIX_PASCAL_MACRO.md`)でPascal風
マクロ向けに導入した「独自の軽量トランスパイラでJavaScriptへ変換し、
`NKMM_FIX_QUICKJS_MACRO`のQuickJSベースのマクロエンジン(`CQuickJSMacroMgr`)
にそのまま実行させる」という方式を、VBScript風マクロにも適用した。

## 既存機能との共存(拡張子の奪い合いを避ける設計)

Pascal版と異なり、`.vbs`は既に`CWSHMacroManager`が使っている拡張子である。
そのため今回は新しい拡張子を割り当てるのではなく、`CMacroFactory::Create`が
「登録順にCreatorを試し、最初に非NULLを返したものを使う」という既存の仕組み
(`CMacroFactory.cpp`)を利用し、`CSMacroMgr`のコンストラクタで
`CWSHMacroManager::declare()`の**後**に`CVbsMacroMgr::declare()`を呼ぶ
順序にした。

- `CWSHMacroManager::Creator`は`.vbs`のスクリプトエンジンがレジストリ
  (`HKEY_CLASSES_ROOT\.vbs\ScriptEngine`)に登録されていない環境では`NULL`を
  返す(既存実装)。
- したがって、WSHが使える環境では従来通りWSHが`.vbs`を処理し、挙動は変わらない。
- WSHが使えない/レジストリが無い環境でのみ、`CVbsMacroMgr`(本トランスパイラ
  経由のQuickJS実行)へ自動的にフォールバックする。

この設計により、利用者側は環境の違いを意識せず同じ`.vbs`マクロを書ける
(ただし本トランスパイラは後述の通りVBScriptの実用的なサブセットのみに
対応するため、フォールバック時に非対応構文があれば実行時にエラーとして
表示される)。

## 追加したファイル

- **`sakura_core/macro/CVbsToJsTranspiler.h`**(新規、ヘッダオンリー) —
  VBScript風マクロ言語をJavaScriptソースへ変換する再帰下降パーサー兼
  コード生成器。[[NKMM_FIX_PASCAL_MACRO]]の`CPasToJsTranspiler.h`と同じ
  設計方針(字句解析→パースしながらJS文字列を組み立てる一体型トランスパイラ)
  を踏襲している。`Dim`/`ReDim`(1・2次元配列)、代入/`Set`、
  `If/ElseIf/Else/End If`(単一行形式含む)、`For(To/Step)`/`For Each`、
  `Do/Loop`(前置・後置`While`/`Until`)、`While/Wend`、`Sub`/`Function`
  (戻り値は関数名への代入で表現)、`Call`、`Exit Sub/Function/For/Do`、
  `Const`、コメント(`'`および`Rem`)、行継続(`_`)、四則演算・文字列連結(`&`)・
  比較・論理演算子(`And`/`Or`/`Not`/`Xor`)、`True`/`False`/`Nothing`/`Null`/
  `Empty`リテラルに対応する。
- **`sakura_core/macro/CVbsMacroMgr.h` / `.cpp`**(新規) — `CQuickJSMacroMgr`を
  継承し、`LoadKeyMacro`/`LoadKeyMacroStr`だけをオーバーライドする構成は
  `CPasMacroMgr`と同一。`Creator(ext)`は拡張子`"vbs"`のときだけ自身を生成する。
- **`changelog/NKMM_FIX_VBS_MACRO.md`**(このファイル)

## 修正した既存ファイル

- **`sakura_core/my_config.h`** — `NKMM_FIX_VBS_MACRO`フラグを追加
  (`NKMM_FIX_PASCAL_MACRO`の直後)。
- **`sakura_core/macro/CSMacroMgr.cpp`** — コンストラクタ内、
  `CPasMacroMgr::declare()`の後に`CVbsMacroMgr::declare()`の呼び出しを追加
  (`CWSHMacroManager::declare()`より後であることが、上記のフォールバック
  設計上重要)。
- **`sakura/sakura.vcxproj` / `sakura/sakura.vcxproj.filters`** — 本セッション
  では編集していない(vcxprojはユーザーが手動管理するテンプレートのため)。
  新規ファイル3点(`CVbsToJsTranspiler.h`, `CVbsMacroMgr.h`, `CVbsMacroMgr.cpp`)
  を`Cpp Source Files\macro`/`Header Files\macro`フィルタへ追加する必要がある
  (`CPasToJsTranspiler.h`等の既存エントリと同じ形式)。

## 実装の詳細

### 1. VBScript特有の設計判断

- **識別子は2文字目以降にアンダースコアを含められる**(例: `S_InsText`)。
  VBScriptの行継続記号`_`(行末の空白の後に置く)と識別子中の`_`は、
  「英字で始まる識別子は英数字と`_`を続けて読む」という字句解析のルールに
  より曖昧無く区別できる(識別子スキャン中に現れた`_`は常に識別子の一部として
  読み進められ、行継続としての判定は識別子の外側=独立した位置に現れた`_`
  だけが対象になる)。この規約はサクラの`S_`接頭辞呼び出し規約([[NKMM_FIX_PASCAL_MACRO]]
  と共通)が機能するために必須(初期実装では識別子にアンダースコアを一切
  許さない設計になっており、`S_InsText`が`S`/`InsText`の2トークンへ分割
  されて構文エラーになる不具合があった。実機検証で発見、修正済み)。
- **`=`は文脈で代入/等価比較を切り替える** — VBScriptには`:=`のような専用の
  代入演算子が無く、`=`が代入にも比較にも使われる。`parseStatement`側で
  「文の先頭で識別子の直後に`=`が来たら代入」と先に判定し、それ以外(式の
  内部)は`parseRelational`が常に等価比較(`===`)として解釈する。
- **配列アクセスと関数/Sub呼び出しの区別** — VBScriptは`a(i)`という同じ構文で
  配列要素アクセスと呼び出しの両方を表す。`Dim`/`ReDim`で宣言された配列名を
  `m_arrayNames`(小文字化して)に記録しておき、参照時にそこに含まれるかで
  `a[i]`(配列アクセス)か`f(args)`(呼び出し)かを出し分けている。単一パスの
  変換のため、配列宣言が使用箇所より前に書かれている前提(既知の制限)。
- **Functionの戻り値の扱い** — VBScriptは関数名自身への代入で戻り値を表現する
  (例: `MyFunc = 1`)。素朴に`let 関数名;`をJS関数本体内に作ると、その時点で
  外側の`function`宣言への束縛がシャドウされ、`MyFunc(...)`のような再帰呼び出し
  が「まだ代入されていないローカル変数」を呼ぼうとして壊れる。これを避けるため、
  戻り値は常に予約名`__ret`という別変数に持たせ、関数名への代入・参照だけを
  パース時に機械的に`__ret`へ読み替える(`ResolveVarName`)。
- **`Array`/`String`ビルトインの名前衝突回避** — VBScriptの組み込み関数
  `Array(...)`(配列リテラル生成)と`String(n, char)`(文字の繰り返し)は、
  JS側の同名グローバル(`Array`コンストラクタ、`String`関数)と名前が衝突する。
  トランスパイラが内部で`new Array(n)`のようなコードを生成する箇所は
  パース結果ではなく直接埋め込むC++文字列のため影響を受けないが、ユーザー
  ソース中の`Array(...)`/`String(...)`呼び出しは`ToJsCallName`で
  `__vbsArray`/`__vbsStringRepeat`(ランタイム側で実装)へ読み替えることで
  衝突を避けている。
- **`&`(文字列連結)の型変換** — VBScriptの`&`は被演算子の型に関わらず常に
  文字列結合になる。両辺をランタイム関数`__vbsStr()`(Empty/Nullを`""`に
  正規化してから`String()`する)で包むことで、`Empty & "x"`が`"undefinedx"`
  のような誤った文字列にならないようにしている。
- **べき乗(`^`)と単項マイナスの優先順位** — VBScriptでは`-2^2`は`-(2^2)`
  (べき乗の方が単項マイナスより強い)と解釈される。`parseUnary`が
  `parsePower`を呼ぶ形にし、指数側(`2^-2`のような)は`parseUnary`を再帰
  呼び出しして単項マイナスを許容している。

### 2. トランスパイルエラーの扱い

[[NKMM_FIX_PASCAL_MACRO]]の`TranspileToJs()`と同じ設計。
`CVbsToJsTranspiler::transpile()`が投げる`ParseError`(`std::runtime_error`)を
`CVbsMacroMgr.cpp`内の`TranspileToJs()`でキャッチし、有効なJavaScriptの例外
送出コード(`throw new Error("VBScript macro syntax error: ...");`)へ差し替えて
`m_Source`にセットする。`LoadKeyMacro`自体は常に成功したことにしつつ、実際の
エラー表示は`CQuickJSMacroMgr::ExecKeyMacro`が持つ既存のJS例外表示処理に乗せる。

### 3. VBScript標準ライブラリのランタイム実装

`CVbsMacroMgr.cpp`の`VBS_RUNTIME_PRELUDE`で、`CStr`/`CInt`/`CLng`/`CDbl`/`CSng`/
`CBool`/`CByte`/`Len`/`Mid`/`Left`/`Right`/`InStr`/`InStrRev`/`Replace`/`UCase`/
`LCase`/`Trim`/`LTrim`/`RTrim`/`Space`/`Chr`/`Asc`/`StrReverse`/`StrComp`/`Split`/
`Join`/`LBound`/`UBound`/`Hex`/`Oct`/`IsEmpty`/`IsNull`/`IsArray`/`IsNumeric`/
`IsDate`/`IsObject`/`TypeName`/`Abs`/`Int`/`Fix`/`Sgn`/`Sqr`/`Rnd`/`Randomize`/
`Exp`/`Log`/`Sin`/`Cos`/`Tan`/`Atn`/`Timer`/`MsgBox`/`InputBox`をJSで実装し、
`vbCrLf`/`vbCr`/`vbLf`/`vbNewLine`/`vbTab`/`vbNullString`/`vbNullChar`という
VBScriptの組み込み定数もグローバル変数として定義したうえで、
トランスパイル結果の先頭に連結してから`m_Source`にセットする。

`IsObject`は本サブセットがCOM/ActiveXオブジェクトを一切扱わないため、常に
`false`を返すだけのスタブ。`Randomize`はJSの`Math.random()`が再シード不可
のため、構文を受理するだけの空実装(実際の乱数系列は変化しない)。
`LBound`/`UBound`は本エンジンが対応する1・2次元配列(ネストしたJS配列)のみ
を想定している。
`MsgBox`/`InputBox`はサクラ本来の同名マクロ関数(`Editor.MessageBox`/
`Editor.InputBox`)と引数の意味が異なる(VBScript版`MsgBox`は
`Prompt/Buttons/Title`、サクラ版`MessageBox`は`Msg/Flags`のみ。VBScript版
`InputBox`は`Prompt/Title/Default`、サクラ版`InputBox`は`Prompt/Default/Flags`)
ため、[[NKMM_FIX_PASCAL_MACRO]]と同じ`S_`接頭辞規約で、サクラ本来の実装は
`S_MsgBox`/`S_InputBox`(→`Editor.MessageBox`/`Editor.InputBox`)経由でのみ
呼べるようにしている。

## 既知の制限

- **Class...End Classは非対応**(クラス構文自体を認識しない。トランスパイル時
  に`ParseError`となる)。
- **COM/ActiveXオブジェクトは非対応**(`CreateObject`、`New`によるオブジェクト
  生成、ドット記法によるプロパティ/メソッドアクセスは一切対応していない。
  ブラウザ/Node.js相当の実行環境が無いQuickJSマクロエンジンでは本来動作
  しないものであるため、意図的に対象外とした)。
- **行ラベル・`GoTo`(エラー処理以外)は非対応**。`On Error Resume Next`/
  `On Error GoTo 0`は認識してコメント化するだけで、実際のエラー握りつぶし
  動作(以後の文でエラーが起きても続行する)は再現していない。
- **配列は1・2次元のみ対応**。3次元以上は`ParseError`になる。
  `ReDim Preserve`は1次元配列のみ対応(2次元以上を指定すると`ParseError`)。
- **配列名の判定は単一パス・フラットなスコープ**。`Dim`/`ReDim`で宣言した
  配列名は関数スコープを区別せず1つの集合で管理しているため、宣言前の
  使用や、配列名と同名の別スコープの非配列変数が混在するコードは誤変換
  される可能性がある。同じ理由で、`Split`/`Array`のように実行時に配列を
  返す関数の戻り値を変数へ代入しただけでは、その変数は配列として登録
  されない。`s = Split(str, ",")`の後で`s(0)`のように添字アクセスすると
  (`s`が`Dim s()`等で配列宣言されていない限り)関数呼び出しと誤認識され、
  `TypeError`になる。`For Each x In Split(str, ",")`のように添字を使わない
  参照であれば問題なく動作する。
- **大文字小文字は正規化しない**([[NKMM_FIX_PASCAL_MACRO]]と同じ制限)。
  VBScriptは本来大文字小文字を区別しないが、トランスパイラは字句をそのまま
  JSへ出力するだけなので、宣言と参照で表記が食い違うコードはJS側で
  `ReferenceError`になる。
- **`ByRef`引数は実際の参照渡しを再現しない**。JSのプリミティブ値渡しの
  制約上、呼び出し先での引数への代入が呼び出し元の変数に反映されることは
  無い(`ByVal`/`ByRef`修飾子はどちらも構文としてスキップされるだけ)。
- **`Nothing`と`Null`はどちらもJSの`null`に畳み込まれる**。オブジェクト参照
  (`Nothing`)と値の不定(`Null`)を区別しない簡略化。
- **`Mod`はJSの`%`へそのまま変換する**。負数を含む被演算子でVBScriptの`Mod`
  とJSの`%`の挙動が厳密に一致しない可能性がある(符号の扱いは一般的な
  ケースでは一致する)。

## 動作確認について

このセッションでは`CVbsToJsTranspiler.h`単体の独立したビルド確認、および
`msbuild`によるファイル単位ビルド確認は未実施。[[NKMM_FIX_PASCAL_MACRO]]の
「動作確認について」と同様の手順(独立コンソールプログラムでの単体実行、
ファイル単位ビルド)での検証と、実機での以下の確認を推奨する。

- WSHが利用可能な環境で、既存の`.vbs`マクロ(WSH実行)の挙動が変わらないこと。
- WSHのVBScriptエンジンをレジストリから一時的に外す(または利用不可な環境を
  用意する)ことで、同じ`.vbs`マクロが`CVbsMacroMgr`(QuickJS経由)へ
  フォールバックし、対応構文の範囲で同等に動作すること。
- 構文エラーを含む`.vbs`マクロをフォールバック実行した際、
  `ReportQuickJSException`のダイアログにトランスパイルエラーの内容が
  表示されること。
