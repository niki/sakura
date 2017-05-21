/*!	@file
	@brief Luaマクロ

	@author 
	@date 2017.5.21
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2017, 

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef _CLUAMACROMGR_H_
#define _CLUAMACROMGR_H_

#include <Windows.h>
#include "CKeyMacroMgr.h"
#include "CLua.h"

/*-----------------------------------------------------------------------
クラスの宣言
-----------------------------------------------------------------------*/
//! Luaマクロ
class CLuaMacroMgr: public CMacroManagerBase
{
public:
	/*
	||  Constructors
	*/
	CLuaMacroMgr();
	~CLuaMacroMgr();

	/*
	||	Luaに委譲する部分
	*/
	virtual bool ExecKeyMacro( class CEditView* pcEditView, int flags ) const;	/* Luaマクロの実行 */
	virtual BOOL LoadKeyMacro( HINSTANCE hInstance, const TCHAR* pszPath);		/* キーボードマクロをファイルから読み込み、CMacroの列に変換 */
	virtual BOOL LoadKeyMacroStr( HINSTANCE hInstance, const TCHAR* pszCode);	/* キーボードマクロを文字列から読み込み、CMacroの列に変換 */

	static class CLua m_cLua;

	// Apr. 29, 2002 genta
	static CMacroManagerBase* Creator(const TCHAR* ext);
	static void declare(void);

protected:
	CNativeW m_cBuffer;
};



///////////////////////////////////////////////////////////////////////
#endif /* _CLUAMACROMGR_H_ */



	
