/*!	@file
	@brief bregonig.dll が見つからない場合に使う PCRE2 ベースの代替実装

	CBregexpDll2 が公開している BMatch/BSubst/BMatchEx/BSubstEx/BRegfree/
	BRegexpVersion と同じ形の関数を提供する。CBregexp (検索/置換/Grep/マクロ)
	と CRegexKeyword (構文強調) の両方から、DLL版と同じ呼び出し規約で使われる
	ことを想定している。

	BREGEXP.DLL 由来のコマンド文字列 ("m/pattern/flags", "s/pattern/repl/flags")
	をパースして PCRE2(libs/pcre2, BSD-3-Clause) にコンパイルする。PCRE2は
	ルックビハインド・POSIX文字クラス・Unicode文字プロパティ等をネイティブ
	サポートしており、BREGEXP(Oniguruma系)の挙動に近い。
	(20260720 実装当初はstd::wregexを使っていたが、ルックビハインド等の
	非対応構文が多かったためPCRE2に置き換えた)
*/
#ifndef SAKURA_CREGEXFALLBACK_9C6E9C9B_9E7B_4B5A_9C2E_2E6E9C6E9C6E_H_
#define SAKURA_CREGEXFALLBACK_9C6E9C9B_9E7B_4B5A_9C2E_2E6E9C6E9C6E_H_

#include "CBregexpDll2.h"

// 20260720 このファイル一式(CRegexFallback.h/.cpp)は「正規表現DLLが見つからない場合に
// PCRE2へフォールバックする」機能(NKMM_FIX_REGEXP_FALLBACK)専用の実装のため、
// 定義が無効なビルドでは丸ごと除外する。my_config.h参照。
#ifdef NKMM_FIX_REGEXP_FALLBACK

namespace RegexFallback {

int BMatch(const wchar_t* str, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg);
int BSubst(const wchar_t* str, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg);
int BMatchEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg);
int BSubstEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg);
int BTrans(const wchar_t* str, wchar_t* target, wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg);
int BSplit(const wchar_t* str, wchar_t* target, wchar_t* targetendp, int limit, BREGEXP_W** rxp, wchar_t* msg);
void BRegfree(BREGEXP_W* rx);
const wchar_t* BRegexpVersion();

} // namespace RegexFallback

#endif // NKMM_FIX_REGEXP_FALLBACK

#endif /* SAKURA_CREGEXFALLBACK_9C6E9C9B_9E7B_4B5A_9C2E_2E6E9C6E9C6E_H_ */
/*[EOF]*/
