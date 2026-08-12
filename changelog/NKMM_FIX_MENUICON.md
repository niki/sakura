# メニューアイコン: オーナードロー統一の解消とサブメニュー/描画不具合の修正 20260810

対象フラグ: `NKMM_FIX_MENUICON`(既存)。新規フラグは追加していない。

対象ファイル:
- `sakura_core/uiparts/CImageListMgr.h` / `.cpp`(`GetAlphaBitmap`, `GetBlankBitmap`)
- `sakura_core/uiparts/CMenuDrawer.cpp`(`MyAppendMenu`)

---

## 背景: アイコン付きメニューだけオーナードローで見た目が不一致だった問題

「メニューにアイコンを表示」をONにすると、Vista以降でもオーナードローになり
見た目がXP風になる問題が2010.03.29のコメントに既知の課題として残っていた。
`CMenuDrawer::MyAppendMenu`の
`if( m_bMenuIcon || !IsWinVista_or_later() ) nFlagAdd = MF_OWNERDRAW;`が原因で、
Vista以降でも`m_bMenuIcon==true`なら常にオーナードローされていた。

Vista以降は`MF_OWNERDRAW`をやめ、`MIIM_BITMAP`/`hbmpItem`に32bppアルファ付き
ビットマップ(透過色をアルファ0に変換したもの)を設定することで、通常のテーマ
メニューのままアイコンを表示できるようにした。チェック中の項目はテーマが自動で
ハイライト枠を描画する。オーナードローはVista未満のみ引き続き使用する
(アクセスキー分の詰め処理のため)。

- `sakura_core/uiparts/CImageListMgr.h` / `.cpp`: `GetAlphaBitmap()`を追加。
  アイコン番号ごとに32bppアルファ付きビットマップを生成してキャッシュする
  (破棄はデストラクタ、または`ResetExtend()`での明示リセット時)
- `sakura_core/uiparts/CMenuDrawer.cpp`: `MyAppendMenu()`で、Vista以降かつ
  アイコンありの項目に`MIIM_BITMAP`を設定するよう変更

## サブメニューだけインデントが揃わずアイコン列が無い状態で表示される不具合を修正

サブメニュー(「折り返し方法」「入力改行コード指定」等)項目は`AppendMenu`系の
慣習で子`HMENU`の値が`nFuncId`引数に渡されるため、`CMenuDrawer::MyAppendMenu`内
では「`nFuncId!=0`」の通常項目の分岐に入る。しかし`GetIconIdByFuncId()`に`HMENU`の
値を渡しても対応する機能アイコンは見つからず`bitmapIdx==-1`のままになるため、
実アイコンがない項目として`MIIM_BITMAP`を一切設定していなかった。

テーマメニューは「兄弟項目に`MIIM_BITMAP`があれば無条件に自動整列する」わけでは
なく、自分自身が`MIIM_BITMAP`を持たない項目はインデントされないと判明。実アイコン
の有無によらず、アイコン付きメニューが有効な間はサブメニュー項目にも必ず
`MIIM_BITMAP`を設定するよう修正し、実アイコンが無い場合は
`CImageListMgr::GetBlankBitmap()`(全画素アルファ0の16x16プレースホルダ、1個だけ
生成してキャッシュ)を代わりに設定して兄弟項目とインデントを揃えるようにした。
セパレータ(`MF_SEPARATOR`)はテーマメニューが全幅で描画するため対象外。

## 上記2件の修正後、メニューを開くと項目のテキストごと何も表示されなくなる不具合を修正

`MIIM_BITMAP`を追加した際、`mii.fMask`に元々あった`MIIM_TYPE`をそのまま残していた
のが原因。`MIIM_TYPE`は`MIIM_BITMAP`/`MIIM_FTYPE`/`MIIM_STRING`を束ねた旧式の複合
フラグで、MSDNにも「`MIIM_BITMAP`と同時に指定すると動作が不定になる」と明記
されている。実機でも項目が空欄になる形で再現した。`MIIM_TYPE`を、`MIIM_BITMAP`と
共存できる現代的な代替(`MIIM_FTYPE`+`MIIM_STRING`)に置き換えて修正。

対象: `sakura_core/uiparts/CMenuDrawer.cpp`の`MyAppendMenu()`の`mii.fMask`

## 動作確認について

各修正は実機での再現・解消確認を経て段階的に修正した(コメント内の記載通り)。
自動テストは無し。
