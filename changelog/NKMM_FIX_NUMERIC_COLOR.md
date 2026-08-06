# NKMM_FIX_NUMERIC_COLOR 削除レポート

対象フラグ: `NKMM_FIX_NUMERIC_COLOR`（削除。導入 20170421、無効化 20260728、コード削除 20260806）
対象ファイル:

- `sakura_core/my_config.h`
- `sakura_core/view/colors/CColor_Numeric.cpp`

---

## 背景

数値の色分け判定(`IsNumber()`)には、元々2系統の実装があった。

1. 手書きの文字ループ版(`#else`節。10進/16進/浮動小数点を1文字ずつ走査)
2. 正規表現版(`REGEX_MODE`マクロで`std::regex`/`boost::regex`/`BREGEXP(bregonig.dll)`/
   `PCRE2`の4エンジンを切り替え可能。既定は`NKMM_FIX_NUMERIC_COLOR`定義時に有効)

正規表現版は2017/04/21に導入され、以後`REGEX_MODE==3`(PCRE2、`NKMM_FIX_REGEXP_FALLBACK`
経由)の追加(20260720)などで拡張されていたが、2026/07/28に次の理由で無効化された
(`my_config.h`の該当コメントより)。

- `IsNumber()`の呼び出し元`CColor_Numeric::BeginColor()`は、画面内の数値になりうる
  全位置に対して高頻度で呼ばれる。
- `REGEX_MODE==2/3`の呼び出し方は`CRegexFallback::BMatchEx`(相当)に毎回
  `str != nullptr`で渡ってしまうため、6パターン(`sPattern[]`)全てを呼ぶたびに
  正規表現をゼロからコンパイル(+JIT)し直す。
- さらにループ1周ごとに直前のコンパイル結果(`BREGEXP_W_Fallback`、JIT実行可能
  メモリ含む)を解放せずに上書きするため、リークする。
  `CRegexKeyword.cpp`のように「1回コンパイルして使い回す」設計になっていない。
- この呼び出しパターンのままでは速くしようがなく、固定・小規模な文法には元々あった
  手書きの文字ループで十分かつ確保/解放が一切ないため、正規表現版を無効化して
  文字ループ版に戻した。

無効化から日を置いても再度有効化される見込みがなく(呼び出しパターン自体を
`CRegexKeyword.cpp`同様のコンパイル結果キャッシュ方式に作り直さない限り同じ問題が
再発するため)、4エンジン分の切り替えマクロ一式を保守し続けるコストに見合わないと
判断し、コード自体を削除した。文字ループ版(`IsNumber()`の`#else`節だった実装)のみを
唯一の実装として残している。

## 削除した実装(参考: 削除直前の全文)

`sakura_core/view/colors/CColor_Numeric.cpp` 冒頭のエンジン選択部:

```cpp
#ifdef NKMM_FIX_NUMERIC_COLOR
#define REGEX_MODE (3)  // 0:std::regex
                        // 1:boost::regex
                        // 2:BREGEXP
                        // 3:PCRE2 (要NKMM_FIX_REGEXP_FALLBACK。bregonig.dllの
                        //   有無に関わらず常時使用可能。20260720追加)
#if REGEX_MODE == 0
	#include <regex>
	using namespace std;
#elif REGEX_MODE == 1
	#pragma comment(lib, "libboost_regex.lib")
	#include <boost/regex.hpp>
	using namespace boost;
#elif REGEX_MODE == 2
	#include "window/CEditWnd.h"
#elif REGEX_MODE == 3
	#ifdef NKMM_FIX_REGEXP_FALLBACK
		#include "extmodule/CRegexFallback.h"
	#else
		#error REGEX_MODE == 3 (PCRE2) を使うには NKMM_FIX_REGEXP_FALLBACK の定義が必要です
	#endif
#endif
#endif // NKMM_
```

`IsNumber()`内、`REGEX_MODE`ごとのマッチングマクロ定義とパターン表・検索ループ:

```cpp
static int IsNumber(const CStringRef& cStr, int offset)
{
#ifdef NKMM_FIX_NUMERIC_COLOR
	const wchar_t *p2 = cStr.GetPtr() + offset;
	const wchar_t *q2 = cStr.GetPtr() + cStr.GetLength();

#if REGEX_MODE == 0  // std::regex
	using _regex = wregex;
	using _match = wcmatch;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            regex_search(p, q, match, pt)
	#define _REG_STARTP(p)      (0)
	#define _REG_ENDP(match)    match.length(0)
	#define _REG_INIT(p)
	#define _REG_FREE(p)
	#define PREFIX
	#define SUFIX
	#define REGSTR(x)           L##x
	#define REGEX(x)            _regex(REGSTR(x))
#elif REGEX_MODE == 1  // boost::regex
	using _regex = wregex;
	using _match = wcmatch;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            regex_search(p, q, match, pt)
	#define _REG_STARTP(p)      (0)
	#define _REG_ENDP(match)    match.length(0)
	#define _REG_INIT(p)
	#define _REG_FREE(p)
	#define PREFIX
	#define SUFIX
	#define REGSTR(x)           L##x
	#define REGEX(x)            _regex(REGSTR(x))
#elif REGEX_MODE == 2  // BREGEXP
	using _regex = std::wstring;
	using _match = BREGEXP_W*;
	#define _REG_IS_AVAILABLE() CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.IsAvailable()
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.BMatch(pt.c_str(), p, q, &(match), msg)
	#define _REG_STARTP(p)      p
	#define _REG_ENDP(match)    match->endp[0]
	#define _REG_INIT(p)        p = nullptr
	#define _REG_FREE(p)        if (p) { CEditDoc::GetInstance(0)->m_pcEditWnd->GetActiveView().m_CurRegexp.BRegfree(p); }
	#define PREFIX              "/"
	#define SUFIX               "/k"
	#define REGSTR(x)           L"" PREFIX ##x SUFIX
	#define REGEX(x)            _regex(REGSTR(x))
#elif REGEX_MODE == 3  // PCRE2 (bregonig.dllの有無に関わらず常時利用可能。20260720追加)
	using _regex = std::wstring;
	using _match = BREGEXP_W*;
	#define _REG_IS_AVAILABLE() (1)
	#define _REG_ENTRY(p, c)    (c == 0 || ::wcschr(p, c))
	#define _REG_SEARCH(pt, p, q, match, msg) \
	                            RegexFallback::BMatch(pt.c_str(), p, q, &(match), msg)
	#define _REG_STARTP(p)      p
	#define _REG_ENDP(match)    match->endp[0]
	#define _REG_INIT(p)        p = nullptr
	#define _REG_FREE(p)        if (p) { RegexFallback::BRegfree(p); }
	#define PREFIX              "/"
	#define SUFIX               "/k"
	#define REGSTR(x)           L"" PREFIX ##x SUFIX
	#define REGEX(x)            _regex(REGSTR(x))
#else  // std::regex
	static_assert(0);
#endif

	static const struct {
		wchar_t enter; // 最低条件
		bool    term;  // 検索グループの終端
		_regex  exp;   // 式
	} sPattern[] = {
		{L'e', false, REGEX("^[0-9]+\\.[0-9]*([eE][-+][0-9]+)([fF]?)")},  // 1e-2
		{L'e', true,  REGEX("^(\\.[0-9]+)([eE][-+][0-9]+)([fF]?)")},      // .12e+2

		{L'.', false, REGEX("^([0-9]+\\.[0-9]*)([fF]?)")},                // 1.0f 1.f 1.
		{L'.', true,  REGEX("^(\\.[0-9]+)([fF]?)")},                      // .1f .1

		{0,    false, REGEX("^0x[0-9a-fA-F]+")},                          // 0x123
		{0,    true,  REGEX("^[0-9]+([uUlL]{0,2})")},                     // 123
	};

	if (_REG_IS_AVAILABLE()) {
		int pos = 0;
		wchar_t szMsg[80] = {}; //!< エラーメッセージ
		_match match;
		_REG_INIT(match);

		for (auto && re : sPattern) {
			if (_REG_ENTRY(p2, re.enter)) {
				if (_REG_SEARCH(re.exp, p2, q2, match, szMsg)) {
					pos = std::max<int>(_REG_ENDP(match) - _REG_STARTP(p2), pos);
				}
				if (re.term) {
					if (pos > 0) break;
				}
			}
		}

		_REG_FREE(match);
		return pos;
	} else {
		// 正規表現ライブラリが読み込まれていない
		// xxx そのまま通常の方法で判定する
		return 0;
	}
#else
	/* ここから文字ループ版(削除後も残る唯一の実装) */
	...
#endif // NKMM_
}
```

(文字ループ版本体は削除しておらず現行の`CColor_Numeric.cpp`にそのまま残っている。
上記の`...`部分はこのレポートでは省略。)

## 削除後の状態

- `NKMM_FIX_NUMERIC_COLOR`マクロの`#define`は`my_config.h`から削除(コメント欄は
  経緯を残すため残置し、本レポートへのリンクを追記)。
- `CColor_Numeric.cpp`は`#ifdef NKMM_FIX_NUMERIC_COLOR` / `#else` / `#endif`の
  分岐ごと削除し、文字ループ版のみが無条件でビルドされる状態にした。
- `REGEX_MODE`・`_REG_*`マクロ群、および`<regex>`/`boost/regex.hpp`/
  `extmodule/CRegexFallback.h`のインクルードもこのファイルからは消えた
  (`NKMM_FIX_REGEXP_FALLBACK`自体は検索/置換/Grep/構文強調キーワードの
  フォールバックとして引き続き使われており、無関係。詳細は
  `changelog/NKMM_FIX_REGEXP_FALLBACK.md`参照)。

将来、`CRegexKeyword.cpp`と同じ「コンパイル結果をキャッシュして使い回す」設計で
作り直したくなった場合は、このレポートまたはGit履歴(本削除コミットの直前)から
上記実装を復元できる。
