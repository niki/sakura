/*!	@file
	@brief 値をease-outで滑らかに始点から終点へ近づける汎用アニメーション

	@author Yu-zuki.
	@date 2026.08.29 新規作成。CDlgFind::StartSlideAnimation()/CDlgCommandPalette::StartSlideAnimation()に
	                  ウィンドウ位置のスライドインとして、定数値・ease-out計算式までほぼ同一のまま
	                  二重実装されていたものを共通化 // NKMM_FIND_DIALOG_FLAT, NKMM_COMMAND_PALETTE
	@date 2026.08.30 「値の時間補間(汎用部)」と「値を実際にどう反映するか(SetWindowPos等、
	                  呼び出し側ごとに異なる描画部)」を分離した。以前はStart(HWND,...)/
	                  OnTimer(HWND)がクラス内でSetWindowPos(位置)を直接呼んでおり、
	                  CDlgCommandPaletteの一覧高さリサイズ(SetWindowPosを2つのウィンドウへ、
	                  高さとして適用する必要がある)を同じ枠組みで扱おうとすると描画部まで
	                  クラス内に持ち込む必要が出てきてしまっていた。呼び出し側がStart()に
	                  ApplyFunc(値を受け取って反映するだけの小さな関数)を登録する形にし、
	                  クラス自体は「今どの値であるべきか」の計算だけを担う
	@date 2026.08.30 InitFunc/FinalizeFuncを追加。ApplyFuncと同じ要領で、開始時に1回だけ
	                  行う処理(スライドイン開始時のShowWindow等)・完了時に1回だけ行う処理を
	                  Start()へまとめて登録できるようにした。これにより呼び出し側は
	                  Start()呼び出しの前後に個別の文を並べる必要がなくなる
	@date 2026.08.30 Start()の引数順をfnApply,fnInit,fnFinalizeからfnInit,fnApply,fnFinalize
	                  (ライフサイクルの一般的な語順)へ変更。実行順序(fnApplyの初回呼び出しが
	                  fnInitより先)はそのまま変えていない点に注意(Start()のコメント参照)
*/
/*
	This source code is designed for sakura editor.
	Please contact the copyright holder to use this code for other purpose.
*/

#ifndef SAKURA_CSLIDEINANIMATOR_20260829_H_
#define SAKURA_CSLIDEINANIMATOR_20260829_H_

#include "util/window.h"
#include <functional>

#if defined(NKMM_COMMAND_PALETTE) || defined(NKMM_FIX_FIND_DIALOG_FLAT)

/*!	@brief 値をease-outで始点から終点へ滑らかに近づける、時間補間だけを担う汎用アニメーション

	「何の値か」(ウィンドウのY座標、一覧の高さ等)や「値をどう反映するか」(SetWindowPos等)は
	一切知らない。Start()で開始値・目標値・所要時間を渡してタイマーを起動し、以後WM_TIMERの
	たびにOnTimer()を呼ぶだけでよい。値の反映は、Start()に渡すApplyFunc(省略可)へ委ねる:
	登録しておけばStart()自身とOnTimer()の両方が現在値でこれを呼び出す。ApplyFuncを登録
	しない場合は、呼び出し側が好きなタイミングでGetCurrentValue()をポーリングしてもよい
	(CDlgCommandPaletteの一覧高さリサイズは、1回のtickでリスト・ダイアログ本体2つの
	ウィンドウへ適用する必要があるため、この形で使っている) 20260829 20260830
*/
class CSlideInAnimator
{
public:
	//! 現在値を受け取り、実際の反映(SetWindowPos等)を行うだけの小さな関数 20260830
	typedef std::function<void( int nValue )>	ApplyFunc;
	typedef std::function<void()>				InitFunc;		//!< Start()で開始値を反映した直後に1回だけ呼ぶ初期化処理(ShowWindow等) 20260830
	typedef std::function<void()>				FinalizeFunc;	//!< アニメーション完了時に1回だけ呼ぶ後処理 20260830

	CSlideInAnimator()
		: m_nDurationMs( 0 )
		, m_nTargetValue( 0 )
		, m_nStartValue( 0 )
		, m_dwStartTick( 0 )
	{
	}

	/*!	@brief nStartValueからnTargetValueへnDurationMsかけてease-outで近づけるアニメーションを開始する

		引数の並びはfnInit,fnApply,fnFinalizeというライフサイクル順だが、実際の呼び出し順は
		これとは異なるので注意: fnApplyを渡した場合、ここで最初の値(=ほぼnStartValue)を
		すぐ1回反映するのが先で、fnInitはその直後に呼ぶ(スライドインで言えば「開始位置へ
		ジャンプ→その後で見せる」の順を保つため、必ずfnApplyの初回呼び出しより後になる)。
		fnFinalizeはIsDone()が真になった最初のOnTimer()呼び出しの中で、最後のfnApply呼び出しの
		後に1回だけ呼ぶ。いずれも省略可能で、fnApplyを省略した場合は呼び出し側がOnTimer()の
		戻り値を見つつGetCurrentValue()で値を取り出して自分で使う形にもできる 20260830
	*/
	void Start( int nStartValue, int nTargetValue, int nDurationMs,
		InitFunc fnInit = InitFunc(), ApplyFunc fnApply = ApplyFunc(), FinalizeFunc fnFinalize = FinalizeFunc() )
	{
		m_nStartValue  = nStartValue;
		m_nTargetValue = nTargetValue;
		m_nDurationMs  = nDurationMs;
		m_dwStartTick  = ::GetTickCount();
		m_fnApply      = fnApply;
		m_fnFinalize   = fnFinalize;

		if( m_fnApply ){
			m_fnApply( GetCurrentValue() );
		}
		if( fnInit ){
			fnInit();
		}
	}

	//! WM_TIMERのたびに呼ぶ。fnApplyを登録していればここで呼んで現在値を反映する。
	//! アニメーション時間に到達済みなら、最後の反映の後にfnFinalize(登録していれば)を
	//! 呼んでからfalseを返す(呼び出し側でタイマーを止めること) 20260830
	bool OnTimer()
	{
		bool	bDone = IsDone();
		if( m_fnApply ){
			m_fnApply( GetCurrentValue() );
		}
		if( bDone && m_fnFinalize ){
			m_fnFinalize();
		}
		return !bDone;
	}

	//! 現在時刻でのease-out値を返す(完了していればnTargetValueそのもの) 20260830
	int GetCurrentValue() const
	{
		DWORD	dwElapsed = ::GetTickCount() - m_dwStartTick;
		if( dwElapsed >= (DWORD)m_nDurationMs ){
			return m_nTargetValue;
		}
		// ease-out (3次): 1 - (1-t)^3
		double	t = (double)dwElapsed / m_nDurationMs;
		double	u = 1.0 - t;
		double	eased = 1.0 - u * u * u;
		return m_nStartValue + (int)( ( m_nTargetValue - m_nStartValue ) * eased );
	}

	//! アニメーション時間(nDurationMs)に到達済みかどうか 20260830
	bool IsDone() const
	{
		return ( ::GetTickCount() - m_dwStartTick ) >= (DWORD)m_nDurationMs;
	}

private:
	int				m_nDurationMs;
	int				m_nTargetValue;	//!< アニメーションの目標値
	int				m_nStartValue;	//!< アニメーション開始時の値
	DWORD			m_dwStartTick;
	ApplyFunc		m_fnApply;		//!< 値が決まるたびに呼ぶ反映処理(未登録ならno-op)
	FinalizeFunc	m_fnFinalize;	//!< 完了時に1回だけ呼ぶ後処理(未登録ならno-op)。fnInitはStart()の中でしか使わないため保持不要
};

#endif // NKMM_COMMAND_PALETTE || NKMM_FIX_FIND_DIALOG_FLAT

#endif /* SAKURA_CSLIDEINANIMATOR_20260829_H_ */
