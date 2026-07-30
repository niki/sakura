# NKMM_FIX_NUMERIC_LANG_LITERAL 修正レポート

対象フラグ: `NKMM_FIX_NUMERIC_LANG_LITERAL`（新規。旧`NKMM_FIX_NUMERIC_CPP_LITERAL`を
タイプ別対応に拡張してリネーム）
対象ファイル(主なもの):

- `sakura_core/view/colors/CColor_Numeric.h`（`ENumericLang`、`Update()`のオーバーライド、`m_eLang`）
- `sakura_core/view/colors/CColor_Numeric.cpp`（`IsNumberXxx()`各種と共通補助関数、`BeginColor()`の分岐)
- `sakura_core/my_config.h`（フラグ定義）

---

## 背景

数値ハイライト処理(`CColor_Numeric`/`IsNumber()`)は全タイプ共通の単一実装であり、
C++固有の記法(2進数リテラル、桁区切り記号、u/U・ll/LLサフィックス等)を
カバーできていなかった([NKMM_FIX_NUMERIC_CPP_LITERAL.md](旧レポート、本ファイルに統合)参照)。

`IsNumber()`は全タイプで共有されているため、同じ制約はC/C++以外の言語
(Java, C#, JavaScript, PHP, Python, Ruby, Perl, VB, Pascal, CSS, Asm, Rich)にも
及んでいた。かつ言語ごとに数値リテラルの文法自体が異なる
(桁区切り記号がC++は`'`だがJava/C#/JavaScriptは`_`、Javaにu/Uサフィックスは
存在しない、JavaScriptはBigIntサフィックス`n`を持つ等)ため、C++用の実装を
そのまま他言語に適用しても正しくならない。

対応方針として、「数値ハイライトが既定ONの13タイプ」を対象に、3〜4言語ずつ
段階的に専用実装を追加していくことにした。

- 第1弾: C/C++, Java, C#, JavaScript(Cライク・波括弧系言語)
- 第2弾: PHP, Python, Ruby, Perl(動的スクリプト系言語)
- 第3弾: Visual Basic, Pascal, CSS, アセンブラ(方言があいまいな/毛色の違う言語)

本レポートは全バッチの内容をまとめて記載する。数値ハイライトが既定ONの
13タイプのうち、リッチテキスト(RTF)を除く12タイプに専用実装を追加した。

## 対応

`IsNumber()`自体は変更せず、`CColor_Numeric::BeginColor()`が現在のタイプに
応じて`IsNumberXxx()`を呼び分ける形にした。

```cpp
enum ENumericLang{
    ENUMLANG_GENERIC = 0,	// 専用実装なし。IsNumber()のみ
    ENUMLANG_CPP,
    ENUMLANG_JAVA,
    ENUMLANG_CSHARP,
    ENUMLANG_JAVASCRIPT,
    ENUMLANG_PHP,
    ENUMLANG_PYTHON,
    ENUMLANG_RUBY,
    ENUMLANG_PERL,
    ENUMLANG_VB,
    ENUMLANG_PASCAL,
    ENUMLANG_CSS,
    ENUMLANG_ASM,
    // リッチテキスト(RTF)は対象外(後述)。ENUMLANG_GENERICのまま
};
```

現在のタイプの判定は`CColor_Numeric::Update()`(タイプ切り替え時に1回だけ
呼ばれる)で`STypeConfig::m_szTypeName`を各タイプの既定名("C/C++", "Java",
"C#", "JavaScript", "PHP", "Python", "Ruby", "Perl", "Visual Basic", "Pascal",
"CSS", "アセンブラ")と比較し、`m_eLang`にキャッシュしている。高頻度に呼ばれる
`BeginColor()`側は、このキャッシュ済みの値で関数ポインタを選んで呼ぶだけ。

各言語のヘルパー関数は共通化した:

- `IsDigitOfBase(c, nBase)` — 2/8/10/16進の数字判定
- `SkipDigitsWithSeparator(p, q, nBase, chSep)` — 桁区切り記号(言語ごとに`'`/`_`)を
  考慮した数字列の読み進め
- `SkipIntSuffix(p, q, nMaxLen)` — u/U・l/Lの組み合わせサフィックス(最大文字数は言語ごと)
- `SkipOneCharOf(p, q, chSet)` — 単独サフィックス(D/d, n等)
- `ContainsFloatMarker(p, n)` — 浮動小数点形(`.`または`e`/`E`を含むか)の判定
- `TryPrefixedLiteral(p, q, chFirst, chLower, chUpper, nBase, chSep)` —
  `0b`/`0o`/`&H`/`&O`等の接頭辞付きリテラルの判定(`IsNumber()`は`0x`しか
  知らない)。第3弾でVBの`&`/Pascalの`$`にも対応するため、接頭辞の1文字目
  (`chFirst`)も引数化した(元は`'0'`固定)
- `SkipRubyNumericSuffix(p, q)` — Ruby専用。有理数/虚数サフィックス(r→iの順)
- `TrySuffixedLiteral(p, q, nBase, pszSuffixSet, bFirstMustBeDecimal)` —
  アセンブラ専用。MASMの「数字列の後ろに進数サフィックスが必須」という
  接尾辞方式の判定(他言語の接頭辞方式とは形が逆)
- `SkipCssUnit(p, q)` — CSS専用。数値直後の単位(px, em, %等)の判定

### 言語別の差分

| 言語 | 2進数 | 8進数(明示) | 16進数 | 桁区切り | サフィックス |
|---|---|---|---|---|---|
| C/C++ | `0b`/`0B` | — | `0x`(IsNumber()) | `'` | 整数u/U・l/L・ll/LL(最大3文字)、Lはd==0以外でも延長 |
| Java | `0b`/`0B` | — | `0x`(IsNumber()) | `_` | L/lのみ(2進数の後にも付く)、D/dを追加(F/fはIsNumber()側) |
| C# | `0b`/`0B` | — | `0x`(IsNumber()) | `_` | 整数u/U・l/L(最大2文字)、浮動小数点d/D・m/M追加(F/fはIsNumber()側) |
| JavaScript | `0b`/`0B` | `0o`/`0O` | `0x`(IsNumber()) | `_` | BigIntの`n`のみ |
| PHP | `0b`/`0B` | `0o`/`0O` | `0x`(IsNumber()) | `_` | なし |
| Python | `0b`/`0B` | `0o`/`0O` | `0x`(IsNumber()) | `_` | 複素数j/J |
| Ruby | `0b`/`0B` | `0o`/`0O` | `0x`(IsNumber()) | `_` | 有理数r/虚数i(常にr→iの順) |
| Perl | `0b`/`0B` | `0o`/`0O` | `0x`(IsNumber()) | `_` | なし |
| VB | `&B`(VB.NET) | `&O` | `&H` | なし(`_`は行継続文字) | 型宣言文字`%&@!#`(単独1文字) |
| Pascal | `%` | `&` | `$` | `_`(FreePascal拡張) | なし |
| CSS | — | — | — | — | 単位(px, em, %等)を数値の一部として色付け |
| アセンブラ | `1010b`/`1010y`(接尾辞) | `17o`/`17q`(接尾辞) | `0FFh`(接尾辞) | なし | 明示的な10進数サフィックスd/D |

C#は浮動小数点形かどうかを`ContainsFloatMarker()`で判定し、整数サフィックスと
浮動小数点サフィックスを排他的に付与する(C++/Javaは`IsNumber()`側の
`d==0`ゲートにそのまま乗っているため、この判定は不要)。

PHP/Python/Ruby/Perlは、従来の`0777`形式の暗黙8進数を`IsNumber()`がそのまま
(10進数の見た目のまま)処理する点は変えていない。明示的な`0o`/`0O`接頭辞の
方だけを追加対応した。

VBは`&H`/`&O`/`&B`いずれも接頭辞の1文字目が`&`で共通するが、2文字目
(H/O/B)で排他的に判定されるため誤認識はない。ただし`&`は数値の後ろに
付くLong型宣言文字でもあるため(例`100&`)、こちらは別途サフィックスとして
扱っている(位置が異なるため衝突しない)。

アセンブラは接尾辞方式そのものが他言語と根本的に形が違うため、`IsNumber()`
への委譲は10進数の場合のみ行っている。また16進数の判定を8進数/2進数より
先に行う必要がある — `B`は2進数サフィックスであると同時に有効な16進数字でも
あるため、順序を間違えると`0BEEFh`のようなコードを`0B`(2進数の0)と
誤認識してしまう。

無効化すれば、全タイプで従来通り`IsNumber()`のみが使われる。

## 既知の未対応(スコープ外、全言語共通)

- 桁区切り記号が小数点をまたぐ場合(例 `1'234.5`, `1_234.5`)。整数部が
  桁区切りで伸ばされた直後に小数点が来ると、その時点で走査が止まり
  小数部が色付けされない。
- マイナス符号付き16進数/2進数(例 `-0x89a`, `-0b101`)。`IsNumber()`の`-`分岐が
  元々`0x`/`0b`を認識しない既存の問題を引き継いでおり、`-0`だけが誤って
  数値扱いされる。
- C++の`i64`サフィックス(MS独自拡張、非推奨)。
- サフィックスを持たない言語(JavaScript, PHP, Perl, Pascal)でも、`IsNumber()`
  自身は常にL/l/F/fサフィックスを1文字消費してしまう場合がある(該当言語には
  元々存在しない記法)。そのようなコードが実質存在しないため実害はない想定。
- Ruby/Perlの`?`付き数値リテラル(`?a`のような文字リテラル記法)や、Perlの
  `v`文字列(バージョン文字列、`v1.2.3`)は今回のスコープ外。
- VB.NET固有のS/I/L/D/F/Rおよびアンダースコア付きサフィックス(US/UI/UL)。
  VB6/VBA/VB.NETで方言差が大きいため、共通の記号サフィックス(`%&@!#`)のみ
  対応した。
- CSSの`#RRGGBB`色指定はCSS文法上「数値」ではない別トークン(hash-token)
  であり対象外。
- アセンブラの浮動小数点定数(REAL4/REAL8等)、およびNASM/GAS等MASM以外の
  方言。CType_Asm.cppのアウトライン解析が`proc`/`endp`キーワードを使って
  いることからMASM系の方言と判断した。

## 対応見送り: リッチテキスト(RTF)

数値ハイライトが既定ONの13タイプのうち、RTFのみ専用実装を作らなかった。
RTFの数値は制御ワードのパラメータ(例 `\fs24`, `\li-360`)としてのみ現れ、
常に「符号+10進整数」の形しか取らない。桁区切り記号・進数接頭辞・
サフィックスの概念が一切存在しないため、既存の`IsNumber()`だけで
過不足なく判定できる。専用のIsNumberRich()を作っても何もしないラッパーに
しかならないため、意図的に作らずジェネリック実装のまま(`ENUMLANG_GENERIC`)
とした。

## 動作確認について

`Release|Win32`構成でのビルド成功に加え、実機確認済み。対応12タイプ分の
サンプルファイルをリポジトリ直下の[numeric_sample/](../numeric_sample/)
に保存してあり、実際にsakura.exeで開いて本文中の表に挙げた項目
(2進数リテラル・桁区切り記号・各種サフィックス等、コード中`← 新規対応`と
コメントした箇所)がすべて数値の色でハイライトされることを確認した。
特にアセンブラの`0BEEFh`(16進数字と2進数サフィックスの曖昧性)、Rubyの
`1ri`(有理数+虚数の組み合わせ)は意図通り動作した。

タイプ別設定の「半角数値」表示がデフォルトでは基本色に対して無効(または
既定色がテキスト色と同系統)になっている場合、色の違いが目視で分かりにくい。
確認時は`sakura.ini`の`[Types(0)]`セクションの`C[NUM]`を
`C[NUM]=1,0,#eb0000,bg,0`(表示ON・赤)に変更して視認性を上げた。

今後、このパッチや`IsNumber()`/`CColor_Numeric.cpp`まわりに手を入れた際は、
このサンプル一式を開いて色分けが壊れていないか目視で再確認できる。
