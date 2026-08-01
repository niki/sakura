/*!	@file
	@brief サードパーティライセンス表示ダイアログ

	@date 20260802 新規作成
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_THIRDPARTY_LICENSE

#include "dlg/CDlgThirdPartyLicense.h"
#include "sakura_rc.h"

#ifdef NKMM_FIX_REGEXP_FALLBACK
#include "extmodule/CRegexFallback.h"
#endif
#ifdef NKMM_FIX_QUICKJS_MACRO
#include "quickjs.h"
#endif
#ifdef NKMM_USE_MIMALLOC
#include <mimalloc.h>
#endif

#ifdef NKMM_FIX_REGEXP_FALLBACK
// libs\pcre2\LICENCE.md
static const wchar_t* const s_pszLicensePcre2 =
LR"PCRE2LIC(Copyright (c) 1997-2007 University of Cambridge
Copyright (c) 2007-2024 Philip Hazel
Copyright (c) 2009-2024 Zoltan Herczeg (JIT compilation support)
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notices,
  this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
  notices, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

* Neither the name of the University of Cambridge nor the names of any
  contributors may be used to endorse or promote products derived from this
  software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
)PCRE2LIC";

// libs\deps\sljit\LICENSE
static const wchar_t* const s_pszLicenseSljit =
LR"SLJITLIC(Copyright Zoltan Herczeg (hzmester@freemail.hu). All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this list of
     conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this list
     of conditions and the following disclaimer in the documentation and/or other materials
     provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL THE COPYRIGHT HOLDER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
)SLJITLIC";
#endif // NKMM_FIX_REGEXP_FALLBACK

#ifdef NKMM_FIX_QUICKJS_MACRO
// libs\quickjs\LICENSE
// 生の非ASCII文字はソースファイルのエンコーディングによって文字化けする恐れがあるため、
// raw string literalを分割してユニバーサル文字名(\uXXXX)で挟む
static const wchar_t* const s_pszLicenseQuickJs =
LR"QJSLIC(The MIT License (MIT)

Copyright (c) 2017-2026 Fabrice Bellard
Copyright (c) 2017-2024 Charlie Gordon
Copyright (c) 2023-2026 Ben Noordhuis
Copyright (c) 2023-2026 Sa)QJSLIC" L"\u00FA" LR"QJSLIC(l Ibarra Corretg)QJSLIC" L"\u00E9" LR"QJSLIC(

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
)QJSLIC";
#endif // NKMM_FIX_QUICKJS_MACRO

#ifdef NKMM_USE_MIMALLOC
// libs\mimalloc\LICENSE
static const wchar_t* const s_pszLicenseMimalloc =
LR"MILIC(MIT License

Copyright (c) 2018-2025 Microsoft Corporation, Daan Leijen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
)MILIC";
#endif // NKMM_USE_MIMALLOC

/* モーダルダイアログの表示 */
int CDlgThirdPartyLicense::DoModal( HINSTANCE hInstance, HWND hwndParent )
{
	return (int)CDialog::DoModal( hInstance, hwndParent, IDD_THIRDPARTY_LICENSE, (LPARAM)NULL );
}

/*! 初期化処理: 各ライブラリの見出し(名前・バージョン・ライセンス種別)とライセンス全文を連結して表示する */
BOOL CDlgThirdPartyLicense::OnInitDialog( HWND hwndDlg, WPARAM wParam, LPARAM lParam )
{
	_SetHwnd( hwndDlg );

	TCHAR szMsg[256];
	CNativeT cmemMsg;
	bool bFirst = true;

#ifdef NKMM_FIX_REGEXP_FALLBACK
	if( !bFirst ) cmemMsg.AppendString( _T("\r\n========================================\r\n\r\n") );
	bFirst = false;
	auto_sprintf( szMsg, _T("%s (BSD-3-Clause)\r\n\r\n"), to_tchar(RegexFallback::Pcre2LibraryVersion()) );
	cmemMsg.AppendString( szMsg );
	cmemMsg.AppendString( s_pszLicensePcre2 );

	cmemMsg.AppendString( _T("\r\n========================================\r\n\r\n") );
	auto_sprintf( szMsg, _T("%s (BSD-2-Clause)\r\n\r\n"), to_tchar(RegexFallback::SljitLibraryVersion()) );
	cmemMsg.AppendString( szMsg );
	cmemMsg.AppendString( s_pszLicenseSljit );
#endif // NKMM_FIX_REGEXP_FALLBACK

#ifdef NKMM_FIX_QUICKJS_MACRO
	if( !bFirst ) cmemMsg.AppendString( _T("\r\n========================================\r\n\r\n") );
	bFirst = false;
	auto_sprintf( szMsg, _T("QuickJS %s (MIT)\r\n\r\n"), to_tchar(JS_GetVersion()) );
	cmemMsg.AppendString( szMsg );
	cmemMsg.AppendString( s_pszLicenseQuickJs );
#endif // NKMM_FIX_QUICKJS_MACRO

#ifdef NKMM_USE_MIMALLOC
	if( !bFirst ) cmemMsg.AppendString( _T("\r\n========================================\r\n\r\n") );
	bFirst = false;
	{
		int nMiVer = mi_version(); // major*10000 + minor*100 + patch
		auto_sprintf( szMsg, _T("mimalloc %d.%d.%d (MIT)\r\n\r\n"),
			nMiVer / 10000, (nMiVer / 100) % 100, nMiVer % 100
		);
		cmemMsg.AppendString( szMsg );
	}
	cmemMsg.AppendString( s_pszLicenseMimalloc );
#endif // NKMM_USE_MIMALLOC

	::DlgItem_SetText( GetHwnd(), IDC_EDIT_LICENSE, cmemMsg.GetStringPtr() );

	/* 基底クラスメンバ */
	return CDialog::OnInitDialog( GetHwnd(), wParam, lParam );
}

#endif // NKMM_FIX_THIRDPARTY_LICENSE
