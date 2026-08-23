# NKMM_FIX_PASCAL_MACRO 実装レポート

対象フラグ: `NKMM_FIX_PASCAL_MACRO`(新規、`NKMM_FIX_QUICKJS_MACRO`と合わせて定義する前提)

## 背景

サクラエディタには元々、外部コンポーネント`PPA.DLL`(Poor-Pascal for Application、
Delphi/C++Builder用のPascalインタプリタ、ソース非公開)を経由してPascal風の構文で
マクロを書ける仕組み(`NKMM_USE_PPA`、`CPPAMacroMgr`/`CPPA`)があった。ただし
`my_config.h`では「古いものなので無効にする 20170722」というコメント付きで既に
無効化されており、`PPA.DLL`自体の配布・入手性の問題もあって、この経路は事実上
使えない状態だった。

一方、[[NKMM_FIX_QUICKJS_MACRO]](`changelog/NKMM_FIX_QUICKJS_MACRO.md`)により、
WSHにもPPA.DLLにも依存しないQuickJSベースのマクロエンジン(`CQuickJSMacroMgr`)が
既に存在する。そこで、`PPA.DLL`を新たに用意する代わりに、Pascal風のソースを
JavaScriptへ変換した上でこのQuickJSエンジンにそのまま実行させる方式を採用した。

## 追加したファイル

- **`sakura_core/macro/CPasToJsTranspiler.h`**(新規、ヘッダオンリー) — Pascal風
  マクロ言語をJavaScriptソースへ変換する再帰下降パーサー兼コード生成器。
  骨格部分(字句解析・構文解析・コード生成の基本構造)はGoogle Geminiで生成した
  ものをベースに、クラス名のプロジェクト規約対応・無限ループ修正・複数行var宣言
  対応・procedure対応等を加えている(詳細は本ファイル内の各節を参照)。
  `var`宣言・`begin/end`・`if/then/else`・`for(to/downto)`・`while`・
  `repeat/until`・代入・関数/手続き呼び出し・四則演算/比較/論理演算子に対応する。
  関数呼び出し(例: `InsText(s)`)は同名のJS関数呼び出しとしてそのまま出力するため、
  `CQuickJSIfObjBinder`がグローバルへ登録するEditor系関数(修飾無し呼び出し)を
  Pascalのコードから直接呼べる。
- **`sakura_core/macro/CPasMacroMgr.h` / `.cpp`**(新規) — `CQuickJSMacroMgr`を
  継承し、`LoadKeyMacro`/`LoadKeyMacroStr`だけをオーバーライドして「読み込んだ
  Pascal風ソースを`CPasToJsTranspiler`でJavaScriptへ変換してから`m_Source`に
  渡す」処理を挟む。`ExecKeyMacro`(実際のJS実行)は`CQuickJSMacroMgr`の実装を
  そのまま使う。`Creator(ext)`は拡張子`"pas"`のときだけ自身を生成する
  (`CQuickJSMacroMgr::Creator`と同じ、レジストリを引かない固定拡張子判定)。
- **`changelog/NKMM_FIX_PASCAL_MACRO.md`**(このファイル)

## 修正した既存ファイル

- **`sakura_core/my_config.h`** — `NKMM_FIX_PASCAL_MACRO`フラグを追加
  (`NKMM_FIX_QUICKJS_MACRO`の直後)。
- **`sakura_core/macro/CSMacroMgr.cpp`** — コンストラクタ内、
  `CQuickJSMacroMgr::declare()`の後に`CPasMacroMgr::declare()`の呼び出しを追加。
- **`sakura/sakura.vcxproj` / `sakura/sakura.vcxproj.filters`** — 上記の新規
  ファイルを`Cpp Source Files\macro`フィルタへ追加。

## 実装の詳細

### 1. なぜ既存のCQuickJSMacroMgrを継承する形にしたか

`CQuickJSMacroMgr`の`ExecKeyMacro`(JSRuntime/JSContextの生成・破棄、マクロ停止
ダイアログとの連携、`CQuickJSIfObjBinder`によるEditor系関数のバインド、JS例外の
表示)はマクロ言語の種類に依存しない実装になっている。差分は「マクロファイルの
中身をどう`m_Source`(JavaScriptソース、protectedメンバ)へ詰めるか」だけなので、
`LoadKeyMacro`/`LoadKeyMacroStr`だけをオーバーライドする継承が最小差分だった。

### 2. トランスパイルエラーの扱い

`CPasToJsTranspiler::transpile()`は、想定外のトークンに遭遇すると
`CPasToJsTranspiler::ParseError`(`std::runtime_error`)を投げる。これを
`CPasMacroMgr.cpp`内の`TranspileToJs()`でキャッチし、**有効なJavaScriptの例外
送出コード**(`throw new Error("Pascal macro syntax error: ...");`)へ差し替えて
`m_Source`にセットする。

この設計により、`LoadKeyMacro`自体は常に成功したことにしつつ、実際のエラー表示は
`CQuickJSMacroMgr::ExecKeyMacro`が既に持っているJS例外表示処理
(`ReportQuickJSException`、マクロ停止ダイアログと同じ導線)にそのまま乗せられる。
専用のエラー表示コードを新設していない。

### 3. 実装中に見つけ、修正した不具合(トランスパイラ本体)

動作確認のため、独立したコンソールプログラムで`CPasToJsTranspiler`を単体実行して
確認したところ、以下の問題が見つかった。いずれも`sakura_core/macro/CPasToJsTranspiler.h`
側の修正で対応済み。

1. **未対応構文で無限ループする** — `parseStatement()`が、どの文パターンにも
   一致しない場合に「1トークンも消費せず空文字列を返す」実装になっていた。
   呼び出し元の`begin...end`/`repeat...until`ループは「現在のトークンが
   `end`/`until`になるまで`parseStatement()`を呼び続ける」形のため、1トークンも
   進まないままループし続け、実質無限ループになる。Pascal形式の文字コード
   リテラル(`#13#10`等、本トランスパイラは非対応)を含むソースで実機再現した
   (`InsText(#13#10);`をパースさせるとハングし、`taskkill`が必要になった)。
   `parseStatement()`/`parsePrimary()`の「該当なし」フォールバックを、必ず
   `ParseError`を投げる形に変更し、無限ループの可能性を無くした。
2. **`var`セクションが複数の宣言行に対応していない** — Pascalの`var`セクションは
   `var s: string; i: Integer;`のように型の異なる宣言行を複数続けて書けるが、
   旧実装は最初の1行(1つの型)分しか読み進めず、`var`文のパースを終えてしまって
   いた。2行目以降(`i: Integer;`)は独立した文として再パースされ、`i`の直後に
   来る`:`トークンがどの文パターンにも一致せず構文エラーになっていた
   (上記1.の無限ループ修正後は、ここで`ParseError`として顕在化する形になった)。
   `var`の後、次のトークンが識別子である間は宣言行が続くとみなして読み進め、
   まとめて1つのJS `let`文にするよう修正した。

### 4. procedure宣言、S_接頭辞、PPA標準ライブラリのランタイム実装

実際のPPAマクロ資産(`macro_bench/calendar.pas`)を使った動作確認を通じて、以下を
追加実装した。

- **`procedure`宣言**(戻り値なし。`function`とは異なり名前への代入で戻り値を返す
  ことはしない)。定義はメイン処理の`begin`より前(`var`と同じ宣言セクション)に書く
  想定。ローカル`var`宣言(0個以上)を挟んでから`begin ... end;`が続く形に対応した。
  JSの`function`宣言(hoistingされる)へ変換するため、定義順に関わらずメイン処理
  から呼び出せる。
- **`mod`/`div`演算子**(整数剰余・整数除算)。`div`は`Math.trunc(a / b)`へ変換。
- **文字列リテラル中の生の改行**(`CRLF := '<実際の改行>';`のような複数行文字列)を
  `\n`へエスケープするよう修正。修正前はJSの不正な文字列リテラル(SyntaxError)に
  なっていた。
- **`Continue`/`Break`文**。他の識別子と同様に「引数無し手続き呼び出し」として
  扱うと`continue();`のような不正なコードになる(JSの予約語であり関数ではない)ため、
  専用に`continue;`/`break;`文へ変換するようにした。
- **`S_`接頭辞によるサクラAPI呼び出しの区別**(`CPasToJsTranspiler::ToJsCallName`)。
  `S_InsText(...)`のように`S_`接頭辞(大文字小文字区別無し)を付けた呼び出しは、
  接頭辞を剥がして`Editor.InsText(...)`という修飾形式へ変換する。接頭辞が無い
  呼び出しは、PPA言語自体が提供していたと思われるPascal/Delphi標準ライブラリ
  相当の関数とみなし、そのままの名前で出力する。
- **PPA標準ライブラリのランタイム実装**(`CPasMacroMgr.cpp`の`PAS_RUNTIME_PRELUDE`)
  — `StrToInt`/`IntToStr`/`Copy`/`Trunc`/`Frac`/`FloatToStr`/`InputBox`/`MessageBox`
  をJSで実装し、トランスパイル結果の先頭に連結してから`m_Source`にセットする。
  `InputBox`/`MessageBox`はサクラ本来の同名マクロ関数と名前が衝突するが引数の
  意味が異なる(PPA版`InputBox`は`Title/Prompt/Default`、サクラ版は
  `Prompt/Default/Flags`。PPA版`MessageBox`は`Msg/Title/Flags`、サクラ版は
  `Msg/Flags`のみ)ため、グローバルの同名関数を意図的に上書きし、サクラ本来の
  実装は`S_InputBox`/`S_MessageBox`(→`Editor.InputBox`/`Editor.MessageBox`)
  経由でのみ呼べるようにした。`Copy`も同様に、サクラの「選択範囲をクリップボード
  にコピーする」コマンドと名前が衝突するため、無印の`Copy`はPascal標準の文字列
  関数として上書きしている(クリップボードコピーを呼びたい場合は`S_Copy()`と書く)。

### 5. program宣言、Length/Pos/Write/Writeln(macro_bench/checkall.pasでの確認)

`macro_bench/checkall.pas`(PPA標準関数の一括チェック用に作成)を使った動作確認で、
以下を追加実装した。

- **`program <name>;`宣言**。トップレベルに書かれる、実行に影響しないプログラム名
  宣言。キーワードとして認識していなかったため、`program();`/`PPALangCheck();`の
  ような2つの不正な関数呼び出しへ誤変換されていた。`CPasToJsTranspiler::parseStatement`
  で無視するよう対応した。
- **`Length`/`Pos`をランタイムライブラリへ追加**。`Length(s)`は`s.length`
  (UTF-16コード単位数。Pascal本来の「バイト数」とは意味が異なる可能性がある点に
  注意)、`Pos(needle, haystack)`は1-indexedの`indexOf`+1(見つからなければ0、
  Pascalの仕様と一致するようJSの`indexOf`の`-1`を利用している)。
- **`Write`/`Writeln`をランタイムライブラリへ追加**。サクラエディタにはコンソール
  出力に相当するものが無いため、カーソル位置への挿入(`Editor.InsText`)に委譲する
  設計にした。`Write`はそのまま、`Writeln`は末尾に`\r\n`を付加する。連続呼び出しの
  たびにダイアログが出ると使い物にならないため、`InfoMsg`(メッセージボックス)では
  なくテキスト挿入を選んだ。

### 動作確認(追加分)

`macro_bench/checkall.pas`をNode.js上のモック環境(`Editor.InputBox`/
`Editor.MessageBox`/`Editor.InsText`をスタブ化)で実行し、`Length`(19文字)、
`Copy`(8文字目から11文字→"Poor-Pascal")、`Pos`(1-indexedで13、未検出時は0)が
いずれも期待通りの結果になることを確認した。実機での`.pas`実行確認は未実施
(`macro_bench/checkall.pas`もUTF-8 BOM付きで保存すること)。

## 既知の制限

- `(* ... *)`形式のPascalブロックコメントは非対応(`{ ... }`と`//`のみ対応)。
- Pascal形式の文字コードリテラル(`#13`等)、単項`+`、`case`文、`function`宣言
  (`procedure`のみ対応)は非対応。トランスパイル時に`ParseError`となり、実行時に
  JSの例外として表示される(無限ループはしない。上記「実装中に見つけ、修正した
  不具合」参照)。
- 型注釈(`: Integer`等)は構文としてスキップされるだけで、JS側では一切の型
  チェックを行わない(PPA.DLLが持っていたであろう静的型チェックは無い)。
- 引数の型変換は`CQuickJSIfObjBinder`の既存実装に従う(`NKMM_FIX_QUICKJS_MACRO.md`
  の既知の制限を参照。`VT_I4`/`VT_R8`/`VT_BSTR`/`VT_BOOL`/`VT_EMPTY`のみ)。
- Pascalは識別子の大文字小文字を区別しないが、トランスパイラは字句をそのまま
  JSへ出力するだけで正規化しない。そのため、宣言と参照で大文字小文字が食い違う
  コード(元のPPA環境では動いていたコード)は、JS変換後に`ReferenceError`になる。
  実際に`macro_bench/calendar.pas`で`MDays`/`Mdays`の表記ゆれによる実例があり、
  ソース側の表記を統一して解決した。
- **`.pas`ファイルはUTF-8(BOM付き)で保存すること。** マクロ読み込みに使っている
  既存の`CTextInputStream`(`sakura_core/io/CTextStream.cpp`)は、ファイル先頭の
  UTF-8 BOMの有無だけで文字コードを判定し、BOMが無いとShift-JIS(CP932)として
  読み込む仕様になっている(`.pas`専用に追加した判定ではなく、`CTextInputStream`
  の既存の挙動)。BOM無しUTF-8で保存した`.pas`(`macro_bench/calendar.pas`が実際に
  該当した)は、日本語部分がShift-JISとして誤読され文字化けする
  (`InputBox`のダイアログ文言が文字化けする形で実機確認した)。BOMを付与すれば
  正しくUTF-8として読み込まれる。

## 動作確認について

- `CPasToJsTranspiler.h`単体を独立したコンソールプログラムへ組み込み、
  変数宣言・分岐・`for`/`while`/`repeat`ループ・関数呼び出しを含むPascal風
  ソースが妥当なJavaScriptへ変換されることを確認した。
- 上記の無限ループ修正について、修正前は実際にハングすることを確認し
  (`taskkill`で強制終了)、修正後は同じ入力が`ParseError`として即座に検出される
  ことを確認した。
- `msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`によるファイル単位ビルドで、
  `CPasMacroMgr.cpp`(`CPasToJsTranspiler.h`を間接的に含む)がDebug/Release×x64、
  Release×Win32の3構成で0エラーを確認した。Debug×Win32は`HeaderMake`/
  `MakefileMake`という無関係な補助プロジェクトが同構成を持たずMSB8013で落ちる、
  既知のサンドボックス制約(`NKMM_FIX_QUICKJS_MACRO.md`にも同種の記載あり)。
- このサンドボックス環境では`preBuild.bat`が見つからずフルビルド(リンクまで)が
  既存の問題として通らないため、`.pas`マクロを実際にsakura.exe上で読み込んで
  実行する確認は未実施。実機で以下を確認してほしい:
  - 拡張子`.pas`のキーボードマクロがQuickJS経由で実行できること
  - `Editor.InsText()`のような修飾付き呼び出しと、`InsText()`のような修飾無し
    呼び出しの両方が動くこと(`CQuickJSIfObjBinder`の既存の二重登録の恩恵)
  - 構文エラーを含む`.pas`マクロを実行した際、`ReportQuickJSException`の
    ダイアログにトランスパイルエラーの内容が表示されること

## 追記: 代表的なランタイム関数の追加(2026-08-23)

[[NKMM_FIX_VBS_MACRO]](VBScript風マクロ)の実装・動作確認を通じて、姉妹
エンジンであるPascal風マクロの`PAS_RUNTIME_PRELUDE`が`StrToInt`/`IntToStr`/
`Copy`/`Trunc`/`Frac`/`FloatToStr`/`InputBox`/`MessageBox`/`Length`/`Pos`/
`Write`/`Writeln`の12関数のみで、Pascal/Delphi標準ライブラリとして代表的な
関数が不足していたため、以下を追加した(`CPasMacroMgr.cpp`)。

- 文字列: `UpperCase`/`LowerCase`/`Trim`/`TrimLeft`/`TrimRight`/
  `StringReplace`/`CompareStr`
- 数値: `Round`/`Sqr`/`Sqrt`/`Odd`/`Chr`/`Ord`/`Random`/`Randomize`

注意点:
- **`Sqr`は「2乗」、`Sqrt`が「平方根」**(標準Pascalの意味どおり。
  [[NKMM_FIX_VBS_MACRO]]側のVBScript`Sqr`(=平方根)とは意味が異なるので、
  両エンジンを併用する場合は取り違えに注意)。
- `Random(n)`は`n`を渡すと`[0, n)`の整数、渡さないと`[0, 1)`の実数を返す
  (標準Pascalの挙動を再現)。`Randomize`はJSの`Math.random()`が再シード
  不可のため、構文を受理するだけの空実装。
- `StringReplace`はDelphi本来の`TReplaceFlags`(大文字小文字区別・全置換
  指定)を持たず、常に全置換・大文字小文字区別ありの単純な置換のみ対応。

動作確認・マニュアルを兼ねた`macro_bench/runtimeall.pas`を新規作成し、
`PAS_RUNTIME_PRELUDE`の全関数(`InputBox`/`MessageBox`を除く)を一通り
呼び出す構成にした。既存の`macro_bench/checkall.pas`と役割を分けており、
`checkall.pas`は主要関数(`Write`/`Writeln`/`Length`/`Copy`/`Pos`)の
確認、`runtimeall.pas`はランタイム関数の網羅的な確認・リファレンスを担う。
