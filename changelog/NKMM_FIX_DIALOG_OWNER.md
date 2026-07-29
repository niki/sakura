# NKMM_FIX_DIALOG_OWNER 実装レポート

対象フラグ: `NKMM_FIX_DIALOG_OWNER`（新規）
対象ファイル(主なもの):

- `sakura_core/env/CSakuraEnvironment.h` / `.cpp`（`ResolveDialogOwnerWindow()`新設）
- `sakura_core/dlg/CDialog.cpp`（`DoModal`/`DoModeless`全3種）
- `sakura_core/util/MessageBoxF.cpp`（`GetMessageBoxOwner`）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

「終了確認ダイアログが画面中央ではなく、アクティブな編集ウィンドウの中央に
表示されるようにできないか」という要望を受けて調査したところ、症状は
終了確認ダイアログに限らず、タスクトレイ経由で表示されるダイアログ/
メッセージボックス全般に共通する問題だと判明したため、個別対応ではなく
共通経路での一般化した修正とした。

## 原因

Windowsのダイアログ/メッセージボックスは、有効なオーナーウィンドウが
渡されればその中央に、オーナーが`NULL`または非表示ウィンドウの場合は
画面中央に表示される。

本アプリはタスクトレイアイコンを持つ「コントロールプロセス」と、実際の
編集ウィンドウを持つ「エディタプロセス」が別プロセスになっているため、
コントロールプロセス側からダイアログ/メッセージボックスを表示する際に
渡せる自プロセス内の妥当なウィンドウが無いケースがある。実例:

- `CControlTray::TerminateApplication`(終了確認)がタスクトレイメニューから
  呼ばれる場合、`GetTrayHwnd()`(タスクトレイの隠しウィンドウ)がオーナーとして
  渡される。
- `CControlTray::DoGrepCreateWindow`呼び出し元の`m_cDlgGrep.DoModal(m_hInstance, NULL, ...)`
  のように、タスクトレイ経由のGrepダイアログは`NULL`がオーナーとして渡される。

いずれも「編集ウィンドウではない/存在しない」オーナーのため、ダイアログが
画面中央に表示されていた。

## 対応

個別の呼び出し箇所を1つずつ直すのではなく、ダイアログ表示の共通経路2箇所に
一括で修正を入れた。

- `CDialog::DoModal` / `DoModeless`(全3オーバーロード) — カスタムダイアログ
  (置換・Grep・ジャンプ・プロパティ等)はすべてここを通る。
- `MessageBoxF.cpp`の`GetMessageBoxOwner` — `MYMESSAGEBOX`/`VMessageBoxF`系の
  メッセージボックスはすべてここを通る。

新設した`ResolveDialogOwnerWindow()`(`CSakuraEnvironment.cpp`)で、渡された
オーナーウィンドウが`NULL`または非表示の場合に限り、フォアグラウンドウィンドウが
編集ウィンドウ(`IsSakuraMainWindow`)であればそちらを使うようにした。

```cpp
HWND ResolveDialogOwnerWindow( HWND hwndOwner )
{
    if( hwndOwner == NULL || !::IsWindowVisible( hwndOwner ) ){
        HWND hwndForeground = ::GetForegroundWindow();
        if( IsSakuraMainWindow( hwndForeground ) ){
            return hwndForeground;
        }
    }
    return hwndOwner;
}
```

既に有効なオーナー(プロパティダイアログ自身など、`IsSakuraMainWindow`が
偽でも可視な正当なウィンドウ)が渡されているケースは`::IsWindowVisible`が
真になるため変更されない。無効化すれば従来通りの挙動に戻る。

### `NKMM_FIX_DIALOG_POS`との関係

`NKMM_FIX_DIALOG_POS`は「オーナーウィンドウに対してどこに配置するか」
(中央ではなく右上寄せ等の座標計算)を担当し、`m_hwndParent`が何であるかには
関与しない。本フラグは逆に「`m_hwndParent`自体を何にするか」を決める層であり、
役割が異なるため競合しない。むしろ、`CDlgGrep`のように`SetPlaceOfWindow()`
(引数無し版、内部で`m_hwndPlaceOfWindow = m_hwndParent`)を使うダイアログでは、
本フラグが`m_hwndParent`をタスクトレイ経由でも正しく解決することで、
`NKMM_FIX_DIALOG_POS`の位置計算が機能する前提条件を補う関係になる。

## 実装の経緯

最初は`CControlTray::TerminateApplication`単体に個別のオーナー解決ロジックを
追加したが、ユーザーから「終了時のダイアログに限らず、ダイアログ全般の話」と
指摘を受け、共通経路(`CDialog::DoModal`/`DoModeless`、`GetMessageBoxOwner`)へ
一般化する方針に変更した。個別パッチは`git checkout`で取り消し済み。

## 既知の制限・要検証事項

- `::GetForegroundWindow()`は瞬間的な状態を見るため、ダイアログ表示の
  タイミングによってはユーザーの意図と異なるウィンドウが「フォアグラウンド」に
  なっている可能性はゼロではない。
- `macro/CWSH.cpp`・`macro/CQuickJSMacroMgr.cpp`の`IDD_MACRORUNNING`
  (「エディタビジーでも表示できるよう、親を指定しない」という意図的なコメント付き)
  も本フラグの対象に含まれる。意図的にNULLを渡している箇所だが、今回はあえて
  除外せず一律で解決するようにした(エディタがビジーでも表示自体は妨げられない
  想定)。この判断の妥当性は実機確認できていない。

## 動作確認について

`Release|Win32`構成でのビルド成功のみ確認済み。タスクトレイメニューからの
終了確認・Grepダイアログが実際にアクティブな編集ウィンドウの中央に表示される
ことの実機確認はユーザー側に依頼中。
