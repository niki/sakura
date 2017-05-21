/*!	@file
	@brief Lua Script Handler

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

#ifndef _CLUA_H_
#define _CLUA_H_

#include <stdio.h>
#include "macro/CSMacroMgr.h"

/*!
	@brief 
*/
class CLua {
public:
	CLua();
	virtual ~CLua();

public:
	bool Execute(class CEditView* pcEditView, int flags, const CNativeW &buffer);

private:
	struct LuaExecInfo {
		CNativeA      m_cMemRet;		//!< コールバックからDLLに渡す文字列を保持
		CEditView     *m_pcEditView;	//	2003.06.01 Moca
		DLLSHAREDATA  *m_pShareData;	//	2003.06.01 Moca
		bool          m_bError;		//!< エラーが2回表示されるのを防ぐ	2003.06.01 Moca
		CNativeA      m_cMemDebug;	//!< デバッグ用変数UserErrorMes 2003.06.01 Moca
		/** オプションフラグ
		
			CEditView::HandleCommand()にコマンドと一緒に渡すことで
			コマンドの素性を教える．
		*/
		int m_commandflags;
	};
	static LuaExecInfo *m_CurInstance;
	static bool m_bIsRunning;
};

#endif
