/*!	@file
	@brief QuickJS用インタフェースオブジェクトバインダ

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_QUICKJS_MACRO

#include "macro/CQuickJSIfObjBinder.h"
#include "macro/CWSHIfObj.h"
#include "macro/CSMacroMgr.h" // MacroFuncInfo
#include "Funccode_enum.h" // EFunctionCode::FA_FROMMACRO
#include <vector>

namespace {

//	JSの値をHandleFunction向けのVARIANTへ変換する。
//	文字列/数値/真偽値以外(オブジェクト等)は文字列化してBSTRにフォールバックする。
void JSArgToVariant(JSContext* ctx, JSValueConst v, VARIANT* pOut)
{
	VariantInit(pOut);
	if( JS_IsUndefined(v) || JS_IsNull(v) ){
		//VT_EMPTYのまま
		return;
	}
	if( JS_IsNumber(v) ){
		double d = 0.0;
		JS_ToFloat64(ctx, &d, v);
		if( d == (double)(int32_t)d ){
			pOut->vt = VT_I4;
			pOut->lVal = (int32_t)d;
		}else{
			pOut->vt = VT_R8;
			pOut->dblVal = d;
		}
		return;
	}
	if( JS_IsBool(v) ){
		pOut->vt = VT_BOOL;
		pOut->boolVal = JS_ToBool(ctx, v) ? VARIANT_TRUE : VARIANT_FALSE;
		return;
	}
	//	文字列、およびそれ以外のオブジェクト等はJS_ToStringで文字列化してから渡す
	JSValue strVal = JS_IsString(v) ? JS_DupValue(ctx, v) : JS_ToString(ctx, v);
	size_t len = 0;
	const uint16_t* pBuf = JS_ToCStringLenUTF16(ctx, &len, strVal);
	pOut->vt = VT_BSTR;
	pOut->bstrVal = ::SysAllocStringLen((const OLECHAR*)pBuf, (UINT)len);
	if( pBuf ) JS_FreeCStringUTF16(ctx, pBuf);
	JS_FreeValue(ctx, strVal);
}

//	HandleFunctionが返したVARIANTをJSValueへ変換する。
JSValue VariantToJSValue(JSContext* ctx, const VARIANT& v)
{
	switch( v.vt ){
	case VT_EMPTY:
	case VT_NULL:
		return JS_UNDEFINED;
	case VT_I2:
		return JS_NewInt32(ctx, v.iVal);
	case VT_I4:
		return JS_NewInt32(ctx, v.lVal);
	case VT_R4:
		return JS_NewFloat64(ctx, v.fltVal);
	case VT_R8:
		return JS_NewFloat64(ctx, v.dblVal);
	case VT_BOOL:
		return JS_NewBool(ctx, v.boolVal != VARIANT_FALSE);
	case VT_BSTR:
		if( v.bstrVal ){
			return JS_NewStringUTF16(ctx, (const uint16_t*)v.bstrVal, ::SysStringLen(v.bstrVal));
		}
		return JS_NULL;
	default:
		return JS_UNDEFINED;
	}
}

} // namespace


CQuickJSIfObjBinder::CQuickJSIfObjBinder()
{
}

CQuickJSIfObjBinder::~CQuickJSIfObjBinder()
{
}

void CQuickJSIfObjBinder::RegisterTable(
	JSContext*			ctx,
	JSValueConst		target,
	JSValueConst		globalTarget,
	CWSHIfObj*			pObj,
	CEditView*			pView,
	MacroFuncInfoArray	info,
	int					flagsToOr
)
{
	for( ; info->m_nFuncID != -1; ++info ){
		int argCount = 0;
		if( info->m_pData ){
			argCount = info->m_pData->m_nArgMinSize;
		}else{
			for( int i = 0; i < 4; ++i ){
				if( info->m_varArguments[i] != VT_EMPTY ) ++argCount;
			}
		}

		SBoundMethod method;
		method.pObj = pObj;
		method.pView = pView;
		method.id = static_cast<EFunctionCode>(info->m_nFuncID | flagsToOr);
		m_methods.push_back(method);
		int magic = (int)m_methods.size() - 1;

		//	関数名はASCII前提(既存マクロ関数名はすべて英数字)なのでそのままナロー変換する
		char szNameA[256];
		int n = 0;
		for( ; info->m_pszFuncName[n] != L'\0' && n < (int)sizeof(szNameA) - 1; ++n ){
			szNameA[n] = (char)info->m_pszFuncName[n];
		}
		szNameA[n] = '\0';

		JSValue fn = JS_NewCFunctionMagic(ctx, &CQuickJSIfObjBinder::Trampoline, szNameA, argCount, JS_CFUNC_generic_magic, magic);
		JS_SetPropertyStr(ctx, target, szNameA, fn);

		//	IsGlobal()のオブジェクトは、WSHのSCRIPTITEM_GLOBALMEMBERS同様
		//	修飾無しでも同じ関数を呼べるようにする(例: Editor.InsText()とInsText()の両方)
		if( !JS_IsUndefined(globalTarget) ){
			JSValue fnGlobal = JS_NewCFunctionMagic(ctx, &CQuickJSIfObjBinder::Trampoline, szNameA, argCount, JS_CFUNC_generic_magic, magic);
			JS_SetPropertyStr(ctx, globalTarget, szNameA, fnGlobal);
		}
	}
}

bool CQuickJSIfObjBinder::BindObject(JSContext* ctx, CWSHIfObj* pObj, CEditView* pView, int flags)
{
	if( !pObj ) return false;

	//	Trampoline(static関数)からこのバインダインスタンスへ戻れるようにする。
	//	1回のスクリプト実行(1JSContext)につきバインダは1つの前提。
	JS_SetContextOpaque(ctx, this);

	JSValue globalObj = JS_GetGlobalObject(ctx);

	//	pObj->Name()(例: "Editor")という名前のオブジェクトは常に作る。
	//	既存マクロはすべて"Editor.InsText()"のように修飾して呼ぶため、これが無いと
	//	ReferenceErrorになる(WSHはAddNamedItemで名前付きアイテムとしても登録される
	//	ため、SCRIPTITEM_GLOBALMEMBERSを指定していても常に"Editor.xxx"で呼べていた)。
	JSValue target = JS_NewObject(ctx);

	char szNameA[64];
	int n = 0;
	for( ; pObj->Name()[n] != L'\0' && n < (int)sizeof(szNameA) - 1; ++n ){
		szNameA[n] = (char)pObj->Name()[n];
	}
	szNameA[n] = '\0';
	JS_SetPropertyStr(ctx, globalObj, szNameA, JS_DupValue(ctx, target));

	JSValueConst globalTarget = pObj->IsGlobal() ? globalObj : JS_UNDEFINED;

	//	 2007.07.20 genta : コマンドに混ぜ込むフラグを渡す(CWSHIfObj::ReadyMethodsと同じ規則)
	RegisterTable(ctx, target, globalTarget, pObj, pView, pObj->InvokeGetMacroCommandInfo(), flags | FA_FROMMACRO);
	RegisterTable(ctx, target, globalTarget, pObj, pView, pObj->InvokeGetMacroFuncInfo(), 0);

	JS_FreeValue(ctx, target);
	JS_FreeValue(ctx, globalObj);
	return true;
}

JSValue CQuickJSIfObjBinder::Trampoline(JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv, int magic)
{
	CQuickJSIfObjBinder* pBinder = (CQuickJSIfObjBinder*)JS_GetContextOpaque(ctx);
	if( !pBinder || magic < 0 || (size_t)magic >= pBinder->m_methods.size() ){
		return JS_UNDEFINED;
	}
	const SBoundMethod& method = pBinder->m_methods[magic];

	if( LOWORD(method.id) >= F_FUNCTION_FIRST ){
		//	関数呼び出し: 引数をVARIANT配列に変換してHandleFunctionへ渡す
		std::vector<VARIANT> args(argc);
		for( int i = 0; i < argc; ++i ){
			JSArgToVariant(ctx, argv[i], &args[i]);
		}

		VARIANT ret;
		VariantInit(&ret);
		method.pObj->InvokeFunction(method.pView, method.id, argc ? &args[0] : NULL, argc, ret);

		JSValue jsRet = VariantToJSValue(ctx, ret);
		VariantClear(&ret);
		for( int i = 0; i < argc; ++i ){
			VariantClear(&args[i]);
		}
		return jsRet;
	}else{
		//	コマンド呼び出し: 全引数を文字列化してHandleCommandへ渡す(戻り値は使わない)
		//	HandleCommand実装はArguments[3]までを無条件参照することがあるため、
		//	CWSHIfObj::MacroCommandと同様に最低4要素は確保してNULL埋めする。
		int argCountMin = t_max(4, argc);
		std::vector<const WCHAR*> strArgs(argCountMin, (const WCHAR*)NULL);
		std::vector<int> strLengths(argCountMin, 0);
		std::vector<const uint16_t*> rawUtf16(argc, (const uint16_t*)NULL);
		std::vector<JSValue> strTemp(argc, JS_UNDEFINED);

		for( int i = 0; i < argc; ++i ){
			strTemp[i] = JS_ToString(ctx, argv[i]);
			size_t len = 0;
			rawUtf16[i] = JS_ToCStringLenUTF16(ctx, &len, strTemp[i]);
			strArgs[i] = (const WCHAR*)rawUtf16[i];
			strLengths[i] = (int)len;
		}

		method.pObj->InvokeCommand(method.pView, method.id, &strArgs[0], &strLengths[0], argc);

		for( int i = 0; i < argc; ++i ){
			if( rawUtf16[i] ) JS_FreeCStringUTF16(ctx, rawUtf16[i]);
			JS_FreeValue(ctx, strTemp[i]);
		}
		return JS_UNDEFINED;
	}
}

#endif // NKMM_FIX_QUICKJS_MACRO
/*[EOF]*/
