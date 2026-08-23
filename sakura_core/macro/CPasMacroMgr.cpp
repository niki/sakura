/*!	@file
	@brief Pascal風マクロ(.pas)エンジン

	@date 2026 NKMM_FIX_PASCAL_MACRO
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_PASCAL_MACRO

#include "macro/CPasMacroMgr.h"
#include "macro/CPasToJsTranspiler.h"
#include "macro/CMacroFactory.h"
#include "io/CTextStream.h"
#include <string>
#include <stdexcept>

namespace {

//	std::wstring(UTF-16) → UTF-8変換。CPasToJsTranspilerの入出力はUTF-8前提のため。
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

//	PPA言語自体が提供していたと思われるPascal/Delphi標準ライブラリ相当の組み込み
//	関数群。CPasToJsTranspiler::ToJsCallNameにより、"S_"接頭辞を持たない識別子は
//	すべてこのランタイム側の実装として解決される前提で変換されるため、ここで
//	定義しておく必要がある。
//
//	InputBox/MessageBoxはサクラエディタ本来の同名マクロ関数(CQuickJSIfObjBinderが
//	グローバルへ登録するもの)と名前が衝突するが、引数の意味が異なる
//	(PPA版InputBoxはTitle/Prompt/Default、サクラ版はPrompt/Default/Flags。
//	PPA版MessageBoxはMsg/Title/Flags、サクラ版はMsg/Flagsのみでtitleは無い)。
//	そのため、ここでグローバルの同名関数を意図的に上書きし、サクラ本来の実装は
//	"Editor."修飾(S_接頭辞から変換される、CQuickJSIfObjBinder::BindObjectが
//	常に作る"Editor"オブジェクト経由)でのみ呼べるようにする。
//	Copyも同様にサクラの「選択範囲をクリップボードにコピーする」コマンドと
//	名前が衝突するため、無印のCopyはPascal標準の文字列関数として上書きする
//	(クリップボードコピーを呼びたい場合はS_Copy()と書けばEditor.Copy()になる)。
const wchar_t* const PAS_RUNTIME_PRELUDE =
	L"function StrToInt(s) { var n = parseInt(String(s), 10); return isNaN(n) ? 0 : n; }\n"
	L"function IntToStr(n) { return String(Math.trunc(n)); }\n"
	L"function Copy(s, start, len) { s = String(s); var i = Math.trunc(start) - 1; if (i < 0) i = 0; return (len === undefined) ? s.substr(i) : s.substr(i, Math.trunc(len)); }\n"
	L"function Trunc(x) { return Math.trunc(x); }\n"
	L"function Frac(x) { return x - Math.trunc(x); }\n"
	L"function FloatToStr(x) { return String(x); }\n"
	L"function InputBox(title, prompt, defaultVal) { return Editor.InputBox(prompt, defaultVal, 0); }\n"
	L"function MessageBox(msg, title, flags) { return Editor.MessageBox(msg, flags); }\n"
	L"function Length(s) { return String(s).length; }\n"
	L"function Pos(needle, haystack) { return String(haystack).indexOf(String(needle)) + 1; }\n"
	L"function UpperCase(s) { return String(s).toUpperCase(); }\n"
	L"function LowerCase(s) { return String(s).toLowerCase(); }\n"
	L"function Trim(s) { return String(s).trim(); }\n"
	L"function TrimLeft(s) { return String(s).replace(/^\\s+/, ''); }\n"
	L"function TrimRight(s) { return String(s).replace(/\\s+$/, ''); }\n"
	L"function StringReplace(s, oldStr, newStr) { return String(s).split(String(oldStr)).join(String(newStr)); }\n"
	L"function CompareStr(a, b) { a = String(a); b = String(b); if (a < b) return -1; if (a > b) return 1; return 0; }\n"
	L"function Round(x) { return Math.round(x); }\n"
	// PascalのSqrは「2乗」、Sqrtが「平方根」(VBScriptのSqrとは意味が異なるので注意)
	L"function Sqr(x) { return x * x; }\n"
	L"function Sqrt(x) { return Math.sqrt(x); }\n"
	L"function Odd(n) { return (Math.trunc(n) % 2) !== 0; }\n"
	L"function Chr(n) { return String.fromCharCode(Math.trunc(n)); }\n"
	L"function Ord(s) { return String(s).charCodeAt(0); }\n"
	// Random(n)はnを渡すと[0,n)の整数、渡さないと[0,1)の実数(標準Pascalの挙動)
	L"function Random(n) { if (n === undefined) return Math.random(); return Math.floor(Math.random() * Math.trunc(n)); }\n"
	L"function Randomize() { }\n" // JSのMath.random()は再シード不可のため何もしない(構文の受理のみ)
	//	Write/Writelnはコンソールを持たないGUI環境向けに、カーソル位置への
	//	テキスト挿入(Editor.InsText)へ委譲する。Writelnは末尾に改行を足す
	L"function Write(s) { Editor.InsText(String(s === undefined ? '' : s)); }\n"
	L"function Writeln(s) { Editor.InsText(String(s === undefined ? '' : s) + '\\r\\n'); }\n";

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

//	Pascal風ソース(UTF-16)をJavaScriptソース(UTF-16)へ変換する
std::wstring TranspileToJs(const std::wstring& sPasSource)
{
	CPasToJsTranspiler transpiler;
	try {
		std::string sUtf8Js = transpiler.transpile(WideToUtf8(sPasSource));
		return PAS_RUNTIME_PRELUDE + Utf8ToWide(sUtf8Js);
	} catch( const std::exception& e ){
		//	構文エラーを、有効なJSの例外送出コードへ差し替える。これにより
		//	LoadKeyMacro自体は成功扱いのまま、実際のエラー表示は
		//	CQuickJSMacroMgr::ExecKeyMacroが持つ既存のJS例外表示処理
		//	(ReportQuickJSException、マクロ停止ダイアログと同じ導線)に
		//	そのまま乗せる。専用のエラー表示コードを新設していない
		std::string sMsg = "Pascal macro syntax error: " + EscapeForJsStringLiteral(e.what());
		return Utf8ToWide("throw new Error(\"" + sMsg + "\");\n");
	}
}

} // namespace

/*!	Pascal風マクロの読み込み(ファイルから)

	CQuickJSMacroMgr::LoadKeyMacroと同様にファイル全体を読み込むが、
	そのままm_Sourceへ渡さず、トランスパイルしたJavaScriptを渡す。
	エラーメッセージは出しません。呼び出し側でよきにはからってください。
*/
BOOL CPasMacroMgr::LoadKeyMacro( HINSTANCE /*hInstance*/, const TCHAR* pszPath )
{
	CTextInputStream in(pszPath);
	if( !in ) return FALSE;

	std::wstring sPasSource;
	while( in ){
		sPasSource += in.ReadLineW() + L"\r\n";
	}

	m_Source = TranspileToJs(sPasSource);
	return TRUE;
}

/*!	Pascal風マクロの読み込み(文字列から)
*/
BOOL CPasMacroMgr::LoadKeyMacroStr( HINSTANCE /*hInstance*/, const TCHAR* pszCode )
{
	m_Source = TranspileToJs(to_wchar(pszCode));
	return TRUE;
}

/*!	Factory

	拡張子"pas"のときだけ自身を生成する(CQuickJSMacroMgr::Creatorと同じ、
	レジストリを引かない固定拡張子判定)。
*/
CMacroManagerBase* CPasMacroMgr::Creator(const TCHAR* FileExt)
{
	if( _tcscmp( FileExt, _T("pas") ) == 0 ){
		return new CPasMacroMgr;
	}
	return NULL;
}

void CPasMacroMgr::declare()
{
	CMacroFactory::getInstance()->RegisterCreator(Creator);
}

#endif // NKMM_FIX_PASCAL_MACRO
/*[EOF]*/
