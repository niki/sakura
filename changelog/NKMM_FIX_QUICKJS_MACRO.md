# NKMM_FIX_QUICKJS_MACRO 実装レポート

対象フラグ: `NKMM_FIX_QUICKJS_MACRO`(新規)

## 背景

サクラエディタのマクロ/プラグインは、JScript/VBScriptをWindows Script Host(WSH,
`IActiveScript`/COMオートメーション経由)で実行する仕組みしか持っていなかった。WSHは
OS登録済みの外部COMコンポーネント(`JScript.dll`/`VBScript.dll`)への依存であり、これが
無い(または将来のWindowsで廃止された)環境ではマクロ機能自体が動かなくなる。また、
JScript/VBAという言語を使わずに、一般利用者でも簡単に「機能追加」をスクリプトで書ける
手段が欲しい、という要望があった。

調査の結果、マクロから呼び出せる関数群(`CEditorIfObj`等)は、すでに「関数名＋引数型＋
戻り値型」のテーブル(`MacroFuncInfo`配列)と、実処理を行う`HandleCommand`/`HandleFunction`
という、呼び出し元エンジンに依存しない形で実装されていることが分かった。WSH固有なのは
これを`IDispatch`/COMオートメーション経由で呼び出す接続部分(`CIfObj`の`IDispatch`実装、
`CWSHClient`が`IActiveScript`を`CoCreateInstance`する部分)だけである。そのため、この
接続部分だけをQuickJS向けの薄いアダプタに差し替える形で実装した。既存のマクロ関数本体
(`CEditorIfObj.cpp`ほか)は一切変更していない。

WSH自体は削除しておらず、`.js`/`.vbs`のWSHマクロ・プラグインは引き続き使用できる
(両エンジンが共存する)。

---

## 追加したファイル

### 外部ライブラリ(vendor)

- **`libs/quickjs/`**(新規) — [quickjs-ng](https://github.com/quickjs-ng/quickjs)
  v0.15.1(MIT License)のインタプリタ本体一式を静的組み込み用にvendor。
  - `quickjs.c` / `quickjs.h`, `quickjs-atom.h`, `quickjs-opcode.h`, `quickjs-c-atomics.h`
  - `libregexp.c` / `.h`, `libregexp-opcode.h`
  - `libunicode.c` / `.h`, `libunicode-table.h`
  - `dtoa.c` / `.h`
  - `cutils.h`(**1箇所手動パッチ済み**。後述)
  - `list.h`
  - `builtin-array-fromasync.h`, `builtin-iterator-zip.h`, `builtin-iterator-zip-keyed.h`
    (`quickjs.c`が直接includeする生成済みヘッダ)
  - `LICENSE`
  - 取り込んだのはquickjs-ngの`CMakeLists.txt`が定義する`qjs_sources`(ライブラリ本体、
    4つの`.c`)と、それらが依存するヘッダのみ。`quickjs-libc.c`(ファイルI/O・print等の
    OSバインディング)、`qjs.c`/`qjsc.c`(CLIツール)、テスト・fuzzing関連は含めていない。
    スクリプトに公開する機能は`CEditorIfObj`等が明示的に登録したものだけに限定するためで、
    これによりWSHの`ActiveXObject("Scripting.FileSystemObject")`のような無制限アクセスの
    穴が無い、WSHよりサンドボックスが狭い状態になっている。

### マクロエンジン本体

- **`sakura_core/macro/CQuickJSMacroMgr.h` / `.cpp`**(新規) — `CMacroManagerBase`を
  継承したQuickJS版マクロエンジン。`CWSHMacroManager`/`CPPAMacroMgr`と同じ形
  (`ExecKeyMacro`/`LoadKeyMacro`/`LoadKeyMacroStr`/`Creator`/`declare`)で
  `CMacroFactory`に登録される。拡張子`.qjs`のマクロファイルを実行する。
  マクロ停止("マクロ停止"ダイアログでの中断)や、プラグインから追加の
  インタフェースオブジェクトを束縛するための`AddParam`/`ClearParam`も持つ。
- **`sakura_core/macro/CQuickJSIfObjBinder.h` / `.cpp`**(新規) — `CWSHIfObj`派生
  オブジェクト(`CEditorIfObj`/`CPluginIfObj`等)が持つ`MacroFuncInfo`テーブルを
  読み取り、QuickJSのグローバルオブジェクトへ関数として登録する橋渡し役。
  `VARIANT`⇔`JSValue`の型変換、`IsGlobal()`オブジェクトの二重登録(`Editor.xxx`と
  修飾無し`xxx`の両方)を行う。

### プラグイン機構

- **`sakura_core/plugin/CQuickJSPlugin.h` / `.cpp`**(新規) — `CWSHPlugin`/`CWSHPlug`
  を複製し、内部で使う`CWSHMacroManager`を`CQuickJSMacroMgr`に置き換えたもの。
  `plugin.def`の`Type:`に`qjs`を指定したプラグインを実行する。
- **`sakura_core/plugin/CPluginIfObj.cpp`**(新規) — 元々`CPluginIfObj.h`内に直接
  書かれていた静的メンバ配列(`m_MacroFuncInfoCommandArr`/`m_MacroFuncInfoArr`)の
  定義本体を切り出したもの(理由は「実装中に見つかった不具合」参照)。

### ドキュメント

- **`changelog/NKMM_FIX_QUICKJS_MACRO.md`**(このファイル)

---

## 修正した既存ファイル

- **`sakura_core/macro/CWSHIfObj.h`** — `HandleCommand`/`HandleFunction`/
  `GetMacroCommandInfo`/`GetMacroFuncInfo`は`protected`のため、QuickJSバインダから
  呼べるよう`public`な薄い転送メソッド(`InvokeCommand`/`InvokeFunction`/
  `InvokeGetMacroCommandInfo`/`InvokeGetMacroFuncInfo`)を追加した。追加分は
  `NKMM_FIX_QUICKJS_MACRO`でガード。COM専用の`ReadyMethods`/`AddMethod`
  (`IDispatch`用TypeInfo構築)には一切触れていない。既存のWSH向け実装は無改造。
- **`sakura_core/macro/CSMacroMgr.cpp`** — コンストラクタ内、`CPPAMacroMgr::declare()`/
  `CKeyMacroMgr::declare()`/`CWSHMacroManager::declare()`の並びに
  `CQuickJSMacroMgr::declare()`の呼び出しを追加。
- **`sakura_core/plugin/CPluginManager.cpp`** — `LoadPlugin`内の`Type:`判定
  (`wcsicmp`で`"wsh"`/`"dll"`を比較している箇所)に`"qjs"`の分岐を追加。
- **`sakura_core/plugin/CPluginIfObj.h`** — 静的メンバ配列の定義本体を`.cpp`へ移動
  (宣言のみ残す)。加えて`plugin/CPlugin.h`のincludeを追加(理由は後述)。
- **`libs/quickjs/cutils.h`** — vendorしたファイルへの**唯一の手動パッチ**。`ctz64()`が
  32-bit MSVCでビルドできない不具合を修正(後述)。
- **`sakura_core/my_config.h`** — `NKMM_FIX_QUICKJS_MACRO`フラグを追加。目的・対象
  ファイル・設計判断を説明するコメントブロック付き。
- **`sakura/sakura.vcxproj` / `sakura/sakura.vcxproj.filters`** — 上記の新規ファイル
  一式の追加と、以下のビルド設定。

---

## 実装の詳細

### 1. QuickJSエンジンの選定: quickjs-ng

Bellard版QuickJSではなく[quickjs-ng](https://github.com/quickjs-ng/quickjs)
(アクティブにメンテナンスされているフォーク、MIT License)のv0.15.1を採用した。

- MSVC/Windowsビルドを`CMakeLists.txt`で公式サポートしている(`if(MSVC)`分岐、
  `/STACK:8388608`等Windows固有の設定も用意されている)。オリジナルのBellard版は
  GCC拡張(computed goto等)やUNIX前提のビルド手順を使っており、MSVCで動かすには
  追加のパッチが必要になる。
- ライセンスはPCRE2導入時の`libs/pcre2`(`NKMM_FIX_REGEXP_FALLBACK`)と同じ考え方で、
  ソース一式を`libs/quickjs/`へvendorし静的にsakura.exeへ組み込む方式にした
  (外部DLLとして新たな依存を増やさないため)。

### 2. CQuickJSMacroMgr(WSHの接続部分の置き換え)

- `ExecKeyMacro`: `CWSHClient`が`Execute`ごとに使い捨ての`IActiveScript`エンジンを
  作るのと同様、実行のたびに`JS_NewRuntime`/`JS_NewContext`で使い捨てのランタイムを
  作り、実行後に`JS_FreeContext`/`JS_FreeRuntime`で破棄する。
- `Creator(ext)`: `CPPAMacroMgr::Creator`と同様、レジストリを引かない固定拡張子判定
  (`_tcscmp(ext, _T("qjs")) == 0`)。WSHの`.js`/`.vbs`は`HKEY_CLASSES_ROOT`の
  ProgID解決に依存しているため、専用拡張子`.qjs`にすることで衝突・曖昧さを避けた。
- マクロ停止("マクロ停止"ダイアログでの中断): WSH版は`IActiveScript::
  InterruptScriptThread`を別スレッドから呼んで中断していたが、QuickJSには
  `JS_SetInterruptHandler(rt, handler, opaque)`(バイトコード実行中に定期的に
  呼ばれるコールバック)があるため、これを使ってabortフラグをポーリングする形に
  した。ダイアログ表示・メッセージポンプ自体は`CWSH.cpp`の`AbortMacroProc`と
  同じ構造(`CDlgCancel`を使う)を踏襲している。ユーザーが明示的にキャンセルした
  場合は、その操作自体が結果を示しているため、重ねて例外エラーダイアログは
  出さない。
- `AddParam`/`ClearParam`: プラグイン(`CQuickJSPlugin`)が`CPluginIfObj`のような
  追加のインタフェースオブジェクトを積んでおけるようにする、`CWSHMacroManager`と
  同名のAPI。`ExecKeyMacro`実行時に`m_Params`の全オブジェクトをバインダへ束縛する。

### 3. CQuickJSIfObjBinder(既存コマンドテーブルの再利用)

`CWSHIfObj`が持つ`GetMacroCommandInfo()`/`GetMacroFuncInfo()`(`MacroFuncInfo`配列)を
読み取り、対応する`JSCFunction`(`JS_NewCFunctionMagic`、`magic`をバインダ内の
メソッド配列へのインデックスとして使用)をQuickJSへ登録する。

- `pObj->Name()`(例: `"Editor"`)という名前のオブジェクトを**常に**グローバルへ作る
  (`Editor.InsText()`のような修飾付き呼び出しのため)。加えて`IsGlobal()`が`true`の
  オブジェクト(`CEditorIfObj`)は、WSHの`SCRIPTITEM_GLOBALMEMBERS`相当として、
  同じ関数をグローバル直下にも(修飾無しで呼べるよう)登録する。
- 呼び出し先の判定・引数の扱いは`CWSHIfObj::MacroCommand`(WSH版のディスパッチャ)の
  挙動をそのまま踏襲: `LOWORD(ID) >= F_FUNCTION_FIRST`なら関数呼び出し(引数を
  `VARIANT`配列に変換して`HandleFunction`。数値は整数ならVT_I4、そうでなければ
  VT_R8、文字列はVT_BSTR)、それ以外はコマンド呼び出し(WSH版と同様、全引数を
  `JS_ToString`で無条件に文字列化して`HandleCommand`。`Arguments[3]`まで無条件
  参照する実装があるため`argCountMin = max(4, argc)`のパディングも踏襲。戻り値は
  スクリプト側には返さない)。

### 4. プラグイン機構への対応(CQuickJSPlugin)

`CWSHPlugin::InvokePlug`はCOM/IDispatchを直接使わず、`CWSHMacroManager`のAPI経由で
完結していたため、`CQuickJSMacroMgr`に同じ形のAPIを持たせた上で、`CWSHPlugin`/
`CWSHPlug`を複製する形で`CQuickJSPlugin`/`CQuickJSPlug`を作成した。
`CPluginManager::LoadPlugin`の`Type:`判定に`"qjs"`を追加しただけで、プラグイン
一覧UI等の変更は不要だった(プラグイン種別はUIでハードコードされておらず、
`plugin.def`の`Type:`文字列をそのまま比較しているだけのため)。

WSH版との差分が1点ある。`CWSHPlugin::InvokePlug`は`CPluginIfObj`(スタック上に
確保)を渡す前に`AddRef()`を呼んでいるが、これはCOMの参照カウントが0に戻った
瞬間に`Release()`が`delete this`を呼んでしまう(スタックオブジェクトに対しては
未定義動作)のを防ぐための帳尻合わせであり、`CQuickJSIfObjBinder::BindObject`は
そもそもAddRef/Releaseを呼ばない設計のため、QuickJS版ではこの手当ては行っていない。

---

## ビルド設定の注意点(vcxproj)

- QuickJSの`.c`ファイル一式(`dtoa.c`/`libregexp.c`/`libunicode.c`/`quickjs.c`)には
  `_GNU_SOURCE;WIN32_LEAN_AND_MEAN;_WIN32_WINNT=0x0601;QUICKJS_NG_BUILD`を指定
  (quickjs-ng公式`CMakeLists.txt`の`qjs_defines`と同じ)。`PrecompiledHeader`は
  `NotUsing`、プロジェクト全体の`ForcedIncludeFiles`(`my.h`、C++専用のsilicaを
  includeしておりCコンパイルと衝突する)は個別に空へ上書き。PCRE2導入時と同じ対応。
- この4ファイルのみ`LanguageStandard_C`を`stdc11`に設定(`quickjs-c-atomics.h`が
  C11の`<stdatomic.h>`を要求するため。プロジェクト全体には広げていない)。
- `AdditionalIncludeDirectories`に`..\libs\quickjs`を追加(4config全て)。
- `sakura.exe`本体の`StackReserveSize`を8MB(`8388608`、quickjs-ng公式の
  `/STACK:8388608`と同値)へ拡張。既定のMSVCスタック(1MB)だと深い再帰スクリプトで
  スタックオーバーフローする恐れがあるため。スタック予約サイズを増やすだけで
  通常は既存処理への実害はない(コミットサイズは既定のまま)。
  `CQuickJSMacroMgr::ExecKeyMacro`側でも`JS_SetMaxStackSize(rt, 4*1024*1024)`を
  設定し、この8MBに収まる安全マージンを取っている。

---

## 実装中に見つかり、修正した不具合

### ビルド時に見つかったもの

1. **`cutils.h`の`ctz64()`が32-bit MSVCでビルドできない** — `_BitScanForward64`は
   64-bit(x64/ARM64)専用のイントリンシックで、32-bit MSVCには存在しない。同ファイルの
   `clz64()`は`#if INTPTR_MAX == INT64_MAX`で32-bit時のフォールバックを既に持っていたが
   `ctz64()`にはそれが無かった(quickjs-ng側の見落としと思われる)。`clz64()`と同じ
   パターンでフォールバックを追加した(vendorしたファイルへの唯一の手動パッチ)。
2. **`quickjs.c`が生成済みヘッダ3本をincludeする** — `builtin-array-fromasync.h`/
   `builtin-iterator-zip.h`/`builtin-iterator-zip-keyed.h`。CMakeLists.txtの
   `qjs_sources`(`.c`一覧)には出てこないが、`quickjs.c`本体が直接includeする
   (TC39提案の一部をJSで実装しバイトコード化したもの)ため、追加でvendorした。
3. **`quickjs-c-atomics.h`がC11の`<stdatomic.h>`を要求する** — 上記「ビルド設定の
   注意点」の通り、対象4ファイルのみ`LanguageStandard_C=stdc11`を設定して解決。
4. **新規ファイルがBOM無しUTF-8だった** — 本プロジェクトの既存ソース(日本語
   コメントを含む)はUTF-8 BOM付きで統一されており、BOMが無いとMSVCがシステムの
   コードページ(この環境ではCP932)で解釈してしまい、コメント中の日本語マルチ
   バイト列が原因で構文エラーになった。新規ファイル全てにBOMを付与して解決。

### 実機確認・追加実装時に見つかったもの

5. **`ReferenceError: Editor is not defined`** — `CQuickJSIfObjBinder::BindObject`の
   実装ミス。`CEditorIfObj::IsGlobal()`が`true`であることを「グローバルオブジェクト
   直下にだけ関数を展開すればよい(`Editor`という名前のオブジェクト自体は作らなくて
   よい)」と誤解しており、`Editor.InsText()`のような修飾付きの呼び出しが一切
   できなかった。実際のWSHの`SCRIPTITEM_GLOBALMEMBERS`は「名前付きアイテムとしての
   登録(`Editor.xxx`)」に加えて「修飾無しでも呼べる」を追加するだけで、名前付き
   アイテムとしての登録自体は`IsGlobal()`の値に関わらず常に行われる。`BindObject`を
   修正し、`Editor`オブジェクトを常にグローバルへ作った上で、`IsGlobal()`のときだけ
   追加でグローバル直下にも同じ関数を登録するようにした。
6. **`LNK2005`: `CPluginIfObj::m_MacroFuncInfoCommandArr`/`m_MacroFuncInfoArr`の
   多重定義** — `CPluginIfObj.h`がクラス静的メンバ配列の**定義本体**をヘッダ内に
   直接書いていた(宣言ではなく定義)。以前は`CWSHPlugin.cpp`だけが`CPluginIfObj.h`を
   includeしていたため問題化していなかったが、`CQuickJSPlugin.cpp`も同じヘッダを
   includeするようになったことで、2つの翻訳単位それぞれに同じシンボルの定義が
   生成され、ODR違反(リンクエラー)になった。定義本体を新規`CPluginIfObj.cpp`へ
   切り出して解決した(`inline`変数(C++17)での解決も検討したが、本プロジェクトの
   Win32構成は`LanguageStandard`を明示指定しておらずC++17未満(`error C7525`で確認
   済み。x64構成のみ`stdcpp20`を指定)のため、Win32/x64で挙動を分けたくなく不採用)。
   副次的に、`CPluginIfObj.h`が`m_cPlugin`(`CPlugin&`)を参照するインライン実装を
   持つのに`plugin/CPlugin.h`をincludeしておらず、`CWSHPlugin.h`が先に`CPlugin.h`を
   includeすることに暗黙に依存していた点も判明したため、`CPluginIfObj.h`に
   `#include "plugin/CPlugin.h"`を追加して自己完結させた。

---

## 既知の制限

- 引数の型変換は`VT_I4`/`VT_R8`/`VT_BSTR`/`VT_BOOL`/`VT_EMPTY`のみ対応(WSH版が実際に
  やり取りしている型集合と同じ)。QuickJSのBigInt・オブジェクト・配列等はそのままでは
  渡せず、コマンド呼び出し(`HandleCommand`)経路では文字列化されて渡る。
- `COutlineIfObj`/`CSmartIndentIfObj`/`CComplementIfObj`(アウトライン解析・スマート
  インデント・補完のプラグイン拡張ポイント)はQuickJS側では未接続。
  `CQuickJSIfObjBinder::BindObject`は`CWSHIfObj`派生オブジェクトであれば何でも
  束縛できる設計にしてあるため、将来対応する場合もこのバインダをそのまま流用できる
  想定。
- QuickJSの例外オブジェクトの`stack`プロパティが無い(または文字列でない)場合は
  メッセージ本文のみを表示する。

---

## 動作確認について

quickjs-ng v0.15.1のソースはGitHubから`git clone --depth 1 --branch v0.15.1`で取得し、
`libs/quickjs/`へvendorした。

このサンドボックスビルド環境では、既存の無関係な不具合によりプロジェクト全体の
通しビルドが最後まで通らないため、`msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`
(x64は`sakura.sln`に対して、Win32は`sakura.vcxproj`に対して。x64を`sakura.vcxproj`
単体に対して実行すると`HeaderMake`/`MakefileMake`という無関係な補助プロジェクトが
Release|x64構成を持たずMSB8013で落ちるため、ソリューション経由でsakuraプロジェクト
だけをターゲットにする必要があった)によるファイル単位ビルドで、Win32/x64の両方に
ついて新規・変更ファイル全てが0エラーでコンパイルできることを確認した(warningのみ。
内容はquickjs-ng自身のsign-compare/truncation系と、既存のsilica/codecvt非推奨警告で、
いずれも今回の変更に起因しない)。

一方、以下は実機での確認ができていない:

- `.qjs`マクロの実行(`Edit.MoveSelectedLinesDown/Up.qjs`への移植・`ReferenceError`
  修正までは実機確認済みだが、修正後の再実行確認は未実施)
- マクロ停止ダイアログでの中断
- `plugin.def`に`Type: qjs`を指定したプラグインの導入・実行
- リンク(フルビルド)そのもの(ファイル単位コンパイルの確認に留まる)
