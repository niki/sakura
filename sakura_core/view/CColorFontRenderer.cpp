/*!	@file
	@brief Direct2D/DirectWriteによるカラーグリフ(絵文字等)描画
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_COLOR_FONT

#include "CColorFontRenderer.h"
#include "CViewFont.h"
#include <vector>

namespace {

/*!	不可視の書式文字(バリエーションセレクタ、ZWJ)かどうか。

	VS15(U+FE0E)/VS16(U+FE0F)や ZWJ(U+200D) は本来グリフを持たない結合用の
	制御文字だが、本文フォントにこのコードポイントが無い場合、GDIのSystemLinkが
	「たまたまこの文字を持っている」フォントへ黙って代替描画してしまうことがあり、
	意味の無い四角や記号として見えてしまう(合字対応前でも、この見た目の破綻だけは
	レイアウト(桁の数え方)を一切変えずに直せる)。
	TryGetColorLayers()はこれらのコードポイントを検出したら即座に
	「背景色で塗り潰すだけ(レイヤー無し)」のセルを返し、GDIの信用できない代替描画を
	上書きして消す。
*/
bool IsInvisibleFormatChar(wchar_t wch)
{
	return 0xFE0E == wch	// VS15 (テキストスタイル指定)
		|| 0xFE0F == wch	// VS16 (絵文字スタイル指定)
		|| 0x200D == wch;	// ZWJ (Zero Width Joiner)
}

/*!	IDWriteFontFallback::MapCharacters用の最小限のIDWriteTextAnalysisSource実装。

	1文字(サロゲートペアなら2コードユニット)ぶんのテキストバッファをそのまま返すだけで、
	複数ロケール混在や数字置換等は一切考慮しない。スタック上で使い捨てる想定のため
	参照カウントは行わない。
*/
class CSingleRunTextAnalysisSource : public IDWriteTextAnalysisSource {
public:
	CSingleRunTextAnalysisSource(const wchar_t* pText, UINT32 nLength)
		: m_pText(pText), m_nLength(nLength)
	{
	}

	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if( __uuidof(IUnknown) == riid || __uuidof(IDWriteTextAnalysisSource) == riid ){
			*ppvObject = this;
			return S_OK;
		}
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
	ULONG STDMETHODCALLTYPE Release() override { return 1; }

	//IDWriteTextAnalysisSource
	HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 textPosition, WCHAR const** textString, UINT32* textLength) override
	{
		if( textPosition >= m_nLength ){
			*textString = NULL;
			*textLength = 0;
		}else{
			*textString = m_pText + textPosition;
			*textLength = m_nLength - textPosition;
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 /*textPosition*/, WCHAR const** textString, UINT32* textLength) override
	{
		*textString = NULL;
		*textLength = 0;
		return S_OK;
	}
	DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override
	{
		return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
	}
	HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 textPosition, UINT32* textLength, WCHAR const** localeName) override
	{
		*localeName = L"";
		*textLength = (textPosition < m_nLength) ? (m_nLength - textPosition) : 0;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 textPosition, UINT32* textLength, IDWriteNumberSubstitution** numberSubstitution) override
	{
		*numberSubstitution = NULL;
		*textLength = (textPosition < m_nLength) ? (m_nLength - textPosition) : 0;
		return S_OK;
	}

private:
	const wchar_t*	m_pText;
	UINT32			m_nLength;
};

/*!	IDWriteTextLayout::Draw()から呼ばれる最小限のIDWriteTextRenderer実装。

	TryShapeCluster専用。1クラスタぶんの短いテキストしか渡さないため、
	DrawGlyphRunが複数回呼ばれた(=フォールバックの途中でフォントが切り替わった等で
	ランが分割された)場合や、1回のランのglyphCountが1でない(=このフォントには
	当該コードポイント列に対応するGSUB合字が無く、複数グリフのままだった)場合は
	「合字化失敗」として扱う。下線・取り消し線・インライン object は絵文字クラスタの
	シェーピングには関係しないため無視する。
*/
class CGlyphRunCaptureRenderer : public IDWriteTextRenderer {
public:
	int					nRunCount;
	UINT32				nGlyphCount;
	UINT16				nGlyphIndex;
	IDWriteFontFace*	pFontFace;	//!< AddRef済み。呼び出し側でRelease必須。

	CGlyphRunCaptureRenderer() : nRunCount(0), nGlyphCount(0), nGlyphIndex(0), pFontFace(NULL) {}

	//IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		if( __uuidof(IUnknown) == riid || __uuidof(IDWriteTextRenderer) == riid || __uuidof(IDWritePixelSnapping) == riid ){
			*ppvObject = this;
			return S_OK;
		}
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
	ULONG STDMETHODCALLTYPE Release() override { return 1; }

	//IDWritePixelSnapping
	HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void* /*clientDrawingContext*/, BOOL* isDisabled) override
	{
		*isDisabled = TRUE;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetCurrentTransform(void* /*clientDrawingContext*/, DWRITE_MATRIX* transform) override
	{
		static const DWRITE_MATRIX identity = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f };
		*transform = identity;
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void* /*clientDrawingContext*/, FLOAT* pixelsPerDip) override
	{
		*pixelsPerDip = 1.0f;
		return S_OK;
	}

	//IDWriteTextRenderer
	HRESULT STDMETHODCALLTYPE DrawGlyphRun(
		void* /*clientDrawingContext*/,
		FLOAT /*baselineOriginX*/,
		FLOAT /*baselineOriginY*/,
		DWRITE_MEASURING_MODE /*measuringMode*/,
		DWRITE_GLYPH_RUN const* glyphRun,
		DWRITE_GLYPH_RUN_DESCRIPTION const* /*glyphRunDescription*/,
		IUnknown* /*clientDrawingEffect*/
	) override
	{
		++nRunCount;
		nGlyphCount = glyphRun->glyphCount;
		if( 1 == glyphRun->glyphCount ){
			nGlyphIndex = glyphRun->glyphIndices[0];
		}
		if( !pFontFace && glyphRun->fontFace ){
			pFontFace = glyphRun->fontFace;
			pFontFace->AddRef();
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE DrawUnderline(void*, FLOAT, FLOAT, DWRITE_UNDERLINE const*, IUnknown*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE DrawStrikethrough(void*, FLOAT, FLOAT, DWRITE_STRIKETHROUGH const*, IUnknown*) override { return S_OK; }
	HRESULT STDMETHODCALLTYPE DrawInlineObject(void*, FLOAT, FLOAT, IDWriteInlineObject*, BOOL, BOOL, IUnknown*) override { return S_OK; }
};

} // namespace

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       DLL動的ロード                         //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
// CD2D1Dll::InitDll()/CDWriteDll::InitDll()の実装はCColorFontRenderer.h参照。
// System32からのみロードする(DLL探索順序ハイジャック対策)。


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                    コンストラクタ・破棄                     //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

CColorFontRenderer::CColorFontRenderer()
: m_bInitAttempted(false)
, m_bAvailable(false)
, m_pD2DFactory(NULL)
, m_pDCRenderTarget(NULL)
, m_pDWriteFactory(NULL)
, m_pDWriteFactory2(NULL)
, m_pGdiInterop(NULL)
, m_pSystemFontFallback(NULL)
, m_hdcScratch(NULL)
, m_hdcOffscreen(NULL)
, m_hbmpOffscreen(NULL)
, m_hbmpOffscreenOld(NULL)
, m_nOffscreenWidth(0)
, m_nOffscreenHeight(0)
, m_nFontCacheGeneration(0)
{
}

CColorFontRenderer::~CColorFontRenderer()
{
	ClearBrushCache();
	for( auto& kv : m_mapFontFaceCache ){
		if( kv.second.pFontFace2 ) kv.second.pFontFace2->Release();
	}
	for( auto& kv : m_mapFallbackFontCache ){
		if( kv.second ) ::DeleteObject(kv.second);
	}
	if( m_pSystemFontFallback ) m_pSystemFontFallback->Release();
	if( m_pGdiInterop )     m_pGdiInterop->Release();
	if( m_pDWriteFactory2 ) m_pDWriteFactory2->Release();
	if( m_pDWriteFactory )  m_pDWriteFactory->Release();
	if( m_pDCRenderTarget ) m_pDCRenderTarget->Release();
	if( m_pD2DFactory )     m_pD2DFactory->Release();
	if( m_hdcScratch )      ::DeleteDC(m_hdcScratch);
	if( m_hdcOffscreen ){
		if( m_hbmpOffscreenOld ) ::SelectObject(m_hdcOffscreen, m_hbmpOffscreenOld);
		if( m_hbmpOffscreen )    ::DeleteObject(m_hbmpOffscreen);
		::DeleteDC(m_hdcOffscreen);
	}
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                            初期化                           //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColorFontRenderer::EnsureInit()
{
	if( m_bInitAttempted ){
		return m_bAvailable;
	}
	m_bInitAttempted = true;

	//DLLロード(d2d1.dll/dwrite.dllが無い環境では、ここで失敗して以後は何もしない)
	if( !m_dllD2D1.InitDll() ){
		return false;
	}
	if( !m_dllDWrite.InitDll() ){
		return false;
	}

	HRESULT hr;

	hr = m_dllD2D1.m_pfnD2D1CreateFactory(
		D2D1_FACTORY_TYPE_SINGLE_THREADED,
		__uuidof(ID2D1Factory),
		NULL,
		(void**)&m_pD2DFactory
	);
	if( FAILED(hr) || !m_pD2DFactory ){
		return false;
	}

	hr = m_dllDWrite.m_pfnDWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		(IUnknown**)&m_pDWriteFactory
	);
	if( FAILED(hr) || !m_pDWriteFactory ){
		return false;
	}

	//TranslateColorGlyphRunはIDWriteFactory2にしかない(Windows 8.1以降相当)。
	//無ければ機能全体を無効化する(Windows 7/8では通常ここで失敗する)。
	hr = m_pDWriteFactory->QueryInterface(__uuidof(IDWriteFactory2), (void**)&m_pDWriteFactory2);
	if( FAILED(hr) || !m_pDWriteFactory2 ){
		return false;
	}

	hr = m_pDWriteFactory->GetGdiInterop(&m_pGdiInterop);
	if( FAILED(hr) || !m_pGdiInterop ){
		return false;
	}

	//本文フォントに無いグリフの代替フォント解決に使う。
	//取得に失敗しても致命的ではない(単に代替フォント解決機能だけが無効になる)ので、
	//ここでは失敗してもEnsureInit自体は続行する。
	m_pDWriteFactory2->GetSystemFontFallback(&m_pSystemFontFallback);

	//HFONT→IDWriteFontFace変換専用のスクラッチHDC
	m_hdcScratch = ::CreateCompatibleDC(NULL);
	if( !m_hdcScratch ){
		return false;
	}

	if( !RecreateRenderTarget() ){
		return false;
	}

	m_bAvailable = true;
	return true;
}

bool CColorFontRenderer::RecreateRenderTarget()
{
	if( m_pDCRenderTarget ){
		m_pDCRenderTarget->Release();
		m_pDCRenderTarget = NULL;
	}
	//古いレンダーターゲットに紐付いていたブラシは道連れで無効になるため作り直す
	ClearBrushCache();

	//BindDCで使うDCレンダーターゲット(GDI互換usageが必須)
	D2D1_RENDER_TARGET_PROPERTIES props = {};
	props.type = D2D1_RENDER_TARGET_TYPE_DEFAULT;
	props.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
	props.dpiX = 96.0f;
	props.dpiY = 96.0f;
	props.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
	props.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

	HRESULT hr = m_pD2DFactory->CreateDCRenderTarget(&props, &m_pDCRenderTarget);
	if( FAILED(hr) || !m_pDCRenderTarget ){
		m_pDCRenderTarget = NULL;
		return false;
	}
	return true;
}

bool CColorFontRenderer::EnsureOffscreenSurface(int nWidth, int nHeight)
{
	if( m_hdcOffscreen && nWidth <= m_nOffscreenWidth && nHeight <= m_nOffscreenHeight ){
		return true;
	}

	//既存のDC/ビットマップは作り直す(内容の保持は不要。1セルごとに元のHDCから
	//コピーし直してから描画するため、古い内容が残っていても問題ない)
	if( m_hdcOffscreen ){
		if( m_hbmpOffscreenOld ) ::SelectObject(m_hdcOffscreen, m_hbmpOffscreenOld);
		if( m_hbmpOffscreen )    ::DeleteObject(m_hbmpOffscreen);
		::DeleteDC(m_hdcOffscreen);
		m_hdcOffscreen = NULL;
		m_hbmpOffscreen = NULL;
		m_hbmpOffscreenOld = NULL;
	}

	//再作成の頻度を抑えるため、要求サイズより少し余裕を持って確保する
	int nNewWidth  = t_max(nWidth,  t_max(m_nOffscreenWidth,  512));
	int nNewHeight = t_max(nHeight, t_max(m_nOffscreenHeight, 512));

	m_hdcOffscreen = ::CreateCompatibleDC(NULL);
	if( !m_hdcOffscreen ){
		return false;
	}

	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth       = nNewWidth;
	bmi.bmiHeader.biHeight      = -nNewHeight;	//トップダウンDIB
	bmi.bmiHeader.biPlanes      = 1;
	bmi.bmiHeader.biBitCount    = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	void* pBits = NULL;
	m_hbmpOffscreen = ::CreateDIBSection(m_hdcOffscreen, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
	if( !m_hbmpOffscreen ){
		::DeleteDC(m_hdcOffscreen);
		m_hdcOffscreen = NULL;
		return false;
	}
	m_hbmpOffscreenOld = (HBITMAP)::SelectObject(m_hdcOffscreen, m_hbmpOffscreen);

	m_nOffscreenWidth  = nNewWidth;
	m_nOffscreenHeight = nNewHeight;
	return true;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                    HFONT→フォントフェイス                   //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

void CColorFontRenderer::ClearFontFaceCacheIfStale()
{
	ULONG nCurrent = CViewFont::GetFontGeneration();
	if( nCurrent == m_nFontCacheGeneration ){
		return;
	}
	m_nFontCacheGeneration = nCurrent;
	for( auto& kv : m_mapFontFaceCache ){
		if( kv.second.pFontFace2 ) kv.second.pFontFace2->Release();
	}
	m_mapFontFaceCache.clear();
}


CColorFontRenderer::SFontFaceCacheEntry* CColorFontRenderer::GetOrCreateFontFaceEntry(HFONT hFont)
{
	ClearFontFaceCacheIfStale();

	auto it = m_mapFontFaceCache.find(hFont);
	if( it != m_mapFontFaceCache.end() ){
		return &it->second;
	}

	SFontFaceCacheEntry entry = {};
	entry.pFontFace2 = NULL;
	entry.bIsColorFont = false;
	entry.fEmSize = 0.0f;
	entry.nAscentPx = 0;

	HFONT hOldFont = (HFONT)::SelectObject(m_hdcScratch, hFont);

	IDWriteFontFace* pFontFace = NULL;
	HRESULT hr = m_pGdiInterop->CreateFontFaceFromHdc(m_hdcScratch, &pFontFace);

	TEXTMETRIC tm = {};
	::GetTextMetrics(m_hdcScratch, &tm);

	LOGFONT lf = {};
	::GetObject(hFont, sizeof(LOGFONT), &lf);

	::SelectObject(m_hdcScratch, hOldFont);

	if( SUCCEEDED(hr) && pFontFace ){
		IDWriteFontFace2* pFontFace2 = NULL;
		if( SUCCEEDED(pFontFace->QueryInterface(__uuidof(IDWriteFontFace2), (void**)&pFontFace2)) && pFontFace2 ){
			entry.pFontFace2 = pFontFace2;
			entry.bIsColorFont = (pFontFace2->IsColorFont() != FALSE);
			entry.fEmSize = (float)abs(lf.lfHeight);
			entry.nAscentPx = tm.tmAscent;
		}
		pFontFace->Release();
	}

	auto ret = m_mapFontFaceCache.insert(std::make_pair(hFont, entry));
	return &ret.first->second;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     カラーレイヤー取得                      //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColorFontRenderer::FetchColorLayers(
	IDWriteFontFace2* pFontFace2,
	UINT16 nGlyphIndex,
	float fEmSize,
	float fAdvanceX,
	SColorGlyphCell* pOutCell
)
{
	DWRITE_GLYPH_OFFSET offset = { 0.0f, 0.0f };
	DWRITE_GLYPH_RUN run = {};
	run.fontFace      = pFontFace2;
	run.fontEmSize    = fEmSize;
	run.glyphCount    = 1;
	run.glyphIndices  = &nGlyphIndex;
	run.glyphAdvances = &fAdvanceX;
	run.glyphOffsets  = &offset;
	run.isSideways    = FALSE;
	run.bidiLevel     = 0;

	IDWriteColorGlyphRunEnumerator* pEnumerator = NULL;
	HRESULT hr = m_pDWriteFactory2->TranslateColorGlyphRun(
		0.0f, 0.0f,
		&run,
		NULL,
		DWRITE_MEASURING_MODE_NATURAL,
		NULL,
		0,
		&pEnumerator
	);
	if( DWRITE_E_NOCOLOR == hr || FAILED(hr) || !pEnumerator ){
		//このグリフにはカラーレイヤーが無い
		return false;
	}

	int nLayerCount = 0;
	BOOL bHasRun = FALSE;
	while( nLayerCount < SColorGlyphCell::MAX_LAYERS
		&& SUCCEEDED(pEnumerator->MoveNext(&bHasRun)) && bHasRun )
	{
		const DWRITE_COLOR_GLYPH_RUN* pColorRun = NULL;
		if( FAILED(pEnumerator->GetCurrentRun(&pColorRun)) || !pColorRun ){
			break;
		}
		//1レイヤー=1グリフのみ対応(実用上のCOLR絵文字フォントはこの前提を満たす)
		if( 1 != pColorRun->glyphRun.glyphCount ){
			continue;
		}

		SColorGlyphLayer& layer = pOutCell->layers[nLayerCount];
		layer.nGlyphIndex = pColorRun->glyphRun.glyphIndices[0];
		layer.fAdvanceX   = pColorRun->glyphRun.glyphAdvances ? pColorRun->glyphRun.glyphAdvances[0] : fAdvanceX;
		layer.fOffsetX    = pColorRun->glyphRun.glyphOffsets ? pColorRun->glyphRun.glyphOffsets[0].advanceOffset : 0.0f;
		layer.fOffsetY    = pColorRun->glyphRun.glyphOffsets ? pColorRun->glyphRun.glyphOffsets[0].ascenderOffset : 0.0f;
		if( 0xFFFF == pColorRun->paletteIndex ){
			layer.bUseForegroundColor = true;
		}else{
			layer.bUseForegroundColor = false;
			layer.crColor = RGB(
				(BYTE)(pColorRun->runColor.r * 255.0f + 0.5f),
				(BYTE)(pColorRun->runColor.g * 255.0f + 0.5f),
				(BYTE)(pColorRun->runColor.b * 255.0f + 0.5f)
			);
		}
		++nLayerCount;
	}
	pEnumerator->Release();

	if( 0 == nLayerCount ){
		return false;
	}
	pOutCell->nLayerCount = nLayerCount;
	return true;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//              合字クラスタ(ZWJ結合絵文字等)のシェーピング   //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColorFontRenderer::TryShapeCluster(
	HFONT hFont,
	const wchar_t* pText,
	int nTextLength,
	float fAdvanceX,
	SColorGlyphCell* pOutCell
)
{
	if( !EnsureInit() ){
		return false;
	}

	LOGFONT lfBase = {};
	::GetObject(hFont, sizeof(LOGFONT), &lfBase);
	float fEmSize = (float)abs(lfBase.lfHeight);
	if( fEmSize <= 0.0f ){
		return false;
	}

	DWRITE_FONT_WEIGHT weight = (DWRITE_FONT_WEIGHT)lfBase.lfWeight;
	if( weight < DWRITE_FONT_WEIGHT_THIN ){
		weight = DWRITE_FONT_WEIGHT_NORMAL;
	}
	DWRITE_FONT_STYLE style = lfBase.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

	IDWriteTextFormat* pFormat = NULL;
	HRESULT hr = m_pDWriteFactory->CreateTextFormat(
		lfBase.lfFaceName, NULL, weight, style, DWRITE_FONT_STRETCH_NORMAL, fEmSize, L"ja-jp", &pFormat);
	if( FAILED(hr) || !pFormat ){
		return false;
	}

	//折り返し・複数行を発生させない、十分に大きい仮想レイアウト領域を与える。
	//実際の描画位置/幅は使わず、DrawGlyphRunで得られるグリフインデックスだけを見る。
	IDWriteTextLayout* pLayout = NULL;
	hr = m_pDWriteFactory->CreateTextLayout(pText, (UINT32)nTextLength, pFormat, 10000.0f, 1000.0f, &pLayout);
	pFormat->Release();
	if( FAILED(hr) || !pLayout ){
		return false;
	}

	CGlyphRunCaptureRenderer renderer;
	hr = pLayout->Draw(NULL, &renderer, 0.0f, 0.0f);
	pLayout->Release();

	if( FAILED(hr) || 1 != renderer.nRunCount || 1 != renderer.nGlyphCount || !renderer.pFontFace ){
		//このフォントには当該コードポイント列に対応するGSUB合字が無く複数グリフの
		//ままだった、またはフォントフォールバックの途中で切り替わり複数ランに
		//分かれた。呼び出し側で1文字ずつの描画にフォールバックすること。
		if( renderer.pFontFace ) renderer.pFontFace->Release();
		return false;
	}

	//シェーピングが実際に使ったフォント(hFont自身とは限らない。IDWriteTextLayoutの
	//自動フォントフォールバックにより、本文フォントに無いグリフはSegoe UI Emoji等へ
	//自動的に解決される)をHFONT化し、既存のHFONTキー方式のキャッシュ/描画パイプライン
	//(FlushQueueのGetOrCreateFontFaceEntry)にそのまま乗せる。ResolveFallbackHFONT()と
	//同じ考え方: 同じ物理フォントファイルであれば、別々に取得したIDWriteFontFace
	//インスタンス間でもグリフインデックスの意味は共通。
	LOGFONT lfResolved = {};
	hr = m_pGdiInterop->ConvertFontFaceToLOGFONT(renderer.pFontFace, &lfResolved);
	//lfFaceNameがLF_FACESIZE以内でnull終端されている保証はAPI契約上あるはずだが、
	//念のため明示的に強制する(%sでの書式化時に配列境界を超えて読むことを防ぐ)。
	lfResolved.lfFaceName[LF_FACESIZE - 1] = L'\0';
	if( FAILED(hr) || 0 == lfResolved.lfFaceName[0] ){
		renderer.pFontFace->Release();
		return false;
	}
	lfResolved.lfHeight      = lfBase.lfHeight;
	lfResolved.lfWidth       = 0;
	lfResolved.lfEscapement  = 0;
	lfResolved.lfOrientation = 0;
	lfResolved.lfUnderline   = FALSE;
	lfResolved.lfStrikeOut   = FALSE;

	wchar_t szKey[LF_FACESIZE + 48];
	swprintf_s(szKey, L"%s|%d|%d|%d|%d", lfResolved.lfFaceName, lfResolved.lfHeight, lfResolved.lfWeight, lfResolved.lfItalic, (int)fAdvanceX);

	HFONT hResolvedFont = NULL;
	auto it = m_mapFallbackFontCache.find(szKey);
	if( it != m_mapFallbackFontCache.end() ){
		hResolvedFont = it->second;
	}else{
		hResolvedFont = ::CreateFontIndirect(&lfResolved);
		if( hResolvedFont ){
			m_mapFallbackFontCache.insert(std::make_pair(std::wstring(szKey), hResolvedFont));
		}
	}
	if( !hResolvedFont ){
		renderer.pFontFace->Release();
		return false;
	}

	bool bSuccess = false;
	IDWriteFontFace2* pFontFace2 = NULL;
	if( SUCCEEDED(renderer.pFontFace->QueryInterface(__uuidof(IDWriteFontFace2), (void**)&pFontFace2)) && pFontFace2 ){
		if( pFontFace2->IsColorFont() ){
			bSuccess = FetchColorLayers(pFontFace2, renderer.nGlyphIndex, fEmSize, fAdvanceX, pOutCell);
		}else{
			//白黒(通常の輪郭のみ)の合字グリフ。パレット無しの1レイヤーとして
			//テキスト前景色で描画する。
			SColorGlyphLayer& layer = pOutCell->layers[0];
			layer.nGlyphIndex = renderer.nGlyphIndex;
			layer.fAdvanceX   = fAdvanceX;
			layer.fOffsetX    = 0.0f;
			layer.fOffsetY    = 0.0f;
			layer.bUseForegroundColor = true;
			pOutCell->nLayerCount = 1;
			bSuccess = true;
		}
		pFontFace2->Release();
	}
	renderer.pFontFace->Release();

	if( bSuccess ){
		pOutCell->hFont = hResolvedFont;
	}
	return bSuccess;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                代替フォント解決(GDIのSystemLinkを使わない)  //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

HFONT CColorFontRenderer::ResolveFallbackHFONT(
	const LOGFONT& lfBase,
	const wchar_t* pData,
	int nLength,
	UINT32 nCodePoint,
	float fAdvanceX,
	UINT16* pOutGlyphIndex
)
{
	if( !m_pSystemFontFallback ){
		return NULL;
	}

	CSingleRunTextAnalysisSource analysisSource(pData, (UINT32)nLength);

	DWRITE_FONT_WEIGHT weight = (DWRITE_FONT_WEIGHT)lfBase.lfWeight;
	if( weight < DWRITE_FONT_WEIGHT_THIN ){
		weight = DWRITE_FONT_WEIGHT_NORMAL;
	}
	DWRITE_FONT_STYLE style = lfBase.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

	UINT32 nMappedLength = 0;
	IDWriteFont* pMappedFont = NULL;
	float fScale = 1.0f;
	HRESULT hr = m_pSystemFontFallback->MapCharacters(
		&analysisSource,
		0,
		(UINT32)nLength,
		NULL,				//baseFontCollection(NULLでシステムフォントコレクション)
		lfBase.lfFaceName,
		weight,
		style,
		DWRITE_FONT_STRETCH_NORMAL,
		&nMappedLength,
		&pMappedFont,
		&fScale
	);
	if( FAILED(hr) || !pMappedFont ){
		return NULL;
	}

	//lfHeight/lfWidth等のサイズ系メンバはConvertFontToLOGFONTでは設定されないため、
	//本文フォント側の値をそのまま使う(セルの大きさは常にGDI実測幅を採用するので、
	//フォントの内部的な字形サイズの違いはここでは無視してよい)。
	LOGFONT lfFallback = {};
	BOOL bIsSystemFont = FALSE;
	m_pGdiInterop->ConvertFontToLOGFONT(pMappedFont, &lfFallback, &bIsSystemFont);
	//lfFaceNameがLF_FACESIZE以内でnull終端されている保証はAPI契約上あるはずだが、
	//念のため明示的に強制する(%sでの書式化時に配列境界を超えて読むことを防ぐ)。
	lfFallback.lfFaceName[LF_FACESIZE - 1] = L'\0';

	IDWriteFontFace* pFontFace = NULL;
	hr = pMappedFont->CreateFontFace(&pFontFace);
	pMappedFont->Release();
	if( FAILED(hr) || !pFontFace ){
		return NULL;
	}

	IDWriteFontFace2* pFontFace2 = NULL;
	hr = pFontFace->QueryInterface(__uuidof(IDWriteFontFace2), (void**)&pFontFace2);
	pFontFace->Release();
	if( FAILED(hr) || !pFontFace2 ){
		return NULL;
	}

	UINT16 nGlyphIndex = 0;
	hr = pFontFace2->GetGlyphIndices(&nCodePoint, 1, &nGlyphIndex);
	pFontFace2->Release();
	if( FAILED(hr) || 0 == nGlyphIndex || 0 == lfFallback.lfFaceName[0] ){
		//代替フォントを解決できても、そのフォントにも当該グリフが無ければ諦める
		return NULL;
	}

	//MapCharactersが返すfScaleは、代替フォントの字形サイズを本文フォントの
	//x-height/字面に視覚的に合わせるための倍率。特にSegoe UI Emoji等のカラー
	//絵文字フォントは字面がem枠いっぱいに大きく作られているため、これを掛けずに
	//本文フォントと同じlfHeightで描画すると行の高さからはみ出して上下が
	//欠けて見える。
	lfFallback.lfHeight      = (LONG)(lfBase.lfHeight * fScale);
	if( 0 == lfFallback.lfHeight ){
		lfFallback.lfHeight = lfBase.lfHeight;
	}
	lfFallback.lfWidth       = 0;
	lfFallback.lfEscapement  = 0;
	lfFallback.lfOrientation = 0;
	lfFallback.lfUnderline   = FALSE;
	lfFallback.lfStrikeOut   = FALSE;

	//fScaleはx-height基準の見た目合わせであり、幅がセル(fAdvanceX、GDI実測の
	//桁幅)に収まる保証が無い。絵文字フォントはem枠いっぱいの正方形に近い字面で
	//作られていることが多く、そのまま描くと隣の文字と重なって「崩れて」見える。
	//実測して、セル幅からはみ出す場合はさらに縮小する。
	if( fAdvanceX > 0.0f && 0 != lfFallback.lfHeight ){
		const bool bPositive = (lfFallback.lfHeight >= 0);
		auto fnMeasureWidth = [&](LONG nHeight) -> LONG {
			LOGFONT lfTest = lfFallback;
			lfTest.lfHeight = nHeight;
			HFONT hTest = ::CreateFontIndirect(&lfTest);
			if( !hTest ){
				return -1;
			}
			HFONT hOldTest = (HFONT)::SelectObject(m_hdcScratch, hTest);
			SIZE sz = {};
			::GetTextExtentPoint32W(m_hdcScratch, pData, nLength, &sz);
			::SelectObject(m_hdcScratch, hOldTest);
			::DeleteObject(hTest);
			return sz.cx;
		};

		//フォントサイズと実測幅の関係はヒンティングの影響で単純比例にならない
		//ため、1回の縮小計算では、収まりはするが必要以上に小さくなることが
		//ある。まず比例計算でおおまかに縮小し(収束保証のため2%の余裕を見る)、
		//その後1刻みずつ大きくし直して、セル幅に収まる範囲でギリギリまで
		//戻すことで、余分な余白を削る。
		for( int nTry = 0; nTry < 8; ++nTry ){
			LONG nWidth = fnMeasureWidth(lfFallback.lfHeight);
			if( nWidth < 0 || nWidth <= (LONG)fAdvanceX ){
				break;
			}
			double fShrink = 0.98 * (double)fAdvanceX / (double)nWidth;
			LONG nNewHeight = (LONG)(lfFallback.lfHeight * fShrink);
			if( nNewHeight == lfFallback.lfHeight ){
				//これ以上縮められない(丸めで変化しなくなった)
				nNewHeight += bPositive ? -1 : 1;
			}
			lfFallback.lfHeight = nNewHeight;
			if( 0 == lfFallback.lfHeight ){
				lfFallback.lfHeight = bPositive ? 1 : -1;
				break;
			}
		}

		//収まる状態から1刻みずつ大きくして、まだ収まるうちは採用する
		for( int nGrow = 0; nGrow < 16; ++nGrow ){
			LONG nCandidate = lfFallback.lfHeight + (bPositive ? 1 : -1);
			LONG nWidth = fnMeasureWidth(nCandidate);
			if( nWidth < 0 || nWidth > (LONG)fAdvanceX ){
				break;
			}
			lfFallback.lfHeight = nCandidate;
		}
	}

	wchar_t szKey[LF_FACESIZE + 48];
	swprintf_s(szKey, L"%s|%d|%d|%d|%d", lfFallback.lfFaceName, lfFallback.lfHeight, lfFallback.lfWeight, lfFallback.lfItalic, (int)fAdvanceX);

	*pOutGlyphIndex = nGlyphIndex;

	auto it = m_mapFallbackFontCache.find(szKey);
	if( it != m_mapFallbackFontCache.end() ){
		return it->second;
	}

	HFONT hFallback = ::CreateFontIndirect(&lfFallback);
	if( !hFallback ){
		return NULL;
	}
	m_mapFallbackFontCache.insert(std::make_pair(std::wstring(szKey), hFallback));
	return hFallback;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                     カラーレイヤー取得                      //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColorFontRenderer::TryGetColorLayers(
	HFONT hFont,
	const wchar_t* pData,
	int nLength,
	float fAdvanceX,
	bool bForceEmojiPresentation,
	SColorGlyphCell* pOutCell
)
{
	if( !EnsureInit() ){
		return false;
	}

	if( 1 == nLength && IsInvisibleFormatChar(pData[0]) ){
		//VS15/VS16/ZWJ: GDIの信用できない代替描画を背景色で塗り潰して消すだけ
		//(レイヤー無し)。詳細はIsInvisibleFormatChar()のコメント参照。
		pOutCell->hFont = hFont;
		pOutCell->bEraseFirst = true;
		pOutCell->nLayerCount = 0;
		return true;
	}

	SFontFaceCacheEntry* pEntry = GetOrCreateFontFaceEntry(hFont);
	if( !pEntry->pFontFace2 ){
		return false;
	}

	//コードポイントの組み立て(サロゲートペア対応)
	UINT32 nCodePoint;
	if( 2 == nLength ){
		nCodePoint = 0x10000 + (((UINT32)(pData[0] - 0xD800)) << 10) + (UINT32)(pData[1] - 0xDC00);
	}else{
		nCodePoint = (UINT32)pData[0];
	}

	UINT16 nGlyphIndex = 0;
	pEntry->pFontFace2->GetGlyphIndices(&nCodePoint, 1, &nGlyphIndex);

	if( 0 != nGlyphIndex && pEntry->bIsColorFont ){
		//本文フォント自身がこのグリフを持っていて、かつそれがカラーフォントである場合は
		//GDIの描画結果をそのまま信用してよい(VS16の有無に関わらずカラーで出るはず)。
		if( !FetchColorLayers(pEntry->pFontFace2, nGlyphIndex, pEntry->fEmSize, fAdvanceX, pOutCell) ){
			return false;
		}
		pOutCell->hFont = hFont;
		pOutCell->bEraseFirst = false;
		return true;
	}

	if( 0 != nGlyphIndex && !bForceEmojiPresentation ){
		//本文フォント自身がこのグリフを持っているが、モノクロフォントで、かつVS16による
		//強制指定も無い → 通常のテキストとして描画されるのが正しいので、GDIの描画結果
		//(モノクロ)をそのまま信用して何もしない。
		return false;
	}

	//ここに来るのは (a) 本文フォントにこのグリフが無い、または
	//(b) 本文フォントは持っているがモノクロで、直後のVS16が「カラーで出してくれ」と
	//明示指定している(bForceEmojiPresentation)、のいずれか。
	//どちらの場合もGDIが実際に描いたグリフ(本文フォントのモノクロ、またはSystemLink経由の
	//代わりの何か)は信用できないため、IDWriteFontFallbackで代替フォントを自前で解決し、
	//そのフォントで確実に描画し直す。
	//本文フォントにこのグリフが無い → GDIはSystemLinkで何らかのフォントへ黙って
	//代替描画しているはずだが、それが何のフォントかはここからは分からず信用できない
	//(欠け・色付き絵文字なのに白黒でしか出ない等の原因)。
	//IDWriteFontFallbackで代替フォントを自前で解決し、そのフォントで確実に描画し直す。
	LOGFONT lfBase = {};
	::GetObject(hFont, sizeof(LOGFONT), &lfBase);

	UINT16 nFallbackGlyphIndex = 0;
	HFONT hFallbackFont = ResolveFallbackHFONT(lfBase, pData, nLength, nCodePoint, fAdvanceX, &nFallbackGlyphIndex);
	if( !hFallbackFont ){
		//代替フォントも見つからない → 手の施しようがない(GDIの描画結果がそのまま残る)
		wchar_t szLog[96];
		swprintf_s(szLog, L"[ColorFont] U+%04X ResolveFallbackHFONT failed\n", nCodePoint);
		::OutputDebugStringW(szLog);
		return false;
	}

	SFontFaceCacheEntry* pFallbackEntry = GetOrCreateFontFaceEntry(hFallbackFont);
	{
		wchar_t szLog[160];
		swprintf_s(szLog, L"[ColorFont] U+%04X fallbackHFONT=0x%p pFontFace2=0x%p isColor=%d\n",
			nCodePoint, (void*)hFallbackFont, (void*)pFallbackEntry->pFontFace2, (int)pFallbackEntry->bIsColorFont);
		::OutputDebugStringW(szLog);
	}
	if( !pFallbackEntry->pFontFace2 ){
		return false;
	}

	if( pFallbackEntry->bIsColorFont ){
		if( !FetchColorLayers(pFallbackEntry->pFontFace2, nFallbackGlyphIndex, pFallbackEntry->fEmSize, fAdvanceX, pOutCell) ){
			wchar_t szLog[96];
			swprintf_s(szLog, L"[ColorFont] U+%04X FetchColorLayers failed (no color layers)\n", nCodePoint);
			::OutputDebugStringW(szLog);
			return false;
		}
		{
			wchar_t szLog[96];
			swprintf_s(szLog, L"[ColorFont] U+%04X color layers=%d queued\n", nCodePoint, pOutCell->nLayerCount);
			::OutputDebugStringW(szLog);
		}
	}else{
		//白黒(通常の輪郭のみ)の代替フォント。パレット無しの1レイヤーとして
		//テキスト前景色で描画する。
		SColorGlyphLayer& layer = pOutCell->layers[0];
		layer.nGlyphIndex = nFallbackGlyphIndex;
		layer.fAdvanceX   = fAdvanceX;
		layer.fOffsetX    = 0.0f;
		layer.fOffsetY    = 0.0f;
		layer.bUseForegroundColor = true;
		pOutCell->nLayerCount = 1;
	}

	pOutCell->hFont = hFallbackFont;
	pOutCell->bEraseFirst = true;
	return true;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                       ブラシキャッシュ                      //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

void CColorFontRenderer::ClearBrushCache()
{
	for( auto& kv : m_mapBrushCache ){
		if( kv.second ) kv.second->Release();
	}
	m_mapBrushCache.clear();
}

ID2D1SolidColorBrush* CColorFontRenderer::GetOrCreateBrush(COLORREF cr)
{
	auto it = m_mapBrushCache.find(cr);
	if( it != m_mapBrushCache.end() ){
		return it->second;
	}
	//実用上、絵文字パレットの色数は少ないため簡易的な上限で丸ごとクリアする
	if( m_mapBrushCache.size() >= 64 ){
		ClearBrushCache();
	}
	D2D1_COLOR_F color = D2D1::ColorF(
		GetRValue(cr) / 255.0f,
		GetGValue(cr) / 255.0f,
		GetBValue(cr) / 255.0f,
		1.0f
	);
	ID2D1SolidColorBrush* pBrush = NULL;
	if( FAILED(m_pDCRenderTarget->CreateSolidColorBrush(color, &pBrush)) ){
		return NULL;
	}
	m_mapBrushCache.insert(std::make_pair(cr, pBrush));
	return pBrush;
}


// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                            描画                             //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

void CColorFontRenderer::FlushQueue(HDC hdc, const RECT& rcUnion, const std::vector<SColorGlyphCell>& vCells)
{
	if( vCells.empty() || !m_bAvailable ){
		return;
	}

	int nWidth  = rcUnion.right  - rcUnion.left;
	int nHeight = rcUnion.bottom - rcUnion.top;
	if( nWidth <= 0 || nHeight <= 0 ){
		return;
	}

	//Sakuraの画面バッファ(m_hdcCompatDC)はCreateCompatibleBitmap由来のDDBであり、
	//Direct2DのBindDCが期待するGDI相互運用可能なサーフェスではない。DDBへ直接
	//BindDCすると、BindDC/BeginDraw/EndDraw自体はエラーを返さないのに一部の
	//描画コマンドの結果が実際のビットマップへ反映されないことがある(先頭付近の
	//セルだけ色が乗らない不具合の原因だった)。そのためDirect2Dの描画は必ず
	//自前のDIBセクション(m_hdcOffscreen)に対して行い、結果を通常のBitBltで
	//hdcへ転送する。
	if( !EnsureOffscreenSurface(nWidth, nHeight) ){
		wchar_t szLog[96];
		swprintf_s(szLog, L"[ColorFont] EnsureOffscreenSurface failed, skip flush\n");
		::OutputDebugStringW(szLog);
		return;
	}

	if( !m_pDCRenderTarget ){
		//前回のEndDraw()がD2DERR_RECREATE_TARGET等で失敗し、レンダーターゲットを
		//手放した状態。作り直せなければ今回は諦める(GDI描画がそのまま残る)。
		if( !RecreateRenderTarget() ){
			wchar_t szLog[128];
			swprintf_s(szLog, L"[ColorFont] RecreateRenderTarget failed, skip flush\n");
			::OutputDebugStringW(szLog);
			return;
		}
	}

	//GDIの描画はバッチされ、GdiFlush()を呼ぶまで実際のビットマップに反映されない
	//ことがある。これから行うBitBlt(hdc→オフスクリーン)がGDIの最新の描画結果を
	//確実に読めるよう、先にflushしておく。
	::GdiFlush();

	//セルごとに、hdc上の既存の内容(GDIが代替フォントで描いた、信用できないかも
	//しれないグリフ)をオフスクリーンDIBへコピーしておく。bEraseFirstがfalseの
	//セル(本文フォント自身がカラーフォントだった場合)は、この上にカラー
	//レイヤーだけを重ね描きする。
	for( size_t i = 0; i < vCells.size(); ++i ){
		const SColorGlyphCell& cell = vCells[i];
		int w = cell.rcCell.right - cell.rcCell.left;
		int h = cell.rcCell.bottom - cell.rcCell.top;
		if( w <= 0 || h <= 0 ){
			continue;
		}
		::BitBlt(
			m_hdcOffscreen,
			cell.rcCell.left - rcUnion.left, cell.rcCell.top - rcUnion.top,
			w, h,
			hdc,
			cell.rcCell.left, cell.rcCell.top,
			SRCCOPY
		);
	}

	RECT rcSurface = { 0, 0, nWidth, nHeight };
	HRESULT hrBind = m_pDCRenderTarget->BindDC(m_hdcOffscreen, &rcSurface);
	{
		wchar_t szLog[192];
		swprintf_s(szLog, L"[ColorFont] FlushQueue cells=%zu rcUnion=(%d,%d,%d,%d) BindDC=0x%08X\n",
			vCells.size(), rcUnion.left, rcUnion.top, rcUnion.right, rcUnion.bottom, (unsigned)hrBind);
		::OutputDebugStringW(szLog);
	}
	if( SUCCEEDED(hrBind) ){
		m_pDCRenderTarget->BeginDraw();

		for( size_t i = 0; i < vCells.size(); ++i ){
			const SColorGlyphCell& cell = vCells[i];
			SFontFaceCacheEntry* pEntry = GetOrCreateFontFaceEntry(cell.hFont);
			if( !pEntry->pFontFace2 ){
				continue;
			}
			float fLocalLeft = (float)(cell.rcCell.left - rcUnion.left);
			float fLocalTop  = (float)(cell.rcCell.top  - rcUnion.top);
			float fBaselineX = fLocalLeft;
			float fBaselineY = fLocalTop + (float)cell.nBaselineTopOffset + (float)pEntry->nAscentPx;

			if( cell.bEraseFirst ){
				//GDIが代替フォントで描画した(信用できない)グリフを塗り潰してから描き直す
				ID2D1SolidColorBrush* pBgBrush = GetOrCreateBrush(cell.crBack);
				if( pBgBrush ){
					D2D1_RECT_F rcErase = D2D1::RectF(
						fLocalLeft, fLocalTop,
						(float)(cell.rcCell.right - rcUnion.left), (float)(cell.rcCell.bottom - rcUnion.top)
					);
					m_pDCRenderTarget->FillRectangle(rcErase, pBgBrush);
				}
			}

			for( int nLayer = 0; nLayer < cell.nLayerCount; ++nLayer ){
				const SColorGlyphLayer& layer = cell.layers[nLayer];

				COLORREF crFore = layer.bUseForegroundColor
					? cell.crFore
					: layer.crColor;
				ID2D1SolidColorBrush* pBrush = GetOrCreateBrush(crFore);
				if( !pBrush ){
					continue;
				}

				UINT16 nGlyphIndex = layer.nGlyphIndex;
				float fAdvance = layer.fAdvanceX;
				DWRITE_GLYPH_OFFSET offset = { layer.fOffsetX, layer.fOffsetY };
				DWRITE_GLYPH_RUN run = {};
				run.fontFace      = pEntry->pFontFace2;
				run.fontEmSize    = pEntry->fEmSize;
				run.glyphCount    = 1;
				run.glyphIndices  = &nGlyphIndex;
				run.glyphAdvances = &fAdvance;
				run.glyphOffsets  = &offset;
				run.isSideways    = FALSE;
				run.bidiLevel     = 0;

				D2D1_POINT_2F ptBaseline = { fBaselineX, fBaselineY };
				m_pDCRenderTarget->DrawGlyphRun(ptBaseline, &run, pBrush, DWRITE_MEASURING_MODE_NATURAL);
			}
		}

		HRESULT hrEnd = m_pDCRenderTarget->EndDraw();
		{
			wchar_t szLog[96];
			swprintf_s(szLog, L"[ColorFont] EndDraw=0x%08X\n", (unsigned)hrEnd);
			::OutputDebugStringW(szLog);
		}
		if( FAILED(hrEnd) ){
			//D2DERR_RECREATE_TARGET等。レンダーターゲットを手放し、次回Flush時に作り直す。
			m_pDCRenderTarget->Release();
			m_pDCRenderTarget = NULL;
			ClearBrushCache();
		}

		//Direct2Dが自前のDIBセクションへ書いた内容を、この後のBitBlt(GDI)から
		//確実に読めるようにする。
		::GdiFlush();

		//結果をセルごとに実際のhdcへ転送する
		for( size_t i = 0; i < vCells.size(); ++i ){
			const SColorGlyphCell& cell = vCells[i];
			int w = cell.rcCell.right - cell.rcCell.left;
			int h = cell.rcCell.bottom - cell.rcCell.top;
			if( w <= 0 || h <= 0 ){
				continue;
			}
			::BitBlt(
				hdc,
				cell.rcCell.left, cell.rcCell.top,
				w, h,
				m_hdcOffscreen,
				cell.rcCell.left - rcUnion.left, cell.rcCell.top - rcUnion.top,
				SRCCOPY
			);
		}
	}
}

#endif // NKMM_FIX_COLOR_FONT
/*[EOF]*/
