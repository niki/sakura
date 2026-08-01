/*!	@file
	@brief サードパーティライセンス表示ダイアログ

	バージョン情報ダイアログのエディットボックスにはPCRE2/sljit/QuickJS/
	mimallocのライセンス全文を収める余地がないため、別ウィンドウとして
	表示する。20260802 新規作成
*/
#ifndef SAKURA_CDLGTHIRDPARTYLICENSE_2F6C9B3D_1C0A_4B7E_9A5F_6D1E9C7B3A2E_H_
#define SAKURA_CDLGTHIRDPARTYLICENSE_2F6C9B3D_1C0A_4B7E_9A5F_6D1E9C7B3A2E_H_

#ifdef NKMM_FIX_THIRDPARTY_LICENSE

#include "dlg/CDialog.h"

class CDlgThirdPartyLicense : public CDialog
{
public:
	int DoModal( HINSTANCE, HWND );	//!< モーダルダイアログの表示
protected:
	BOOL OnInitDialog( HWND, WPARAM, LPARAM );
};

#endif // NKMM_FIX_THIRDPARTY_LICENSE

#endif /* SAKURA_CDLGTHIRDPARTYLICENSE_2F6C9B3D_1C0A_4B7E_9A5F_6D1E9C7B3A2E_H_ */
