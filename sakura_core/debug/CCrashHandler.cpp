// 20260815 新規作成: NKMM_CRASH_HANDLER
#include "StdAfx.h"
#include "CCrashHandler.h"

#ifdef NKMM_CRASH_HANDLER

// DbgHelp.h(MiniDumpWriteDump等)はStdAfx.h -> apiwrap/StdApi.h経由で読み込み済み
#pragma comment(lib, "Dbghelp.lib")
#include "util/file.h"

namespace {

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
//! (1)実行ファイルと同じフォルダ配下のCrashDumps\ (2)%TEMP%配下のCrashDumps\
//! の順。(1)は書き込み権限が無いフォルダ(Program Files等)にインストールされて
//! いると失敗しうるため、(2)は常に書き込み可能な場所として用意している。
//!
//! @note 意図的にCCommandLine/CFileNameManager等、共有メモリ(DLLSHAREDATA)に
//!	依存する仕組みは一切使わない。それらはこのクラッシュハンドラより後に
//!	初期化されるため、初期化前のクラッシュではまだ使えない
//!	(このコードベースのassert()はSEH例外を投げずダイアログ表示して処理を
//!	続けるため、__try/__exceptで安全に防御できない)。
bool TryWriteMiniDumpToCandidateDirs( TCHAR* pszDumpPathOut, size_t nDumpPathOutCount,
	EXCEPTION_POINTERS* pExceptionInfo )
{
	SYSTEMTIME st;
	::GetLocalTime( &st );

	TCHAR szCandidateDir[2][_MAX_PATH];
	szCandidateDir[0][0] = _T('\0');
	szCandidateDir[1][0] = _T('\0');
	GetExedir( szCandidateDir[0], NULL );	// 末尾に'\\'が付かない実装のため明示的に付与する
	_tcscat_s( szCandidateDir[0], _countof(szCandidateDir[0]), _T("\\CrashDumps\\") );
	if( 0 != ::GetTempPath( _countof(szCandidateDir[1]), szCandidateDir[1] ) ){
		_tcscat_s( szCandidateDir[1], _countof(szCandidateDir[1]), _T("sakura_CrashDumps\\") );
	}

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

LONG WINAPI OnUnhandledException( EXCEPTION_POINTERS* pExceptionInfo )
{
	if( 0 != ::InterlockedCompareExchange( &g_bHandlingCrash, 1, 0 ) ){
		// ダンプ生成中に別の例外(二重クラッシュ)が起きた場合は諦めてそのまま終了する
		return EXCEPTION_EXECUTE_HANDLER;
	}

	TCHAR szDumpPath[_MAX_PATH] = _T("");
	bool bWrote = TryWriteMiniDumpToCandidateDirs( szDumpPath, _countof(szDumpPath), pExceptionInfo );

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

#endif // NKMM_
