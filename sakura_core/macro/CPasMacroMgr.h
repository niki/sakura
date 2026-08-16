/*!	@file
	@brief Pascal風マクロ(.pas)エンジン

	PPA.DLL(外部コンポーネント、ソース非公開)には依存せず、CPasToJsTranspilerで
	Pascal風ソースをJavaScriptへ変換したうえで、CQuickJSMacroMgrのJS実行パスに
	そのまま流し込む。拡張子".pas"のマクロファイルを実行する。

	@date 2026 NKMM_FIX_PASCAL_MACRO
*/
#ifndef SAKURA_CPASMACROMGR_5E8C1A4B_2D7F_4B3E_8A9C_4B7D3E2F1A6C_H_
#define SAKURA_CPASMACROMGR_5E8C1A4B_2D7F_4B3E_8A9C_4B7D3E2F1A6C_H_

#ifdef NKMM_FIX_PASCAL_MACRO

#include "macro/CQuickJSMacroMgr.h"

class CPasMacroMgr : public CQuickJSMacroMgr
{
public:
	virtual BOOL LoadKeyMacro( HINSTANCE hInstance, const TCHAR* pszPath );
	virtual BOOL LoadKeyMacroStr( HINSTANCE hInstance, const TCHAR* pszCode );

	static CMacroManagerBase* Creator( const TCHAR* FileExt );
	static void declare();
};

#endif // NKMM_FIX_PASCAL_MACRO

#endif /* SAKURA_CPASMACROMGR_5E8C1A4B_2D7F_4B3E_8A9C_4B7D3E2F1A6C_H_ */
/*[EOF]*/
