// 20260815 新規作成: NKMM_CRASH_HANDLER
#include "StdAfx.h"
#include "CCrashHandler.h"

#ifdef NKMM_CRASH_HANDLER

// DbgHelp.h(MiniDumpWriteDump等)はStdAfx.h -> apiwrap/StdApi.h経由で読み込み済み
#pragma comment(lib, "Dbghelp.lib")
#include "util/file.h"

#ifdef NKMM_CRASH_HANDLER_BUFFER
#include "window/CEditWnd.h"
#include "doc/CEditDoc.h"
#include "CWriteManager.h"
#include "EditInfo.h"
#include <vector>
#endif // NKMM_

namespace {

//! ダンプ(および退避ファイル)の保存先候補を2つ求める。
//! (1)実行ファイルと同じフォルダ配下のCrashDumps\ (2)%TEMP%配下のsakura_CrashDumps\
//! の順。(1)は書き込み権限が無いフォルダ(Program Files等)にインストールされて
//! いると失敗しうるため、(2)は常に書き込み可能な場所として用意している。
//!
//! @note 意図的にCCommandLine/CFileNameManager等、共有メモリ(DLLSHAREDATA)に
//!	依存する仕組みは一切使わない。それらはこのクラッシュハンドラより後に
//!	初期化されるため、初期化前のクラッシュではまだ使えない
//!	(このコードベースのassert()はSEH例外を投げずダイアログ表示して処理を
//!	続けるため、__try/__exceptで安全に防御できない)。
void GetCandidateDirs( TCHAR szCandidateDir[2][_MAX_PATH] )
{
	szCandidateDir[0][0] = _T('\0');
	szCandidateDir[1][0] = _T('\0');
	GetExedir( szCandidateDir[0], NULL );	// 末尾に'\\'が付かない実装のため明示的に付与する
	_tcscat_s( szCandidateDir[0], _MAX_PATH, _T("\\CrashDumps\\") );
	if( 0 != ::GetTempPath( _MAX_PATH, szCandidateDir[1] ) ){
		_tcscat_s( szCandidateDir[1], _MAX_PATH, _T("sakura_CrashDumps\\") );
	}
}

//! 二重クラッシュ(ダンプ生成処理自体が壊れたヒープに触れて再度例外を起こす)防止ガード
volatile LONG g_bHandlingCrash = 0;

bool WriteMiniDump( const TCHAR* pszDumpPath, EXCEPTION_POINTERS* pExceptionInfo )
{
	HANDLE hFile = ::CreateFile( pszDumpPath, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if( hFile == INVALID_HANDLE_VALUE ) return false;

	MINIDUMP_EXCEPTION_INFORMATION mdei;
	mdei.ThreadId = ::GetCurrentThreadId();
	mdei.ExceptionPointers = pExceptionInfo;
	mdei.ClientPointers = FALSE;

	// データセグメント(グローバル/静的変数)とスタック・レジスタから間接参照される
	// メモリを含める。フルメモリダンプほど巨大にはせず、原因調査に足る情報に絞る。
	MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(
		MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory );

	BOOL bOk = ::MiniDumpWriteDump(
		::GetCurrentProcess(), ::GetCurrentProcessId(),
		hFile, dumpType,
		pExceptionInfo ? &mdei : NULL,
		NULL, NULL );

	::CloseHandle( hFile );
	return bOk ? true : false;
}

//! ダンプの保存先候補を順に試し、実際にダンプを書けたパスを返す。
bool TryWriteMiniDumpToCandidateDirs( TCHAR* pszDumpPathOut, size_t nDumpPathOutCount,
	EXCEPTION_POINTERS* pExceptionInfo )
{
	SYSTEMTIME st;
	::GetLocalTime( &st );

	TCHAR szCandidateDir[2][_MAX_PATH];
	GetCandidateDirs( szCandidateDir );

	for( int i = 0; i < 2; ++i ){
		if( szCandidateDir[i][0] == _T('\0') ) continue;

		::CreateDirectory( szCandidateDir[i], NULL );	// 無ければ作る。既存/失敗は無視する

		TCHAR szDumpPath[_MAX_PATH];
		auto_sprintf( szDumpPath, _T("%s%04d%02d%02d_%02d%02d%02d_%08X.dmp"),
			szCandidateDir[i], st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
			::GetCurrentProcessId() );

		if( WriteMiniDump( szDumpPath, pExceptionInfo ) ){
			_tcscpy_s( pszDumpPathOut, nDumpPathOutCount, szDumpPath );
			return true;
		}
	}
	return false;
}

#ifdef NKMM_CRASH_HANDLER_BUFFER
//! CEditWnd::getInstance()等、アプリ本体のシングルトンに触れてよくなったかどうか。
//! NotifyMainWindowReady()が呼ばれるまでは0のまま(詳細はヘッダのコメント参照)。
volatile LONG g_bMainWindowReady = 0;

//! TryFlushUnsavedBuffer()の実処理。C++オブジェクト(EditInfo/SSaveInfo/CWriteManager等、
//! いずれもデストラクタを持つ)をローカルに置くため、__tryとは別関数に分離している
//! (同じ関数内にC++オブジェクトのアンワインディングと__tryを同居させるとC2712になる)。
void TryFlushUnsavedBufferInner( const TCHAR* pszDumpDir, const TCHAR* pszBaseName )
{
	CEditWnd* pWnd = CEditWnd::getInstance();
	if( !pWnd || !pWnd->GetHwnd() ) return;
	CEditDoc* pDoc = pWnd->GetDocument();
	if( !pDoc ) return;
	if( !pDoc->m_cDocEditor.IsModified() ) return;

	EditInfo info;
	pDoc->GetEditInfo( &info );

	TCHAR szBufPath[_MAX_PATH];
	auto_sprintf( szBufPath, _T("%s%s.rbuf"), pszDumpDir, pszBaseName );

	// MYWM_DUMPBUFFER(NKMM_SESSION_RESTORE_BUFFER)と同じ形式(UTF-8+BOM、改行無変換)。
	// 復元側もRestoreBufferOverlay()と同じ読み込み経路を使う
	SSaveInfo sSaveInfo;
	sSaveInfo.cFilePath = szBufPath;
	sSaveInfo.eCharCode = CODE_UTF8;
	sSaveInfo.bBomExist = true;
	sSaveInfo.cEol = EOL_NONE;

	CWriteManager cWriter;
	if( RESULT_FAILURE == cWriter.WriteFile_From_CDocLineMgr( pDoc->m_cDocLineMgr, sSaveInfo ) ) return;

	// 元のファイルパス(無題なら空文字列)をBOM無しUTF-16LEの生バイト列で記録する
	TCHAR szMetaPath[_MAX_PATH];
	auto_sprintf( szMetaPath, _T("%s%s.rpath"), pszDumpDir, pszBaseName );
	HANDLE hFile = ::CreateFile( szMetaPath, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if( hFile != INVALID_HANDLE_VALUE ){
		DWORD dwWritten;
		::WriteFile( hFile, info.m_szPath, (DWORD)(_tcslen(info.m_szPath) * sizeof(TCHAR)), &dwWritten, NULL );
		::CloseHandle( hFile );
	}
}

//! クラッシュしたウィンドウがアクティブに編集していたドキュメントに未保存の
//! 変更があれば、その内容とファイルパスをダンプと同じフォルダへ退避する。
//! ベストエフォート。失敗しても(あるいは何が起きても)呼び出し元には影響しない。
void TryFlushUnsavedBuffer( const TCHAR* pszDumpDir, const TCHAR* pszBaseName )
{
	if( 0 == g_bMainWindowReady ) return;

	__try{
		TryFlushUnsavedBufferInner( pszDumpDir, pszBaseName );
	}
	__except( EXCEPTION_EXECUTE_HANDLER ){
		// 退避処理自体が再クラッシュしても無視する。ミニダンプは既に保存済みなので致命的ではない
	}
}
#endif // NKMM_

LONG WINAPI OnUnhandledException( EXCEPTION_POINTERS* pExceptionInfo )
{
	if( 0 != ::InterlockedCompareExchange( &g_bHandlingCrash, 1, 0 ) ){
		// ダンプ生成中に別の例外(二重クラッシュ)が起きた場合は諦めてそのまま終了する
		return EXCEPTION_EXECUTE_HANDLER;
	}

	TCHAR szDumpPath[_MAX_PATH] = _T("");
	bool bWrote = TryWriteMiniDumpToCandidateDirs( szDumpPath, _countof(szDumpPath), pExceptionInfo );

#ifdef NKMM_CRASH_HANDLER_BUFFER
	if( bWrote ){
		TCHAR szDumpDir[_MAX_PATH], szDrive[_MAX_DRIVE], szDir[_MAX_DIR], szBaseName[_MAX_FNAME];
		_tsplitpath( szDumpPath, szDrive, szDir, szBaseName, NULL );
		auto_sprintf( szDumpDir, _T("%s%s"), szDrive, szDir );
		TryFlushUnsavedBuffer( szDumpDir, szBaseName );
	}
#endif // NKMM_

	TCHAR szMsg[_MAX_PATH + 256];
	if( bWrote ){
		auto_sprintf( szMsg,
			_T("予期しないエラーが発生したため、サクラエディタを終了します。\n\n")
			_T("原因調査用の情報を以下に保存しました。\n%s"),
			szDumpPath );
	}else{
		_tcscpy_s( szMsg, _countof(szMsg),
			_T("予期しないエラーが発生したため、サクラエディタを終了します。\n\n")
			_T("(診断情報の保存には失敗しました)") );
	}
	::MessageBox( NULL, szMsg, _T("サクラエディタ"), MB_OK | MB_ICONERROR );

	return EXCEPTION_EXECUTE_HANDLER;
}

} // namespace

void InstallCrashHandler()
{
	::SetUnhandledExceptionFilter( OnUnhandledException );
}

#ifdef NKMM_CRASH_HANDLER_BUFFER
void NotifyMainWindowReady()
{
	::InterlockedExchange( &g_bMainWindowReady, 1 );
}

bool FindAndClaimCrashRecovery( std::wstring& origPath, std::wstring& bufPath )
{
	origPath.clear();
	bufPath.clear();

	TCHAR szCandidateDir[2][_MAX_PATH];
	GetCandidateDirs( szCandidateDir );

	for( int i = 0; i < 2; ++i ){
		if( szCandidateDir[i][0] == _T('\0') ) continue;

		std::wstring strPattern = std::wstring( szCandidateDir[i] ) + L"*.rpath";
		WIN32_FIND_DATA fd;
		HANDLE hFind = ::FindFirstFile( strPattern.c_str(), &fd );
		if( hFind == INVALID_HANDLE_VALUE ) continue;

		bool bClaimed = false;
		do{
			if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) continue;

			TCHAR szDrive[_MAX_DRIVE], szDir[_MAX_DIR], szFname[_MAX_FNAME];
			std::wstring strRPathFull = std::wstring( szCandidateDir[i] ) + fd.cFileName;
			_tsplitpath( strRPathFull.c_str(), szDrive, szDir, szFname, NULL );

			TCHAR szClaimedPath[_MAX_PATH];
			_tmakepath( szClaimedPath, szDrive, szDir, szFname, _T(".claimed") );

			// 排他的にclaimする(rename)。他プロセスが先に処理していれば失敗するので次を探す
			if( !::MoveFile( strRPathFull.c_str(), szClaimedPath ) ) continue;

			// 元のファイルパス(BOM無しUTF-16LEの生バイト列)を読む
			HANDLE hFile = ::CreateFile( szClaimedPath, GENERIC_READ, FILE_SHARE_READ, NULL,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
			if( hFile != INVALID_HANDLE_VALUE ){
				DWORD dwSize = ::GetFileSize( hFile, NULL );
				if( dwSize > 0 && dwSize < sizeof(TCHAR) * _MAX_PATH ){
					std::vector<TCHAR> vBuf( dwSize / sizeof(TCHAR) + 1, _T('\0') );
					DWORD dwRead = 0;
					::ReadFile( hFile, vBuf.data(), dwSize, &dwRead, NULL );
					origPath.assign( vBuf.data(), dwRead / sizeof(TCHAR) );
				}
				::CloseHandle( hFile );
			}

			TCHAR szBufPath[_MAX_PATH];
			_tmakepath( szBufPath, szDrive, szDir, szFname, _T(".rbuf") );
			if( fexist( szBufPath ) ){
				bufPath = szBufPath;
				bClaimed = true;
			}else{
				// 対応するバッファファイルが無い(壊れた/中途半端な退避)。claim済みメタファイルも削除して諦める
				::DeleteFile( szClaimedPath );
				origPath.clear();
			}
		}while( !bClaimed && ::FindNextFile( hFind, &fd ) );
		::FindClose( hFind );

		if( bClaimed ) return true;
	}
	return false;
}

void DeleteCrashRecoveryFiles( const std::wstring& bufPath )
{
	if( bufPath.empty() ) return;

	TCHAR szDrive[_MAX_DRIVE], szDir[_MAX_DIR], szFname[_MAX_FNAME];
	_tsplitpath( bufPath.c_str(), szDrive, szDir, szFname, NULL );

	TCHAR szClaimedPath[_MAX_PATH];
	_tmakepath( szClaimedPath, szDrive, szDir, szFname, _T(".claimed") );

	::DeleteFile( bufPath.c_str() );
	::DeleteFile( szClaimedPath );
}
#endif // NKMM_

#endif // NKMM_
