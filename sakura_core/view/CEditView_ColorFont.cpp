/*!	@file
	@brief カラーフォント(絵文字等)描画キューの管理

	CFigure_Text::DrawImp が1文字GDI描画するたびに TryQueueColorGlyph() を呼び、
	1visual行の描画が完全に終わった直後(選択範囲の反転描画も含む)に
	DrawLayoutLine から FlushColorGlyphQueue() が呼ばれ、たまった分をまとめて
	Direct2D/DirectWriteでオーバーレイ描画する。
*/
#include "StdAfx.h"
#include "CEditView.h"

#ifdef NKMM_FIX_COLOR_FONT

#include "CColorFontRenderer.h"
#include "uiparts/CGraphics.h"
#include "env/CShareData.h"

namespace {

//! ZWJ (Zero Width Joiner)。合字クラスタの結合記号。
bool IsZWJChar(wchar_t wch) { return 0x200D == wch; }
//! VS15(テキストスタイル)/VS16(絵文字スタイル)。単体では新しいクラスタを
//! 開始しないが、直前の要素へ常に結合する。
bool IsVariationSelectorChar(wchar_t wch) { return 0xFE0E == wch || 0xFE0F == wch; }
//! 肌色修飾子(Fitzpatrick modifier)。ZWJを介さず直前の絵文字へ直接結合する。
bool IsFitzpatrickModifier(UINT32 nCodePoint) { return nCodePoint >= 0x1F3FBu && nCodePoint <= 0x1F3FFu; }
//! 結合用囲み記号(U+20E3)。キーキャップ(数字/#/* + VS16 + これ)の最後の要素。
//! ZWJを介さず直前(VS16)へ直接結合する。
bool IsCombiningEnclosingKeycap(wchar_t wch) { return 0x20E3 == wch; }

UINT32 DecodeCodePoint(const wchar_t* pData, int nLength)
{
	if( 2 == nLength ){
		return 0x10000u + ((((UINT32)pData[0] - 0xD800u) << 10) + ((UINT32)pData[1] - 0xDC00u));
	}
	return (UINT32)pData[0];
}

} // namespace

/*!	1文字ぶんの描画待ちをキューへ積む。

	ZWJ結合絵文字(合字)対応のため、直前の文字がZWJだった場合や、自身がZWJ/VS15/
	VS16/肌色修飾子/結合用囲み記号(キーキャップ)の場合は、即座にはm_vPendingColorGlyphsへ積まず、まず
	m_vPendingClusterCallsへ保留する。「合字化できるかもしれない区間」が確定した
	時点(区間が続かない文字が来た時、または行末)でFlushPendingCluster()が
	DirectWriteのシェーピングを試み、単一グリフへ合字化できればまとめて1セルとして、
	できなければ元通り1文字ずつ、m_vPendingColorGlyphsへ積む。
	プレーンテキスト(合字候補でもカラーグリフでもない大多数の文字)は今まで通り
	即座に破棄され、保留バッファには一切触れない(高速パス)。
*/
void CEditView::TryQueueColorGlyph(HFONT hFont, const wchar_t* pData, int nLength, const RECT& rcCell, int nBaselineTopOffset, COLORREF crFore, COLORREF crBack, bool bForceEmojiPresentation)
{
	//GDIが実際に確保したピクセル幅をそのままグリフ送り幅として使う(桁ズレ防止)
	float fAdvanceX = (float)(rcCell.right - rcCell.left);
	if( fAdvanceX <= 0.0f ){
		return;
	}

	UINT32 nCodePoint = DecodeCodePoint(pData, nLength);

	bool bLastWasZWJ = !m_vPendingClusterCalls.empty()
		&& 1 == m_vPendingClusterCalls.back().nLength
		&& IsZWJChar(m_vPendingClusterCalls.back().data[0]);
	bool bContinuesCluster = !m_vPendingClusterCalls.empty()
		&& ( IsZWJChar(pData[0]) || bLastWasZWJ || IsVariationSelectorChar(pData[0]) || IsFitzpatrickModifier(nCodePoint) || IsCombiningEnclosingKeycap(pData[0]) );
	if( !bContinuesCluster ){
		FlushPendingCluster();
	}

	SColorGlyphCell cell;
	cell.rcCell = rcCell;
	cell.nBaselineTopOffset = nBaselineTopOffset;
	cell.crFore = crFore;
	cell.crBack = crBack;
	cell.bEraseFirst = false;
	bool bResolved = CColorFontRenderer::getInstance()->TryGetColorLayers(hFont, pData, nLength, fAdvanceX, bForceEmojiPresentation, &cell);

	//ZWJ/VS15/VS16/肌色修飾子/結合用囲み記号は、それ単体では通常のテキストとして
	//解決される(TryGetColorLayersがfalseを返す)場合でも、合字クラスタの入口/継続
	//候補としての意味を持つため保留バッファに積む。それ以外でTryGetColorLayersが
	//falseなら、今まで通り何もしない。
	bool bClusterCandidate = IsZWJChar(pData[0]) || IsVariationSelectorChar(pData[0]) || IsFitzpatrickModifier(nCodePoint) || IsCombiningEnclosingKeycap(pData[0]);
	if( !bResolved && !bClusterCandidate ){
		return;
	}

	SPendingGlyphCall call;
	call.hFont = hFont;
	call.data[0] = pData[0];
	call.data[1] = (2 == nLength) ? pData[1] : 0;
	call.nLength = nLength;
	call.rcCell = rcCell;
	call.nBaselineTopOffset = nBaselineTopOffset;
	call.crFore = crFore;
	call.crBack = crBack;
	call.bForceEmojiPresentation = bForceEmojiPresentation;
	call.bResolved = bResolved;
	call.resolvedCell = cell;
	m_vPendingClusterCalls.push_back(call);
}

/*!	保留中の合字クラスタ候補区間を確定する。

	要素が1個だけなら合字化の余地が無いので、そのまま(合字化しなかった場合と同じ
	フォールバック経路で)個別に積む。2個以上あれば、フォント/前景色/背景色/
	ベースラインが揃っている場合に限りDirectWriteのシェーピングを試みる
	(CColorFontRenderer::TryShapeCluster)。単一グリフに合字化できればセル矩形の
	和集合(rcUnion)を1セルとしてまとめて積み、できなければ従来通り1文字ずつ
	個別に積む。
*/
void CEditView::FlushPendingCluster()
{
	if( m_vPendingClusterCalls.empty() ){
		return;
	}

	if( 1 < m_vPendingClusterCalls.size() ){
		bool bUniform = true;
		for( size_t i = 1; i < m_vPendingClusterCalls.size(); ++i ){
			if( m_vPendingClusterCalls[i].hFont             != m_vPendingClusterCalls[0].hFont
			 || m_vPendingClusterCalls[i].crFore             != m_vPendingClusterCalls[0].crFore
			 || m_vPendingClusterCalls[i].crBack             != m_vPendingClusterCalls[0].crBack
			 || m_vPendingClusterCalls[i].nBaselineTopOffset != m_vPendingClusterCalls[0].nBaselineTopOffset )
			{
				bUniform = false;
				break;
			}
		}

		//共通設定「全般」タブの「合字」チェックが外れている場合は、合字化を一切
		//試みない(常に1文字ずつのフォールバック描画になる)。
		if( bUniform && GetDllShareData().m_Common.m_sWindow.m_bUseEmojiLigature ){
			wchar_t szText[64];
			int nTextLen = 0;
			RECT rcUnion = m_vPendingClusterCalls[0].rcCell;
			bool bOverflow = false;
			for( size_t i = 0; i < m_vPendingClusterCalls.size(); ++i ){
				const SPendingGlyphCall& c = m_vPendingClusterCalls[i];
				if( nTextLen + c.nLength > (int)(sizeof(szText) / sizeof(szText[0])) ){
					bOverflow = true;	//異常に長いクラスタは諦める(通常あり得ない)
					break;
				}
				szText[nTextLen++] = c.data[0];
				if( 2 == c.nLength ) szText[nTextLen++] = c.data[1];
				rcUnion.left   = t_min(rcUnion.left,   c.rcCell.left);
				rcUnion.top    = t_min(rcUnion.top,    c.rcCell.top);
				rcUnion.right  = t_max(rcUnion.right,  c.rcCell.right);
				rcUnion.bottom = t_max(rcUnion.bottom, c.rcCell.bottom);
			}

			//合字化された絵文字グリフはフォントのem正方形サイズで描画されるため、
			//見た目上ほぼ正方形になる。キーキャップ(数字+VS16+結合用囲み記号)の
			//ように、構成文字が全てBMP内の細い桁ばかりのクラスタでは、各セル幅を
			//単純に合算しただけでは正方形に足りず、右端が描画されずに(FlushQueueの
			//最終BitBltがrcCell外を切り捨てるため)欠けて見える。black cat等、
			//構成要素にサロゲートペア(全角2桁)を含むクラスタは合算幅に余裕がある
			//ためこの問題が起きない。行の高さを下限に、左端を固定したまま右方向へ
			//拡張して正方形を確保する(縦方向はTryShapeCluster側でフォントサイズ
			//そのものを行の高さに収まるよう縮小するため、ここでは広げない)。
			int nRowHeight = rcUnion.bottom - rcUnion.top;
			if( rcUnion.right - rcUnion.left < nRowHeight ){
				rcUnion.right = rcUnion.left + nRowHeight;
			}

			if( !bOverflow ){
				SColorGlyphCell mergedCell;
				mergedCell.rcCell = rcUnion;
				mergedCell.nBaselineTopOffset = m_vPendingClusterCalls[0].nBaselineTopOffset;
				mergedCell.crFore = m_vPendingClusterCalls[0].crFore;
				mergedCell.crBack = m_vPendingClusterCalls[0].crBack;
				mergedCell.bEraseFirst = true;	//個別描画済みの下地をまとめて塗り潰してから描き直す
				float fUnionAdvanceX = (float)(rcUnion.right - rcUnion.left);
				if( CColorFontRenderer::getInstance()->TryShapeCluster(
						m_vPendingClusterCalls[0].hFont, szText, nTextLen, fUnionAdvanceX, (float)nRowHeight, &mergedCell) )
				{
					m_vPendingColorGlyphs.push_back(mergedCell);
					m_vPendingClusterCalls.clear();
					return;
				}
				//合字化できなかった(このフォントに当該組み合わせのGSUB合字が無い)
				//場合は、以下のフォールバックで1文字ずつ積み直す。
			}
		}
	}

	//フォールバック: 保留していた各要素を、今まで通り1文字ずつ個別に積む。
	for( size_t i = 0; i < m_vPendingClusterCalls.size(); ++i ){
		if( m_vPendingClusterCalls[i].bResolved ){
			m_vPendingColorGlyphs.push_back(m_vPendingClusterCalls[i].resolvedCell);
		}
	}
	m_vPendingClusterCalls.clear();
}

void CEditView::FlushColorGlyphQueue(CGraphics& gr)
{
	FlushPendingCluster();
	if( m_vPendingColorGlyphs.empty() ){
		return;
	}

	RECT rcUnion = m_vPendingColorGlyphs[0].rcCell;
	for( size_t i = 1; i < m_vPendingColorGlyphs.size(); ++i ){
		const RECT& r = m_vPendingColorGlyphs[i].rcCell;
		rcUnion.left   = t_min(rcUnion.left,   r.left);
		rcUnion.top    = t_min(rcUnion.top,    r.top);
		rcUnion.right  = t_max(rcUnion.right,  r.right);
		rcUnion.bottom = t_max(rcUnion.bottom, r.bottom);
	}

	CColorFontRenderer::getInstance()->FlushQueue((HDC)gr, rcUnion, m_vPendingColorGlyphs);
	m_vPendingColorGlyphs.clear();
}

#endif // NKMM_FIX_COLOR_FONT
/*[EOF]*/
