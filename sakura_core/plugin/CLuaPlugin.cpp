/*!	@file
	@brief Luaプラグインクラス

*/
/*
	Copyright (C) 2009, syat
	Copyright (C) 2017, Reiris

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
#include "StdAfx.h"
#include "plugin/CLuaPlugin.h"
#include "macro/CLuaMacroMgr.h"

// デストラクタ
CLuaPlugin::~CLuaPlugin(void)
{
	for( CPlug::ArrayIter it = m_plugs.begin(); it != m_plugs.end(); it++ ){
		delete *it;
	}
}

//プラグイン定義ファイルを読み込む
bool CLuaPlugin::ReadPluginDef( CDataProfile *cProfile, CDataProfile *cProfileMlang )
{
	ReadPluginDefCommon( cProfile, cProfileMlang );

	//Luaセクションの読み込み
	cProfile->IOProfileData<bool>( PII_LUA, PII_LUA_USECACHE, m_bUseCache );

	//プラグの読み込み
	ReadPluginDefPlug( cProfile, cProfileMlang );

	//コマンドの読み込み
	ReadPluginDefCommand( cProfile, cProfileMlang );

	//オプション定義の読み込み	// 2010/3/24 Uchi
	ReadPluginDefOption( cProfile, cProfileMlang );

	//文字列定義の読み込み
	ReadPluginDefString( cProfile, cProfileMlang );

	return true;
}

//オプションファイルを読み込む
bool CLuaPlugin::ReadPluginOption( CDataProfile *cProfile )
{
	return true;
}

//プラグを実行する
bool CLuaPlugin::InvokePlug( CEditView* view, CPlug& plug, CWSHIfObj::List& params )
{
	CLuaPlug& luaPlug = static_cast<CLuaPlug&>( plug );
	CLuaMacroMgr* pLua = NULL;

	if( !m_bUseCache || luaPlug.m_Lua == NULL ){
		CFilePath path( plug.m_cPlugin.GetFilePath( to_tchar(plug.m_sHandler.c_str()) ).c_str() );

		pLua = (CLuaMacroMgr*)CLuaMacroMgr::Creator( path.GetExt( true ) );
		if( pLua == NULL ){ return false; }

		BOOL bLoadResult = pLua->LoadKeyMacro( G_AppInstance(), path );
		if ( !bLoadResult ){
			//ErrorMessage( NULL, LS(STR_WSHPLUG_LOADMACRO), static_cast<const TCHAR*>(path) );
			delete pLua;
			return false;
		}

	}else{
		pLua = luaPlug.m_Lua;
	}

	//CPluginIfObj cPluginIfo(*this);		//Pluginオブジェクトを追加
	//cPluginIfo.AddRef();
	//cPluginIfo.SetPlugIndex( plug.m_id );	//実行中プラグ番号を提供
	//pLua->AddParam( &cPluginIfo );

	//pWsh->AddParam( params );			//パラメータを追加

	pLua->ExecKeyMacro2( view, FA_NONRECORD | FA_FROMMACRO );

	//pLua->ClearParam();

	if( m_bUseCache ){
		luaPlug.m_Lua = pLua;
	}else{
		// 終わったら開放
		delete pLua;
	}

	return true;
}
