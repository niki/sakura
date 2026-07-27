# NKMM_FIX_FIND_DIALOG_FLAT 実装レポート

対象フラグ: `NKMM_FIX_FIND_DIALOG_FLAT`（新規、`NKMM_FIX_FIND_DIALOG`のサブオプション）
対象ファイル(主なもの):

- `sakura_core/my_config.h`（フラグ定義）
- `sakura_core/sakura_rc.rc`（`IDD_FIND`のスタイル・CAPTION分岐）
- `sakura_core/dlg/CDlgFind.h` / `.cpp`（角丸・スライドインアニメーション本体）
- `sakura_core/window/CEditWnd.cpp`（親ウィンドウ追従の呼び出し）

関連する既存フラグへの追記:

- `NKMM_FIX_DIALOG_POS`（`CDlgFind::FollowParentWindow()`を追加、後述）

---

## 背景

検索ダイアログ(`IDD_FIND`)を、VSCode/Chromeの検索バーのような「エディタ右上にヌッと
スライドインしてくる、タイトルバーの無いフローティングパネル」風の見た目に変更したい、
という要望を受けて実装した。

無効化すれば従来通りタイトルバー付きダイアログに戻る(`#else`側に旧実装を保持)。

## 実装した機能

1. **親ウィンドウへの追従**(`NKMM_FIX_DIALOG_POS`に追記): `CDlgFind::FollowParentWindow()`
   を追加し、`CEditWnd`の`WM_SIZE`/`WM_MOVE`から呼び出す。エディタ本体を動かす/リサイズ
   すると検索ダイアログも追従して位置を再計算する。`SWP_NOACTIVATE`を指定し、追従による
   再配置がエディタ側のフォーカスを奪わないようにしている。
2. **タイトルバー無し + 角丸**: `IDD_FIND`から`WS_CAPTION`/`DS_MODALFRAME`を排除し、
   Windows 11の角丸(`DWMWA_WINDOW_CORNER_PREFERENCE`)を適用。
3. **上からのスライドインアニメーション**: ダイアログ生成直後、最終位置より少し上に
   ウィンドウを配置してから、`WM_TIMER`で実際にウィンドウ位置を150msかけて
   ease-out(3次)で下ろす(`CDlgFind::StartSlideAnimation()` / `OnTimer()`)。

## 実装の経緯（つまずいた点）

### 1. `AnimateWindow(AW_SLIDE)`では子コントロールが追従しない

最初は`AnimateWindow(hWnd, 150, AW_SLIDE | AW_VER_POSITIVE | AW_ACTIVATE)`で実装した。
これはGDI時代の「クライアント領域のビットマップを徐々にめくって見せる」方式のAPIで、
DWM合成下では**ウィンドウ背景(パネル)だけが即座に表示され、コンボボックスやチェック
ボタンなどの子ウィンドウは追従せず遅れて表示される**という不具合が発生した(実機で
「枠が先に全表示されてからコントロールがスライドしてくる」現象として確認)。

対策として、`AnimateWindow`をやめ、`SetWindowPos`でウィンドウの実位置そのものを
`WM_TIMER`で動かす方式に変更した。ウィンドウ自体を移動させれば子コントロールも
一緒に動くため、この問題は解消した。

### 2. `DwmExtendFrameIntoClientArea`による影付けが「青い領域が先に出る」原因に

角丸と合わせて`DwmExtendFrameIntoClientArea`(margins={1,1,1,1})でこのウィンドウ
専用のドロップシャドウを付けようとしたが、DWMがこの拡張マージン領域を
**システムのアクセントカラー(既定で青)で即座に合成描画してしまい**、
`AnimateWindow`によるクライアント領域の再生より先に見えてしまう(「青地がすでに
出ていて、前面のグレー部分が上からアニメしてくる」)という不具合が出た。

`AnimateWindow`自体を廃止した後もこの副作用のリスクが残るため、
`DwmExtendFrameIntoClientArea`の呼び出しは撤去した。影はDWMの既定動作
(通常のトップレベルポップアップに対する自動シャドウ)に任せている。

なお、影付けの手段として一般的な`CS_DROPSHADOW`クラススタイルは意図的に使用して
いない。`IDD_FIND`はリソースで`CLASS`を指定しておらず既定の共有ダイアログクラス
`#32770`を使うため、`SetClassLongPtr`で`CS_DROPSHADOW`を付けると**アプリ内の
すべてのダイアログ**に影が付いてしまう。ウィンドウ単位で安全に影を付けたい場合は
専用のウィンドウクラスを登録する必要があり、今回は見送った。

### 3. `CAPTION`ステートメントが`WS_CAPTION`を暗黙に強制する(最大の落とし穴)

`STYLE`から`WS_CAPTION`を外してもタイトルバーが消えない不具合に長時間ハマった。
ビルドが最新か(リビルド、実行ファイルの取り違え、多重起動プロセスの残留)を一通り
疑って潰した上で最終的に判明した原因は、**rc.exeの仕様として、ダイアログ
テンプレートに`CAPTION "文字列"`ステートメントがあると、`STYLE`の内容に
かかわらず`WS_CAPTION`が自動的に追加される**というものだった。

`.res`をバイナリレベルで直接パースし(`DIALOGEX`テンプレートの`style`
DWORDフィールドを読む)、`STYLE WS_POPUP`単体でコンパイルしても実際には
`style=0x80C00040`(`WS_CAPTION`ビットが立っている)になることを最小再現
コードで確認して原因を特定した。`CAPTION`行を`#ifndef NKMM_FIX_FIND_DIALOG_FLAT`
で囲んで出力自体を止めたところ、`style=0x80000040`(`WS_CAPTION`ビット無し)
になることを同じ方法で確認した。

## 既知の制限

- 影はDWMの既定動作任せのため、環境(DWM設定・ホットパッチ状況)によっては
  従来のような明確な影が出ない可能性がある。
- チェックボタン(`|Ab|` `Aa` `.*`)や「検索」「X」ボタンはまだ標準Win32の
  見た目のまま(オーナードロー化は未着手、次段階の課題として提示済み)。
- `DWMWA_WINDOW_CORNER_PREFERENCE`はWindows 11(22000+)のみ有効。Windows 10では
  無視されるだけで実害はない。

## 動作確認について

`cl.exe /Zs`(構文チェック)・`rc.exe`単体コンパイルに加え、`.res`をバイナリで
直接パースして`IDD_FIND`(ID=110)の`DIALOGEX`スタイルDWORDを検証する自作スクリプトで
`WS_CAPTION`ビットの有無を確認した。実機での最終確認はユーザー側で実施し、
タイトルバー無し・角丸・親ウィンドウへの追従・スライドインアニメーションが
すべて意図通り動作することを確認済み。
