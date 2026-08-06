# 表示行レイアウト管理のダングリングポインタ修正 20260805

対象フラグ: `NKMM_FIX_LAYOUT_DANGLING_PREVREFER`（新規）
対象ファイル:

- `sakura_core/doc/layout/CLayoutMgr.cpp`（`CLayoutMgr::DeleteLayoutAsLogical`）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

「表示行レイアウト計算について解析と修正案」という依頼を受け、`CLayoutMgr`(表示行の
折り返し計算・管理)まわりを調査した際に見つかった、既存コード中の未解決バグ。

## 原因

`CLayoutMgr::DeleteLayoutAsLogical()`は、複数論理行の削除・置換のたびに、削除範囲に
該当する`CLayout`ノード群を`delete`する。この際、`SearchLineByLayoutY()`/
`LogicToLayout()`が使う位置キャッシュ`m_pLayoutPrevRefer`/`m_nPrevReferLine`を、
削除範囲に含まれない安全なノードへ付け替える必要がある。

旧実装は関数冒頭で`pLayoutInThisArea->GetPrevLayout()`を「安全なノード」とみなして
先にキャッシュへ設定していたが、直後のバックワードサーチ(削除範囲の先頭ノード
`pLayoutWork`を探す処理)はさらに前方まで戻ることがあり、その場合このノード自体が
削除対象に含まれてしまっていた(折り返し表示が有効で、削除開始位置が論理行の先頭
表示行セグメントでない場合に発生)。

```cpp
// 1999.11.22
m_pLayoutPrevRefer = pLayoutInThisArea->GetPrevLayout();
m_nPrevReferLine = nLineOf_pLayoutInThisArea - CLayoutInt(1);

/* 範囲内先頭に該当するレイアウト情報をサーチ */
pLayoutWork = pLayoutInThisArea->GetPrevLayout();
while( NULL != pLayoutWork && nLineFrom <= pLayoutWork->GetLogicLineNo()){
	pLayoutWork = pLayoutWork->GetPrevLayout();   // ここでm_pLayoutPrevReferを追い越すことがある
}
```

この状態は削除ループ内の`if( m_pLayoutPrevRefer == pLayout ){ DEBUG_TRACE(_T("バグバグ\n")); }`
というトレースで検出されてはいたが、対応する修正コード(すぐ上にコメントアウトされて
残っていた)は無効化されたまま放置されており、検出後も`delete pLayout`がそのまま
実行されるため、`m_pLayoutPrevRefer`は解放済みメモリを指すダングリングポインタに
なっていた。`DEBUG_TRACE`は`sakura_core/debug/Debug1.h`でReleaseビルドでは空マクロに
なるため、この不具合はReleaseビルドでは無警告・無トレースで発生する。

`CLayoutMgr::LogicToLayout()`(キャレット位置変換、カーソル移動・再描画のたびに
呼ばれる高頻度関数)は`m_pLayoutPrevRefer->GetLogicLineNo()`をNULLチェックのみで
直接デリファレンスするため、上記の状態が発生した直後に呼ばれると解放済みメモリを
読む未定義動作となる。折り返し表示ON時、複数表示行にまたがる論理行を含む範囲を
削除・置換(検索置換、ブロック削除、Undo/Redoなど)した直後にカーソル移動や再描画が
発生すると、ヒープの再利用状況次第で不定期クラッシュ・表示崩壊を引き起こしうる。

## 対応

削除範囲の先頭を探すバックワードサーチの結果である`pLayoutWork`(削除範囲に絶対
含まれないことが保証される)を基準に、キャッシュを設定し直すよう変更した。
バックワードサーチで戻ったノード数(`nStepBack`)を数え、`m_nPrevReferLine`もその分
だけ調整する。

```cpp
CLayoutInt nStepBack = CLayoutInt(0);
pLayoutWork = pLayoutInThisArea->GetPrevLayout();
while( NULL != pLayoutWork && nLineFrom <= pLayoutWork->GetLogicLineNo()){
	pLayoutWork = pLayoutWork->GetPrevLayout();
	++nStepBack;
}
m_pLayoutPrevRefer = pLayoutWork;
m_nPrevReferLine = nLineOf_pLayoutInThisArea - CLayoutInt(1) - nStepBack;
```

この変更により、削除ループ内で`m_pLayoutPrevRefer`が削除対象ノードと一致する
状況は構造的に発生しなくなるため、旧来の`DEBUG_TRACE`検出コードとその手前の
無効化されていた修正案は削除した(無効化フラグ時は旧実装をそのまま`#else`側に
保持している)。

## 動作確認について

VS2022(`sakura.sln`、Debug/x64)でのビルドを実施。このリポジトリは本修正とは
無関係な既存の型変換エラー(`CStrictInteger`関連、`cmd/CViewCommander_Bookmark.cpp`
等、複数箇所)により全体ビルドが通らない既知の制約があるが、変更前後でビルドログの
エラー一覧(31件)が完全に一致することを確認し、`CLayoutMgr.cpp`自体はエラー・警告
なくコンパイルされることを確認済み。

### 追記: 実機確認済み 20260806

報告者が実機で、「折り返しあり」設定で折り返された行の途中(先頭セグメントで
ない位置)から始まる範囲の削除・置換、直後のカーソル移動・スクロール・再描画、
Undo/Redoの繰り返しを確認し、問題なしとの報告を受けた。
