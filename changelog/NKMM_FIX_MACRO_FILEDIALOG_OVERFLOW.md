# NKMM_FIX_MACRO_FILEDIALOG_OVERFLOW 修正レポート

対象フラグ: `NKMM_FIX_MACRO_FILEDIALOG_OVERFLOW`（新規）
対象ファイル(主なもの):

- `sakura_core/macro/CMacro.cpp`（`F_FILEOPENDIALOG`/`F_FILESAVEDIALOG`ハンドラ）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

セキュリティレビュー(複数の専門エージェントによる並列調査)で、マクロエンジンの
ダイアログ系関数にスタックバッファオーバーフローの疑いが見つかったため修正した。

## 原因

`CMacro.cpp`の`F_FILEOPENDIALOG`/`F_FILESAVEDIALOG`(マクロから呼べる
`FileOpenDialog(defaultName, filter)` / `FileSaveDialog(defaultName, filter)`関数)の
実装が、マクロ引数から得た既定ファイル名文字列(`sDefault`、長さ無制限の`std::wstring`)を
固定長260文字のスタックバッファへ無検査にコピーしていた。

```cpp
TCHAR szPath[ _MAX_PATH ];
_tcscpy( szPath, sDefault.c_str() );
```

`sDefault`はマクロの第1引数(`VARIANT`→`BSTR`)をそのまま文字列化したものであり、
上限チェックが無い。マクロが `FileSaveDialog(Editor.GetSelectedText(), "*.*")` の
ように、編集中のドキュメント内容やそこから導出した文字列を渡すのは一般的な用法であり、
260文字を超える選択テキストを含む細工済みファイルを開いて当該マクロ関数を呼ぶだけで
スタックバッファオーバーフローに到達できる。マクロ作者の意図ではなく、インタプリタ側の
引数処理に起因するため、ドキュメント内容起点で発火する点が問題だった。

同じファイル内の`F_INPUTBOX`(`t_min(sDefaultValue.length(), nMaxLen)`で
クランプしてから`auto_memcpy`)や`F_FOLDERDIALOG`は正しく境界チェックしており、
`FileOpenDialog`/`FileSaveDialog`だけがこのパターンから外れていた。

## 対応

`F_INPUTBOX`と同じパターンに合わせ、コピー長を`_MAX_PATH-1`にクランプしてから
`auto_memcpy`で終端を保証するように変更した。

```cpp
TCHAR szPath[ _MAX_PATH ];
#ifdef NKMM_FIX_MACRO_FILEDIALOG_OVERFLOW
size_t nCopyLen = t_min( sDefault.length(), (size_t)(_MAX_PATH - 1) );
auto_memcpy( szPath, sDefault.c_str(), nCopyLen );
szPath[nCopyLen] = _T('\0');
#else
_tcscpy( szPath, sDefault.c_str() );
#endif // NKMM_
```

無効化すれば従来通りの無検査`_tcscpy`に戻る。

## 動作確認について

`Release|Win32`構成でのビルド成功のみ確認済み。マクロから実際に長い文字列を
`FileOpenDialog`/`FileSaveDialog`に渡してクラッシュしないこと、および
`_MAX_PATH`超過時に末尾で正しく切り詰められることの実機確認は未実施。
