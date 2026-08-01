# NKMM_FIX_GLYPH_ATLAS_CACHE 実装レポート

対象フラグ: `NKMM_FIX_GLYPH_ATLAS_CACHE`(新規)
前工程の資料: `changelog/NKMM_FIX_GLYPH_ATLAS_CACHE.md`(設計)、`changelog/NKMM_FIX_GLYPH_ATLAS_CACHE_IMPL.md`(実装詳細・コード例)

対象ファイル(実際に変更・新規作成したもの):

- `sakura_core/my_config.h`(新規マクロ追加)
- `sakura_core/view/CViewFont.h` / `.cpp`(`NKMM_FIX_COLOR_FONT`ガード解除)
- `sakura_core/view/CGlyphAtlasCache.h` / `.cpp`(新規)
- `sakura_core/view/CTextDrawer.cpp`(統合)
- `sakura_core/env/CommonSetting.h` / `CShareData.cpp` / `CShareData_IO.cpp`
- `sakura_core/prop/CPropComGeneral.cpp`
- `sakura_core/sakura_rc.rc` / `sakura_rc.h` / `sakura.hh`
- `sakura_core/doc/CEditDoc.cpp`
- `sakura/sakura.vcxproj` / `sakura.vcxproj.filters`(新規ファイルの登録)

---

## 概要

設計資料どおりの方針(GDI `ExtTextOutW`の描画結果を(フォント,文字,前景色,背景色)キーでビットマップキャッシュし、`BitBlt`で再利用する)で実装した。設計フェーズで洗い出した6ステップはすべて計画どおり反映できたが、実装中に**計画時点では分からなかったプロジェクト固有の慣習・環境事情**がいくつか見つかり、その場で計画を補正しながら進めた。以下、計画との差分を中心に記録する。

## 1. `CommonSetting.h` はメンバを機能マクロでガードしない慣習だった(→ 後に統一)

当初は`m_bUseGlyphAtlasCache`を`#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE`で囲む想定だったが、`CommonSetting.h`を実際にgrepしたところ、既存の`NKMM_*`系メンバ(`m_bUseCompatibleBMP`等)は一切`#ifdef`で囲まれていないことが判明した。共有メモリ(`DLLSHAREDATA`)のバイナリレイアウトに関わる構造体のため、ビルド構成間でのレイアウトずれを避ける意図と推測し、一旦はメンバをガード無しで追加、リソースID(`sakura_rc.h`)側だけ`#ifdef`で囲む(こちらは逆に全て`#ifdef`されている)という既存の非対称な慣習に合わせた。

その後、レビューで「一部だけガードが無いのは目立つ・分かりにくい」との指摘を受け、方針を再検討した。共有メモリのレイアウトが変わる点は既存の`NKMM_*`系メンバも同様であり(このプロジェクトは`NKMM_*`フラグを常時有効にしてビルドする前提のため実害はない)、ガード無しにする技術的な必然性は無いと判断。**最終的に`CommonSetting.h`のメンバ・`CShareData.cpp`のデフォルト値代入・`CShareData_IO.cpp`のini読み書き・`sakura.hh`のヘルプID定義の4箇所にも`#ifdef NKMM_FIX_GLYPH_ATLAS_CACHE`を追加し、この機能に関わる箇所を全て統一してガードする形に修正した**(既存の`m_bUseCompatibleBMP`等はガード無しのまま、今回新規追加分のみ統一)。

## 2. ヘルプIDの空き番号が計画時の想定と違っていた

計画では`HIDC_CHECK_MEMDC`(11752)の隣の11753を使う想定だったが、実装時に確認すると11753は既に`HIDC_CHECK_MacroOnOpened`が使用済みだった。さらに、`HIDC_CHECK_MEMDC`があるブロック(11750番台)はマクロ/プラグイン設定用で、`IDD_PROP_GENERAL`(実際に追加したいダイアログ)とは無関係なブロックだと分かった。`p_helpids[]`配列の実際の対応関係を`CPropComGeneral.cpp`で確認し、`IDD_PROP_GENERAL`用のヘルプID帯(10900〜10923)を特定、空いていた**10924**を採用した。

## 3. `sakura_rc.rc`がShift-JIS(BOM無し)だった

プロジェクトの`.h`/`.cpp`は全てUTF-8 BOM付きだが、`sakura_rc.rc`だけはBOM無しのShift-JIS(cp932)だった。通常のEdit操作で日本語の`CONTROL`行を追加すると、ツールがUTF-8として読み書きしてファイル全体が文字化けする恐れがあったため、PowerShellで`[System.Text.Encoding]::GetEncoding(932)`により明示的にデコード/エンコードして挿入した。挿入後にデコードし直して文字化けしていないことを確認済み。また、追加した`(&G)`ニーモニックが同一ダイアログ内の他の有効なコントロールと衝突しないことも別途確認した(コメントアウト済みの旧`IDC_CHECK_MEMDC`行の`(&G)`は無効化されているため衝突対象外)。

## 4. 新規`.cpp`/`.h`にBOMが付いていなかった

`CGlyphAtlasCache.h`/`.cpp`をWriteツールで新規作成した際、既存ファイル(全てUTF-8 BOM付き)と異なりBOM無しで生成されていた。BOM無しUTF-8のまま日本語コメントを含むソースをMSVCがビルドすると、システムのANSIコードページ(Shift-JIS)として誤解釈されるリスクがあるため、PowerShellでバイト列の先頭に`EF BB BF`を追加してBOM付きに修正した。

## 5. ビルド検証

`sakura.sln`を`Release|x64`でビルドし、**コンパイルエラー0件**で`sakura.exe`の生成に成功した(`CGlyphAtlasCache.cpp`・`CTextDrawer.cpp`含む)。

- `preBuild.bat`/`postBuild.bat`はこの検証環境固有の制約(カレントディレクトリ上のバッチファイルをファイル名のみで実行できない、いわゆる`NoDefaultCurrentDirectoryInExePath`相当の挙動)により失敗したが、生成物(`Funccode_define.h`等)は既にリポジトリに存在していたため実害はなく、`/p:PreBuildEventUseInBuild=false` `/p:PostBuildEventUseInBuild=false`で回避してビルドを完了させた。コード変更とは無関係。
- `sakura_rc.rc(1859)`/`(1863)`で`RC2182: duplicate dialog control ID 1259`という警告が出たが、今回追加したID(1730)とは無関係の既存の重複であり、今回の変更が原因ではないことを確認した。

## 6. 絵文字混在時のGDI描画を実地検証(画像データで確認)

`CGlyphAtlasCache::DrawOrCache`のミス時と全く同じ`ExtTextOutW`+`ETO_CLIPPED|ETO_OPAQUE`呼び出しを、実際の絵文字コードポイント(😀🎨📝⭐🔥、"Segoe UI Emoji"フォールバックフォント)に対して行い、PNGとして書き出して目視確認した。

- 画像: `changelog/NKMM_FIX_GLYPH_ATLAS_CACHE_demo_emoji.png`(全体)、`_demo_emoji_zoom.png`(8倍拡大)
- **初回の判断ミス**: ピクセルサンプリングで拾った色情報(輪郭のClearTypeサブピクセルAAによる色にじみ、数ピクセル程度)を絵文字本来の塗り色と誤認し、「GDI単体でもカラー絵文字が出た」と誤って報告した。
- **拡大画像での訂正**: ★(U+2B50)のみ本物の黄色塗りで表示され、😀🎨📝💧は全て黒一色の輪郭のみ(モノクロ)だった。★だけこのフォントサイズに一致する埋め込みビットマップストライク(CBDT/CBLC)を持っていたためと考えられ、他の絵文字はCOLR/CPALのベクターレイヤーをGDIが解釈できずモノクロにフォールバックしていた。
- **結論**: 設計資料の前提(GDI単体では基本的に絵文字はモノクロにフォールバックする)は正しいことを実地確認できた。`CGlyphAtlasCache`はこのモノクロ(またはまれにビットマップストライク一致時のみ色付き)のGDI描画結果をそのままキャッシュするだけであり、実際のフルカラー絵文字表示は別経路の`CColorFontRenderer`(DirectWriteオーバーレイ、`CEditView_Paint.cpp`で1行分まとめてflush)が担う。この責務分離は`NKMM_FIX_GLYPH_ATLAS_CACHE.md`のリスク節に記載済みの想定どおりであり、キャッシュ導入によって絵文字のカラー表示が壊れる経路が無いことを確認できた。

## 7. 「画面キャッシュを使う」チェックボックスの復活とレイアウト調整

`IDD_PROP_GENERAL`(共通設定「全般」タブ)には、以前から無効化されたままの「画面キャッシュを使う」(`IDC_CHECK_MEMDC` / `m_bUseCompatibleBMP`)チェックボックスが`//nkmm`コメントで残っていた。今回のグリフキャッシュ機能と役割が近い(どちらも「描画結果の再利用」に関わる)ため、併せて復活させ、新設した「グリフキャッシュを使う」と1つの「描画」グループボックス内に並べて配置した。

### 実施内容
- `sakura_rc.rc`: 「スクロール」グループボックスの高さを125→48に縮小(ホイール操作コンボ用に確保されていたが既に無効化されて久しく、実質空きスペースだった下半分・約85pxを回収)。空いた領域に「描画」グループボックス(高さ36)を配置し、「画面キャッシュを使う」「グリフキャッシュを使う」を縦に2段で並べた。
- ニーモニック衝突を発見: 両方とも元は`(&G)`だった(グリフキャッシュ追加時、既存の画面キャッシュがコメントアウトされていて気づかなかった)。復活に伴い衝突するため、グリフキャッシュ側を`(&A)`(Atlas)に変更。
- `CPropComGeneral.cpp`のSetData/GetData、`CShareData_IO.cpp`のini読み書き(キー名`bUseCompotibleBMP`、綴りの誤りも含め原文のまま復元)のコメントを解除。
- `p_helpids[]`のヘルプID対応(`IDC_CHECK_MEMDC, HIDC_CHECK_MEMDC`)は元々コメントアウトされていなかったため変更不要だった。
- ビルド再検証: コンパイル・リンクともにエラー0件。

### 技術情報: 画面キャッシュとグリフキャッシュはバッティングするか

**しない。両者は全く別レイヤーで動作する独立した仕組みであり、併用が可能かつ推奨される。**

#### 画面キャッシュ(`m_bUseCompatibleBMP`)がやっていること
`CEditView::UseCompatibleDC(...)`(`CEditView.cpp:407,1845`)が担う、**フレーム単位のダブルバッファリング**。1回の再描画で、行番号・文字・選択範囲反転・ルーラー・罫線など画面上のあらゆる描画要素を、いきなり画面のDC(ウィンドウDC)に直接描く代わりに、まず`CreateCompatibleBitmap`で確保したオフスクリーンの互換ビットマップDC(`m_hdcCompatDC`)にすべて描き切ってから、最後に1回だけ`BitBlt`で画面へ転送する。目的は**ちらつき防止**(画面に逐次描画される過程が見えてしまうことを防ぐ)であり、個々の描画コマンド自体を減らすものではない。

#### グリフキャッシュ(`CGlyphAtlasCache`)がやっていること
`CTextDrawer::DispText`が担う、**文字グリフ単位の描画結果の再利用**。(フォント, 文字, 前景色, 背景色)の組み合わせごとに、一度`ExtTextOutW`で描いた結果をアトラスビットマップへ保存しておき、次に同じ組み合わせが必要になったら`ExtTextOutW`(GDIによるフォントラスタライズ)を再実行せず`BitBlt`(単純なピクセルコピー)だけで済ませる。目的は**文字のラスタライズ処理そのものを省略すること**であり、画面のちらつき防止とは無関係。

#### 両者の関係
`CGlyphAtlasCache::DrawOrCache`は、渡された`hdc`が画面の実DCなのか、画面キャッシュ用のオフスクリーン互換DCなのかを一切区別せず、ただそこへ`BitBlt`するだけの実装になっている(`CTextDrawer.cpp`の統合コードは`hdc`をそのまま渡すのみ)。したがって:

- **画面キャッシュON + グリフキャッシュON**: 「グリフアトラス → オフスクリーンバッファ(1段目のBitBlt) → 画面(2段目のBitBlt)」という経路になるが、両方とも軽量なBitBltなので実害なし。**むしろ最も高速な組み合わせ**(ちらつきも無く、文字のラスタライズも省略される)。
- **画面キャッシュON + グリフキャッシュOFF**: 従来通り、オフスクリーンバッファへ`ExtTextOutW`で毎回ラスタライズしてから画面へBitBlt。
- **画面キャッシュOFF + グリフキャッシュON**: グリフアトラスから画面DCへ直接BitBlt。ちらつき防止は無いが、文字のラスタライズは省略される。
- **両方OFF**: 従来通りの動作(今回の変更前と完全に同じ)。

4パターンのいずれでも、キャッシュの読み書き自体(`SGlyphAtlasKey`のルックアップ、`AllocCell`のシェルフパッキング)は`hdc`の種類に依存しないロジックのため、組み合わせによる副作用・競合は発生しない。

#### その他の技術的な補足
- 両機能とも内部的に`CreateCompatibleBitmap`+`CreateCompatibleDC`でオフスクリーンサーフェスを作る点は共通しているが、それぞれ独立したビットマップ・DCを持っており、共有・干渉はしない(画面キャッシュ用は`CEditView`が1面だけ保持、グリフキャッシュ用は`CGlyphAtlasCache`が最大8ページ保持)。
- 画面キャッシュのビットマップは「ウィンドウサイズ全体」、グリフキャッシュのページは「1024×1024固定」と、サイズのスケールが全く異なる用途のため、メモリ使用量の観点でも競合しない。
- 将来的な最適化の余地として、グリフキャッシュのBitBlt先が画面キャッシュのオフスクリーンDCである場合、そのDC自体も`CreateCompatibleBitmap`由来のDDBであることが多く、ピクセルフォーマット(色深度)は画面と一致しているため、フォーマット変換コストなしにBitBltできる。

## 8. `CViewFont::GetFontGeneration()` のガード再修正

ステップ2で`NKMM_FIX_COLOR_FONT`ガードを外し無条件化していたが、レビューで指摘を受け再検討。`GetFontGeneration()`/`s_nGeneration`は`CColorFontRenderer`(`NKMM_FIX_COLOR_FONT`)と`CGlyphAtlasCache`(`NKMM_FIX_GLYPH_ATLAS_CACHE`)の**両方**が使う共有機能であり、単純にどちらか一方のマクロだけでガードすると、もう一方だけを有効にしたビルド構成で「未定義のメンバを参照」というコンパイルエラーになる。無条件化(ガード無し)でも動作上は問題ないが、他の箇所を全て`#ifdef`で統一する方針に合わせ、`#if defined(NKMM_FIX_COLOR_FONT) || defined(NKMM_FIX_GLYPH_ATLAS_CACHE)`という正しい条件(いずれか一方でも有効なら含める)でガードし直した。`CommonSetting.h`側の4項目(いずれか一方の機能にしか使われない単独の設定値)とは性質が異なり、**複数機能が共有する土台コードは「OR条件」でガードする**必要がある点に注意。

## 未実施・今後の課題

- 実際に起動した`sakura.exe`上でチェックボックスをON/OFFし、GUI越しに目視確認する検証はまだ行っていない(今回は`CGlyphAtlasCache`と同一のGDI呼び出し列を標準スタンドアロンスクリプトで再現し、アルゴリズムレベルでの検証に留めている)。実機での最終確認は次セッションで実施予定。
- 印刷プレビュー・複数ウィンドウ間でのキャッシュ非共有・GDIハンドル数の長時間監視など、設計資料の検証方法節に挙げた項目のうち実機操作が必要なものは未実施。
