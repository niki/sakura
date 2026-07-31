# NKMM_USE_MIMALLOC_OVERRIDE 実装レポート

対象フラグ: `NKMM_USE_MIMALLOC_OVERRIDE`（新規、`NKMM_USE_MIMALLOC`の追加オプション）
対象ファイル(主なもの):

- `sakura_core/config/mimalloc_override_fi.h`（新規、`static.c`専用のForcedIncludeFile）
- `sakura_core/my_config.h`
- `sakura/sakura.vcxproj`（`static.c`の`ForcedIncludeFiles`を変更）/ `.vcxproj.filters`

---

## 背景

`NKMM_USE_MIMALLOC`はC++の`operator new`/`delete`のみをmimallocに差し替えており、
`malloc`/`HeapAlloc`をCから直接呼んでいる箇所(13ファイル)はその恩恵を受けない。
`NKMM_USE_MIMALLOC`実装時の記録では「Windowsでの完全上書きには`mimalloc-redirect.dll`
の配布が別途必要になるため見送り」としていたが、これは動的CRT(`/MD`)を前提にした
判断で、本プロジェクトが実際に使っている静的CRT(`/MT`)では事情が異なることが
分かったため、今回`malloc`/`free`等の完全上書きに対応した。

## 静的CRT(/MT)ではredirect dllが不要な理由

mimallocのオーバーライド実装(`libs/mimalloc/src/alloc-override.c`)は
`#if defined(MI_MALLOC_OVERRIDE) && !defined(_DLL)`で全体を囲んでいる。`_DLL`は
MSVCが動的CRT(`/MD`、`/MDd`)使用時にのみ自動定義するマクロで、静的CRT(`/MT`、
`/MTd`)では定義されない。

- 動的CRT: `malloc`/`free`の実体は`ucrtbase.dll`/`vcruntime140.dll`という
  別のDLLの中にあるため、自分の翻訳単位に同名関数を定義しても差し替わらない
  (IATを書き換えるsurrogate/redirect dllのような仕組みが別途必要)
- 静的CRT: `malloc`/`free`等の実体はリンク時に`libcmt.lib`から取り込まれる
  オブジェクトファイル単位の関数。mimalloc側が同名(`malloc`/`free`/`calloc`/
  `realloc`/`_aligned_malloc`/`strdup`等、UCRTの宣言と完全に一致する形)の
  関数を**先に**提供すれば、リンカは未解決シンボルを満たすためだけに
  `libcmt.lib`から該当オブジェクトを取り込むので、mimalloc側の定義が
  そのまま採用される。redirect dllのような実行時の仕掛けは不要

本プロジェクトの`sakura.vcxproj`は全構成で`/MT`(Runtime Library:
Multi-threaded)を指定しているため、`static.c`を`MI_MALLOC_OVERRIDE`付きで
コンパイルするだけで有効化できる。

## 実装

`libs/mimalloc/src/static.c`はvendor元のまま無改造とし、`MI_MALLOC_OVERRIDE`の
定義だけを`sakura_core/config/mimalloc_override_fi.h`(新規)という専用の
ForcedIncludeFileに切り出した。

```c
#include "my_config.h"

#if defined(NKMM_USE_MIMALLOC_OVERRIDE) && !defined(MI_MALLOC_OVERRIDE)
#define MI_MALLOC_OVERRIDE 1
#endif
```

`static.c`はCとしてコンパイルされ、プロジェクト全体のForcedIncludeFiles(`my.h`、
C++専用の`silica`を含む)をそのまま使うとコンパイルエラーになるため、従来から
`ForcedIncludeFiles`を空に上書きしていた。今回はその代わりに上記ヘッダを指定する
形にした。`my_config.h`自体はプリプロセッサ定義のみで構成されており、C11として
単体コンパイル可能であることを確認済み。

`my_config.h`側では、`NKMM_USE_MIMALLOC_OVERRIDE`は`NKMM_USE_MIMALLOC`が同時に
有効であることを前提とし、そうでない組み合わせは`#error`でビルド時に検出するよう
にした。

## 動作確認

標準ヘッダの`mimalloc.h`が公開している`mi_is_in_heap_region()`(そのポインタが
mimallocの管理領域内かを返す)を使い、実際にプロジェクトの`my_config.h`・新設の
`mimalloc_override_fi.h`・`libs/mimalloc/src/static.c`をそのままの設定
(`/MT /DNDEBUG`、x64)でビルドした小さな検証プログラムで確認した。

```
malloc(123)       -> mi_is_in_heap_region = true
calloc(4,32)      -> mi_is_in_heap_region = true
realloc(...,500)  -> mi_is_in_heap_region = true
strdup(...)       -> mi_is_in_heap_region = true
```

`NKMM_USE_MIMALLOC_OVERRIDE`を無効にした状態で同じ検証プログラムを再ビルドすると、
`malloc`等が通常のCRT実装に戻り、mimalloc管理外のポインタを`mi_is_in_heap_region()`
に渡すことになるため、mimalloc側のヒープ探索が異常終了(クラッシュ)する。これは
バグではなく、「mimallocが管理していないポインタをmimalloc自身の内部構造として
解釈しようとすると壊れる」という、越境チェックの結果であり、逆説的に無効時は
`malloc`がmimalloc経由になっていないことの傍証にもなっている。

`libs/mimalloc/src/static.c`のファイル単位ビルド(有効/無効の両方)、および
`msbuild sakura.vcxproj /p:PreBuildEventUseInBuild=false
/p:PostBuildEventUseInBuild=false`によるフルビルド+リンク(Release|Win32、
`NKMM_USE_MIMALLOC_OVERRIDE`有効)を、いずれも0エラーで確認した。実機での
長時間動作確認(メモリリーク・デバッグヒープ機能との相性等)は未実施。

## 既知の制約

- CRTのデバッグヒープ機能(`_CRTDBG_MAP_ALLOC`による`_malloc_dbg`等への自動置換)
  との組み合わせは未検証。本プロジェクトはこれを使っていないため影響なしと
  判断している
- `HeapAlloc`/`HeapFree`等、Win32 APIを直接呼んでいる箇所は対象外(CRTの
  malloc/freeとは別系統のため、mimallocのCRTオーバーライドの対象にはならない)
