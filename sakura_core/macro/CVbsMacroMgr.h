/*!	@file
	@brief VBScript風マクロ(.vbs)エンジン

	WSHのVBScriptエンジンには依存せず、CVbsToJsTranspilerでVBScript風
	ソースをJavaScriptへ変換したうえで、CQuickJSMacroMgrのJS実行パスに
	そのまま流し込む。拡張子".vbs"のマクロファイルを実行する。

	@date 2026 NKMM_FIX_VBS_MACRO
*/
#ifndef SAKURA_CVBSMACROMGR_3A7D5C9E_1F4B_4A2D_9C6E_8B5A3D2F7C1E_H_
#define SAKURA_CVBSMACROMGR_3A7D5C9E_1F4B_4A2D_9C6E_8B5A3D2F7C1E_H_

#ifdef NKMM_FIX_VBS_MACRO

#include "macro/CQuickJSMacroMgr.h"

class CVbsMacroMgr : public CQuickJSMacroMgr
{
public:
	virtual BOOL LoadKeyMacro( HINSTANCE hInstance, const TCHAR* pszPath );
	virtual BOOL LoadKeyMacroStr( HINSTANCE hInstance, const TCHAR* pszCode );

	static CMacroManagerBase* Creator( const TCHAR* FileExt );
	static void declare();
};

#endif // NKMM_FIX_VBS_MACRO

#endif /* SAKURA_CVBSMACROMGR_3A7D5C9E_1F4B_4A2D_9C6E_8B5A3D2F7C1E_H_ */
/*[EOF]*/
