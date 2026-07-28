/*!	@file
	@brief QuickJSマクロエンジン

	@date 2026 NKMM_FIX_QUICKJS_MACRO
*/
#include "StdAfx.h"

#ifdef NKMM_FIX_QUICKJS_MACRO

#include <process.h> // _beginthreadex
#include "macro/CQuickJSMacroMgr.h"
#include "macro/CQuickJSIfObjBinder.h"
#include "macro/CEditorIfObj.h"
#include "macro/CMacroFactory.h"
#include "view/CEditView.h"
#include "io/CTextStream.h"
#include "dlg/CDlgCancel.h"
#include "sakura_rc.h"
#include "quickjs.h"
#include <string>

namespace {

//	std::wstring(UTF-16) → UTF-8変換。JS_Evalの入力はUTF-8前提のため。
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

/*!	20260728 実行ファイルサイズ削減のため、JS_NewContext()(全機能を有効化する
	便利関数)の代わりに、必要な機能だけを個別に有効化したJSContextを作る。

	quickjs.cのJS_NewContext()実体(コピー元)は以下の通り:
	  JS_AddIntrinsicBaseObjects, Date, Eval, RegExp, JSON, Proxy,
	  MapSet, TypedArrays, Promise, WeakRef, AToB, Performance

	このうちマクロ用途で省いても実害が薄いと判断したものをコメントアウトして
	いる。元に戻したい場合は、該当行の"//"を外すだけでよい。

	- JS_AddIntrinsicPromise: マクロ実行はJS_Evalでスクリプト全体を1回評価して
	  終了する同期モデルで、ジョブキュー(JS_ExecutePendingJob)を一切ポーリング
	  していないため、Promise/async/awaitは実質動作しない(then/awaitの継続が
	  実行されるタイミングが無い)。有効化するだけではマクロから使えるように
	  ならないので、復活させる場合はジョブキューのポーリングも別途必要。
	- JS_AddIntrinsicProxy: Editor.xxxのバインディング(CQuickJSIfObjBinder)は
	  JS_SetPropertyStrで直接プロパティ登録しており、Proxyには依存していない。
	- JS_AddIntrinsicWeakRef: GCと連携する高度な機能で自動化スクリプトでは
	  まず使われない。
	- JS_AddPerformance(performance.now()): Date.now()で代替可能。

	BaseObjects/Date/Eval/RegExp/JSON/MapSet/TypedArraysはマクロ作者が普通に
	使う可能性が高いため、そのまま残している。
*/
static JSContext* NewMacroContext(JSRuntime* rt)
{
	JSContext* ctx = JS_NewContextRaw(rt);
	if( !ctx ) return NULL;

	if( JS_AddIntrinsicBaseObjects(ctx) ||
		JS_AddIntrinsicDate(ctx) ||
		JS_AddIntrinsicEval(ctx) ||
		JS_AddIntrinsicRegExp(ctx) ||
		JS_AddIntrinsicJSON(ctx) ||
	//	JS_AddIntrinsicProxy(ctx) ||		// 20260728 マクロ用途では未使用のため無効化(復活可)
		JS_AddIntrinsicMapSet(ctx) ||
		JS_AddIntrinsicTypedArrays(ctx)
	//	|| JS_AddIntrinsicPromise(ctx)		// 20260728 ジョブキュー未対応のため無効化(復活可、要ジョブキュー対応)
	//	|| JS_AddIntrinsicWeakRef(ctx)		// 20260728 マクロ用途では未使用のため無効化(復活可)
	){
		JS_FreeContext(ctx);
		return NULL;
	}
	//	JS_AddPerformance(ctx);				// 20260728 Date.now()で代替可能なため無効化(復活可)

	return ctx;
}

//	マクロ停止スレッドへ渡すパラメータ(CWSH.cppのSAbortMacroParamに相当)
struct SQuickJSAbortParam {
	HANDLE			hEvent;
	int				nCancelTimer;
	CEditView*		view;
	volatile bool	bAbort;
};

//	QuickJSがバイトコード実行中に定期的に呼ぶ中断確認コールバック。
//	IActiveScript::InterruptScriptThreadに相当する処理をポーリングで代替する。
int QuickJSInterruptHandler(JSRuntime* /*rt*/, void* opaque)
{
	SQuickJSAbortParam* pParam = (SQuickJSAbortParam*)opaque;
	return pParam->bAbort ? 1 : 0;
}

//	マクロ実行を中止するスレッド(CWSH.cppのAbortMacroProcと同じUI/操作性)
unsigned __stdcall QuickJSAbortMacroProc( LPVOID lpParameter )
{
	SQuickJSAbortParam* pParam = (SQuickJSAbortParam*)lpParameter;

	//停止ダイアログ表示前に数秒待つ
	if( ::WaitForSingleObject(pParam->hEvent, pParam->nCancelTimer * 1000) == WAIT_TIMEOUT ){
		MSG msg;
		CDlgCancel cDlgCancel;
		HWND hwndDlg = cDlgCancel.DoModeless(G_AppInstance(), NULL, IDD_MACRORUNNING);
		::SendMessage(hwndDlg, WM_SETTEXT, 0, (LPARAM)GSTR_APPNAME);
		::SendMessage(GetDlgItem(hwndDlg, IDC_STATIC_CMD),
			WM_SETTEXT, 0, (LPARAM)pParam->view->GetDocument()->m_cDocFile.GetFilePath());

		bool bCanceled = false;
		for(;;){
			DWORD dwResult = MsgWaitForMultipleObjects( 1, &pParam->hEvent, FALSE, INFINITE, QS_ALLINPUT );
			if(dwResult == WAIT_OBJECT_0){
				::SendMessage( cDlgCancel.GetHwnd(), WM_CLOSE, 0, 0 );
			}else if(dwResult == WAIT_OBJECT_0+1){
				while(::PeekMessage(&msg , NULL , 0 , 0, PM_REMOVE )){
					if(cDlgCancel.GetHwnd() != NULL && ::IsDialogMessage(cDlgCancel.GetHwnd(), &msg)){
					}else{
						::TranslateMessage(&msg);
						::DispatchMessage(&msg);
					}
				}
			}else{
				//MsgWaitForMultipleObjectsに与えたハンドルのエラー
				break;
			}
			if(!bCanceled && cDlgCancel.IsCanceled()){
				bCanceled = true;
				cDlgCancel.CloseDialog( 0 );
			}
			if(cDlgCancel.GetHwnd() == NULL){
				break;
			}
		}

		//JS_SetInterruptHandlerが次回のポーリングでこれを見て評価を打ち切る
		if( bCanceled ){
			pParam->bAbort = true;
		}
	}

	return 0;
}

//	JSの例外内容をメッセージボックスで表示する(WSH版のOnScriptError相当)
void ReportQuickJSException(HWND hwndOwner, JSContext* ctx)
{
	JSValue exc = JS_GetException(ctx);

	JSValue msgVal = JS_ToString(ctx, exc);
	std::wstring sMsg = JSValueToWString(ctx, msgVal);
	JS_FreeValue(ctx, msgVal);

	//スタックトレースがあれば付加する
	JSValue stackVal = JS_GetPropertyStr(ctx, exc, "stack");
	if( JS_IsString(stackVal) ){
		std::wstring sStack = JSValueToWString(ctx, stackVal);
		if( !sStack.empty() ){
			sMsg += L"\r\n";
			sMsg += sStack;
		}
	}
	JS_FreeValue(ctx, stackVal);
	JS_FreeValue(ctx, exc);

	::MessageBox(hwndOwner, sMsg.c_str(), L"QuickJS", MB_ICONERROR);
}

} // namespace


CQuickJSMacroMgr::CQuickJSMacroMgr()
{
}

CQuickJSMacroMgr::~CQuickJSMacroMgr()
{
}

/*!	QuickJSマクロの実行

	CWSHMacroManager::ExecKeyMacro(CWSH.cppのCWSHClient::Execute)に相当。
	WSHが実行ごとにIActiveScriptエンジンを作り直すのと同様、ここでも実行のたびに
	使い捨てのJSRuntime/JSContextを作る。
*/
bool CQuickJSMacroMgr::ExecKeyMacro( CEditView* pcEditView, int flags ) const
{
	JSRuntime* rt = JS_NewRuntime();
	if( !rt ) return false;

	//sakura.exeのメインスレッドスタックは8MBへ拡張済み(sakura.vcxproj参照)。
	//QuickJS側の論理スタック上限もそれに収まるよう控えめに設定する。
	JS_SetMaxStackSize(rt, 4 * 1024 * 1024);

	JSContext* ctx = NewMacroContext(rt);
	if( !ctx ){
		JS_FreeRuntime(rt);
		return false;
	}

	//マクロ停止スレッドの起動(WSH版と同じ仕組み。IActiveScript::InterruptScriptThreadの
	//代わりに、JS_SetInterruptHandlerで定期ポーリングされるフラグを使う)
	SQuickJSAbortParam sThreadParam;
	sThreadParam.nCancelTimer = GetDllShareData().m_Common.m_sMacro.m_nMacroCancelTimer;
	sThreadParam.view = pcEditView;
	sThreadParam.bAbort = false;
	sThreadParam.hEvent = NULL;

	HANDLE hThread = NULL;
	unsigned int nThreadId = 0;
	if( 0 < sThreadParam.nCancelTimer ){
		sThreadParam.hEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
		JS_SetInterruptHandler(rt, QuickJSInterruptHandler, &sThreadParam);
		hThread = (HANDLE)_beginthreadex( NULL, 0, QuickJSAbortMacroProc, (LPVOID)&sThreadParam, 0, &nThreadId );
	}

	//インタフェースオブジェクトの登録(WSH版のCEditorIfObj登録と同じ)
	CQuickJSIfObjBinder binder;
	CEditorIfObj editorObj;
	binder.BindObject(ctx, &editorObj, pcEditView, flags);
	//	プラグイン等が追加したインタフェースオブジェクト(例: CPluginIfObj)も束縛する
	for( CWSHIfObj::ListIter it = m_Params.begin(); it != m_Params.end(); ++it ){
		binder.BindObject(ctx, *it, pcEditView, flags);
	}

	std::string sUtf8Source = WideToUtf8(m_Source);
	JSValue result = JS_Eval(ctx, sUtf8Source.c_str(), sUtf8Source.size(), "macro.qjs", JS_EVAL_TYPE_GLOBAL);

	bool bRet = true;
	if( JS_IsException(result) ){
		bRet = false;
		//ユーザーが「マクロ停止」ダイアログでキャンセルした場合は、既にその操作自体が
		//結果を示しているので、重ねてエラーダイアログは出さない。
		if( !sThreadParam.bAbort ){
			ReportQuickJSException(pcEditView->GetHwnd(), ctx);
		}
	}
	JS_FreeValue(ctx, result);

	if( sThreadParam.hEvent ){
		::SetEvent(sThreadParam.hEvent);
		::WaitForSingleObject(hThread, INFINITE);
		::CloseHandle(hThread);
		::CloseHandle(sThreadParam.hEvent);
	}

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
	return bRet;
}

/*!	QuickJSマクロの読み込み(ファイルから)

	エラーメッセージは出しません。呼び出し側でよきにはからってください。
*/
BOOL CQuickJSMacroMgr::LoadKeyMacro( HINSTANCE /*hInstance*/, const TCHAR* pszPath )
{
	m_Source = L"";

	CTextInputStream in(pszPath);
	if( !in ) return FALSE;

	while( in ){
		m_Source += in.ReadLineW() + L"\r\n";
	}
	return TRUE;
}

/*!	QuickJSマクロの読み込み(文字列から)
*/
BOOL CQuickJSMacroMgr::LoadKeyMacroStr( HINSTANCE /*hInstance*/, const TCHAR* pszCode )
{
	m_Source = to_wchar( pszCode );
	return TRUE;
}

/*!	Factory

	拡張子"qjs"のときだけ自身を生成する(CPPAMacroMgr::Creatorと同じ、レジストリを
	引かない固定拡張子判定。WSHの".js"/".vbs"はレジストリのProgID解決に依存するため、
	専用拡張子".qjs"にすることで衝突・曖昧さを避ける)。
*/
CMacroManagerBase* CQuickJSMacroMgr::Creator(const TCHAR* FileExt)
{
	if( _tcscmp( FileExt, _T("qjs") ) == 0 ){
		return new CQuickJSMacroMgr;
	}
	return NULL;
}

void CQuickJSMacroMgr::declare()
{
	CMacroFactory::getInstance()->RegisterCreator(Creator);
}

//インタフェースオブジェクトを追加する
void CQuickJSMacroMgr::AddParam( CWSHIfObj* param )
{
	m_Params.push_back( param );
}

//インタフェースオブジェクト達を追加する
void CQuickJSMacroMgr::AddParam( CWSHIfObj::List& params )
{
	m_Params.insert( m_Params.end(), params.begin(), params.end() );
}

//インタフェースオブジェクトを削除する
void CQuickJSMacroMgr::ClearParam()
{
	m_Params.clear();
}

#endif // NKMM_FIX_QUICKJS_MACRO
/*[EOF]*/
