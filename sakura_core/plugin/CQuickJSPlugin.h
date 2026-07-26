/*!	@file
	@brief QuickJSプラグインクラス

	CWSHPlugin/CWSHPlugと同じ構造で、CWSHMacroManagerの代わりにCQuickJSMacroMgrを
	使う。plugin.defのTypeに"qjs"を指定したプラグイン向け。

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#ifndef SAKURA_CQUICKJSPLUGIN_9D4E1B7A_6C3F_4A2E_8B5D_2C6B9B8B7C4A_H_
#define SAKURA_CQUICKJSPLUGIN_9D4E1B7A_6C3F_4A2E_8B5D_2C6B9B8B7C4A_H_

#ifdef NKMM_FIX_QUICKJS_MACRO

#include "plugin/CPlugin.h"
#include "macro/CQuickJSMacroMgr.h"

#define	PII_QJS						L"Qjs"			//QuickJSセクション
#define	PII_QJS_USECACHE			L"UseCache"		//読み込んだスクリプトを再利用する

class CQuickJSPlug :
	public CPlug
{
public:
	CQuickJSPlug( CPlugin& plugin, PlugId id, wstring sJack, wstring sHandler, wstring sLabel ) :
		CPlug( plugin, id, sJack, sHandler, sLabel )
	{
		m_QuickJS = NULL;
	}
	virtual ~CQuickJSPlug() {
		if( m_QuickJS ){
			delete m_QuickJS;
			m_QuickJS = NULL;
		}
	}
	CQuickJSMacroMgr* m_QuickJS;
};

class CQuickJSPlugin :
	public CPlugin
{
	//コンストラクタ
public:
	CQuickJSPlugin( const tstring& sBaseDir ) : CPlugin( sBaseDir ) {
		m_bUseCache = false;
	}

	//デストラクタ
public:
	~CQuickJSPlugin(void);

	//操作
	//CPlugインスタンスの作成。ReadPluginDefPlug/Command から呼ばれる。
	virtual CPlug* CreatePlug( CPlugin& plugin, PlugId id, wstring sJack, wstring sHandler, wstring sLabel )
	{
		return new CQuickJSPlug( plugin, id, sJack, sHandler, sLabel );
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

#endif // NKMM_FIX_QUICKJS_MACRO

#endif /* SAKURA_CQUICKJSPLUGIN_9D4E1B7A_6C3F_4A2E_8B5D_2C6B9B8B7C4A_H_ */
/*[EOF]*/
