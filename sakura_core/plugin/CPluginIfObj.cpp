/*!	@file
	@brief Pluginオブジェクト

	CPluginIfObj::m_MacroFuncInfoCommandArr/m_MacroFuncInfoArrの定義本体。
	以前はCPluginIfObj.h(ヘッダ)内で直接定義していたが、CWSHPlugin.cppに加えて
	CQuickJSPlugin.cppもCPluginIfObj.hをincludeするようになったため、複数の翻訳単位で
	同じ非inlineクラス静的メンバを定義することになりODR違反(LNK2005多重定義)になった。
	通常の.cppへ定義を切り出すことで解決する(NKMM_FIX_QUICKJS_MACRO)。

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#include "StdAfx.h"
#include "plugin/CPluginIfObj.h"

//コマンド情報
MacroFuncInfo CPluginIfObj::m_MacroFuncInfoCommandArr[] =
{
	//ID									関数名							引数										戻り値の型	m_pszData
	{EFunctionCode(F_PL_SETOPTION),			LTEXT("SetOption"),				{VT_BSTR, VT_BSTR, VT_VARIANT, VT_EMPTY},	VT_EMPTY,	NULL }, //オプションファイルに値を書く
	{EFunctionCode(F_PL_ADDCOMMAND),		LTEXT("AddCommand"),			{VT_BSTR, VT_BSTR, VT_BSTR, VT_EMPTY},		VT_EMPTY,	NULL }, //コマンドを追加する
	//	終端
	{F_INVALID,	NULL, {VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}
};

//関数情報
MacroFuncInfo CPluginIfObj::m_MacroFuncInfoArr[] =
{
	//ID									関数名							引数										戻り値の型	m_pszData
	{EFunctionCode(F_PL_GETPLUGINDIR),		LTEXT("GetPluginDir"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //プラグインフォルダパスを取得する
	{EFunctionCode(F_PL_GETDEF),			LTEXT("GetDef"),				{VT_BSTR, VT_BSTR, VT_EMPTY, VT_EMPTY},		VT_BSTR,	NULL }, //設定ファイルから値を読む
	{EFunctionCode(F_PL_GETOPTION),			LTEXT("GetOption"),				{VT_BSTR, VT_BSTR, VT_EMPTY, VT_EMPTY},		VT_BSTR,	NULL }, //オプションファイルから値を読む
	{EFunctionCode(F_PL_GETCOMMANDNO),		LTEXT("GetCommandNo"),			{VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_I4,		NULL }, //オプションファイルから値を読む
	{EFunctionCode(F_PL_GETSTRING),			LTEXT("GetString"),				{VT_I4,    VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_BSTR,	NULL }, //設定ファイルから文字列を読む
	//	終端
	{F_INVALID,	NULL, {VT_EMPTY, VT_EMPTY, VT_EMPTY, VT_EMPTY},	VT_EMPTY,	NULL}
};
/*[EOF]*/
