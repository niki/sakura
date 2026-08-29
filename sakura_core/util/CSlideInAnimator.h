/*!	@file
	@brief フローティングダイアログの上からのスライドインアニメーション

	@author Yu-zuki.
	@date 2026.08.29 新規作成。CDlgFind::StartSlideAnimation()/CDlgCommandPalette::StartSlideAnimation()に
	                  定数値・ease-out計算式までほぼ同一のまま二重実装されていたものを共通化 // NKMM_FIND_DIALOG_FLAT, NKMM_COMMAND_PALETTE
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CSLIDEINANIMATOR_20260829_H_
#define SAKURA_CSLIDEINANIMATOR_20260829_H_

#include "util/window.h"

#if defined(NKMM_COMMAND_PALETTE) || defined(NKMM_FIX_FIND_DIALOG_FLAT)

/*!	@brief ウィンドウを最終位置より少し上から下へease-outでスライドインさせる

	Start()呼び出し時点のhwndのウィンドウ位置を最終位置とし、そこから
	DpiScaleY(nDistanceDip)だけ上へ即座に動かす(スライド開始位置)。以後
	WM_TIMERのたびにOnTimer()を呼ぶと、経過時間に応じた位置へhwndを動かす。
	AnimateWindow(AW_SLIDE)はDWM合成下で子コントロール(コンボボックス等)が
	追従せず枠だけ先に全表示されてしまう不具合があったため、ウィンドウ位置
	そのものをタイマーで動かす方式にしている 20260829
*/
class CSlideInAnimator
{
public:
	CSlideInAnimator()
		: m_nDurationMs( 0 )
		, m_nX( 0 )
		, m_nTargetY( 0 )
		, m_nStartY( 0 )
		, m_dwStartTick( 0 )
	{
	}

	//! hwndの現在位置を最終位置として記録し、スライド開始位置(最終位置より上)へ動かす
	void Start( HWND hwnd, int nDurationMs, int nDistanceDip )
	{
		m_nDurationMs = nDurationMs;

		RECT	rc;
		::GetWindowRect( hwnd, &rc );
		m_nX          = rc.left;
		m_nTargetY    = rc.top;
		m_nStartY     = rc.top - DpiScaleY( nDistanceDip );
		m_dwStartTick = ::GetTickCount();

		::SetWindowPos( hwnd, NULL, m_nX, m_nStartY, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	}

	//! WM_TIMERのたびに呼ぶ。hwndを経過時間に応じた位置へ動かす。最終位置に
	//! 到達済みならfalseを返す(呼び出し側でタイマーを止めること)
	bool OnTimer( HWND hwnd )
	{
		DWORD	dwElapsed = ::GetTickCount() - m_dwStartTick;
		if( dwElapsed >= (DWORD)m_nDurationMs ){
			::SetWindowPos( hwnd, NULL, m_nX, m_nTargetY, 0, 0,
				SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
			return false;
		}

		// ease-out (3次): 1 - (1-t)^3
		double	t = (double)dwElapsed / m_nDurationMs;
		double	u = 1.0 - t;
		double	eased = 1.0 - u * u * u;
		int	y = m_nStartY + (int)( ( m_nTargetY - m_nStartY ) * eased );

		::SetWindowPos( hwnd, NULL, m_nX, y, 0, 0,
			SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
		return true;
	}

private:
	int		m_nDurationMs;
	int		m_nX;
	int		m_nTargetY;
	int		m_nStartY;
	DWORD	m_dwStartTick;
};

#endif // NKMM_COMMAND_PALETTE || NKMM_FIX_FIND_DIALOG_FLAT

#endif /* SAKURA_CSLIDEINANIMATOR_20260829_H_ */
