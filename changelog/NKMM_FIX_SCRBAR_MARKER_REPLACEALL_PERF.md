# ReplaceAllが時々極端に遅くなる不具合の調査と修正 20260728

対象フラグ: `NKMM_FIX_EDITVIEW_SCRBAR`(既存)配下の実装バグ修正。新規フラグは追加していない。

対象ファイル:
- `sakura_core/view/CEditView.cpp`(`CEditView::ScrBarMarker::Build()`)
- `sakura_core/cmd/CViewCommander_Search.cpp`(`Command_REPLACE_ALL()`)

---

## 経緯

正規表現JIT(sljit)の効果を計測するベンチマークマクロ(`macro_bench/BenchmarkRegex.qjs`)を
実機で走らせたところ、同じパターン・同じドキュメントに対する`ReplaceAll`が、ある回は
1秒未満で終わるのに、別の回は数十秒〜300秒以上かかるという極端な不安定さが見つかった。
JITの効果(数%程度)どころではない桁違いのノイズで、当初の目的(JIT有無の比較)が
達成できなかった。

正規表現の有無に関わらず(単純な文字列置換でも)同様の現象が起きていたため、
`CBregexp`やPCRE2そのものではなく、置換の実処理(`CViewCommander::Command_INSTEXT`)
または検索の実処理(`Command_SEARCH_NEXT`)側に原因があると推測し、一時的な計測コード
(`OutputDebugStringW`で全呼び出しのENTER/EXITを記録)を仕込んで実測した。

## 根本原因

`Command_INSTEXT`は編集のたびに末尾で無条件に`SB_Marker_Clear(2000)`
(`CViewCommander_Clipboard.cpp`)を呼ぶ。これは`CEditView::ScrBarMarker::Clear()`→
`Build(bCacheClear=true, foo)`(`CEditView.cpp`)を呼び、次の処理を無条件に行う。

```cpp
if (bCacheClear) {
    WaitForBuild(true);
    vLines_.clear();          // スクロールバーマーカーのキャッシュを空にする
}
if (vLines_.empty()) {
    ...
    hBuildThread_ = (HANDLE)_beginthreadex(NULL, 0, &SB_Marker_BuildThread, ...);
    ::Sleep(10);               // ← 無条件に10ms止まる
    ...
```

キャッシュを空にした直後は必ず`vLines_.empty()`が真になるため、**編集1回につき
必ず新しいビルドスレッドを起動し、`::Sleep(10)`で最低10msブロックする**。実測でも
`Command_INSTEXT`の所要時間はほぼ常に12〜29msで、これがそのまま原因だった
(`Command_SEARCH_NEXT`・`CBregexp::Replace`はサブミリ秒で安定しており無関係)。

`ReplaceAll`は1行ごとに`Command_INSTEXT`を呼ぶため、20,000行のドキュメントに対する
1回の`ReplaceAll`だけで20,000回×10ms以上=200秒以上かかる計算になり、実測値
(数十秒〜300秒超、かつ実行のたびにばらつく)とよく一致する。ばらつきの理由は、
スレッド起動のOSレベルのスケジューリング負荷やシステム負荷に依存するため。

## 修正方針

`Command_INSTEXT`側(呼び出し元)は変更していない。`bRedraw=false`で呼ぶ箇所が
ペースト・単発置換・ファイル読み込みなど10箇所以上あり、個別に手を入れるより
既存の仕組みに相乗りする方が安全と判断した。

代わりに、`CEditView::ScrBarMarker::Build()`に「描画抑制中は何もしない」ガードを
追加した。

```cpp
void CEditView::ScrBarMarker::Build(bool bCacheClear, int foo)
{
    if (CEditApp::getInstance()->m_pcGrepAgent->m_bGrepMode) return;
    if (!pEditView_->GetDrawSwitch()) return;  // 追加(第1版、後述の理由で修正)
    ...
```

### 追記: 初版の修正がドキュメント破損を引き起こした

初版(`if (!pEditView_->GetDrawSwitch()) return;`だけ)を実機のベンチマークで
検証したところ、`ReplaceAll`完了後にドキュメントの中身が空になる不具合が
発生した。

原因は、初版が「描画抑制中は無条件に即リターン」としてしまい、**既に実行中の
ビルド/描画スレッドを止める処理(`WaitForBuild`/`WaitForDraw`)まで一緒に
スキップしてしまっていた**ことにある。たとえば`InsText()`で大量のテキストを
挿入した直後は、その内容に対するマーカー構築スレッドがバックグラウンドで
まだ走っている可能性がある。そこへ`ReplaceAll`が`SetDrawSwitch(false)`にして
編集ループへ入ると、初版のコードはその古いスレッドを止めずに毎回即座に
抜けてしまい、**ドキュメントを読み取り中の古いスレッドと、ドキュメントを
書き換え中のメインスレッドが同時に走る**競合状態になっていた。

修正版:

```cpp
if (!pEditView_->GetDrawSwitch()) {
    WaitForDraw(true);
    WaitForBuild(true);
    vLines_.clear();
    return;
}
```

`WaitForBuild`/`WaitForDraw`はスレッドが既に止まっていれば実質何もせず
瞬時に返る(`bBuildThreadRunning_`等のフラグを見るだけ)ため、ループ中に
20,000回呼ばれても最初の1回以外はほぼコスト無し。「性能を上げつつ、実行中の
スレッドとの競合は必ず解消する」という、初版より安全な形にした。

`Command_REPLACE_ALL`は元々ループの前後で`SetDrawSwitch(false)`→
`SetDrawSwitch(bDrawSwitchOld)`という「今は描画しない」宣言を自前で行っている
(`CViewCommander_Search.cpp`)。この既存のフラグに相乗りすることで、ループ中の
`Command_INSTEXT`が呼ぶ`SB_Marker_Clear`は「どうせ描画しないので何もしない」で
即座に抜けるようになる。`SetDrawSwitch(false)`は他にも(Grep置換・行削除系コマンド
など)10箇所ほどで使われており、それらも同じ理由で自動的に速度面の恩恵を受ける。

ただし、ループ中ずっとキャッシュ更新をサボった分、置換完了後にスクロールバー
マーカー(検索結果・ブックマーク・カーソル行の表示)が古いままになる懸念がある
ため、`Command_REPLACE_ALL`の末尾、`SetDrawSwitch(bDrawSwitchOld)`で描画を
再開した直後に`SB_Marker_Clear(1510)`を1回追加し、正しい状態へ作り直すように
した。他の`SetDrawSwitch(false)`使用箇所については、同様の「完了後に1回再構築」
の要否は今回は未確認・未対応。

## 動作確認について

`msbuild /t:sakura:ClCompile /p:SelectedFiles=<file>`によるファイル単位ビルドで、
`CViewCommander_Search.cpp`・`CEditView.cpp`をRelease×Win32/x64で0エラー・0警告
確認済み。Debug構成は`CEditView.cpp`が今回の変更とは無関係な既存の型変換エラー
(`CStrictInteger`関連、変更前のコードでも同じ行で再現することを確認済み)により
ビルドが通らない(このリポジトリの既知の制約)。

実機でのベンチマーク再実行は、初版(競合状態あり)でドキュメント破損を確認した後、
修正版に差し替えた状態。修正版での再検証(`ReplaceAll`の所要時間が安定して速く
なるか、ドキュメントが破損しないか、置換後のスクロールバーマーカー表示が
正しいか)はこれから。

### 追記2: ループ開始前の残存スレッドによるクラッシュ

修正版でも実機検証したところ、`this->m_CurRegexp.m_pRegExp`がnullptrの状態で
`CBregexp::GetIndex()`(`CBregexp.h:119`)を読み取り、読み取りアクセス違反で
クラッシュする不具合が発生した(Visual Studioの例外ダイアログで確認)。

`CEditView::ScrBarMarker::IsFoundLine()`はバックグラウンドのマーカー構築スレッド
(`SB_Marker_BuildThread`)から呼ばれ、`m_bCurSrchKeyMark`が立っていると
`IsSearchString()`経由で`CEditView::m_CurRegexp`(検索用の正規表現オブジェクト、
`CBregexp`)を参照する。一方、`Command_REPLACE_ALL`の正規表現置換ループは
`cRegexp`(同じく`m_CurRegexp`)を毎回`Replace()`で書き換える。

`CEditView::ScrBarMarker::Build()`側の修正(追記1)は「描画抑制中は新規スレッドを
起動しない」だが、これは`Command_INSTEXT`が呼ばれた**後**(=1行編集した後)に
しか効かない。加えて`Command_REPLACE_ALL`は`ChangeCurRegexp()`(`m_CurRegexp`を
書き換える)を`SetDrawSwitch(false)`より**前**に呼んでいるため、「ループ開始直前に
`SB_Marker_Clear`を1回追加する」という最初の対策(`SetDrawSwitch`の直後に配置)
では間に合わず、実機で試したところ今度はアプリがハングする(応答なしになる)
症状が出た。`WaitForBuild(true)`はスレッド側が1行ごとに中断フラグを見ている
ため通常は速やかに返るはずだが、`IsFoundLine`→`IsSearchString`が`m_CurRegexp`の
壊れた状態を参照して1回の呼び出し自体が終わらなくなり、`WaitForSingleObject`が
無限待ちになったものと考えられる。

## 追記3: 最終的な対策(検索ハイライトの一時無効化)

`m_CurRegexp`へのアクセスをスレッド間できちんと同期する本格対応は実装コストと
リスクが大きいため、より単純な対策を採った: **`Command_REPLACE_ALL`の実行中は
検索ハイライト(`m_bCurSrchKeyMark`)を一時的に無効化する**。これにより
`IsFoundLine()`は`m_CurRegexp`に一切触れずに`false`を返すだけになり、
バックグラウンドスレッドと`Command_REPLACE_ALL`の間で`m_CurRegexp`を
同時に触る経路そのものが無くなる。

`ChangeCurRegexp()`より前に無効化する必要がある(前述の通り、そこが最初に
`m_CurRegexp`を書き換える箇所のため)ため、関数のどのreturn経路でも確実に
元へ戻せるよう、RAIIガード`CSuppressSrchKeyMarkForReplaceAll`
(`CViewCommander_Search.cpp`のファイル先頭、無名namespace内)を新設し、
`Command_REPLACE_ALL`の先頭でインスタンス化するだけにした。

```cpp
class CSuppressSrchKeyMarkForReplaceAll {
public:
    explicit CSuppressSrchKeyMarkForReplaceAll(CEditView* pView)
        : m_pView(pView), m_bOld(pView->m_bCurSrchKeyMark)
    {
        m_pView->m_bCurSrchKeyMark = false;
        m_pView->SB_Marker_Clear(810);  // 既存スレッドがあれば止める
    }
    ~CSuppressSrchKeyMarkForReplaceAll()
    {
        m_pView->m_bCurSrchKeyMark = m_bOld;
        m_pView->SB_Marker_Clear(811); // 元の状態で1回だけ正しく再構築
    }
    ...
};

void CViewCommander::Command_REPLACE_ALL()
{
    CSuppressSrchKeyMarkForReplaceAll _srchKeyMarkGuard(m_pCommanderView);
    if( !m_pCommanderView->ChangeCurRegexp() ){
        return;  // ここで抜けてもデストラクタが必ず復元する
    }
    ...
```

これに伴い、`SetDrawSwitch(false)`直後の`SB_Marker_Clear(840)`(追記2で追加した
もの)と、関数末尾の`SB_Marker_Clear(1510)`(初版で追加したもの)は不要になり
削除した。ガードのコンストラクタ/デストラクタがそれぞれの役割を兼ねている。

既知の副作用: 置換の実行中(および完了直後、デストラクタが復元するまでの一瞬)は、
編集画面上の「検索文字列のハイライト」表示も無効化される。ループ中は
`SetDrawSwitch(false)`によりそもそも再描画されないため実害はほぼ無いはずだが、
完了直後にごく短時間ちらつく可能性はゼロではない。

なお、スクロールバーの「描画」スレッド(`hDrawThread_`/`Draw()`)は、
既にビルド済みの`vLines_`キャッシュから描画するだけで`m_CurRegexp`には
触れていないように見えるため、今回の一連の不具合には無関係と判断し、そちらの
経路(`WM_APP_SCRBAR_PAINT`メッセージ経由の再起動)は変更していない。

この3回目の修正についても実機での再検証はまだ。

## 追記4: Undo履歴の際限ない蓄積(エディタ側は未修正、ベンチマーク側で回避)

3回目の修正後、実機でクラッシュ・ハングは無くなったが、別の現象に気づいた:
`ReplaceAll`が20,000行付近まで進んだところで進捗が長時間停滞する(完全に
固まってはおらず、待てば進む)。`SUPPORT_JIT`を無効化しても同じ位置で同じ
ように遅くなったため、JIT自体は無関係と判断できた。

さらに`macro_bench/BenchmarkRegex.qjs`のREPEATを重ねて計測したところ、
「1回のReplaceAll完了後、次のReplaceAllが始まるまでの待ち時間が、回を追う
ごとに伸びていく」ことが分かった。Sakuraには最大Undo段数を制限する設定が
見当たらず(`sakura_core/env/CommonSetting.h`にそれらしき項目なし)、
`COpeBlk`(`sakura_core/COpeBlk.h`)への追記(`AppendOpe`、`std::vector`への
`push_back`)自体はO(1)だが、Undo履歴(`COpe`)がドキュメントが存在する限り
無制限に蓄積し続ける作りになっている。今回のベンチマークは1回の`ReplaceAll`
で数万件、それをREPEAT=20回繰り返すため、最終的に240万件規模のUndo記録が
単一ドキュメントに積み上がることになり、これが回を追うごとの遅さの原因と
考えられる。

これは通常の対話的な編集では起こりえない規模(誰も1つのドキュメントに
Undoなしで240万件も編集を続けない)であり、エディタ本体の不具合というより
「今回のベンチマークの負荷のかけ方が非現実的だった」側の問題と判断し、
エディタ側(Undo履歴の上限管理)は修正していない。

代わりに`macro_bench/BenchmarkRegex.qjs`側で対策した: `benchReplace()`が
計測1回ごとに`FileNew()`で新規タブを作り、そこへ`InsText(text)`でテキストを
流し込んでから`ReplaceAll`を計測するように変更した(v7)。新規タブは
Undo履歴が空の状態から始まるため、REPEATを重ねても蓄積しない。ドキュメント
準備(`FileNew`/`InsText`)はタイマーの外側で行うため計測には影響しない。
副作用として、実行後に`REPEAT×2`枚の検証用タブが残るようになった
(すべて保存せず閉じてよい)。

Undo履歴の上限管理自体(エディタ本体の改善)は、今回は範囲外として見送った。

## 追記5: ChangeCurRegexp()がガードを無効化していた(真の原因)

v7(FileNew()で毎回新規タブ化)を試したところ、Undo履歴が蓄積しないはずなのに
「REPEATを重ねるごとに遅くなる」現象が解消しなかった。開いているウィンドウは
(タブまとめ表示ではなく)ファイルごとに別々の40個の独立ウィンドウであることも
確認した。

原因は追記3のRAIIガード自体にあった。`Command_REPLACE_ALL`は
```cpp
CSuppressSrchKeyMarkForReplaceAll _srchKeyMarkGuard(m_pCommanderView); // ここでfalseに
if( !m_pCommanderView->ChangeCurRegexp() ){ return; }                  // ここでtrueに戻る(!)
```
という順序で呼んでいたが、`CEditView::ChangeCurRegexp()`
(`CEditView_Command.cpp:404`)は内部で無条件に`m_bCurSrchKeyMark = true`を
セットし直す(検索文字列マークの設定)。つまりガードによる無効化は
`ChangeCurRegexp()`の1行で即座に上書きされ、実質何も抑制できていなかった。

結果として、`m_bCurSrchKeyMark`はループの間ずっと`true`のままになり、
関数の最初(`SetDrawSwitch(false)`前)と最後(`SetDrawSwitch(bDrawSwitchOld)`後、
ガードのデストラクタ内)、つまり`GetDrawSwitch()==true`の間に呼ばれる
`SB_Marker_Clear`は、追記1のガードでは止められず、`IsFoundLine`が実際に
`m_CurRegexp`で20,000行全部を正規表現マッチさせるバックグラウンドスレッドを
律儀に起動していた。ウィンドウを閉じずに40個まで増やし続けたため、後半に
なるほど「まだ前のウィンドウの20,000行スキャンが終わっていない」バック
グラウンドスレッドが複数同時に走り、CPUを奪い合って全体が遅くなっていった
ものと考えられる(JITの有無に関わらず同じ場所で遅くなったのも、この
バックグラウンドスレッドの競合がボトルネックだったのなら説明がつく)。

### 修正

`ChangeCurRegexp()`の直後で、改めて`m_bCurSrchKeyMark`をfalseへ戻す。

```cpp
if( !m_pCommanderView->ChangeCurRegexp() ){
    return;
}
m_pCommanderView->m_bCurSrchKeyMark = false;  // 追加: ChangeCurRegexp()が
                                               // trueへ戻すのを再度無効化する
```

関数を抜けるときは(returnの経路によらず)ガードのデストラクタが
`m_bCurSrchKeyMark`を元の値へ復元するので、これで置換ループの間
(`ChangeCurRegexp()`の直後から関数終了まで)は確実に`false`のまま維持される。

## 追記6: 「開いているウィンドウ数」に応じて遅くなる別の現象(未解決・対応見送り)

追記5の修正後、実機で再検証したところ、クラッシュ・ハングは解消し全体的な
速度も改善したが、「REPEATを重ねるごとに1回あたりの所要時間が伸びていく」
傾向自体は残った。

切り分けとして、ベンチマークで毎回作られる検証用タブ/ウィンドウを都度
閉じずに残しているため、実行が進むにつれて開いているウィンドウ数が
増えていくことを確認した(タブまとめ表示ではなく、ファイルごとに独立した
ウィンドウが増える構成であることも確認済み)。ウィンドウを閉じてから
まっさらな状態で再実行しても同じ傾向が出たため、複数回の試行にまたがる
蓄積ではなく、**1回の実行の中でウィンドウが1個→40個に増えていく過程**
そのものが遅さと相関しているとみられる。

具体的にどのコードが「開いているウィンドウ数」に応じて遅くなっているのかは
未特定。候補としては、共有メモリ(`DLLSHAREDATA`)経由のウィンドウ間同期や、
何らかのウィンドウ一覧の走査などが考えられるが、深追いすると本題(JIT比較)
から外れるため、今回は原因特定・修正を見送った。実運用で「保存せず数十枚の
ウィンドウを開いたまま大量の置換を繰り返す」状況はまず起きないため、
実害の小さい既知の制限として扱う。

### ベンチマーク側の対応

`macro_bench/BenchmarkRegex.qjs`をv8に変更し、新規タブを増やさない方式に
した: `FileNew()`は最初の1回だけ呼び、同じタブに対して`REPEAT`回
`ReplaceAll`を繰り返す(`REPEAT`も20→5に減らした)。これにより「ウィンドウ
数増加による遅化」を回避しつつ、JIT有無の比較に必要な計測ができるように
した。Undo履歴はこの範囲(5回×2パターン)であれば蓄積してもJITの比較を
妨げるほどの影響は出ないと判断している。

実機での再検証はまだ。
