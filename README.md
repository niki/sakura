# sakura editor
<p>
  <!-- 1行目：環境・仕様・ライセンス -->
  <a href="https://github.com/niki/sakura/releases">
    <img src="https://img.shields.io/github/v/release/niki/sakura?color=blue" alt="release">
  </a>
  <img src="https://img.shields.io/badge/platform-Windows-0078D6" alt="platform">
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white" alt="C++">
  <a href="https://github.com/niki/sakura/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-Zlib-blue" alt="License: Zlib">
  </a>
  <br>
  <!-- 2行目：独自機能・アクティビティ -->
  <img src="https://img.shields.io/badge/DirectWrite-Supported-0078D6?logo=windows&logoColor=white" alt="DirectWrite">
  <img src="https://img.shields.io/badge/Color_Font-Supported-ff69b4?logo=artstation&logoColor=white" alt="Color Font">
  <img src="https://img.shields.io/github/last-commit/niki/sakura" alt="last commit">
  <a href="https://github.com/niki/sakura/stargazers">
    <img src="https://img.shields.io/github/stars/niki/sakura?style=social" alt="Stars">
  </a>
  <br>
  <a href="https://note.com/katakotori/n/n7463242cc326">
    <img src="https://img.shields.io/badge/note-記事を読む(Read article)-2cb696?logo=note&logoColor=white" alt="note">
  </a>
</p>

<hr>

機能追加やバグの修正などを行っています。<br>
別のエディタも使用しているため、目新しい機能はつっこんでいければいいなと。<br>

<!-- ![](https://drive.google.com/file/d/1elIKbkufCMeIcR_eKpRdFadV11puRh0h/view?usp=drive_link) -->
![](https://lh3.googleusercontent.com/d/1elIKbkufCMeIcR_eKpRdFadV11puRh0h)
<!-- ![](https://drive.google.com/file/d/1nWvonr39GWAdrt5uzmFgDlanZPwD77uY/view?usp=drive_link) -->
![](https://lh3.googleusercontent.com/d/1nWvonr39GWAdrt5uzmFgDlanZPwD77uY)

<b>[Added features]</b><br>

<b>■ 描画・見た目</b><br>
・絵文字などのカラー文字をそのままカラーで表示（通常は単色になってしまう問題を解消）。肌色違いや職業アイコンなど複数の文字が合体する絵文字も1つの絵として表示し、絵文字の表示に使うフォントを共通設定「全般」から固定指定することも可能<br>
・文字の描画をキャッシュ（既定でON、重ければ共通設定「全般」でOFFも可能）<br>
・絵文字が混ざる行でもルーラーの桁位置がズレないよう補正<br>
・半角スペースを「・」で表示、改行されない特殊なスペース(NBSP)も同様に見やすく表示<br>
・タブ文字の矢印表示をやめてシンプルな線だけに<br>
・行間を下側に揃えて詰まった印象を軽減<br>
・カーソルがある行の行番号も背景色で強調し、現在位置を分かりやすく<br>
・変更した行・ブックマークした行を縦線で表示（行番号を非表示にしていても分かる）<br>
・検索結果・ブックマーク・カーソル位置をスクロールバー上にマーク表示し、ファイル全体から一目で探せる。マーク部分はクリックで該当行へジャンプ可能<br>
・スクロールバーをシステム標準の細いデザインに変更<br>
・選択中のタブの下にアクセントカラーの線を表示して見分けやすく<br>
・タブを切り替えたときのちらつきを軽減<br>
・フォントの描画品質（アンチエイリアスの種類）を共通設定「全般」の「描画」から選べるように<br>
・「メニューにアイコンを表示」をONにしても、Windows標準の見た目のまま表示されるように改善（以前は古いXP風の見た目に切り替わっていました）<br>
・ステータスバー・ツールバーをVS Code風のフラットなデザインに一新<br>

<b>■ 操作性</b><br>
・マルチカーソル編集に対応（Ctrl+Alt+↑/↓でカーソルを追加）。VS Code / Sublime Textのように複数箇所へ同時にタイピング・選択・Undo/Redoが可能<br>
・コマンドパレット（Shift+Ctrl+P）を追加。コマンド実行・開いているファイルの切り替え・最近使ったファイル・アウトライン（@）・ブックマーク（#）を1つの絞り込みリストからまとめて検索・実行でき、ローマ字入力のまま日本語のコマンド名もあいまい検索可能<br>
・範囲選択を削除した直後のUndoで、削除前の選択状態も一緒に復元（VS Code等と同じ挙動）<br>
・選択した行（複数行可）をAlt+↑/↓でそのまま上下に入れ替え<br>
・Shift+マウスホイールで水平スクロールできるように<br>
・同じファイルを別ウィンドウで複製して開ける（タブの右クリックメニューから）<br>
・タブをクリックしただけなのにドラッグと誤認識され、並びが入れ替わってしまう不具合を修正<br>
・検索ダイアログをタイトルバーのないフローティングパネル風に一新し、Visual Studioのようなインクリメンタル検索に対応<br>
・水平スクロールバーを、端まで届いていないときだけ自動表示するように変更（画面より長い行がある場合のみ表示）<br>

<b>■ マクロ・検索</b><br>
・WSHが使えない環境でも動くマクロ言語QuickJS(拡張子.qjs)に対応。従来のWSHマクロもそのまま使用可<br>
・正規表現用の外部DLL(bregonig.dll)が無い環境でも、代替エンジン(PCRE2)で正規表現の検索・置換・強調表示が動くように<br>
・PCRE2使用時に改行を含む置換文字列が正しく置換されない不具合を修正<br>
・置換ダイアログで単発置換を連続で実行するとダイアログが閉じてしまう不具合を修正<br>
・置換ダイアログに「プレビュー」機能を追加。実際に「すべて置換」する前に、マッチ箇所と置換後の内容を一覧で確認できる<br>
・プログラミング言語ごとに数値の色分け精度を向上（C/C++, Java, C#, JavaScript, PHP, Python, Ruby, Perl, Visual Basic, Pascal, CSS, アセンブラ）<br>

<b>■ パフォーマンス・安定性</b><br>
・終了時に開いていたファイル一覧（未保存の編集内容を含む）を記憶し、次回起動時に自動復元<br>
・クラッシュ時にミニダンプと未保存の編集内容を保存し、次回起動時に復元するか確認できるように<br>
・メモリ確保の仕組みを見直し(mimalloc採用)、全体的な動作を高速化<br>
・元に戻す(Undo)の履歴が際限なく増えてメモリを圧迫しないよう、上限を設定可能に<br>
・大きな内容を扱った行のメモリをファイル保存時に自動的に解放<br>
・文字数カウントを高速化し、巨大なファイルでもカーソル移動が重くならないように<br>
・大規模ファイルのアウトライン（ファンクション一覧）解析を高速化し、再解析時のフリーズや、ダイアログを閉じた後のビジー状態を解消<br>

<b>■ 設定・カスタマイズ</b><br>
・強調キーワードをCSVファイルで一元管理でき、共通設定側でも読込中の内容をプレビュー表示<br>
・強調キーワードの組み込み定義（キーワードファイル）に正規表現でのパターン指定を追加<br>
・タイプ別設定の配色を統一し、見た目に一貫性を持たせつつ設定もシンプルに<br>
・キー割り当て／ツールバー／カスタムメニューをそれぞれ初期状態に戻すボタンを追加<br>
・キー割り当てを、VS Code・Sublime Text・Notepad++・Visual Studio・ReSharper風などのプリセットからワンクリックで一括変更可能<br>
・キー割り当てタブの隣に、機能名とショートカットを一覧できる読み取り専用の「ショートカット一覧」タブを追加<br>
・キー割り当てを書き出す際、各キーに割り当てた機能名も一緒に出力<br>
・キー割り当ての機能一覧をダブルクリックすると、対応するキー入力欄に自動で同期<br>
・キー入力欄でマウスの右クリックやサイドボタン、ホイール操作もショートカットとして割り当て可能に<br>
・バージョン情報画面から使用ライブラリのライセンスを確認できるように<br>

<hr>

