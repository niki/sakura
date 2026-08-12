# NKMM_FIX_KEYWORDSET_UI 組み込みキーワード化 対応レポート

対象フラグ: `NKMM_FIX_KEYWORDSET_UI`（既存フラグの機能拡張）+ `BUILD_OPT_IMPKEYWORD`（既存だが無効化されていたスイッチを有効化）+ `NKMM_FIX_REGEX_OUTLINE_EMBED`（新規、フェーズ8・9の`.rkw`/`.rule`埋め込み分。`.kwd`とは仕組みが別物のため別フラグに分離）

対象ファイル(主なもの):

- `sakura_core/types/CType.cpp`（`GetEmbeddedKeywordArr()`新設、`InitKeywordFromList()`/`InitKeyword()`両方へのフォールバック配線）
- `sakura_core/types/CType_Cpp.cpp` 他15ファイル（元々組み込み配列を持っていたタイプ。配列本体を`generated/*.inc`へ外出し）
- `sakura_core/types/CType_Css.cpp` / `CType_JavaScript.cpp` / `CType_Php.cpp` / `CType_Python.cpp` / `CType_Ruby.cpp` / `CType_Csharp.cpp`（新規に組み込み配列を追加した6タイプ）
- `sakura_core/types/CType_Dos.cpp`, `CTypeInit.h`（`BUILD_OPT_IMPKEYWORD`有効化）
- `sakura_core/CKeyWordSetMgr.h` / `.cpp`（`m_bKeyWordEmbeddedArr`、`SetKeyWordEmbedded()`/`GetKeyWordEmbedded()`追加）
- `sakura_core/env/CShareData.h`（`GetEmbeddedKeywordArr()`を`CShareData`の静的メンバとして公開）
- `sakura_core/env/CShareData_IO.cpp`（`ShareData_IO_KeyWords()`: 全セット組み込み時は`[KeyWords]`セクションを書かない）
- `sakura_core/prop/CPropCommon.h` / `CPropComKeyword.cpp`（「更新」ボタンでの既定キーワード確認ダイアログ、セット名コンボボックスの`(embed)`表示）
- `sakura_core/String_define.h` / `sakura_core/sakura_rc.rc` / `sakura_lang_en_US/sakura_lang_rc.rc`（確認ダイアログの文言リソース追加）
- `sakura_keyword/`（新設、`*.kwd`のマスターコピー27ファイル + README.md）
- `sakura_core/types/generated/`（新設、`.inc`生成物。**gitignore対象**、後述）
- `tools/GenerateKeywordInc.ps1`（新設、`.inc`再生成スクリプト）
- `sakura_core/my_config.h`

---

## 背景・目的

サクラエディタの強調キーワード機能は、`sakura.keywordset.csv`が指す`Keyword\*.kwd`（実行ファイルと同じ階層に配布）を読み込むことで、タイプ別（C/C++、HTML、…）のキーワードハイライトを実現している。しかし、この`Keyword\`一式は配布物（zip等）としてのみ提供され、exeと一緒にコピーし忘れる／削除される／パスがずれるなどの理由で**存在しないと、実装済みのはずのタイプでも強調キーワードが一切効かなくなる**という弱点があった。

目的は、**実装済みタイプについては外部ファイルが無くてもソースに組み込んだデフォルトキーワードで動作するようにする**こと。既存の`Keyword\*.kwd`が存在する場合はそちらを優先し、無い場合だけ組み込みキーワードにフォールバックする。

---

## 読み込み優先順位（重要・分かりにくいので図解）

強調キーワードの実際の中身がどこから来るかは、単純な一本の優先順位ではなく、**`sakura.keywordset.csv`の有無で経路そのものが分岐**する。混同しやすいので明記しておく。

```
起動時
  │
  ├─ sakura.keywordset.csv が存在する？(毎起動チェック。CShareData_IO.cpp:166/182/304)
  │
  ├─ [ある] → InitKeywordFromList() が毎回、全キーワードセットを"まるごと作り直す"
  │            (以前の内容・iniの[KeyWords]セクションは一切見ない/書かない)
  │              1. Keyword\<file>.kwd が開ければそれを使う
  │              2. 開けなければ組み込み配列にフォールバック
  │
  └─ [ない] → InitKeywordFromList() は一切呼ばれない。代わりに:
               │
               ├─ sakura.ini に [KeyWords] セクションがある？
               │  (ShareData_IO_KeyWords, CShareData_IO.cpp:3219-3244)
               │
               ├─ [ある] → ini に最後に保存されていた内容だけで全セットを再構築。
               │            中身が元々どこ由来だったかは問わない。これが最優先で決着。
               │
               └─ [ない] → 起動時に既に済んでいる InitShareData() 内の
                            InitKeyword(bInit=false) によるベースラインがそのまま有効。
                              - 対応22タイプ中21タイプ: 組み込み配列
                              - PHP2のみ: この時点で Keyword\php.kwd のインポートを
                                別途試みる(無ければ空のまま)
```

**要点**:
- csvが存在する間は、ini の `[KeyWords]` セクションへの読み書き自体が完全にスキップされる(`CShareData_IO.cpp:304`)ので、iniに古いキャッシュが溜まることもない。
- csvが無い場合だけ、iniのキャッシュが優先される。**iniに一度でも`[KeyWords]`セクションが書き込まれてしまうと、それ以降はcsvが無い限りずっとそのキャッシュが使われ続ける**(組み込みキーワードや`Keyword\`の更新は反映されない)。動作確認や不具合調査でこの経路に入っているか判断がつかない場合は、`sakura.ini`を直接開いて`[KeyWords]`セクションの有無を見るのが確実。
- 「外部kwdファイル vs 組み込み」のフォールバックと「iniキャッシュ vs 組み込み」のフォールバックは**同時には発生しない**、独立した2つの分岐であることに注意。

---

## 実装内容

### フェーズ1: 既存の埋め込みインフラの有効化 + フォールバック配線

調査の結果、このコードベースには元々`BUILD_OPT_IMPKEYWORD`という「内蔵キーワードを定義するにはこれを定義してください」という趣旨のスイッチが`CTypeInit.h`にコメントアウトされたまま残っており、C/C++以外の15タイプ（HTML, PL/SQL, COBOL, Java, CORBA IDL, AWK, MS-DOS batch, Pascal, TeX, TeX2, Perl, Perl2, Visual Basic, Visual Basic2, Rich Text）は既に`g_ppszKeywordsXXX[]`という組み込み配列をソースに持ちながら、このスイッチがOFFのため**コンパイルから除外**されていた（C/C++だけは無条件でコンパイルされる特例だったため唯一有効だった）。

1. `BUILD_OPT_IMPKEYWORD`を有効化（`CTypeInit.h`）。これだけで上記15タイプの組み込み配列が実行ファイルにリンクされるようになった。
2. `CType.cpp`に`GetEmbeddedKeywordArr(filename, ...)`を新設。`sakura.keywordset.csv`が指すファイル名（例: `cpp.kwd`）から、対応する組み込み配列を検索する。
3. `InitKeywordFromList()`（csv駆動の初期化経路）内で、`CImpExpKeyWord::Import()`が外部ファイルのオープンに失敗した場合のみ、`GetEmbeddedKeywordArr()`にフォールバックするよう変更。外部ファイルが存在すればそちらを優先。

### フェーズ2: 元々組み込みが無かった6タイプの追加

CSS / JavaScript / JavaScript2 / PHP / python / Ruby1-4 / C# / C# content は、後年追加されたタイプで元々ソースに組み込み配列を持っていなかった。配布用`Keyword\`フォルダの実ファイルから、コメント行・空行を除いた実キーワードのみを手作業で書き起こして各`CType_*.cpp`に追加した。

- CSS: 700語、JS: 41語、JS2: 402語、PHP: 88語、python: 39語、Ruby1〜4: 計419語、C#: 77語、C# content: 48語
- **PHP2（`php.kwd`）は対象外**。中身はPHP組み込み関数一覧で1万1千語超あり、全キーワードセット共有の格納領域（`CKeyWordSetMgr.h`の`MAX_KEYWORDNUM=15000`）を大きく圧迫するため、埋め込みを見送った。実行時に`Keyword\php.kwd`が無ければPHP2セットは空のまま。

### フェーズ3: 組み込みインフラの自動生成化

手書き転記はミスの元であり保守もできないため、`.kwd`→`.inc`の変換をスクリプト化した。

- `tools/GenerateKeywordInc.ps1`: 各`.kwd`から空行・`//`コメントを除いた行を`L"キーワード",`形式で`sakura_core/types/generated/*.inc`へ出力。各`CType_*.cpp`側は`#include "generated/xxx_keywords.inc"`で読み込むだけにし、配列本体を保持しない。
- `sakura_keyword/`: `*.kwd`のマスターコピーを新設し、git管理下に置いた（`sakura_lang/`と同階層）。従来配布物としてのみ存在した`Keyword\`とは別物で、ビルド時にソースへ変換するための素材。README.mdに対応表・再生成方法・注意点を記載。
- 新規6タイプだけでなく、フェーズ1で有効化した元の16タイプについても同じ`#include`方式へ統一（既存の埋め込み内容はそのまま`.inc`化しただけで、`sakura_keyword`の現在の`.kwd`内容とは意図的に未同期。詳細はREADME.md参照）。

### フェーズ4: 「更新」ボタンでの既定キーワードへのフォールバック

共通設定「強調キーワード」タブの「更新」（`Reload_List_KeyWord`、セット単位の再読み込み）ボタンでファイル読み込みに失敗した場合、組み込みキーワードがあればそれを使うかどうかをYes/Noで確認するようにした。「はい」なら`GetEmbeddedKeywordArr()`で取得した配列を`SetKeyWordArr()`で設定する。文言は`STR_PROPCOMKEYWORD_RELOAD_DEFAULT`として日英両方のリソースに追加。

### フェーズ5: `(embed)`表示

キーワードセットの現在の内容が組み込み由来かどうかを`CKeyWordSetMgr::m_bKeyWordEmbeddedArr[]`で追跡し、「強調キーワード」タブのセット名コンボボックスに`セット名 (embed)`と表示するようにした。**表示上の飾りのみ**で、実際のセット名（`GetTypeName()`、リネームダイアログや削除確認で使う値）は一切変更しない。

### フェーズ6: スクリプトの堅牢化

`.kwd`にバックスラッシュ・ダブルクォートを含む行（TeXの`\AA`、RTF制御語の`\ansi`等）があると生成が停止していたのを、C++文字列リテラルとして正しくエスケープするよう修正。また、対応する`.kwd`が1件でも見つからないとスクリプト全体が止まっていたのを、その1件だけ警告付きでスキップし残りは生成を続けるよう変更。

### フェーズ7: 全セット組み込みのときiniへ書き込まない

上記「読み込み優先順位」の要点にある通り、csvが無い状態で一度でも`sakura.ini`に`[KeyWords]`セクションが書き込まれると、以降はcsvが無い限りそのキャッシュが使われ続け、組み込みキーワードのソース更新が反映されなくなる。これを避けるため、`ShareData_IO_KeyWords()`の書き込み側で、**現在の全キーワードセットが組み込みのままなら`[KeyWords]`セクション自体を書かない**ようにした。

読み込み側は`nKeyWordSetNum`から`0..N-1`の連番キーを前提に全セットを一括再構築しており、各タイプの`m_nKeyWordSetIdx[]`はこの位置(インデックス)に依存する。そのため「組み込みのセットだけ選んで書かない」といった部分スキップは位置ズレを起こし危険（1つ飛ばすと後続タイプのキーワードセット紐付けが全部ずれる）。**1つでもユーザーがカスタマイズした(組み込みでない)セットがあれば、従来通り全セットを書く**という「全か無か」の判定にした。

`ShareData_IO_2(bRead=false)`（書き込み経路）は`ReadProfile()`を呼ばず`CDataProfile`を空から組み立て、`WriteProfile()`でini全体を書き直す実装だったため、`IOProfileData()`の呼び出しを丸ごとスキップするだけで済み、古い`[KeyWords]`内容が残留する心配も無いことを確認した上で実装した。

### フェーズ8: 正規表現キーワード(`.rkw`)への対応拡張

`.kwd`(強調キーワード)とは別に、`.rkw`(正規表現キーワード。`RxKey[999]=ColorName,/pattern/flags`形式、`CImpExpRegex::Import()`が読む書式)も同じ`tools\GenerateKeywordInc.ps1`で扱えるようにした。`$Targets`の各要素は`File`(.kwd)か`Regex`(.rkw)のどちらか一方を持ち、スクリプト側で分岐する。

- `.kwd`は`CKeyWordSetMgr::SetKeyWordArr()`向けの`L"キーワード",`データ配列として`<name>_keywords.inc`に出力(従来通り)。
- `.rkw`は`STypeConfig::m_RegexKeywordArr`/`m_RegexKeywordList`向けに、`RegexAdd( pType, keywordPos, idx++, COLORIDX_XXX, L"/pattern/flags" );`という**関数呼び出し文の並び**として`<name>_regex.inc`に出力する(データ配列ではない)。消費側は`CType_JavaScript.cpp`が元々手書きしていた`RegexAdd()`呼び出し列と同じ形で、`InitTypeConfigImp()`内の`int keywordPos = 0; int idx = 0; pType->m_bUseRegexKeyword = true;`の直後に`#include`する。
- `.rkw`のColorName(3文字コード、例: `KW1`→`COLORIDX_KEYWORD1`、`RK1`→`COLORIDX_REGEX1`、`WQT`→`COLORIDX_WSTRING`)からCOLORIDX定数名への対応表は、`CColorStrategy.cpp`の`g_ColorAttributeArr`と`EColorIndexType.h`のenum定義を突き合わせて`$ColorNameMap`としてスクリプトに持たせた(順序で対応しているテーブルなので、片方だけ増減すると対応がずれる点に注意)。

**`.rkw`には`.kwd`のような自動フォールバック機構が無いことに注意**: `.kwd`は`sakura.keywordset.csv`経由で起動のたびに自動読み込みされ、無ければ組み込みにフォールバックするが、`.rkw`はタイプ別設定「正規表現キーワード」タブの手動インポートでのみ読み込まれる仕組みで、そもそも自動読み込みの起点が無い。そのため`.rkw`の組み込みは「ソースに直接書けば動く」形にする以外の選択肢が無く、実行時フォールバックの概念自体が存在しない。

実際に`sakura_keyword/cpp.rkw`(11件)を追加し、`CType_Cpp.cpp`の`InitTypeConfigImp()`に`generated/cpp_regex.inc`を組み込んだ。生成結果が元の`cpp.rkw`と1件ずつ一致すること(ColorNameのマッピング、バックスラッシュの二重エスケープ含む)を目視確認し、ビルドが通ることを確認済み。

続けて`perl.rkw`(25件)・`Ruby.rkw`(33件)も追加した。Perlは`CType_Perl.cpp`に既存の`RegexAdd()`呼び出しが無かったため、CPPと同じ手順でそのまま`#include`を追加するだけで済んだ。

Rubyは事情が異なった。**`CType_Ruby.cpp`には元々`Ruby.rkw`と同内容の`RegexAdd()`手書き列(33件)が既に存在していた**(このリポジトリではRubyタイプの正規表現キーワードは元から実装済みだった)。単純に`#include`を追加すると全件が二重に登録されてしまうところだった。1行ずつ突き合わせたところ、32件は完全一致だったが**1件だけ食い違いがあった**: `&`始まりのブロック引数パターン(`/\&\$*\w([\w\d']|::)*/k`)の色が、手書き側は`COLORIDX_REGEX2`、`Ruby.rkw`側は`RK3`(`COLORIDX_REGEX3`)。

どちらが正しいか判断がつかなかったため、ユーザーに確認したところ「手書き側(実際に長年使われてきた挙動)を正として、Ruby.rkwへ逆変換してから使う」という方針が示された。そこで、

1. `CType_Ruby.cpp`の33件の`RegexAdd()`呼び出しをパースし、C++文字列リテラルのエスケープを逆変換(`\\`→`\`、`\"`→`"`)、COLORIDX定数名を3文字コードへ逆マッピングする一時スクリプトを書いて`Ruby.rkw`を上書き生成した。
2. 生成した`Ruby.rkw`を`tools\GenerateKeywordInc.ps1`に通し、出力が手書きブロックとバイト単位で完全一致すること(改行コードの差を除く)を確認した(往復検証)。
3. `CType_Ruby.cpp`の手書き33行を`#include "generated/ruby_regex.inc"`に置き換えた。

この一時スクリプト自体も、最初はスクリプトファイル自身がBOM無しUTF-8で保存されており、埋め込んだ日本語コメントがPowerShellにShift-JISとして誤読され文字化けする問題が起きた(`tools\GenerateKeywordInc.ps1`のときと同じ原因)。UTF-8 BOM付きで保存し直して解決した。

### フェーズ9: アウトライン解析ルール(`.rule`)への対応拡張

`.kwd`/`.rkw`とはさらに別物の`.rule`(アウトライン解析ルール。`CDocOutline::ReadRuleFile()`が読む`key1,key2 /// GroupName,Lv=1`/`;Mode=Regex`等の書式)も同じ`tools\GenerateKeywordInc.ps1`で扱えるようにした(`$Targets`に`Rule`メンバーを追加)。

`.kwd`/`.rkw`との決定的な違いは、**`.rule`はCDocOutline自身がアウトライン表示のたびにファイル全体を都度パースする**実装だったこと。`.kwd`(`sakura.keywordset.csv`経由で起動時に1回読み込み、`CKeyWordSetMgr`に格納)や`.rkw`(手動インポートで`STypeConfig`のフィールドに一括変換)のように「あらかじめパース済みのデータ構造」へ変換する対象が無いため、1行ずつの解析は不要。そのぶんスクリプト側の実装は`.kwd`/`.rkw`より単純で、**ファイル内容をそのままC++11のraw文字列リテラル`LR"RULEDATA(...)RULEDATA"`として書き出すだけ**(エスケープ一切不要。ただし区切り文字列`)RULEDATA"`がファイル内容に含まれていたら生成時にエラーにする安全策を入れた)。

消費側も`.rkw`と同様に自動読み込みの起点が無いため、CType_*.cpp側を変更する必要が無い。その代わり、`CDocOutline::ReadRuleFile()`自体(`sakura_core/doc/CDocOutline.cpp`)を改修した:

- 埋め込み文字列3つ(JS/Ruby/PHP)を`#include`で定義し、`GetEmbeddedOutlineRule()`でファイル名(ディレクトリ部分を除いた末尾)から対応する文字列を引けるようにした。
- `ReadRuleFile()`本体は、`CTextInputStream_AbsIni`が実ファイルを開けなかった場合のみ、埋め込み文字列を`\n`区切りで分割した`std::vector<std::wstring>`にフォールバックする。既存のパース処理本体(`while`ループの中身)は一切変更せず、ループ条件と行取得部分だけを「ファイルから読むか埋め込み配列から読むか」で分岐させる形にした(`SplitEmbeddedRuleLines()`が`CTextInputStream_AbsIni::ReadLineW()`相当の`\r\n`/`\n`両対応の行分割を行う)。

ハマった点: `CDocOutline.cpp`は`sakura_core/doc/`に置かれており、生成物は`sakura_core/types/generated/`にあるため、`.kwd`/`.rkw`の消費側(`CType_*.cpp`、同じ`types/`配下)のような`#include "generated/xxx.inc"`という相対パスでは見つからない。`#include "../types/generated/xxx.inc"`と1階層上がる相対パスに直す必要があった。

`sakura_keyword/lua.rule`も置かれているが、このリポジトリに`CType_Lua.cpp`は存在しないため対応先が無く、対象外とした。

---

## 見つけて直した不具合

### 1. `CType_Php.cpp`のキーワードセット割り当てミス

```cpp
pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_PHP;
pType->m_nKeyWordSetIdx[0] = g_nKeywordsIdx_PHP2;  // ← 同じ[0]に代入、PHPが即座に上書きされていた
```

PHP（予約語）とPHP2（組み込み関数）が両方とも`m_nKeyWordSetIdx[0]`に代入されており、PHPのインデックスがPHP2で即座に上書きされ、**PHP予約語のセットはずっと強調表示に使われていなかった**（他タイプはRuby1-4/TeX/TeX2等含め全て`[0]`,`[1]`,...と正しく別スロットを使っていた）。`[0]`/`[1]`に修正。

### 2. 旧来の`InitKeyword()`経路が新規6タイプ分の組み込みキーワードを使っていなかった

`sakura.keywordset.csv`が存在しない場合に使われる旧来の`InitKeyword()`では、CSS/JS/JS2/PHP/python/Ruby1-4/C#/C# content（10タイプ、PHP2除く）が`BUILD_OPT_IMPKEYWORD`の値に関係なく`PopulateKeyword2`（外部ファイル読み込みのみ、組み込みフォールバック無し）に決め打ちされていた。フェーズ1のフォールバックは`InitKeywordFromList()`にしか配線しておらず、csvが無い環境ではこの10タイプだけ強調キーワードが効かないままだった。`PopulateKeyword2`→`PopulateKeyword`（`BUILD_OPT_IMPKEYWORD`時は`PopulateKeyword1`に展開）に変更して解消。

ユーザーの実機テスト（`sakura.keywordset.csv`をリネームして無効化した状態で確認）でこの経路の`(embed)`表示が出ないことから発覚。ただし実際には**既存の`sakura.ini`に以前保存された`[KeyWords]`セクションのキャッシュが優先して読み込まれる**という第3の経路（`ShareData_IO_KeyWords()`）があり、これは仕様上`(embed)`フラグの対象外（ini管理下に移った時点で「今まさに組み込み配列から読んでいる」わけではないため）。

### 3. MSVCはサイズ0の配列を許可しない（C2466）

スクリプト堅牢化の初期実装で「対応`.kwd`が無ければ空の`.inc`を生成」としたが、`const wchar_t* arr[] = {};`は`error C2466`でコンパイル不可なことを実際にビルドして確認。該当ターゲットの生成を丸ごとスキップする方式に変更した。

### 4. スクリプトのテスト実行が本番の`.inc`を上書きした事故

「`-KeywordDir`省略時に`sakura_keyword`を見るようにする」変更を実機確認のため引数無しで実行したところ、直前に追加したエスケープ修正が功を奏して`tex1.kwd`（バックスラッシュ含む）以降も生成が進み、**元々16タイプ分あった手書きの組み込み配列のうち9タイプ分（CPP, HTML, PLSQL, COBOL, JAVA, CORBA_IDL, AWK, BAT, PASCAL）を`sakura_keyword`の現行`.kwd`内容で意図せず上書き**してしまった（ユーザーからは「現在の埋め込み内容をそのまま`.inc`化してほしい、同期はまだしない」という要望を受けていたにもかかわらず）。

一度もコミットしていなかったため、`git show HEAD:sakura_core/types/CType_Xxx.cpp`から該当9ファイルの元の配列を再抽出し、行数が事故前と完全に一致することを確認した上で復元した。

### 5. Shift-JISファイルへのテキスト編集による文字化けリスク

`sakura_rc.rc`（日本語UI文字列）はShift-JIS（コードページ932）、BOM無しでエンコードされている。通常のテキスト編集ツールで書き換えると文字化けする恐れがあったため、PowerShellで明示的にCP932エンコードとして読み書きし、`git diff`で追加した1行以外に差分が無いことを確認してから反映した。

---

## 重要: 新しいビルド前提条件

`sakura_core/types/generated/*.inc`は`.gitignore`で除外されており、**git管理下に無い**（`generated/`、`*.inc`パターンが`.gitignore`に追加されている）。そのため、フレッシュな`clone`ではこれらのファイルが存在せず、`#include "generated/xxx_keywords.inc"`でビルドが失敗する。

ビルド前に一度、以下を実行して`.inc`一式を生成する必要がある。

```powershell
.\tools\GenerateKeywordInc.ps1
```

引数省略時はリポジトリ直下の`sakura_keyword\`（git管理下）を参照する。

---

## 対応タイプまとめ

PHP2を除く全22タイプ・27キーワードセットが、`Keyword\*.kwd`（配布物）が無くても組み込みキーワードで強調表示できる：

C/C++, HTML, PL/SQL, COBOL, Java, CORBA IDL, AWK, MS-DOS batch, Pascal, TeX, TeX2, Perl, Perl2, Visual Basic, Visual Basic2, Rich Text, C#, C# content, CSS, JavaScript, JavaScript2, PHP, python, Ruby1, Ruby2, Ruby3, Ruby4

PHP2のみ、従来通り`Keyword\php.kwd`が必要。

---

## 今後の拡張方法

あるタイプに新しい組み込みキーワードスロットを追加したい場合：

1. `sakura_keyword\`に対応する`.kwd`を追加する。
2. `tools\GenerateKeywordInc.ps1`の`$Targets`に1エントリ追加する。
3. `.\tools\GenerateKeywordInc.ps1`を実行して`.inc`を生成する。
4. 該当`CType_*.cpp`に配列宣言＋`#include`を追加し、`m_nKeyWordSetIdx[n]`に配線する。

空スロット（対応する`.kwd`が無いスロット）を`sakura.keywordset.csv`やUIのセット名一覧へ**先回りして登録することはしない**方針とした（未使用の空セットが強調キーワード設定ダイアログに並ぶのを避けるため）。あくまで上記の4ステップを踏んだ時点で初めて有効になる。

---

## 動作確認について

- 各フェーズごとにReleaseX64構成で`sakura.sln`をビルドし、エラー・新規警告が無いことを確認。
- `NKMM_FIX_KEYWORDSET_UI`のガード漏れが無いことを確認するため、`NKMM_USE_KEYWORDSET_CSV`を一時的に`0`にして（`NKMM_FIX_KEYWORDSET_UI`が未定義になる状態で）フルビルドし、エラーが出ないことを確認してから設定を元に戻した(フェーズ7追加時も同様に再確認)。
- 生成スクリプトは、実際の`sakura_keyword\`一式に対する生成に加え、意図的にファイルを1件除いた状態でも残り26件が正常に生成されエラーで停止しないことを確認。
- 実機での強調表示・ダイアログ操作（「更新」ボタンのYes/No確認、`(embed)`表示）の目視確認はユーザー側で実施。フェーズ6の不具合2はその過程で報告・修正した。
- フェーズ8・9(`.rkw`/`.rule`埋め込み)は実装当初、他フォークルールの「変更はNKMM_FIX_*フラグでガードする」規約への準拠が漏れていた(ユーザー指摘で発覚)。事後に`NKMM_FIX_REGEX_OUTLINE_EMBED`を新設し、`CType_Cpp.cpp`/`CType_Perl.cpp`(新規追加分、無効時は何も追加しない)・`CType_Ruby.cpp`(無効時は元の手書き33行の`RegexAdd()`に戻す)・`CDocOutline.cpp`(無効時は`ReadRuleFile()`が元通りファイル読み込み失敗時に即0を返す)をガードした。
- `CDocOutline.cpp`側は最初、`bUseEmbedded`という実行時boolに頼って`while`条件や行取得を`bUseEmbedded ? ... : ...`という三項演算子だけで済ませていた(フラグ無効時は`bUseEmbedded`が常にfalseになるので機能的には元と同じだが、`CType_Ruby.cpp`のような「無効時は元のコードにテキストとして戻る」形にはなっていなかった)。これも指摘を受けて、変数宣言・`while`条件・行取得・`file.Close()`の4箇所すべてを`#ifdef`/`#else`で分岐させ、無効時は元の`while( file.Good() && nCount < nMaxCount )`/`file.ReadLineW()`/`file.Close()`にテキストとして戻るよう修正した。

---

## 追記: JS/Text/Xmlへの正規表現キーワード組み込み化、およびTextタイプ未発火バグの修正 20260811

対象フラグ: `NKMM_FIX_REGEX_OUTLINE_EMBED`(既存、対象タイプを追加)/
`NKMM_FIX_TEXTTYPE_URL_REGEX`(新規)。

対象ファイル(主なもの):
- `sakura_core/types/CType_JavaScript.cpp`・`CType_Text.cpp`・`CType_Xml.cpp`
- `sakura_keyword/JavaScript.rkw`・`Text.rkw`・`Xml.rkw`(新規)
- `tools/GenerateKeywordInc.ps1`(`$Targets`に`JS`/`TEXT`/`XML`の`.rkw`エントリを追加)
- `sakura_core/my_config.h`(フラグ定義)

### 内容

フェーズ8・9で対応した`.rkw`(正規表現キーワード)埋め込みの対象を、
それまでのC/C++・Perl・Rubyに加えてJavaScript・Text・Xmlの3タイプへ拡張した。
各タイプの`InitTypeConfigImp()`内に手書きされていた`RegexAdd()`呼び出し列を
対応する`sakura_keyword/*.rkw`へ抽出し、`generated/js_regex.inc`・
`text_regex.inc`・`xml_regex.inc`として組み込み、フラグ無効時は元の
手書き`RegexAdd()`列にそのまま戻る構成にした(既存タイプと同じ方式)。

### 副次的に見つかった不具合: Textタイプの正規表現キーワードが一度も有効化されていなかった

`CType_Text.cpp`は`C:\～`・`\\～`をクリッカブルにする`RegexAdd()`を2件
登録していたが、他の正規表現キーワード使用タイプ(Xml/Cpp/Ruby/Perl/
JavaScript)と異なり`pType->m_bUseRegexKeyword`を一度も`true`にしていな
かった。既定値は`CType.cpp`側で`false`のため、この2件は登録されるだけで
実際には一度も有効化されていなかった(パスのクリッカブル化が機能していな
かった)。

`NKMM_FIX_TEXTTYPE_URL_REGEX`を新設し、`CType_Text::InitTypeConfigImp()`
冒頭で`pType->m_bUseRegexKeyword = true;`を設定するよう修正した。
- `NKMM_FIX_KEYWORDSET_UI`と同じ手順(フラグを一時的にコメントアウトしてフルビルド→エラー無し→復元)で、上記2回とも無効化ビルドを確認済み。
