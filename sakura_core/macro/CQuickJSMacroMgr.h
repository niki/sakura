/*!	@file
	@brief QuickJSマクロエンジン

	WSH(JScript/VBScript)に依存しない、QuickJS(quickjs-ng)ベースのマクロエンジン。
	拡張子".qjs"のマクロファイルを実行する。CWSHMacroManager/CPPAMacroMgrと同じ形で
	CMacroManagerBase(CMacroFactory経由)に登録される。

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#ifndef SAKURA_CQUICKJSMACROMGR_7B4E9A3B_6A9A_4C4B_9B2C_2C6B9B8B7C3B_H_
#define SAKURA_CQUICKJSMACROMGR_7B4E9A3B_6A9A_4C4B_9B2C_2C6B9B8B7C3B_H_

#ifdef NKMM_FIX_QUICKJS_MACRO

#include <Windows.h>
#include <string>
#include "macro/CMacroManagerBase.h"
#include "macro/CWSHIfObj.h" // CWSHIfObj::List (プラグイン等が追加のIfObjを積むため)

class CEditView;

class CQuickJSMacroMgr : public CMacroManagerBase
{
public:
	CQuickJSMacroMgr();
	virtual ~CQuickJSMacroMgr();

	virtual bool ExecKeyMacro( CEditView* pcEditView, int flags ) const;
	virtual BOOL LoadKeyMacro( HINSTANCE hInstance, const TCHAR* pszPath );
	virtual BOOL LoadKeyMacroStr( HINSTANCE hInstance, const TCHAR* pszCode );

	static CMacroManagerBase* Creator( const TCHAR* FileExt );
	static void declare();

	//	CEditorIfObj以外に、追加のインタフェースオブジェクトを束縛したい場合に使う
	//	(CQuickJSPlugin::InvokePlugがCPluginIfObj等を追加するのに使用)
	void AddParam( CWSHIfObj* param );
	void AddParam( CWSHIfObj::List& params );
	void ClearParam();

protected:
	std::wstring m_Source;
	CWSHIfObj::List m_Params;
};

#endif // NKMM_FIX_QUICKJS_MACRO

#endif /* SAKURA_CQUICKJSMACROMGR_7B4E9A3B_6A9A_4C4B_9B2C_2C6B9B8B7C3B_H_ */
/*[EOF]*/
