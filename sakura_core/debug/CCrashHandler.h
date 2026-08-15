/*!	@file
	@brief 未処理例外(クラッシュ)発生時にミニダンプを保存し、未保存の編集内容も
		可能な範囲で退避する

	SetUnhandledExceptionFilter()でトップレベルの例外フィルタを1つ登録し、
	Win32例外(アクセス違反等)でプロセスが強制終了する直前にMiniDumpWriteDump()で
	原因調査用のダンプファイルを書き出す。ダンプ保存に続けて、クラッシュした
	ウィンドウがアクティブに編集していたドキュメントに未保存の変更があれば、
	その内容もダンプと同じフォルダへベストエフォートで退避する
	(20260815 NKMM_CRASH_HANDLER_BUFFER)。

	意図的に「例外を検知して処理を継続する」機能は持たせていない。発生した
	例外はメモリ破壊を伴っている可能性があり、そのまま処理を続けるとファイル
	破損など、クラッシュそのものより深刻な被害につながりうるため、ダンプ保存後は
	速やかにプロセスを終了させる。未保存内容の退避も同じ理由でベストエフォート
	(失敗しても無視してダンプ保存自体は守る)にとどめている。
*/
#ifndef SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_
#define SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_

#include <string>

#ifdef NKMM_CRASH_HANDLER

//! WinMainの最初期(他の初期化より前)で1回呼ぶ。以後、未処理例外が発生すると
//! 自動でダンプファイルを書き出してからプロセスを終了する。
void InstallCrashHandler();

#ifdef NKMM_CRASH_HANDLER_BUFFER
//! メインウィンドウ(CEditWnd)が実際に作成できた直後に1回呼ぶ。クラッシュハンドラは
//! この通知より前は CEditWnd::getInstance() 等アプリ本体の状態に一切触れない
//! (共有メモリ未初期化の段階でシングルトンへ触れると、このコードベースのassert()が
//! SEH例外を投げずダイアログを出して処理を続けてしまい、ハングしうるため。詳細はcpp参照)
void NotifyMainWindowReady();

//! CrashDumps候補フォルダに未消費の退避ファイルが無いか探し、見つかれば
//! そのファイルを排他的に「claim」した上でtrueを返す(2重起動時の競合防止)。
//! @param[out] origPath 元のファイルパス(無題だった場合は空)
//! @param[out] bufPath  退避したバッファ内容(UTF-8+BOM)ファイルの絶対パス
bool FindAndClaimCrashRecovery( std::wstring& origPath, std::wstring& bufPath );

//! FindAndClaimCrashRecovery()で見つかった退避ファイル一式を削除する。
//! 復元する/しないに関わらず、確認後は必ず呼ぶこと。
void DeleteCrashRecoveryFiles( const std::wstring& bufPath );
#endif // NKMM_

#endif // NKMM_

#endif /* SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_ */
/*[EOF]*/
