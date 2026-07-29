# NKMM_FIX_KEYWORD_OVERFLOW 修正レポート

対象フラグ: `NKMM_FIX_KEYWORD_OVERFLOW`（新規）
対象ファイル(主なもの):

- `sakura_core/CKeyWordSetMgr.cpp`（`CKeyWordSetMgr::SetKeyWordArr`）
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

セキュリティレビュー(複数の専門エージェントによる並列調査)で、タイプ別設定の
キーワード登録処理に共有メモリ上のバッファオーバーフローの疑いが見つかったため修正した。

## 原因

`CKeyWordSetMgr::SetKeyWordArr`は、`\t`または`\0`区切りのキーワード文字列
(`sakura.ini`のタイプ別設定`szKW[NN]`から読み込まれる)を、固定長スロット
`wchar_t m_szKeyWordArr[MAX_KEYWORDNUM][MAX_KEYWORDLEN + 1]`
(`MAX_KEYWORDLEN` = 63、`CKeyWordSetMgr.h`)へコピーする際、区切り位置までの
長さ(`kwlen`)を63でクランプせずに`wmemcpy`していた。

```cpp
const wchar_t* pTop = ptr;
while( *ptr != L'\t' && *ptr != L'\0' ) ++ptr;
int kwlen = ptr - pTop;
wmemcpy( m_szKeyWordArr[i], pTop, kwlen );   // 未クランプ
m_szKeyWordArr[i][kwlen] = L'\0';            // OOB null終端も
```

呼び出し元の`CShareData_IO.cpp`(`ShareData_IO_KeyWords`)は、`szKW[NN]`の値を
あえて境界チェック付きの`StringBufferW`を使わず、無制限長の`std::wstring`として
iniから読み込んでいる(「`StringBufferW`ではNGだった」という既存コメントがあり、
意図的な設計)。

`m_szKeyWordArr`は`CKeyWordSetMgr`ごと共有メモリ(`DLLSHAREDATA`)に直接
埋め込まれているため、64文字以上のキーワード(タブを含まない)を含む
`sakura.ini`(共有・インポートされた設定ファイル)を読み込ませるだけで、
隣接するキーワードスロットや`DLLSHAREDATA`の他フィールドを破壊できる。

## 対応

`kwlen`を`MAX_KEYWORDLEN`でクランプしてから`wmemcpy`するように変更した。

```cpp
int kwlen = ptr - pTop;
#ifdef NKMM_FIX_KEYWORD_OVERFLOW
kwlen = t_min( kwlen, MAX_KEYWORDLEN );
#endif // NKMM_
wmemcpy( m_szKeyWordArr[i], pTop, kwlen );
m_szKeyWordArr[i][kwlen] = L'\0';
```

無効化すれば従来通りの無検査`wmemcpy`に戻る。

## 動作確認について

`Release|Win32`構成でのビルド成功のみ確認済み。64文字以上のキーワードを含む
`sakura.ini`を実際に読み込ませてクラッシュ・データ破壊が起きないこと、および
63文字で正しく切り詰められることの実機確認は未実施。
