# ステータスバー文字数表示のO(1)キャッシュ化 20260806

対象フラグ: `NKMM_FIX_STATUSBAR_WORDNUM_CACHE`（新規）
対象ファイル:

- `sakura_core/COpe.h` / `sakura_core/COpe.cpp`（`CalcOpeLineDataCharCount`）
- `sakura_core/doc/CEditDoc.h` / `sakura_core/doc/CEditDoc.cpp`（文字数キャッシュ本体）
- `sakura_core/view/CEditView.cpp`（`CEditView::GetDocumentWordNum`）
- `sakura_core/view/CEditView_Command_New.cpp`（`CEditView::ReplaceData_CEditView3`）
- `sakura_core/doc/layout/CLayoutMgr_New2.cpp`（`CLayoutMgr::ReplaceData_CLayoutMgr`。
  追記参照）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

ユーザーから「500MBのデータを読み込ませるとすべての動作が遅くなる」という報告を
受け調査した。読み込み自体の時間は妥当(ファイルサイズに比例するのは当然)だが、
読み込み後の操作、特に矢印キーでの1文字/1行移動だけで顕著に重くなるとのことだった。
対象ファイルは色分け対象外の単純な`.txt`、折り返しなし設定、行構造は「普通の行が
大量にある」(1行が極端に長いわけではない)という条件だったため、表示行レイアウト
キャッシュ側(`m_pLayoutPrevRefer`等)の距離依存コストではなく、キャレット移動の
たびに毎回無条件で走る何かがファイルサイズに比例していると推測し、再描画・
ステータスバー更新経路を中心に調査した。

## 原因

`CCaret::ShowCaretPosInfo()`(`CCaret.cpp`)は、キャレットが動くたびに(矢印キー
1回ごとに)ステータスバーの「行:列」表示を更新するが、`NKMM_FIX_STATUSBAR`
(既存、常時有効)の実装では、そこに文書全体の文字数を含めるため
`CEditView::GetDocumentWordNum()`を呼んでいた。

```cpp
auto_sprintf( szText_1, LS( STR_STATUS_ROW_COL ), m_pEditView->GetDocumentWordNum(), ptCaret.y, ptCaret.x );
```

`GetDocumentWordNum()`(`CEditView.cpp`)は、`CLayoutMgr::GetLineCount()`が返す
**全レイアウト行**を毎回ループしてサロゲートペア考慮の文字数を積算する実装だった。

```cpp
for( nLineNum = CLayoutInt(0); nLineNum <= nLineCount; ++nLineNum ){
	pLine = ...GetLineStr( nLineNum, &nLineLen, &pcLayout );
	select_sum += CNativeW::GetCharCountInRange( pLine, nLineLen, 0, pcLayout->GetLengthWithoutEOL() );
}
```

500MBの通常行ファイル(数百万行)では、矢印キー1回ごとに数百万行を毎回舐め直す
計算になり、これが体感の重さの直接原因だった。決定的な傍証として、同関数の
`#else`(`NKMM_FIX_STATUSBAR`無効時の旧実装)には次のコメントが残っている。

```cpp
auto_sprintf( szText_1, LS( STR_STATUS_ROW_COL ), ptCaret.y, ptCaret.x );	//Oct. 30, 2000 JEPRO 千万行も要らん
```

オリジナルのさくらエディタは、意図的にこの全文字数カウントをステータスバーの
毎回更新から外していた。`NKMM_FIX_STATUSBAR`がその安全策なしに文字数表示を
復活させてしまっていたのが根本原因。

## 対応

文書全体を毎回数え直す代わりに、文字数を`CEditDoc`にキャッシュし、実際に
テキストが書き換わる関数`CEditView::ReplaceData_CEditView3()`で、そのとき
挿入/削除された分の文字数だけを増減させる方式にした。

`ReplaceData_CEditView3()`はライブ編集(入力・削除・貼り付け等)だけでなく、
`Command_UNDO`/`Command_REDO`によるUndo/Redoの実際の適用も含め、テキストが
実際に書き換わる箇所を一つ残らず経由する(`COpeBuf::DoUndo`/`DoRedo`自体は
ポインタを返すだけで文書を書き換えない。実際の書き換えは呼び出し元が
`ReplaceData_CEditView3()`を再度呼ぶことで行われる)。

差分の計算には、Undoバッファ(`COpeLineData`)が保持するのと同じ挿入/削除
データを使う。`NKMM_FIX_UNDO_BUFFER_LIMIT`が概算バイト数を積算する
`CalcOpeLineDataByteSize()`と同じパターンで、文字数版`CalcOpeLineDataCharCount()`
を`COpe.h/cpp`に追加した(改行文字を末尾から後方トリムして除外、サロゲートペアは
1文字として数える点も既存の`GetDocumentWordNum()`と揃えている)。

```cpp
int CalcOpeLineDataCharCount(const COpeLineData& lineData)
{
	const bool bExtEol = GetDllShareData().m_Common.m_sEdit.m_bEnableExtEol;
	int total = 0;
	for( const CLineData& ld : lineData ){
		const wchar_t* p = ld.cmemLine.GetStringPtr();
		const int nLen = ld.cmemLine.GetStringLength();
		int nEolLen = 0;
		while( nEolLen < nLen && WCODE::IsLineDelimiter(p[nLen - 1 - nEolLen], bExtEol) ){
			++nEolLen;
		}
		total += CNativeW::GetCharCountInRange(p, nLen, 0, nLen - nEolLen);
	}
	return total;
}
```

`ReplaceData_CEditView3()`内では、挿入データ(`pInsData`)は呼び出し先で中身が
`SetDocLineStringMove()`等により実際に「移動」されてしまう(呼び出し後に読むと
空になっている)ため、置換処理を呼ぶ**前**に文字数を数えておく必要がある。
削除データ(`pcmemDeleted`)は呼び出し先が書き込む出力なので、呼び出し**後**に
数える。

```cpp
const int nInsCharsForWordNumCache = pInsData ? CalcOpeLineDataCharCount(*pInsData) : 0;
// ...置換呼び出し...
if( pcMemDeleted ){
	const int nDelCharsForWordNumCache = CalcOpeLineDataCharCount(*pcMemDeleted);
	m_pcEditDoc->AdjustDocumentCharCountCache( nInsCharsForWordNumCache - nDelCharsForWordNumCache );
}else{
	m_pcEditDoc->InvalidateDocumentCharCountCache();
}
```

`pcOpeBlk`(Undo記録先)も`pcmemCopyOfDeleted`も指定されない一部の内部処理では
削除データが捕捉されないため、その場合はキャッシュを無効化するだけにして、
次回`GetDocumentWordNum()`が呼ばれたときに1回だけ全体を数え直させる
フォールバックにした(クラッシュにはならず、その回だけ従来と同じコストに
戻るだけ)。`CEditDoc::Clear()`(新規読み込み・新規文書時)でも同様に無効化する。

`CEditView::GetDocumentWordNum()`はキャッシュが有効ならO(1)で返し、無効な
場合(初回・読み込み直後・上記フォールバック時)のみ既存の全行ループで1回
数え直してキャッシュを確定する。

## 追記: タイプ入力では文字数が更新されず、Undo/Redoでしか変化しない不具合 20260806

実機で確認したユーザーから「文字数カウンタが変動しない、文字キャッシュの
有無に関係なく」「具体的にはアンドゥリドゥのときしか変化しない」という報告を
受け、原因を調査・修正した。

### 原因1: フックした関数が「唯一の合流点」ではなかった

`ReplaceData_CEditView3()`は、実は**全ての編集が通る関数ではなかった**。
`CLayoutMgr::ReplaceData_CLayoutMgr()`(実際にテキストを書き換える処理)には
3つの呼び出し元があり、`ReplaceData_CEditView3()`はそのうちの1つに過ぎない。

- `CEditView::InsertData_CEditView()`(`CEditView_Command_New.cpp:214`) —
  **通常のタイプ入力・貼り付けの入口**。`ReplaceData_CLayoutMgr()`を直接呼ぶ
- `CEditView::DeleteData2()`(`CEditView_Command_New.cpp:416`) — 矩形選択削除
- `CEditView::ReplaceData_CEditView3()`の`!bFastMode`分岐
  (`CEditView_Command_New.cpp:885`) — 検索置換・Undo/Redo等

つまりタイプ入力は`InsertData_CEditView()`→`ReplaceData_CLayoutMgr()`と
直接進み、`ReplaceData_CEditView3()`を一切経由しないため、そこに置いた
文字数差分の更新コードが実行されなかった。一方Undo/Redoは
`ReplaceData_CEditView3()`を経由するため、そちらだけ正しく動いていた
(`Command_UNDO`/`Command_REDO`(`CViewCommander_Edit.cpp`)は
`ReplaceData_CEditView3()`を呼ぶ)。

これは`CSearchAgent::ReplaceData()`(真の最下層の合流点、2箇所からしか
呼ばれない)を確認した際の初期調査结果を、1つ上位の`ReplaceData_CEditView3()`
にすり替えて実装してしまったことによる誤り。`ReplaceData_CEditView3()`の
`bFastMode`分岐は`CSearchAgent::ReplaceData()`を直接呼ぶが、`!bFastMode`分岐は
`ReplaceData_CLayoutMgr()`を経由し、その内部で`CSearchAgent::ReplaceData()`を
呼ぶ。`InsertData_CEditView()`/`DeleteData2()`も同じく`ReplaceData_CLayoutMgr()`
を経由するため、そちらに合わせてフックし直す必要があった。

### 対応1

文字数差分の更新を`CLayoutMgr::ReplaceData_CLayoutMgr()`
(`CLayoutMgr_New2.cpp`、`CSearchAgent::ReplaceData()`呼び出しの直後)へ
移動した。`ReplaceData_CEditView3()`側は`bFastMode`分岐(`ReplaceData_CLayoutMgr()`
を経由しない経路)だけに絞り、`!bFastMode`分岐は`ReplaceData_CLayoutMgr()`側の
更新に任せることで、二重カウントを避けた。これで`InsertData_CEditView()`/
`DeleteData2()`/`ReplaceData_CEditView3()`(両分岐)の全経路が正しくカバーされる。

### 原因2: `pcmemDeleted == NULL`の意味が2通りあった

修正1を適用しても、まだタイプ入力では更新されなかった。`InsertData_CEditView()`
は`arg.pcmemDeleted = NULL`を**意図的に**渡している(選択範囲があれば呼び出し前に
別途`DeleteData()`で削除済みのため、この呼び出し自体には削除が無い)。

当初のコードは「`pcmemDeleted`がNULL → 削除データを捕捉できない → キャッシュ
無効化」という判定だったため、**削除が無い普通の1文字入力のたびに毎回
キャッシュを無効化**してしまい、次の`GetDocumentWordNum()`呼び出し(次の
キー入力のステータスバー更新)で毎回全体を数え直す、つまり元の重い実装に
逆戻りしていた(表示上は「更新されているように見えて実は毎回フルスキャン」
になるため、「変動しない」という報告とは見た目上一致しないように思えるが、
実際には次のキー入力時点ではまだ古い値が表示され続け、フルスキャンの結果は
さらにその次の表示でようやく反映される、という1手遅れの状態になっていた)。

`pcmemDeleted == NULL`には「削除データを捕捉できなかった(不明)」と
「削除範囲が空なので確実に0文字(既知)」の2通りの意味があり、区別できて
いなかったのが根本原因。削除範囲(`sDelRange`の`From`/`To`)自体は常に渡される
ので、まずそちらが空かどうかで判定するように修正した。

```cpp
if( ptFrom == ptTo ){
	// 削除範囲が空 = 削除文字数は確実に0(pcmemDeletedの有無によらない)
	m_pcEditDoc->AdjustDocumentCharCountCache( nInsCharsForWordNumCache );
}else if( DLRArg.pcmemDeleted ){
	// 削除範囲があり、かつ削除データも捕捉できている → 正確に差分計算できる
	const int nDelCharsForWordNumCache = CalcOpeLineDataCharCount(*DLRArg.pcmemDeleted);
	m_pcEditDoc->AdjustDocumentCharCountCache( nInsCharsForWordNumCache - nDelCharsForWordNumCache );
}else{
	// 削除範囲はあるが捕捉されていない → 差分不明、無効化
	m_pcEditDoc->InvalidateDocumentCharCountCache();
}
```

同じ修正を`ReplaceData_CEditView3()`の`bFastMode`分岐側にも適用した。

### 副次的な修正

`CLayoutMgr_New2.cpp`は`CEditDoc`の前方宣言しか見えていなかった
(`CLayoutMgr.h`はポインタ`m_pcEditDoc`しか持たないため前方宣言で足りていた)
ため、`m_pcEditDoc->AdjustDocumentCharCountCache()`等のメンバ関数呼び出しに
`CEditDoc`の完全な定義が必要になり、`#include "doc/CEditDoc.h"`を追加した。

## 残る既知の制約

`CSearchAgent::ReplaceData()`の2箇所の呼び出し元(`ReplaceData_CEditView3()`の
`bFastMode`分岐、`CLayoutMgr::ReplaceData_CLayoutMgr()`)は両方ともフック済みで、
かつ`ReplaceData_CLayoutMgr()`の3つの呼び出し元(`InsertData_CEditView`,
`DeleteData2`, `ReplaceData_CEditView3`の`!bFastMode`分岐)を実際にgrepで
確認した上での結論だが、Grep置換やマクロ経由の書き換えなど、この2つの
関数群を経由しない編集経路が別途存在した場合は、その編集はキャッシュに
反映されず、表示上の文字数が実際とズレたまま残るリスクがなお残る
(クラッシュ系の不具合ではなく表示のズレに留まる)。

## 動作確認について

VS2022(`sakura.sln`、Debug/x64)でのビルドを実施。このリポジトリは本修正とは
無関係な既存の型変換エラー(`CStrictInteger`関連、`cmd/CViewCommander_Bookmark.cpp`
等、複数箇所)により全体ビルドが通らない既知の制約があるが、変更前後でビルドログの
エラー一覧(31件)が完全に一致することを確認し、`COpe.cpp`/`COpe.h`/`CEditDoc.h`/
`CEditDoc.cpp`/`CLayoutMgr_New2.cpp`/`CEditView_Command_New.cpp`にはエラー・警告が
一切ないことを確認済み(追記の修正込みで再確認済み)。

### 追記: 実機確認済み 20260806

追記の2つのバグ修正後、報告者が実機でタイプ入力による文字数の増減を確認し、
問題なしとの報告を受けた。
