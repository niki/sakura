/*!	@file
	@brief Lua Script Handler

	@date 2017.5.21
*/
/*
	Copyright (C) 2017, 

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

/*
The Programming Language Lua <http://www.lua.org/>
Lua: license <https://www.lua.org/license.html>
Lua Binaries <http://luabinaries.sourceforge.net/>

良いもの。悪いもの。: Lua基礎文法最速マスター <http://handasse.blogspot.com/2010/02/lua.html>

Lua: 4.1: Functions and Types <http://pgl.yoyo.org/luai/i/4.1+Functions+and+Types>
Lua 5.1 リファレンスマニュアル <http://milkpot.sakura.ne.jp/lua/lua51_manual_ja.html#luaL_checkstring>
Lua言語のライブラリ関数 <http://www.rtpro.yamaha.co.jp/RT/docs/lua/tutorial/library.html#string.format>

LuaのC++組み込み方自分用まとめ - Qiita <http://qiita.com/hiz_/items/8739c46ddd2563a5603f>
C と lua の連携方法メモ - Qiita <http://qiita.com/miuk/items/82e5566ea01313a8b1af>
全ては時の中に… : 【C++】C++からLuaスクリプトを実行する <http://blog.livedoor.jp/akf0/archives/51600634.html>
【Lua組み込み】Luaスクリプトのファイル分割を独自のファイルロード処理で行う方法 - Flat Leon Works <http://flat-leon.hatenablog.com/entry/lua_script_file_divide>
その１ Luaのインストール <http://marupeke296.com/LUA_No1_Install.html>
その５ LuaからC言語の関数を呼び出す <http://marupeke296.com/LUA_No5_CallFuncOfCFromLua.html>

Lua 5.1とLua 5.2の非互換について - エンジニアのソフトウェア的愛情 <http://d.hatena.ne.jp/E_Mattsan/20120416/1334584047>
c - Embedding Lua 5.2 and defining libraries - Stack Overflow <https://stackoverflow.com/questions/13442907/embedding-lua-5-2-and-defining-libraries>

C_Function · torus/embedding-lua Wiki <https://github.com/torus/embedding-lua/wiki/C_Function>
LuaをC/C++プログラムに組み込む - プログラミングの魔物 <http://p-monster.hatenablog.com/entry/2013/02/26/224807>

設定ファイルとして使うためのLua組み込みあれこれ - 新・日々録 by TRASH BOX@Eel <http://d.hatena.ne.jp/eel3/20150718/1437221172>
*/

#include "StdAfx.h"
#include "CLua.h"
#include "view/CEditView.h"
#include "func/Funccode.h"
#include "CMacro.h"
#include "macro/CSMacroMgr.h"// 2002/2/10 aroka
#include "env/CShareData.h"
#include "env/DLLSHAREDATA.h"
#include <lua/lua.hpp>

static lua_State *_L = nullptr;  // 保持

//! 関数定義マクロ
#define LUA_FUNC(_name_) \
static int l_##_name_(lua_State *L)

//! サクラ用関数定義マクロ
#define LUA_SAKURA_FUNC(_funcid_, _name_)       \
	static int l_##_name_(lua_State *L) {         \
		CLua::DoCommand((EFunctionCode)(_funcid_)); \
		return 0;                                   \
	}

//! 関数登録
#define LUA_ADD_FUNC(_name_) \
	lua_pushcfunction(_L, l_##_name_); \
	lua_setglobal(_L, #_name_);

//! サクラ用ライブラリ関数(コマンド)定義マクロ
#define LUA_SAKURA_FUNC_C(_funcid_, _name_)       \
	{                                               \
		_name_,                                       \
		[](lua_State *L)-> int {                      \
			CLua::DoCommand((EFunctionCode)(_funcid_)); \
			return 0;                                   \
		}                                             \
	},

//! サクラ用ライブラリ関数(関数)定義マクロ
#define LUA_SAKURA_FUNC_F(_funcid_, _name_)              \
	{                                                      \
		_name_,                                              \
		[](lua_State *L)-> int {                             \
			if (CLua::DoFunction((EFunctionCode)(_funcid_))) { \
				return 1;                                        \
			} else {                                           \
				return 0;                                        \
			}                                                  \
		}                                                    \
	},


//! ログ出力
LUA_FUNC(log) {
  const char *s = luaL_checkstring(L, 1);
  mix::log(to_wchar(s));
  return 0;
}
LUA_FUNC(logln) {
  const char *s = luaL_checkstring(L, 1);
  mix::logln(to_wchar(s));
  return 0;
}
//! 文字列をLuaコードとして評価
LUA_FUNC(eval) {
  const char *s = luaL_checkstring(L, 1);
  mix::logln(to_wchar(s));

  // 式にする
  std::string exp = "num = ";
  exp = exp + s;

  double num;

  // 別のlua_Stateで評価する
  {
    lua_State *L2 = luaL_newstate();
    int ret = luaL_dostring(L2, exp.c_str());
    
    lua_getglobal(L2, "num");
    num = lua_tonumber(L2, -1);

    lua_close(L2);
  }

  // 変数の書式化
  char buf[256] = {};
  ::snprintf(buf, sizeof(buf) - 1, "%g", num);

  //mix::logln(to_wchar(buf));

  // 戻り値
  lua_pushstring(L, buf);
  return 1;
}

//------------------------------------------------------------------
//! コマンドの実行
//------------------------------------------------------------------
void CLua::DoCommand(EFunctionCode nFuncID) {
	lua_State *L = _L;
	CEditView *pcEditView = m_CurInstance->m_pcEditView;

	const WCHAR** tmpArguments = new const WCHAR*[16];
	int* tmpArgLengths = new int[16];

	const MacroFuncInfo *mInfo = CSMacroMgr::GetFuncInfoByID(nFuncID);
	int nArgSizeMax = _countof(mInfo->m_varArguments);
	if (mInfo->m_pData) {
		nArgSizeMax = mInfo->m_pData->m_nArgMaxSize;
	}
	int nArgs;
	for (nArgs = 0; ; nArgs++) {
		VARTYPE type = VT_EMPTY;
		if (nArgs < 4) {
			type = mInfo->m_varArguments[nArgs];
		} else {
			if (mInfo->m_pData && nArgs < mInfo->m_pData->m_nArgMinSize) {
				type = mInfo->m_pData->m_pVarArgEx[nArgs - 4];
			}
		}
		if (type == VT_EMPTY) {
			break;
		}
		
		if (type == VT_BSTR) {
			const char *s = lua_tostring(L, nArgs + 1);
			if (s) {
				//mix::logln(to_wchar(s));
				tmpArguments[nArgs] = mbstowcs_new(s);
				tmpArgLengths[nArgs] = wcslen(tmpArguments[nArgs]);
			} else {
				//mix::logln(L"lua_tostring NULL");
				tmpArguments[nArgs] = NULL;
				tmpArgLengths[nArgs] = 0;
			}
		} else if (type == VT_I4) {
			int n = (int)lua_tonumber(L, nArgs + 1);
			//mix::logln(L"lua_tonumber = %d", n);
			tmpArguments[nArgs] = mbstowcs_new(std::to_string(n).c_str());
			tmpArgLengths[nArgs] = wcslen(tmpArguments[nArgs]);
		} else {
			//nop
			//mix::logln(L"nop");
			tmpArguments[nArgs] = NULL;
			tmpArgLengths[nArgs] = 0;
		}
	}

	CMacro::HandleCommand(pcEditView, nFuncID, tmpArguments, tmpArgLengths, nArgs);

	delete[] tmpArguments;
	delete[] tmpArgLengths;
}

//------------------------------------------------------------------
//! ファンクションの実行
//------------------------------------------------------------------
bool CLua::DoFunction(EFunctionCode nFuncID) {
	lua_State *L = _L;
	CEditView *pcEditView = m_CurInstance->m_pcEditView;

	const int maxArgSize = 8;
	VARIANT vtArg[maxArgSize];

	const MacroFuncInfo *mInfo = CSMacroMgr::GetFuncInfoByID(nFuncID);
	for (int i = 0; i < maxArgSize; i++) {
		::VariantInit(&vtArg[i]);
	}
	int nArgs;
	for (nArgs = 0; ; nArgs++) {
		VARTYPE type = VT_EMPTY;
		if (nArgs < 4) {
			type = mInfo->m_varArguments[nArgs];
		} else {
			if (mInfo->m_pData && nArgs < mInfo->m_pData->m_nArgMinSize) {
				type = mInfo->m_pData->m_pVarArgEx[nArgs - 4];
			}
		}
		if (type == VT_EMPTY) {
			break;
		}
		
		if (type == VT_BSTR) {
			const char *s = lua_tostring(L, nArgs + 1);
			SysString S(s, lstrlenA(s));
			Wrap(&vtArg[nArgs])->Receive(S);
			//mix::logln(to_wchar(s));
		} else if (type == VT_I4) {
			int n = (int)lua_tonumber(L, nArgs + 1);
			//mix::logln(L"lua_tonumber = %d", n);
			vtArg[nArgs].vt = VT_I4;
			vtArg[nArgs].lVal = n;
		} else {
			//nop
			//mix::logln(L"nop");
			::VariantClear(&vtArg[nArgs]);
		}
	}

	bool bSuccess = false;
	VARIANT Ret;
	::VariantInit(&Ret);
	if (CMacro::HandleFunction(pcEditView, nFuncID, vtArg, nArgs, Ret)) {
		if (Ret.vt == VT_BSTR) {
			int len;
			char* buf;
			Wrap(&Ret.bstrVal)->Get(&buf,&len);
			m_CurInstance->m_cMemRet.SetString(buf,len); // Mar. 9, 2003 genta
			delete[] buf;
			lua_pushstring(L, m_CurInstance->m_cMemRet.GetStringPtr());
			bSuccess = true;
		} else if (Ret.vt == VT_I4) {
			//lua_pushnumber(L, Ret.lVal);
			lua_pushinteger(L, Ret.intVal);
			bSuccess = true;
		} else if (Ret.vt == VT_INT) {
			lua_pushinteger(L, Ret.intVal);
			bSuccess = true;
		} else if (Ret.vt == VT_UINT) {
			lua_pushinteger(L, Ret.uintVal);
			bSuccess = true;
		}
	}
	::VariantClear(&Ret);

	return bSuccess;
}

//! mixlib関数群
static int luaopen_mixlib(lua_State* L) {
	static const struct luaL_Reg mixlib[] = {
		{"log", [](lua_State *L)->int{mix::log(to_wchar(luaL_checkstring(_L, 1))); return 0;}},
		{"logln", [](lua_State *L)->int{mix::logln(to_wchar(luaL_checkstring(_L, 1))); return 0;}},
		{NULL, NULL}
	};

	luaL_newlib(L, mixlib);
	return 1;
}

//! サクラマクロ関数群
//! @todo あとでちゃんと名前テーブルから関数を登録するようにする
//! 【参照】・CSMacroMgr.cpp(46,27): MacroFuncInfo CSMacroMgr::m_MacroFuncInfoCommandArr[] = 
//!         ・CSMacroMgr.cpp(414,27): MacroFuncInfo CSMacroMgr::m_MacroFuncInfoArr[] = 
static int luaopen_editor(lua_State* L) {
	static const struct luaL_Reg editor[] = {
		/* ファイル操作系 */
		LUA_SAKURA_FUNC_C(F_FILENEW, "FileNew")
		LUA_SAKURA_FUNC_C(F_FILEOPEN2, "FileOpen")
		LUA_SAKURA_FUNC_C(F_FILESAVE, "FileSave")
		LUA_SAKURA_FUNC_C(F_FILESAVEALL, "FileSaveAll")
		LUA_SAKURA_FUNC_C(F_FILESAVEAS_DIALOG, "FileSaveAsDialog")
		LUA_SAKURA_FUNC_C(F_FILESAVEAS, "FileSaveAs")
		LUA_SAKURA_FUNC_C(F_FILECLOSE, "FileClose")
		LUA_SAKURA_FUNC_C(F_FILECLOSE_OPEN, "FileCloseOpen")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN, "FileReopen")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_SJIS, "FileReopenSJIS")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_JIS, "FileReopenJIS")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_EUC, "FileReopenEUC")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_LATIN1, "FileReopenLatin1")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UNICODE, "FileReopenUNICODE")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UNICODEBE, "FileReopenUNICODEBE")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UTF8, "FileReopenUTF8")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_CESU8, "FileReopenCESU8")
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UTF7, "FileReopenUTF7")
		LUA_SAKURA_FUNC_C(F_PRINT, "Print")
		//	{F_PRINT_DIALOG,				LTEXT("PrintDialog"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //印刷ダイアログ
		LUA_SAKURA_FUNC_C(F_PRINT_PREVIEW, "PrintPreview")
		LUA_SAKURA_FUNC_C(F_PRINT_PAGESETUP, "PrintPageSetup")
		LUA_SAKURA_FUNC_C(F_OPEN_HfromtoC, "OpenHfromtoC")
		//	{F_OPEN_HHPP,					LTEXT("OpenHHpp"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //同名のC/C++ヘッダファイルを開く	//Feb. 9, 2001 jepro「.cまたは.cppと同名の.hを開く」から変更		del 2008/6/23 Uchi
		//	{F_OPEN_CCPP,					LTEXT("OpenCCpp"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //同名のC/C++ソースファイルを開く	//Feb. 9, 2001 jepro「.hと同名の.c(なければ.cpp)を開く」から変更	del 2008/6/23 Uchi
		LUA_SAKURA_FUNC_C(F_ACTIVATE_SQLPLUS, "ActivateSQLPLUS")
		LUA_SAKURA_FUNC_C(F_PLSQL_COMPILE_ON_SQLPLUS, "ExecSQLPLUS")
		LUA_SAKURA_FUNC_C(F_BROWSE, "Browse")
		LUA_SAKURA_FUNC_C(F_VIEWMODE, "ViewMode")
		LUA_SAKURA_FUNC_C(F_VIEWMODE, "ReadOnly")
		LUA_SAKURA_FUNC_C(F_PROPERTY_FILE, "PropertyFile")
		LUA_SAKURA_FUNC_C(F_EXITALLEDITORS, "ExitAllEditors")
		LUA_SAKURA_FUNC_C(F_EXITALL, "ExitAll")
		LUA_SAKURA_FUNC_C(F_PUTFILE, "PutFile")
		LUA_SAKURA_FUNC_C(F_INSFILE, "InsFile")

		/* 編集系 */
		LUA_SAKURA_FUNC_C(F_WCHAR, "Char")
		LUA_SAKURA_FUNC_C(F_IME_CHAR, "CharIme")
		LUA_SAKURA_FUNC_C(F_UNDO, "Undo")
		LUA_SAKURA_FUNC_C(F_REDO, "Redo")
		LUA_SAKURA_FUNC_C(F_DELETE, "Delete")
		LUA_SAKURA_FUNC_C(F_DELETE_BACK, "DeleteBack")
		LUA_SAKURA_FUNC_C(F_WordDeleteToStart, "WordDeleteToStart")
		LUA_SAKURA_FUNC_C(F_WordDeleteToEnd, "WordDeleteToEnd")
		LUA_SAKURA_FUNC_C(F_WordCut, "WordCut")
		LUA_SAKURA_FUNC_C(F_WordDelete, "WordDelete")
		LUA_SAKURA_FUNC_C(F_LineCutToStart, "LineCutToStart")
		LUA_SAKURA_FUNC_C(F_LineCutToEnd, "LineCutToEnd")
		LUA_SAKURA_FUNC_C(F_LineDeleteToStart, "LineDeleteToStart")
		LUA_SAKURA_FUNC_C(F_LineDeleteToEnd, "LineDeleteToEnd")
		LUA_SAKURA_FUNC_C(F_CUT_LINE, "CutLine")
		LUA_SAKURA_FUNC_C(F_DELETE_LINE, "DeleteLine")
		LUA_SAKURA_FUNC_C(F_DUPLICATELINE, "DuplicateLine")
		LUA_SAKURA_FUNC_C(F_INDENT_TAB, "IndentTab")
		LUA_SAKURA_FUNC_C(F_UNINDENT_TAB, "UnindentTab")
		LUA_SAKURA_FUNC_C(F_INDENT_SPACE, "IndentSpace")
		LUA_SAKURA_FUNC_C(F_UNINDENT_SPACE, "UnindentSpace")
		//	{F_WORDSREFERENCE,		LTEXT("WordReference"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語リファレンス
		LUA_SAKURA_FUNC_C(F_LTRIM, "LTrim")
		LUA_SAKURA_FUNC_C(F_RTRIM, "RTrim")
		LUA_SAKURA_FUNC_C(F_SORT_ASC, "SortAsc")
		LUA_SAKURA_FUNC_C(F_SORT_DESC, "SortDesc")
		LUA_SAKURA_FUNC_C(F_MERGE, "Merge")

		/* カーソル移動系 */
		LUA_SAKURA_FUNC_C(F_UP, "Up")
		LUA_SAKURA_FUNC_C(F_DOWN, "Down")
		LUA_SAKURA_FUNC_C(F_LEFT, "Left")
		LUA_SAKURA_FUNC_C(F_RIGHT, "Right")
		LUA_SAKURA_FUNC_C(F_UP2, "Up2")
		LUA_SAKURA_FUNC_C(F_DOWN2, "Down2")
		LUA_SAKURA_FUNC_C(F_WORDLEFT, "WordLeft")
		LUA_SAKURA_FUNC_C(F_WORDRIGHT, "WordRight")
		LUA_SAKURA_FUNC_C(F_GOLINETOP, "GoLineTop")
		LUA_SAKURA_FUNC_C(F_GOLINEEND, "GoLineEnd")
		LUA_SAKURA_FUNC_C(F_HalfPageUp, "HalfPageUp")
		LUA_SAKURA_FUNC_C(F_HalfPageDown, "HalfPageDown")
		LUA_SAKURA_FUNC_C(F_1PageUp, "PageUp")
		LUA_SAKURA_FUNC_C(F_1PageUp, "1PageUp")
		LUA_SAKURA_FUNC_C(F_1PageDown, "PageDown")
		LUA_SAKURA_FUNC_C(F_1PageDown, "1PageDown")
		LUA_SAKURA_FUNC_C(F_GOFILETOP, "GoFileTop")
		LUA_SAKURA_FUNC_C(F_GOFILEEND, "GoFileEnd")
		LUA_SAKURA_FUNC_C(F_CURLINECENTER, "CurLineCenter")
		LUA_SAKURA_FUNC_C(F_JUMPHIST_PREV, "MoveHistPrev")
		LUA_SAKURA_FUNC_C(F_JUMPHIST_NEXT, "MoveHistNext")
		LUA_SAKURA_FUNC_C(F_JUMPHIST_SET, "MoveHistSet")
		LUA_SAKURA_FUNC_C(F_WndScrollDown, "F_WndScrollDown")
		LUA_SAKURA_FUNC_C(F_WndScrollUp, "F_WndScrollUp")
		LUA_SAKURA_FUNC_C(F_GONEXTPARAGRAPH, "GoNextParagraph")
		LUA_SAKURA_FUNC_C(F_GOPREVPARAGRAPH, "GoPrevParagraph")
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_NEXT, "GoModifyLineNext")
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_PREV, "GoModifyLinePrev")
		LUA_SAKURA_FUNC_C(F_MOVECURSOR, "MoveCursor")
		LUA_SAKURA_FUNC_C(F_MOVECURSORLAYOUT, "MoveCursorLayout")
		LUA_SAKURA_FUNC_C(F_WHEELUP, "WheelUp")
		LUA_SAKURA_FUNC_C(F_WHEELDOWN, "WheelDown")
		LUA_SAKURA_FUNC_C(F_WHEELLEFT, "WheelLeft")
		LUA_SAKURA_FUNC_C(F_WHEELRIGHT, "WheelRight")
		LUA_SAKURA_FUNC_C(F_WHEELPAGEUP, "WheelPageUp")
		LUA_SAKURA_FUNC_C(F_WHEELPAGEDOWN, "WheelPageDown")
		LUA_SAKURA_FUNC_C(F_WHEELPAGELEFT, "WheelPageLeft")
		LUA_SAKURA_FUNC_C(F_WHEELPAGERIGHT, "WheelPageRight")

		/* 選択系 */
		LUA_SAKURA_FUNC_C(F_SELECTWORD, "SelectWord")
		LUA_SAKURA_FUNC_C(F_SELECTALL, "SelectAll")
		LUA_SAKURA_FUNC_C(F_SELECTLINE, "SelectLine")
		LUA_SAKURA_FUNC_C(F_BEGIN_SEL, "BeginSelect")
		LUA_SAKURA_FUNC_C(F_UP_SEL, "Up_Sel")
		LUA_SAKURA_FUNC_C(F_DOWN_SEL, "Down_Sel")
		LUA_SAKURA_FUNC_C(F_LEFT_SEL, "Left_Sel")
		LUA_SAKURA_FUNC_C(F_RIGHT_SEL, "Right_Sel")
		LUA_SAKURA_FUNC_C(F_UP2_SEL, "Up2_Sel")
		LUA_SAKURA_FUNC_C(F_DOWN2_SEL, "Down2_Sel")
		LUA_SAKURA_FUNC_C(F_WORDLEFT_SEL, "WordLeft_Sel")
		LUA_SAKURA_FUNC_C(F_WORDRIGHT_SEL, "WordRight_Sel")
		LUA_SAKURA_FUNC_C(F_GOLINETOP_SEL, "GoLineTop_Sel")
		LUA_SAKURA_FUNC_C(F_GOLINEEND_SEL, "GoLineEnd_Sel")
		LUA_SAKURA_FUNC_C(F_HalfPageUp_Sel, "HalfPageUp_Sel")
		LUA_SAKURA_FUNC_C(F_HalfPageDown_Sel, "HalfPageDown_Sel")
		LUA_SAKURA_FUNC_C(F_1PageUp_Sel, "PageUp_Sel")
		LUA_SAKURA_FUNC_C(F_1PageUp_Sel, "1PageUp_Sel")
		LUA_SAKURA_FUNC_C(F_1PageDown_Sel, "PageDown_Sel")
		LUA_SAKURA_FUNC_C(F_1PageDown_Sel, "1PageDown_Sel")
		LUA_SAKURA_FUNC_C(F_GOFILETOP_SEL, "GoFileTop_Sel")
		LUA_SAKURA_FUNC_C(F_GOFILEEND_SEL, "GoFileEnd_Sel")
		LUA_SAKURA_FUNC_C(F_GONEXTPARAGRAPH_SEL, "GoNextParagraph_Sel")
		LUA_SAKURA_FUNC_C(F_GOPREVPARAGRAPH_SEL, "GoPrevParagraph_Sel")
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_NEXT_SEL, "GoModifyLineNext_Sel")
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_PREV_SEL, "GoModifyLinePrev_Sel")

		/* 矩形選択系 */
		LUA_SAKURA_FUNC_C(F_BEGIN_BOX, "BeginBoxSelect")
		LUA_SAKURA_FUNC_C(F_UP_BOX, "Up_BoxSel")
		LUA_SAKURA_FUNC_C(F_DOWN_BOX, "Down_BoxSel")
		LUA_SAKURA_FUNC_C(F_LEFT_BOX, "Left_BoxSel")
		LUA_SAKURA_FUNC_C(F_RIGHT_BOX, "Right_BoxSel")
		LUA_SAKURA_FUNC_C(F_UP2_BOX, "Up2_BoxSel")
		LUA_SAKURA_FUNC_C(F_DOWN2_BOX, "Down2_BoxSel")
		LUA_SAKURA_FUNC_C(F_WORDLEFT_BOX, "WordLeft_BoxSel")
		LUA_SAKURA_FUNC_C(F_WORDRIGHT_BOX, "WordRight_BoxSel")
		LUA_SAKURA_FUNC_C(F_GOLOGICALLINETOP_BOX, "GoLogicalLineTop_BoxSel")
		LUA_SAKURA_FUNC_C(F_GOLINETOP_BOX, "GoLineTop_BoxSel")
		LUA_SAKURA_FUNC_C(F_GOLINEEND_BOX, "GoLineEnd_BoxSel")
		LUA_SAKURA_FUNC_C(F_HalfPageUp_BOX, "HalfPageUp_BoxSel")
		LUA_SAKURA_FUNC_C(F_HalfPageDown_BOX, "HalfPageDown_BoxSel")
		LUA_SAKURA_FUNC_C(F_1PageUp_BOX, "PageUp_BoxSel")
		LUA_SAKURA_FUNC_C(F_1PageUp_BOX, "1PageUp_BoxSel")
		LUA_SAKURA_FUNC_C(F_1PageDown_BOX, "PageDown_BoxSel")
		LUA_SAKURA_FUNC_C(F_1PageDown_BOX, "1PageDown_BoxSel")
		LUA_SAKURA_FUNC_C(F_GOFILETOP_BOX, "GoFileTop_BoxSel")
		LUA_SAKURA_FUNC_C(F_GOFILEEND_BOX, "GoFileEnd_BoxSel")

		/* クリップボード系 */
		LUA_SAKURA_FUNC_C(F_CUT, "Cut")
		LUA_SAKURA_FUNC_C(F_COPY, "Copy")
		LUA_SAKURA_FUNC_C(F_PASTE, "Paste")
		LUA_SAKURA_FUNC_C(F_COPY_ADDCRLF, "CopyAddCRLF")
		LUA_SAKURA_FUNC_C(F_COPY_CRLF, "CopyCRLF")
		LUA_SAKURA_FUNC_C(F_PASTEBOX, "PasteBox")
		LUA_SAKURA_FUNC_C(F_INSBOXTEXT, "InsBoxText")
		LUA_SAKURA_FUNC_C(F_INSTEXT_W, "InsText")
		LUA_SAKURA_FUNC_C(F_ADDTAIL_W, "AddTail")
		LUA_SAKURA_FUNC_C(F_COPYLINES, "CopyLines")
		LUA_SAKURA_FUNC_C(F_COPYLINESASPASSAGE, "CopyLinesAsPassage")
		LUA_SAKURA_FUNC_C(F_COPYLINESWITHLINENUMBER, "CopyLinesWithLineNumber")
		LUA_SAKURA_FUNC_C(F_COPY_COLOR_HTML, "CopyColorHtml")
		LUA_SAKURA_FUNC_C(F_COPY_COLOR_HTML_LINENUMBER, "CopyColorHtmlWithLineNumber")
		LUA_SAKURA_FUNC_C(F_COPYPATH, "CopyPath")
		LUA_SAKURA_FUNC_C(F_COPYFNAME, "CopyFilename")
		LUA_SAKURA_FUNC_C(F_COPYTAG, "CopyTag")
		LUA_SAKURA_FUNC_C(F_CREATEKEYBINDLIST, "CopyKeyBindList")

		/* 挿入系 */
		LUA_SAKURA_FUNC_C(F_INS_DATE, "InsertDate")
		LUA_SAKURA_FUNC_C(F_INS_TIME, "InsertTime")
		LUA_SAKURA_FUNC_C(F_CTRL_CODE_DIALOG, "CtrlCodeDialog")
		LUA_SAKURA_FUNC_C(F_CTRL_CODE, "CtrlCode")

		/* 変換系 */
		LUA_SAKURA_FUNC_C(F_TOLOWER, "ToLower")
		LUA_SAKURA_FUNC_C(F_TOUPPER, "ToUpper")
		LUA_SAKURA_FUNC_C(F_TOHANKAKU, "ToHankaku")
		LUA_SAKURA_FUNC_C(F_TOHANKATA, "ToHankata")
		LUA_SAKURA_FUNC_C(F_TOZENEI, "ToZenEi")
		LUA_SAKURA_FUNC_C(F_TOHANEI, "ToHanEi")
		LUA_SAKURA_FUNC_C(F_TOZENKAKUKATA, "ToZenKata")
		LUA_SAKURA_FUNC_C(F_TOZENKAKUHIRA, "ToZenHira")
		LUA_SAKURA_FUNC_C(F_HANKATATOZENKATA, "HanKataToZenKata")
		LUA_SAKURA_FUNC_C(F_HANKATATOZENHIRA, "HanKataToZenHira")
		LUA_SAKURA_FUNC_C(F_TABTOSPACE, "TABToSPACE")
		LUA_SAKURA_FUNC_C(F_SPACETOTAB, "SPACEToTAB")
		LUA_SAKURA_FUNC_C(F_CODECNV_AUTO2SJIS, "AutoToSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_EMAIL, "JIStoSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_EUC2SJIS, "EUCtoSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_UNICODE2SJIS, "CodeCnvUNICODEtoSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_UNICODEBE2SJIS, "CodeCnvUNICODEBEtoSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_UTF82SJIS, "UTF8toSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_UTF72SJIS, "UTF7toSJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2JIS, "SJIStoJIS")
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2EUC, "SJIStoEUC")
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2UTF8, "SJIStoUTF8")
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2UTF7, "SJIStoUTF7")
		LUA_SAKURA_FUNC_C(F_BASE64DECODE, "Base64Decode")
		LUA_SAKURA_FUNC_C(F_UUDECODE, "Uudecode")

		/* 検索系 */
		LUA_SAKURA_FUNC_C(F_SEARCH_DIALOG, "SearchDialog")
		LUA_SAKURA_FUNC_C(F_SEARCH_NEXT, "SearchNext")
		LUA_SAKURA_FUNC_C(F_SEARCH_PREV, "SearchPrev")
		LUA_SAKURA_FUNC_C(F_REPLACE_DIALOG, "ReplaceDialog")
		LUA_SAKURA_FUNC_C(F_REPLACE, "Replace")
		LUA_SAKURA_FUNC_C(F_REPLACE_ALL, "ReplaceAll")
		LUA_SAKURA_FUNC_C(F_SEARCH_CLEARMARK, "SearchClearMark")
		LUA_SAKURA_FUNC_C(F_JUMP_SRCHSTARTPOS, "SearchStartPos")
		LUA_SAKURA_FUNC_C(F_GREP, "Grep")
		LUA_SAKURA_FUNC_C(F_GREP_REPLACE, "GrepReplace")
		LUA_SAKURA_FUNC_C(F_JUMP, "Jump")
		LUA_SAKURA_FUNC_C(F_OUTLINE, "Outline")
		LUA_SAKURA_FUNC_C(F_TAGJUMP, "TagJump")
		LUA_SAKURA_FUNC_C(F_TAGJUMPBACK, "TagJumpBack")
		LUA_SAKURA_FUNC_C(F_TAGS_MAKE, "TagMake")
		LUA_SAKURA_FUNC_C(F_DIRECT_TAGJUMP, "DirectTagJump")
		LUA_SAKURA_FUNC_C(F_TAGJUMP_KEYWORD, "KeywordTagJump")
		LUA_SAKURA_FUNC_C(F_COMPARE, "Compare")
		LUA_SAKURA_FUNC_C(F_DIFF_DIALOG, "DiffDialog")
		LUA_SAKURA_FUNC_C(F_DIFF, "Diff")
		LUA_SAKURA_FUNC_C(F_DIFF_NEXT, "DiffNext")
		LUA_SAKURA_FUNC_C(F_DIFF_PREV, "DiffPrev")
		LUA_SAKURA_FUNC_C(F_DIFF_RESET, "DiffReset")
		LUA_SAKURA_FUNC_C(F_BRACKETPAIR, "BracketPair")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_SET, "BookmarkSet")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_NEXT, "BookmarkNext")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_PREV, "BookmarkPrev")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_RESET, "BookmarkReset")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_VIEW, "BookmarkView")
		LUA_SAKURA_FUNC_C(F_BOOKMARK_PATTERN, "BookmarkPattern")
		LUA_SAKURA_FUNC_C(F_FUNCLIST_NEXT, "FuncListNext")
		LUA_SAKURA_FUNC_C(F_FUNCLIST_PREV, "FuncListPrev")

		/* モード切り替え系 */
		LUA_SAKURA_FUNC_C(F_CHGMOD_INS, "ChgmodINS")
		LUA_SAKURA_FUNC_C(F_CHG_CHARSET, "ChgCharSet")
		LUA_SAKURA_FUNC_C(F_CHGMOD_EOL, "ChgmodEOL")
		LUA_SAKURA_FUNC_C(F_CANCEL_MODE, "CancelMode")

		/* マクロ系 */
		LUA_SAKURA_FUNC_C(F_EXECEXTMACRO, "ExecExternalMacro")

		/* 設定系 */
		LUA_SAKURA_FUNC_C(F_SHOWTOOLBAR, "ShowToolbar")
		LUA_SAKURA_FUNC_C(F_SHOWFUNCKEY, "ShowFunckey")
		LUA_SAKURA_FUNC_C(F_SHOWTAB, "ShowTab")
		LUA_SAKURA_FUNC_C(F_SHOWSTATUSBAR, "ShowStatusbar")
		LUA_SAKURA_FUNC_C(F_SHOWMINIMAP, "ShowMiniMap")
		LUA_SAKURA_FUNC_C(F_TYPE_LIST, "TypeList")
		LUA_SAKURA_FUNC_C(F_CHANGETYPE, "ChangeType")
		LUA_SAKURA_FUNC_C(F_OPTION_TYPE, "OptionType")
		LUA_SAKURA_FUNC_C(F_OPTION, "OptionCommon")
		LUA_SAKURA_FUNC_C(F_FONT, "SelectFont")
		LUA_SAKURA_FUNC_C(F_SETFONTSIZE, "SetFontSize")
		LUA_SAKURA_FUNC_C(F_WRAPWINDOWWIDTH, "WrapWindowWidth")
		LUA_SAKURA_FUNC_C(F_FAVORITE, "OptionFavorite")
		LUA_SAKURA_FUNC_C(F_SET_QUOTESTRING, "SetMsgQuoteStr")
		LUA_SAKURA_FUNC_C(F_TEXTWRAPMETHOD, "TextWrapMethod")
		LUA_SAKURA_FUNC_C(F_SELECT_COUNT_MODE, "SelectCountMode")
		LUA_SAKURA_FUNC_C(F_EXECMD, "ExecCommand")
		LUA_SAKURA_FUNC_C(F_EXECMD_DIALOG, "ExecCommandDialog")

		/* カスタムメニュー */
		LUA_SAKURA_FUNC_C(F_MENU_RBUTTON, "RMenu")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_1, "CustMenu1")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_2, "CustMenu2")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_3, "CustMenu3")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_4, "CustMenu4")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_5, "CustMenu5")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_6, "CustMenu6")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_7, "CustMenu7")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_8, "CustMenu8")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_9, "CustMenu9")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_10, "CustMenu10")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_11, "CustMenu11")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_12, "CustMenu12")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_13, "CustMenu13")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_14, "CustMenu14")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_15, "CustMenu15")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_16, "CustMenu16")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_17, "CustMenu17")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_18, "CustMenu18")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_19, "CustMenu19")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_20, "CustMenu20")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_21, "CustMenu21")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_22, "CustMenu22")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_23, "CustMenu23")
		LUA_SAKURA_FUNC_C(F_CUSTMENU_24, "CustMenu24")

		/* ウィンドウ系 */
		LUA_SAKURA_FUNC_C(F_SPLIT_V, "SplitWinV")
		LUA_SAKURA_FUNC_C(F_SPLIT_H, "SplitWinH")
		LUA_SAKURA_FUNC_C(F_SPLIT_VH, "SplitWinVH")
		LUA_SAKURA_FUNC_C(F_WINCLOSE, "WinClose")
		LUA_SAKURA_FUNC_C(F_WIN_CLOSEALL, "WinCloseAll")
		LUA_SAKURA_FUNC_C(F_CASCADE, "CascadeWin")
		LUA_SAKURA_FUNC_C(F_TILE_V, "TileWinV")
		LUA_SAKURA_FUNC_C(F_TILE_H, "TileWinH")
		LUA_SAKURA_FUNC_C(F_NEXTWINDOW, "NextWindow")
		LUA_SAKURA_FUNC_C(F_PREVWINDOW, "PrevWindow")
		LUA_SAKURA_FUNC_C(F_WINLIST, "WindowList")
		LUA_SAKURA_FUNC_C(F_MAXIMIZE_V, "MaximizeV")
		LUA_SAKURA_FUNC_C(F_MAXIMIZE_H, "MaximizeH")
		LUA_SAKURA_FUNC_C(F_MINIMIZE_ALL, "MinimizeAll")
		LUA_SAKURA_FUNC_C(F_REDRAW, "ReDraw")
		LUA_SAKURA_FUNC_C(F_WIN_OUTPUT, "ActivateWinOutput")
		LUA_SAKURA_FUNC_C(F_TRACEOUT, "TraceOut")
		LUA_SAKURA_FUNC_C(F_TOPMOST, "WindowTopMost")
		LUA_SAKURA_FUNC_C(F_GROUPCLOSE, "GroupClose")
		LUA_SAKURA_FUNC_C(F_NEXTGROUP, "NextGroup")
		LUA_SAKURA_FUNC_C(F_PREVGROUP, "PrevGroup")
		LUA_SAKURA_FUNC_C(F_TAB_MOVERIGHT, "TabMoveRight")
		LUA_SAKURA_FUNC_C(F_TAB_MOVELEFT, "TabMoveLeft")
		LUA_SAKURA_FUNC_C(F_TAB_SEPARATE, "TabSeparate")
		LUA_SAKURA_FUNC_C(F_TAB_JOINTNEXT, "TabJointNext")
		LUA_SAKURA_FUNC_C(F_TAB_JOINTPREV, "TabJointPrev")
		LUA_SAKURA_FUNC_C(F_TAB_CLOSEOTHER, "TabCloseOther")
		LUA_SAKURA_FUNC_C(F_TAB_CLOSELEFT, "TabCloseLeft")
		LUA_SAKURA_FUNC_C(F_TAB_CLOSERIGHT, "TabCloseRight")

		/* 支援 */
		LUA_SAKURA_FUNC_C(F_HOKAN, "Complete")
		LUA_SAKURA_FUNC_C(F_TOGGLE_KEY_SEARCH, "ToggleKeyHelpSearch")
		LUA_SAKURA_FUNC_C(F_HELP_CONTENTS, "HelpContents")
		LUA_SAKURA_FUNC_C(F_HELP_SEARCH, "HelpSearch")
		LUA_SAKURA_FUNC_C(F_MENU_ALLFUNC, "CommandList")
		LUA_SAKURA_FUNC_C(F_EXTHELP1, "ExtHelp1")
		LUA_SAKURA_FUNC_C(F_EXTHTMLHELP, "ExtHtmlHelp")
		LUA_SAKURA_FUNC_C(F_ABOUT, "About")

		/*マクロ用*/
		LUA_SAKURA_FUNC_C(F_STATUSMSG, "StatusMsg")
		LUA_SAKURA_FUNC_C(F_MSGBEEP, "MsgBeep")
		LUA_SAKURA_FUNC_C(F_COMMITUNDOBUFFER, "CommitUndoBuffer")
		LUA_SAKURA_FUNC_C(F_ADDREFUNDOBUFFER, "AddRefUndoBuffer")
		LUA_SAKURA_FUNC_C(F_SETUNDOBUFFER, "SetUndoBuffer")
		LUA_SAKURA_FUNC_C(F_APPENDUNDOBUFFERCURSOR, "AppendUndoBufferCursor")
		LUA_SAKURA_FUNC_C(F_CLIPBOARDEMPTY, "ClipboardEmpty")
		LUA_SAKURA_FUNC_C(F_SETVIEWTOP, "SetViewTop")
		LUA_SAKURA_FUNC_C(F_SETVIEWLEFT, "SetViewLeft")
		
		/*非コマンド関数*/
		LUA_SAKURA_FUNC_F(F_GETFILENAME, "GetFilename")
		LUA_SAKURA_FUNC_F(F_GETSAVEFILENAME, "GetSaveFilename")
		LUA_SAKURA_FUNC_F(F_GETSELECTED, "GetSelectedString")
		LUA_SAKURA_FUNC_F(F_EXPANDPARAMETER, "ExpandParameter")
		LUA_SAKURA_FUNC_F(F_GETLINESTR, "GetLineStr")
		LUA_SAKURA_FUNC_F(F_GETLINECOUNT, "GetLineCount")
		LUA_SAKURA_FUNC_F(F_CHGTABWIDTH, "ChangeTabWidth")
		LUA_SAKURA_FUNC_F(F_ISTEXTSELECTED, "IsTextSelected")
		LUA_SAKURA_FUNC_F(F_GETSELLINEFROM, "GetSelectLineFrom")
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNFROM, "GetSelectColmFrom")
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNFROM, "GetSelectColumnFrom")
		LUA_SAKURA_FUNC_F(F_GETSELLINETO, "GetSelectLineTo")
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNTO, "GetSelectColmTo")
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNTO, "GetSelectColumnTo")
		LUA_SAKURA_FUNC_F(F_ISINSMODE, "IsInsMode")
		LUA_SAKURA_FUNC_F(F_GETCHARCODE, "GetCharCode")
		LUA_SAKURA_FUNC_F(F_GETLINECODE, "GetLineCode")
		LUA_SAKURA_FUNC_F(F_ISPOSSIBLEUNDO, "IsPossibleUndo")
		LUA_SAKURA_FUNC_F(F_ISPOSSIBLEREDO, "IsPossibleRedo")
		LUA_SAKURA_FUNC_F(F_CHGWRAPCOLUMN, "ChangeWrapColm")
		LUA_SAKURA_FUNC_F(F_CHGWRAPCOLUMN, "ChangeWrapColumn")
		LUA_SAKURA_FUNC_F(F_ISCURTYPEEXT, "IsCurTypeExt")
		LUA_SAKURA_FUNC_F(F_ISSAMETYPEEXT, "IsSameTypeExt")
		LUA_SAKURA_FUNC_F(F_INPUTBOX, "InputBox")
		LUA_SAKURA_FUNC_F(F_MESSAGEBOX, "MessageBox")
		LUA_SAKURA_FUNC_F(F_ERRORMSG, "ErrorMsg")
		LUA_SAKURA_FUNC_F(F_WARNMSG, "WarnMsg")
		LUA_SAKURA_FUNC_F(F_INFOMSG, "InfoMsg")
		LUA_SAKURA_FUNC_F(F_OKCANCELBOX, "OkCancelBox")
		LUA_SAKURA_FUNC_F(F_YESNOBOX, "YesNoBox")
		LUA_SAKURA_FUNC_F(F_COMPAREVERSION, "CompareVersion")
		LUA_SAKURA_FUNC_F(F_MACROSLEEP, "Sleep")
		LUA_SAKURA_FUNC_F(F_FILEOPENDIALOG, "FileOpenDialog")
		LUA_SAKURA_FUNC_F(F_FILESAVEDIALOG, "FileSaveDialog")
		LUA_SAKURA_FUNC_F(F_FOLDERDIALOG, "FolderDialog")
		LUA_SAKURA_FUNC_F(F_GETCLIPBOARD, "GetClipboard")
		LUA_SAKURA_FUNC_F(F_SETCLIPBOARD, "SetClipboard")
		LUA_SAKURA_FUNC_F(F_LAYOUTTOLOGICLINENUM, "LayoutToLogicLineNum")
		LUA_SAKURA_FUNC_F(F_LOGICTOLAYOUTLINENUM, "LogicToLayoutLineNum")
		LUA_SAKURA_FUNC_F(F_LINECOLUMNTOINDEX, "LineColumnToIndex")
		LUA_SAKURA_FUNC_F(F_LINEINDEXTOCOLUMN, "LineIndexToColumn")
		LUA_SAKURA_FUNC_F(F_GETCOOKIE, "GetCookie")
		LUA_SAKURA_FUNC_F(F_GETCOOKIEDEFAULT, "GetCookieDefault")
		LUA_SAKURA_FUNC_F(F_SETCOOKIE, "SetCookie")
		LUA_SAKURA_FUNC_F(F_DELETECOOKIE, "DeleteCookie")
		LUA_SAKURA_FUNC_F(F_GETCOOKIENAMES, "GetCookieNames")
		LUA_SAKURA_FUNC_F(F_SETDRAWSWITCH, "SetDrawSwitch")
		LUA_SAKURA_FUNC_F(F_GETDRAWSWITCH, "GetDrawSwitch")
		LUA_SAKURA_FUNC_F(F_ISSHOWNSTATUS, "IsShownStatus")
		LUA_SAKURA_FUNC_F(F_GETSTRWIDTH, "GetStrWidth")
		LUA_SAKURA_FUNC_F(F_GETSTRLAYOUTLENGTH, "GetStrLayoutLength")
		LUA_SAKURA_FUNC_F(F_GETDEFAULTCHARLENGTH, "GetDefaultCharLength")
		LUA_SAKURA_FUNC_F(F_ISINCLUDECLIPBOARDFORMAT, "IsIncludeClipboardFormat")
		LUA_SAKURA_FUNC_F(F_GETCLIPBOARDBYFORMAT, "GetClipboardByFormat")
		LUA_SAKURA_FUNC_F(F_SETCLIPBOARDBYFORMAT, "SetClipboardByFormat")
		LUA_SAKURA_FUNC_F(F_GETLINEATTRIBUTE, "GetLineAttribute")
		LUA_SAKURA_FUNC_F(F_ISTEXTSELECTINGLOCK, "IsTextSelectingLock")
		LUA_SAKURA_FUNC_F(F_GETVIEWLINES, "GetViewLines")
		LUA_SAKURA_FUNC_F(F_GETVIEWCOLUMNS, "GetViewColumns")
		LUA_SAKURA_FUNC_F(F_CREATEMENU, "CreateMenu")
		
		{NULL, NULL}
	};

	luaL_newlib(L, editor);
	return 1;
}

CLua::LuaExecInfo *CLua::m_CurInstance = NULL;
bool CLua::m_bIsRunning = false;

CLua::CLua() {
	lua_State *L;
	int ret;
	
	L = luaL_newstate();
	
	luaL_openlibs(L);
	
	_L = L;

	// test
	//luaL_register(L, "mix", mixlib);
	luaL_requiref(L, "mix", luaopen_mixlib, 1);
	luaL_requiref(L, "Editor", luaopen_editor, 1);
	//LUA_ADD_FUNC(log);
	//LUA_ADD_FUNC(logln);
	LUA_ADD_FUNC(eval);

	//LUA_ADD_FUNC(FileNew);
	//LUA_ADD_FUNC(FileOpen);
	//LUA_ADD_FUNC(FileSave)
	//LUA_ADD_FUNC(FileSaveAll)
	//LUA_ADD_FUNC(FileSaveAsDialog)
	//LUA_ADD_FUNC(FileSaveAs)
}

CLua::~CLua() {
	lua_State *L = _L;
	lua_close(L);
}

bool CLua::Execute(CEditView* pcEditView, int flags, const CNativeW &buffer)
{
	if (CLua::m_bIsRunning) {
//		MYMESSAGEBOX( pcEditView->GetHwnd(), MB_OK, LS(STR_ERR_DLGPPA7), LS(STR_ERR_DLGPPA1) );
		CLua::m_bIsRunning = false;
		return false;
	}
	CLua::m_bIsRunning = true;

	LuaExecInfo info;
	info.m_pcEditView = pcEditView;
	info.m_pShareData = &GetDllShareData();
	info.m_bError = false;
	info.m_cMemDebug.SetString("");
	info.m_commandflags = flags | FA_FROMMACRO;
	
	//	実行前にインスタンスを待避する
	LuaExecInfo *old_instance = m_CurInstance;
	m_CurInstance = &info;
	
	{
		lua_State *L = _L;
		int ret;
		
		//mix::logln(buffer.GetStringPtr());
		ret = luaL_dostring(L, to_achar(buffer.GetStringPtr()));
		
		if (ret != 0) {
			info.m_bError = true;
			
			// エラー時のメッセージ表示
			std::string error_message = lua_tostring(L, -1);
			mix::logln(to_wchar(error_message.c_str()));
			::MessageBoxW(NULL, to_wchar(error_message.c_str()), L"Lua Script failed!!", MB_OK);
		}
	}
	
	//	マクロ実行完了後はここに戻ってくる
	m_CurInstance = old_instance;

	CLua::m_bIsRunning = false;
	return !info.m_bError;
}

