# NKMM_FIND_DIALOG_NO_SHADOW 実装レポート

対象フラグ: `NKMM_FIND_DIALOG_NO_SHADOW`（新規、`NKMM_FIX_FIND_DIALOG_FLAT`のサブオプション）
対象ファイル(主なもの):

- `sakura_core/my_config.h`（フラグ定義）
- `sakura_core/dlg/CDlgFind.cpp`（`OnInitDialog`のDWM設定分岐）
- `sakura_core/sakura_rc.rc`（`IDD_FIND`のSTYLE分岐、`WS_BORDER`追加）

---

## 背景

`NKMM_FIX_FIND_DIALOG_FLAT`(検索ダイアログのフローティングパネル化)で、
`DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND`により角丸にした際、
Windowsが自動で付ける影を消したい、という要望を受けて実装した。

## 内容

`DWMWA_WINDOW_CORNER_PREFERENCE`と`DWMWA_NCRENDERING_POLICY`は同じDWMの
非クライアント描画パイプラインを使っているため、角丸と影は分離できず二者択一になる。
`NKMM_FIND_DIALOG_NO_SHADOW`を1にすると、角丸を諦める代わりに
`DWMWA_NCRENDERING_POLICY = DWMNCRP_DISABLED`でDWMの非クライアント描画自体を
無効化し、影を消す。

```cpp
#if defined(NKMM_FIND_DIALOG_NO_SHADOW) && NKMM_FIND_DIALOG_NO_SHADOW == 1
{
    const int ncRenderingPolicy = DWMNCRP_DISABLED;
    ::DwmSetWindowAttribute( hwnd, DWMWA_NCRENDERING_POLICY, &ncRenderingPolicy, sizeof(ncRenderingPolicy) );
}
#else
{
    const int cornerPreference = DWMWCP_ROUND;
    ::DwmSetWindowAttribute( hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference) );
}
#endif
```

影が無いと背景との境界が分かりにくくなるため、代わりにDWMに依存しない
1px枠線(`WS_BORDER`)を`IDD_FIND`のSTYLEに追加して視覚的な区切りを補っている。

```
#if defined(NKMM_FIND_DIALOG_NO_SHADOW) && NKMM_FIND_DIALOG_NO_SHADOW == 1
STYLE WS_POPUP | WS_BORDER
#else
STYLE WS_POPUP
#endif // NKMM_
```

`0`にすれば従来通り角丸+DWM既定の影に戻る。

## 実装の経緯（つまずいた点）: `sakura_rc.rc`のSJIS破壊

`sakura_rc.rc`はShift-JISでエンコードされたファイルだが、編集ツール経由で
日本語コメント付きの変更を書き込んだところ、ファイル全体がUTF-8として書き直され、
RCコンパイラ(CP932前提)が既存の文字列を誤読して`FONT 9, NKMM_RES_FONT_NAME`の
行(`IDD_TAGJUMPLIST`、変更箇所とは無関係の別ダイアログ)で
`RC2104: undefined keyword or key name: MS`という構文エラーになった
(`git diff --stat`で数千行規模の差分になっていたことで発覚)。

`git stash`で元のSJISファイルに戻した後、PowerShellでバイト列を直接置換する方式
(追記内容をASCII範囲のみに限定したため、SJIS/UTF-8どちらの解釈でも安全)で
同じ変更を入れ直した。差分は意図通り5行追加のみになることを確認した。

この経験から、`.rc`ファイル(および他のSJISエンコードファイル)への日本語コメント
追加を伴う変更は、通常の編集ツールではなく生バイト列操作で行う必要があることが
分かった。

## 既知の制限

- 影を消すとDWMの非クライアント描画が無効化されるため、角丸も同時に失われる
  (両立不可)。
- `WS_BORDER`は単色1pxの素朴な枠線であり、角丸+影版のような立体感は無い。

## 動作確認について

`Release|Win32`構成でのビルド成功のみ確認済み。実際に検索ダイアログを表示して
影が消え、枠線が表示されることの実機確認はユーザー側に依頼中。
