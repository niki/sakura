/*!	@file
	@brief bregonig.dll が見つからない場合に使う PCRE2 ベースの代替実装

	20260720 エンジンをstd::wregexからPCRE2(libs/pcre2, BSD-3-Clause)に置き換えた。
	PCRE2はルックビハインド・POSIX文字クラス・Unicode文字プロパティ・拡張構文(/x)・
	複数行モード(/m)をネイティブサポートしており、BREGEXP(Oniguruma系)の挙動に
	std::regex(ECMAScript文法)よりずっと近い。
*/
#include "StdAfx.h"
#include "CRegexFallback.h"
#include <string>
#include <vector>
#include <memory>

// 20260720 NKMM_FIX_REGEXP_FALLBACK専用の実装。my_config.h参照。
#ifdef NKMM_FIX_REGEXP_FALLBACK

#include "pcre2.h"

namespace RegexFallback {

namespace {

//! BREGEXP_W(outp/outendp/splitctr/splitpがconst)を集成体初期化して値で返す。
//! メンバ初期化子リストの中で直接 "BREGEXP_W{...}" と書くとMSVCの解析でつまずく
//! ため、いったん関数の戻り値として作ってからコピー初期化する。
BREGEXP_W MakeBaseW(wchar_t* outp, wchar_t* outendp)
{
	return BREGEXP_W{ outp, outendp, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, 0 };
}

//! フォールバック版の BREGEXP_W。startp/endp/nparens は毎回更新できる(非const)が、
//! outp/outendp/splitctr/splitp は BREGEXP_W 側で const 宣言されているため、
//! 結果文字列を持つ新しいインスタンスを都度作り直す(実DLLがBREGEXP_W**を
//! 引数に取るのもこの目的のため)。
//! code(コンパイル済みパターン)はポインタの持ち主が1つに定まるようmove-onlyにする。
struct BREGEXP_W_Fallback : public BREGEXP_W {
	explicit BREGEXP_W_Fallback(std::unique_ptr<wchar_t[]> buf = nullptr, size_t len = 0)
		: BREGEXP_W(MakeBaseW(buf.get(), buf.get() + len))
		, outHeap(std::move(buf))
	{
	}
	~BREGEXP_W_Fallback()
	{
		if (code) pcre2_code_free_16(code);
	}
	BREGEXP_W_Fallback(const BREGEXP_W_Fallback&) = delete;
	BREGEXP_W_Fallback& operator=(const BREGEXP_W_Fallback&) = delete;

	pcre2_code_16*           code = nullptr;	//!< コンパイル済みパターン(このインスタンスが所有)
	bool                     bGlobal = false;
	std::wstring             replacementTemplate;
	std::vector<wchar_t*>    vStartp;
	std::vector<wchar_t*>    vEndp;
	std::unique_ptr<wchar_t[]> outHeap;	//!< outp/outendp が指す実体
};

void SetMsg(wchar_t* msg, const std::wstring& text)
{
	if (!msg) return;
	// 呼び出し側バッファの最小サイズは CBregexp::m_szMsg[80]
	const size_t cap = 79;
	size_t n = (text.size() < cap) ? text.size() : cap;
	for (size_t i = 0; i < n; ++i) msg[i] = text[i];
	msg[n] = L'\0';
}

//! PCRE2のエラーコードを人間可読なメッセージに変換してmsgにセットする
void SetPcre2Error(wchar_t* msg, const wchar_t* prefix, int errorcode)
{
	PCRE2_UCHAR16 buf[120];
	int n = pcre2_get_error_message_16(errorcode, buf, _countof(buf));
	std::wstring text = prefix;
	if (n >= 0) {
		text += reinterpret_cast<wchar_t*>(buf);
	} else {
		text += L"(unknown error)";
	}
	SetMsg(msg, text);
}

//! delimiter の直前が奇数個の連続バックスラッシュでない出現位置を探す
size_t FindDelimiter(const std::wstring& s, wchar_t delim, size_t pos)
{
	for (size_t i = pos; i < s.size(); ++i) {
		if (s[i] != delim) continue;
		size_t nbs = 0;
		size_t j = i;
		while (j > pos && s[j - 1] == L'\\') { ++nbs; --j; }
		if ((nbs % 2) == 0) return i;
	}
	return std::wstring::npos;
}

struct ParsedPattern {
	bool bSubst = false;
	std::wstring pattern;
	std::wstring replacement;
	bool bIcase = false;
	bool bGlobal = false;
	bool bExtend = false;
	bool bMultiline = false;
};

//! "m/pattern/flags" "s/pattern/repl/flags" "/pattern/flags" 形式をパースする
//! (BREGEXP.DLLのコマンド文字列書式。CBregexp::MakePatternSub/CRegexKeywordが生成する)
bool ParseCommand(const wchar_t* str, ParsedPattern& out, std::wstring& errMsg)
{
	std::wstring s(str);
	if (s.empty()) {
		errMsg = L"empty regexp";
		return false;
	}

	size_t pos = 0;
	bool bSubst = false;
	if (s[0] == L's') {
		bSubst = true;
		pos = 1;
	} else if (s[0] == L'm') {
		bSubst = false;
		pos = 1;
	}
	// それ以外は delimiter 文字そのものから始まる省略形(検索扱い)

	if (pos >= s.size()) {
		errMsg = L"malformed regexp";
		return false;
	}
	const wchar_t delim = s[pos];
	++pos;

	const size_t patEnd = FindDelimiter(s, delim, pos);
	if (patEnd == std::wstring::npos) {
		errMsg = L"malformed regexp (missing delimiter)";
		return false;
	}
	std::wstring pattern = s.substr(pos, patEnd - pos);

	size_t flagsStart;
	std::wstring replacement;
	if (bSubst) {
		const size_t repStart = patEnd + 1;
		const size_t repEnd = FindDelimiter(s, delim, repStart);
		if (repEnd == std::wstring::npos) {
			errMsg = L"malformed regexp (missing delimiter)";
			return false;
		}
		replacement = s.substr(repStart, repEnd - repStart);
		flagsStart = repEnd + 1;
	} else {
		flagsStart = patEnd + 1;
	}

	const std::wstring flags = (flagsStart <= s.size()) ? s.substr(flagsStart) : L"";

	out.bSubst = bSubst;
	out.pattern = std::move(pattern);
	out.replacement = std::move(replacement);
	out.bIcase = flags.find(L'i') != std::wstring::npos;
	out.bGlobal = flags.find(L'g') != std::wstring::npos;
	out.bExtend = flags.find(L'x') != std::wstring::npos;
	out.bMultiline = flags.find(L'm') != std::wstring::npos;
	// k(漢字対応)/a/u/d/l(文字集合)/R(CRLF)は受理するが no-op
	// (PCRE2は既定でUnicode対応するため、a/u/d/lの違いを作り分ける必要性が薄い)
	return true;
}

//! コンパイル。失敗時は false を返し errMsg にメッセージをセットする(*outCodeはnullptrのまま)
bool CompilePattern(const ParsedPattern& pp, pcre2_code_16*& outCode, std::wstring& errMsg)
{
	// 20260720 bregonig(Oniguruma系)同様、\w等をUnicode基準にするためPCRE2_UCPを既定で有効化。
	uint32_t options = PCRE2_UTF | PCRE2_UCP;
	if (pp.bIcase) options |= PCRE2_CASELESS;
	if (pp.bExtend) options |= PCRE2_EXTENDED;
	if (pp.bMultiline) options |= PCRE2_MULTILINE;

	int errorcode;
	PCRE2_SIZE erroroffset;
	outCode = pcre2_compile_16(
		reinterpret_cast<PCRE2_SPTR16>(pp.pattern.c_str()), PCRE2_ZERO_TERMINATED,
		options, &errorcode, &erroroffset, nullptr);
	if (outCode == nullptr) {
		PCRE2_UCHAR16 buf[120];
		int n = pcre2_get_error_message_16(errorcode, buf, _countof(buf));
		errMsg = L"regex compile error: ";
		if (n >= 0) errMsg += reinterpret_cast<wchar_t*>(buf);
		return false;
	}
	return true;
}

//! Match: startp/endp/nparens を fb に書き込む(既存インスタンスを使い回せる)
int DoMatch(BREGEXP_W_Fallback* fb, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, wchar_t* msg)
{
	PCRE2_SPTR16 subject = reinterpret_cast<PCRE2_SPTR16>(targetbeg);
	PCRE2_SIZE subjectLen = static_cast<PCRE2_SIZE>(targetendp - targetbeg);
	PCRE2_SIZE startOffset = static_cast<PCRE2_SIZE>(target - targetbeg);

	pcre2_match_data_16* md = pcre2_match_data_create_from_pattern_16(fb->code, nullptr);
	if (!md) {
		SetMsg(msg, L"regex match: out of memory");
		return -1;
	}

	// targetbeg基準のオフセットとして検索開始位置を渡せるため、
	// ルックビハインドや\bがtargetbeg基準で正しく効く。
	int rc = pcre2_match_16(fb->code, subject, subjectLen, startOffset, 0, md, nullptr);

	if (rc == PCRE2_ERROR_NOMATCH) {
		pcre2_match_data_free_16(md);
		return 0;
	}
	if (rc < 0) {
		SetPcre2Error(msg, L"regex match error: ", rc);
		pcre2_match_data_free_16(md);
		return -1;
	}

	PCRE2_SIZE* ovector = pcre2_get_ovector_pointer_16(md);
	uint32_t ovcount = pcre2_get_ovector_count_16(md);
	// rc==0はovectorが不足していたことを示すが、match_dataはパターンから
	// 自動サイズしているので通常発生しない。念のため全件分を使う。
	uint32_t n = (rc == 0) ? ovcount : static_cast<uint32_t>(rc);

	fb->vStartp.assign(n, nullptr);
	fb->vEndp.assign(n, nullptr);
	for (uint32_t i = 0; i < n; ++i) {
		PCRE2_SIZE s = ovector[2 * i];
		PCRE2_SIZE e = ovector[2 * i + 1];
		if (s == PCRE2_UNSET || e == PCRE2_UNSET) continue;	// マッチしなかった任意グループ
		fb->vStartp[i] = const_cast<wchar_t*>(targetbeg) + s;
		fb->vEndp[i]   = const_cast<wchar_t*>(targetbeg) + e;
	}
	fb->startp = fb->vStartp.data();
	fb->endp   = fb->vEndp.data();
	fb->nparens = static_cast<int>(n - 1);

	pcre2_match_data_free_16(md);
	return 1;
}

//! Subst: 置換結果を持つ新しい BREGEXP_W_Fallback を作って返す(outp/outendpがconstのため)。
//! codeの所有権はこの関数では取らない(呼び出し元のBSubstExが管理する)。
BREGEXP_W_Fallback* DoSubst(pcre2_code_16* code, bool bGlobal, const std::wstring& replacementTemplate,
	const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp,
	int& count, wchar_t* msg)
{
	count = 0;
	PCRE2_SPTR16 subject = reinterpret_cast<PCRE2_SPTR16>(targetbeg);
	PCRE2_SIZE subjectLen = static_cast<PCRE2_SIZE>(targetendp - targetbeg);
	PCRE2_SIZE startOffset = static_cast<PCRE2_SIZE>(target - targetbeg);

	// 20260720 BREGEXP仕様: Subst後もGetIndex()/GetMatchLen()は最初の一致位置を指す
	// (CDocOutline.cppの "GetString() = ..." コメントの実例に合わせる必要があるため、
	//  置換前に一致位置を確保しておく)。pcre2_substituteにはPCRE2_SUBSTITUTE_MATCHEDで
	// この結果をそのまま渡し、内部で二重にマッチングさせない。
	pcre2_match_data_16* md = pcre2_match_data_create_from_pattern_16(code, nullptr);
	if (!md) {
		SetMsg(msg, L"regex substitute: out of memory");
		return nullptr;
	}
	int mrc = pcre2_match_16(code, subject, subjectLen, startOffset, 0, md, nullptr);
	if (mrc == PCRE2_ERROR_NOMATCH) {
		pcre2_match_data_free_16(md);
		return new BREGEXP_W_Fallback();	// マッチ無し。置換0件、outp=NULL(エラーではない)
	}
	if (mrc < 0) {
		SetPcre2Error(msg, L"regex match error: ", mrc);
		pcre2_match_data_free_16(md);
		return nullptr;
	}

	PCRE2_SIZE* ovector = pcre2_get_ovector_pointer_16(md);
	uint32_t ovcount = pcre2_get_ovector_count_16(md);
	uint32_t n = (mrc == 0) ? ovcount : static_cast<uint32_t>(mrc);
	std::vector<wchar_t*> vStartp(n, nullptr), vEndp(n, nullptr);
	for (uint32_t i = 0; i < n; ++i) {
		PCRE2_SIZE s = ovector[2 * i];
		PCRE2_SIZE e = ovector[2 * i + 1];
		if (s == PCRE2_UNSET || e == PCRE2_UNSET) continue;
		vStartp[i] = const_cast<wchar_t*>(targetbeg) + s;
		vEndp[i]   = const_cast<wchar_t*>(targetbeg) + e;
	}

	uint32_t options = PCRE2_SUBSTITUTE_OVERFLOW_LENGTH | PCRE2_SUBSTITUTE_MATCHED;
	if (bGlobal) options |= PCRE2_SUBSTITUTE_GLOBAL;
	PCRE2_SPTR16 repl = reinterpret_cast<PCRE2_SPTR16>(replacementTemplate.c_str());
	PCRE2_SIZE replLen = static_cast<PCRE2_SIZE>(replacementTemplate.size());

	// 出力バッファはPCRE2が必要サイズを教えてくれるので、まず見積もりで試し、
	// 不足すればPCRE2_ERROR_NOMEMORYと共に教えられた正確なサイズで再試行する。
	PCRE2_SIZE outLen = subjectLen + replLen + 64;
	std::unique_ptr<wchar_t[]> buf(new wchar_t[outLen]);
	int rc = pcre2_substitute_16(code, subject, subjectLen, startOffset, options, md, nullptr,
		repl, replLen, reinterpret_cast<PCRE2_UCHAR16*>(buf.get()), &outLen);
	if (rc == PCRE2_ERROR_NOMEMORY) {
		// outLenに必要な文字数(終端\0含む)が入っている
		buf.reset(new wchar_t[outLen]);
		rc = pcre2_substitute_16(code, subject, subjectLen, startOffset, options, md, nullptr,
			repl, replLen, reinterpret_cast<PCRE2_UCHAR16*>(buf.get()), &outLen);
	}
	pcre2_match_data_free_16(md);

	if (rc < 0) {
		SetPcre2Error(msg, L"regex substitute error: ", rc);
		return nullptr;
	}

	count = rc;
	BREGEXP_W_Fallback* result = new BREGEXP_W_Fallback(std::move(buf), outLen);
	result->vStartp = std::move(vStartp);
	result->vEndp = std::move(vEndp);
	result->startp = result->vStartp.data();
	result->endp   = result->vEndp.data();
	result->nparens = static_cast<int>(n - 1);
	return result;
}

} // 無名namespace


int BMatchEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	if (msg) msg[0] = L'\0';

	BREGEXP_W_Fallback* fb;
	if (str != nullptr) {
		ParsedPattern pp;
		std::wstring errMsg;
		if (!ParseCommand(str, pp, errMsg)) {
			SetMsg(msg, errMsg);
			return -1;
		}
		pcre2_code_16* code = nullptr;
		if (!CompilePattern(pp, code, errMsg)) {
			SetMsg(msg, errMsg);
			return -1;
		}
		fb = new BREGEXP_W_Fallback();
		fb->code = code;
		fb->bGlobal = pp.bGlobal;
		*rxp = fb;
	} else {
		fb = static_cast<BREGEXP_W_Fallback*>(*rxp);
	}
	return DoMatch(fb, targetbeg, target, targetendp, msg);
}

int BMatch(const wchar_t* str, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	return BMatchEx(str, target, target, targetendp, rxp, msg);
}

int BSubstEx(const wchar_t* str, const wchar_t* targetbeg, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	if (msg) msg[0] = L'\0';

	// 新規コンパイル(str!=NULL)ならcode/bGlobal/replacementTemplateを作る。
	// 再利用(str==NULL)なら直前のインスタンスから引き継ぐ。
	pcre2_code_16* code = nullptr;
	bool bGlobal = false;
	std::wstring replacementTemplate;
	BREGEXP_W_Fallback* old = nullptr;

	if (str != nullptr) {
		ParsedPattern pp;
		std::wstring errMsg;
		if (!ParseCommand(str, pp, errMsg)) {
			SetMsg(msg, errMsg);
			return -1;
		}
		if (!CompilePattern(pp, code, errMsg)) {
			SetMsg(msg, errMsg);
			return -1;
		}
		bGlobal = pp.bGlobal;
		replacementTemplate = pp.replacement;
	} else {
		old = static_cast<BREGEXP_W_Fallback*>(*rxp);
		code = old->code;
		bGlobal = old->bGlobal;
		replacementTemplate = old->replacementTemplate;
	}

	int count = 0;
	BREGEXP_W_Fallback* nfb = DoSubst(code, bGlobal, replacementTemplate, targetbeg, target, targetendp, count, msg);
	if (nfb == nullptr) {
		if (old == nullptr) pcre2_code_free_16(code);	// 新規コンパイル分の後始末(このパスでのみ自分が所有者)
		return -1;
	}
	nfb->code = code;	// 所有権をnfbへ移す
	nfb->bGlobal = bGlobal;
	nfb->replacementTemplate = replacementTemplate;

	if (old != nullptr) {
		old->code = nullptr;	// 所有権はnfbへ移ったので二重解放を防ぐ
		delete old;
	}
	*rxp = nfb;
	return count;
}

int BSubst(const wchar_t* str, const wchar_t* target, const wchar_t* targetendp, BREGEXP_W** rxp, wchar_t* msg)
{
	return BSubstEx(str, target, target, targetendp, rxp, msg);
}

int BTrans(const wchar_t*, wchar_t*, wchar_t*, BREGEXP_W**, wchar_t* msg)
{
	SetMsg(msg, L"BTrans is not supported by the fallback regexp engine");
	return -1;
}

int BSplit(const wchar_t*, wchar_t*, wchar_t*, int, BREGEXP_W**, wchar_t* msg)
{
	SetMsg(msg, L"BSplit is not supported by the fallback regexp engine");
	return -1;
}

void BRegfree(BREGEXP_W* rx)
{
	delete static_cast<BREGEXP_W_Fallback*>(rx);
}

const wchar_t* BRegexpVersion()
{
	static std::wstring s;
	if (s.empty()) {
		PCRE2_UCHAR16 verbuf[64];
		int n = pcre2_config_16(PCRE2_CONFIG_VERSION, verbuf);
		s = L"PCRE2 ";
		if (n >= 0) s += reinterpret_cast<wchar_t*>(verbuf);
		s += L" (fallback: bregonig.dll not found)";
	}
	return s.c_str();
}

} // namespace RegexFallback

#endif // NKMM_FIX_REGEXP_FALLBACK
/*[EOF]*/
