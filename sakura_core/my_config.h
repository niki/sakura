// -*- mode:c++; coding:utf-8-ws -*-
#ifndef MY_CONFIG_H
#define MY_CONFIG_H

#pragma warning(disable : 4244) // 型変換による警告
#pragma warning(disable : 4267) // 型変換による警告
#pragma warning(disable : 26495) // 未初期化による警告

// clang-format off

//
// 〆 … 削除候補
//

/*
●やりたいこと.
- [v] 20170722@20260801 同じファイルのウィンドウを複製する
  → NKMM_FIX_TAB_DUPLICATE で対応。F_TAB_DUPLICATE。
    タブメニュー(共通設定→カスタムメニュー→タブメニュー)に既定で追加済みなので
    タブ右クリックからも呼べる。

- [v] 20150702 開いているタブのファイル名をコピー

- [ ] 20170110 bug?, 画面上端下端でキャレットが消えたタイミングでスクロールさせるとキャレットが消えたままスクロールする

- [v] 20150605@20260801 bug, カーソル移動したときに移動前の状態が一瞬残る
  \view\CEditView_Scroll.cpp:void CEditView::ScrollDraw() があやしい?
  ScrollWindowEx() で行われる更新をなんとかすればいい?
  → NKMM_FIX_FLICKER で対応済み(UpdateWindow()呼び出しタイミングの一括化)。
    その後のカーソル移動高速化もあって体感できないレベルになったため完了扱い。

- [v] 20150804@20260729 タスクバーアイコンのちらつき
  \window\CTabWnd.cpp:CTabWnd::ShowHideWindow()
  SendMessageTimeout() と TabWnd_ActivateFrameWindow() の関係
  → NKMM_TABWND_SYNC_HIDE で対応。新ウィンドウ表示直後に旧ウィンドウを同期的に隠すよう修正

- [ ] 20170303 テキスト描画, 文字の右端が欠ける, ExtTextOutのタイミング
  [patchunicode:#588]をあてて目立たなくはしている, 続けて描画されれば欠けない
  [patchunicode:#860]をあてれば解消しそう
    sakura_core\view\CEditView_Paint.cpp
    pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);

- [ ] 20170404 BkSpを押したときにタブ入力文字だけしかない場合は逆TABにする

- [X] 20170607 #RRGGBB を色付け

- [ ] 20170611 空白タブ改行行番号の表示切替

- [v] 20170611@20170620 Grep, Exclude dirsの追加、検索時に # をつけて「ファイル」につなげる
  Grep, 「$cpp」を「*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp」などに展開する機能
*/

// ● フォント
//   https://support.microsoft.com/ja-jp/kb/74299
//   http://d.hatena.ne.jp/itoasuka/20100104/1262585983
//
// lf.lfHeight = DpiPointsToPixels(-10); // 高DPI対応（ポイント数から算出）

// 修正者
#define NKMM_AUTHOR       "Yu-zuki."
#define NKMM_AUTHOR_PAGE  "https://github.com/niki/sakura"

// 拡張用レジストリキー
#define NKMM_REGKEY _T("Software\\sakura-niki")

//------------------------------------------------------------------
// バージョン情報ダイアログの変更 20170315
//------------------------------------------------------------------
#define NKMM_FIX_VERDLG
	#define PR_VER      2,3,2,260810	// 2.4系に倣う
	#define PR_VER_STR "2.3.2.260810"
	#define PR_VER_VAL	2320
	#define PR_LV		260810
//	#define BASE_REV    4205  // このSVNのリビジョンを最後に修正を加えています

//-------------------------------------------------------------------------
// デバッグ出力 20150324
//-------------------------------------------------------------------------
//#define NKMM_OUTPUT_DEBUG_STRING

//------------------------------------------------------------------
// スクロール
//------------------------------------------------------------------
#define NKMM_FIX_SCROLL
	// 水平スクロールの変更
	//  - スクロール開始マージンを 1 に変更。画面の端でスクロール開始 20140507
	//  - スクロール幅を 16 に設定。一度に大きく移動することで見やすくする (Imitate 'Notepad') 20150902
	#define NKMM_HORIZONTAL_SCROLL_MARGIN (16)
	// 垂直スクロールの変更
	//  - スクロールマージンを 0 に変更 20170409
	#define NKMM_VERTICAL_SCROLL_MARGIN (0)

//------------------------------------------------------------------
// カーソルが大きく移動する処理ではカーソル行をセンタリングする 20170413
//  (行番号ジャンプやタグジャンプ、アウトライン解析から移動した場合など)
//  - 同一画面内の移動はセンタリングしない
//  ! CCaret::MoveCursor, 同期スクロールの対応がまだ
//------------------------------------------------------------------
#define NKMM_FIX_CENTERING_CURSOR_JUMP

//------------------------------------------------------------------
// カーソルの移動（1行目で↑で左端移動、最終行で↓で右端移動）
//------------------------------------------------------------------
//#define NKMM_FIX_CURSOR_MOVE_TOPEND

//------------------------------------------------------------------
// タブ入力文字の切り替え(タブ<->空白)を追加
//  - S_ChangeTabWidthマクロ修正, 負の値を渡すとタブ文字の切り替え
//------------------------------------------------------------------
#define NKMM_FIX_CHANGE_TAB_WIDTH_MACRO

//------------------------------------------------------------------
// UIフォント
//  - "ＭＳ Ｐゴシック" -> "MS Shell Dlg" or "MS Shell Dlg 2"
//  - .rcファイルは数が多いので置換対応
//  - Vista以降ではMS Shell Dlgを使わず直接Fontを指定したほうがいいらしい
//------------------------------------------------------------------
#define NKMM_FIX_UI_FONT
	#if (WINVER >= _WIN32_WINNT_VISTA)
		#define NKMM_RES_FONT_NAME "MS Shell Dlg"
	#else
		#define NKMM_RES_FONT_NAME "MS Shell Dlg"
	#endif // NKMM_

//------------------------------------------------------------------
// SetMainFontにポイントのオフセット引数を追加 20170622
//------------------------------------------------------------------
#define NKMM_FIX_SETMAINFONT

//------------------------------------------------------------------
// タブ名カラー
//------------------------------------------------------------------
#define NKMM_FIX_TAB_CAPTION_COLOR
	// 変更ドキュメントタブ名カラー (REG/TabCaptionModifiedColor:#0000d7) 20170322
	#define NKMM_MODIFIED_TAB_CAPTION_COLOR _T("#0000d7")
	// マクロ記録中ドキュメントタブ名カラー (REG/TabCaptionRecMacroColor:#d80000) 20170328
	#define NKMM_RECMACRO_TAB_CAPTION_COLOR _T("#d80000")

//------------------------------------------------------------------
// タブウィンドウ 20150828
//  - タブをダブルクリックで閉じる 20170406 - 20170407
//  - ウィンドウが非アクティブなどきに非アクティブタブを選択したらそのタブをアクティブにする 20170413
//  - WM_LBUTTONDOWN でウィンドウをアクティブにするようにする (修正前は WM_LBUTTONUP) 20170414
//    この修正の影響で非アクティブウィンドウのドラッグができなくなった (対応予定)
//  - タブのクリックにドラッグ移動量のしきい値を追加 20260718
//    アクティブなタブをクリックした際、しきい値チェックが無かったため、
//    ボタン押下から離すまでのわずかなカーソル移動だけでドラッグ開始(タブの並び替え)と誤認識され、
//    タブを切り替えようとしただけなのにタブの並びが意図せず入れ替わってしまう不具合があった。
//    システム標準のドラッグしきい値(SM_CXDRAG/SM_CYDRAG)を超えるまではドラッグ開始とみなさないようにして修正。
//  ? 各ウィンドウのタブウィンドウは生成時に自身の位置が選択されている状態から始まる
//    オーダーが変わらない限り選択タブが変わることはない
//    ウィンドウ切り替え時に自身が選択されたタブウィンドウが表示されることでタブを切り替えたように表現しているだけ
//    そのためスクロール状態からの切り替え時にスクロール位置が同期していない
//------------------------------------------------------------------
#define NKMM_FIX_TABWND
	#define NKMM_TABWND_FLICKER     (1)  // ウィンドウまとめモードの切り替え時にスリープを10ms入れる(ちらつき抑制) 20170406
	                                      // → 20260807 UpdateWindow(hwnd)がhwnd自身しか同期再描画せず、
	                                      //    エディットビュー等の子ウィンドウのWM_PAINTや、タイトルバーの
	                                      //    WM_NCPAINTがキューに残るため、可視化直後の一瞬だけ中身が
	                                      //    描画されない・タイトルバーが白くなる問題を追加修正。
	                                      //    RedrawWindow(..., RDW_ALLCHILDREN | RDW_FRAME)で子ウィンドウと
	                                      //    非クライアント領域も同期再描画。
	                                      // → 20260807 上記だけではタイトルバーの白フラッシュが確率的に
	                                      //    残った（DWM合成側の遷移アニメーションが原因で、アプリ側の
	                                      //    同期描画だけでは防げない）。切り替え中だけ
	                                      //    DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED, TRUE)
	                                      //    でDWMの遷移アニメーションを止めるよう追加修正。
	                                      // → 20260807 それでもタイトルバー/メニュー/タブが毎回ちらつく
	                                      //    (確率的でなく再現性あり)と報告あり。CEditWnd::OnSize2()の
	                                      //    再レイアウトをWM_SETREDRAWで抑止する修正もあわせて実施。
	                                      // → 20260807 上記でも直らず。OutputDebugString相当のログを
	                                      //    仕込んで実際のメッセージ列を採取したところ、切替のたびに
	                                      //    AddEditWndList()(CAppNodeManager.cpp)がMRU(最近アクティブ)
	                                      //    リストの並べ替えとしてTWNT_ORDERをグループ全員へ
	                                      //    ブロードキャストしており、CTabWnd::TabWindowNotify()の
	                                      //    TWNT_ORDERハンドラがTabCtrl_SetCurSel(m_hwndTab,nScrollPos)→
	                                      //    TabCtrl_SetCurSel(m_hwndTab,nIndex)と選択状態を2段階で
	                                      //    同期変更していたのが真因と判明（スクロール位置を強制的に
	                                      //    リセットしてから目的タブを選択するための実装だが、その間の
	                                      //    「誤った選択状態」が毎回一瞬そのまま画面に出ていた）。
	                                      //    この2段階の選択変更をWM_SETREDRAWで挟んで抑止し、
	                                      //    最後に一度だけRedrawWindow()で描き直すよう修正。
	                                      //    タイトルバー/メニューのちらつきは、可視化に伴う一連の処理の
	                                      //    体感速度が上がったことで別要因(DWM等)が目立たなくなった。
	                                      // → 20260807 残っているごく短時間(1〜2フレーム程度)の空白は、
	                                      //    RedrawWindow(RDW_UPDATENOW)がGDI描画をアプリ側で完了させる
	                                      //    だけで、DWMがそれを実際に画面へ合成・提示するのを待たない
	                                      //    ことが一因。直後のSleep(10)は「間に合うだろう」という
	                                      //    時間当てずっぽうでしかないため、DWMが次のフレームの
	                                      //    合成・提示を終えるまで実際に待つDwmFlush()に置き換えた
	                                      //    （DwmFlush()が使えない環境向けにSleep(10)はフォールバックとして残す）。
	                                      // → 20260807 DwmFlush()に変えても体感が変わらないと報告あり。
	                                      //    ログに再度タイムスタンプを仕込んで確認したところ、
	                                      //    マウスでタブをクリックした場合はCTabWnd::ShowHideWindow()
	                                      //    経由で上記の修正が効くが、Ctrl+Tab等(F_NEXTWINDOW/
	                                      //    F_PREVWINDOW)によるタブ切替はCControlTray::ActiveNextWindow/
	                                      //    ActivePrevWindow() → util/window.cpp の ActivateFrameWindow()
	                                      //    という別経路を通ることが判明。この関数はShowHideWindow()と
	                                      //    違って新ウィンドウ表示後に旧ウィンドウを同期的に隠す処理が
	                                      //    無く、AddEditWndList()発のTWNT_ORDER通知の非同期往復待ちの
	                                      //    ままだった（NKMM_TABWND_SYNC_HIDEが2026.07.29に対応したのは
	                                      //    ShowHideWindow()側だけで、ActivateFrameWindow()は未対応だった）。
	                                      //    ActivateFrameWindow()にも同じ同期非表示処理
	                                      //    （HideOtherGroupWindows()、util/window.cpp）を追加した。
	                                      // → 20260807 あわせて、タブ項目はTCS_OWNERDRAWFIXEDでWM_DRAWITEMが
	                                      //    毎回全面を塗りつぶすため不要なはずのデフォルトWM_ERASEBKGND
	                                      //    （erase→redrawの二度塗り）をCTabWnd用のサブクラスプロシージャ
	                                      //    (TabWndProc)で無効化した。
	                                      // → 20260807 【未解決】Ctrl+Tab切替では、上記修正後もログ上は
	                                      //    ShowHideWindow()/TWNT_ORDER関連の処理が一切発生していない
	                                      //    （AddEditWndList()はWM_ACTIVATEAPPからのみ呼ばれ、同一デスクトップ
	                                      //    内のプロセス間フォーカス移動でも今回のテストでは発火しなかった）
	                                      //    にもかかわらず、可視化から約100〜150ms後の1〜2フレーム
	                                      //    (約15〜30ms)だけタブ帯が一瞬空白/欠落するちらつきが
	                                      //    高速連写キャプチャで再現し続けている。マウスクリックでの
	                                      //    切替（TWNT_ORDER経由）よりこちらの方が症状が大きい。
	                                      //    RedrawWindow/DwmFlush/WM_ERASEBKGND抑止のいずれでも解消せず、
	                                      //    原因はまだ特定できていない。要追加調査。
	                                      // → 20260807 ユーザーより「白くなるのと同時にウィンドウの影が
	                                      //    濃くなる」との報告。影の濃淡はDWMの非クライアント領域の
	                                      //    アクティブ/非アクティブ描画（WM_NCACTIVATE相当）に連動する
	                                      //    ため、白フラッシュはSetForegroundWindow()によるアクティブ化の
	                                      //    タイミングで起きている可能性が高いと判明。ところが、これまでの
	                                      //    DwmSetWindowAttribute(DWMWA_TRANSITIONS_FORCEDISABLED)は
	                                      //    CTabWnd::AdjustWindowPlacement()内だけで有効→無効を完結させて
	                                      //    おり、SetForegroundWindow()（呼び出し元のShowHideWindow()/
	                                      //    ActivateFrameWindow()側で、AdjustWindowPlacement()の後に呼ばれる）
	                                      //    の時点では既に遷移アニメーションが元に戻ってしまっていた＝
	                                      //    保護範囲から漏れていたことが真因の可能性が高い。
	                                      //    DWM無効化・最終RedrawWindow・DwmFlushによる合成待ちを
	                                      //    AdjustWindowPlacement()単体からShowHideWindow()/
	                                      //    ActivateFrameWindow()側に引き上げ、「可視化開始～
	                                      //    SetForegroundWindow()によるアクティブ化完了」までを
	                                      //    一括で保護するよう修正。
	#define NKMM_TAB_CLOSE_BTN_DRAW (0)  //〆 タブを閉じるボタンをグラフィカルにする 20170423
	#define NKMM_TABWND_DRAG_THRESHOLD (1)  // タブクリックがドラッグ移動としきい値なしで誤認識され、切替えただけでタブの並びが入れ替わる問題を修正 20260718
	#define NKMM_BUGFIX_TAB_EDGE    (1)  // 間に選択タブがあると右側のエッヂがないバグを修正 (となりのタブが上書き描画していた) 20170429
	#define NKMM_TABWND_SYNC_HIDE   (1)  // タブ切替時、旧ウィンドウを隠す処理がTWNT_ORDER通知の非同期往復待ちになっており、新旧ウィンドウが重なって見えるちらつきの原因だったため、ウィンドウ表示直後に同期的に隠すよう修正 20260729
	#define NKMM_TAB_CURRENT_LINE   (1)  // カレントタブの下部に線を引いて選択中のタブを分かりやすくする (#1a73e8) 20260807
	                                      // → 20260807 固定色(#1a73e8)ではなくWindowsのアクセントカラーを
	                                      //    使うよう変更。DwmGetColorizationColor()（dwmapi.h、Vista以降の
	                                      //    素のWin32 API）で取得する。Windows 7/8/8.1/10/11いずれでも
	                                      //    動作し、Win10/11の「設定→個人用設定→色」のアクセントカラー
	                                      //    （背景から自動的に選択する設定時も含む）とも連動する。
	                                      //    WinRTのUISettings.GetColorValue(Accent)を使う手もあるが、
	                                      //    Windows 8以降専用でCOM初期化も要るため、互換性・実装コストの
	                                      //    両面でDwmGetColorizationColor()を採用。Windows 7の
	                                      //    ClassicテーマなどDWM合成が無効な環境ではAPIが失敗しうるため、
	                                      //    GetSysColor(COLOR_HIGHLIGHT)にフォールバックする。
	                                      //    WM_DWMCOLORIZATIONCOLORCHANGED受信時に再取得・再描画する。

//------------------------------------------------------------------
// エディット画面 スクロールバー
//  - 検索結果を表示 20170609
//  - ブックマークを表示 20170609
//  - カーソル行を表示 20170611
//  - スクロールバーの再描画をマーカー描画のタイミングに合わせて更新する 20170721
//  ? バーにカーソルを乗せた時, フェードアウトして消えてしまう:(
//  - 水平スクロールバーのSetScrollInfoスキップ判定がテキスト幅の縮小を見て
//    おらず更新漏れが起きる不具合を修正。実機確認済み。詳細はchangelog/
//    NKMM_FIX_EDITVIEW_SCRBAR_HWIDTH_SKIP.md参照 20260806
//  - SB_Marker_BuildThread(バックグラウンドの検索ヒット行スキャン)とCEditView::
//    ChangeCurRegexp()の競合クラッシュを修正 20260809
//    - SB_Marker_BuildThreadはIsFoundLine()経由でCEditView::m_sSearchPattern
//      (Sunday-Quickスキップ配列を保持)をバックグラウンドスレッドから読む。
//      検索文字列入力のたびにChangeCurRegexp()がUIスレッドでm_sSearchPattern.
//      SetPattern()を呼び、内部でReset()して配列を解放してから再構築するため、
//      この2つが競合するとNULL/解放済みポインタを読んでクラッシュする。
//      500万行ファイルへのインクリメンタル検索(NKMM_FIX_FIND_DIALOGのデバウンス
//      化で連続検索が速くなったことで発生頻度が上がった)で実機クラッシュを確認・
//      WinDbgのダンプ解析で原因特定済み(CSearchAgent::SearchStringでの
//      アクセス違反、スタックはSB_Marker_BuildThread→IsFoundLine→
//      IsSearchString→SearchString)。ChangeCurRegexp()がm_sSearchPatternを
//      書き換える直前にSBMarker_->WaitForBuild(true)で実行中のビルドスレッドを
//      中断・待機するよう修正(ReplaceAll前のCSuppressSrchKeyMarkForReplaceAll
//      と同じ考え方)。
//    - sakura_core\view\CEditView_Command.cpp (ChangeCurRegexp)
//  - WM_APP_SCRBAR_PAINTハンドラのブロッキング待ちを解消 20260809
//    - 上記の競合修正でビルドスレッドの再始動頻度が上がったことで、ビルド中に
//      スクロールすると、スクロールが発生させる描画要求のたびに
//      SBMarker_->WaitForBuild(false)(ビルド完了までブロック)に引っかかり、
//      スクロールバー(実質画面全体)の更新が止まって見える不具合が顕在化した。
//      実機確認済み。ビルド中はこの描画要求を待たずに諦めるよう変更。ビルド完了時に
//      スレッド自身がSB_Marker_DrawRequest()で再描画を要求してくるので、それに
//      任せれば十分(そもそもブロックして待つ必要がない設計だった)。
//    - sakura_core\view\CEditView.cpp (WM_APP_SCRBAR_PAINTハンドラ)
//  - 通常編集とSB_Marker_BuildThreadの競合防止・ビルドの並列化 20260809
//    - CSuppressSrchKeyMarkForReplaceAllはReplaceAll専用のガードで、通常の
//      タイプ入力・貼り付け・削除中にSB_Marker_BuildThreadが巨大ファイルを
//      走査していた場合の競合(CDocLineMgrへの読み取り中の書き換え)は未対策
//      だった。編集の唯一の合流点CEditView::ReplaceData_CEditView3の先頭で
//      SBMarker_->WaitForBuild(true)を呼び、編集前に確実に待避させるよう修正。
//    - 「折り返しなし」かつ正規表現でない場合に限り、SB_Marker_BuildThreadの
//      走査ループをOpenMPで並列化(SB_Marker_DrawThreadと同じ手法)。折り返し
//      ありはLogicToLayout()の共有ヒントキャッシュ、正規表現はCEditView::
//      m_CurRegexpという単一の共有エンジンインスタンスがあり、どちらも複数
//      スレッドから同時に触ると競合するため対象外(既存の逐次パスにフォール
//      バック)。500万行ファイルでのスクロールバーマーク再構築(検索文字列
//      入力のたびに再始動する)を高速化する目的。
//    - sakura_core\view\CEditView_Command_New.cpp (ReplaceData_CEditView3),
//      sakura_core\view\CEditView.cpp (SB_Marker_BuildThread)
//  - SB_Marker_DrawThreadで同じY座標への重複描画をスキップ 20260809
//    - スクロールバーの高さはせいぜい数百〜千数百pxしかないのに対し、ヒット数が
//      数万〜数十万件(500万行ファイルでよくある文字列を検索した場合等)になると、
//      大半のヒットが直前と同じピクセル行へ描画することになり、GDIのFillRect
//      呼び出しが無駄に大量発生して描画が極端に遅くなっていた。実機確認済み。
//      行番号順に処理しているためY座標はほぼ単調増加であり、直前と同じYなら
//      描画をスキップするよう変更(見た目の結果は同じで、無駄な描画回数だけ減る)。
//    - sakura_core\view\CEditView.cpp (SB_Marker_DrawThread)
//  - キャッシュ作成/描画をスレッドプール(PTP_WORK)化 20260810
//    - SB_Marker_BuildThread/SB_Marker_DrawThreadは_beginthreadexで編集/描画の
//      たびにOSスレッドを新規生成しており、生成コストに加えて起動直後に
//      ::Sleep(10)で呼び出し元(UIスレッド)を固定10msブロックしていた。編集の
//      たびにこの経路を通ると1回あたり最低10ms以上かかる。詳細は
//      changelog/NKMM_FIX_EDITVIEW_SCRBAR_THREADPOOL.md参照。
//    - CreateThreadpoolWork()でビルド用/描画用のPTP_WORKをScrBarMarker生成時に
//      1個ずつ作成し、以降はSubmitThreadpoolWork()で使い回す。スレッド生成が
//      無くなるため、Sleep(10)による head-start待ちも不要になった。
//    - ビルド実行中に来た再構築要求は、旧実装のようにWaitForBuild(true)で
//      UIスレッドをブロックして強制中断・作り直す代わりに、bRebuildPending_
//      フラグを立てるだけにした。BuildWorkCallback側はdo-whileで完了直後に
//      このフラグを確認し、立っていればもう一度最新のドキュメント状態で
//      スキャンし直す(取りこぼし防止、UIスレッドは常にノンブロッキング)。
//    - WaitForBuild/WaitForDrawはWaitForSingleObject(ハンドル)から
//      WaitForThreadpoolWorkCallbacks(work, fCancelPendingCallbacks)に置き換え。
//      abort=trueの場合は未開始分をキャンセルしつつ実行中分の完了を待つ点は
//      旧実装(中断フラグ+INFINITE待ち)と同じ。
//    - CEditView単位(=分割ウィンドウ/タブ単位)でスレッドを常駐させる案も検討
//      したが、開いているウィンドウ数だけスレッドが増える(既知の未解決事象、
//      NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md追記6参照)ため見送り、
//      プロセス既定のスレッドプールを共有する方式にした。
//    - m_CurRegexp共有競合(NKMM_FIX_SCRBAR_MARKER_REPLACEALL_PERF.md参照)とは
//      無関係な変更のため、その対策(CSuppressSrchKeyMarkForReplaceAll等)は
//      そのまま維持している。
//    - sakura_core\view\CEditView.h (ScrBarMarker),
//      sakura_core\view\CEditView.cpp (BuildWorkCallback/DrawWorkCallback/
//      Build/Draw/WaitForBuild/WaitForDraw/コンストラクタ/デストラクタ)
//  - bRebuildPending_取りこぼしレースの修正(mutex導入) 20260810
//    - コードレビューで、BuildWorkCallback終了処理の「pending確認→running解除」の
//      間にBuild()が割り込むと、その時に立てたbRebuildPending_を誰も消費せず
//      再構築要求が黙って失われるTOCTOUレースがあることに気付いた。
//    - mtxBuildState_(std::mutex)を新設し、Build()側の「running確認→pending/
//      running設定」とBuildWorkCallback側の「pending確認→pending消費 or
//      running解除」を、それぞれ同じロックの中で不可分に行うよう修正。
//    - 描画側(SB_Marker_DrawThread由来のbRestartRequestDrawThread_/
//      bDrawThreadRunning_)にも理屈上は同種のレースが残っていたため、
//      mtxDrawState_として同じ対策を追加(下記の別項参照)。
//    - sakura_core\view\CEditView.h (mtxBuildState_),
//      sakura_core\view\CEditView.cpp (Build/BuildWorkCallback)
//  - 描画側(bRestartRequestDrawThread_)取りこぼしレースの修正(mutex導入) 20260810
//    - 上記と同じ種類のレースがDraw()/DrawWorkCallbackにも存在した(移植元の
//      SB_Marker_DrawThreadから変更していない既存ロジックだが、同じ対策を転用)。
//    - mtxDrawState_(std::mutex)を新設し、Draw()側の「running確認→restart/
//      running設定」と、DrawWorkCallback側(描画が完走してend_threadへ
//      フォールスルーする直前)の「restart確認→pending消費 or running解除」を、
//      それぞれ同じロックの中で不可分に行うよう修正。中断(bExitRequestDrawThread_)
//      経路はWaitForDraw(true)がUIスレッドをブロックするためレースの心配が無く、
//      対象外。
//    - sakura_core\view\CEditView.h (mtxDrawState_),
//      sakura_core\view\CEditView.cpp (Draw/DrawWorkCallback)
//------------------------------------------------------------------
#define NKMM_FIX_EDITVIEW_SCRBAR
	#define WM_APP_SCRBAR_PAINT    (WM_APP + 2501)  // スクロールバー描画メッセージ
	#define WM_APP_SCRBAR_ENDPAINT (WM_APP + 2502)  // スクロールバー描画終了メッセージ
	#define NKMM_SCRBAR_FOUND_MAGIC (0x10000000)  // 検索
	#define NKMM_SCRBAR_MARK_MAGIC  (0x20000000)  // ブックマーク
	#define NKMM_SCRBAR_LINEN_MASK  (0x0fffffff)  // 行番号マスク
	#define NKMM_SCRBAR_MAGIC_MASK  (0xf0000000)  // マジックマスク
	// 検索文字列のある行の色 (REG/EditViewScrBarFoundColor:#0000d7)
	#define NKMM_SCRBAR_FOUND_COLOR  _T("#f4a721") //_T("#32CD32") //_T("#0000d7")
	// ブックマークのある行の色 (REG/EditViewScrBarMarkColor:#d80000)
	#define NKMM_SCRBAR_MARK_COLOR   _T("#ff0032") //_T("#ff0000") //_T("#d80000")
	// キャレットのある行の色 (REG/EditViewScrBarMarkColor:#d80000)
	#define NKMM_SCRBAR_CURSOR_COLOR _T("#0026ff") //_T("#0000cd") //_T("#00d800")

	#define NKMM_EDITVIEW_H_SCRBAR_REDRAW_TIMING  (1)  // 水平スクロールバーの更新タイミングを修正
	// システム(Explorer)風の細いスクロールバーにする 20260717
	// SetWindowTheme(hwnd, L"Explorer", NULL) を適用する。テーマ無効環境では従来通りの見た目にフォールバックする
	#define NKMM_SCRBAR_SYSTEM_STYLE (1)

//------------------------------------------------------------------
// 行間を上揃えではなく下揃えにする
//  - デフォルトでは行は上揃えになっているので行間は下に付加される
//  - キャレットを行間含む高さにする (カーソル行アンダーラインが表示されている場合は交差箇所が消せないので通常処理)
//------------------------------------------------------------------
#define NKMM_LINE_MARGIN_TOP
	#define NKMM_LINE_MARGIN_TOP_WITH_CARET_HEIGHT (1)  // キャレットの高さを行の高さにする

//------------------------------------------------------------------
// キャレットの幅を入力タイプで変更する (半角:1px, 全角:2px)
//------------------------------------------------------------------
//〆 #define NKMM_FIX_CARET_WIDTH

//------------------------------------------------------------------
// カーソル行の行番号背景を「カーソル行の背景色」で描画する 20171002
//------------------------------------------------------------------
#define NKMM_FIX_CUR_BACK_DRAW

//------------------------------------------------------------------
// EOFのみの行にも行番号を表示 20170310
//------------------------------------------------------------------
#define NKMM_FIX_EOFLN_DISP_NR

//------------------------------------------------------------------
// 半角空白文字
// - 半角空白文字を '・' で描画 (Imitate 'Sublime Text') 20130602
// - Non-Breaking-SPaceを半角空白として表示する 20170415
//------------------------------------------------------------------
#define NKMM_FIX_HAN_SPACE

//------------------------------------------------------------------
// タブ文字（矢印）の鏃(>)は表示しない (Imitate 'Sublime Text') 20150525
//  - 「長い矢印」「短い矢印」→「線」 20160819
//    -> タブ表示の文字指定廃止, 表示は線のみ 20170329
//------------------------------------------------------------------
#define NKMM_FIX_TAB_MARK

//------------------------------------------------------------------
// 行番号表示切替マクロ (S_SwitchDispLineNumber()) 20180110
// - Funccode_x.hsrcを修正
//------------------------------------------------------------------
#define NKMM_FIX_SWITCH_DISP_LINENR_MACRO

//------------------------------------------------------------------
// カーソル行アンダーライン
//  - 行番号を含む左端から 20150130
//------------------------------------------------------------------
#define NKMM_FIX_CUR_UNDERLINE

//------------------------------------------------------------------
// 選択領域の色
//  - テキストと背景のブレンド率設定 20150605
//  - 選択時のテキスト属性（太字、下線）に選択領域ではなく現在のテキストを使用する
//  - カラー設定は背景カラーのみ
//------------------------------------------------------------------
#define NKMM_FIX_SELAREA
	#define NKMM_SELAREA_TEXT_BLEND_PER (0)    // 選択領域のブレンド率[%] (REG/SelectAreaTextBlendPer:0x00000000) 20150605
	#define NKMM_SELAREA_BACK_BLEND_PER (100)  // 選択範囲のブレンド率[%] (REG/SelectAreaBackBlendPer:0x00000064)
	#define NKMM_SELAREA_BACK_BLEND_PER2 (60)  // 特定の下地のときの選択範囲のブレンド率 (REG/SelectAreaBackBlendPer2:0x0000003C)

//------------------------------------------------------------------
// 空白,TAB,改行,EOF,ノート線のカラー (Imitate 'Sublime Text') 20150605
//  - 現在のテキスト色と現在の背景色をブレンドする (空白TABのカラー設定は無効化されます) 20150608
//    対象は空白TABなどで、コントロールコードには適用されません
//  - 空白,タブ,改行の色は他のカラー設定の影響を受けます
//------------------------------------------------------------------
#define NKMM_FIX_WS_COLOR
	#define NKMM_WS_BLEND_PER (30)  // 空白,TAB,改行,EOF,ノート線 現在のカラーのブレンド率[%] (REG/WhiteSpaceBlendPer:0x0000001E) 20150605
//						     //
						     //

//------------------------------------------------------------------
// カラー設定 20160625
//  - カーソル位置縦線 テキストカラーのみ
//  - 折り返し記号 テキストカラーのみ
//------------------------------------------------------------------
#define NKMM_FIX_COLOR_STRATEGY

//------------------------------------------------------------------
// コメント行 20161227
//  - 改行以降もコメントカラーを有効にする (Imitate 'Sublime Text')
//------------------------------------------------------------------
#define NKMM_FIX_COMMENT

//------------------------------------------------------------------
// 数値の色付け判定
//  - 正規表現で判定する 20170421
//  - 判定に使う正規表現エンジンはCColor_Numeric.cppのREGEX_MODEで切り替え可能
//    (0:std::regex 1:boost::regex 2:BREGEXP(bregonig.dll) 3:PCRE2)
//    3:PCRE2はNKMM_FIX_REGEXP_FALLBACKのフォールバックエンジンをbregonig.dllの
//    有無に関わらず直接使う設定 20260720
//  - 無効化: IsNumber()の呼び出し元(CColor_Numeric::BeginColor)は画面内の
//    数値になりうる全位置に対して高頻度で呼ばれるが、REGEX_MODE==2/3の
//    呼び出し方はCRegexFallback::BMatchExに毎回str!=nullptrで渡ってしまうため
//    6パターン全てを呼ぶたびにゼロからコンパイル(+JIT)し直し、かつ
//    ループ1周ごとに直前のコンパイル結果(BREGEXP_W_Fallback、JIT実行可能
//    メモリ含む)を解放せず上書きしてリークする(CRegexKeyword.cppのように
//    1回コンパイルして使い回す設計になっていない)。この呼び出しパターンの
//    まま速くしようがない上、固定・小規模な文法には元々あった手書きの
//    文字ループ(CColor_Numeric.cppの#else節)で十分かつ確保/解放が一切ない
//    ため、正規表現版を無効化して文字ループ版に戻す 20260728
//  - 削除: 上記の理由で復活の見込みがないため、REGEX_MODE一式(std::regex/
//    boost::regex/BREGEXP/PCRE2の4エンジン切り替え)を含めてコード自体を削除。
//    削除前の実装全文はchangelog/NKMM_FIX_NUMERIC_COLOR.mdに退避 20260806
//------------------------------------------------------------------

//------------------------------------------------------------------
// ステータスバー 20150610 - 20170401, 20170611
//  - ちらつき抑制 (スクロール時)
//  - カラムの並べ替え
//  - 左クリックでモード切り替えメニューを表示
//  - タイプ名を表示 (左クリック: メニュー表示)
//  - タブサイズを表示 (左クリック: メニュー表示)
//  - 入力改行コードを主に使われているシステム名で表記
//  - ファイル名を表示 (Ctrl+左クリック: ファイルの場所を開く, 右クリック: ファイル名をコピー)
//------------------------------------------------------------------
#define NKMM_FIX_STATUSBAR

//------------------------------------------------------------------
// 折り返しモード 20170603
//  - トグルで切り替えたときに「折り返さない」が処理されていないので修正
//------------------------------------------------------------------
#define NKMM_FIX_WRAP_MODE

//------------------------------------------------------------------
// 'bregonig.dll' の検索方法の修正 20170709
//  - 32bit 'bregonig32.dll' → 'bregonig.dll'
//  - 64bit 'bregonig64.dll' → 'bregonig.dll'
//------------------------------------------------------------------
#define NKMM_FIX_BREGONIG_NAME_SEARCH

//------------------------------------------------------------------
// PPAを使用する
//  - 64bit版のときはPPAの処理を無効にする
//  - 古いものなので無効にする 20170722
//------------------------------------------------------------------
//#ifndef _WIN64
//#define NKMM_USE_PPA
//#endif

//------------------------------------------------------------------
// プロファイル
//  - カラー設定のインポートはカラー情報だけを適用させる 20170504
//  - カラー設定の色に名前を付ける (fg,bg,white,blackなど) 20170510
//  - プライグインの設定書き込み時、未定義値を無視する 20170612
//  - 印刷設定書き込み時、未定義値を無視する 20170612
//------------------------------------------------------------------
#define NKMM_FIX_PROFILES
	#define NKMM_SEPARATE_HISTORY                    (0)  // 履歴は別ファイルで扱う (sakura.recent) 20170502
	#define NKMM_HISTORY_BLOCK_IN_INI                (1)  // NKMM_SEPARATE_HISTORYが0のとき、履歴(Mru/Keys/Grep/Cmd)をsakura.ini内にブロック形式(#XXX〜#end)で保存する。読み込みは新形式(ブロック)/旧形式(MRU[00].xxx=など)の両方に対応、書き込みは新形式のみ。iniファイル肥大化対策 20260728
	#define NKMM_DELETE_HISTORY_NOT_EXIST_AT_STARTUP (1)  // 起動時に存在しないファイル・フォルダの履歴は削除する (ndef(NKMM_FIX_PROFILES)の時の処理がない) 20170410
	#define NKMM_USE_KEYWORDSET_CSV                  (1)  // sakura.keywordset.csvを用意し、強調キーワードの管理はこのファイルで行う 20170513

//------------------------------------------------------------------
// 共通設定「強調キーワード」タブの、sakura.keywordset.csv対応強化 20260802
//  - sakura.keywordset.csvから読み込んだかどうかのフラグを共有メモリに持ち、
//    読み込んでいる間は「強調キーワード」タブの編集系コントロール(セット追加/
//    削除/名称変更/キーワード追加・編集・削除・大文字小文字区別/インポート/
//    エクスポート/整理)をDisableにする(次回起動時にcsvへ上書きされ編集内容が
//    失われるため)
//  - 「変更」ボタンと同じ位置に「更新」ボタンを重ねて配置し、csv側のキーワード
//    ファイルからセット単位で再読み込みできるようにする(再読込可能な時だけ
//    「変更」の代わりに表示)
//  - csv読み込み時は「セット追加」「セット削除」ボタンも隠し、空いた場所に
//    現在のセットが使用しているキーワードファイル名を表示する
//  - キーワード一覧に、実際にエディタで使われる強調表示色・太字/下線・
//    フォントをプレビュー表示する(色は常に「基本」に統一されるが、太字/下線/
//    フォントはタイプ別の設定(m_bUseTypeDisp/m_bUseTypeFont)を反映する)
//  - 上記プレビューで背景色が反映されない問題を修正 20260803
//    (ビジュアルスタイル有効時、ListViewはNM_CUSTOMDRAWのclrTextBkを無視して
//    テーマの背景を描画してしまう。文字色(clrText)は反映されるため気付きにくい。
//    PreventVisualStyle()でこのリストビューのテーマを無効化し、背景色を反映させる)
//  - sakura.keywordset.csvが参照するKeyword\*.kwdが見つからない場合、実装済み
//    タイプ(cpp.kwd,html5.kwd,plsql.kwd,COBOL.kwd,java.kwd,corba.kwd,awk.kwd,
//    batch.kwd,pascal.kwd,tex1.kwd,tex2.kwd,perl.kwd,perlvar.kwd,vb.kwd,vb2.kwd,
//    rtf.kwd)についてはソースに組み込み済みのキーワード配列(g_ppszKeywordsXXX)
//    で代用し、外部ファイル無しでも強調表示できるようにする 20260809
//    (外部ファイルが存在する場合は従来通りそちらを優先する)
//  - 上記フォールバックにCSS(css2.1.kwd),JavaScript(ecmascript_sys.kwd),
//    JavaScript2(javascript.kwd),PHP(php_reserved.kwd),python(python_2.5.kwd),
//    Ruby1-4(ruby1〜4.kwd),C#/C# content(csharp.kwd,csharp-context.kwd)も追加
//    (元々ソース未組み込みだったため、Keyword\配下の該当ファイルから起こして
//    埋め込んだ)。ただしPHP2(php.kwd)はPHP組み込み関数一覧で1万語超と大きく、
//    全キーワードセット共有の格納領域(MAX_KEYWORDNUM=15000)を圧迫するため
//    埋め込み対象外とし、従来通りKeyword\php.kwdが必要 20260809
//    (ついでにCType_Php.cppのm_nKeyWordSetIdx[0]がPHP/PHP2の両方に使われて
//    いてPHPが常に上書きされ強調に使われていなかったバグを[0]/[1]に修正)
//  - sakura.keywordset.csvが存在せず旧来のInitKeyword()に落ちた場合、上記10タイプ
//    (CSHARP/CSHARP2/CSS/JS/JS2/PHP/PYTHON/RUBY1-4。PHP2除く)はPopulateKeyword2が
//    決め打ちで組み込み配列を使わず、外部ファイルが無いと強調キーワード0件になって
//    いた(InitKeywordFromList経由のGetEmbeddedKeywordArr()フォールバックはこの経路を
//    通らないため)。PopulateKeyword2->PopulateKeywordに変更し、BUILD_OPT_IMPKEYWORD
//    時はこの経路でも組み込み配列を使うようにした。PHP2のみ組み込み対象外のため
//    PopulateKeyword2のまま 20260809
//  - sakura_core\env\DLLSHAREDATA.h: SShare_Flags::m_bKeywordSetLoadedFromCsv
//  - sakura_core\env\CShareData_IO.cpp: 上記フラグの設定
//  - sakura_core\CKeyWordSetMgr.h,cpp: ClearKeyWord/SetKeyWordFile/GetKeyWordFile
//  - sakura_core\types\CType.cpp: InitKeywordFromListでのキーワードファイル名記録、
//    GetEmbeddedKeywordArr()による組み込みキーワードへのフォールバック 20260809
//  - sakura_core\types\CType_Css.cpp,CType_JavaScript.cpp,CType_Php.cpp,
//    CType_Python.cpp,CType_Ruby.cpp,CType_Csharp.cpp: 上記の組み込み配列追加
//  - 組み込みキーワード配列の中身はsakura_keyword\*.kwdから
//    tools\GenerateKeywordInc.ps1でsakura_core\types\generated\*.incへ生成する
//    (generated\*.incはgit管理外のため、フレッシュcloneでは初回ビルド前に実行が必要)
//  - sakura.keywordset.csvが無い状態で、全キーワードセットが組み込みキーワードの
//    ままだった場合、sakura.ini保存時に[KeyWords]セクション自体を書かないように
//    した(sakura_core\env\CShareData_IO.cpp: ShareData_IO_KeyWords())。書いてしまうと
//    次回起動時からcsvが無い限りそのini内容が優先され続け、ソース更新後の組み込み
//    キーワードが反映されなくなるため。1つでもユーザーがカスタマイズした(組み込み
//    でない)セットがあれば、位置(インデックス)整合性を保つため従来通り全セットを
//    書く(部分的にスキップすると、他タイプのm_nKeyWordSetIdxとの対応がずれるため) 20260809
//  - 詳細はchangelog/NKMM_FIX_KEYWORDSET_UI.md参照
//  - sakura_core\prop\CPropCommon.h,CPropComKeyword.cpp: ダイアログ側の実装
//  - sakura_core\sakura_rc.rc,sakura_lang_rc.rc,sakura_rc.h,sakura.hh:
//    IDC_BUTTON_KEYWORD_RELOAD, IDC_STATIC_KEYWORD_FILE
//------------------------------------------------------------------
#if defined(NKMM_FIX_PROFILES) && NKMM_USE_KEYWORDSET_CSV
	#define NKMM_FIX_KEYWORDSET_UI
#endif // NKMM_

//------------------------------------------------------------------
// メインメニューはデフォルトを使用する 20170515
// (メインメニューのカスタマイズは混乱を招く原因になっているため)
//  - 共通設定から「メインメニュー」タブを削除します
//------------------------------------------------------------------
#define NKMM_FIX_MAINMENU_FORCE_DEFAULT

//------------------------------------------------------------------
// 開かれているファイルを自己管理する前提で多重オープンの許可 20130619
//  - Shiftを押しながらファイルドロップで多重オープン
//    -> 開いているドキュメントを複製する機能をつけたい
//------------------------------------------------------------------
#define NKMM_FIX_MULTIPLE_OPEN_FILES

//------------------------------------------------------------------
// デフォルト値を変更 20180620
//  - タスクトレイのアイコンを常駐しない
//  - キーリピート時の左右移動数を1にする
//  - ファイルの履歴ＭＡＸを36, フォルダの履歴ＭＡＸを20にする
//------------------------------------------------------------------
#define NKMM_FIX_DEFAULT_VALUE

//------------------------------------------------------------------
// 最大数 20131002, 20161213, 20170618
// \sakura_core\config\maxdata.h
//------------------------------------------------------------------
#define NKMM_FIX_MAXDATA
	#define NKMM_MAX_SEARCHKEY  (20) // 検索キー (REG/RecentSearchKeyMax:20)
	#define NKMM_MAX_REPLACEKEY (20) // 置換キー (REG/RecentReplaceKeyMax:20)
	#define NKMM_MAX_GREPFILE   (10) // Grepファイル (REG/RecentGrepFileMax:10)
	#define NKMM_MAX_GREPFOLDER (20) // Grepフォルダ (REG/RecentGrepFolderMax:20)

//------------------------------------------------------------------
// 最近使ったファイル
//  - ファイルパスを短縮して表示する (REG/FilePathCompactLength:60) 20170615
//  - ファイルサイズを表示 20170615
// \sakura_core\env\CFileNameManager.cpp
//   bool CFileNameManager::GetMenuFullLabel(
// \sakura_core\recent\CMRUFile.cpp
//   HMENU CMRUFile::CreateMenu( HMENU	hMenuPopUp, CMenuDrawer* pCMenuDrawer ) const
// \sakura_core\window\CEditWnd.cpp
//   cMRU.CreateMenu( hMenu, &m_cMenuDrawer );	//	ファイルメニュー
// - ディレクトリの場合はサイズ表示はなし
//   →IsDirectory()を使ったがうまくいかなかったので::PathIsDirectory()を使用
//------------------------------------------------------------------
#define NKMM_FIX_RECENT_FILE_DISP_NAME
	#define NKMM_FILEPATH_COMPACT_LENGTH (60)

//------------------------------------------------------------------
// タイプ別設定一覧の「追加」から任意のタイプを追加できるようにする 20170620
//  ! コピー不足未確認
//------------------------------------------------------------------
#define NKMM_FIX_TYPELIST_ADD_ANY_TYPE

//------------------------------------------------------------------
// ステータスバー「タイプ」ポップアップで、名前が空のタイプ枠を
// 「-- undefined name --」ではなく区切り線として表示する 20260731
//------------------------------------------------------------------
#define NKMM_FIX_TYPE_MENU_EMPTY_SEPARATOR

//------------------------------------------------------------------
// タスクトレイから「タイプ別設定一覧」を開いたとき、前面の編集ウィンドウ
// (または開いているウィンドウが1つだけの場合はそれ)の現在の文書のタイプを
// デフォルト選択にする 20260731
//------------------------------------------------------------------
#define NKMM_FIX_TRAY_TYPELIST_CURRENT_TYPE

//------------------------------------------------------------------
// タイプ別設定一覧の一覧の右側に、タイプ別設定の全タブ(スクリーン/カラー/
// ウィンドウ/支援/正規表現キーワード/キーワードヘルプ)をタブコントロールと
// して埋め込み、タイプを切り替えながら1画面で編集できるようにする。
// これにより「設定変更」ボタンから別ウィンドウの個別タイププロパティ
// シートを開く必要がなくなる 20260731
// → 20260807 見送り。タイプ別設定一覧への統合は行わないことにした
//   (「初期化」など他の追加機能はそのまま残す)
//------------------------------------------------------------------
//#define NKMM_FIX_TYPELIST_EMBED_ALLTABS

//------------------------------------------------------------------
// デフォルト文字コードを UTF8にする 20170622
//------------------------------------------------------------------
#define NKMM_FIX_DEFAULT_CHARCODE_TO_UTF8

//------------------------------------------------------------------
// 正規表現検索の際、検索文字列の正規表現記号をクォートする 20150601
// PHPの preg_quote() みたいなもの
//------------------------------------------------------------------
//#define NKMM_FIX_SEARCH_KEY_REGEXP_AUTO_QUOTE

//------------------------------------------------------------------
// メニューアイコン
//  ! ビットマップメニュー
//      ::InsertMenuItem( hMenu, 0xFFFFFFFF, TRUE, &mii );
//      http://home.a00.itscom.net/hatada/windows/introduction/menu01.html
//      http://eternalwindows.jp/winbase/menu/menu10.html
//    起動時にアイコンの数だけHBITMAPを生成する
//      \sakura_core\uiparts\CImageListMgr.cpp
//  - アイコン付きメニューだけオーナードローで、アイコンなしメニューは通常の
//    テーマメニューという見た目の不一致を修正 20260810
//    (「メニューにアイコンを表示」をONにすると、Vista以降でもオーナードロー
//    になり見た目がXP風になる問題が2010.03.29のコメントに既知の課題として
//    残っていた。CMenuDrawer::MyAppendMenuの
//    `if( m_bMenuIcon || !IsWinVista_or_later() ) nFlagAdd = MF_OWNERDRAW;`
//    が原因で、Vista以降でもm_bMenuIcon==trueなら常にオーナードローされていた)
//    Vista以降はMF_OWNERDRAWをやめ、MIIM_BITMAP/hbmpItemに32bppアルファ付き
//    ビットマップ(透過色をアルファ0に変換したもの)を設定することで、通常の
//    テーマメニューのままアイコンを表示できるようにした。チェック中の項目は
//    テーマが自動でハイライト枠を描画する。オーナードローはVista未満のみ
//    引き続き使用する(アクセスキー分の詰め処理のため)。
//    - sakura_core\uiparts\CImageListMgr.h,cpp: GetAlphaBitmap()を追加。
//      アイコン番号ごとに32bppアルファ付きビットマップを生成してキャッシュする
//      (破棄はデストラクタ、またはResetExtend()での明示リセット時)
//    - sakura_core\uiparts\CMenuDrawer.cpp: MyAppendMenu()で、Vista以降かつ
//      アイコンありの項目にMIIM_BITMAPを設定するよう変更
//  - サブメニュー(「折り返し方法」「入力改行コード指定」等)だけインデントが
//    揃わずアイコン列が無い状態で表示される不具合を修正 20260810
//    (サブメニュー項目はAppendMenu系の慣習で子HMENUの値がnFuncId引数に渡される
//    ため、CMenuDrawer::MyAppendMenu内では「nFuncId!=0」の通常項目の分岐に
//    入る。しかしGetIconIdByFuncId()にHMENUの値を渡しても対応する機能アイコンは
//    見つからずbitmapIdx==-1のままになるため、実アイコンがない項目としてMIIM_BITMAP
//    を一切設定していなかった。テーマメニューは「兄弟項目にMIIM_BITMAPがあれば
//    無条件に自動整列する」わけではなく、自分自身がMIIM_BITMAPを持たない項目は
//    インデントされないと判明。実アイコンの有無によらず、アイコン付きメニューが
//    有効な間はサブメニュー項目にも必ずMIIM_BITMAPを設定するよう修正し、実アイコンが
//    無い場合はCImageListMgr::GetBlankBitmap()(全画素アルファ0の16x16プレース
//    ホルダ、1個だけ生成してキャッシュ)を代わりに設定して兄弟項目とインデントを
//    揃えるようにした。セパレータ(MF_SEPARATOR)はテーマメニューが全幅で描画する
//    ため対象外)
//  - 上記2件の修正後、メニューを開くと項目のテキストごと何も表示されなくなる
//    不具合を修正 20260810
//    (MIIM_BITMAPを追加した際、mii.fMaskに元々あった`MIIM_TYPE`をそのまま
//    残していたのが原因。MIIM_TYPEはMIIM_BITMAP/MIIM_FTYPE/MIIM_STRINGを
//    束ねた旧式の複合フラグで、MSDNにも「MIIM_BITMAPと同時に指定すると
//    動作が不定になる」と明記されている。実機でも項目が空欄になる形で
//    再現した。MIIM_TYPEを、MIIM_BITMAPと共存できる現代的な代替
//    (MIIM_FTYPE+MIIM_STRING)に置き換えて修正。
//    - sakura_core\uiparts\CMenuDrawer.cpp: MyAppendMenu()のmii.fMask
//------------------------------------------------------------------
#define NKMM_FIX_MENUICON

//------------------------------------------------------------------
// 検索 (未実装)
//  ! 検索履歴からのオートコンプリート
//------------------------------------------------------------------
//#define NKMM_FIX_FIND

//------------------------------------------------------------------
// Grep 20150824
//  - Grepするフォルダの指定を UI的に増やす (Imitate 'MIFES')
//    →；で区切るより分かれていた方が扱いやすいし、履歴管理もしやすい.
//  - 除外フォルダ指定を別ボックスで指定できるようにする 20170618
//  - 指定フォルダをすべてチェックをはずすと「現在編集中のファイルから検索」とする
//  - 「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする
//    →本来の「現在編集中のファイルから検索」を使用すると、
//      影響を受ける他のチェックボックスの状態が変更したまま戻らないのが嫌だから.
//  - ファイル(フィルタ)指定はフォルダのあとに置く (フォルダのほうが変更する機会が多いため)
//------------------------------------------------------------------
#define NKMM_FIX_GREP

//------------------------------------------------------------------
// 置換 20160804
//  - 置換後文字列に置換前文字列を設定する
//------------------------------------------------------------------
#define NKMM_FIX_REPLACE

//------------------------------------------------------------------
// フォルダ選択ダイアログ 20150825
//  - CLSID_FileOpenDialogを使用する
//    使用するには Vista以降にする必要がある
//      -  WINVER=0x0500;_WIN32_WINNT=0x0500;_WIN32_IE=0x0501
//        -> WINVER=0x0601;_WIN32_WINNT=0x0601;_WIN32_IE=0x0800
// http://eternalwindows.jp/installer/originalinstall/originalinstall02.html
// https://msdn.microsoft.com/ja-jp/library/windows/desktop/ff485843(v=vs.85).aspx Minimum supported client
// http://qiita.com/hkuno/items/7b8daa37d9b68e390d7e _WIN32_WINNTの設定値
// http://www.02.246.ne.jp/~torutk/cxx/vc/vcpp100.html
//------------------------------------------------------------------
#if (WINVER >= _WIN32_WINNT_VISTA)
#define NKMM_FIX_SELECTDIR
#endif

//------------------------------------------------------------------
// ダイアログを編集ウィンドウに配置 20170404
// (デスクトップではなくサクラエディタのウィンドウの位置の影響を受けます)
//  - アウトライン解析
//  - 外部コマンド実行
//  - 検索（parent追従、no move）
//  - 置換
//  - Grep
//  - Grep置換
//  - 指定行へジャンプ
//  - タグファイルの作成
//  - タグジャンプリスト
//  - ウィンドウ一覧表示
//  - 文字コードの指定
//  - 履歴とお気に入りの管理
//  - 更新通知及び確認
//  - 中断
//------------------------------------------------------------------
#define NKMM_FIX_DIALOG_POS

//------------------------------------------------------------------
// タグジャンプ一覧
//  - 表示するカラムの選別と並び替え 20150827
//------------------------------------------------------------------
#define NKMM_FIX_TAGJUMP

//------------------------------------------------------------------
// ダイアログ
//------------------------------------------------------------------
#define NKMM_FIX_DIALOG
	#define NKMM_COMBO_DROP_ALT_AND_UPDOWN_ONLY      (1)  // コンボボックスのドロップダウンは上下キーでは行えないようにする(誤操作防止) 20170704
	#define NKMM_CLOSE_DIALOG_WITH_MODE_CANCELLATION (1)  // モード取り消し時にダイアログもいっしょに閉じる 20170809

//------------------------------------------------------------------
// タグファイル作成ダイアログ
//  - タグ作成フォルダの初期値を tags ファイルがあるところまで辿る 20170512
//------------------------------------------------------------------
#define NKMM_FIX_TAGMAKE_DIALOG

//------------------------------------------------------------------
// アウトライン解析ダイアログ 20161214
//  - フォントをメインフォントにする
//  - ドッキング時、ウィンドウカラーにシステムカラーを使う
//    (REG/OutlineDockSystemColor:1)
//  - ルールファイル解析で「デフォルト」だとソートしていないため逆順になる 20170509
//  - ダブルクリックでツリーの展開／縮小をできるようにする 20170720
//  ? SetWindowTheme(hwndList, L"Explorer", NULL);
//  ? ::SetWindowLongPtr(hwndList, GWL_STYLE, ::GetWindowLongPtr(hwndList, GWL_STYLE) & ~TVS_HASLINES);
//------------------------------------------------------------------
#define NKMM_FIX_OUTLINE_DIALOG

//------------------------------------------------------------------
// 検索ダイアログ
//  - ダイアログにフォーカスがあるときも「次を検索」「前を検索」キーを使用できるようにする 20170624
//  - ダイアログにフォーカスがあるときも上下キーで画面のスクロールをできるようにする 20170724
//  - レイアウト,検索方法を VisualStudio の検索を模倣 20170624
//    - インクリメンタル検索をする 20170621
//    - 「検索ダイアログを自動的に閉じる」を排除 20170711
//    - 「見つからないときにメッセージを表示」を排除 20170711
//  - インクリメンタル検索をデバウンス化 20260809
//    - 検索文字列コンボボックスの変更(CBN_EDITCHANGE)のたびに、UIスレッド上で
//      同期的にF_SEARCH_NEXT(文書全体の線形スキャン)を実行していたため、
//      数百万行規模のファイルではキー入力のたびにエディタ全体がフリーズして
//      いた。1文字打つごとに検索文字列自体が変わるため、既存のスクロール
//      バーマーカーキャッシュ(検索結果の行キャッシュ)は効かない
//      (パターンが変わるたびに無効化されるため)。実際の検索実行を
//      150msデバウンスし、連続入力中は走らせず、入力が止まってから
//      一度だけ実行するように変更。実機確認済み(500万行ファイルで再現)
//    - 20260809 このデバウンス用タイマーIDに1を使ったところ、同じCDlgFind
//      ウィンドウで既に使われていたID_TIMER_FIND_SLIDEIN(スライドイン
//      アニメーション用、これも1)と衝突していた。SetTimer()は同じHWND+
//      同じIDだと新規タイマー作成ではなく既存タイマーの間隔を上書きする
//      ため、ダイアログ表示中に入力するとスライドインアニメーションが
//      デバウンスタイマーに乗っ取られ、アニメーションが完了しないまま
//      ダイアログの位置が少しずれて止まる不具合になっていた(実機確認済み:
//      「入力すると検索ダイアログが少し下にずれる」)。デバウンス用IDを
//      2に変更して衝突を解消。
//  - sakura_core\dlg\CDlgFind.cpp, CDlgFind.h
//------------------------------------------------------------------
#define NKMM_FIX_FIND_DIALOG

//------------------------------------------------------------------
// 次を検索(F3・検索ダイアログのインクリメンタル検索)の非同期化 20260809
//  - デバウンス(NKMM_FIX_FIND_DIALOG側)だけでは、入力が止まるたびに走る
//    「文書全体を線形走査して見つからないと判定する」1回分(数百万行規模の
//    ファイルで実測150〜200ms)がUIスレッドをブロックし続けていた。実機で
//    500万行ファイルにて確認済み。
//  - CSearchAgent::SearchWord()の走査ループに中断フラグを追加し(同期呼び出しは
//    nullptrを渡せば従来通り)、Command_SEARCH_NEXT()の「検索開始位置の調整
//    (選択中テキストがある場合の特殊処理)」を伴わない単純なケース
//    (選択なし・pcSelectLogic==NULL・すべて置換実行中でない・正規表現でない・
//    文書が一定行数を超える)に限り、CEditView::AsyncFindNextでバックグラウンド
//    スレッドに検索を回し、結果が出たらWM_APP_ASYNC_SEARCH_DONEで戻して
//    カーソル移動などのUI反映を行う。上記以外(選択中の検索開始・すべて置換・
//    正規表現・小さいファイル)は既存の同期パスをそのまま使う(挙動変更なし)。
//  - 文書変更(タイプ入力/貼り付け/削除/Undo/Redo/すべて置換)との競合防止のため、
//    唯一に近い合流点であるCEditView::ReplaceData_CEditView3の先頭で、実行中の
//    検索スレッドを中断・待機してから編集を進める(ScrBarMarkerのWaitForBuild
//    (true)と同じ考え方)。検索スレッドが参照する検索パターン文字列・オプションは
//    共有メンバを直接参照せず、リクエスト時に独立コピーを作って渡す
//    (CSearchStringPattern::SetPattern()はポインタを保持するだけでコピーしない
//    ため、共有バッファを渡すと次のキー入力で解放/書き換えされうる)。
//  - sakura_core\CSearchAgent.h,cpp / view\CEditView.h,cpp / cmd\CViewCommander.h,
//    CViewCommander_Search.cpp / view\CEditView_Command_New.cpp
//  - 20260810 既知の制約(修正保留・要検討): 新旧2.3.2.0の検索速度をマクロ
//    (WSH/JScript、bench_search.js)で比較しようとしたところ、5,174,307行/
//    500MBファイルでEditor.SearchNext()呼び出し直後にEditor.GetSelectLineFrom()
//    を読んでも常に0(未検出)が返ることが判明。原因: 上記の非同期化は
//    F_SEARCH_NEXT自体(F3キー・マクロ問わず同じCommand_SEARCH_NEXT経路)に
//    掛かっているため、マクロから呼んでも対象がAsyncFindNext::Requestに
//    回されてすぐreturnする。マクロのJScript実行はUIスレッド上で完全に
//    同期的に進み、文の合間でメッセージポンプが回らない
//    (CWSH.cpp内のPeekMessage/DispatchMessageは「マクロ強制終了確認
//    ダイアログ」監視用の別スレッドにしかなく、通常のマクロ実行では出番が
//    ない)ため、バックグラウンドスレッド完了時に飛ぶ
//    WM_APP_ASYNC_SEARCH_DONEがマクロ実行中には一切ディスパッチされず、
//    選択範囲(カーソル移動)への反映がマクロから見えない。
//    影響: 20万行(NKMM_ASYNC_SEARCH_NEXT_LINE_THRESHOLD)を超える文書に対して
//    SearchNext()を呼び、直後に選択位置/ヒット文字列を読むマクロは、この
//    フォークでは検索結果を取得できなくなる(手元のF3操作は非同期完了後に
//    正しく反映されるため無症状)。対応する場合はWM_APP_ASYNC_SEARCH_DONE
//    受信までブロックする同期版マクロAPI(例: Editor.WaitForSearchNext()相当)
//    の追加を要検討。
//  - 20260810 上記調査中、真の検索完了時間(バックグラウンドスレッド内の
//    SearchWord()実測)を計測するため、AsyncFindNextThreadProc内に
//    QueryPerformanceCounterで計測しD:\github.niki\sakura\
//    bench_async_core_times.csvへ書き出す一時的な計測コードを追加して測定した
//    (測定後に削除済み)。初回の結果: 公式2.3.2.0(x86)がマクロ計測(同期)で
//    230ms、フォーク(x64/x86)が実測ネット検索時間で約110〜165ms、「検索
//    アルゴリズム自体が1.5〜2倍速くなった」と結論しかけたが、
//    NKMM_USE_MIMALLOC/NKMM_USE_MIMALLOC_OVERRIDEを一時的に無効化して
//    同条件で再測定したところ約256ms(x64, n=10)まで悪化し、公式ビルドと
//    同等かむしろ遅い水準に戻った。結論を訂正: CSearchAgent::SearchWord()の
//    走査ループ自体(SearchString/CDocLine::GetDocLineStrWithEOL/GetNextLine)
//    は検索中に一切ヒープ確保しておらず(スキップテーブルはRequest()側で
//    スレッド起動前に1回だけ構築済み)、フォークと公式とで検索コード自体は
//    実質同一。したがって観測された速度差の大部分は「非同期化」でも
//    「アルゴリズム改善」でもなく、5,174,307行分のCDocLine/文字列バッファを
//    読み込み時にどのアロケータ(mimalloc vs 既定のCRTヒープ)が確保したかに
//    よるメモリレイアウト(キャッシュ局所性)の差だった可能性が高い。
//    非同期化そのものの効果は「呼び出し元(UI/マクロ)を即座に解放する」
//    ことに限られ、走査自体の速さにはほぼ寄与していない。なお公式2.3.2.0を
//    同一ファイルで2回計測(230ms→235ms)しOSファイルキャッシュのウォーム
//    アップでは説明できないことは確認済み(Editor.GoFileTop()以降だけを
//    計測しており、その時点でファイルは既にドキュメント構造へ読み込み
//    完了済みのため、計測区間はディスクI/Oを経由しない)。
//------------------------------------------------------------------
#define NKMM_FIX_ASYNC_SEARCH_NEXT
	#define WM_APP_ASYNC_SEARCH_DONE (WM_APP + 2503)  // 非同期検索完了メッセージ
	// この行数を超える文書でのみ、対象範囲の検索を非同期化する(小さいファイルは
	// スレッド起動のオーバーヘッド/結果反映の1フレーム遅延を避けるため従来通り同期)
	#define NKMM_ASYNC_SEARCH_NEXT_LINE_THRESHOLD (200000)

//------------------------------------------------------------------
// アウトライン解析
//  - 正規表現を使用した場合はマッチした文字列のみリストに登録 20170608
//------------------------------------------------------------------
#define NKMM_FIX_OUTLINE

//------------------------------------------------------------------
// アンドゥ, リドゥ
//  - 行数が変わらないときの高速化 20170723
//  - あらかじめバッファを確保する 20170723
//------------------------------------------------------------------
#define NKMM_FIX_UNDOREDO

//------------------------------------------------------------------
// ウェイトカーソル 20150709
//  - 一部、正しい位置に修正
//  - 文字列削除時に表示しない（アンドゥのときなど）
//------------------------------------------------------------------
#define NKMM_FIX_WAITCUESOR

//------------------------------------------------------------------
// 検索ダイアログの「正規表現」が影響を受けないようにする
// (下記動作を行うとチェックが外れてしまうバグ)
//  - 検索マーク切り替え時 20150601
//  - インクリメンタルサーチ時 20161214
//------------------------------------------------------------------
#define NKMM_FIX_SEARCH_KEEP_REGEXP

//------------------------------------------------------------------
// 変更行は縦線で描画する 20170731
//  - 副作用で行番号が非表示のときも状態がわかるようになります
//------------------------------------------------------------------
#define NKMM_FIX_MODGYOU_DRAW_VLINE

//------------------------------------------------------------------
// ブックマークは縦線で描画する 20170727
//  - 副作用で行番号が非表示のときも状態がわかるようになります
//    (そもそもは行番号が表示されていないときにブックマーク行がわからないバグ)
//------------------------------------------------------------------
#define NKMM_FIX_BOOKMARK_DRAW_VLINE

//------------------------------------------------------------------
// 偶数行背景はEOF以降は適用しない 20170620
//------------------------------------------------------------------
#define NKMM_FIX_NOT_EVEN_LINE_FROM_EOF

//------------------------------------------------------------------
// ノート線はEOF以降は適用しない 20170620
//------------------------------------------------------------------
#define NKMM_FIX_NOT_NOTE_LINE_FROM_EOF

//------------------------------------------------------------------
// ::ExtTextOut による塗りつぶしを ::PatBlt に変更 20170708
//------------------------------------------------------------------
#define NKMM_FIX_EXTTEXTOUT_TO_PATBLT

//------------------------------------------------------------------
// WM_ERASEBKGNDの抑制 20170708
//------------------------------------------------------------------
#define NKMM_FIX_SUPPRESSION_OF_WM_ERASEBKGND

//------------------------------------------------------------------
// タイプ設定のデフォルトを直接設定するようにする 20170714
//  - なぜstaticで持ってコピーしているのか不明
//------------------------------------------------------------------
#define NKMM_FIX_TYPE_CONFIG_DEFAULT

//------------------------------------------------------------------
// カーソル移動時のちらつきを暫定で対処 20150804
// カーソル上下移動時に次の条件?のときに画面の更新が間に合わずに描画が崩れる 20150804
//  - キーリピートが早い
//  - 裏で描画を頻繁に行うアプリが動いている
//    → UpdateWindow() を呼び出すタイミングを変更することで一時対応
//  ? スクロールした時に ScrollWindowEx() と再描画の同期がとれていない
//    → MacTypeなどを使用すると描画の負荷が高くなり顕著になる。使わない場合は高速にやると再現する
//  - 水平スクロールしたときに直前の描画状態が残るのを修正 (水平スクロール時のみ再描画) 20170710
//------------------------------------------------------------------
#define NKMM_FIX_FLICKER


//------------------------------------------------------------------
// カラーフォント(絵文字等のCOLR/CPALカラーグリフ)の描画対応 20260716
//  - 既存のGDI描画はそのまま残し、DirectWrite/Direct2Dでカラーグリフだけを
//    追加でオーバーレイ描画する(GDIはCOLR/CPAL形式のカラーフォントを
//    単色でしか描画できないため)
//  - Direct2D/DirectWriteが利用できない環境(DLL不在など)では自動的に
//    何もせず、従来通りのGDI描画のみになる
//  - sakura_core\view\CColorFontRenderer.h/.cpp
//  - 詳細はchangelog/NKMM_FIX_COLOR_FONT.md参照
//------------------------------------------------------------------
#define NKMM_FIX_COLOR_FONT

//------------------------------------------------------------------
// サロゲートペア文字(絵文字等)の桁幅をルーラーの固定グリッドに揃える 20260717
//  - CalcPxWidthByFont2()はGDIが実測した字送り幅をそのまま桁幅として使うが、
//    本文フォントにグリフが無い絵文字等はSystemLinkで代替フォントへ黙って
//    差し替えられて描画されるため、実測幅が「半角幅のちょうど2倍」から
//    ズレることがある。レイアウトの桁位置計算はこの値をそのままピクセル
//    座標として積算していく一方、ルーラーは半角幅の固定グリッドで描画される
//    ため、ズレが後続の文字へ累積し、絵文字混在行でルーラーとの位置が
//    徐々に食い違っていく(NKMM_FIX_COLOR_FONTの有無に関わらず発生する
//    既存の不具合)。
//  - サロゲートペア文字は全角2桁として扱う前提(CNativeW::GetKetaOfChar等)
//    と矛盾しないよう、桁幅の計算は常にちょうど半角幅の2倍に丸める。
//  - sakura_core\charset\charcode.cpp (LocalCache::CalcPxWidthByFont2)
//------------------------------------------------------------------
#define NKMM_FIX_EMOJI_WIDTH

//------------------------------------------------------------------
// 設定ダイアログ(共通設定/タイプ別設定)の多重オープンを排他制御 20260719
//  - 複数ウィンドウで同時に設定ダイアログを開いて別々の項目を変更しOKすると、
//    Applyがダイアログを開いた時点のスナップショットを丸ごと共有メモリへ
//    上書きする実装のため、後からOKした側が先にOKした側の変更を
//    消してしまう(lost update)不具合があった
//  - 既に他ウィンドウで設定ダイアログが開いている場合は新規に開かず、
//    既存のダイアログ(ウィンドウ)をアクティブにするだけにする
//  - env/DLLSHAREDATA.h,cpp: CPropSheetOwnerGuard
//------------------------------------------------------------------
#define NKMM_FIX_PROPSHEET_EXCLUSIVE

//------------------------------------------------------------------
// 桁・文字数カウントのサロゲートペア対応 20260719
//  - 文字数カウント(ドキュメント全体/選択範囲)や「文字単位」表示モードの
//    キャレット桁位置が、UTF-16のコード単位(wchar_t)数をそのまま文字数と
//    して数えており、サロゲートペア文字(絵文字等)を2文字と誤カウントして
//    いた
//  - CNativeW::GetSizeOfChar()を使って論理文字単位で数えるように修正
//  - なお、CCaret.cppの「行番号をCRLF単位で表示」+「文字単位」表示を
//    同時に使う組み合わせは、キャレット位置計算で折り返し前の物理行データ
//    を参照できないため未対応(既知の制限)
//  - sakura_core\mem\CNativeW.h,cpp: GetCharCountInRange()
//  - sakura_core\view\CEditView.cpp, CViewSelect.cpp, CCaret.cpp
//  - 詳細はchangelog/NKMM_FIX_SURROGATE_CHAR_COUNT.md参照
//------------------------------------------------------------------
#define NKMM_FIX_SURROGATE_CHAR_COUNT

//------------------------------------------------------------------
// 正規表現ライブラリ(bregonig.dll)が見つからない場合はPCRE2にフォールバック 20260720
//  - 検索/置換/Grep/マクロ(CBregexp)と構文強調キーワード(CRegexKeyword)の両方に適用。
//    DLLが「見つからない」場合のみ発動(壊れている場合は今まで通りエラー)
//  - フォールバックエンジンにPCRE2(libs/pcre2, BSD-3-Clause)を静的vendor。
//    JIT化(pcre2_jit_compile_16、sljitをlibs/deps/sljitに新規vendor)済み 20260728
//  - libs\pcre2, sakura_core\extmodule\CRegexFallback.h,cpp
//  - ReplaceAllで行頭以外にマッチした場合、行頭からマッチ位置までの前置文字列が
//    結果に二重挿入される不具合を修正(DoSubst()がpcre2_substitute()に渡す
//    subjectをtargetbeg起点にしていたため、戻り読み用の文脈がそのまま出力に
//    混入していた。出力からstartOffset分を読み飛ばすよう修正)。実bregonig.dll
//    使用時は元々発生しないフォールバック限定の不具合だった 20260810
//  - 詳細はchangelog/NKMM_FIX_REGEXP_FALLBACK.md参照
//------------------------------------------------------------------
#define NKMM_FIX_REGEXP_FALLBACK

//------------------------------------------------------------------
// タイプ別設定の色情報を全タイプで共有・統一する 20260726
//  - 色(文字色・背景色)は常に「基本」(Types(0))の設定をそのまま使う。タイプ
//    ごとに変えられるのは表示/非表示・太字・下線のみ(STypeConfig::m_bUseTypeDisp
//    でトグル)。共通色専用の別ストレージは持たず、色の解決はGetTypeConfig()の
//    1箇所のみで行う
//  - sakura_core\types\CType.h,cpp / env\CDocTypeManager.cpp /
//    typeprop\CPropTypes.h,CPropTypesColor.cpp
//  - 詳細はchangelog/NKMM_FIX_SHARED_TYPE_COLOR.md参照
//------------------------------------------------------------------
#define NKMM_FIX_SHARED_TYPE_COLOR

//------------------------------------------------------------------
// WSH(JScript/VBScript)に依存しないマクロエンジンとしてQuickJSを追加 20260726
//  - WSHはOS登録済みの外部COMコンポーネント依存で、無い/廃止された環境では
//    マクロが使えない。拡張子".qjs"のマクロファイルをQuickJS(quickjs-ng, MIT)
//    で実行できるようにした。WSH/PPAは変更・削除せず両方とも従来通り使える
//  - libs\quickjs (新規vendor)、sakura_core\macro\CQuickJSMacroMgr.h,cpp,
//    CQuickJSIfObjBinder.h,cpp
//  - 詳細はchangelog/NKMM_FIX_QUICKJS_MACRO.md参照
//------------------------------------------------------------------
#define NKMM_FIX_QUICKJS_MACRO

//------------------------------------------------------------------
// 検索ダイアログ（フローティングパネル化）
//  - タイトルバー・DS_MODALFRAMEを廃止し、枠なしのフローティングパネル風にする
//  - Windows 11 の角丸(DWMWA_WINDOW_CORNER_PREFERENCE)を適用
//  - 親ウィンドウ(エディタ)の移動・リサイズに追従(NKMM_FIX_DIALOG_POSに追記)
//  - 生成時に上からスライドインするアニメーションを付与
//  - 無効化すると従来通りタイトルバー付きダイアログに戻る 20260727
//  - 詳細はchangelog/NKMM_FIX_FIND_DIALOG_FLAT.md参照
//------------------------------------------------------------------
#define NKMM_FIX_FIND_DIALOG_FLAT
	// 1にすると角丸をやめてDWMの非クライアント描画自体を無効化し、影も消す。
	// (DWMWA_WINDOW_CORNER_PREFERENCEとDWMWA_NCRENDERING_POLICYは同じNC描画
	//  パイプラインのため、角丸と影は分離できず二者択一) 比較検証用 20260729
	// 影の代わりにWS_BORDERで薄い縁取りを付ける(sakura_rc.rcのIDD_FIND STYLE)
	// 詳細はchangelog/NKMM_FIND_DIALOG_NO_SHADOW.md参照
	#define NKMM_FIND_DIALOG_NO_SHADOW (1)
	// 単語単位/大文字小文字/正規表現のトグルボタン("|Ab|"/"Aa"/".*")が小さく
	// 読み取りにくいため、ボールド体フォントにして視認性を上げる 20260801
	#define NKMM_FIND_DIALOG_BOLD_TOGGLE_BUTTONS (1)

//------------------------------------------------------------------
// mimalloc(MIT)によるoperator new/deleteの高速化 20260727
//  - CRTのmalloc/freeは上書きしない(完全上書きはNKMM_USE_MIMALLOC_OVERRIDE参照)。
//    C++のnew/deleteのみをmimalloc(mi_malloc/mi_free)に差し替える
//  - _main/WinMain.cppで1箇所だけmimalloc-new-delete.hをincludeしている
//  - libs\mimalloc (新規、mimalloc v3.4.3をvendor)
//  - ベンチマーク(小オブジェクトの確保/解放churnを模した合成マイクロベンチ、
//    アロケータ単体を分離して計測): 詳細はchangelog/NKMM_USE_MIMALLOC.md参照
//------------------------------------------------------------------
#define NKMM_USE_MIMALLOC

//------------------------------------------------------------------
// mimalloc(MIT)によるmalloc/free/calloc/realloc等の完全上書き 20260731
//  - NKMM_USE_MIMALLOCの追加オプション。malloc()等のCRT標準関数を直接呼んでいる
//    箇所(operator new/delete経由でない13ファイル)にも効果を及ぼしたい場合に使う。
//    静的CRT(/MT)ビルドのため、mimalloc本体を`MI_MALLOC_OVERRIDE`付きで
//    コンパイルするだけでredirect dll無しに静的上書きできる
//  - sakura_core\config\mimalloc_override_fi.h (ForcedIncludeFile)
//  - 詳細はchangelog/NKMM_USE_MIMALLOC_OVERRIDE.md参照
//  - 20260810 Debug(/MTd)ではlibucrtd.libの_expand(expand.obj)とmimalloc側の
//    _expand定義が衝突し LNK2005/LNK1169 でリンクに失敗することが判明した。
//    NKMM_USE_MIMALLOC_OVERRIDE.mdの検証はRelease|Win32のみで、Debug/x64は
//    未検証だったことが原因。DebugビルドはCRTの通常のデバッグヒープを使う方が
//    デバッグ用途としても好都合なため、Release(NDEBUG)限定にした
//------------------------------------------------------------------
#if !defined(_DEBUG)
#define NKMM_USE_MIMALLOC_OVERRIDE
#endif

#if defined(NKMM_USE_MIMALLOC_OVERRIDE) && !defined(NKMM_USE_MIMALLOC)
#error NKMM_USE_MIMALLOC_OVERRIDE requires NKMM_USE_MIMALLOC to also be defined
#endif

//------------------------------------------------------------------
// マクロ関数FileOpenDialog/FileSaveDialogの既定値バッファオーバーフロー修正 20260729
//  - macro\CMacro.cppのF_FILEOPENDIALOG/F_FILESAVEDIALOGハンドラで、
//    マクロ引数(ドキュメント内容から作られうる文字列)を無検査に
//    _tcscpyでTCHAR szPath[_MAX_PATH]へコピーしていたため、
//    _MAX_PATH文字を超える引数でスタックバッファオーバーフローする不具合を修正
//  - 詳細はchangelog/NKMM_FIX_MACRO_FILEDIALOG_OVERFLOW.md参照
//------------------------------------------------------------------
#define NKMM_FIX_MACRO_FILEDIALOG_OVERFLOW

//------------------------------------------------------------------
// キーワード登録時の固定長バッファオーバーフロー修正 20260729
//  - CKeyWordSetMgr::SetKeyWordArrで、ini(タイプ別設定のszKW[NN])から読み込んだ
//    キーワード文字列の長さをMAX_KEYWORDLEN(63文字)でクランプせずに
//    wmemcpyしていたため、64文字以上のキーワードを含む設定ファイルの読み込みで
//    共有メモリ(DLLSHAREDATA)上のm_szKeyWordArrがオーバーフローする不具合を修正
//  - 詳細はchangelog/NKMM_FIX_KEYWORD_OVERFLOW.md参照
//------------------------------------------------------------------
#define NKMM_FIX_KEYWORD_OVERFLOW

//------------------------------------------------------------------
// ダイアログ・メッセージボックスの表示位置修正 20260729
//  - タスクトレイメニュー等、編集ウィンドウを持たない場所から呼ばれた場合、
//    ダイアログ/メッセージボックスのオーナーがNULLやタスクトレイの隠しウィンドウに
//    なり、アクティブな編集ウィンドウではなく画面中央に表示されてしまっていた。
//    (終了確認ダイアログ、Grepダイアログ等)
//  - CDialog::DoModal/DoModeless、GetMessageBoxOwnerの共通経路で、
//    フォアグラウンドウィンドウが編集ウィンドウであればそちらをオーナーにすることで、
//    アクティブな編集ウィンドウの中央に表示されるようにした
//  - 詳細はchangelog/NKMM_FIX_DIALOG_OWNER.md参照
//------------------------------------------------------------------
#define NKMM_FIX_DIALOG_OWNER

//------------------------------------------------------------------
// タイプ別に数値ハイライトの専用実装を追加 20260730
//  - 既存のIsNumber()(全タイプ共通の汎用実装)は言語ごとに異なる数値リテラル
//    記法(2進数、桁区切り記号、サフィックス等)を色分けできなかった。対応
//    タイプのときだけタイプ別のIsNumberXxx()で差分を加算する形にした
//    (無効化すれば従来のIsNumber()のみの挙動に戻る)
//  - 対応タイプ: C/C++, Java, C#, JavaScript, PHP, Python, Ruby, Perl,
//    Visual Basic, Pascal, CSS, アセンブラ(各言語の対応記法・既知の未対応は
//    CColor_Numeric.cppの言語別セクションのコメント参照)
//  - sakura_core\view\colors\CColor_Numeric.h,cpp
//  - 詳細はchangelog/NKMM_FIX_NUMERIC_LANG_LITERAL.md参照
//------------------------------------------------------------------
#define NKMM_FIX_NUMERIC_LANG_LITERAL

//------------------------------------------------------------------
// 共通設定「キー割り当て」「ツールバー」ページに「初期化」ボタンを追加 20260731
//  - どちらも既存の「メインメニュー」ページの「初期化」ボタンと同じ文言・確認
//    メッセージの体裁("〇〇の設定を初期状態に戻します。\nよろしいですか？")
//  - キー割り当て: 全キーの割り当てを出荷時の既定値に戻す(1キー単位で戻す
//    既存の「解除」ボタンとは別機能)
//  - ツールバー: ボタン構成一覧のみ既定値に戻す。「フラットなボタン」設定は
//    ボタン構成とは独立した見た目の設定のため変更しない
//  - 既存のCShareData::InitKeyAssign()/InitToolButtons()(DLLSHAREDATA全体を
//    要求する起動時初期化専用)から、ダイアログの一時データ(CommonSetting_KeyBind/
//    CommonSetting_ToolBar単体)にも使える形でCShareData::ResetKeyBindToDefault()/
//    ResetToolBarButtonsToDefault()としてロジックを複製・分離
//  - sakura_core\env\CShareData.h,cpp
//  - sakura_core\func\CKeyBind.cpp
//  - sakura_core\prop\CPropCommon.h
//  - sakura_core\prop\CPropComKeybind.cpp, CPropComToolbar.cpp
//  - sakura_core\sakura_rc.rc,h: IDC_BUTTON_INITIALIZE(既存ID再利用)
//------------------------------------------------------------------
#define NKMM_FIX_KEYBIND_TOOLBAR_RESET

//------------------------------------------------------------------
// 共通設定「カスタムメニュー」ページに「初期化」ボタンを追加 20260731
//  - NKMM_FIX_KEYBIND_TOOLBAR_RESETと同じ文言・確認メッセージの体裁
//  - 選択中のメニュー(右クリックメニュー/カスタムメニュー1/タブメニュー等)だけでなく、
//    全メニューまとめて出荷時の既定値に戻す(既存の「削除」ボタンは1項目単位)
//  - 既存のCShareData::InitPopupMenu()(DLLSHAREDATA全体を要求する起動時初期化専用、
//    かつ実際には引数を使わずm_pShareData経由で自分自身の共有メモリしか参照して
//    いなかった)の引数をCommonSetting_CustomMenu&に変更(呼び出しは1箇所のみで
//    動作は変わらない)。これを土台にCShareData::ResetCustomMenuToDefault()を追加
//  - sakura_core\env\CShareData.h,cpp
//  - sakura_core\prop\CPropCommon.h
//  - sakura_core\prop\CPropComCustmenu.cpp
//  - sakura_core\sakura_rc.rc,h: IDC_BUTTON_INITIALIZE(既存ID再利用)
//------------------------------------------------------------------
#define NKMM_FIX_CUSTMENU_RESET

//------------------------------------------------------------------
// キー割り当てエクスポート時に、各キーの8モディファイア組み合わせすべての
// 割り当て機能名を注釈として書き出す 20260801
//  - 既存のKeyBind[NNN]=コード,f0..f7,キー名 の行はそのまま維持し、末尾に
//    TAB区切りで8個の機能コードを人が読める名前に解決したものをカンマ区切りで
//    追記する(同じ行の最後に列が増えたように見える)。
//    KeyBind[NNN]=コード,f0..f7,キー名(TAB)name0,...,name7
//  - IO_KeyBind()はキー名を行末までとして読み込むため、TAB以降も本来はキー名に
//    混入してしまう。そのためImport()側でTAB以降を切り捨てて元のキー名に復元
//    しており、キー割り当ての動作(機能コード8個)自体には一切影響しない
//  - 値が0(F_DEFAULT、未設定)のスロットも、CKeyBind::GetFuncCodeAt()で実際に
//    有効な既定機能(例: Ctrl+F4のタブ/ウィンドウを閉じる、など状況依存のもの)
//    まで解決してから名前を出す。真に無割り当てのものは"(none)"と表示する
//  - sakura_core\typeprop\CImpExpManager.cpp: CImpExpKeybind::Export()と
//    CImpExpKeybind::Import()(Ver3/Ver4読み込み部)のみに追加。IO_KeyBind()
//    自体や、sakura.ini本体の通常の読み書き(ShareData_IO_KeyBind)には影響しない
//------------------------------------------------------------------
#define NKMM_FIX_KEYBIND_EXPORT_FUNCNAME

//------------------------------------------------------------------
// グリフキャッシュ(グリフアトラス)によるテキスト描画の高速化 20260801
//  - (フォント,文字,前景色,背景色,セル幅,セル高さ)をキーに、ExtTextOutで
//    一度描画した結果をHBITMAPアトラス(シェルフパッキング)へキャッシュし、
//    以後はBitBltで再利用する。ClearTypeの見た目は完全に保持される
//  - 背景画像(壁紙)使用時・複数文字を1回で描画するケース・横スクロールで
//    部分的に切れるグリフはキャッシュを使わず直接描画にフォールバックする
//  - 共通設定「全般」タブでON/OFFを切り替え可能(既定OFF)
//  - sakura_core\view\CGlyphAtlasCache.h,cpp
//  - 20260802: セル幅・高さ不一致によるBitBlt塗り残しバグ修正/転送の2フェーズ化
//    /ASCII文字まとめ焼き(WarmUpAscii)/設定ON/OFF反映漏れバグ修正、を追加
//  - 20260809: 描画待ちキュー(m_vPendingBlits)がシングルトン全体で共有されて
//    おり、複数の独立した描画パス(通常のOnPaint、対括弧強調表示の即時描画)が
//    互いのキューを誤って一緒にFlushしてしまいうる設計上の不具合を修正
//    - 通常のOnPaint中に対括弧の即時描画(DrawBracketPair、自前のGetDCで
//      FlushQueueする)が挟まると、片方のFlushQueue()がもう片方の積み残しごと
//      BitBltしてしまい、色付き矩形/線が誤った位置に描画される可能性があった。
//      FlushQueue()にキュー位置の「印」(BeginQueue()で取得)を渡すよう変更し、
//      各描画パスは自分がBeginQueue()した位置から末尾までしか処理・削除しない
//      ようにした(パスが入れ子になっても互いのキューを侵さない)。
//      これ自体は正しい修正だが、後述の「タブ切り替えで線が出る」不具合の
//      直接の原因ではなかった(下記参照。修正後も再現した)。
//    - sakura_core\view\CGlyphAtlasCache.h,cpp (BeginQueue追加、FlushQueueに
//      markBegin引数追加), CEditView_Paint.cpp, CEditView_Paint_Bracket.cpp
//      (呼び出し側でBeginQueue()を捕捉してFlushQueue()に渡すよう変更)
//  - 「行の間隔」使用時にタブ切り替えで線状の描画異常が出る不具合を修正 20260809
//    - タイプ別設定「行の間隔」(GetLineMargin())が非0のとき、テキスト領域に
//      細い横線状の描画異常(色付きの矩形)が出ることがあった。タブ切り替え
//      (フォーカス変更)だけで再現し、行の間隔=0なら再現しなかった。実機で
//      再現・修正確認済み。
//    - 根本原因: CTextDrawer::DispText()で、クリップ矩形rcClipの上端は
//      マージンを含まない`y`だが、グリフキャッシュへ渡す描画先Y
//      (旧nDrawY = GetLineMargin() + y + marginy)はマージン込みだった。
//      一方セルの高さ(nCellHeightPx = GetHankakuDy() = 文字縦幅+行間隔)は
//      マージン込みのまま。そのため、キャッシュ経由のBitBltは本来の行の
//      上端(マージン部分)を塗り残したまま、次の行のマージン部分にまで
//      はみ出して描画していた(非キャッシュのExtTextOutパスはrcClip全体を
//      ETO_OPAQUEで塗るため問題が起きない)。
//      調査時、「セルの余白がETO_OPAQUEで塗られていない」「AllocCell()の
//      シェルフ再利用で高さが不一致」等の仮説も検証したが、いずれも実測で
//      否定された(セル高さ・シェルフ割り当て・行間隔はすべて一致していた)。
//      実際の原因は「セルの中身」ではなく「BitBlt先の基準点とセル内での
//      グリフ描画位置がズレていたこと」だった。
//    - DrawOrCache()/WarmUpAscii()に新しい引数nGlyphYOffsetを追加。呼び出し側
//      (CTextDrawer.cpp)はnDestYとして必ずrcClip.top(マージン抜き)を渡し、
//      マージン分のずらしはnGlyphYOffset(=旧nDrawY - rcClip.top)として
//      別に渡す。セルの不透明フィル(ETO_OPAQUE)はセル全体(rcCellDest)に
//      対して行い、実際のグリフ描画位置だけをnGlyphYOffset分ずらすことで、
//      非キャッシュパスと同じ見た目(マージン部分は背景色、グリフはマージン
//      分下にずれた位置)を再現する。
//    - sakura_core\view\CGlyphAtlasCache.h,cpp (DrawOrCache/WarmUpAsciiに
//      nGlyphYOffset引数追加), CTextDrawer.cpp (呼び出し側)
//  - ベンチマーク・修正履歴の詳細はchangelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md,
//    NKMM_FIX_GLYPH_ATLAS_CACHE_REPORT.md, NKMM_FIX_GLYPH_ATLAS_CACHE_IMPL.md参照
//------------------------------------------------------------------
#define NKMM_FIX_GLYPH_ATLAS_CACHE

//------------------------------------------------------------------
// 描画品質(アンチエイリアスの種類)を選べるようにする 20260810
//  - LOGFONT.lfQualityは値ごとにDEFAULT/DRAFT/PROOF/NONANTIALIASED/
//    ANTIALIASED/CLEARTYPE/CLEARTYPE_NATURALの7種類が選べるが、これまでは
//    全箇所でDRAFT_QUALITY(1)固定でハードコードされていた
//  - 共通設定「全般」タブの「描画」グループに「描画品質」ドロップリストを追加。
//    タイプ別/共通のどちらのフォントを使っていてもこの設定値で上書きする
//    (LOGFONT側のlfQualityは無視する。ChooseFontコモンダイアログはlfQuality用の
//    UIを持たず、フォント選択のたびに値が保持されるとは限らないため、
//    フォント選択とは独立した1つのグローバル設定にした)
//  - 既定値はDRAFT_QUALITY(1)。これまでの見た目を変えない
//  - sakura_core\env\CommonSetting.h: CommonSetting_Window::m_nFontQuality
//  - sakura_core\env\CShareData.cpp: 既定値の設定
//  - sakura_core\env\CShareData_IO.cpp: ShareData_IO_Common()での読み書き
//  - sakura_core\prop\CPropComGeneral.cpp: ダイアログ側の実装
//  - sakura_core\view\CViewFont.cpp: CreateFont()での上書き適用
//  - sakura_core\sakura_rc.rc,sakura_rc.h,sakura.hh: IDC_COMBO_FONTQUALITY
//------------------------------------------------------------------
#define NKMM_FIX_FONT_QUALITY

//------------------------------------------------------------------
// グリフアトラスのページ内容を実DIBのままBMPファイルへダンプするデバッグ機能 20260801
//  - NKMM_FIX_GLYPH_ATLAS_CACHEとは独立にON/OFFする(通常ビルドでは無効のまま)
//  - CGlyphAtlasCache::Clear()でページを破棄する直前、GetDIBits()でHBITMAPの
//    生ピクセルを取得し、BITMAPFILEHEADER/BITMAPINFOHEADERを自前で組み立てて
//    %TEMP%\sakura_glyph_atlas_dump\へ実物のbmpとして書き出す
//    (System.Drawing/GDI+等を介さない、HDCの内容そのもの)
//  - sakura_core\view\CGlyphAtlasCache.h,cpp
//------------------------------------------------------------------
//#define NKMM_DEBUG_GLYPH_ATLAS_DUMP

//------------------------------------------------------------------
// グリフアトラスの統計をステータスバーに常時表示するデバッグHUD 20260802
//  - NKMM_FIX_GLYPH_ATLAS_CACHEとは独立にON/OFFする(通常ビルドでは無効のまま)。
//    このフラグを有効にしてもNKMM_FIX_GLYPH_ATLAS_CACHE自体が無効、または
//    共通設定「全般」タブの「グリフキャッシュを使う」がOFFのままなら何も
//    表示しない(両方ONにする必要がある)
//  - ページ数・エントリ数・(プロセス寿命での累積)ヒット数・ミス数・
//    ヒット率・WarmUpAscii()で焼いた枚数を1行のテキストで、CEditView_Paint.cpp
//    のOnPaint末尾からステータスバーの専用パーツ(index 9、末尾に追加)へ
//    SetStatusText()で書く。既存パーツ(0〜8、位置表示・検索結果の一時
//    メッセージ等)とは表示を奪い合わない。分割ウィンドウ時はアクティブ
//    ペインのみ更新する
//  - CEditWnd.cppのステータスバー分割(WM_SIZE時、nStArr/pszLabel)に、
//    このマクロが有効なときだけ末尾へ1パーツ追加する。幅は代表的な最大長
//    ラベル文字列から計算するので、実際の値がどれだけ長くても既存パーツを
//    圧迫しない。ついでにnStArr[]の配列サイズが実際のnStArrNum(9、この
//    マクロ併用時10)に対して1小さいまま(8)だった既存のバッファ書き込み
//    超過バグも一緒に直した
//  - 手描きのオーバーレイ(GetDC+ExtTextOut)、ステータスバー パーツ0の
//    使い回し、OutputDebugStringWへのログ出しは、画面上でリアルタイムに
//    確認できる・既存表示と競合しない、のどちらかを満たせず不採用にした
//    経緯がある。デバッグ時の参考として残す:
//      20260802 案1: gr/pPs->rcPaint(部分再描画矩形)に相対配置 →
//        キャレット点滅や1文字入力のような小さい範囲だけの再描画では
//        画面上端がそもそも再描画対象に入らず表示されなかった
//      20260802 案2: CEditView自身の実DCへ固定位置描画 →
//        CEditViewはScrollWindowEx()で高速スクロール時にピクセルを直接
//        シフトする子ウィンドウのため、invalidateを経由しないHUDの
//        ピクセルがスクロールで引きずられテキスト領域内に残像が残った
//      20260802 案3: フレームウィンドウの実DCへ固定位置描画 →
//        GetDCは子ウィンドウ(タブ・ツールバー)が占める領域を自動的に
//        クリップ除外するため、その領域に描いても見えなかった
//      20260802 案4: ステータスバー パーツ0(検索結果等の一時メッセージ
//        表示場所)を使い回す → パーツ0の実幅がウィンドウ幅・分割数次第で
//        狭く、全項目が入りきらず隣のパーツの再描画で尻切れに見えた。
//        文言を削って要点だけにする案・実幅を実測して"..."で削る案も
//        検討したが、そもそも見たい情報が見えなくなる本末転倒な対処だった
//      20260802 案5: OutputDebugStringWへ全項目をログ出力 →
//        DebugView等の別ツールなしに画面上でリアルタイム確認できないと
//        HUDとして意味がないとの指摘で不採用
//  - sakura_core\view\CGlyphAtlasCache.h,cpp: CGlyphAtlasCache::GetStats()
//  - sakura_core\view\CEditView_Paint.cpp
//  - sakura_core\window\CEditWnd.cpp
//------------------------------------------------------------------
//#define NKMM_DEBUG_GLYPH_ATLAS_HUD

//------------------------------------------------------------------
// タブを複製する(同じファイルを新規ウィンドウとして強制的に開く) 20260801
//  - 20170722のTODO対応。NKMM_FIX_MULTIPLE_OPEN_FILESの「既に開いている
//    ときは新しいウィンドウで開く」設定(m_bFileOpen2Open)を、この操作の
//    呼び出し中だけ一時的にONにしてOpenNewEditor()を呼ぶことで、既存の
//    「既に開いているファイルか」チェックを迂回せずに確実に新規ウィンドウ
//    として複製を開く(ファイルドロップでの多重オープンと同じ経路)
//    ※ 当初は-DUPWINコマンドラインオプションでチェックそのものを
//      バイパスする方式だったが、既存ウィンドウがロックしていない
//      ファイルでも「(新規)」になってしまう問題があったため変更した
//  - タブモードでは複製したウィンドウのタブが末尾ではなく、複製元タブの
//    右隣りに来るよう、生成後にCAppNodeManager::ReorderTab()で並び替える
//  - sakura_core\cmd\CViewCommander_Window.cpp: Command_TAB_DUPLICATE()
//------------------------------------------------------------------
#define NKMM_FIX_TAB_DUPLICATE

//------------------------------------------------------------------
// バージョン情報にサードパーティライセンス表示ボタンを追加する 20260802
//  - PCRE2/sljit/QuickJS/mimallocのライセンス全文はバージョン情報の
//    エディットボックスに収まらないため、別ウィンドウ(モーダルダイアログ)
//    に表示するボタンを追加する
//  - ライセンス文面はビルド時にsakura_core\dlg\CDlgThirdPartyLicense.cpp内へ
//    埋め込み済み(実行時に配布物からlibs以下のLICENSEファイルを探す必要がない)。
//    元ネタ: libs\pcre2\LICENCE.md, libs\deps\sljit\LICENSE,
//            libs\quickjs\LICENSE, libs\mimalloc\LICENSE
//  - 各ライブラリの節はCDlgAbout.cppと同じ#ifdef(NKMM_FIX_REGEXP_FALLBACK等)
//    でガードしているので、実際にビルドに含まれているものだけ表示される
//  - sakura_core\dlg\CDlgThirdPartyLicense.h,cpp
//------------------------------------------------------------------
#define NKMM_FIX_THIRDPARTY_LICENSE

//------------------------------------------------------------------
// WM_MOUSEMOVEの間引き(コアレシング) 20260802
//  - CEditView::DispatchEventはWM_MOUSEMOVEを受け取るたびに同期で
//    ChangeSelectAreaByCurrentCursor()→DrawSelectArea()→OnPaint()まで
//    実行する。WM_MOUSEMOVEはWM_PAINTと違いOS側で自動的に間引かれない
//    ため、マウスを素早く動かして範囲選択すると、キューに溜まった
//    古い座標のWM_MOUSEMOVEを1件ずつ同期再描画してから処理することになり、
//    選択範囲がカーソルに追いつかず遅れて見える
//  - 対策として、処理対象のWM_MOUSEMOVEを受け取った時点で同じウィンドウ
//    宛てのWM_MOUSEMOVEがキューに既にあれば(PM_NOREMOVEで覗くだけ)、
//    今回分の処理はスキップする。古い座標の処理を丸ごと飛ばして、
//    結果的に一番新しい座標のときだけ選択範囲の更新・再描画を行う
//  - WM_MOUSEMOVEは位置以外に処理必須の副作用を持たないため、
//    スキップしても最終的な選択範囲・カーソル位置には影響しない
//    (次にキューから取り出されるWM_MOUSEMOVEがより新しい座標を持つ)
//  - sakura_core\view\CEditView.cpp: DispatchEvent()のcase WM_MOUSEMOVE
//------------------------------------------------------------------
#define NKMM_FIX_MOUSEMOVE_COALESCE

//------------------------------------------------------------------
// 行バッファの縮小(メモリ節約) 20260802
//  - CMemory::AllocBuffer()は必要になったときだけバッファを拡大し、縮小は
//    しない設計(std::vectorのcapacity()と同じ考え方)。そのため、巨大な行
//    (大きい内容を貼り付けた、圧縮/難読化されたJS/JSON等を1行で開いた)を
//    一度でも経由した行は、その後編集で大部分を消してもバッファ容量が
//    その最大時のまま解放されず、ファイルを閉じるまで保持され続ける
//  - CMemory::ShrinkToFit()を追加。AllocBuffer()と同じサイズ計算式
//    (_ComputeBufSizeへ共通化)で、実データ長に対して過剰なバッファを
//    reallocで縮める。CDocLine::ShrinkToFit()、CDocLineMgr::
//    ShrinkAllLineBuffers()で全行へ伝播させ、ファイル保存の節目
//    (CSaveAgent::OnAfterSave)でまとめて回収する
//  - 毎回の編集で縮小・拡大を繰り返すと再確保コストが逆に増えるため、
//    あえて「保存時」という低頻度の節目でのみ行う(編集中は従来通り
//    拡大のみで再確保回数を抑える)
//  - 20260802 mi_collect(true)の追加: 本プロジェクトはNKMM_USE_MIMALLOC_OVERRIDE
//    によりmalloc/free/reallocがmimallocに完全上書きされている。mimallocは
//    free/reallocで縮めても内部セグメントに保持し続け、OSへは即座に返却
//    しないため、ShrinkAllLineBuffers()だけではタスクマネージャ等の
//    プロセスメモリ使用量に変化が見えないことがある。同じ節目でmimallocの
//    mi_collect(true)も呼び、未使用メモリを明示的にOSへ返却させる
//  - sakura_core\mem\CMemory.h,cpp: ShrinkToFit(), _ComputeBufSize()
//  - sakura_core\doc\logic\CDocLine.h: ShrinkToFit()
//  - sakura_core\doc\logic\CDocLineMgr.h,cpp: ShrinkAllLineBuffers()
//  - sakura_core\CSaveAgent.cpp: OnAfterSave()、mi_collect(true)
//------------------------------------------------------------------
#define NKMM_FIX_SHRINK_LINE_BUFFER

//------------------------------------------------------------------
// 元に戻す(Undo)履歴のデータ量に上限を設ける 20260802
//  - COpeBuf(Undo/Redoバッファ)は元々件数・データ量とも無制限で、
//    CDeleteOpe/CInsertOpe/CReplaceOpeが保持するテキストのコピー
//    (COpeLineData、実体はCNativeW)がセッション中ずっと積み上がり続ける。
//    超長い行を削除しても、そのコピーがUndo履歴に残り続ける限りメモリは
//    解放されない(NKMM_FIX_SHRINK_LINE_BUFFERは「現在表示されている行」の
//    バッファしか縮められないため、この分は対象外)
//  - 件数ではなくデータ量(バイト数)で上限を管理する。共通設定「編集」タブに
//    KB単位の入力欄を追加(0=無制限、既定0=従来通りの挙動を維持)
//  - COpe::GetDataByteSize()(仮想関数、既定0)をCDeleteOpe/CInsertOpe/
//    CReplaceOpeでオーバーライドし、保持するCNativeWの実バッファ容量
//    (_GetMemory()->capacity())を合計する。COpeBlk::AppendOpe()のたびに
//    加算してブロック単位のバイト数をO(1)でキャッシュし、COpeBuf側でも
//    全ブロックの合計をO(1)で追跡する(履歴全体を毎回舐めない)
//  - COpeBuf::AppendOpeBlk()の末尾で上限判定(_ShrinkToBudget())を行い、
//    超過していたら一番古い(Undo方向の)ブロックから破棄する。Redo対象
//    (m_nCurrentPointer以降)は直後に必要になり得るため破棄しない。
//    「保存済みに一致する」基準点(m_nNoModifiedIndex、行ごとの変更行
//    表示に使う)が破棄対象に含まれていた場合は-1(追跡不能)にする。
//    この場合、行ごとの「変更行」表示が実態より多め(安全側)になるだけで、
//    ファイル全体の変更フラグ(CDocEditor::IsModified())は別の独立した
//    フラグのため影響を受けない
//  - sakura_core\env\CommonSetting.h: CommonSetting_Edit::m_nUndoBufMaxKB
//  - sakura_core\env\CShareData.cpp,CShareData_IO.cpp: 既定値・INI永続化
//  - sakura_core\COpe.h: COpe::GetDataByteSize()
//  - sakura_core\COpeBlk.h,cpp: COpeBlk::GetByteSize()
//  - sakura_core\COpeBuf.h,cpp: COpeBuf::_ShrinkToBudget()
//  - sakura_core\prop\CPropComEdit.cpp、sakura_rc.h,rc: 共通設定UI
//    (IDD_PROP_EDIT「編集」タブに追加。EN_US言語版rcは未対応 20260802。
//    未対応でもビルド・実行は可能で、この設定のUIが出ないだけ)
//  - 20260802 アップダウンコントロール(IDC_SPIN_UNDOBUFMAXKB)を追加。
//    Win32のアップダウンコントロールは刻み幅をネイティブに持たず、矢印
//    クリックのたびに来るUDN_DELTAPOS通知をアプリ側で解釈する仕組みのため、
//    他の項目(1刻み)と違いここでは4KBずつ増減させている
//------------------------------------------------------------------
#define NKMM_FIX_UNDO_BUFFER_LIMIT

//------------------------------------------------------------------
// 共通設定「キー割り当て」の隣に「ショートカット一覧」タブを追加する 20260803
//  - 既存の「キー割り当て」タブは1キーずつ選ぶ編集用UIで余白がなく、
//    「機能名とショートカットを対にした一覧」を見る手段がなかったため、
//    俯瞰用の読み取り専用タブを追加(一覧生成はCKeyBind::CreateKeyBindList()を流用)
//  - sakura_core\prop\CPropCommon.h,cpp, CPropComKeybindList.cpp(新規)
//  - 詳細はchangelog/NKMM_FIX_KEYBIND_LIST_TAB.md参照
//------------------------------------------------------------------
#define NKMM_FIX_KEYBIND_LIST_TAB

//------------------------------------------------------------------
// 表示行レイアウト管理のダングリングポインタ修正 20260805
//  - CLayoutMgr::DeleteLayoutAsLogical()で、複数論理行削除時に位置キャッシュ
//    m_pLayoutPrevReferが削除対象ノードを指したまま残る(解放済みメモリを
//    参照する)不具合を修正。Releaseビルドでは無警告で発生し、直後の
//    カーソル移動・再描画で不定期クラッシュを招きうる。実機確認済み
//  - sakura_core\doc\layout\CLayoutMgr.cpp: CLayoutMgr::DeleteLayoutAsLogical()
//  - 詳細はchangelog/NKMM_FIX_LAYOUT_DANGLING_PREVREFER.md参照
//------------------------------------------------------------------
#define NKMM_FIX_LAYOUT_DANGLING_PREVREFER

//------------------------------------------------------------------
// ステータスバーの文字数表示をO(1)キャッシュ化 20260806
//  - GetDocumentWordNum()が矢印キー1回ごとに全行を数え直しており、巨大
//    ファイルでカーソル移動が重くなる原因になっていた。文字数をCEditDocに
//    キャッシュし、実際にテキストが書き換わる箇所で挿入/削除ぶんだけ差分
//    更新する方式に変更。実機確認済み(タイプ入力で更新されない不具合が
//    一度見つかり修正済み。詳細はchangelog参照)
//  - sakura_core\COpe.h,cpp / doc\CEditDoc.h,cpp / view\CEditView.cpp,
//    CEditView_Command_New.cpp / doc\layout\CLayoutMgr_New2.cpp
//  - 詳細はchangelog/NKMM_FIX_STATUSBAR_WORDNUM_CACHE.md参照
//------------------------------------------------------------------
#define NKMM_FIX_STATUSBAR_WORDNUM_CACHE

//------------------------------------------------------------------
// 「折り返さない」時のテキスト最大幅算出の高速化(次点候補8件方式) 20260806
//  - 最長行が編集で無効化されるたびに全行スキャンしていたのを、幅上位8件の
//    候補リストで大半回避。NKMM_FIX_TEXTWIDTH_MULTISET_CACHE(上位版、下記)の
//    #else側に残置。通常はそちらが有効なので本フラグの出番はない
//  - sakura_core\doc\layout\CLayoutMgr.h, CLayoutMgr_New.cpp,
//    CLayoutMgr_DoLayout.cpp
//  - 詳細はchangelog/NKMM_FIX_TEXTWIDTH_TOPK_CACHE.md参照
//------------------------------------------------------------------
#define NKMM_FIX_TEXTWIDTH_TOPK_CACHE

//------------------------------------------------------------------
// 「折り返さない」時のテキスト最大幅算出をstd::multisetで常時追従 20260806
//  - NKMM_FIX_TEXTWIDTH_TOPK_CACHEの上位版。全行の(幅,CLayout*)を常時集合で
//    保持し、全行スキャンを原理上不要にする。CLayout*をキーに使うことで
//    行番号シフト処理が不要。実機確認済み
//  - sakura_core\doc\layout\CLayoutMgr.h, CLayoutMgr.cpp, CLayoutMgr_New.cpp,
//    CLayoutMgr_DoLayout.cpp
//  - 詳細はchangelog/NKMM_FIX_TEXTWIDTH_MULTISET_CACHE.md参照
//------------------------------------------------------------------
#define NKMM_FIX_TEXTWIDTH_MULTISET_CACHE

//------------------------------------------------------------------
// ファイル読み込み時の行バッファを「必要分だけ」確保する(べき乗切り上げなし) 20260809
//  - CMemory::AllocBuffer()は追記(AppendRawData等)の償却コストを抑えるため
//    必要サイズを次のべき乗に切り上げて確保する。しかしファイル読み込みは
//    CReadManager::ReadFile_To_CDocLineMgrが1行ずつ最終サイズ確定済みの
//    データをCDocEditAgent::AddLineStrX→CDocLine::SetDocLineStringで
//    一括セットするだけで、その後追記で伸長することはない。にもかかわらず
//    従来はAllocBuffer()を共用していたため、行ごとに最大で約2倍(平均約1.4倍)
//    の未使用余白を抱えたまま保持し続けていた(500MB程度のファイルで数百MB
//    規模の無駄)
//  - AllocBuffer()自体には手を入れず(編集時の償却成長の挙動を変えないため)、
//    べき乗切り上げなしで「実データ長+8Byte整列」のみ行う
//    AllocBufferExact()/SetRawDataExact()を別関数として追加し、
//    ファイル読み込み経路(AddLineStrX)だけがそちらを使うようにする
//  - AddLineStrXの呼び出し元はReadFile_To_CDocLineMgrのみ(1行末尾追加専用)
//    なので、この経路を切り替えても編集時のタイピング性能への影響はない
//  - sakura_core\mem\CMemory.h,cpp: AllocBufferExact(), SetRawDataExact()
//  - sakura_core\mem\CNativeW.h,cpp: SetStringExact()
//  - sakura_core\doc\logic\CDocLine.h,cpp: SetDocLineStringExact()
//  - sakura_core\doc\CDocEditor.cpp: CDocEditAgent::AddLineStrX()
//------------------------------------------------------------------
#define NKMM_FIX_LOAD_EXACT_LINE_BUFFER

//
//#define USE_SSE2

#endif /* MY_CONFIG_H */
