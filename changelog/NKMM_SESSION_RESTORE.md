# セッション（終了時に開いていたファイルを次回起動時に自動復元する） 20260814

対象フラグ: `NKMM_SESSION_RESTORE`(新規)。

対象ファイル:
- `sakura_core/my_config.h`
- `sakura_core/env/CommonSetting.h`（`CommonSetting_General::m_bRestoreSession`、既定OFF）
- `sakura_core/env/CShareData.cpp`（既定値の設定）
- `sakura_core/env/CShareData_IO.h` / `.cpp`（`SaveSessionFileList`/`LoadSessionFileList`、`ShareData_IO_Common()`での`bRestoreSession`読み書き）
- `sakura_core/env/DLLSHAREDATA.h`（一時共有フラグ`SShare_Flags::m_bSessionHandledByCloseAll`）
- `sakura_core/_main/CCommandLine.h`（`SetFilesForSessionRestore()`）
- `sakura_core/_main/CNormalProcess.cpp`（復元フック）
- `sakura_core/_main/CControlTray.cpp`（保存フック＝`CloseAllEditor()`、および`OnDestroy()`でのバックアップ/書き戻し）
- `sakura_core/window/CEditWnd.cpp`（個別クローズ時のセッションクリア、WM_DESTROY）
- `sakura_core/prop/CPropComGeneral.cpp`、`sakura_rc.rc`/`.h`、`sakura.hh`（共通設定「全般」タブのチェックボックス）

---

## 背景

サクラエディタには「終了時に開いていたファイル群を次回起動時にまとめて復元する」機能が無かった。既存の「起動時に存在しない履歴を確認して削除する」はMRU履歴の掃除であり別物、カーソル位置復元も個々のファイルの最終位置を覚えるだけ。

v1スコープはユーザーとの相談で決定：タブグループ構成・ウィンドウ位置は対象外、ファイルパス一覧のみ。事前調査で、グループ情報を持たないフラットなファイルパス一覧を`sakura.exe f1 f2 f3`として素朴に再起動時に渡すだけで、既存の複数ファイル起動ロジック（1本目のウィンドウに残りが自動的にタブ合流する）がそのまま働くことを確認済みなので、この方式を採用した。

## アーキテクチャ上の制約

サクラエディタの「タブ」は1プロセス内の複数ドキュメントではなく、**タブ1枚＝別プロセスの`sakura.exe`**が共有メモリ(`DLLSHAREDATA`/`CAppNodeManager`)経由でグループ化(`EditNode.m_nGroup`、IDは1始まり)されて見た目上まとまっているだけ。このため「今開いている全ファイル」の取得は`CAppNodeManager::GetOpenedWindowArr()`（プロセス横断）を使う必要があり、「本当にアプリが終了するタイミング」も1プロセスのWM_DESTROYだけでは判断できない：複数ウィンドウ（＝複数プロセス）が連鎖的に閉じる場合、自分が最後の1枚になった時点では既に他のウィンドウが配列から消えている。

## 実装

### 保存フック

最終的に `CControlTray::CloseAllEditor()` に一本化した（詳細下記の「per-window WM_DESTROYを断念した理由」参照）。ここで`GetOpenedWindowArr()`により閉じ始める前の完全な一覧を取得できる。

保存条件（「実質的に全終了か」の判定）:
- `nGroup==0`（「ファイル→サクラエディタの終了」「編集の全終了」。常に全グループ対象）
- または `nGroup!=0`（特定グループだけを閉じる操作。タブがまとめ表示されている場合、タイトルバーの×はこちらを通ることを実機確認済み）だが、現在開いている全ウィンドウがそのグループに属する（＝他に残るグループが無い＝結果的に全終了と同じ）

上記いずれでもない場合（他のグループが残る）は何もしない。

追加ルール（ユーザー要望）: `n>=2`（一括で閉じようとしているウィンドウが2枚以上）のときだけ実ファイル一覧を保存する。`n==1`の終了は通常の単一ファイル起動と同じ意味なので、**空リストで明示的に上書き**し、以前の（複数ウィンドウ終了時の）古いセッションが残り続けないようにする。

### per-window WM_DESTROYを断念した理由

当初`CEditWnd`のWM_DESTROYで`m_nEditArrNum==1`を見て保存する設計だったが、`RequestCloseEditor()`が同期的`SendMessage(MYWM_CLOSE)`で1枚ずつ順に閉じるため、複数ウィンドウが連鎖的に閉じる場合、最後の1枚が自分のWM_DESTROYで数を確認する時点で**既に他のきょうだいウィンドウは配列から消えている**（＝自分1枚分の情報しか取れない）。単発の1ウィンドウ直接クローズでは問題ないが、一括終了では機能しない。

### 個別クローズ時のセッションクリア（`CEditWnd`のWM_DESTROY、現存）

`CloseAllEditor()`を経由しない個別クローズ（「ファイル→閉じる」、タブグループ化していない状態でのタブ×等）で自分が最後の1枚になる場合、それは「一括終了」ではないので保存対象外。ただし、以前の一括終了で保存された古いセッションが残り続けると紛らわしいため、その場でセッションを明示的にクリアする。

`CloseAllEditor()`とこの個別クローズクリアが二重に走って上書き合戦にならないよう、一時共有フラグ`DLLSHAREDATA::m_sFlags::m_bSessionHandledByCloseAll`（iniには保存しない）を使う。`CloseAllEditor()`が保存/クリアを行った直後にTRUEを立て、`RequestCloseEditor()`（同期的に全ウィンドウを閉じ切る）が戻ったらFALSEに戻す。WM_DESTROY側はこのフラグが立っていれば何もしない。

### 復元フック（`CNormalProcess::InitializeProcess()`）

以下すべてを満たす場合のみ、保存されたファイル一覧を`CCommandLine`の内部ファイルリスト（先頭を`m_fi`、残りを`m_vFiles`）に注入し、既存の複数ファイル起動経路をそのまま再利用する:
- `bRestoreSession`がON
- コマンドラインでファイル指定が一切無い（`GetFileNum()==0`かつ`m_fi.m_szPath`も空。1ファイルだけの起動は`m_fi`に入り`m_vFiles`は0件になるため、`GetFileNum()`だけでは判定不十分）
- 他にsakuraウィンドウが1つも起動していない（`m_nEditArrNum==0`）。これにより「新規ウィンドウを開く」操作や二重起動時に誤って復元が発動しないようにしている（実機確認済み）

### 永続化と`SaveShareData()`の落とし穴

`CShareData_IO::SaveSessionFileList`/`LoadSessionFileList`は、`DLLSHAREDATA`や通常の`ShareData_IO_2`（設定の読み書き本体）を経由せず、`sakura.ini`の`[Session]`セクション（`_Session_Counts`、`Session[NN].szPath`）へ独自に読み書きする。

大きな落とし穴: `CShareData_IO::SaveShareData()`（→`ShareData_IO_2(false)`）は**iniファイル全体をゼロから組み立てて書き直す**（`ReadProfile()`せず、既知のキーだけを書いて`WriteProfile()`で丸ごと上書き）。これは通常の設定保存では正しいが、`[Session]`のように独自に書き込むセクションは、既知のキーではないため**跡形もなく消される**。

`SaveShareData()`が呼ばれるのは`CControlTray::OnDestroy()`（トレイウィンドウのWM_DESTROY、アプリ終了時のただ一箇所）。`CloseAllEditor()`で`[Session]`を書いた直後にこれが走り、書いたそばから消える不具合が実機で発生した。対策として`OnDestroy()`内で`SaveShareData()`の直前に`LoadSessionFileList()`で退避し、直後に`SaveSessionFileList()`で書き戻す。

このとき**空リストの扱いに注意**: `LoadSessionFileList()`は0件のとき`false`を返すため、戻り値で「退避が必要か」を判定すると、`n==1`終了時の「明示的な空クリア」が「退避不要」と誤判定され、書き戻されずセクションごと消えてしまう不具合が実機で発生した。`bRestoreSession`がONなら常に（空でも）無条件で書き戻すよう修正済み。

## OS シャットダウン対応（WM_QUERYENDSESSION/WM_ENDSESSION）20260815

各ウィンドウ（＝別プロセス）が独立してWM_QUERYENDSESSIONを受け取るため、`CloseAllEditor()`のような単一の集約点が無い。`CEditWnd::DispatchEvent()`のWM_QUERYENDSESSIONハンドラで、以下の方式により対応した：

- 最初にWM_QUERYENDSESSIONを受け取ったウィンドウが、まだ誰も閉じ始めていない時点での全ウィンドウ一覧（`GetOpenedWindowArr(&p, TRUE)`、タブ表示順）からセッションを保存する。この保存処理は`CloseAllEditor()`と共通化し、`CControlTray::SaveSessionSnapshot(pWndArr, n)`として切り出した。
- 「自分が最初か」の判定は、単純なif/代入ではなく`InterlockedCompareExchange()`で`m_bSessionHandledByCloseAll`をFALSE→TRUEへアトミックに遷移させることで行う。WM_QUERYENDSESSIONが複数プロセスへ本当に1つずつ順番に配送される保証はOS実装に依存しコード側で保証できないため、万一2つ以上のプロセスがほぼ同時にこの分岐に入っても、実際にスナップショットを取るのは1プロセスだけになるようにしている。
- 拒否時（このウィンドウがシャットダウンを拒否した場合）は`m_bSessionHandledByCloseAll`をFALSEへ戻す。`WM_ENDSESSION`（`wParam==FALSE`＝シャットダウンがキャンセルされた）でも念のため同様に戻す（二重の安全策）。
- 他のウィンドウが拒否してシャットダウン自体がキャンセルされた場合、既に閉じてしまったウィンドウは戻らない（サクラエディタ既存の挙動、対応範囲外）。

**NKMM_SESSION_RESTORE_BUFFER統合時に発見・対処したレース条件**: 詳細は[NKMM_SESSION_RESTORE_BUFFER.md](NKMM_SESSION_RESTORE_BUFFER.md)の「OSシャットダウン対応との統合」を参照。`m_bSessionHandledByCloseAll`だけを見て「保存確認を抑制してよいか」を判断すると、他ウィンドウのダンプ完了前に自分のクローズ処理が先行するレースでデータを失いかねなかったため、ウィンドウごとのローカルな実績フラグ(`CEditDoc::m_bSessionBufferCaptured`)を追加で確認するよう修正した。

## 既知の制約

- 強制終了（タスクキル等、WM_DESTROYを経由しない終了）では保存されない。

## 動作確認について

実機で以下を確認済み:
- 「ファイル→サクラエディタの終了」「編集の全終了」（全グループ、2枚以上）: 保存・復元とも正常
- 特定グループだけを閉じる操作（タイトルバー×等）でも、それが実質的に全終了なら保存・復元とも正常
- グループA/グループBのように他のグループが残る状態でグループ単体を閉じても保存されない（正しい）
- 1枚だけの終了はセッションが空クリアされ、次回起動で復元されない
- 個別クローズ（グループ化を経由しない）は保存もクリアも発生しないが、以前の一括終了時の古いセッションは正しくクリアされる
- 無題（新規）バッファはセッション対象外（フィルタ済み）
- 復元済みの状態でさらに`sakura.exe`を引数なしで二重起動しても、復元が再発動せず空の「(新規)」が開くのみ

デバッグ時の注意（実装外の話）: 実機検証中、サンドボックス環境でクラッシュダイアログを`Stop-Process -Force`で強制終了する操作を繰り返した結果、環境自体が劣化し、変更と無関係な既定プロファイル単体起動でも「Debug Assertion Failed: Buffer is too small」が再現するようになった。`git stash`で変更前の元コードに戻しても同じ手順で再現することを確認し、コード起因ではなくサンドボックスのリソース枯渇と判断した。
