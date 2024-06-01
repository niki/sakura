﻿// -*- mode:c++; coding:utf-8-ws -*-
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
- [ ] 20170722 同じファイルのウィンドウを複製する

- [x] 20150702 開いているタブのファイル名をコピー

- [ ] 20170110 bug?, 画面上端下端でキャレットが消えたタイミングでスクロールさせるとキャレットが消えたままスクロールする

- [ ] 20150605 bug, カーソル移動したときに移動前の状態が一瞬残る
  \view\CEditView_Scroll.cpp:void CEditView::ScrollDraw() があやしい?
  ScrollWindowEx() で行われる更新をなんとかすればいい?

- [ ] 20150804 タスクバーアイコンのちらつき
  \window\CTabWnd.cpp:CTabWnd::ShowHideWindow()
  SendMessageTimeout() と TabWnd_ActivateFrameWindow() の関係

- [ ] 20170303 テキスト描画, 文字の右端が欠ける, ExtTextOutのタイミング
  [patchunicode:#588]をあてて目立たなくはしている, 続けて描画されれば欠けない
  [patchunicode:#860]をあてれば解消しそう
    sakura_core\view\CEditView_Paint.cpp
    pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);

- [ ] 20170404 BkSpを押したときにタブ入力文字だけしかない場合は逆TABにする

- [ ] 20170607 #RRGGBB を色付け

- [ ] 20170611 空白タブ改行行番号の表示切替

- [x] 20170611@20170620 Grep, Exclude dirsの追加、検索時に # をつけて「ファイル」につなげる
  Grep, 「$cpp」を「*.c *.cpp *.cc *.cxx *.c++ *.h *.hpp」などに展開する機能
*/

// ● フォント
//   https://support.microsoft.com/ja-jp/kb/74299
//   http://d.hatena.ne.jp/itoasuka/20100104/1262585983
//
// lf.lfHeight = DpiPointsToPixels(-10); // 高DPI対応（ポイント数から算出）

// 修正者
#define NKMM_AUTHOR       "NekoMiMi Co."
#define NKMM_AUTHOR_PAGE  "https://github.com/niki/sakura"

// 拡張用レジストリキー
#define NKMM_REGKEY _T("Software\\sakura-niki")

//------------------------------------------------------------------
// バージョン情報ダイアログの変更 20170315
//------------------------------------------------------------------
#define NKMM_FIX_VERDLG
	#define PR_VER      2,3,2,0
	#define PR_VER_STR "2.3.2.0"
	#define PR_LV		20
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
//  ? 各ウィンドウのタブウィンドウは生成時に自身の位置が選択されている状態から始まる
//    オーダーが変わらない限り選択タブが変わることはない
//    ウィンドウ切り替え時に自身が選択されたタブウィンドウが表示されることでタブを切り替えたように表現しているだけ
//    そのためスクロール状態からの切り替え時にスクロール位置が同期していない
//------------------------------------------------------------------
#define NKMM_FIX_TABWND
	#define NKMM_TABWND_FLICKER     (1)  // ウィンドウまとめモードの切り替え時にスリープを10ms入れる(ちらつき抑制) 20170406
	#define NKMM_TAB_CLOSE_BTN_DRAW (0)  //〆 タブを閉じるボタンをグラフィカルにする 20170423
	#define NKMM_BUGFIX_TAB_EDGE    (1)  // 間に選択タブがあると右側のエッヂがないバグを修正 (となりのタブが上書き描画していた) 20170429

//------------------------------------------------------------------
// エディット画面 スクロールバー
//  - 検索結果を表示 20170609
//  - ブックマークを表示 20170609
//  - カーソル行を表示 20170611
//  - スクロールバーの再描画をマーカー描画のタイミングに合わせて更新する 20170721
//  ? バーにカーソルを乗せた時, フェードアウトして消えてしまう:(
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
//------------------------------------------------------------------
#define NKMM_FIX_NUMERIC_COLOR

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
	#define NKMM_SEPARATE_HISTORY                    (1)  // 履歴は別ファイルで扱う (sakura.recent) 20170502
	#define NKMM_DELETE_HISTORY_NOT_EXIST_AT_STARTUP (1)  // 起動時に存在しないファイル・フォルダの履歴は削除する 20170410
	#define NKMM_USE_KEYWORDSET_CSV                  (1)  // sakura.keywordset.csvを用意し、強調キーワードの管理はこのファイルで行う 20170513

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
// デフォルト文字コードを UTF8にする 20170622
//------------------------------------------------------------------
#define NKMM_FIX_DEFAULT_CHARCODE_TO_UTF8

//------------------------------------------------------------------
// 正規表現検索の際、検索文字列の正規表現記号をクォートする 20150601
// PHPの preg_quote() みたいなもの
//------------------------------------------------------------------
#define NKMM_FIX_SEARCH_KEY_REGEXP_AUTO_QUOTE

//------------------------------------------------------------------
// メニューアイコン (未実装)
//  ! ビットマップメニュー
//      ::InsertMenuItem( hMenu, 0xFFFFFFFF, TRUE, &mii );
//      http://home.a00.itscom.net/hatada/windows/introduction/menu01.html
//      http://eternalwindows.jp/winbase/menu/menu10.html
//    起動時にアイコンの数だけHBITMAPを生成する
//      \sakura_core\uiparts\CImageListMgr.cpp
//------------------------------------------------------------------
//#define NKMM_FIX_MENUICON

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
//  - 検索
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
//------------------------------------------------------------------
#define NKMM_FIX_FIND_DIALOG

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


//
//#define USE_SSE2

#endif /* MY_CONFIG_H */
