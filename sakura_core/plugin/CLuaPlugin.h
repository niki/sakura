/*!	@file
	@brief Luaプラグインクラス

*/
/*
	Copyright (C) 2009, syat
	Copyright (C) 2017, Calette

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
#ifndef CLUAPLUGIN_H
#define CLUAPLUGIN_H

#include "plugin/CPlugin.h"
#include "macro/CLuaMacroMgr.h"

#define PII_LUA						L"Lua"			//Luaセクション
#define PII_LUA_USECACHE			L"UseCache"		//読み込んだスクリプトを再利用する


class CLuaPlug :
	public CPlug
{
public:
	CLuaPlug( CPlugin& plugin, PlugId id, wstring sJack, wstring sHandler, wstring sLabel ) :
		CPlug( plugin, id, sJack, sHandler, sLabel )
	{
		m_Lua = NULL;
	}
	virtual ~CLuaPlug() {
		if( m_Lua ){
			delete m_Lua;
			m_Lua = NULL;
		}
	}
	CLuaMacroMgr* m_Lua;
};

class CLuaPlugin :
	public CPlugin
{
	//コンストラクタ
public:
	CLuaPlugin( const tstring& sBaseDir ) : CPlugin( sBaseDir ) {
		m_bUseCache = false;
	}

	//デストラクタ
public:
	~CLuaPlugin(void);

	//操作
	//CPlugインスタンスの作成。ReadPluginDefPlug/Command から呼ばれる。
	virtual CPlug* CreatePlug( CPlugin& plugin, PlugId id, wstring sJack, wstring sHandler, wstring sLabel )
	{
		return new CLuaPlug( plugin, id, sJack, sHandler, sLabel );
	}

	//実装
public:
	bool ReadPluginDef( CDataProfile *cProfile, CDataProfile *cProfileMlang );
	bool ReadPluginOption( CDataProfile *cProfile );
	CPlug::Array GetPlugs() const{
		return m_plugs;
	}
	bool InvokePlug( CEditView* view, CPlug& plug, CWSHIfObj::List& params );

	//メンバ変数
private:
	bool m_bUseCache;

};

#endif /* CLUAPLUGIN_H */
/*[EOF]*/
