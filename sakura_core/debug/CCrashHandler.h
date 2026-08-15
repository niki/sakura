/*!	@file
	@brief 未処理例外(クラッシュ)発生時にミニダンプを保存する

	SetUnhandledExceptionFilter()でトップレベルの例外フィルタを1つ登録し、
	Win32例外(アクセス違反等)でプロセスが強制終了する直前にMiniDumpWriteDump()で
	原因調査用のダンプファイルを書き出す。

	意図的に「例外を検知して処理を継続する」機能は持たせていない。発生した
	例外はメモリ破壊を伴っている可能性があり、そのまま処理を続けるとファイル
	破損など、クラッシュそのものより深刻な被害につながりうるため、ダンプ保存後は
	速やかにプロセスを終了させる。
*/
#ifndef SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_
#define SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_

#ifdef NKMM_CRASH_HANDLER

//! WinMainの最初期(他の初期化より前)で1回呼ぶ。以後、未処理例外が発生すると
//! 自動でダンプファイルを書き出してからプロセスを終了する。
void InstallCrashHandler();

#endif // NKMM_

#endif /* SAKURA_CCRASHHANDLER_7A1D3E9C_5B6F_4C3A_9E7C_1F2A3B4C5D6E_H_ */
/*[EOF]*/
