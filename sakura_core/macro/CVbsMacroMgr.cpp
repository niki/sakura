/*!	@file
	@brief VBScript風マクロ(.vbs)エンジン

	@date 2026 NKMM_FIX_VBS_MACRO
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_VBS_MACRO

#include "macro/CVbsMacroMgr.h"
#include "macro/CVbsToJsTranspiler.h"
#include "macro/CMacroFactory.h"
#include "io/CTextStream.h"
#include <string>
#include <stdexcept>

namespace {

//	std::wstring(UTF-16) → UTF-8変換。CVbsToJsTranspilerの入出力はUTF-8前提のため。
std::string WideToUtf8(const std::wstring& src)
{
	if( src.empty() ) return std::string();
	int nLen = ::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), (int)src.size(), NULL, 0, NULL, NULL);
	std::string dst(nLen, '\0');
	if( 0 < nLen ){
		::WideCharToMultiByte(CP_UTF8, 0, src.c_str(), (int)src.size(), &dst[0], nLen, NULL, NULL);
	}
	return dst;
}

//	UTF-8(std::string) → std::wstring(UTF-16)変換
std::wstring Utf8ToWide(const std::string& src)
{
	if( src.empty() ) return std::wstring();
	int nLen = ::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), (int)src.size(), NULL, 0);
	std::wstring dst(nLen, L'\0');
	if( 0 < nLen ){
		::MultiByteToWideChar(CP_UTF8, 0, src.c_str(), (int)src.size(), &dst[0], nLen);
	}
	return dst;
}

//	VBScript標準ライブラリ相当の組み込み関数群。CVbsToJsTranspiler::ToJsCallName
//	により、"S_"接頭辞を持たない識別子はすべてこのランタイム側の実装として
//	解決される前提で変換されるため、ここで定義しておく必要がある。
//
//	MsgBox/InputBoxはサクラエディタ本来の同名マクロ関数(CQuickJSIfObjBinderが
//	グローバルへ登録するInputBox/MessageBox)と引数の意味が異なる
//	(VBScript版MsgBoxはPrompt/Buttons/Title、サクラ版MessageBoxはMsg/Flagsのみ。
//	VBScript版InputBoxはPrompt/Title/Default、サクラ版InputBoxはPrompt/Default/Flags)。
//	そのため、ここでVBScript本来の引数順を持つ同名関数を定義し、サクラ本来の
//	実装は"Editor."修飾(S_接頭辞から変換される、CQuickJSIfObjBinder::BindObjectが
//	常に作る"Editor"オブジェクト経由)でのみ呼べるようにする。
//	Array/StringはJSの同名グローバルと衝突するため、CVbsToJsTranspiler::ToJsCallName
//	側で呼び出し時点から__vbsArray/__vbsStringRepeatへ読み替えている。
const wchar_t* const VBS_RUNTIME_PRELUDE =
	L"var vbCrLf = '\\r\\n', vbCr = '\\r', vbLf = '\\n', vbNewLine = '\\r\\n', vbTab = '\\t', vbNullString = '', vbNullChar = '\\u0000';\n"
	L"function __vbsStr(x) { if (x === undefined || x === null) return ''; return String(x); }\n"
	L"function __vbsArray() { return Array.prototype.slice.call(arguments); }\n"
	L"function __vbsStringRepeat(n, ch) { return String(ch === undefined ? ' ' : ch).repeat(Math.max(0, Math.trunc(n))); }\n"
	L"function __vbsReDimPreserve(oldArr, newSize) { var na = new Array(newSize).fill(undefined); var n = Math.min(oldArr ? oldArr.length : 0, newSize); for (var i = 0; i < n; i++) na[i] = oldArr[i]; return na; }\n"
	L"function __vbsXor(a, b) { return (!!a) !== (!!b); }\n"
	L"function Chr(n) { return String.fromCharCode(Math.trunc(n)); }\n"
	L"function Asc(s) { return String(s).charCodeAt(0); }\n"
	L"function Timer() { var d = new Date(); return d.getHours() * 3600 + d.getMinutes() * 60 + d.getSeconds() + d.getMilliseconds() / 1000; }\n"
	L"function CStr(x) { return __vbsStr(x); }\n"
	L"function CInt(x) { var n = Number(x); return isNaN(n) ? 0 : Math.round(n); }\n"
	L"function CLng(x) { return CInt(x); }\n"
	L"function CDbl(x) { var n = Number(x); return isNaN(n) ? 0 : n; }\n"
	L"function CSng(x) { return CDbl(x); }\n"
	L"function CBool(x) { if (typeof x === 'boolean') return x; if (typeof x === 'string') { var t = x.trim().toLowerCase(); if (t === 'true') return true; if (t === 'false') return false; return Number(t) !== 0; } return Number(x) !== 0; }\n"
	L"function Len(x) { return Array.isArray(x) ? x.length : String(x).length; }\n"
	L"function Mid(s, start, len) { s = String(s); var i = Math.trunc(start) - 1; if (i < 0) i = 0; return (len === undefined) ? s.substr(i) : s.substr(i, Math.trunc(len)); }\n"
	L"function Left(s, n) { return String(s).substr(0, Math.trunc(n)); }\n"
	L"function Right(s, n) { s = String(s); n = Math.trunc(n); return n <= 0 ? '' : s.substr(Math.max(0, s.length - n)); }\n"
	L"function InStr(a, b, c) { if (arguments.length <= 2) { return String(a).indexOf(String(b)) + 1; } var start = Math.trunc(a) - 1; if (start < 0) start = 0; return String(b).indexOf(String(c), start) + 1; }\n"
	L"function InStrRev(s, find, start) { s = String(s); find = String(find); var from = (start === undefined) ? s.length : Math.trunc(start); return s.lastIndexOf(find, from - 1) + 1; }\n"
	L"function Replace(s, find, repl) { return String(s).split(String(find)).join(String(repl)); }\n"
	L"function UCase(s) { return String(s).toUpperCase(); }\n"
	L"function LCase(s) { return String(s).toLowerCase(); }\n"
	L"function Trim(s) { return String(s).trim(); }\n"
	L"function LTrim(s) { return String(s).replace(/^\\s+/, ''); }\n"
	L"function RTrim(s) { return String(s).replace(/\\s+$/, ''); }\n"
	L"function Space(n) { return ' '.repeat(Math.max(0, Math.trunc(n))); }\n"
	L"function IsEmpty(x) { return x === undefined; }\n"
	L"function IsNull(x) { return x === null; }\n"
	L"function IsArray(x) { return Array.isArray(x); }\n"
	L"function IsNumeric(x) { if (x === null || x === undefined || x === '') return false; return !isNaN(Number(x)); }\n"
	L"function TypeName(x) { if (x === null) return 'Object'; if (x === undefined) return 'Empty'; if (Array.isArray(x)) return 'Variant()'; if (typeof x === 'string') return 'String'; if (typeof x === 'boolean') return 'Boolean'; if (typeof x === 'number') return Number.isInteger(x) ? 'Integer' : 'Double'; return 'Object'; }\n"
	L"function Abs(x) { return Math.abs(Number(x)); }\n"
	L"function Int(x) { return Math.floor(Number(x)); }\n"
	L"function Fix(x) { return Math.trunc(Number(x)); }\n"
	L"function Sgn(x) { x = Number(x); return x > 0 ? 1 : (x < 0 ? -1 : 0); }\n"
	L"function Sqr(x) { return Math.sqrt(Number(x)); }\n"
	L"function Rnd() { return Math.random(); }\n"
	L"function Randomize() { }\n" // JSのMath.random()は再シード不可のため何もしない(構文の受理のみ)
	L"function Exp(x) { return Math.exp(Number(x)); }\n"
	L"function Log(x) { return Math.log(Number(x)); }\n"
	L"function Sin(x) { return Math.sin(Number(x)); }\n"
	L"function Cos(x) { return Math.cos(Number(x)); }\n"
	L"function Tan(x) { return Math.tan(Number(x)); }\n"
	L"function Atn(x) { return Math.atan(Number(x)); }\n"
	L"function Hex(n) { return (Math.trunc(n) >>> 0).toString(16).toUpperCase(); }\n"
	L"function Oct(n) { return (Math.trunc(n) >>> 0).toString(8); }\n"
	L"function CByte(x) { var n = Math.round(Number(x)); if (isNaN(n)) n = 0; return Math.max(0, Math.min(255, n)); }\n"
	L"function IsDate(x) { if (x instanceof Date) return true; return typeof x === 'string' && x !== '' && !isNaN(Date.parse(x)); }\n"
	// COM/ActiveXオブジェクトを扱わないサブセットのため、IsObjectは常にfalseを返すスタブ
	L"function IsObject(x) { return false; }\n"
	L"function StrReverse(s) { return String(s).split('').reverse().join(''); }\n"
	L"function StrComp(a, b, compareType) { a = String(a); b = String(b); if (compareType) { a = a.toLowerCase(); b = b.toLowerCase(); } if (a < b) return -1; if (a > b) return 1; return 0; }\n"
	// Split/Joinの戻り値/引数は配列だが、m_arrayNamesによる静的な配列名追跡の
	// 対象外(Dim/ReDimで宣言された名前のみ追跡するため)。Split結果を直接
	// s(0)のように添字アクセスすると関数呼び出しと誤認識される点に注意
	// (For Each等、添字を使わない参照は問題なく動作する)
	L"function Split(s, delim, limit) { s = String(s); delim = (delim === undefined) ? ' ' : String(delim); var parts = delim === '' ? [s] : s.split(delim); if (limit !== undefined && limit >= 0 && parts.length > limit) { var head = parts.slice(0, limit - 1); head.push(parts.slice(limit - 1).join(delim)); parts = head; } return parts; }\n"
	L"function Join(arr, delim) { delim = (delim === undefined) ? ' ' : String(delim); return (arr || []).join(delim); }\n"
	// LBound/UBoundは本エンジンが対応する1・2次元配列(ネストしたJS配列)のみを想定
	L"function LBound(arr, dim) { return 0; }\n"
	L"function UBound(arr, dim) { dim = (dim === undefined) ? 1 : Math.trunc(dim); var a = arr; for (var i = 1; i < dim; i++) { a = a[0]; } return (a ? a.length : 0) - 1; }\n"
	L"function MsgBox(prompt, buttons, title) { return Editor.MessageBox(String(prompt), buttons === undefined ? 0 : buttons); }\n"
	L"function InputBox(prompt, title, defaultVal) { return Editor.InputBox(String(prompt), defaultVal === undefined ? '' : defaultVal, 0); }\n";

//	トランスパイルエラーのメッセージをJS文字列リテラルへ埋め込めるようエスケープする
std::string EscapeForJsStringLiteral(const std::string& src)
{
	std::string dst;
	for( char c : src ){
		if( c == '\\' || c == '"' ) dst += '\\';
		if( c == '\n' ){ dst += "\\n"; continue; }
		if( c == '\r' ) continue;
		dst += c;
	}
	return dst;
}

//	VBScript風ソース(UTF-16)をJavaScriptソース(UTF-16)へ変換する
std::wstring TranspileToJs(const std::wstring& sVbsSource)
{
	CVbsToJsTranspiler transpiler;
	try {
		std::string sUtf8Js = transpiler.transpile(WideToUtf8(sVbsSource));
		return VBS_RUNTIME_PRELUDE + Utf8ToWide(sUtf8Js);
	} catch( const std::exception& e ){
		//	構文エラーを、有効なJSの例外送出コードへ差し替える。[[CPasMacroMgr.cpp]]
		//	のTranspileToJsと同じ設計(LoadKeyMacro自体は成功扱いのまま、実際の
		//	エラー表示はCQuickJSMacroMgr::ExecKeyMacroが持つ既存のJS例外表示処理に乗せる)
		std::string sMsg = "VBScript macro syntax error: " + EscapeForJsStringLiteral(e.what());
		return Utf8ToWide("throw new Error(\"" + sMsg + "\");\n");
	}
}

} // namespace

/*!	VBScript風マクロの読み込み(ファイルから)
*/
BOOL CVbsMacroMgr::LoadKeyMacro( HINSTANCE /*hInstance*/, const TCHAR* pszPath )
{
	CTextInputStream in(pszPath);
	if( !in ) return FALSE;

	std::wstring sVbsSource;
	while( in ){
		sVbsSource += in.ReadLineW() + L"\r\n";
	}

	m_Source = TranspileToJs(sVbsSource);
	return TRUE;
}

/*!	VBScript風マクロの読み込み(文字列から)
*/
BOOL CVbsMacroMgr::LoadKeyMacroStr( HINSTANCE /*hInstance*/, const TCHAR* pszCode )
{
	m_Source = TranspileToJs(to_wchar(pszCode));
	return TRUE;
}

/*!	Factory

	拡張子"vbs"のときだけ自身を生成する(CQuickJSMacroMgr::Creatorと同じ、
	レジストリを引かない固定拡張子判定)。
*/
CMacroManagerBase* CVbsMacroMgr::Creator(const TCHAR* FileExt)
{
	if( _tcscmp( FileExt, _T("vbs") ) == 0 ){
		return new CVbsMacroMgr;
	}
	return NULL;
}

void CVbsMacroMgr::declare()
{
	CMacroFactory::getInstance()->RegisterCreator(Creator);
}

#endif // NKMM_FIX_VBS_MACRO
/*[EOF]*/
