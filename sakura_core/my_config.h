#ifndef MY_CONFIG_H
#define MY_CONFIG_H

// clang-format off

// 2016.7.28, 2.1.1.4(r3825)ベース
// 2016.7.29, 2.2.0.1(r4011)ベース

/* --------------------------------------------------------------------------
●やりたいこと.

- [x] #1. 2015.6.1
 検索時、選択している文字列があった場合に正規表現にチェックが入っていたら
 正規表現で使用する記号を自動的にクォートする.
 PHPの preg_quote みたいなもの.

- [ ] #2. 2015.6.2
 検索文字列がある行番号の色を変更したい.

- [ ] #3. 2015.6.5
 bug?, 単語を削除する際、一瞬選択状態になる

- [x] #4. 2015.6.5
 bug, カーソル移動したときに移動前の状態が一瞬残る
 \view\CEditView_Scroll.cpp:void CEditView::ScrollDraw() があやしい?
 ScrollWindowEx() で行われる更新をなんとかすればいい?

- [x] #5. 2015.6.5
 Grep対象のフォルダの複数指定.

- [ ] #6. 2015.6.8
 起動時、最近使ったファイルの整理. 存在しないファイルの項目を排除.

- [x] #7. 2015.6.9 *pending
 デフォルトの正規表現ライブラリを使用する.
 → re2 http://naoyat.hatenablog.jp/entry/2012/01/12/220812  
 → SRELL http://www.akenotsuki.com/misc/srell/  

- [ ] #8. 2015.6.9
 ウィンドウ一覧, DELでウィンドウを閉じる.
 \window\CTabWnd.cpp : CTabWnd::TabListMenu : TrackPopupMenu()
 → すでにパッチあり [patchunicode:#1072] ウィンドウ一覧ダイアログ 

- [x] #9. 2015.6.10
 ステータスバーにタイプ名を表示.
 \window\CMainStatusBar.cpp : CMainStatusBar::CreateStatusBar()
 \apiwrap\CommonControl.h : ApiWrap::StatusBar_SetParts
 \window\CEditWnd.cpp : CEditWnd::OnSize2 : nStArr カラムサイズ

- [x] #10. 2015.6.11
 iniをレジストリによる読み書きにする.

- [ ] #11. 2015.6.30
 コメント内の検索文字列はコメント色の影響を受ける.

- [ ] #12. 2015.7.2
 開いているタブのファイル名をコピー.

- [ ] #14. 2015.8.4
 タスクバーアイコンのちらつき
 \window\CTabWnd.cpp:CTabWnd::ShowHideWindow()
 SendMessageTimeout() と TabWnd_ActivateFrameWindow() の関係

- [ ] #16. 2015.8.25
 タブを切り替えた際、タブがクリックした位置とは別の位置にスクロール?するのを直したい
 LRESULT CTabWnd::OnTabLButtonUp( WPARAM wParam, LPARAM lParam )
 void CTabWnd::ShowHideWindow( HWND hwnd, BOOL bDisp )
 void CTabWnd::AdjustWindowPlacement( void )
 void CTabWnd::LayoutTab( void )
 2001年12月: TM SOFT(http://tm-soft.seesaa.net/archives/20011218-1.html)
 TabCtrl_GetItemRect()

- [x] #17. 2015.11.23
 置換のとき、置換後ボックスに置換前のテキストを入れる  

- [ ] #18. 2016.12.13
 各ダイアログボックスの位置をデスクトップの中央ではなく  
 呼び出し元ウィンドウの中央、または左上に配置する  
  →Grep検索中
  →外部コマンド実行ダイアログ

- [ ] #19. 2017.1.5
 コメント内の空白タブが検索文字列のとき、背景色が空白タブに反映されない
 REI_MOD_SP_COLOR の修正による影響  

- [x] #20. 2017.1.10
 選択中のテキストの装飾は元のテキストを優先する

- [ ] #21. 2017.1.10
 bug?, 画面上端下端でキャレットが消えたタイミングでスクロールさせるとキャレットが消えたままスクロールする

- [ ] #22. 2017.3.3
 テキスト描画、右端が欠ける
 [patchunicode:#588]をあてて目立たなくはしている
 sakura_core\view\CEditView_Paint.cpp
 pcFigureManager->GetTextFigure().DrawImp(pInfo, nPosBgn, pInfo->GetPosInLogic() - nPosBgn);

-------------------------------------------------------------------------- */

// ● フォント
//   https://support.microsoft.com/ja-jp/kb/74299
//   http://d.hatena.ne.jp/itoasuka/20100104/1262585983
//
// lf.lfHeight = DpiPointsToPixels(-10); // 高DPI対応（ポイント数から算出）

//-------------------------------------------------------------------------
// デバッグ用
//-------------------------------------------------------------------------

// デバッグ出力 2015.3.24  
#define REI_OUTPUT_DEBUG_STRING 0


//-------------------------------------------------------------------------
// 編集
//-------------------------------------------------------------------------

// 水平スクロールの変更  
//  - スクロール開始マージンを 1 に変更。画面の端でスクロール開始 2014.5.7  
//  - スクロール幅を 16 に設定。一度に大きく移動することで見やすくする (動きはメモ帳参照) 2015.9.2  
#define REI_MOD_HORIZONTAL_SCR 16

// 垂直スクロールの変更
//  - スクロールマージン行を調整
#define REI_MOD_VERTICAL_SCR 0

// タブ文字のタブと空白の切り替えを追加
//  - S_ChangeTabWidthマクロに負の値を渡すとタブ文字の切り替え
#define REI_MOD_CHANGE_TAB_WIDTH_MACRO 1


//-------------------------------------------------------------------------
// 表示
//-------------------------------------------------------------------------

// 変更ドキュメントタブ名カラーを設定
// (REG/ModifiedTabCaptionColor:0x00BBGGRR)
#define REI_MOD_MODIFIED_TAB_CAPTION_COLOR (0x000000d8)

// 行を中央ぞろえにする 2014.3.26 - 2015.7.24  
//  - デフォルトでは行は上揃えになっているので行間は下に付加される  
//  - キャレットを行間含む高さにする  
#define REI_LINE_CENTERING 1

// EOFのみの行に行番号を表示 2017.3.10
#define REI_MOD_EOFLN_DISP_NR 1

// キャレットの変更 2012.10.11  
// 0: 変更なし
// 1-10: キャレットサイズ
// 11: 1バイトコードの時は1px、2バイトコードの時は2px  
// 12: 半角入力の時は1px、全角入力の時は2px 2015.8.26  
// (REG/CaretType:2)
#define REI_MOD_CARET 2

// 半角空白文字を "･" で描画 2013.6.2  
#define REI_MOD_HAN_SPACE 1

// 全角空白文字
#define REI_MOD_ZEN_SPACE 

// タブ文字（矢印）の鏃(>)は表示しない 2015.5.25  
//  - 「長い矢印」「短い矢印」→「線」 2016.8.19
#define REI_MOD_TAB 1

// カーソル行アンダーライン
//  - 左端から 2015.1.30  
#define REI_MOD_CUR_UL 1

// 選択領域の色を変更  
//  - テキストと背景のブレンド率設定 2015.6.5  
//  - 選択時のテキスト属性（太字、下線）に選択領域ではなく現在のテキストを使用する
#define REI_MOD_SELAREA 1
  // 選択領域のブレンド率[%] 2015.6.5  
  // (REG/SelectAreaBlendPer:0x00000064)
  #define REI_MOD_SELAREA_BLEND_PER (/*Text=*/(0 << 8) | /*Back=*/(100))

// 空白TAB,改行のカラーを変更 2015.6.5  
//  - 現在のテキスト色と現在の背景色をブレンドする (空白TABのカラー設定は無視されます) 2015.6.8  
//    対象は空白TABなどで、コントロールコードには適用されません  
#define REI_MOD_SP_COLOR 1
  // 空白TAB 現在のカラーのブレンド率[%] 2015.6.5  
  // (REG/WhiteSpaceBlendPer:0x0000001E)
  #define REI_MOD_SP_BLEND_PER (30)
//						     
						     

// カラー設定の変更 2016.6.25  
//  - カーソル位置縦線、折り返し記号をテキストカラーのみ
#define REI_MOD_COLOR_STRATEGY 1

// コメント行の修正 2016.12.27
// 0x01: 改行以降もカラーを有効にする
#define REI_MOD_COMMENT 1

// 折り返し記号表示時に折り返し位置の線を引かない 2016.6.25  
// (REG/NoWrapLine:1)
#define REI_MOD_WRAP_LINE 1

// ステータスバーを変更 2015.6.10  
//  - タイプ名を表示  
//  - タブサイズを表示  
//  - RECの色を赤にする  
//  - 「? 行 ? 桁」→「(?, ?)」に変更、左端に表示  
#define REI_MOD_STATUSBAR 1

//-------------------------------------------------------------------------
// 機能
//-------------------------------------------------------------------------

// プロファイルの読み書きにレジストリを使用する
//  - レジストリキーがない場合はiniファイルから読み込む
//  - バージョンアップ時のバックアップファイル作成は行わない
// (REG/NoReadProfilesFromRegistry:0) 1にするとレジストリから読み込まなくなります
// (REG/NoWriteProfilesToRegistry:0) 1にするとレジストリに書き込まなくなります
#define REI_USE_REGISTRY_FOR_PROFILES 1

// 開かれているファイルを自己管理する前提で多重オープンの許可 2013.6.19  
//  - Shiftを押しながらファイルドロップで多重オープン  
#define REI_MULTIPLE_OPEN_FILES 1

// 最大数を変更 2013.10.2, 2016.12.13  
// \sakura_core\config\maxdata.h
// (REG/RecentSearchKeyMax:16)
// (REG/RecentReplaceKeyMax:16)
// (REG/RecentGrepFileMax:8)
// (REG/RecentGrepFolderMax:16)
#define REI_CHG_MAXDATA 1

// 正規表現検索の際、検索文字列の正規表現記号をクォートする 2015.6.1  
// (REG/RegexpAutoQuote:1)
#define REI_MOD_SEARCH_KEY_REGEXP_AUTO_QUOTE 1

// Grep変更 2015.8.24  
//  - Grepするフォルダの指定を UI的に増やす
//    →カンマで区切るより分かれていた方が扱いやすいし、履歴管理もしやすい.
//  - 指定フォルダをすべてチェックをはずすと「現在編集中のファイルから検索」とする
//  - 「現在編集中のファイルから検索」をチェックした時の状態を保持しないようにする
//    →本来の「現在編集中のファイルから検索」を使用すると、
//      影響を受ける他のチェックボックスの状態が変更したまま戻らないのが嫌だから.
//  - ファイル(フィルタ)指定はフォルダのあとに置く (フォルダのほうが変更する機会が多いため)  
//  - 検索中のダイアログを中央に置かない（リアルタイム時に見づらい）
#define REI_MOD_GREP 1

// 置換変更 2016.8.4  
// (REG/ReplaceTextToText:1)
//  - 置換後文字列に置換前文字列を設定する  
#define REI_MOD_REPLACE 1

// フォルダ選択ダイアログを変更 2015.8.25  
//  - CLSID_FileOpenDialogを使用する  
//    使用するには Vista以降にする必要がある  
//      WINVER=0x0500;_WIN32_WINNT=0x0500;_WIN32_IE=0x0501  
//                          ↓  
//      WINVER=0x0601;_WIN32_WINNT=0x0601;_WIN32_IE=0x0800  
// [http://eternalwindows.jp/installer/originalinstall/originalinstall02.html]
// [https://msdn.microsoft.com/ja-jp/library/windows/desktop/ff485843(v=vs.85).aspx]
#define REI_MOD_SELECTDIR 1

// ダイレクトタグジャンプ一覧を変更
//  - 表示するカラムの選別と並び替え 2015.8.27  
//  - 常に中央に表示(サイズは継承) 2017.2.15
#define REI_MOD_DIRECTTAGJUMP 1

// 外部コマンド実行ダイアログを変更 2016.12.20  
//  - ウィンドウの位置が決まった位置にでるようにする
#define REI_MOD_EXECDLG 1

// アウトライン解析ダイアログの変更 2016.12.14  
//  - フォントをメインフォントにする
//  - ドッキング時にウィンドウカラーをテキストに合わせない
// (REG/OutlineDockColorDefault:0)
#define REI_MOD_OUTLINEDLG 1

// バージョン情報ダイアログの変更 2017.3.15
#define REI_MOD_VERDLG 1

// ウェイトカーソルを変更 2015.7.9  
//  - 一部、正しい位置に修正  
//  - 文字列削除時に表示しない（アンドゥのときなど）  
#define REI_MOD_WAITCUESOR 1

// SetMainFontを修正
//  - 引数ptを追加、デフォルトを10ポイントにする 2017.3.7
#define REI_MOD_SET_MAIN_FONT 1


//-------------------------------------------------------------------------
// 修正
//-------------------------------------------------------------------------

// 検索ダイアログの「正規表現」が影響を受けないようにする
//  - 検索マーク切り替え時 2015.6.1  
//  - インクリメンタルサーチ時 2016.12.14
#define REI_FIX_SEARCH_KEEP_REGEXP 1

// 行番号が非表示でブックマークが表示のときブックマークは線で描画する 2017.1.13
#define REI_FIX_DRAW_BOOKMARK_LINE_NOGYOU 1

// ルーラー非表示時は「ルーラーとテキストの隙間」を無視する 2016.12.28
#define REI_FIX_RULER_HIDE_IGNORE_BOTTOM_SPACE 0

// 行番号縦線を行番号の色で描画する 2016.12.21
#define REI_FIX_LINE_TERM_TYPE 1

// カーソル上下移動時に次の条件?のときに画面の更新が間に合わずに描画が崩れる 2015.8.4  
//  - キーリピートが早い  
//  - 裏で描画を頻繁に行うアプリが動いている  
// →UpdateWindow() を呼び出すことで一時対応  
#define REI_FIX_CALL_CURSOR_MOVE_UPDATEWINDOW 1

// カーソル移動時のちらつきを暫定で対処 2015.8.4  
//  - スクロールした時に ScrollWindowEx() と再描画の同期がとれていない?  
//  - MacTypeなどを使用すると描画の負荷が高くなり顕著になる。使わない場合は高速にやると再現する  
// →MoveCursor()に処理をまとめてしばらく様子見 2015.8.5  
#define REI_FIX_CURSOR_MOVE_FLICKER 1

// タブウィンドウの処理を修正 2015.8.28  
//  - しなくていい処理をちゃんとしないようにする  
//  - 1. はじめて切り替えたタブが左側にある場合にタブの位置が右にズレる問題 (対応中)  
//    2. Refresh()の処理が関係しているようだ  
//    3. どうやらすべてのウィンドウがタブコントロールを持っていてタブが変更されるたびに同期しているようだ  
//       2.の挙動が起こるのはそのせい  
//       それならばタブの位置を同期することはできないか？  
#define REI_FIX_TABWND 


//
//#define USE_SSE2

// clang-format on

#include <windows.h>
#include <memory>
#include <string>
//#include <locale>
//#include <codecvt>
//#include <vector>
#include <errno.h>

//------------------------------------------------------------------
//! レジストリキーの読み取り(DWORD)
//! @param key_name
//! @param defValue
//------------------------------------------------------------------
inline DWORD RegGetDword(LPCTSTR key_name, DWORD defValue) {
  HKEY hKey;
  DWORD dwType = REG_DWORD;  // REG_SZ;
  DWORD dwByte = 4;
  DWORD dwValue;
  LONG rc;

  std::wstring subkey = L"Software\\sakura_REI";

  rc = RegOpenKeyEx(HKEY_CURRENT_USER,  //親キー
                    subkey.c_str(),     //サブキー
                    0,                  //常にゼロ
                    KEY_ALL_ACCESS,     //セキュリティマスク
                    &hKey);             //キーのハンドル

  if (rc != ERROR_SUCCESS) return defValue;

  rc = RegQueryValueEx(hKey,              //キーのハンドル
                       key_name,          //読み取るキーの名前
                       NULL,              //常に NULL
                       &dwType,           //データのタイプ
                       (BYTE *)&dwValue,  //受け取る領域
                       &dwByte);          //領域のバイト数(受け取ったバイト数)

  RegCloseKey(hKey);

  if (rc != ERROR_SUCCESS) {
    return defValue;
  } else {
    return dwValue;
  }
}

//------------------------------------------------------------------
//! レジストリキーの読み取り(SZ)
//! @param key_name
//! @param out
//------------------------------------------------------------------
inline bool RegGetString(LPCTSTR key_name, char *out) {
  HKEY hKey;
  DWORD dwType;
  DWORD dwByte;
  LONG rc;

  std::wstring subkey = L"Software\\sakura_REI";

  rc = RegOpenKeyEx(HKEY_CURRENT_USER,  //親キー
                    subkey.c_str(),     //サブキー
                    0,                  //常にゼロ
                    KEY_ALL_ACCESS,     //セキュリティマスク
                    &hKey);             //キーのハンドル

  if (rc != ERROR_SUCCESS) return false;

  rc = RegQueryValueEx(hKey,      //キーのハンドル
                       key_name,  //読み取るキーの名前
                       NULL,      //常に NULL
                       &dwType,   //データのタイプ
                       NULL,      //受け取る領域
                       &dwByte);  //領域のバイト数(受け取ったバイト数)

  if (rc != ERROR_SUCCESS) return false;

  rc = RegQueryValueEx(hKey,                  //キーのハンドル
                       key_name,              //読み取るキーの名前
                       NULL,                  //常に NULL
                       &dwType,               //データのタイプ
                       (LPBYTE)(LPCTSTR)out,  //受け取る領域
                       &dwByte);              //領域のバイト数(受け取ったバイト数)

  RegCloseKey(hKey);

  if (rc != ERROR_SUCCESS) {
    return false;
  } else {
    return true;
  }
}

//------------------------------------------------------------------
//! ファイル名の取得
//! @param path パス名
//------------------------------------------------------------------
inline std::wstring ExtractFileName(const std::wstring &path) {
  std::wstring fname;
  size_t pos = path.rfind('\\');
  if (pos != std::wstring::npos) {
    return path.substr(pos + 1, path.size() - pos - 1);
  } else {
    pos = path.rfind('/');
    if (pos != std::wstring::npos) {
      return path.substr(pos + 1, path.size() - pos - 1);
    }
  }
  return path;
}

//------------------------------------------------------------------
//! レジストリキーの確認
//! @param profile_name プロファイル名
//! @return true:存在する, false:存在しない
//------------------------------------------------------------------
inline bool IsExistProfileReg(const std::wstring &profile_name) {
  HKEY hKey;
  LONG rc;

  std::wstring fname = ExtractFileName(profile_name);
  std::wstring subkey = L"Software\\" + fname;

  rc = RegOpenKeyEx(HKEY_CURRENT_USER,  //親キー
                    subkey.c_str(),     //サブキー
                    0,                  //常にゼロ
                    KEY_ALL_ACCESS,     //セキュリティマスク
                    &hKey);             //キーのハンドル

  if (rc != ERROR_SUCCESS) return false;

  RegCloseKey(hKey);

  return true;
}

//------------------------------------------------------------------
//! レジストリキーの読み取り(SZ)
//! @param profile_name プロファイル名
//! @param section_name セクション名
//! @param key_name キー名
//! @param data データ
//------------------------------------------------------------------
inline bool GetRegProfileString(const std::wstring &profile_name, const std::wstring &section_name,
                                const std::wstring &key_name, std::wstring &data) {
  HKEY hKey;
  DWORD dwType = REG_SZ;
  DWORD dwByte = 1024;
  LONG rc;

  std::wstring fname = ExtractFileName(profile_name);
  std::wstring subkey = L"Software\\" + fname + L"\\" + section_name;

  data = L"";

  rc = RegOpenKeyEx(HKEY_CURRENT_USER,  //親キー
                    subkey.c_str(),     //サブキー
                    0,                  //常にゼロ
                    KEY_ALL_ACCESS,     //セキュリティマスク
                    &hKey);             //キーのハンドル

  if (rc != ERROR_SUCCESS) return false;

  rc = RegQueryValueExW(hKey,              //キーのハンドル
                        key_name.c_str(),  //読み取るキーの名前
                        NULL,              //常に NULL
                        &dwType,           //データのタイプ
                        NULL,              //受け取る領域
                        &dwByte);          //領域のバイト数(受け取ったバイト数)

  if (rc != ERROR_SUCCESS) return false;

  if (dwType == REG_DWORD) {
    int i;
    rc = RegQueryValueExW(hKey,              //キーのハンドル
                          key_name.c_str(),  //読み取るキーの名前
                          NULL,              //常に NULL
                          &dwType,           //データのタイプ
                          (BYTE *)&i,        //受け取る領域
                          &dwByte);          //領域のバイト数(受け取ったバイト数)

    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS) {
      return false;
    } else {
      data.assign(std::to_wstring(i));
      return true;
    }

  } else {
    wchar_t *buffer = new wchar_t[dwByte + 1];

    rc = RegQueryValueExW(hKey,              //キーのハンドル
                          key_name.c_str(),  //読み取るキーの名前
                          NULL,              //常に NULL
                          &dwType,           //データのタイプ
                          (LPBYTE)buffer,    //受け取る領域
                          &dwByte);          //領域のバイト数(受け取ったバイト数)

    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS) {
      delete[] buffer;
      return false;
    } else {
      if (dwByte == 0) {
        data.assign(L"");
      } else {
        buffer[dwByte] = L'\0';
        data.assign(buffer);
      }
      delete[] buffer;
      return true;
    }
  }
}

//------------------------------------------------------------------
//! レジストリキーの書き込み(SZ)
//! @param profile_name プロファイル名
//! @param section_name セクション名
//! @param key_name キー名
//! @param data データ
//------------------------------------------------------------------
inline bool SetRegProfileString(const std::wstring &profile_name, const std::wstring &section_name,
                                const std::wstring &key_name, const std::wstring &data) {
  HKEY hKey;
  DWORD dwType = REG_SZ;
  DWORD dwDisposition;  //新規作成:REG_CREATED_NEW_KEY
                        //既存:REG_OPENED_EXISTING_KEY
  LONG rc;

  std::wstring fname = ExtractFileName(profile_name);
  std::wstring subkey = L"Software\\" + fname + L"\\" + section_name;

  rc = RegCreateKeyEx(HKEY_CURRENT_USER,        //親キー
                      subkey.c_str(),           //サブキー
                      0,                        //常にゼロ
                      NULL,                     //
                      REG_OPTION_NON_VOLATILE,  //再起動しても消えない設定
                      KEY_ALL_ACCESS,           //セキュリティマスク
                      NULL,                     //
                      &hKey,                    //
                      &dwDisposition);

  if (rc != ERROR_SUCCESS) return false;

  int i = 0;
  bool is_num = false;

  if (!data.empty()) {
    wchar_t *endptr;
    errno = 0;
    i = wcstol(data.c_str(), &endptr, 10);
    is_num = !(*endptr != L'\0' || (i == INT_MAX && errno == ERANGE));
  }

  if (is_num) {
    rc = RegSetValueEx(hKey,              //キーのハンドル
                       key_name.c_str(),  //読み取るキーの名前
                       NULL,              //常に NULL
                       REG_DWORD,         //データのタイプ
                       (CONST BYTE *)&i,  //書き込む領域
                       sizeof(DWORD));    // dataのサイズを指定する
  } else {
    rc = RegSetValueEx(hKey,                                 //キーのハンドル
                       key_name.c_str(),                     //読み取るキーの名前
                       NULL,                                 //常に NULL
                       REG_SZ,                               //データのタイプ
                       (CONST BYTE *)(LPCTSTR)data.c_str(),  //書き込む領域
                       (int)data.length() * 2);              // dataのサイズを指定する
  }

  RegCloseKey(hKey);

  if (rc != ERROR_SUCCESS) {
    return false;
  } else {
    return true;
  }
}

#endif /* MY_CONFIG_H */
