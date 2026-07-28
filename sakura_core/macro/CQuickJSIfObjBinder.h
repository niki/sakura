/*!	@file
	@brief QuickJS用インタフェースオブジェクトバインダ

	CWSHIfObj派生オブジェクト(CEditorIfObj等)が持つMacroFuncInfoテーブルと
	HandleCommand/HandleFunction実装を、IDispatch/COMを経由せずQuickJSの
	グローバルオブジェクトへ橋渡しする。CWSHIfObj側の実装は一切変更しない。

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#ifndef SAKURA_CQUICKJSIFOBJBINDER_2A6F9B3E_9C7E_4C33_9C7B_2C6B9B8B7C2A_H_
#define SAKURA_CQUICKJSIFOBJBINDER_2A6F9B3E_9C7E_4C33_9C7B_2C6B9B8B7C2A_H_

#ifdef NKMM_FIX_QUICKJS_MACRO

#include <vector>
#include <string>
#include "quickjs.h"
#include "func/Funccode.h" // EFunctionCode

class CWSHIfObj;
class CEditView;
struct MacroFuncInfo;
typedef MacroFuncInfo* MacroFuncInfoArray;

/*!	JS文字列をwstringへ変換する。

	quickjs.cにサクラエディタ側で追加したJS_ToCStringLenUTF16/JS_FreeCStringUTF16
	(quickjs-ng本体には無い独自追加。UTF-8版の兄弟関数JS_ToCStringLen2と違い、
	戻り値をNUL終端しない・文字列連結由来のスライス文字列の内部ポインタをそのまま
	返すことがある、という2つの不具合を持つ)には頼らず、常に正しく動く
	JS_ToCStringLen2(UTF-8, 常にNUL終端された自分専用バッファを保証)経由で
	MultiByteToWideCharによりUTF-16へ変換する。vendorしたquickjs.c自体は変更しない。
*/
std::wstring JSValueToWString(JSContext* ctx, JSValueConst v);

class CQuickJSIfObjBinder {
public:
	CQuickJSIfObjBinder();
	~CQuickJSIfObjBinder();

	/*!	pObjが持つコマンド/関数群をctxへ登録する。

		pObj->Name()という名前のオブジェクト(例: "Editor")を常にグローバルへ作り、
		その配下へ全メソッドを登録する(Editor.InsText()のような呼び出しが常に
		できるようにするため)。加えてpObjがグローバル(CWSHIfObj::IsGlobal()==true)
		の場合は、WSHのSCRIPTITEM_GLOBALMEMBERS相当として、同じメソッドをグローバル
		オブジェクト直下にも(修飾無しで呼べるよう)登録する。

		1回のスクリプト実行(JSContext)につき1つのバインダインスタンスを使い、
		実行対象の全IfObjをBindObjectで積み重ねてから評価する。
	*/
	bool BindObject(JSContext* ctx, CWSHIfObj* pObj, CEditView* pView, int flags);

private:
	//	1つのスクリプト公開関数に対応する実体情報
	struct SBoundMethod {
		CWSHIfObj*		pObj;
		CEditView*		pView;
		EFunctionCode	id;
	};

	static JSValue Trampoline(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv, int magic);

	//	MacroFuncInfo配列1本ぶんをtargetへ登録する。globalTargetがJS_UNDEFINEDでなければ
	//	同じメソッドをそちらにも(修飾無しで呼べるよう)登録する。
	void RegisterTable(JSContext* ctx, JSValueConst target, JSValueConst globalTarget, CWSHIfObj* pObj, CEditView* pView, MacroFuncInfoArray info, int flagsToOr);

	std::vector<SBoundMethod> m_methods;
};

#endif // NKMM_FIX_QUICKJS_MACRO

#endif /* SAKURA_CQUICKJSIFOBJBINDER_2A6F9B3E_9C7E_4C33_9C7B_2C6B9B8B7C2A_H_ */
/*[EOF]*/
