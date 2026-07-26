/*!	@file
	@brief QuickJSプラグインクラス

	CWSHPlugin.cppと同じ構造(CWSHMacroManager→CQuickJSMacroMgrへの置き換えのみ)。

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_QUICKJS_MACRO

#include "plugin/CQuickJSPlugin.h"
#include "plugin/CPluginIfObj.h"

// デストラクタ
CQuickJSPlugin::~CQuickJSPlugin(void)
{
	for( CPlug::ArrayIter it = m_plugs.begin(); it != m_plugs.end(); it++ ){
		delete *it;
	}
}

//プラグイン定義ファイルを読み込む
bool CQuickJSPlugin::ReadPluginDef( CDataProfile *cProfile, CDataProfile *cProfileMlang )
{
	ReadPluginDefCommon( cProfile, cProfileMlang );

	//Qjsセクションの読み込み
	cProfile->IOProfileData<bool>( PII_QJS, PII_QJS_USECACHE, m_bUseCache );

	//プラグの読み込み
	ReadPluginDefPlug( cProfile, cProfileMlang );

	//コマンドの読み込み
	ReadPluginDefCommand( cProfile, cProfileMlang );

	//オプション定義の読み込み
	ReadPluginDefOption( cProfile, cProfileMlang );

	//文字列定義の読み込み
	ReadPluginDefString( cProfile, cProfileMlang );

	return true;
}

//オプションファイルを読み込む
bool CQuickJSPlugin::ReadPluginOption( CDataProfile *cProfile )
{
	return true;
}

//プラグを実行する
bool CQuickJSPlugin::InvokePlug( CEditView* view, CPlug& plug, CWSHIfObj::List& params )
{
	CQuickJSPlug& qjsPlug = static_cast<CQuickJSPlug&>( plug );
	CQuickJSMacroMgr* pQuickJS = NULL;

	if( !m_bUseCache || qjsPlug.m_QuickJS == NULL ){
		CFilePath path( plug.m_cPlugin.GetFilePath( to_tchar(plug.m_sHandler.c_str()) ).c_str() );

		pQuickJS = (CQuickJSMacroMgr*)CQuickJSMacroMgr::Creator( path.GetExt( true ) );
		if( pQuickJS == NULL ){ return false; }

		BOOL bLoadResult = pQuickJS->LoadKeyMacro( G_AppInstance(), path );
		if ( !bLoadResult ){
			ErrorMessage( NULL, LS(STR_WSHPLUG_LOADMACRO), static_cast<const TCHAR*>(path) );
			delete pQuickJS;
			return false;
		}

	}else{
		pQuickJS = qjsPlug.m_QuickJS;
	}

	//	CPluginIfObjはスタック上に確保する。CQuickJSIfObjBinder::BindObjectは
	//	IDispatch/COM参照カウント(AddRef/Release)を一切使わずポインタをそのまま
	//	保持・破棄するだけなので、WSH版(CWSHPlugin.cpp)のようなAddRef()による
	//	参照カウントの帳尻合わせ(スタックオブジェクトがRelease()でdeleteされない
	//	ようにする対策)は不要。
	CPluginIfObj cPluginIfo(*this);		//Pluginオブジェクトを追加
	cPluginIfo.SetPlugIndex( plug.m_id );	//実行中プラグ番号を提供
	pQuickJS->AddParam( &cPluginIfo );

	pQuickJS->AddParam( params );			//パラメータを追加

	pQuickJS->ExecKeyMacro2( view, FA_NONRECORD | FA_FROMMACRO );

	pQuickJS->ClearParam();

	if( m_bUseCache ){
		qjsPlug.m_QuickJS = pQuickJS;
	}else{
		// 終わったら開放
		delete pQuickJS;
	}

	return true;
}

#endif // NKMM_FIX_QUICKJS_MACRO
/*[EOF]*/
