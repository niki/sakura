# セキュアでない文字列・メモリ関数の監査と置き換え 作業レポート

対象フラグ: なし（`#ifdef NKMM_*` によるトグルではなく、`auto_～`系共通ヘルパーへの
恒久的な変更。無効化スイッチは存在しない）

対象ファイル(主なもの):

- `sakura_core/util/string_ex.h`（`auto_sprintf` / `auto_strcpy` / `auto_strcat` の実装本体）
- `sakura_core/types/CType_Html.cpp`、`sakura_core/types/CType_Vb.cpp`（実バグの修正）
- 上記以外、sakura_core配下 150ファイル超（後述のGroup①〜⑥、機械置換＋個別修正）

---

## 背景

「セキュアでないレガシーな関数（`_s`が付いていないもの）を改修したい」という依頼から開始。
最初に`strcpy`/`sprintf`系を洗い出したところ、`auto_sprintf`という独自ラッパーマクロが
実質無制限（`MAX_BUF=0x7FFFFFFF`）にコピーしており、過去に何度も個別のスタックバッファ
オーバーフロー修正（GHSA-jg35-7phr-86wq 等）が入っていたことが判明。同じ問題が
`auto_strcpy`/`auto_strcat`（境界チェック皆無で`strcpy`/`wcscpy`等をそのまま呼ぶだけ）にも
あり、さらに生の`wcscpy`/`_tcscpy`/`wcscat`/`_tcscat`/`strcpy`/`strcat`/`lstrcpy`/`lstrcat`/
`swprintf`/`wsprintf`が`auto_～`系を経由せず直接呼ばれている箇所が564箇所あった。

ひと通り片付いた後、「`mem`系（`memcpy`/`memset`）はどうか」という追加確認も行った。

## 対応の全体像

### 1. `auto_sprintf`（369箇所）

**マクロ自体を書き込み先配列サイズ自動判定版に変更**:

```cpp
// Before
#define auto_sprintf(buf, format, ...) tchar_sprintf((buf), (format), __VA_ARGS__)  // 実質無制限

// After
#define auto_sprintf(buf, format, ...) tchar_sprintf_s((buf), std::size(buf), (format), __VA_ARGS__)
```

`buf`が配列でなくポインタの場合は`std::size`がコンパイルエラーになる性質を利用し、
「マクロを直してビルドし、エラーになった箇所だけ手で直す」という方法で、実際にバッファ
サイズが不明なまま呼ばれていた危険な39箇所を特定・個別修正した（残り330箇所は元々配列
渡しだったため無変更で自動的に安全化）。

### 2. `auto_strcpy`/`auto_strcat`（564箇所）

同じ考え方で、配列サイズを自動デデュースするテンプレートに変更:

```cpp
template <size_t N> inline ACHAR* auto_strcpy(ACHAR (&dst)[N], const ACHAR* src){ auto_strcpy_s(dst,N,src); return dst; }
template <size_t N> inline WCHAR* auto_strcpy(WCHAR (&dst)[N], const WCHAR* src){ auto_strcpy_s(dst,N,src); return dst; }
// auto_strcat も同様
```

生の`wcscpy`/`_tcscpy`/`wcscat`/`_tcscat`/`strcpy`/`strcat`/`lstrcpy`/`lstrcat`/`swprintf`/
`wsprintf`呼び出し564箇所を、ディレクトリ単位で6グループ（①types/ ②dlg/ ③cmd+macro/
④env+prop+typeprop+recent/ ⑤view+window+print+outline+func+doc/ ⑥util+雑多ファイル）に
分けて`auto_strcpy`/`auto_strcat`/`auto_sprintf`へ機械的に置換し、都度ビルドしてコンパイル
エラーになった約100箇所（ポインタ引数・構造体メンバ経由でサイズ情報が無いもの）を
個別に安全化した。`SFilePath`等の独自文字列型（`StaticString<TCHAR,N>`）については
既存の`_countof2()`マクロを利用。

### 3. `mem`系（`memcpy`/`memmove`/`memset`）監査

`memcpy`系は元々サイズ引数が必須のため「マクロを直して機械的に洗い出す」手法が使えず、
主要な呼び出し約100箇所を手動でレビューした。その結果、**同一クラスの実バグを2件発見・
修正**した。

#### バグ1: `CType_Html.cpp` — HTMLアウトライン解析のスタックバッファオーバーフロー

```cpp
wchar_t szTitle[32];
for(j=0; i+j<nLineLen && j<_countof(szTitle)-1; )
{
    int nCharSize = CNativeW::GetSizeOfChar(pLine, nLineLen-i, j);   // サロゲートペアなら2
    memcpy(szTitle + j, pLine + j, nCharSize * sizeof(wchar_t));
    j += nCharSize;
}
szTitle[j] = '\0';   // jが32になり得る（szTitleは有効index 0-31）
```

ループ条件`j<31`はサロゲートペア（絵文字等、`nCharSize==2`）を考慮しておらず、`j=30`で
2バイト文字を読むと`j`が32まで進み、直後のヌル終端書き込みが配列外に書き込む。
**サロゲートペア文字を含むタグ名を持つHTMLファイルを開く（アウトライン解析が走る）だけで
到達可能**。書き込み前に`j+nCharSize`が収まるか確認するよう修正。

#### バグ2: `CType_Vb.cpp` — VBアウトライン解析で同根のバグ（2箇所）

```cpp
wchar_t szWord[256];
const int nMaxWordLeng = 255;
...
if( nWordIdx >= nMaxWordLeng ){          // nCharCharsを考慮していない
    ...
}else{
    auto_memcpy( &szWord[nWordIdx], &pLine[i], nCharChars );   // nCharCharsは1 or 2
    szWord[nWordIdx + nCharChars] = L'\0';                      // 256になり得る
}
```

`nMaxWordLeng`(255)と`szWord`の実サイズ(256)の余白がちょうど1文字分しかなく、
`nWordIdx==254`でサロゲートペア文字が来ると同じ理屈でヌル終端が配列外(`szWord[256]`)に
書き込まれる。同じファイル内に隣接する`CType_Java.cpp`は`if(nWordIdx + nCharChars >= nMaxWordLeng)`
という正しいガードで書かれていたため、それに合わせて修正。

同種のガード(`nWordIdx >= nMaxWordLeng`のみで`+nCharChars`が無いパターン)は
`CType_Cpp.cpp`・`CType_Perl.cpp`・`CType_Sql.cpp`にも存在したが、いずれも
`nMaxWordLeng`と実配列サイズの間に30〜156文字の余白があるか、1文字ずつしか書き込まない
実装だったため実害なしと判断し、変更していない。

#### 問題なしと判断したもの

`memset`の生呼び出し17箇所は全て`sizeof(自分自身)`パターン（ポインタのsizeof誤用なし）。
`auto_memcpy`/`auto_memset`44箇所および他の`memcpy`/`memmove`約40箇所は、
「2パスで必要サイズを事前計算してから確保」「`t_min`/`_countof`による境界チェック」
「割り当て直後にそのサイズ分だけコピー」のいずれかのパターンで安全と確認した
（`CMemory::_AddData`、`CUtf7`のUTF-7デコード、`CConvert_TabToSpace`等）。

## 動作確認について

各フェーズごとに`MSBuild`で`Debug|x64`・`Release|x64`のビルドをエラー0件まで確認済み。
`Debug|Win32`は本作業と無関係な既存の問題（`RomajiFuzzyMatch.hpp`が`std::optional`を使うが
Win32構成に`/std:c++17`が設定されておらずビルド不可）が本作業前から存在しており、
今回の変更前後で同じ37件のエラーが出ることを確認して切り分け済み。

実際にサロゲートペア文字を含むHTML/VBファイルを開いてクラッシュしないこと、
および長大な入力を伴う各修正箇所の実機・実操作での動作確認は未実施。
