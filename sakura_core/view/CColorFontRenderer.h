/*!	@file
	@brief Direct2D/DirectWriteによるカラーグリフ(絵文字等)描画

	既存のGDI描画はそのまま残し、COLR/CPAL形式のカラーフォントのグリフだけを
	追加でオーバーレイ描画するためのヘルパー。d2d1.dll/dwrite.dllは動的ロード
	(LoadLibrary)のみで扱い、静的リンクはしない。そのため、これらのDLLや
	IDWriteFactory2 (Windows 8.1以降相当)が無い環境では EnsureInit() が
	false を返し続け、呼び出し側は何もせず従来通りのGDI描画のみになる。
*/
#ifndef SAKURA_CCOLORFONTRENDERER_9B8B7C1D_5C2A_4A9E_8B7A_2C6B9B8B7C1D_H_
#define SAKURA_CCOLORFONTRENDERER_9B8B7C1D_5C2A_4A9E_8B7A_2C6B9B8B7C1D_H_

#ifdef NKMM_FIX_COLOR_FONT

#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwrite_2.h>	// IDWriteFactory2, IDWriteFontFace2, TranslateColorGlyphRun等
#include <unordered_map>
#include <string>
#include "util/design_template.h"
#include "CColorGlyphCell.h"

/*!	d2d1.dll/dwrite.dllをSystem32からのみロードするための共通ヘルパー。

	既存の CDllImp (extmodule/CDllHandler.h) はDLL探索時に実行ファイルの
	フォルダを優先的に検索する(LoadLibraryExedir)。これはプラグイン用DLL等、
	意図的にexeフォルダを検索したいケースには適切だが、d2d1.dll/dwrite.dllは
	OS標準でSystem32に常駐するシステムDLLであり、この検索順だとポータブル
	配布のexeフォルダに同名の偽DLLを置かれた場合にそちらを読み込んでしまう
	(DLL探索順序ハイジャック)。そのためこれらはLOAD_LIBRARY_SEARCH_SYSTEM32
	を指定し、System32以外を一切検索しない方式でロードする。
*/
class CSystem32Dll {
public:
	CSystem32Dll() : m_hInstance(NULL) {}
	~CSystem32Dll(){ if( m_hInstance ) ::FreeLibrary(m_hInstance); }

protected:
	bool LoadFromSystem32(LPCWSTR pszDllName)
	{
		if( m_hInstance ) return true;
		//LOAD_LIBRARY_SEARCH_SYSTEM32: System32ディレクトリのみを検索する
		//(exeのフォルダ・カレントディレクトリ・PATH等は一切検索しない)
		m_hInstance = ::LoadLibraryExW(pszDllName, NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
		return m_hInstance != NULL;
	}
	FARPROC GetProcAddressChecked(LPCSTR pszProcName) const
	{
		return m_hInstance ? ::GetProcAddress(m_hInstance, pszProcName) : NULL;
	}

private:
	HMODULE m_hInstance;
};

//! d2d1.dll の動的ロード(System32限定)
class CD2D1Dll : public CSystem32Dll {
public:
	bool InitDll()
	{
		if( !LoadFromSystem32(L"d2d1.dll") ) return false;
		m_pfnD2D1CreateFactory = (HRESULT (WINAPI*)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**))
			GetProcAddressChecked("D2D1CreateFactory");
		return NULL != m_pfnD2D1CreateFactory;
	}
	HRESULT (WINAPI* m_pfnD2D1CreateFactory)(D2D1_FACTORY_TYPE, REFIID, CONST D2D1_FACTORY_OPTIONS*, void**) = NULL;
};

//! dwrite.dll の動的ロード(System32限定)
class CDWriteDll : public CSystem32Dll {
public:
	bool InitDll()
	{
		if( !LoadFromSystem32(L"dwrite.dll") ) return false;
		m_pfnDWriteCreateFactory = (HRESULT (WINAPI*)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**))
			GetProcAddressChecked("DWriteCreateFactory");
		return NULL != m_pfnDWriteCreateFactory;
	}
	HRESULT (WINAPI* m_pfnDWriteCreateFactory)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**) = NULL;
};

/*!
	Direct2D/DirectWriteによるカラーグリフ描画のシングルトン。

	- EnsureInit() は初回呼び出し時にのみ実際の初期化を試み、以後は結果をキャッシュする
	  (失敗した環境で LoadLibrary を毎回やり直さない)。
	- HFONTからのIDWriteFontFace2導出結果はHFONTをキーにキャッシュする。
	  CViewFont::GetFontGeneration() の値が変化したらキャッシュ全体を破棄する。
*/
class CColorFontRenderer : public TSingleton<CColorFontRenderer> {
	friend class TSingleton<CColorFontRenderer>;
	CColorFontRenderer();
	~CColorFontRenderer();

public:
	//! 利用可能かどうか(初回呼び出し時に一度だけ初期化を試みる)
	bool EnsureInit();

	/*!	指定フォント・コードポイントのカラーレイヤーを取得する。

		本文フォント(hFont)にそのコードポイントのグリフが無い場合、GDIはSystemLink経由で
		黙って別フォントのグリフを代替描画するが、そのフォントは呼び出し側からは分からず、
		カラーフォントかどうかも判定できない。そのため本メソッドはその場合、
		IDWriteFontFallback::MapCharactersでSakura自身が代替フォントを解決し、
		カラー/白黒を問わずそのフォントのグリフで上書き描画する指示をpOutCellに書き込む
		(pOutCell->bEraseFirstがtrueになる)。

		@param hFont      [in]  描画に使われているHFONT
		@param pData      [in]  グリフに対応する文字データの先頭(サロゲートペア可)
		@param nLength    [in]  pDataの長さ(1または2)
		@param fAdvanceX  [in]  GDIが実測したグリフ送り幅(px)。DirectWrite側の送り幅ではなくこちらを採用する。
		@param pOutCell   [out] layers/nLayerCount/bEraseFirstを書き込む(rcCell/hFont/crFore/crBackは
		                        呼び出し側で設定済みの前提。代替フォントを使う場合はhFontを本メソッド内で書き換える)

		@retval true  描画すべきレイヤーが見つかり、pOutCellに書き込んだ
		@retval false カラーフォントでない・レイヤーが無い・機能が利用不可、のいずれか
	*/
	bool TryGetColorLayers(
		HFONT hFont,
		const wchar_t* pData,
		int nLength,
		float fAdvanceX,
		SColorGlyphCell* pOutCell
	);

	//! キューに溜まった分をまとめて描画する(1visual行につき1回の呼び出しを想定)
	void FlushQueue(HDC hdc, const RECT& rcUnion, const std::vector<SColorGlyphCell>& vCells);

private:
	struct SFontFaceCacheEntry {
		IDWriteFontFace2*	pFontFace2;
		bool				bIsColorFont;
		float				fEmSize;	//!< DWRITE_GLYPH_RUN::fontEmSize (DIP。96dpi前提でpxと同値)
		int					nAscentPx;	//!< ベースライン算出用
	};

	void ClearFontFaceCacheIfStale();
	SFontFaceCacheEntry* GetOrCreateFontFaceEntry(HFONT hFont);
	ID2D1SolidColorBrush* GetOrCreateBrush(COLORREF cr);
	void ClearBrushCache();

	//! m_pDCRenderTargetを(再)作成する。EnsureInit()からの初回作成と、
	//! EndDraw()がD2DERR_RECREATE_TARGET等で失敗した後の再作成の両方で使う。
	bool RecreateRenderTarget();

	//! Direct2D描画専用のオフスクリーンDIBセクション(32bpp)を、少なくとも
	//! 指定サイズ分は確保されている状態にする。
	//!
	//! Sakuraの画面バッファ(m_hdcCompatDC)はCreateCompatibleBitmap由来の
	//! デバイス依存ビットマップ(DDB)であり、Direct2DのBindDCが要求する
	//! GDI相互運用可能なサーフェス(DIBセクション)ではない。DDBへ直接BindDC
	//! すると、BindDC/BeginDraw/EndDraw自体はエラーを返さないにもかかわらず、
	//! 一部の描画コマンドの結果が実際のビットマップへ反映されないことがある
	//! (先頭付近のセルだけ色が乗らない不具合の原因)。
	//! そのためDirect2Dの描画は必ずこの自前のDIBセクションに対して行い、
	//! 結果は通常のBitBltでSakura側のHDCへ転送する。
	bool EnsureOffscreenSurface(int nWidth, int nHeight);

	//! 指定フォントフェイス・グリフのカラーレイヤーをpOutCellへ書き込む(共通処理)
	bool FetchColorLayers(
		IDWriteFontFace2* pFontFace2,
		UINT16 nGlyphIndex,
		float fEmSize,
		float fAdvanceX,
		SColorGlyphCell* pOutCell
	);

	//! 本文フォントに無いコードポイントについて、IDWriteFontFallbackで代替フォントを解決し、
	//! そのフォントに対応するHFONTを返す(既存のGetOrCreateFontFaceEntryキャッシュに載せられるようにするため)。
	//! 代替フォントが見つからない・グリフが無い場合はNULLを返す。
	HFONT ResolveFallbackHFONT(const LOGFONT& lfBase, const wchar_t* pData, int nLength, UINT32 nCodePoint, float fAdvanceX, UINT16* pOutGlyphIndex);

	CD2D1Dll			m_dllD2D1;
	CDWriteDll			m_dllDWrite;
	bool				m_bInitAttempted;
	bool				m_bAvailable;

	ID2D1Factory*			m_pD2DFactory;
	ID2D1DCRenderTarget*	m_pDCRenderTarget;
	IDWriteFactory*			m_pDWriteFactory;
	IDWriteFactory2*		m_pDWriteFactory2;
	IDWriteGdiInterop*		m_pGdiInterop;
	IDWriteFontFallback*	m_pSystemFontFallback;	//!< GDIのSystemLinkに頼らず代替フォントを解決するため

	HDC					m_hdcScratch;	//!< HFONT→IDWriteFontFace変換専用のスクラッチHDC(プロセス寿命)

	HDC					m_hdcOffscreen;		//!< Direct2D描画専用、DIBセクション選択済みのメモリDC
	HBITMAP				m_hbmpOffscreen;	//!< m_hdcOffscreenに選択中の32bpp DIBセクション
	HBITMAP				m_hbmpOffscreenOld;	//!< m_hdcOffscreen作成直後にデフォルトで入っていたビットマップ(破棄前に選択し直す)
	int					m_nOffscreenWidth;
	int					m_nOffscreenHeight;

	ULONG				m_nFontCacheGeneration;
	std::unordered_map<HFONT, SFontFaceCacheEntry>			m_mapFontFaceCache;
	std::unordered_map<COLORREF, ID2D1SolidColorBrush*>	m_mapBrushCache;

	//! ResolveFallbackHFONTで作成したHFONTのキャッシュ(キー: フォント名|高さ|太さ|イタリック)。
	//! プロセス寿命でDeleteObjectする。
	std::unordered_map<std::wstring, HFONT>				m_mapFallbackFontCache;
};

#endif // NKMM_FIX_COLOR_FONT

#endif /* SAKURA_CCOLORFONTRENDERER_9B8B7C1D_5C2A_4A9E_8B7A_2C6B9B8B7C1D_H_ */
/*[EOF]*/
