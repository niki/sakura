/*!	@file
	@brief Lua Script Handler

	@date 2017.5.21
*/
/*
	Copyright (C) 2017, Koma

	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef CLUA_H
#define CLUA_H

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

	static void DoCommand(EFunctionCode nFuncID);
	static bool DoFunction(EFunctionCode nFuncID);

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

#endif /* CLUA_H */
