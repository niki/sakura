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


LUA_FUNC(log) {
  const char *s = luaL_checkstring(_L, 1);
  mix::log(to_wchar(s));
  return 0;
}
LUA_FUNC(logln) {
  const char *s = luaL_checkstring(_L, 1);
  mix::logln(to_wchar(s));
  return 0;
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
static int luaopen_editor(lua_State* L) {
	static const struct luaL_Reg editor[] = {
		/* ファイル操作系 */
		LUA_SAKURA_FUNC_C(F_FILENEW, "FileNew") //	{F_FILENEW,						LTEXT("FileNew"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //新規作成
		LUA_SAKURA_FUNC_C(F_FILEOPEN2, "FileOpen") //	{F_FILEOPEN2,					LTEXT("FileOpen"),				{VT_BSTR,  VT_I4,    VT_I4,    VT_BSTR},	VT_EMPTY,	NULL}, //開く2
		LUA_SAKURA_FUNC_C(F_FILESAVE, "FileSave") //	{F_FILESAVE,					LTEXT("FileSave"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //上書き保存
		LUA_SAKURA_FUNC_C(F_FILESAVEALL, "FileSaveAll") //	{F_FILESAVEALL,					LTEXT("FileSaveAll"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //上書き保存
		LUA_SAKURA_FUNC_C(F_FILESAVEAS_DIALOG, "FileSaveAsDialog") //	{F_FILESAVEAS_DIALOG,			LTEXT("FileSaveAsDialog"),		{VT_BSTR,  VT_I4,    VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //名前を付けて保存(ダイアログ) 2013.05.02
		LUA_SAKURA_FUNC_C(F_FILESAVEAS, "FileSaveAs") //	{F_FILESAVEAS,					LTEXT("FileSaveAs"),			{VT_BSTR,  VT_I4,    VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //名前を付けて保存
		LUA_SAKURA_FUNC_C(F_FILECLOSE, "FileClose") //	{F_FILECLOSE,					LTEXT("FileClose"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //閉じて(無題)	//Oct. 17, 2000 jepro 「ファイルを閉じる」というキャプションを変更
		LUA_SAKURA_FUNC_C(F_FILECLOSE_OPEN, "FileCloseOpen") //	{F_FILECLOSE_OPEN,				LTEXT("FileCloseOpen"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //閉じて開く
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN, "FileReopen") //	{F_FILE_REOPEN,					LTEXT("FileReopen"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //開き直す	//Dec. 4, 2002 genta
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_SJIS, "FileReopenSJIS") //	{F_FILE_REOPEN_SJIS,			LTEXT("FileReopenSJIS"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //SJISで開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_JIS, "FileReopenJIS") //	{F_FILE_REOPEN_JIS,				LTEXT("FileReopenJIS"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //JISで開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_EUC, "FileReopenEUC") //	{F_FILE_REOPEN_EUC,				LTEXT("FileReopenEUC"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //EUCで開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_LATIN1, "FileReopenLatin1") //	{F_FILE_REOPEN_LATIN1,			LTEXT("FileReopenLatin1"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //Latin1で開き直す	// 2010/3/20 Uchi
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UNICODE, "FileReopenUNICODE")  //	{F_FILE_REOPEN_UNICODE,			LTEXT("FileReopenUNICODE"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //Unicodeで開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UNICODEBE, "FileReopenUNICODEBE") //	{F_FILE_REOPEN_UNICODEBE,		LTEXT("FileReopenUNICODEBE"),	{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //UnicodeBEで開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UTF8, "FileReopenUTF8") //	{F_FILE_REOPEN_UTF8,			LTEXT("FileReopenUTF8"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //UTF-8で開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_CESU8, "FileReopenCESU8") //	{F_FILE_REOPEN_CESU8,			LTEXT("FileReopenCESU8"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //CESU-8で開き直す
		LUA_SAKURA_FUNC_C(F_FILE_REOPEN_UTF7, "FileReopenUTF7") //	{F_FILE_REOPEN_UTF7,			LTEXT("FileReopenUTF7"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //UTF-7で開き直す
		LUA_SAKURA_FUNC_C(F_PRINT, "Print") //	{F_PRINT,						LTEXT("Print"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //印刷
		//	{F_PRINT_DIALOG,				LTEXT("PrintDialog"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //印刷ダイアログ
		LUA_SAKURA_FUNC_C(F_PRINT_PREVIEW, "PrintPreview") //	{F_PRINT_PREVIEW,				LTEXT("PrintPreview"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //印刷プレビュー
		LUA_SAKURA_FUNC_C(F_PRINT_PAGESETUP, "PrintPageSetup") //	{F_PRINT_PAGESETUP,				LTEXT("PrintPageSetup"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //印刷ページ設定	//Sept. 14, 2000 jepro 「印刷のページレイアウトの設定」から変更
		LUA_SAKURA_FUNC_C(F_OPEN_HfromtoC, "OpenHfromtoC") //	{F_OPEN_HfromtoC,				LTEXT("OpenHfromtoC"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //同名のC/C++ヘッダ(ソース)を開く	//Feb. 7, 2001 JEPRO 追加
		//	{F_OPEN_HHPP,					LTEXT("OpenHHpp"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //同名のC/C++ヘッダファイルを開く	//Feb. 9, 2001 jepro「.cまたは.cppと同名の.hを開く」から変更		del 2008/6/23 Uchi
		//	{F_OPEN_CCPP,					LTEXT("OpenCCpp"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //同名のC/C++ソースファイルを開く	//Feb. 9, 2001 jepro「.hと同名の.c(なければ.cpp)を開く」から変更	del 2008/6/23 Uchi
		LUA_SAKURA_FUNC_C(F_ACTIVATE_SQLPLUS, "ActivateSQLPLUS") //	{F_ACTIVATE_SQLPLUS,			LTEXT("ActivateSQLPLUS"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* Oracle SQL*Plusをアクティブ表示 */
		LUA_SAKURA_FUNC_C(F_PLSQL_COMPILE_ON_SQLPLUS, "ExecSQLPLUS") //	{F_PLSQL_COMPILE_ON_SQLPLUS,	LTEXT("ExecSQLPLUS"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* Oracle SQL*Plusで実行 */
		LUA_SAKURA_FUNC_C(F_BROWSE, "Browse") //	{F_BROWSE,						LTEXT("Browse"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ブラウズ
		LUA_SAKURA_FUNC_C(F_VIEWMODE, "ViewMode") //	{F_VIEWMODE,					LTEXT("ViewMode"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ビューモード
		LUA_SAKURA_FUNC_C(F_VIEWMODE, "ReadOnly") //	{F_VIEWMODE,					LTEXT("ReadOnly"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ビューモード(旧)
		LUA_SAKURA_FUNC_C(F_PROPERTY_FILE, "PropertyFile") //	{F_PROPERTY_FILE,				LTEXT("PropertyFile"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ファイルのプロパティ
		LUA_SAKURA_FUNC_C(F_EXITALLEDITORS, "ExitAllEditors") //	{F_EXITALLEDITORS,				LTEXT("ExitAllEditors"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //編集の全終了	// 2007.02.13 ryoji 追加
		LUA_SAKURA_FUNC_C(F_EXITALL, "ExitAll") //	{F_EXITALL,						LTEXT("ExitAll"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //サクラエディタの全終了	//Dec. 27, 2000 JEPRO 追加
		LUA_SAKURA_FUNC_C(F_PUTFILE, "PutFile") //	{F_PUTFILE,						LTEXT("PutFile"),				{VT_BSTR,  VT_I4,    VT_I4,    VT_EMPTY},   VT_EMPTY,	NULL}, // 作業中ファイルの一時出力 2006.12.10 maru
		LUA_SAKURA_FUNC_C(F_INSFILE, "InsFile") //	{F_INSFILE,						LTEXT("InsFile"),				{VT_BSTR,  VT_I4,    VT_I4,    VT_EMPTY},   VT_EMPTY,	NULL}, // キャレット位置にファイル挿入 2006.12.10 maru

		/* 編集系 */
		LUA_SAKURA_FUNC_C(F_WCHAR, "Char") //	{F_WCHAR,				LTEXT("Char"),					{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //文字入力
		LUA_SAKURA_FUNC_C(F_IME_CHAR, "CharIme") //	{F_IME_CHAR,			LTEXT("CharIme"),				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //全角文字入力
		LUA_SAKURA_FUNC_C(F_UNDO, "Undo") //	{F_UNDO,				LTEXT("Undo"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //元に戻す(Undo)
		LUA_SAKURA_FUNC_C(F_REDO, "Redo") //	{F_REDO,				LTEXT("Redo"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //やり直し(Redo)
		LUA_SAKURA_FUNC_C(F_DELETE, "Delete") //	{F_DELETE,				LTEXT("Delete"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //削除
		LUA_SAKURA_FUNC_C(F_DELETE_BACK, "DeleteBack") //	{F_DELETE_BACK,			LTEXT("DeleteBack"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル前を削除
		LUA_SAKURA_FUNC_C(F_WordDeleteToStart, "WordDeleteToStart") //	{F_WordDeleteToStart,	LTEXT("WordDeleteToStart"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語の左端まで削除
		LUA_SAKURA_FUNC_C(F_WordDeleteToEnd, "WordDeleteToEnd") //	{F_WordDeleteToEnd,		LTEXT("WordDeleteToEnd"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語の右端まで削除
		LUA_SAKURA_FUNC_C(F_WordCut, "WordCut") //	{F_WordCut,				LTEXT("WordCut"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語切り取り
		LUA_SAKURA_FUNC_C(F_WordDelete, "WordDelete") //	{F_WordDelete,			LTEXT("WordDelete"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語削除
		LUA_SAKURA_FUNC_C(F_LineCutToStart, "LineCutToStart") //	{F_LineCutToStart,		LTEXT("LineCutToStart"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行頭まで切り取り(改行単位)
		LUA_SAKURA_FUNC_C(F_LineCutToEnd, "LineCutToEnd") //	{F_LineCutToEnd,		LTEXT("LineCutToEnd"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行末まで切り取り(改行単位)
		LUA_SAKURA_FUNC_C(F_LineDeleteToStart, "LineDeleteToStart") //	{F_LineDeleteToStart,	LTEXT("LineDeleteToStart"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行頭まで削除(改行単位)
		LUA_SAKURA_FUNC_C(F_LineDeleteToEnd, "LineDeleteToEnd") //	{F_LineDeleteToEnd,		LTEXT("LineDeleteToEnd"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行末まで削除(改行単位)
		LUA_SAKURA_FUNC_C(F_CUT_LINE, "CutLine") //	{F_CUT_LINE,			LTEXT("CutLine"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行切り取り(折り返し単位)
		LUA_SAKURA_FUNC_C(F_DELETE_LINE, "DeleteLine") //	{F_DELETE_LINE,			LTEXT("DeleteLine"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行削除(折り返し単位)
		LUA_SAKURA_FUNC_C(F_DUPLICATELINE, "DuplicateLine") //	{F_DUPLICATELINE,		LTEXT("DuplicateLine"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行の二重化(折り返し単位)
		LUA_SAKURA_FUNC_C(F_INDENT_TAB, "IndentTab") //	{F_INDENT_TAB,			LTEXT("IndentTab"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //TABインデント
		LUA_SAKURA_FUNC_C(F_UNINDENT_TAB, "UnindentTab") //	{F_UNINDENT_TAB,		LTEXT("UnindentTab"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //逆TABインデント
		LUA_SAKURA_FUNC_C(F_INDENT_SPACE, "IndentSpace") //	{F_INDENT_SPACE,		LTEXT("IndentSpace"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //SPACEインデント
		LUA_SAKURA_FUNC_C(F_UNINDENT_SPACE, "UnindentSpace") //	{F_UNINDENT_SPACE,		LTEXT("UnindentSpace"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //逆SPACEインデント
		//	{F_WORDSREFERENCE,		LTEXT("WordReference"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語リファレンス
		LUA_SAKURA_FUNC_C(F_LTRIM, "LTrim") //	{F_LTRIM,				LTEXT("LTrim"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //左(先頭)の空白を削除 2001.12.03 hor
		LUA_SAKURA_FUNC_C(F_RTRIM, "RTrim") //	{F_RTRIM,				LTEXT("RTrim"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //右(末尾)の空白を削除 2001.12.03 hor
		LUA_SAKURA_FUNC_C(F_SORT_ASC, "SortAsc") //	{F_SORT_ASC,			LTEXT("SortAsc"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択行の昇順ソート 2001.12.06 hor
		LUA_SAKURA_FUNC_C(F_SORT_DESC, "SortDesc") //	{F_SORT_DESC,			LTEXT("SortDesc"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択行の降順ソート 2001.12.06 hor
		LUA_SAKURA_FUNC_C(F_MERGE, "Merge") //	{F_MERGE,				LTEXT("Merge"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択行のマージ 2001.12.06 hor

		/* カーソル移動系 */
		LUA_SAKURA_FUNC_C(F_UP, "Up") //	{F_UP,					LTEXT("Up"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル上移動
		LUA_SAKURA_FUNC_C(F_DOWN, "Down") //	{F_DOWN,				LTEXT("Down"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル下移動
		LUA_SAKURA_FUNC_C(F_LEFT, "Left") //	{F_LEFT,				LTEXT("Left"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル左移動
		LUA_SAKURA_FUNC_C(F_RIGHT, "Right") //	{F_RIGHT,				LTEXT("Right"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル右移動
		LUA_SAKURA_FUNC_C(F_UP2, "Up2") //	{F_UP2,					LTEXT("Up2"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル上移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_DOWN2, "Down2") //	{F_DOWN2,				LTEXT("Down2"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル下移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_WORDLEFT, "WordLeft") //	{F_WORDLEFT,			LTEXT("WordLeft"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語の左端に移動
		LUA_SAKURA_FUNC_C(F_WORDRIGHT, "WordRight") //	{F_WORDRIGHT,			LTEXT("WordRight"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //単語の右端に移動
		LUA_SAKURA_FUNC_C(F_GOLINETOP, "GoLineTop") //	{F_GOLINETOP,			LTEXT("GoLineTop"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行頭に移動(折り返し単位/改行単位)
		LUA_SAKURA_FUNC_C(F_GOLINEEND, "GoLineEnd") //	{F_GOLINEEND,			LTEXT("GoLineEnd"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //行末に移動(折り返し単位)
		LUA_SAKURA_FUNC_C(F_HalfPageUp, "HalfPageUp") //	{F_HalfPageUp,			LTEXT("HalfPageUp"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //半ページアップ	//Oct. 6, 2000 JEPRO 名称をPC-AT互換機系に変更(ROLL→PAGE) //Oct. 10, 2000 JEPRO 名称変更
		LUA_SAKURA_FUNC_C(F_HalfPageDown, "HalfPageDown") //	{F_HalfPageDown,		LTEXT("HalfPageDown"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //半ページダウン	//Oct. 6, 2000 JEPRO 名称をPC-AT互換機系に変更(ROLL→PAGE) //Oct. 10, 2000 JEPRO 名称変更
		LUA_SAKURA_FUNC_C(F_1PageUp, "PageUp") //	{F_1PageUp,				LTEXT("PageUp"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //１ページアップ	//Oct. 10, 2000 JEPRO 従来のページアップを半ページアップと名称変更し１ページアップを追加
		LUA_SAKURA_FUNC_C(F_1PageUp, "1PageUp") //	{F_1PageUp,				LTEXT("1PageUp"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //１ページアップ	//Oct. 10, 2000 JEPRO 従来のページアップを半ページアップと名称変更し１ページアップを追加
		LUA_SAKURA_FUNC_C(F_1PageDown, "PageDown") //	{F_1PageDown,			LTEXT("PageDown"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //１ページダウン	//Oct. 10, 2000 JEPRO 従来のページダウンを半ページダウンと名称変更し１ページダウンを追加
		LUA_SAKURA_FUNC_C(F_1PageDown, "1PageDown") //	{F_1PageDown,			LTEXT("1PageDown"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //１ページダウン	//Oct. 10, 2000 JEPRO 従来のページダウンを半ページダウンと名称変更し１ページダウンを追加
		LUA_SAKURA_FUNC_C(F_GOFILETOP, "GoFileTop") //	{F_GOFILETOP,			LTEXT("GoFileTop"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ファイルの先頭に移動
		LUA_SAKURA_FUNC_C(F_GOFILEEND, "GoFileEnd") //	{F_GOFILEEND,			LTEXT("GoFileEnd"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ファイルの最後に移動
		LUA_SAKURA_FUNC_C(F_CURLINECENTER, "CurLineCenter") //	{F_CURLINECENTER,		LTEXT("CurLineCenter"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル行をウィンドウ中央へ
		LUA_SAKURA_FUNC_C(F_JUMPHIST_PREV, "MoveHistPrev") //	{F_JUMPHIST_PREV,		LTEXT("MoveHistPrev"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //移動履歴: 前へ
		LUA_SAKURA_FUNC_C(F_JUMPHIST_NEXT, "MoveHistNext") //	{F_JUMPHIST_NEXT,		LTEXT("MoveHistNext"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //移動履歴: 次へ
		LUA_SAKURA_FUNC_C(F_JUMPHIST_SET, "MoveHistSet") //	{F_JUMPHIST_SET,		LTEXT("MoveHistSet"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //現在位置を移動履歴に登録
		LUA_SAKURA_FUNC_C(F_WndScrollDown, "F_WndScrollDown") //	{F_WndScrollDown,		LTEXT("F_WndScrollDown"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //テキストを１行下へスクロール	// 2001/06/20 asa-o
		LUA_SAKURA_FUNC_C(F_WndScrollUp, "F_WndScrollUp") //	{F_WndScrollUp,			LTEXT("F_WndScrollUp"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //テキストを１行上へスクロール	// 2001/06/20 asa-o
		LUA_SAKURA_FUNC_C(F_GONEXTPARAGRAPH, "GoNextParagraph") //	{F_GONEXTPARAGRAPH,		LTEXT("GoNextParagraph"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次の段落へ移動
		LUA_SAKURA_FUNC_C(F_GOPREVPARAGRAPH, "GoPrevParagraph") //	{F_GOPREVPARAGRAPH,		LTEXT("GoPrevParagraph"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前の段落へ移動
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_NEXT, "GoModifyLineNext") //	{F_MODIFYLINE_NEXT,		LTEXT("GoModifyLineNext"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次の変更行へ移動
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_PREV, "GoModifyLinePrev") //	{F_MODIFYLINE_PREV,		LTEXT("GoModifyLinePrev"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前の変更行へ移動
		LUA_SAKURA_FUNC_C(F_MOVECURSOR, "MoveCursor") //	{F_MOVECURSOR,			LTEXT("MoveCursor"),		{VT_I4,    VT_I4,    VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル移動
		LUA_SAKURA_FUNC_C(F_MOVECURSORLAYOUT, "MoveCursorLayout") //	{F_MOVECURSORLAYOUT,	LTEXT("MoveCursorLayout"),	{VT_I4,    VT_I4,    VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //カーソル移動(レイアウト単位)
		LUA_SAKURA_FUNC_C(F_WHEELUP, "WheelUp") //	{F_WHEELUP,				LTEXT("WheelUp"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールアップ
		LUA_SAKURA_FUNC_C(F_WHEELDOWN, "WheelDown") //	{F_WHEELDOWN,			LTEXT("WheelDown"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールダウン
		LUA_SAKURA_FUNC_C(F_WHEELLEFT, "WheelLeft") //	{F_WHEELLEFT,			LTEXT("WheelLeft"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイール左
		LUA_SAKURA_FUNC_C(F_WHEELRIGHT, "WheelRight") //	{F_WHEELRIGHT,			LTEXT("WheelRight"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイール右
		LUA_SAKURA_FUNC_C(F_WHEELPAGEUP, "WheelPageUp") //	{F_WHEELPAGEUP,			LTEXT("WheelPageUp"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールページアップ
		LUA_SAKURA_FUNC_C(F_WHEELPAGEDOWN, "WheelPageDown") //	{F_WHEELPAGEDOWN,		LTEXT("WheelPageDown"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールページダウン
		LUA_SAKURA_FUNC_C(F_WHEELPAGELEFT, "WheelPageLeft") //	{F_WHEELPAGELEFT,		LTEXT("WheelPageLeft"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールページ左
		LUA_SAKURA_FUNC_C(F_WHEELPAGERIGHT, "WheelPageRight") //	{F_WHEELPAGERIGHT,		LTEXT("WheelPageRight"),	{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ホイールページ右

		/* 選択系 */	//Oct. 15, 2000 JEPRO 「カーソル移動系」が多くなったので「選択系」として独立化(サブメニュー化は構造上できないので)
		LUA_SAKURA_FUNC_C(F_SELECTWORD, "SelectWord") //	{F_SELECTWORD,			LTEXT("SelectWord"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //現在位置の単語選択
		LUA_SAKURA_FUNC_C(F_SELECTALL, "SelectAll") //	{F_SELECTALL,			LTEXT("SelectAll"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //すべて選択
		LUA_SAKURA_FUNC_C(F_SELECTLINE, "SelectLine") //	{F_SELECTLINE,			LTEXT("SelectLine"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //1行選択	// 2007.10.13 nasukoji
		LUA_SAKURA_FUNC_C(F_BEGIN_SEL, "BeginSelect") //	{F_BEGIN_SEL,			LTEXT("BeginSelect"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //範囲選択開始 Mar. 5, 2001 genta 名称修正
		LUA_SAKURA_FUNC_C(F_UP_SEL, "Up_Sel") //	{F_UP_SEL,				LTEXT("Up_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル上移動
		LUA_SAKURA_FUNC_C(F_DOWN_SEL, "Down_Sel") //	{F_DOWN_SEL,			LTEXT("Down_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル下移動
		LUA_SAKURA_FUNC_C(F_LEFT_SEL, "Left_Sel") //	{F_LEFT_SEL,			LTEXT("Left_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル左移動
		LUA_SAKURA_FUNC_C(F_RIGHT_SEL, "Right_Sel") //	{F_RIGHT_SEL,			LTEXT("Right_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル右移動
		LUA_SAKURA_FUNC_C(F_UP2_SEL, "Up2_Sel") //	{F_UP2_SEL,				LTEXT("Up2_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル上移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_DOWN2_SEL, "Down2_Sel") //	{F_DOWN2_SEL,			LTEXT("Down2_Sel"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)カーソル下移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_WORDLEFT_SEL, "WordLeft_Sel") //	{F_WORDLEFT_SEL,		LTEXT("WordLeft_Sel"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)単語の左端に移動
		LUA_SAKURA_FUNC_C(F_WORDRIGHT_SEL, "WordRight_Sel") //	{F_WORDRIGHT_SEL,		LTEXT("WordRight_Sel"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)単語の右端に移動
		LUA_SAKURA_FUNC_C(F_GOLINETOP_SEL, "GoLineTop_Sel") //	{F_GOLINETOP_SEL,		LTEXT("GoLineTop_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)行頭に移動(折り返し単位/改行単位)
		LUA_SAKURA_FUNC_C(F_GOLINEEND_SEL, "GoLineEnd_Sel") //	{F_GOLINEEND_SEL,		LTEXT("GoLineEnd_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)行末に移動(折り返し単位)
		LUA_SAKURA_FUNC_C(F_HalfPageUp_Sel, "HalfPageUp_Sel") //	{F_HalfPageUp_Sel,		LTEXT("HalfPageUp_Sel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)半ページアップ	//Oct. 6, 2000 JEPRO 名称をPC-AT互換機系に変更(ROLL→PAGE) //Oct. 10, 2000 JEPRO 名称変更
		LUA_SAKURA_FUNC_C(F_HalfPageDown_Sel, "HalfPageDown_Sel") //	{F_HalfPageDown_Sel,	LTEXT("HalfPageDown_Sel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)半ページダウン	//Oct. 6, 2000 JEPRO 名称をPC-AT互換機系に変更(ROLL→PAGE) //Oct. 10, 2000 JEPRO 名称変更
		LUA_SAKURA_FUNC_C(F_1PageUp_Sel, "PageUp_Sel") //	{F_1PageUp_Sel,			LTEXT("PageUp_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)１ページアップ	//Oct. 10, 2000 JEPRO 従来のページアップを半ページアップと名称変更し１ページアップを追加
		LUA_SAKURA_FUNC_C(F_1PageUp_Sel, "1PageUp_Sel") //	{F_1PageUp_Sel,			LTEXT("1PageUp_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)１ページアップ	//Oct. 10, 2000 JEPRO 従来のページアップを半ページアップと名称変更し１ページアップを追加
		LUA_SAKURA_FUNC_C(F_1PageDown_Sel, "PageDown_Sel") //	{F_1PageDown_Sel,		LTEXT("PageDown_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)１ページダウン	//Oct. 10, 2000 JEPRO 従来のページダウンを半ページダウンと名称変更し１ページダウンを追加
		LUA_SAKURA_FUNC_C(F_1PageDown_Sel, "1PageDown_Sel") //	{F_1PageDown_Sel,		LTEXT("1PageDown_Sel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)１ページダウン	//Oct. 10, 2000 JEPRO 従来のページダウンを半ページダウンと名称変更し１ページダウンを追加
		LUA_SAKURA_FUNC_C(F_GOFILETOP_SEL, "GoFileTop_Sel") //	{F_GOFILETOP_SEL,		LTEXT("GoFileTop_Sel"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)ファイルの先頭に移動
		LUA_SAKURA_FUNC_C(F_GOFILEEND_SEL, "GoFileEnd_Sel") //	{F_GOFILEEND_SEL,		LTEXT("GoFileEnd_Sel"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)ファイルの最後に移動
		LUA_SAKURA_FUNC_C(F_GONEXTPARAGRAPH_SEL, "GoNextParagraph_Sel") //	{F_GONEXTPARAGRAPH_SEL,	LTEXT("GoNextParagraph_Sel"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)次の段落へ移動
		LUA_SAKURA_FUNC_C(F_GOPREVPARAGRAPH_SEL, "GoPrevParagraph_Sel") //	{F_GOPREVPARAGRAPH_SEL,	LTEXT("GoPrevParagraph_Sel"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)前の段落へ移動
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_NEXT_SEL, "GoModifyLineNext_Sel") //	{F_MODIFYLINE_NEXT_SEL,	LTEXT("GoModifyLineNext_Sel"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)次の変更行へ移動
		LUA_SAKURA_FUNC_C(F_MODIFYLINE_PREV_SEL, "GoModifyLinePrev_Sel") //	{F_MODIFYLINE_PREV_SEL,	LTEXT("GoModifyLinePrev_Sel"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(範囲選択)前の変更行へ移動

		/* 矩形選択系 */	//Oct. 17, 2000 JEPRO (矩形選択)が新設され次第ここにおく
		LUA_SAKURA_FUNC_C(F_BEGIN_BOX, "BeginBoxSelect") //	{F_BEGIN_BOX,			LTEXT("BeginBoxSelect"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //矩形範囲選択開始
		LUA_SAKURA_FUNC_C(F_UP_BOX, "Up_BoxSel") //	{F_UP_BOX,				LTEXT("Up_BoxSel"),				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル上移動
		LUA_SAKURA_FUNC_C(F_DOWN_BOX, "Down_BoxSel") //	{F_DOWN_BOX,			LTEXT("Down_BoxSel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル下移動
		LUA_SAKURA_FUNC_C(F_LEFT_BOX, "Left_BoxSel") //	{F_LEFT_BOX,			LTEXT("Left_BoxSel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル左移動
		LUA_SAKURA_FUNC_C(F_RIGHT_BOX, "Right_BoxSel") //	{F_RIGHT_BOX,			LTEXT("Right_BoxSel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル右移動
		LUA_SAKURA_FUNC_C(F_UP2_BOX, "Up2_BoxSel") //	{F_UP2_BOX,				LTEXT("Up2_BoxSel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル上移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_DOWN2_BOX, "Down2_BoxSel") //	{F_DOWN2_BOX,			LTEXT("Down2_BoxSel"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)カーソル下移動(２行ごと)
		LUA_SAKURA_FUNC_C(F_WORDLEFT_BOX, "WordLeft_BoxSel") //	{F_WORDLEFT_BOX,		LTEXT("WordLeft_BoxSel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)単語の左端に移動
		LUA_SAKURA_FUNC_C(F_WORDRIGHT_BOX, "WordRight_BoxSel") //	{F_WORDRIGHT_BOX,		LTEXT("WordRight_BoxSel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)単語の右端に移動
		LUA_SAKURA_FUNC_C(F_GOLOGICALLINETOP_BOX, "GoLogicalLineTop_BoxSel") //	{F_GOLOGICALLINETOP_BOX,LTEXT("GoLogicalLineTop_BoxSel"),{VT_I4,   VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)行頭に移動(改行単位)
		LUA_SAKURA_FUNC_C(F_GOLINETOP_BOX, "GoLineTop_BoxSel") //	{F_GOLINETOP_BOX,		LTEXT("GoLineTop_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)行頭に移動(折り返し単位/改行単位)
		LUA_SAKURA_FUNC_C(F_GOLINEEND_BOX, "GoLineEnd_BoxSel") //	{F_GOLINEEND_BOX,		LTEXT("GoLineEnd_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)行末に移動(折り返し単位)
		LUA_SAKURA_FUNC_C(F_HalfPageUp_BOX, "HalfPageUp_BoxSel") //	{F_HalfPageUp_BOX,		LTEXT("HalfPageUp_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)半ページアップ
		LUA_SAKURA_FUNC_C(F_HalfPageDown_BOX, "HalfPageDown_BoxSel") //	{F_HalfPageDown_BOX,	LTEXT("HalfPageDown_BoxSel"),	{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)半ページダウン
		LUA_SAKURA_FUNC_C(F_1PageUp_BOX, "PageUp_BoxSel") //	{F_1PageUp_BOX,			LTEXT("PageUp_BoxSel"),			{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)１ページアップ
		LUA_SAKURA_FUNC_C(F_1PageUp_BOX, "1PageUp_BoxSel") //	{F_1PageUp_BOX,			LTEXT("1PageUp_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)１ページアップ
		LUA_SAKURA_FUNC_C(F_1PageDown_BOX, "PageDown_BoxSel") //	{F_1PageDown_BOX,		LTEXT("PageDown_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)１ページダウン
		LUA_SAKURA_FUNC_C(F_1PageDown_BOX, "1PageDown_BoxSel") //	{F_1PageDown_BOX,		LTEXT("1PageDown_BoxSel"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)１ページダウン
		LUA_SAKURA_FUNC_C(F_GOFILETOP_BOX, "GoFileTop_BoxSel") //	{F_GOFILETOP_BOX,		LTEXT("GoFileTop_BoxSel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)ファイルの先頭に移動
		LUA_SAKURA_FUNC_C(F_GOFILEEND_BOX, "GoFileEnd_BoxSel") //	{F_GOFILEEND_BOX,		LTEXT("GoFileEnd_BoxSel"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //(矩形選択)ファイルの最後に移動

		/* クリップボード系 */
		LUA_SAKURA_FUNC_C(F_CUT, "Cut") //	{F_CUT,						LTEXT("Cut"),						{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //切り取り(選択範囲をクリップボードにコピーして削除)
		LUA_SAKURA_FUNC_C(F_COPY, "Copy") //	{F_COPY,					LTEXT("Copy"),						{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //コピー(選択範囲をクリップボードにコピー)
		LUA_SAKURA_FUNC_C(F_PASTE, "Paste") //	{F_PASTE,					LTEXT("Paste"),						{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //貼り付け(クリップボードから貼り付け)
		LUA_SAKURA_FUNC_C(F_COPY_ADDCRLF, "CopyAddCRLF") //	{F_COPY_ADDCRLF,			LTEXT("CopyAddCRLF"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //折り返し位置に改行をつけてコピー
		LUA_SAKURA_FUNC_C(F_COPY_CRLF, "CopyCRLF") //	{F_COPY_CRLF,				LTEXT("CopyCRLF"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //CRLF改行でコピー(選択範囲を改行コード=CRLFでコピー)
		LUA_SAKURA_FUNC_C(F_PASTEBOX, "PasteBox") //	{F_PASTEBOX,				LTEXT("PasteBox"),					{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //矩形貼り付け(クリップボードから矩形貼り付け)
		LUA_SAKURA_FUNC_C(F_INSBOXTEXT, "InsBoxText") //	{F_INSBOXTEXT,				LTEXT("InsBoxText"),				{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // 矩形テキスト挿入
		LUA_SAKURA_FUNC_C(F_INSTEXT_W, "InsText") //	{F_INSTEXT_W,				LTEXT("InsText"),					{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // テキストを貼り付け
		LUA_SAKURA_FUNC_C(F_ADDTAIL_W, "AddTail") //	{F_ADDTAIL_W,				LTEXT("AddTail"),					{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // 最後にテキストを追加
		LUA_SAKURA_FUNC_C(F_COPYLINES, "CopyLines") //	{F_COPYLINES,				LTEXT("CopyLines"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択範囲内全行コピー
		LUA_SAKURA_FUNC_C(F_COPYLINESASPASSAGE, "CopyLinesAsPassage") //	{F_COPYLINESASPASSAGE,		LTEXT("CopyLinesAsPassage"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択範囲内全行引用符付きコピー
		LUA_SAKURA_FUNC_C(F_COPYLINESWITHLINENUMBER, "CopyLinesWithLineNumber") //	{F_COPYLINESWITHLINENUMBER,	LTEXT("CopyLinesWithLineNumber"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択範囲内全行行番号付きコピー
		LUA_SAKURA_FUNC_C(F_COPY_COLOR_HTML, "CopyColorHtml") //	{F_COPY_COLOR_HTML,			LTEXT("CopyColorHtml"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択範囲内色付きHTMLコピー
		LUA_SAKURA_FUNC_C(F_COPY_COLOR_HTML_LINENUMBER, "CopyColorHtmlWithLineNumber") //	{F_COPY_COLOR_HTML_LINENUMBER,	LTEXT("CopyColorHtmlWithLineNumber"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //選択範囲内行番号色付きHTMLコピー
		LUA_SAKURA_FUNC_C(F_COPYPATH, "CopyPath") //	{F_COPYPATH,				LTEXT("CopyPath"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //このファイルのパス名をクリップボードにコピー
		LUA_SAKURA_FUNC_C(F_COPYFNAME, "CopyFilename") //	{F_COPYFNAME,				LTEXT("CopyFilename"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //このファイル名をクリップボードにコピー // 2002/2/3 aroka
		LUA_SAKURA_FUNC_C(F_COPYTAG, "CopyTag") //	{F_COPYTAG,					LTEXT("CopyTag"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //このファイルのパス名とカーソル位置をコピー	//Sept. 15, 2000 jepro 上と同じ説明になっていたのを修正
		LUA_SAKURA_FUNC_C(F_CREATEKEYBINDLIST, "CopyKeyBindList") //	{F_CREATEKEYBINDLIST,		LTEXT("CopyKeyBindList"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //キー割り当て一覧をコピー	//Sept. 15, 2000 JEPRO 追加 //Dec. 25, 2000 復活

		/* 挿入系 */
		LUA_SAKURA_FUNC_C(F_INS_DATE, "InsertDate") //	{F_INS_DATE,				LTEXT("InsertDate"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // 日付挿入
		LUA_SAKURA_FUNC_C(F_INS_TIME, "InsertTime") //	{F_INS_TIME,				LTEXT("InsertTime"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // 時刻挿入
		LUA_SAKURA_FUNC_C(F_CTRL_CODE_DIALOG, "CtrlCodeDialog") //	{F_CTRL_CODE_DIALOG,		LTEXT("CtrlCodeDialog"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //コントロールコードの入力(ダイアログ)	//@@@ 2002.06.02 MIK
		LUA_SAKURA_FUNC_C(F_CTRL_CODE, "CtrlCode") //	{F_CTRL_CODE,				LTEXT("CtrlCode"),				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //コントロールコードの入力 2013.12.12

		/* 変換系 */
		LUA_SAKURA_FUNC_C(F_TOLOWER, "ToLower") //	{F_TOLOWER,		 			LTEXT("ToLower"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //小文字
		LUA_SAKURA_FUNC_C(F_TOUPPER, "ToUpper") //	{F_TOUPPER,		 			LTEXT("ToUpper"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //大文字
		LUA_SAKURA_FUNC_C(F_TOHANKAKU, "ToHankaku") //	{F_TOHANKAKU,		 		LTEXT("ToHankaku"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 全角→半角 */
		LUA_SAKURA_FUNC_C(F_TOHANKATA, "ToHankata") //	{F_TOHANKATA,		 		LTEXT("ToHankata"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 全角カタカナ→半角カタカナ */	//Aug. 29, 2002 ai
		LUA_SAKURA_FUNC_C(F_TOZENEI, "ToZenEi") //	{F_TOZENEI,		 			LTEXT("ToZenEi"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 半角英数→全角英数 */			//July. 30, 2001 Misaka
		LUA_SAKURA_FUNC_C(F_TOHANEI, "ToHanEi") //	{F_TOHANEI,		 			LTEXT("ToHanEi"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 全角英数→半角英数 */
		LUA_SAKURA_FUNC_C(F_TOZENKAKUKATA, "ToZenKata") //	{F_TOZENKAKUKATA,	 		LTEXT("ToZenKata"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 半角＋全ひら→全角・カタカナ */	//Sept. 17, 2000 jepro 説明を「半角→全角カタカナ」から変更
		LUA_SAKURA_FUNC_C(F_TOZENKAKUHIRA, "ToZenHira") //	{F_TOZENKAKUHIRA,	 		LTEXT("ToZenHira"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 半角＋全カタ→全角・ひらがな */	//Sept. 17, 2000 jepro 説明を「半角→全角ひらがな」から変更
		LUA_SAKURA_FUNC_C(F_HANKATATOZENKATA, "HanKataToZenKata") //	{F_HANKATATOZENKATA,	LTEXT("HanKataToZenKata"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 半角カタカナ→全角カタカナ */
		LUA_SAKURA_FUNC_C(F_HANKATATOZENHIRA, "HanKataToZenHira") //	{F_HANKATATOZENHIRA,	LTEXT("HanKataToZenHira"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 半角カタカナ→全角ひらがな */
		LUA_SAKURA_FUNC_C(F_TABTOSPACE, "TABToSPACE") //	{F_TABTOSPACE,				LTEXT("TABToSPACE"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* TAB→空白 */
		LUA_SAKURA_FUNC_C(F_SPACETOTAB, "SPACEToTAB") //	{F_SPACETOTAB,				LTEXT("SPACEToTAB"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 空白→TAB */ //---- Stonee, 2001/05/27
		LUA_SAKURA_FUNC_C(F_CODECNV_AUTO2SJIS, "AutoToSJIS") //	{F_CODECNV_AUTO2SJIS,		LTEXT("AutoToSJIS"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 自動判別→SJISコード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_EMAIL, "JIStoSJIS") //	{F_CODECNV_EMAIL,			LTEXT("JIStoSJIS"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //E-Mail(JIS→SJIS)コード変換
		LUA_SAKURA_FUNC_C(F_CODECNV_EUC2SJIS, "EUCtoSJIS") //	{F_CODECNV_EUC2SJIS,		LTEXT("EUCtoSJIS"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //EUC→SJISコード変換
		LUA_SAKURA_FUNC_C(F_CODECNV_UNICODE2SJIS, "CodeCnvUNICODEtoSJIS") //	{F_CODECNV_UNICODE2SJIS,	LTEXT("CodeCnvUNICODEtoSJIS"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //Unicode→SJISコード変換
		LUA_SAKURA_FUNC_C(F_CODECNV_UNICODEBE2SJIS, "CodeCnvUNICODEBEtoSJIS") //	{F_CODECNV_UNICODEBE2SJIS,	LTEXT("CodeCnvUNICODEBEtoSJIS"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // UnicodeBE→SJISコード変換
		LUA_SAKURA_FUNC_C(F_CODECNV_UTF82SJIS, "UTF8toSJIS") //	{F_CODECNV_UTF82SJIS,		LTEXT("UTF8toSJIS"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* UTF-8→SJISコード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_UTF72SJIS, "UTF7toSJIS") //	{F_CODECNV_UTF72SJIS,		LTEXT("UTF7toSJIS"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* UTF-7→SJISコード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2JIS, "SJIStoJIS") //	{F_CODECNV_SJIS2JIS,		LTEXT("SJIStoJIS"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* SJIS→JISコード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2EUC, "SJIStoEUC") //	{F_CODECNV_SJIS2EUC,		LTEXT("SJIStoEUC"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* SJIS→EUCコード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2UTF8, "SJIStoUTF8") //	{F_CODECNV_SJIS2UTF8,		LTEXT("SJIStoUTF8"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* SJIS→UTF-8コード変換 */
		LUA_SAKURA_FUNC_C(F_CODECNV_SJIS2UTF7, "SJIStoUTF7") //	{F_CODECNV_SJIS2UTF7,		LTEXT("SJIStoUTF7"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* SJIS→UTF-7コード変換 */
		LUA_SAKURA_FUNC_C(F_BASE64DECODE, "Base64Decode") //	{F_BASE64DECODE,	 		LTEXT("Base64Decode"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //Base64デコードして保存
		LUA_SAKURA_FUNC_C(F_UUDECODE, "Uudecode") //	{F_UUDECODE,		 		LTEXT("Uudecode"),					{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //uudecodeして保存	//Oct. 17, 2000 jepro 説明を「選択部分をUUENCODEデコード」から変更

		/* 検索系 */
		LUA_SAKURA_FUNC_C(F_SEARCH_DIALOG, "SearchDialog") //	{F_SEARCH_DIALOG,			LTEXT("SearchDialog"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //検索(単語検索ダイアログ)
		LUA_SAKURA_FUNC_C(F_SEARCH_NEXT, "SearchNext") //	{F_SEARCH_NEXT,				LTEXT("SearchNext"),		{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次を検索
		LUA_SAKURA_FUNC_C(F_SEARCH_PREV, "SearchPrev") //	{F_SEARCH_PREV,				LTEXT("SearchPrev"),		{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前を検索
		LUA_SAKURA_FUNC_C(F_REPLACE_DIALOG, "ReplaceDialog") //	{F_REPLACE_DIALOG,			LTEXT("ReplaceDialog"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //置換(置換ダイアログ)
		LUA_SAKURA_FUNC_C(F_REPLACE, "Replace") //	{F_REPLACE,					LTEXT("Replace"),			{VT_BSTR,  VT_BSTR,  VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //置換(実行)
		LUA_SAKURA_FUNC_C(F_REPLACE_ALL, "ReplaceAll") //	{F_REPLACE_ALL,				LTEXT("ReplaceAll"),		{VT_BSTR,  VT_BSTR,  VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, //すべて置換(実行)
		LUA_SAKURA_FUNC_C(F_SEARCH_CLEARMARK, "SearchClearMark") //	{F_SEARCH_CLEARMARK,		LTEXT("SearchClearMark"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //検索マークのクリア
		LUA_SAKURA_FUNC_C(F_JUMP_SRCHSTARTPOS, "SearchStartPos") //	{F_JUMP_SRCHSTARTPOS,		LTEXT("SearchStartPos"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //検索開始位置へ戻る			// 02/06/26 ai
		LUA_SAKURA_FUNC_C(F_GREP, "Grep") //	{F_GREP,					LTEXT("Grep"),				{VT_BSTR,  VT_BSTR,  VT_BSTR,  VT_I4   },	VT_EMPTY,	&s_MacroInfoEx_i}, //Grep
		LUA_SAKURA_FUNC_C(F_GREP_REPLACE, "GrepReplace") //	{F_GREP_REPLACE,			LTEXT("GrepReplace"),		{VT_BSTR,  VT_BSTR,  VT_BSTR,  VT_BSTR },	VT_EMPTY,	&s_MacroInfoEx_ii}, //Grep置換
		LUA_SAKURA_FUNC_C(F_JUMP, "Jump") //	{F_JUMP,					LTEXT("Jump"),				{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //指定行ヘジャンプ
		LUA_SAKURA_FUNC_C(F_OUTLINE, "Outline") //	{F_OUTLINE,					LTEXT("Outline"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //アウトライン解析
		LUA_SAKURA_FUNC_C(F_TAGJUMP, "TagJump") //	{F_TAGJUMP,					LTEXT("TagJump"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タグジャンプ機能
		LUA_SAKURA_FUNC_C(F_TAGJUMPBACK, "TagJumpBack") //	{F_TAGJUMPBACK,				LTEXT("TagJumpBack"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タグジャンプバック機能
		LUA_SAKURA_FUNC_C(F_TAGS_MAKE, "TagMake") //	{F_TAGS_MAKE,				LTEXT("TagMake"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タグファイルの作成	//@@@ 2003.04.13 MIK
		LUA_SAKURA_FUNC_C(F_DIRECT_TAGJUMP, "DirectTagJump") //	{F_DIRECT_TAGJUMP,			LTEXT("DirectTagJump"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ダイレクトタグジャンプ機能	//@@@ 2003.04.15 MIK
		LUA_SAKURA_FUNC_C(F_TAGJUMP_KEYWORD, "KeywordTagJump") //	{F_TAGJUMP_KEYWORD,			LTEXT("KeywordTagJump"),	{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //キーワードを指定してダイレクトタグジャンプ機能 //@@@ 2005.03.31 MIK
		LUA_SAKURA_FUNC_C(F_COMPARE, "Compare") //	{F_COMPARE,					LTEXT("Compare"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ファイル内容比較
		LUA_SAKURA_FUNC_C(F_DIFF_DIALOG, "DiffDialog") //	{F_DIFF_DIALOG,				LTEXT("DiffDialog"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //DIFF差分表示(ダイアログ)	//@@@ 2002.05.25 MIK
		LUA_SAKURA_FUNC_C(F_DIFF, "Diff") //	{F_DIFF,					LTEXT("Diff"),				{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //DIFF差分表示				//@@@ 2002.05.25 MIK	// 2005.10.03 maru
		LUA_SAKURA_FUNC_C(F_DIFF_NEXT, "DiffNext") //	{F_DIFF_NEXT,				LTEXT("DiffNext"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //DIFF差分表示(次へ)			//@@@ 2002.05.25 MIK
		LUA_SAKURA_FUNC_C(F_DIFF_PREV, "DiffPrev") //	{F_DIFF_PREV,				LTEXT("DiffPrev"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //DIFF差分表示(前へ)			//@@@ 2002.05.25 MIK
		LUA_SAKURA_FUNC_C(F_DIFF_RESET, "DiffReset") //	{F_DIFF_RESET,				LTEXT("DiffReset"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //DIFF差分表示(全解除)		//@@@ 2002.05.25 MIK
		LUA_SAKURA_FUNC_C(F_BRACKETPAIR, "BracketPair") //	{F_BRACKETPAIR,				LTEXT("BracketPair"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //対括弧の検索
		LUA_SAKURA_FUNC_C(F_BOOKMARK_SET, "BookmarkSet") //	{F_BOOKMARK_SET,			LTEXT("BookmarkSet"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ブックマーク設定・解除
		LUA_SAKURA_FUNC_C(F_BOOKMARK_NEXT, "BookmarkNext") //	{F_BOOKMARK_NEXT,			LTEXT("BookmarkNext"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次のブックマークへ
		LUA_SAKURA_FUNC_C(F_BOOKMARK_PREV, "BookmarkPrev") //	{F_BOOKMARK_PREV,			LTEXT("BookmarkPrev"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前のブックマークへ
		LUA_SAKURA_FUNC_C(F_BOOKMARK_RESET, "BookmarkReset") //	{F_BOOKMARK_RESET,			LTEXT("BookmarkReset"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ブックマークの全解除
		LUA_SAKURA_FUNC_C(F_BOOKMARK_VIEW, "BookmarkView") //	{F_BOOKMARK_VIEW,			LTEXT("BookmarkView"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ブックマークの一覧
		LUA_SAKURA_FUNC_C(F_BOOKMARK_PATTERN, "BookmarkPattern") //	{F_BOOKMARK_PATTERN,		LTEXT("BookmarkPattern"),	{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // 2002.01.16 hor 指定パターンに一致する行をマーク
		LUA_SAKURA_FUNC_C(F_FUNCLIST_NEXT, "FuncListNext") //	{F_FUNCLIST_NEXT,			LTEXT("FuncListNext"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次の関数リストマークへ
		LUA_SAKURA_FUNC_C(F_FUNCLIST_PREV, "FuncListPrev") //	{F_FUNCLIST_PREV,			LTEXT("FuncListPrev"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前の関数リストマークへ

		/* モード切り替え系 */
		LUA_SAKURA_FUNC_C(F_CHGMOD_INS, "ChgmodINS") //	{F_CHGMOD_INS,				LTEXT("ChgmodINS"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //挿入／上書きモード切り替え
		LUA_SAKURA_FUNC_C(F_CHG_CHARSET, "ChgCharSet") //	{F_CHG_CHARSET,				LTEXT("ChgCharSet"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //文字コードセット指定		2010/6/14 Uchi
		LUA_SAKURA_FUNC_C(F_CHGMOD_EOL, "ChgmodEOL") //	{F_CHGMOD_EOL,				LTEXT("ChgmodEOL"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //入力改行コード指定 2003.06.23 Moca
		LUA_SAKURA_FUNC_C(F_CANCEL_MODE, "CancelMode") //	{F_CANCEL_MODE,				LTEXT("CancelMode"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //各種モードの取り消し

		/* マクロ系 */
		LUA_SAKURA_FUNC_C(F_EXECEXTMACRO, "ExecExternalMacro") //	{F_EXECEXTMACRO,			LTEXT("ExecExternalMacro"),	{VT_BSTR, VT_BSTR, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //名前を指定してマクロ実行

		/* 設定系 */
		LUA_SAKURA_FUNC_C(F_SHOWTOOLBAR, "ShowToolbar") //	{F_SHOWTOOLBAR,				LTEXT("ShowToolbar"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* ツールバーの表示 */
		LUA_SAKURA_FUNC_C(F_SHOWFUNCKEY, "ShowFunckey") //	{F_SHOWFUNCKEY,				LTEXT("ShowFunckey"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* ファンクションキーの表示 */
		LUA_SAKURA_FUNC_C(F_SHOWTAB, "ShowTab") //	{F_SHOWTAB,					LTEXT("ShowTab"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* タブの表示 */	//@@@ 2003.06.10 MIK
		LUA_SAKURA_FUNC_C(F_SHOWSTATUSBAR, "ShowStatusbar") //	{F_SHOWSTATUSBAR,			LTEXT("ShowStatusbar"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* ステータスバーの表示 */
		LUA_SAKURA_FUNC_C(F_SHOWMINIMAP, "ShowMiniMap") //	{F_SHOWMINIMAP,				LTEXT("ShowMiniMap"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // ミニマップの表示
		LUA_SAKURA_FUNC_C(F_TYPE_LIST, "TypeList") //	{F_TYPE_LIST,				LTEXT("TypeList"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* タイプ別設定一覧 */
		LUA_SAKURA_FUNC_C(F_CHANGETYPE, "ChangeType") //	{F_CHANGETYPE,				LTEXT("ChangeType"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タイプ別設定一時適用 2013.05.02
		LUA_SAKURA_FUNC_C(F_OPTION_TYPE, "OptionType") //	{F_OPTION_TYPE,				LTEXT("OptionType"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* タイプ別設定 */
		LUA_SAKURA_FUNC_C(F_OPTION, "OptionCommon") //	{F_OPTION,					LTEXT("OptionCommon"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 共通設定 */
		LUA_SAKURA_FUNC_C(F_FONT, "SelectFont") //	{F_FONT,					LTEXT("SelectFont"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* フォント設定 */
		LUA_SAKURA_FUNC_C(F_SETFONTSIZE, "SetFontSize") //	{F_SETFONTSIZE,				LTEXT("SetFontSize"),		{VT_I4,    VT_I4,    VT_I4,    VT_EMPTY},	VT_EMPTY,	NULL}, /* フォントサイズ設定 */
		LUA_SAKURA_FUNC_C(F_WRAPWINDOWWIDTH, "WrapWindowWidth") //	{F_WRAPWINDOWWIDTH,			LTEXT("WrapWindowWidth"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 現在のウィンドウ幅で折り返し */	//Oct. 7, 2000 JEPRO WRAPWINDIWWIDTH を WRAPWINDOWWIDTH に変更
		LUA_SAKURA_FUNC_C(F_FAVORITE, "OptionFavorite") //	{F_FAVORITE,				LTEXT("OptionFavorite"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 履歴の管理 */	//@@@ 2003.04.08 MIK
		LUA_SAKURA_FUNC_C(F_SET_QUOTESTRING, "SetMsgQuoteStr") //	{F_SET_QUOTESTRING,			LTEXT("SetMsgQuoteStr"),	{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 共通設定→書式→引用符の設定 */	//Jan. 29, 2005 genta
		LUA_SAKURA_FUNC_C(F_TEXTWRAPMETHOD, "TextWrapMethod") //	{F_TEXTWRAPMETHOD,			LTEXT("TextWrapMethod"),	{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* テキストの折り返し方法 */	// 2008.05.30 nasukoji
		LUA_SAKURA_FUNC_C(F_SELECT_COUNT_MODE, "SelectCountMode") //	{F_SELECT_COUNT_MODE,		LTEXT("SelectCountMode"),	{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //文字カウント方法
		LUA_SAKURA_FUNC_C(F_EXECMD, "ExecCommand") //	{F_EXECMD,					LTEXT("ExecCommand"),		{VT_BSTR,  VT_I4,    VT_BSTR,  VT_EMPTY},	VT_EMPTY,	NULL}, /* 外部コマンド実行 */
		LUA_SAKURA_FUNC_C(F_EXECMD_DIALOG, "ExecCommandDialog") //	{F_EXECMD_DIALOG,			LTEXT("ExecCommandDialog"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //外部コマンド実行(ダイアログ)

		/* カスタムメニュー */
		LUA_SAKURA_FUNC_C(F_MENU_RBUTTON, "RMenu") //	{F_MENU_RBUTTON,			LTEXT("RMenu"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 右クリックメニュー */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_1, "CustMenu1") //	{F_CUSTMENU_1,				LTEXT("CustMenu1"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー1 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_2, "CustMenu2") //	{F_CUSTMENU_2,				LTEXT("CustMenu2"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー2 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_3, "CustMenu3") //	{F_CUSTMENU_3,				LTEXT("CustMenu3"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー3 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_4, "CustMenu4") //	{F_CUSTMENU_4,				LTEXT("CustMenu4"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー4 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_5, "CustMenu5") //	{F_CUSTMENU_5,				LTEXT("CustMenu5"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー5 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_6, "CustMenu6") //	{F_CUSTMENU_6,				LTEXT("CustMenu6"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー6 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_7, "CustMenu7") //	{F_CUSTMENU_7,				LTEXT("CustMenu7"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー7 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_8, "CustMenu8") //	{F_CUSTMENU_8,				LTEXT("CustMenu8"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー8 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_9, "CustMenu9") //	{F_CUSTMENU_9,				LTEXT("CustMenu9"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー9 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_10, "CustMenu10") //	{F_CUSTMENU_10,				LTEXT("CustMenu10"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー10 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_11, "CustMenu11") //	{F_CUSTMENU_11,				LTEXT("CustMenu11"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー11 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_12, "CustMenu12") //	{F_CUSTMENU_12,				LTEXT("CustMenu12"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー12 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_13, "CustMenu13") //	{F_CUSTMENU_13,				LTEXT("CustMenu13"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー13 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_14, "CustMenu14") //	{F_CUSTMENU_14,				LTEXT("CustMenu14"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー14 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_15, "CustMenu15") //	{F_CUSTMENU_15,				LTEXT("CustMenu15"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー15 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_16, "CustMenu16") //	{F_CUSTMENU_16,				LTEXT("CustMenu16"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー16 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_17, "CustMenu17") //	{F_CUSTMENU_17,				LTEXT("CustMenu17"), 		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー17 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_18, "CustMenu18") //	{F_CUSTMENU_18,				LTEXT("CustMenu18"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー18 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_19, "CustMenu19") //	{F_CUSTMENU_19,				LTEXT("CustMenu19"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー19 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_20, "CustMenu20") //	{F_CUSTMENU_20,				LTEXT("CustMenu20"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー20 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_21, "CustMenu21") //	{F_CUSTMENU_21,				LTEXT("CustMenu21"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー21 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_22, "CustMenu22") //	{F_CUSTMENU_22,				LTEXT("CustMenu22"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー22 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_23, "CustMenu23") //	{F_CUSTMENU_23,				LTEXT("CustMenu23"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー23 */
		LUA_SAKURA_FUNC_C(F_CUSTMENU_24, "CustMenu24") //	{F_CUSTMENU_24,				LTEXT("CustMenu24"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* カスタムメニュー24 */

		/* ウィンドウ系 */
		LUA_SAKURA_FUNC_C(F_SPLIT_V, "SplitWinV") //	{F_SPLIT_V,					LTEXT("SplitWinV"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //上下に分割	//Sept. 17, 2000 jepro 説明の「縦」を「上下に」に変更
		LUA_SAKURA_FUNC_C(F_SPLIT_H, "SplitWinH") //	{F_SPLIT_H,					LTEXT("SplitWinH"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //左右に分割	//Sept. 17, 2000 jepro 説明の「横」を「左右に」に変更
		LUA_SAKURA_FUNC_C(F_SPLIT_VH, "SplitWinVH") //	{F_SPLIT_VH,				LTEXT("SplitWinVH"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //縦横に分割	//Sept. 17, 2000 jepro 説明に「に」を追加
		LUA_SAKURA_FUNC_C(F_WINCLOSE, "WinClose") //	{F_WINCLOSE,				LTEXT("WinClose"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ウィンドウを閉じる
		LUA_SAKURA_FUNC_C(F_WIN_CLOSEALL, "WinCloseAll") //	{F_WIN_CLOSEALL,			LTEXT("WinCloseAll"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //すべてのウィンドウを閉じる	//Oct. 17, 2000 JEPRO 名前を変更(F_FILECLOSEALL→F_WIN_CLOSEALL)
		LUA_SAKURA_FUNC_C(F_CASCADE, "CascadeWin") //	{F_CASCADE,					LTEXT("CascadeWin"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //重ねて表示
		LUA_SAKURA_FUNC_C(F_TILE_V, "TileWinV") //	{F_TILE_V,					LTEXT("TileWinV"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //上下に並べて表示
		LUA_SAKURA_FUNC_C(F_TILE_H, "TileWinH") //	{F_TILE_H,					LTEXT("TileWinH"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //左右に並べて表示
		LUA_SAKURA_FUNC_C(F_NEXTWINDOW, "NextWindow") //	{F_NEXTWINDOW,				LTEXT("NextWindow"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次のウィンドウ
		LUA_SAKURA_FUNC_C(F_PREVWINDOW, "PrevWindow") //	{F_PREVWINDOW,				LTEXT("PrevWindow"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前のウィンドウ
		LUA_SAKURA_FUNC_C(F_WINLIST, "WindowList") //	{F_WINLIST,					LTEXT("WindowList"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ウィンドウ一覧ポップアップ表示	// 2006.03.23 fon
		LUA_SAKURA_FUNC_C(F_MAXIMIZE_V, "MaximizeV") //	{F_MAXIMIZE_V,				LTEXT("MaximizeV"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //縦方向に最大化
		LUA_SAKURA_FUNC_C(F_MAXIMIZE_H, "MaximizeH") //	{F_MAXIMIZE_H,				LTEXT("MaximizeH"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //横方向に最大化 //2001.02.10 by MIK
		LUA_SAKURA_FUNC_C(F_MINIMIZE_ALL, "MinimizeAll") //	{F_MINIMIZE_ALL,			LTEXT("MinimizeAll"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //すべて最小化	//Sept. 17, 2000 jepro 説明の「全て」を「すべて」に統一
		LUA_SAKURA_FUNC_C(F_REDRAW, "ReDraw") //	{F_REDRAW,					LTEXT("ReDraw"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //再描画
		LUA_SAKURA_FUNC_C(F_WIN_OUTPUT, "ActivateWinOutput") //	{F_WIN_OUTPUT,				LTEXT("ActivateWinOutput"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //アウトプットウィンドウ表示
		LUA_SAKURA_FUNC_C(F_TRACEOUT, "TraceOut") //	{F_TRACEOUT,				LTEXT("TraceOut"),			{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //マクロ用アウトプットウィンドウに出力	2006.04.26 maru
		LUA_SAKURA_FUNC_C(F_TOPMOST, "WindowTopMost") //	{F_TOPMOST,					LTEXT("WindowTopMost"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //常に手前に表示
		LUA_SAKURA_FUNC_C(F_GROUPCLOSE, "GroupClose") //	{F_GROUPCLOSE,				LTEXT("GroupClose"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //グループを閉じる	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_NEXTGROUP, "NextGroup") //	{F_NEXTGROUP,				LTEXT("NextGroup"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次のグループ	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_PREVGROUP, "PrevGroup") //	{F_PREVGROUP,				LTEXT("PrevGroup"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前のグループ	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_MOVERIGHT, "TabMoveRight") //	{F_TAB_MOVERIGHT,			LTEXT("TabMoveRight"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タブを右に移動	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_MOVELEFT, "TabMoveLeft") //	{F_TAB_MOVELEFT,			LTEXT("TabMoveLeft"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //タブを左に移動	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_SEPARATE, "TabSeparate") //	{F_TAB_SEPARATE,			LTEXT("TabSeparate"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //新規グループ	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_JOINTNEXT, "TabJointNext") //	{F_TAB_JOINTNEXT,			LTEXT("TabJointNext"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //次のグループに移動	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_JOINTPREV, "TabJointPrev") //	{F_TAB_JOINTPREV,			LTEXT("TabJointPrev"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //前のグループに移動	// 2007.06.20 ryoji
		LUA_SAKURA_FUNC_C(F_TAB_CLOSEOTHER, "TabCloseOther") //	{F_TAB_CLOSEOTHER,			LTEXT("TabCloseOther"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //このタブ以外を閉じる	// 2010/3/14 Uchi
		LUA_SAKURA_FUNC_C(F_TAB_CLOSELEFT, "TabCloseLeft") //	{F_TAB_CLOSELEFT,			LTEXT("TabCloseLeft"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //左をすべて閉じる		// 2010/3/14 Uchi
		LUA_SAKURA_FUNC_C(F_TAB_CLOSERIGHT, "TabCloseRight") //	{F_TAB_CLOSERIGHT,			LTEXT("TabCloseRight"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //右をすべて閉じる		// 2010/3/14 Uchi

		/* 支援 */
		LUA_SAKURA_FUNC_C(F_HOKAN, "Complete") //	{F_HOKAN,					LTEXT("Complete"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 入力補完 */	//Oct. 15, 2000 JEPRO 入ってなかったので英名を付けて入れてみた
		LUA_SAKURA_FUNC_C(F_TOGGLE_KEY_SEARCH, "ToggleKeyHelpSearch") //	{F_TOGGLE_KEY_SEARCH,		LTEXT("ToggleKeyHelpSearch"), {VT_I4, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //キーワードヘルプ自動表示 2013.05.03
		LUA_SAKURA_FUNC_C(F_HELP_CONTENTS, "HelpContents") //	{F_HELP_CONTENTS,			LTEXT("HelpContents"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* ヘルプ目次 */			//Nov. 25, 2000 JEPRO 追加
		LUA_SAKURA_FUNC_C(F_HELP_SEARCH, "HelpSearch") //	{F_HELP_SEARCH,				LTEXT("HelpSearch"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* ヘルプキーワード検索 */	//Nov. 25, 2000 JEPRO 追加
		LUA_SAKURA_FUNC_C(F_MENU_ALLFUNC, "CommandList") //	{F_MENU_ALLFUNC,			LTEXT("CommandList"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* コマンド一覧 */
		LUA_SAKURA_FUNC_C(F_EXTHELP1, "ExtHelp1") //	{F_EXTHELP1,				LTEXT("ExtHelp1"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 外部ヘルプ１ */
		LUA_SAKURA_FUNC_C(F_EXTHTMLHELP, "ExtHtmlHelp") //	{F_EXTHTMLHELP,				LTEXT("ExtHtmlHelp"),		{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* 外部HTMLヘルプ */
		LUA_SAKURA_FUNC_C(F_ABOUT, "About") //	{F_ABOUT,					LTEXT("About"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, /* バージョン情報 */	//Dec. 24, 2000 JEPRO 追加

		/*マクロ用*/
		LUA_SAKURA_FUNC_C(F_STATUSMSG, "StatusMsg") //	{F_STATUSMSG,				LTEXT("StatusMsg"),			{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //ステータスメッセージ
		LUA_SAKURA_FUNC_C(F_MSGBEEP, "MsgBeep") //	{F_MSGBEEP,					LTEXT("MsgBeep"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, //Beep音
		LUA_SAKURA_FUNC_C(F_COMMITUNDOBUFFER, "CommitUndoBuffer") //	{F_COMMITUNDOBUFFER,		LTEXT("CommitUndoBuffer"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL }, //OpeBlKコミット
		LUA_SAKURA_FUNC_C(F_ADDREFUNDOBUFFER, "AddRefUndoBuffer") //	{F_ADDREFUNDOBUFFER,		LTEXT("AddRefUndoBuffer"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL }, //OpeBlK AddRef
		LUA_SAKURA_FUNC_C(F_SETUNDOBUFFER, "SetUndoBuffer") //	{F_SETUNDOBUFFER,			LTEXT("SetUndoBuffer"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL }, //OpeBlK Release
		LUA_SAKURA_FUNC_C(F_APPENDUNDOBUFFERCURSOR, "AppendUndoBufferCursor") //	{F_APPENDUNDOBUFFERCURSOR,	L"AppendUndoBufferCursor",	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL }, //OpeBlK にカーソル位置を追加
		LUA_SAKURA_FUNC_C(F_CLIPBOARDEMPTY, "ClipboardEmpty") //	{F_CLIPBOARDEMPTY,			LTEXT("ClipboardEmpty"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL},
		LUA_SAKURA_FUNC_C(F_SETVIEWTOP, "SetViewTop") //	{F_SETVIEWTOP,				L"SetViewTop",				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // ビューの上の行数を設定
		LUA_SAKURA_FUNC_C(F_SETVIEWLEFT, "SetViewLeft") //	{F_SETVIEWLEFT,				L"SetViewLeft",				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}, // ビューの左端の桁数を設定
		
		LUA_SAKURA_FUNC_F(F_GETFILENAME, "GetFilename") //	{F_GETFILENAME,			LTEXT("GetFilename"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //ファイル名を返す
		LUA_SAKURA_FUNC_F(F_GETSAVEFILENAME, "GetSaveFilename") //	{F_GETSAVEFILENAME,		LTEXT("GetSaveFilename"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //保存時のファイル名を返す 2006.09.04 ryoji
		LUA_SAKURA_FUNC_F(F_GETSELECTED, "GetSelectedString") //	{F_GETSELECTED,			LTEXT("GetSelectedString"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //選択部分
		LUA_SAKURA_FUNC_F(F_EXPANDPARAMETER, "ExpandParameter") //	{F_EXPANDPARAMETER,		LTEXT("ExpandParameter"),		{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //特殊文字の展開
		LUA_SAKURA_FUNC_F(F_GETLINESTR, "GetLineStr") //	{F_GETLINESTR,			LTEXT("GetLineStr"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, // 指定論理行の取得 2003.06.01 Moca
		LUA_SAKURA_FUNC_F(F_GETLINECOUNT, "GetLineCount") //	{F_GETLINECOUNT,		LTEXT("GetLineCount"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 全論理行数の取得 2003.06.01 Moca
		LUA_SAKURA_FUNC_F(F_CHGTABWIDTH, "ChangeTabWidth") //	{F_CHGTABWIDTH,			LTEXT("ChangeTabWidth"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //タブサイズ変更 2004.03.16 zenryaku
		LUA_SAKURA_FUNC_F(F_ISTEXTSELECTED, "IsTextSelected") //	{F_ISTEXTSELECTED,		LTEXT("IsTextSelected"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //テキストが選択されているか 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELLINEFROM, "GetSelectLineFrom") //	{F_GETSELLINEFROM,		LTEXT("GetSelectLineFrom"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択開始行の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNFROM, "GetSelectColmFrom") //	{F_GETSELCOLUMNFROM,	LTEXT("GetSelectColmFrom"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択開始桁の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNFROM, "GetSelectColumnFrom") //	{F_GETSELCOLUMNFROM,	LTEXT("GetSelectColumnFrom"),	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択開始桁の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELLINETO, "GetSelectLineTo") //	{F_GETSELLINETO,		LTEXT("GetSelectLineTo"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択終了行の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNTO, "GetSelectColmTo") //	{F_GETSELCOLUMNTO,		LTEXT("GetSelectColmTo"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択終了桁の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETSELCOLUMNTO, "GetSelectColumnTo") //	{F_GETSELCOLUMNTO,		LTEXT("GetSelectColumnTo"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 選択終了桁の取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_ISINSMODE, "IsInsMode") //	{F_ISINSMODE,			LTEXT("IsInsMode"),				{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 挿入／上書きモードの取得 2005.7.30 maru
		LUA_SAKURA_FUNC_F(F_GETCHARCODE, "GetCharCode") //	{F_GETCHARCODE,			LTEXT("GetCharCode"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 文字コード取得 2005.07.31 maru
		LUA_SAKURA_FUNC_F(F_GETLINECODE, "GetLineCode") //	{F_GETLINECODE,			LTEXT("GetLineCode"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 改行コード取得 2005.08.05 maru
		LUA_SAKURA_FUNC_F(F_ISPOSSIBLEUNDO, "IsPossibleUndo") //	{F_ISPOSSIBLEUNDO,		LTEXT("IsPossibleUndo"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // Undo可能か調べる 2005.08.05 maru
		LUA_SAKURA_FUNC_F(F_ISPOSSIBLEREDO, "IsPossibleRedo") //	{F_ISPOSSIBLEREDO,		LTEXT("IsPossibleRedo"),		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // Redo可能か調べる 2005.08.05 maru
		LUA_SAKURA_FUNC_F(F_CHGWRAPCOLUMN, "ChangeWrapColm") //	{F_CHGWRAPCOLUMN,		LTEXT("ChangeWrapColm"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //折り返し桁変更 2008.06.19 ryoji
		LUA_SAKURA_FUNC_F(F_CHGWRAPCOLUMN, "ChangeWrapColumn") //	{F_CHGWRAPCOLUMN,		LTEXT("ChangeWrapColumn"),		{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //折り返し桁変更 2008.06.19 ryoji
		LUA_SAKURA_FUNC_F(F_ISCURTYPEEXT, "IsCurTypeExt") //	{F_ISCURTYPEEXT,		LTEXT("IsCurTypeExt"),			{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // 指定した拡張子が現在のタイプ別設定に含まれているかどうかを調べる 2006.09.04 ryoji
		LUA_SAKURA_FUNC_F(F_ISSAMETYPEEXT, "IsSameTypeExt") //	{F_ISSAMETYPEEXT,		LTEXT("IsSameTypeExt"),			{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, // ２つの拡張子が同じタイプ別設定に含まれているかどうかを調べる 2006.09.04 ryoji
		LUA_SAKURA_FUNC_F(F_INPUTBOX, "InputBox") //	{F_INPUTBOX,			LTEXT("InputBox"),				{VT_BSTR,  VT_BSTR,  VT_I4,    VT_EMPTY},	VT_BSTR,	NULL }, //テキスト入力ダイアログの表示
		LUA_SAKURA_FUNC_F(F_MESSAGEBOX, "MessageBox") //	{F_MESSAGEBOX,			LTEXT("MessageBox"),			{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックスの表示
		LUA_SAKURA_FUNC_F(F_ERRORMSG, "ErrorMsg") //	{F_ERRORMSG,			LTEXT("ErrorMsg"),				{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックス（エラー）の表示
		LUA_SAKURA_FUNC_F(F_WARNMSG, "WarnMsg") //	{F_WARNMSG,				LTEXT("WarnMsg"),				{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックス（警告）の表示
		LUA_SAKURA_FUNC_F(F_INFOMSG, "InfoMsg") //	{F_INFOMSG,				LTEXT("InfoMsg"),				{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックス（情報）の表示
		LUA_SAKURA_FUNC_F(F_OKCANCELBOX, "OkCancelBox") //	{F_OKCANCELBOX,			LTEXT("OkCancelBox"),			{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックス（確認：OK／キャンセル）の表示
		LUA_SAKURA_FUNC_F(F_YESNOBOX, "YesNoBox") //	{F_YESNOBOX,			LTEXT("YesNoBox"),				{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メッセージボックス（確認：はい／いいえ）の表示
		LUA_SAKURA_FUNC_F(F_COMPAREVERSION, "CompareVersion") //	{F_COMPAREVERSION,		LTEXT("CompareVersion"),		{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //バージョン番号の比較
		LUA_SAKURA_FUNC_F(F_MACROSLEEP, "Sleep") //	{F_MACROSLEEP,			LTEXT("Sleep"),					{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //指定した時間（ミリ秒）停止
		LUA_SAKURA_FUNC_F(F_FILEOPENDIALOG, "FileOpenDialog") //	{F_FILEOPENDIALOG,		LTEXT("FileOpenDialog"),		{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //ファイルを開くダイアログの表示
		LUA_SAKURA_FUNC_F(F_FILESAVEDIALOG, "FileSaveDialog") //	{F_FILESAVEDIALOG,		LTEXT("FileSaveDialog"),		{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //ファイルを保存ダイアログの表示
		LUA_SAKURA_FUNC_F(F_FOLDERDIALOG, "FolderDialog") //	{F_FOLDERDIALOG,		LTEXT("FolderDialog"),			{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //フォルダを開くダイアログの表示
		LUA_SAKURA_FUNC_F(F_GETCLIPBOARD, "GetClipboard") //	{F_GETCLIPBOARD,		LTEXT("GetClipboard"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //クリップボードの文字列を取得
		LUA_SAKURA_FUNC_F(F_SETCLIPBOARD, "SetClipboard") //	{F_SETCLIPBOARD,		LTEXT("SetClipboard"),			{VT_I4,    VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //クリップボードに文字列を設定
		LUA_SAKURA_FUNC_F(F_LAYOUTTOLOGICLINENUM, "LayoutToLogicLineNum") //	{F_LAYOUTTOLOGICLINENUM,LTEXT("LayoutToLogicLineNum"),	{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //ロジック行番号取得
		LUA_SAKURA_FUNC_F(F_LOGICTOLAYOUTLINENUM, "LogicToLayoutLineNum") //	{F_LOGICTOLAYOUTLINENUM,LTEXT("LogicToLayoutLineNum"),	{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //レイアウト行番号取得
		LUA_SAKURA_FUNC_F(F_LINECOLUMNTOINDEX, "LineColumnToIndex") //	{F_LINECOLUMNTOINDEX,	LTEXT("LineColumnToIndex"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //ロジック桁番号取得
		LUA_SAKURA_FUNC_F(F_LINEINDEXTOCOLUMN, "LineIndexToColumn") //	{F_LINEINDEXTOCOLUMN,	LTEXT("LineIndexToColumn"),		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //レイアウト桁番号取得
		LUA_SAKURA_FUNC_F(F_GETCOOKIE, "GetCookie") //	{F_GETCOOKIE,			LTEXT("GetCookie"),				{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //Cookie取得
		LUA_SAKURA_FUNC_F(F_GETCOOKIEDEFAULT, "GetCookieDefault") //	{F_GETCOOKIEDEFAULT,	LTEXT("GetCookieDefault"),		{VT_BSTR,  VT_BSTR,  VT_BSTR,  VT_EMPTY},	VT_BSTR,	NULL }, //Cookie取得デフォルト値
		LUA_SAKURA_FUNC_F(F_SETCOOKIE, "SetCookie") //	{F_SETCOOKIE,			LTEXT("SetCookie"),				{VT_BSTR,  VT_BSTR,  VT_BSTR,  VT_EMPTY},	VT_I4,		NULL }, //Cookie設定
		LUA_SAKURA_FUNC_F(F_DELETECOOKIE, "DeleteCookie") //	{F_DELETECOOKIE,		LTEXT("DeleteCookie"),			{VT_BSTR,  VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //Cookie削除
		LUA_SAKURA_FUNC_F(F_GETCOOKIENAMES, "GetCookieNames") //	{F_GETCOOKIENAMES,		LTEXT("GetCookieNames"),		{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //Cookie名前取得
		LUA_SAKURA_FUNC_F(F_SETDRAWSWITCH, "SetDrawSwitch") //	{F_SETDRAWSWITCH,		LTEXT("SetDrawSwitch"),			{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //再描画スイッチ設定
		LUA_SAKURA_FUNC_F(F_GETDRAWSWITCH, "GetDrawSwitch") //	{F_GETDRAWSWITCH,		LTEXT("GetDrawSwitch"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //再描画スイッチ取得
		LUA_SAKURA_FUNC_F(F_ISSHOWNSTATUS, "IsShownStatus") //	{F_ISSHOWNSTATUS,		LTEXT("IsShownStatus"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //ステータスバーが表示されているか
		LUA_SAKURA_FUNC_F(F_GETSTRWIDTH, "GetStrWidth") //	{F_GETSTRWIDTH,			LTEXT("GetStrWidth"),			{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //文字列幅取得
		LUA_SAKURA_FUNC_F(F_GETSTRLAYOUTLENGTH, "GetStrLayoutLength") //	{F_GETSTRLAYOUTLENGTH,	LTEXT("GetStrLayoutLength"),	{VT_BSTR,  VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //文字列のレイアウト幅取得
		LUA_SAKURA_FUNC_F(F_GETDEFAULTCHARLENGTH, "GetDefaultCharLength") //	{F_GETDEFAULTCHARLENGTH,	L"GetDefaultCharLength",	{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //デフォルト文字幅の取得
		LUA_SAKURA_FUNC_F(F_ISINCLUDECLIPBOARDFORMAT, "IsIncludeClipboardFormat") //	{F_ISINCLUDECLIPBOARDFORMAT,L"IsIncludeClipboardFormat",{VT_BSTR,  VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //クリップボードの形式取得
		LUA_SAKURA_FUNC_F(F_GETCLIPBOARDBYFORMAT, "GetClipboardByFormat") //	{F_GETCLIPBOARDBYFORMAT,	L"GetClipboardByFormat",	{VT_BSTR,  VT_I4,    VT_I4,    VT_EMPTY},	VT_BSTR,	NULL }, //クリップボードの指定形式で取得
		LUA_SAKURA_FUNC_F(F_SETCLIPBOARDBYFORMAT, "SetClipboardByFormat") //	{F_SETCLIPBOARDBYFORMAT,	L"SetClipboardByFormat",	{VT_BSTR,  VT_BSTR,  VT_I4,    VT_I4,    },	VT_I4,		NULL }, //クリップボードの指定形式で設定
		LUA_SAKURA_FUNC_F(F_GETLINEATTRIBUTE, "GetLineAttribute") //	{F_GETLINEATTRIBUTE,		L"GetLineAttribute",		{VT_I4,    VT_I4,    VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //行属性取得
		LUA_SAKURA_FUNC_F(F_ISTEXTSELECTINGLOCK, "IsTextSelectingLock") //	{F_ISTEXTSELECTINGLOCK,		L"IsTextSelectingLock",		{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //選択状態のロックを取得
		LUA_SAKURA_FUNC_F(F_GETVIEWLINES, "GetViewLines") //	{F_GETVIEWLINES,			L"GetViewLines",			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //ビューの行数取得
		LUA_SAKURA_FUNC_F(F_GETVIEWCOLUMNS, "GetViewColumns") //	{F_GETVIEWCOLUMNS,			L"GetViewColumns",			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //ビューの列数取得
		LUA_SAKURA_FUNC_F(F_CREATEMENU, "CreateMenu") //	{F_CREATEMENU,				L"CreateMenu",				{VT_I4,    VT_BSTR,  VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //メニュー作成
		
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
	//LUA_ADD_FUNC(SearchDialog);

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

