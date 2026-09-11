/*
	Copyright (C) 2026, Yu-zuki.

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
#include "docplus/CFoldManager.h"
#include "doc/logic/CDocLineMgr.h"
#include "doc/logic/CDocLine.h"

#ifdef NKMM_CODE_FOLDING

bool CFoldManager::GetLineFoldable(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetFoldable();
}
void CFoldManager::SetLineFoldable(CDocLine* pcDocLine, bool bFlag)
{
	pcDocLine->m_sMark.m_cFolded.SetFoldable(bFlag);
}
bool CFoldManager::GetLineFolded(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetFolded();
}
void CFoldManager::SetLineFolded(CDocLine* pcDocLine, bool bFlag)
{
	pcDocLine->m_sMark.m_cFolded.SetFolded(bFlag);
}
bool CFoldManager::GetLineFoldHidden(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetHidden();
}
void CFoldManager::SetLineFoldHidden(CDocLine* pcDocLine, bool bFlag)
{
	pcDocLine->m_sMark.m_cFolded.SetHidden(bFlag);
}
int CFoldManager::GetLineFoldEndLine(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetFoldEndLine();
}
void CFoldManager::SetLineFoldEndLine(CDocLine* pcDocLine, int nEndLine)
{
	pcDocLine->m_sMark.m_cFolded.SetFoldEndLine(nEndLine);
}
int CFoldManager::GetLineFoldNameCol(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetNameCol();
}
void CFoldManager::SetLineFoldNameCol(CDocLine* pcDocLine, int nCol)
{
	pcDocLine->m_sMark.m_cFolded.SetNameCol(nCol);
}
int CFoldManager::GetLineFoldNameLen(const CDocLine* pcDocLine) const
{
	return pcDocLine->m_sMark.m_cFolded.GetNameLen();
}
void CFoldManager::SetLineFoldNameLen(CDocLine* pcDocLine, int nLen)
{
	pcDocLine->m_sMark.m_cFolded.SetNameLen(nLen);
}

/* 折りたたみ情報をすべてリセット */
void CFoldManager::ResetAllFoldMark(CDocLineMgr* pcDocLineMgr)
{
	CDocLine* pDocLine = pcDocLineMgr->GetDocLineTop();
	while( pDocLine ){
		CDocLine* pDocLineNext = pDocLine->GetNextLine();
		SetLineFoldable(pDocLine, false);
		SetLineFolded(pDocLine, false);
		SetLineFoldHidden(pDocLine, false);
		SetLineFoldEndLine(pDocLine, -1);
		SetLineFoldNameCol(pDocLine, -1);
		SetLineFoldNameLen(pDocLine, 0);
		pDocLine = pDocLineNext;
	}
}

#endif // NKMM_
