/*!	@file
	@brief カラーグリフ(絵文字等)描画待ちキューのPOD定義

	CColorFontRenderer (Direct2D/DirectWrite) の描画待ちキューに使う値型。
	CEditView.h や CFigureStrategy.cpp から軽量に参照できるよう、
	d2d1.h/dwrite.h には一切依存させない。
*/
#ifndef SAKURA_CCOLORGLYPHCELL_3F9B9E9B_7B6B_4B7B_9C9E_1B7B9E9B7B9E_H_
#define SAKURA_CCOLORGLYPHCELL_3F9B9E9B_7B6B_4B7B_9C9E_1B7B9E9B7B9E_H_

#include <Windows.h>

#ifdef NKMM_FIX_COLOR_FONT

//! カラーグリフ1レイヤー分の情報 (IDWriteColorGlyphRunEnumerator の1要素に対応)
struct SColorGlyphLayer {
	UINT16		nGlyphIndex;
	float		fAdvanceX;				//!< グリフ送り幅(DIP)。GDI実測幅から算出した値を使う。
	float		fOffsetX;
	float		fOffsetY;
	COLORREF	crColor;				//!< bUseForegroundColorがtrueのときは無視
	bool		bUseForegroundColor;	//!< パレットインデックスが0xFFFF(前景色を使う)のとき true
};

//! カラーグリフ1文字(1セル)分の描画待ち情報
struct SColorGlyphCell {
	static const int MAX_LAYERS = 8;

	RECT				rcCell;			//!< GDIの描画クリップ矩形(クライアント座標。行スロット全体で、行間隔等を含む)
	HFONT				hFont;			//!< このセルの描画に使われたHFONT
	int					nLayerCount;
	SColorGlyphLayer	layers[MAX_LAYERS];

	//! rcCell.topから実際にGDIがグリフを描画したY位置までのオフセット(px)。
	//! DispTextはrcCell.top(=行クリップ矩形の上端)そのものではなく、
	//! GetLineMargin()(行間マージン)とGetCharHeightMarginByFontNo()(フォント別の
	//! ベースライン調整量)を加えた位置にグリフを描画するため、上書き描画の
	//! ベースライン計算にはこのオフセットを加算する必要がある。
	int					nBaselineTopOffset;

	//! このセルをGDIで描画した瞬間のテキスト前景色(bUseForegroundColorレイヤー用)。
	//! 1visual行分をまとめてFlushする時点のHDCの状態は最後に描画した文字のものに
	//! なってしまっている(選択反転等で行内でも変化しうる)ため、キュー積み時点で
	//! 個別に保持しておく。
	COLORREF			crFore;

	//! true: 本文フォントにグリフが無く、GDIがSystemLinkで代替描画した(かもしれない)
	//! グリフが信用できないため、描画前にrcCellをcrBackで塗り潰してから上書きする。
	bool				bEraseFirst;
	COLORREF			crBack;			//!< bEraseFirstがtrueのときにrcCellを塗り潰す背景色
};

#endif // NKMM_FIX_COLOR_FONT

#endif /* SAKURA_CCOLORGLYPHCELL_3F9B9E9B_7B6B_4B7B_9C9E_1B7B9E9B7B9E_H_ */
/*[EOF]*/
