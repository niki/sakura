/*!	@file
	@brief GDIによるグリフキャッシュ(グリフアトラス)

	(フォント,文字,前景色,背景色)をキーに、ExtTextOutの描画結果を
	ビットマップとしてキャッシュし、以後はBitBltで再利用する。

	DispTextは常にETO_OPAQUEで背景も同時描画するため、ClearTypeの
	サブピクセルレンダリング結果は(フォント,文字,fg,bg)の組だけで
	完全に決まる。そのためこの方式はClearTypeの見た目を一切損なわない。

	詳細はchangelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md、
	changelog/NKMM_FIX_GLYPH_ATLAS_CACHE_IMPL.md参照。
*/
#ifndef SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_
#define SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_

#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE

#include <unordered_map>
#include <vector>
#include "util/design_template.h"

//! グリフアトラスキャッシュのキー: (フォント,文字,前景色,背景色,セル幅,セル高さ)の組で一意
//! セル幅・高さも含めるのは、同じ(フォント,文字,fg,bg)でも呼び出し元のDx配列(字送り)
//! 計算の都合で異なる幅を要求してくることがあるため。幅を含めないと、キャッシュヒット時に
//! 保存済みサイズでBitBltしてしまい、今回要求されたセルより小さいと右端に前フレームの
//! 内容が塗り残る(「ゴミ」)。フォントサイズを大きくすると誤差が拡大されて可視化しやすい。
//! 20260802 バグ修正でセル幅・高さをキーに追加
struct SGlyphAtlasKey {
	HFONT     hFont;
	wchar_t   wch0;      //!< 1文字目(サロゲート上位、または単一文字)
	wchar_t   wch1;      //!< サロゲート下位。単一コード単位のときは 0
	COLORREF  crFore;
	COLORREF  crBack;
	int       nCellWidthPx;
	int       nCellHeightPx;

	bool operator==(const SGlyphAtlasKey& rhs) const
	{
		return hFont == rhs.hFont && wch0 == rhs.wch0 && wch1 == rhs.wch1
			&& crFore == rhs.crFore && crBack == rhs.crBack
			&& nCellWidthPx == rhs.nCellWidthPx && nCellHeightPx == rhs.nCellHeightPx;
	}
};

//! SGlyphAtlasKey用のハッシュ関数(std::unordered_map用)
struct SGlyphAtlasKeyHash {
	size_t operator()(const SGlyphAtlasKey& k) const
	{
		size_t h = std::hash<void*>()((void*)k.hFont);
		h = h * 131 + (size_t)k.wch0;
		h = h * 131 + (size_t)k.wch1;
		h = h * 131 + (size_t)k.crFore;
		h = h * 131 + (size_t)k.crBack;
		h = h * 131 + (size_t)k.nCellWidthPx;
		h = h * 131 + (size_t)k.nCellHeightPx;
		return h;
	}
};

//! アトラスページ内の1グリフ分の格納情報
struct SGlyphAtlasEntry {
	int  nPageIndex = 0;
	RECT rcCell = {0, 0, 0, 0};	//!< ページ内座標(左上原点)
	int  nCellWidthPx = 0;
	int  nCellHeightPx = 0;
};

//! FlushQueue()まで先送りする1グリフ分のBitBlt指示(転送元ページ内矩形→転送先座標)
struct SGlyphAtlasBlit {
	int  nPageIndex = 0;
	RECT rcSrc = {0, 0, 0, 0};		//!< 転送元(ページ内座標)
	int  nDestX = 0;
	int  nDestY = 0;
};

//! グリフアトラスの1ページ(複数グリフをシェルフパッキングで敷き詰めた1枚のビットマップ)
struct SGlyphAtlasPage {
	HBITMAP hBitmap = nullptr;
	HDC     hdcPage = nullptr;		//!< CreateCompatibleDC + SelectObject(hBitmap) 済み
	HBITMAP hbmpOld = nullptr;
	int     nShelfX = 0;			//!< 現シェルフ内の次の書き込みX
	int     nShelfY = 0;			//!< 現シェルフの開始Y
	int     nShelfHeight = 0;		//!< 現シェルフの高さ(最大セル高さ)
};

/*!	GDIによるグリフキャッシュ(グリフアトラス)のシングルトン。

	CTextDrawer::DispTextから、1文字(またはサロゲートペア)描画のたびに
	DrawOrCache()を呼び出す。ヒット・ミスとも実際のBitBltはその場では行わず
	m_vPendingBlitsへ積むだけにし、1回の描画パスの最後にFlushQueue()で
	まとめて転送する(20260802 2フェーズ化。詳細はDrawOrCache/FlushQueueの
	コメント、およびchangelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md参照)。

	CViewFont::GetFontGeneration()の値が変化したら(=フォントが作り直されたら)
	キャッシュ全体を破棄する(ClearIfStale)。
*/
class CGlyphAtlasCache : public TSingleton<CGlyphAtlasCache> {
	friend class TSingleton<CGlyphAtlasCache>;
	CGlyphAtlasCache();
	~CGlyphAtlasCache();

public:
	//! 設定でON/OFFする。OFFにすると即座に全ページを解放する。
	void SetEnabled(bool bEnabled);
	bool IsEnabled() const { return m_bEnabled; }

	/*!	グリフ1個(または1サロゲートペア)分の転送をキューへ積む(または新規に焼く)。

		ミス時、グリフをアトラスページへ焼く処理(SelectObject/SetTextColor/
		SetBkColor/ExtTextOut、ページ側のHDCのみを触る)はこの場で完了させる。
		一方、hdcへの実際のBitBlt(ヒット時・ミス時とも)はm_vPendingBlitsへ
		積むだけで、この場では行わない。呼び出し側は1回の描画パスの最後に
		必ずFlushQueue()を呼ぶこと(呼ばないとグリフが画面に出ない)。

		@param hdc            [in] 描画先DC(FlushQueue()に渡すDCと同じである前提)
		@param hFont          [in] 現在選択中のフォント(GetCurrentObject(hdc,OBJ_FONT)相当)
		@param pData          [in] 描画対象文字列の先頭
		@param nLength        [in] pDataの長さ(1または2のみ受け付ける)
		@param crFore         [in] 前景色(GetTextColor(hdc)相当)
		@param crBack         [in] 背景色(GetBkColor(hdc)相当)
		@param nDestX         [in] 描画先X(ExtTextOutと同じ原点)
		@param nDestY         [in] 描画先Y。行の折り返しクリップ矩形の上端
		                          (呼び出し側のrcClip.top)であること。
		                          行の途中(行間マージン分ずらした位置)を
		                          渡さないこと(下記nGlyphYOffsetで表現する)。
		@param nGlyphYOffset  [in] セル内でグリフを実際に描く際のYオフセット。
		                          呼び出し側が行間マージン等でグリフ位置を
		                          rcClip.topからずらして描画している場合、
		                          そのずらし量(nDrawY - rcClip.top)を渡す。
		                          20260809 これを渡さずnDestYに直接ずらし後の
		                          Y座標を渡すと、セルの高さ(nCellHeightPx、
		                          マージン込み)とBitBlt先の位置がずれ、行の
		                          上端(マージン部分)が塗り残されたまま次の
		                          行のマージン部分に描画がはみ出す不具合になる
		                          (実機確認済み: 行の間隔≠0のときだけ発生)。
		@param nCellWidthPx   [in] このグリフの描画幅(呼び出し側のDx配列合計)
		@param nCellHeightPx  [in] 行の高さ(CTextMetrics::GetHankakuDy())
		@param pDx            [in] ミス時の実描画に使うDx配列(nLength要素)

		@retval true  キューに積んだ(呼び出し側はExtTextOutを呼ばなくてよい。
		              ただし後でFlushQueue()を呼ぶまで実際には画面に出ない)
		@retval false 何もしなかった(呼び出し側が通常どおりExtTextOutすること)
	*/
	bool DrawOrCache(
		HDC hdc,
		HFONT hFont,
		const wchar_t* pData, int nLength,
		COLORREF crFore, COLORREF crBack,
		int nDestX, int nDestY,
		int nGlyphYOffset,
		int nCellWidthPx, int nCellHeightPx,
		const int* pDx
	);

	/*!	描画パスを開始する前に呼び、その時点でのキュー位置を「印」として受け取る。
		FlushQueue()にそのまま渡すこと。

		20260809 m_vPendingBlitsはシングルトン全体で共有されている一方、
		DrawOrCache()→FlushQueue()の呼び出しペアは「対括弧の即時描画」
		(CEditView_Paint_Bracket.cpp)や「通常のOnPaint」(CEditView_Paint.cpp)など
		複数の独立した描画パスから呼ばれる。これらがWM_PAINTの入れ子(例:
		OnSetFocus/UpdateWindowの同期呼び出しや、Grepのリアルタイム表示のように
		複数ペインが短時間に再描画される状況)で重なると、内側のパスの
		FlushQueue()が外側のパスがまだ積んでいた分まで一緒に描画してしまい、
		外側のパス用のグリフが内側のパスのDCへ誤って転送される
		(実機で「置換のリアルタイム表示中に別タブへ切り替える」操作で、
		本来出るはずのない位置に色付きの矩形/線が現れる不具合として確認・
		修正済み)。BeginQueue()で「自分がDrawOrCache()を呼び始める前の
		キュー位置」を覚えておき、FlushQueue()はその位置から末尾までだけを
		処理・削除することで、外側のパス分をそのまま残せるようにする。
	*/
	size_t BeginQueue() const { return m_vPendingBlits.size(); }

	/*!	DrawOrCache()でキューに積んだ分をまとめてBitBltする。

		呼び出し側は1回の描画パス(通常のOnPaint、対括弧強調表示の即時描画など、
		DispText経由でDrawOrCache()を呼ぶ一連の処理)の最初にBeginQueue()を呼び、
		最後に必ずその戻り値を渡してFlushQueue()を呼ぶこと。呼び忘れると
		グリフが画面に出ないので、DispTextを直接呼ぶ新しい描画経路を追加する
		ときは必ずこれも呼ぶこと。

		@param hdc        [in] 転送先DC。DrawOrCache()に渡したhdcと同じであること。
		@param markBegin  [in] このパスの直前にBeginQueue()で取得した値。
	*/
	void FlushQueue(HDC hdc, size_t markBegin);

	//! 全ページを解放し、キャッシュを空にする。積み残しのキューも破棄する。
	void Clear();

#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
	//! デバッグHUD用の統計スナップショット
	struct SStats {
		int    nPageCount;
		size_t nEntryCount;
		size_t nPendingBlitCount;
		UINT64 nHitCount;
		UINT64 nMissCount;
		UINT64 nWarmedCount;	//!< WarmUpAscii()で(個別ミスとは別に)焼いたグリフの累計数
	};
	SStats GetStats() const;
#endif // NKMM_

private:
	void ClearIfStale();
	bool AllocCell(int w, int h, SGlyphAtlasEntry* pOut);
	SGlyphAtlasPage* CreatePage();

	/*!	半角ASCII印字可能文字(0x20〜0x7E)を(hFont,crFore,crBack)の組でまとめて
		アトラスへ焼く。

		DrawOrCache()は1グリフごとにSelectObject/SetTextColor/SetBkColorを
		やり直すが、初めて見る(フォント,fg,bg)の組み合わせでは印字可能ASCIIの
		どれか1文字がミスした時点でこの関数を呼び、以後同じ組み合わせで発生する
		はずだった残りのASCIIミス(のセットアップコスト)を1回のページ選択に
		まとめて償却する。既にキャッシュ済みの文字はスキップするので、
		途中で呼ばれても安全(ページ確保に失敗したらそこで打ち切る)。

		@param nCellWidthPx  [in] 半角1文字分のセル幅。呼び出し側は「今回ミスした
		                          文字自身が半角ASCIIだった」ときのnCellWidthPxを
		                          そのまま渡すこと(全角セル幅を渡すと壊れる)
	*/
	void WarmUpAscii(HFONT hFont, COLORREF crFore, COLORREF crBack, int nGlyphYOffset, int nCellWidthPx, int nCellHeightPx);

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
	//! デバッグ用: 指定ページの実ピクセルをGetDIBits()で取得し、そのままbmpファイルへ書き出す
	//! (System.Drawing/GDI+等を介さない、HDCの内容そのもの)。Clear()から自動的に呼ばれる。
	void DumpPageToFile(const SGlyphAtlasPage& page, const wchar_t* pszPath) const;
#endif // NKMM_

	bool  m_bEnabled;
	ULONG m_nFontGeneration;
	bool  m_bPageAllocFailed;	//!< 一度でもページ確保に失敗したら以後は試みない
	std::vector<SGlyphAtlasPage> m_vPages;
	std::unordered_map<SGlyphAtlasKey, SGlyphAtlasEntry, SGlyphAtlasKeyHash> m_mapEntries;
	std::vector<SGlyphAtlasBlit> m_vPendingBlits;	//!< FlushQueue()まで先送りしたBitBlt指示

#ifdef NKMM_DEBUG_GLYPH_ATLAS_DUMP
	int m_nDumpCounter = 0;	//!< Clear()の呼び出しごとにファイル名が重複しないようにする通し番号
#endif // NKMM_

#ifdef NKMM_DEBUG_GLYPH_ATLAS_HUD
	//! プロセス寿命で積算するデバッグHUD用カウンタ(Clear()では リセットしない。
	//! フォント変更をまたいだキャッシュ全体の効きを見たいため)
	UINT64 m_nHitCount = 0;
	UINT64 m_nMissCount = 0;
	UINT64 m_nWarmedCount = 0;
#endif // NKMM_

	static const int PAGE_SIZE = 1024;
	static const int MAX_PAGES = 8;
};

#endif // NKMM_FIX_GLYPH_ATLAS_CACHE

#endif /* SAKURA_CGLYPHATLASCACHE_3E7B9B9C_3E9B_4B3B_9C7B_9C7B9C7B9C7B_H_ */
