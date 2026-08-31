#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CColor_Found.h"
#include "types/CTypeSupport.h"
#include "view/CViewSelect.h"
#include <limits.h>


void CColor_Select::OnStartScanLogic()
{
	m_nSelectLine	= CLayoutInt(-1);
	m_nSelectStart	= CLogicInt(-1);
	m_nSelectEnd	= CLogicInt(-1);
#ifdef NKMM_MULTI_CURSOR
	m_vExtraSelectCache.clear();
#endif // NKMM_
}

bool CColor_Select::BeginColor(const CStringRef& cStr, int nPos)
{
	assert(0);
	return false;
}

bool CColor_Select::BeginColorEx(const CStringRef& cStr, int nPos, CLayoutInt nLineNum, const CLayout* pcLayout)
{

	if(!cStr.IsValid())return false;

	const CEditView& view = *(CColorStrategyPool::getInstance()->GetCurrentView());
	if( !view.GetSelectionInfo().IsTextSelected() || !CTypeSupport(&view,COLORIDX_SELECT).IsDisp() ){
		return false;
	}

	// 2011.12.27 レイアウト行頭で1回だけ確認してあとはメンバー変数をみる
	if( m_nSelectLine != nLineNum ){
		m_nSelectLine = nLineNum;
		CLayoutRange selectArea = view.GetSelectionInfo().GetSelectAreaLine(nLineNum, pcLayout);
		CLayoutInt nSelectFrom = selectArea.GetFrom().x;
		CLayoutInt nSelectTo = selectArea.GetTo().x;
		if( nSelectFrom == nSelectTo || -1 == nSelectFrom ){
			m_nSelectStart = -1;
			m_nSelectEnd = -1;
		}
		else{
			m_nSelectStart = view.LineColumnToIndex(pcLayout, nSelectFrom) + pcLayout->GetLogicOffset();
			m_nSelectEnd = view.LineColumnToIndex(pcLayout, nSelectTo) + pcLayout->GetLogicOffset();
		}
#ifdef NKMM_MULTI_CURSOR
		// マルチカーソル: 追加カーソルの選択範囲も、行頭で1回だけプライマリと同じ考え方で
		// 算出してキャッシュする。各extraはプライマリの選択範囲(m_sSelect)をnRelLine/
		// nRelColumnだけ平行移動しただけなので、キャレット追従(ResolveExtraCursor)と
		// 同じ理屈でShift+方向キー等のプライマリの選択操作にそのまま追従する 20260901
		m_vExtraSelectCache.clear();
		m_vExtraSelectCache.reserve(view.m_vExtraCursors.size());
		for( const auto& extra : view.m_vExtraCursors ){
			SExtraSelectRange range{ CLogicInt(-1), CLogicInt(-1) };
			if( pcLayout ){
				CLayoutRange extraArea = view.ResolveExtraCursorSelectAreaLine(extra, nLineNum);
				CLayoutInt nFrom = extraArea.GetFrom().x;
				CLayoutInt nTo = extraArea.GetTo().x;
				if( nFrom != nTo && nFrom != CLayoutInt(-1) ){
					range.nStart = view.LineColumnToIndex(pcLayout, nFrom) + pcLayout->GetLogicOffset();
					range.nEnd = view.LineColumnToIndex(pcLayout, nTo) + pcLayout->GetLogicOffset();
				}
			}
			m_vExtraSelectCache.push_back(range);
		}
#endif // NKMM_
	}
	if( m_nSelectStart <= nPos && nPos < m_nSelectEnd ){
		return true;
	}
#ifdef NKMM_MULTI_CURSOR
	for( const auto& range : m_vExtraSelectCache ){
		if( range.nStart <= nPos && nPos < range.nEnd ){
			return true;
		}
	}
#endif // NKMM_
	return false;
}

bool CColor_Select::EndColor(const CStringRef& cStr, int nPos)
{
	//マッチ文字列終了検出
	if( m_nSelectEnd <= nPos ){
		// -- -- マッチ文字列を描画 -- -- //

		return true;
	}

	return false;
}


CColor_Found::CColor_Found()
: validColorNum( 0 )
{}

void CColor_Found::OnStartScanLogic()
{
	m_nSearchResult	= 1;
	m_nSearchStart	= CLogicInt(-1);
	m_nSearchEnd	= CLogicInt(-1);

	this->validColorNum = 0;
	for( int color = COLORIDX_SEARCH; color <= COLORIDX_SEARCHTAIL; ++color ) {
		if( m_pTypeData->m_ColorInfoArr[ color ].m_bDisp ) {
			this->highlightColors[ this->validColorNum++ ] = EColorIndexType( color );
		}
	}
}

bool CColor_Found::BeginColor(const CStringRef& cStr, int nPos)
{
	if(!cStr.IsValid())return false;
	const CEditView* pcView = CColorStrategyPool::getInstance()->GetCurrentView();
	if( !pcView->m_bCurSrchKeyMark || 0 == this->validColorNum ){
		return false;
	}

	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	//        検索ヒットフラグ設定 -> bSearchStringMode            //
	// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
	// 2002.02.08 hor 正規表現の検索文字列マークを少し高速化
	if( pcView->m_sCurSearchOption.bWordOnly || (m_nSearchResult && m_nSearchStart < nPos) ){
		m_nSearchResult = pcView->IsSearchString(
			cStr,
			CLogicInt(nPos),
			&m_nSearchStart,
			&m_nSearchEnd
		);
	}
	//マッチ文字列検出
	if( m_nSearchResult && m_nSearchStart==nPos){
		return true;
	}
	return false;
}

bool CColor_Found::EndColor(const CStringRef& cStr, int nPos)
{
	//マッチ文字列終了検出
	if( m_nSearchEnd <= nPos ){ //+ == では行頭文字の場合、m_nSearchEndも０であるために文字色の解除ができないバグを修正 2003.05.03 かろと
		// -- -- マッチ文字列を描画 -- -- //

		return true;
	}

	return false;
}

