# NKMM_USE_MIMALLOC 実装レポート

対象フラグ: `NKMM_USE_MIMALLOC`（新規）
対象ファイル(主なもの):

- `libs/mimalloc/`（新規、mimalloc v3.4.3をvendor。MIT）
- `sakura_core/_main/WinMain.cpp`（`mimalloc-new-delete.h`のinclude追加）
- `sakura_core/config/build_config.h`（`FILL_STRANGE_IN_NEW_MEMORY`との排他制御）
- `sakura_core/my_config.h`
- `sakura/sakura.vcxproj` / `.vcxproj.filters`

---

## 背景

エディタは行の挿入・削除・Undo/Redoのたびに小さなオブジェクトの確保/解放が大量に発生する
ワークロードであり、標準CRTの`malloc`/`operator new`はこの種のパターンでオーバーヘッドが
出やすい。mimalloc(Microsoft Research製、MITライセンス)はこうした小オブジェクト中心の
ワークロードで高速なアロケータで、既存コードを変更せずに導入できる。

## 組み込み方針: operator new/delete のみ上書き

mimallocはCRTの`malloc`/`free`ごと丸ごと差し替える「完全上書き」と、C++の`operator new`/
`delete`だけを差し替える「部分上書き」の2通りの使い方ができる。完全上書きをWindowsで行うには
`mimalloc-redirect.dll`をexeと同じフォルダに配置する仕組みが別途必要になり、配布物が1つ増える。

今回は配布・ビルドの単純さを優先し、**C++のoperator new/deleteのみ**をmimallocに差し替える
方式を採用した。`libs/mimalloc/include/mimalloc-new-delete.h`を`sakura_core/_main/WinMain.cpp`
(実行ファイルのエントリポイント)の1箇所だけでincludeしている。このヘッダは「1つの翻訳単位でのみ
includeすること」という制約があり、複数箇所でincludeすると多重定義エラーになる。

既存コードの`malloc`/`HeapAlloc`直呼び出し箇所(13ファイル)はこの変更の恩恵を受けないが、
C++の`new`/`delete`経由の確保がエディタの大半を占めるため、影響範囲としては妥当と判断した。

## vendoring方法: 単一翻訳単位ビルド(static.c)

mimallocは`src/static.c`が他の全`.c`ファイルを`#include`でまとめて1つの翻訳単位としてビルド
する「static override」向けの構成を公式に用意している。これを使うことで、`libs/pcre2`のように
個々の`.c`ファイルを1つずつ`ClCompile`に登録して`ExcludedFromBuild`を config ごとに設定する
必要がなく、`ClCompile`は`static.c`の1エントリのみで済む。他の`.c`/`.h`ファイルは
`ClInclude`としてのみ追加している(IDE表示・編集用で、直接コンパイルはされない)。

`src/prim/prim.c`が`#if defined(_WIN32)`で`src/prim/windows/prim.c`を選択するため、
プラットフォーム別の分岐もvcxproj側で書く必要がない。

- `PrecompiledHeader`は`NotUsing`にし、プロジェクト全体の`ForcedIncludeFiles`(`my.h`、
  C++専用のsilicaをinclude)を空に上書きしている(PCRE2/quickjsの.cファイルと同じ理由)。
- mimalloc本体はCMakeで`C_STANDARD 11`を前提にしているため、`static.c`だけ
  `LanguageStandard_C=stdc11`を指定した(quickjsの`.c`ファイルと同じ対応)。
- `MI_MALLOC_OVERRIDE`は定義していない。これはCRTの`malloc`/`free`自体を差し替える
  スイッチで、operator new/deleteのみ上書きする今回の方針には不要(未定義でも
  `alloc-override.c`は無害にコンパイルされるだけで、何も上書きしない)。
- `MI_DEBUG`はmimalloc側が`NDEBUG`の有無から自動判定するため(`NDEBUG`定義時は0、
  未定義時は2)、プロジェクト側で明示的な指定はしていない。

## build_config.hとの衝突と対応

`sakura_core/config/build_config.h`は`_DEBUG`時に`FILL_STRANGE_IN_NEW_MEMORY`を自動定義し、
newされたメモリをわざと汚す(未初期化バグの発見を助ける)ためのグローバル`operator new`/
`operator new[]`/`operator delete`/`operator delete[]`を`inline`で定義している。これは
PCHを通じて事実上すべての翻訳単位に存在する。

`NKMM_USE_MIMALLOC`を有効にすると`mimalloc-new-delete.h`が同名のoperator new/deleteを
(inlineではなく)`WinMain.cpp`の翻訳単位に定義するため、Debugビルドでは名前が衝突し
ODR違反・リンクエラーになる。そのため`FILL_STRANGE_IN_NEW_MEMORY`の自動定義を
`!defined(NKMM_USE_MIMALLOC)`の条件で無効化した。mimallocのDebugビルド(`MI_DEBUG=2`)は
解放済みメモリの汚染や不正解放の検出を独自に行うため、この機能が担っていた役割は
おおむね代替される。

## 動作確認について

このサンドボックスビルド環境ではフルビルドが、今回の変更とは無関係な既存の問題
(`preBuild.bat`がコード生成ツールHeaderMake/MakefileMakeの実行に失敗する。
`NKMM_FIX_REGEXP_FALLBACK`実装時と同種の、この環境固有の制約)により最後までは通らない。

そのため`msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`によるファイル単位ビルドで、
以下を4構成(Debug/Release × Win32/x64)全てで確認した。

- `libs/mimalloc/src/static.c`: 0警告0エラー
- `sakura_core/_main/WinMain.cpp`(`mimalloc-new-delete.h`のinclude込み): 0警告0エラー

`build_config.h`修正前はDebug構成のPCHビルド時に`operator new`等のC4595警告
(非メンバーnew/deleteをinlineにできない、という将来のリンクエラーを予告する警告)が
出ることを確認しており、修正後は解消されている。ただし実際にexeまでリンクしての
動作確認・パフォーマンス計測は未実施。
