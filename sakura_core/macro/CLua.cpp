/*!	@file
	@brief Lua Library Handler

	@date 2017.5.21
*/
/*
	Copyright (C) 2017, 

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose, 
	including commercial applications, and to alter it and redistribute it 
	freely, subject to the following restrictions:

		1. The origin of this software must not be misrepresented;
		   you must not claim that you wrote the original software.
		   If you use this software in a product, an acknowledgment
		   in the product documentation would be appreciated but is
		   not required.

		2. Altered source versions must be plainly marked as such, 
		   and must not be misrepresented as being the original software.

		3. This notice may not be removed or altered from any source
		   distribution.
*/

/*
http://flat-leon.hatenablog.com/entry/lua_script_file_divide
http://blog.livedoor.jp/akf0/archives/51600634.html
https://msdn.microsoft.com/ja-jp/library/6wtdswk0.aspx
http://qiita.com/hiz_/items/8739c46ddd2563a5603f
http://pgl.yoyo.org/luai/i/luaL_checkstring
http://qiita.com/miuk/items/82e5566ea01313a8b1af
http://luabinaries.sourceforge.net/download.html
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

static int l_log(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  mix::log(to_wchar(s));
  return 0;
}
static int l_logln(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  mix::logln(to_wchar(s));
  return 0;
}

CLua::LuaExecInfo *CLua::m_CurInstance = NULL;
bool CLua::m_bIsRunning = false;

CLua::CLua() {
}

CLua::~CLua() {
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
		lua_State *L;
		int ret;
		
		//L = lua_open();
		L = luaL_newstate();
		
		luaL_openlibs(L);
		
		// test
		lua_pushcfunction(L, l_log);
		lua_setglobal(L, "log");
		lua_pushcfunction(L, l_logln);
		lua_setglobal(L, "logln");
		
		//mix::logln(buffer.GetStringPtr());
		ret = luaL_dostring(L, to_achar(buffer.GetStringPtr()));
		
		if (ret != 0) {
			info.m_bError = true;
			mix::logln(L" ** Lua script, Failed");
		}
		
		lua_close(L);
	}
	
	//	マクロ実行完了後はここに戻ってくる
	m_CurInstance = old_instance;

	CLua::m_bIsRunning = false;
	return !info.m_bError;
}

