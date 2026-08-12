# 単発置換を連続で押すと検索/置換ダイアログが閉じてしまう不具合の修正 20260811

対象フラグ: `NKMM_FIX_DIALOG`(既存、`NKMM_CLOSE_DIALOG_WITH_MODE_CANCELLATION`,
2017年導入)。新規フラグは追加していない。

対象ファイル:
- `sakura_core/cmd/CViewCommander.cpp`(`F_CANCEL_MODE`ハンドラ)
- `sakura_core/cmd/CViewCommander.h`(`Command_CANCEL_MODE`宣言)
- `sakura_core/cmd/CViewCommander_ModeChange.cpp`(`Command_CANCEL_MODE`実装)

---

## 症状

置換ダイアログの「置換」ボタン(単発置換)を連続で押すと、2回目以降で
ダイアログそのものが閉じてしまう。検索ダイアログでも同様の操作で再現する。

## 原因

`NKMM_FIX_DIALOG`の`NKMM_CLOSE_DIALOG_WITH_MODE_CANCELLATION`(2017-08-09
導入)は、ESCキー等でユーザーが明示的に「各種モードの取り消し」
(`F_CANCEL_MODE`→`Command_CANCEL_MODE()`)を行った際に、開いている検索/
置換ダイアログもあわせて閉じる機能。

一方`Command_CANCEL_MODE()`は、選択解除(`Command_LEFT`/`Command_RIGHT`等)
やUndo/Redoの前処理など、ユーザーの明示操作を伴わない内部的なモード
リセットの経路からも共通で呼ばれている。置換の実行後にも選択状態の
リセットを目的とした内部呼び出しが発生するため、単発置換を実行するたびに
「モード取り消し」経路を通り、ユーザーが意図しないままダイアログが
閉じる結果になっていた。

呼び出し元を区別する手段が無く、`Command_CANCEL_MODE()`のどの呼び出しが
「ユーザーの明示操作」でどれが「内部的なリセット」かを関数内部からは
判別できないことが根本原因。

## 修正

`Command_CANCEL_MODE()`に`bAllowCloseDialog`引数(既定値`false`)を追加し、
`NKMM_CLOSE_DIALOG_WITH_MODE_CANCELLATION`によるダイアログクローズ処理を
`if (bAllowCloseDialog)`で囲んでガードした。

```cpp
void CViewCommander::Command_CANCEL_MODE( int whereCursorIs = 0, bool bAllowCloseDialog = false );
```

呼び出し元のうち、ユーザーが明示的にモード取り消し操作を行った
`F_CANCEL_MODE`ハンドラ(ESCキー等)のみ`true`を渡す。

```cpp
case F_CANCEL_MODE:  Command_CANCEL_MODE(0, true);break;  //各種モードの取り消し(ESC等の明示操作なのでダイアログクローズを許可)
```

選択解除やUndo/Redo前処理からの内部的な呼び出しは既定値`false`のままで、
ダイアログクローズ処理を素通りするようになり、置換の連続実行でダイアログが
閉じる問題が解消した。

## 発覚の経緯

`PCRE2_SUBSTITUTE_EXTENDED`未指定によるエスケープ解釈漏れ
([NKMM_FIX_REGEXP_FALLBACK.md](NKMM_FIX_REGEXP_FALLBACK.md)参照)の調査・
検証と同じセッションで、置換の連続実行を手動確認していた際に発見された。
コード上は無関係(PCRE2/正規表現フォールバックとは別経路)なため、
本ファイルとして独立させて記録している。

## 動作確認について

置換ダイアログを開いた状態で「置換」ボタンを連続で押してもダイアログが
閉じずに残ることを確認。ESCキーによる明示的なモード取り消し操作では、
従来通りダイアログが閉じることも確認した。
