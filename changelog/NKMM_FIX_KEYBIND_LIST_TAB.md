# NKMM_FIX_KEYBIND_LIST_TAB 実装レポート

対象フラグ: `NKMM_FIX_KEYBIND_LIST_TAB`（新規）
対象ファイル(主なもの):

- `sakura_core/my_config.h`（フラグ定義）
- `sakura_core/prop/CPropCommon.h`（`CPropKeybindList`宣言、`ID_PROPCOM_PAGENUM_KEYLIST`、共有データメンバー）
- `sakura_core/prop/CPropCommon.cpp`（`ComPropSheetInfoList[]`への登録、sizeofアサート）
- `sakura_core/prop/CPropComKeybindList.cpp`（新規、本機能の実体）
- `sakura_core/sakura_rc.h` / `sakura_core/sakura_rc.rc`（`IDD_PROP_KEYBIND_LIST`とその子コントロール、Shift-JIS）
- `sakura_core/String_define.h`
- `sakura_lang_en_US/sakura_lang_rc.rc`（英語版ミラー、UTF-8）
- `sakura/sakura.vcxproj` / `sakura/sakura.vcxproj.filters`

---

## 背景

共通設定の「キー割り当て」タブ(`IDD_PROP_KEYBIND`)は、キーを1つ選んでチェックボックス
(Shift/Ctrl/Alt)＋機能一覧から割り当てる「編集」用のUIで、293×240のダイアログが
既にコントロールで埋まっており「機能名とショートカットを対にした一覧」を差し込む
余白がない。全体を俯瞰しながら割り当てたいという要望を受け、既存タブはそのまま残し、
隣に新しい「ショートカット一覧」タブを追加した。

## 実装した機能

1. **機能名/キーの一覧表示**: `CFuncLookup`のカテゴリ・機能列挙(機能一覧コンボ
   ボックスと同じ並び)をそのままたどり、各機能の実際のショートカットを
   `CKeyBind::GetKeyStrList()`で取得してSysListView32(2列)に表示する。
   未割り当ての機能や未登録の外部マクロ枠(`マクロ0 (未登録)`等)も含め、
   将来この画面から設定できるようにする前提ですべて表示する。
2. **種別(カテゴリ)区切り行**: 空のカテゴリ([外部マクロ]等)でも区切り行だけは
   出し、機能一覧コンボボックスとの網羅性を一致させる。
3. **機能名／キーでの絞り込み**: 専用のEditボックスに入力するたびに一覧を
   作り直す。該当する機能が1つもないカテゴリは見出し行ごと除く。
4. **Shift/Ctrl/Alt＋キーのキーセット指定UI**: 指定した組み合わせが既存の
   一覧行と一致すればスクロールしてフォーカスする(逆方向の同期はダブル
   クリック時のみ、詳細は後述)。
5. **「追加」「解除」ボタン**: 現在指定中のキーセットを選択中の機能に追加/
   解除する。実際の保存は共通設定ダイアログ全体のOK押下時まで行われない
   (`m_Common.m_sKeyBind`への書き込みのみ)。
6. **スクロールしても種別が分かる固定表示(スティッキーヘッダー)**: 一覧の
   先頭に見えている行の種別を、専用のSTATICコントロールで常に上書き表示する。

## 実装の経緯（つまずいた点）

### 1. Shift-JISの`sakura_rc.rc`をEditツールや全体デコードで編集すると壊れる

`sakura_rc.rc`はShift-JISで、通常のテキスト編集やPowerShellでの全体デコード/
再エンコードはラウンドトリップ非互換により無関係な行を大量に破壊する。安全な
方法は、生バイトを読み、ASCIIアンカーで位置を特定し、ASCII部分は
`Encoding.ASCII`、日本語部分は`Encoding.GetEncoding(932)`でバイト列を作って
直接差し込む、という外科的スプライシングのみ。`sakura_lang_en_US/sakura_lang_rc.rc`
は正真正銘のUTF-8なので通常のEditツールで問題ない。

### 2. `LVM_SETITEM`が`iSubItem != 0`のとき`LVIF_PARAM`込みだと失敗する

`mask`に`LVIF_PARAM`を含めたまま`iSubItem=1`(キー列)へ`ListView_SetItem`すると、
テキストが設定されずキー列が空のまま表示される。サブアイテム設定前に
`mask = LVIF_TEXT`へリセットする必要がある。

### 3. `BS_AUTOCHECKBOX`は`WM_CTLCOLORBTN`の返却ブラシを背景に反映しない

キーが競合しているときチェックボックスを警告色にしたかったが、標準の
`BS_AUTOCHECKBOX`は`WM_CTLCOLORBTN`を無視する(このコードベースの
`CPropComKeybind.cpp`に残る「うまくいってません」という古いコメント通りの
既知の制限)。`BS_OWNERDRAW`にして`DrawFrameControl`で自前描画することで解決した
(チェック状態もBM_GETCHECK/SETCHECKが効かないため自前で保持)。

### 4. `LVM_GETSUBITEMRECT`は`iSubItem=0`だと`LVIR_BOUNDS`でも行全体の矩形を返す

MSDNに明記された既知の仕様。0列目の文字位置だけは`LVIR_LABEL`(`ListView_GetItemRect`)
で取得する必要がある。この矩形も既定の文字開始位置とは数px異なり、実測して
微調整した。

### 5. `nmcd.uItemState`の`CDIS_SELECTED`が実際の選択状態とずれることがある

`NM_CUSTOMDRAW`の`CDDS_ITEMPREPAINT`で渡される`uItemState`を信用すると、
未選択行が選択済みとして塗られたり、逆に選択行が塗られなかったりする不具合が
発生した。`ListView_GetItemState()`で実際の状態を都度取り直すことで解決した。

### 6. `LVS_EX_FULLROWSELECT`の選択行は`clrTextBk`+`CDRF_NEWFONT`だけでは背景が変わらない

`clrTextBk`/`clrText`を設定して`CDRF_NEWFONT`を返す標準的なイディオムは、
非選択行の色付けには効くが、選択行の背景ハイライトには効かない(既定の
ハイライト色のまま)。種別区切り行と同様、選択行も背景・文字を自前で
`FillRect`/`DrawText`し、`CDRF_SKIPDEFAULT`を返す方式に変更して解決した。

### 7. `NM_CUSTOMDRAW`の`CDDS_POSTPAINT`直接描画は原因不明のまま断念

固定表示ヘッダーを「別ウィンドウを重ねる」方式ではなく「リスト自身のDCに
`CDDS_POSTPAINT`で直接重ね描きする」方式に切り替えれば、種別区切り行と全く
同じ描画コードを再利用でき、位置ズレやスクロール時のゴミ残りを構造的に
解消できるはずだった。`CDRF_NOTIFYPOSTPAINT`を返すよう変更し実装したが、
この環境では**診断用の単色塗り(`FillRect`でRGB(0,255,0)の全面塗り)すら
画面に反映されず**、`PrintWindow`(`PW_RENDERFULLCONTENT`)で強制キャプチャ
しても効果が見えなかった。原因を切り分けきれなかったため、この方式は
断念し、以前から動作確認済みだった「STATICコントロールを重ねる」方式に
戻した。

### 8. インタラクティブな選択変更時、右端まで塗った色がクリップされて残ることがある

`UpdateList()`によるリスト全体の作り直し(全体が再描画対象になる)では
発生しないが、マウスクリック等での選択変更(`LVN_ITEMCHANGED`)では、
Windowsが項目本来の(列幅ぶんの)矩形しか再描画対象にせず、`NM_CUSTOMDRAW`で
`rcItem.right = rcClient.right`として拡張して塗っている右側(スクロール
バー際まで)がクリップされて古い状態のまま残ることがあった。
`LVN_ITEMCHANGED`で選択状態が変化するたびにリスト全体を明示的に
`InvalidateRect`する対処を入れた。あわせて`LVS_EX_DOUBLEBUFFER`も追加し、
オフスクリーン合成後の一括転送でちらつき・タイミングずれを抑えている。

### 9. 列見出し(SysHeader32)だけテーマが残ると文字が欠けて見える

リスト本体は`PreventVisualStyle()`でテーマを無効化しているが、内蔵の
列見出しコントロールは対象外だったため、行の高さ計算(テーマ有効時の計算)と
テーマの描画パディングが食い違い、「機能名」ヘッダーの文字下端が欠けて
見える不具合があった。見出し側にも`PreventVisualStyle()`を適用し、
`HDM_LAYOUT`(`Header_Layout`)で正しい高さを取り直して`SetWindowPos`で
反映することで解消した。なお`HDS_FLAT`(見出しの立体的な縁取りを消す
スタイル)も試したが、文字位置が2pxほど左にずれる副作用があったため
不採用とした。

### 10. `EM_SETCUEBANNER`は文字色を指定できない

絞り込み欄のヒント文字を薄いグレーで出したかったが、`EM_SETCUEBANNER`は
文字列は指定できても色はシステム既定のまま変えられない。`SetWindowSubclass`
でEditをサブクラス化し、未入力かつ非フォーカス時は`WM_PAINT`を自前で
処理してヒント文字を任意の色で描画する方式に切り替えた。

### 11. 種別区切り行と固定表示オーバーレイの色が食い違って見える

両者とも同じ`COLOR_BTNFACE`から計算する式(`-70`してクランプ)を使っていたが、
見た目上ズレて見えるとの指摘を受けた。動的計算をやめ、どちらも
`RGB(170, 170, 170)`の固定値を使うよう統一した。

## 既知の制限・要検証事項

- スクロールで新たに画面内へ入ってくる選択行の描画タイミングのズレ
  (「非表示から表示になった瞬間、色が一瞬遅れる」)について、
  `LVS_EX_DOUBLEBUFFER`と選択変更時の明示的`InvalidateRect`で緩和を
  試みたが、完全な解消は未確認。根本対応にはリスト全体を
  `LVS_OWNERDRAWFIXED`にして全行を常に自前描画する、より大きな作り
  直しが必要になる可能性がある。
- 本機能の開発中、色・位置のピクセル単位の自動検証(`GetPixel`/
  `PrintWindow`によるキャプチャ)が同一箇所で複数回異なる値を返すなど
  不安定な場面が何度かあった。コード自体のレビューでは問題が見当たら
  ない場合でも、最終的な見た目の確認はユーザーの目視に頼っている
  部分がある。
- 貼り付け(F9, Shift+Ins, Ctrl+V)のように1機能に複数キーを割り当てる
  運用は「追加」ボタンで個別にキーを積み増すことで実現する設計とした
  (1機能1キーに制限する案は検討の末に不採用)。

## 動作確認について

Release/x64でのビルド成功を都度確認しつつ実装した。UI自動操作
(`SendMessage`によるタブ切り替え・`LVM_*`メッセージでのリスト操作・
`GetPixel`/`PrintWindow`でのキャプチャ)で、カテゴリ列挙・絞り込み・
追加/解除・選択色・スティッキーヘッダーの追従といった主要な機能・
描画は検証済み。マウスクリック等の実インタラクションを安全に自動化
する手段がこの環境には無い(ハングのリスクがあるため断念)ため、
インタラクティブな選択変更に伴う描画タイミングの問題を含む一部の
見た目については、ユーザー自身による実機確認に依っている。
