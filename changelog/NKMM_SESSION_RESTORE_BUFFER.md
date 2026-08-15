# セッション：バッファ内容の復元 20260814

対象フラグ: `NKMM_SESSION_RESTORE_BUFFER`(新規、`NKMM_SESSION_RESTORE`に依存)。

対象ファイル:
- `sakura_core/my_config.h`
- `sakura_core/env/CommonSetting.h`（`CommonSetting_General::m_bRestoreSessionBuffer`、既定OFF）
- `sakura_core/env/CShareData.cpp`（既定値の設定）
- `sakura_core/env/CShareData_IO.h` / `.cpp`（`SSessionEntry`、`SaveSessionFileList`/`LoadSessionFileList`をパス一覧からエントリ一覧へ拡張、`GetSessionBufferDir`/`GetSessionBufferFilePath`、`ShareData_IO_Common()`での`bRestoreSessionBuffer`読み書き）
- `sakura_core/EditInfo.h`（`m_szBufRestorePath`）
- `sakura_core/_main/CCommandLine.h` / `.cpp`（`-BUFRESTORE=`スイッチ、`m_cmBufRestorePath`/`m_vBufRestorePaths`、`SetFilesForSessionRestore()`の拡張）
- `sakura_core/_main/CControlTray.cpp`（`CloseAllEditor()`でのバッファダンプ、`OpenNewEditor2()`での`-BUFRESTORE=`橋渡し）
- `sakura_core/_main/CNormalProcess.cpp`（復元トリガでのエントリ振り分け、`OpenFiles()`での橋渡し、復元フック）
- `sakura_core/doc/CDocFileOperation.h` / `.cpp`（`RestoreBufferOverlay()`）
- `sakura_core/config/system_constants.h`（`MYWM_DUMPBUFFER`）
- `sakura_core/env/DLLSHAREDATA.h`（ワークバッファ`m_szDumpBufferTargetPath_MYWM_DUMPBUFFER`）
- `sakura_core/window/CEditWnd.cpp`（`MYWM_DUMPBUFFER`ハンドラ）
- `sakura_core/prop/CPropComGeneral.cpp`、`sakura_rc.rc`/`.h`、`sakura.hh`（共通設定「全般」タブの「バッファ内容の復元」チェックボックス、「セッションの復元」と連動して有効/無効化）

---

## 背景

[NKMM_SESSION_RESTORE](NKMM_SESSION_RESTORE.md)は、全終了時に開いていたファイルの**パス一覧**だけを保存し、次回起動時にそのパスを普通に開き直す機能。ディスク上のファイルを再読込するだけなので、**未保存の変更**や**無題（一度も保存していない）バッファ**は復元されない。

本機能は、全終了時にバッファの中身そのものを退避し、次回復元時に（保存していなくても）その中身を復元できるようにする。未保存内容が平文でプロファイルフォルダ配下に一時的に残るという性質があるため、ユーザーとの相談の結果、既存の「セッションの復元」チェックボックスとは**別の明示的なオプトインチェックボックス**として分離した（「セッションの復元」がOFFのときはこちらも意味を持たないので、UI上も連動して無効化する）。

## 対象範囲

- 対象: 全終了時に「変更あり」（`CDocEditor::IsModified()`）だったウィンドウ ―― 保存済みファイルの未保存編集、および無題バッファに文字が入っている場合。
- 対象外: 変更なしのファイル（ディスクの内容と一致するので、既存のパス復元だけで十分）、変更なしの空の無題ウィンドウ（従来通り対象外）。
- タブグループ構成・ウィンドウ位置は`NKMM_SESSION_RESTORE`と同様スコープ外。
- 名前付きファイルは`NKMM_SESSION_RESTORE`既存のルール通り、ウィンドウが1枚だけの終了（`n==1`）は対象外（通常の単一ファイル起動と同じ意味なので）。ただし**無題（パス無し）で変更ありのバッファはn==1でも対象**とする。名前付きファイルと違い、パスによる代替の復元手段が無く、保存しなければ内容そのものが失われるため。

## アーキテクチャ上の制約と対応

`NKMM_SESSION_RESTORE`と同じ「タブ＝別プロセス」制約がある。セッション復元時、先頭ファイルは自プロセス内で開かれるが、残りは`CNormalProcess::OpenFiles()`が`CControlTray::OpenNewEditor2()`経由で**実際に`sakura.exe`を新規プロセスとして`CreateProcess`する**。つまりバッファの中身そのものを直接プロセス間で渡すことはできず、**渡せるのはコマンドライン文字列だけ**。

このため、バッファ内容は一旦ファイル（プロファイルフォルダ配下`SessionBuffers\`内の連番ファイル、UTF-8+BOM）に書き出し、そのファイルパスをプロセス間の橋渡しに使う方式にした。

- 別プロセスへは新設コマンドラインスイッチ `-BUFRESTORE="<パス>"` （`-PROF=`と同じ文字列スイッチパターン）で橋渡しする。`EditInfo::m_szBufRestorePath`を経由し、`CControlTray::OpenNewEditor2()`が既存の`-X=`/`-Y=`と同じ場所でコマンドラインに追加する。
- 先頭ファイル（自プロセス内）は、`CCommandLine::SetFilesForSessionRestore()`がコマンドラインを介さず直接`CCommandLine::m_cmBufRestorePath`にセットする。
- 復元の実処理（`CNormalProcess::InitializeProcess()`の、実ファイル/無題どちらの分岐でも通る合流点）は1箇所のみで、`CCommandLine::GetBufRestorePath()`（コマンドライン経由・直接注入経由のどちらでも同じ場所から読める）を見て`CDocFileOperation::RestoreBufferOverlay()`を呼ぶ。

## 実装

### バッファ→ファイルのダンプ（`CloseAllEditor()` → `MYWM_DUMPBUFFER` → `CEditWnd`）

`CloseAllEditor()`の、既存の`NKMM_SESSION_RESTORE`用ループ（各ウィンドウへ`MYWM_GETFILEINFO`を送ってパスと変更フラグを取得する部分）を流用し、`m_bRestoreSessionBuffer`がONのときだけ：

- 空の無題バッファ（`m_bIsModified==false`）は従来通り対象外。
- `m_bIsModified==true`のウィンドウには、新設`MYWM_DUMPBUFFER`メッセージを同期送信する。対象パスは共有ワークバッファ（`DLLSHAREDATA::m_sWorkBuffer::m_szDumpBufferTargetPath_MYWM_DUMPBUFFER`、`MYWM_GETFILEINFO`の`EditInfo`受け渡しと同じパターン）経由で渡す。
- `CEditWnd`側のハンドラは、`CSaveAgent::OnSave()`が使う`CWriteManager::WriteFile_From_CDocLineMgr()`を直接呼ぶ。`DoSaveFlow()`/`FileSaveAs()`と違い、マクロ・プラグインフックもドキュメント識別情報（現在のファイルパス／変更フラグ）の変更も一切行わない、純粋な「バッファ内容→任意パスへの生ダンプ」。UTF-8+BOM固定（改行コードは無変換）で書き出す。

### 永続化フォーマット（`CShareData_IO::SSessionEntry`）

`[Session]`セクションのエントリを、パス文字列の配列から`SSessionEntry{ path, bModified, bufFile }`の配列に拡張した。

```cpp
struct SSessionEntry {
    std::wstring path;      // 空 = 無題バッファ
    bool bModified = false;
    std::wstring bufFile;   // bModified時のみ有効。SessionBuffers\配下の絶対パス
};
```

ini追加キー: `Session[NN].bModified`、`Session[NN].szBuf`（ファイル名のみ。プロファイルフォルダが移動しても壊れないよう、絶対パスではなく`SessionBuffers\`からの相対名で保存する）。

**後方互換**: 旧フォーマット（キー無し）を読んだ場合は`bModified=false`扱いになるだけで問題なく動く。`NKMM_SESSION_RESTORE`単体（バッファ機能OFF）のini書式ともキー単位で共存できる。

### SessionBuffers\フォルダの管理（`ClearSessionBufferDir()`、唯一の掃除ポイント、非消費型）

`SaveSessionFileList()`自体はiniの`[Session]`セクションの読み書きのみを行い、`SessionBuffers\`フォルダの中身には触れない。フォルダの掃除は`CShareData_IO::ClearSessionBufferDir()`が担い、呼び出し側（`CloseAllEditor()`、`CEditWnd`の個別クローズ時セッションクリア）が**新しいバックアップファイルをダンプする前**に呼んで、フォルダの中身を無条件で全消去する。

当初は「entriesが参照するファイルだけ残し、参照されなくなった古いファイルを削除する」reconcile方式だったが、`SaveSessionFileList()`が`m_bIniReadOnly`等の理由で早期returnした場合、直前にダンプしたファイルがreconcileされずに残骸として残る抜け道があったため、**保存の直前に無条件で全消去してから書き直す**方式に変更した。タイミングによりファイルが使用中でロックされている等の理由で削除に失敗する場合は無視する（次回の全消去時に再度試みる。「タイミングが悪いときは仕方ない」の割り切り）。

**復元時にバックアップファイルを削除する「消費型」にはしなかった**。理由は2つ：
1. 既存の`NKMM_SESSION_RESTORE`（パス一覧）も、`LoadSessionFileList()`は読むだけで復元時に`[Session]`を消しておらず、次回全終了時の上書きまで残る設計。バッファ版もこれに揃え、「復元は読み取り専用、掃除は保存時の全消去のみ」という単一の掃除ポイントに統一した。
2. 消費型だと、復元してから一度も完全終了せずにクラッシュ／強制終了して再度コールドスタートした場合、2回目は既にバックアップファイルが無く復元できない。非消費型なら同じ未保存内容が毎回復元される＝復旧機能としてより堅牢。

トレードオフ: 復元してから次に完全終了するまでの間、未保存内容の平文バックアップが`SessionBuffers\`に残り続ける（露出期間が伸びる）。「バッファ内容の復元」を既存の「セッションの復元」と別チェックボックスに分離したのは、主にこの点をユーザーが意識して選べるようにするため。

`CEditWnd`のWM_DESTROYでの個別クローズ時セッションクリア（`NKMM_SESSION_RESTORE`既存機能、空リストで`SaveSessionFileList()`を呼ぶ）でも`ClearSessionBufferDir()`を呼び、セッションを空クリアするのに合わせて`SessionBuffers\`も掃除する。

バックアップファイルの拡張子は`.txt`ではなく`.swp`（一時的な内部ファイルであることが分かりやすいように、Vim等のスワップファイルの慣習に合わせた）。

### バッファ→ドキュメントの復元（`CDocFileOperation::RestoreBufferOverlay()`）

ダンプの対称として、`CLoadAgent::OnLoad()`が使う`CReadManager::ReadFile_To_CDocLineMgr()`（`CDocLineMgr`だけを書き換える生ロード関数）を直接呼ぶ。`FileLoad()`/`ReloadCurrentFile()`と違い、`InitDoc()`（変更フラグをfalseへリセットしてしまう）や`SetFilePathAndIcon()`（現在のファイルパス＝ドキュメント識別情報の変更）は一切呼ばない。読込後はレイアウト再計算(`SetLayoutInfo()`)を行い、最後に`CDocEditor::SetModified(true, true)`で明示的に変更フラグを立てる。

`RestoreBufferOverlay()`はバックアップファイルが存在しない場合（既に別要因で無くなっている等）は静かに`false`を返し、呼び出し側は何もしない＝通常のパスオープン結果（ディスクの内容）がそのまま残る、というフォールバックになる。

### タブの並び順（`CloseAllEditor()`、`GetOpenedWindowArr()`のソート指定）

`CloseAllEditor()`は元々`CAppNodeManager::GetOpenedWindowArr(&pWndArr, FALSE)`（`bSort=FALSE`）でウィンドウ一覧を取得していた。この`FALSE`は、タブの表示順（ドラッグ操作で並べ替えると更新される`EditNode::m_nIndex`、`CAppNodeManager::ReorderTab()`参照）ではなく、共有メモリ配列上のスロット順（生成順に近い、ソートしない生の順序）を返す。

このため、タブをドラッグで並べ替えた状態で「すべて閉じる」を行うと、セッション保存の並び順（＝閉じる処理自体の順序も）が実際の表示順と食い違い、復元後にタブの並びが変わってしまっていた。`bSort=TRUE`に変更し、`m_nIndex`順（＝実際のタブ表示順）でウィンドウ一覧を取得するようにした。これにより閉じる処理自体もタブの端から順に行われるようになり、セッション保存時の並び順も表示順と一致する。

### 復元直後の再描画（`CDocFileOperation::RestoreBufferOverlay()`）

`RestoreBufferOverlay()`は`DoLoadFlow()`（→`NotifyFinalLoad()`→`CLoadAgent::OnFinalLoad()`）を通らないため、通常のファイル読込の最後に行われる再描画・スクロールバー更新処理もバイパスしてしまう。`CLoadAgent::OnFinalLoad()`にならい、`CEditWnd::Views_RedrawAll()`・`InvalidateRect()`・キャレット位置再設定・`CEditView::AdjustScrollBars()`を明示的に呼ぶようにした。これが無いと、復元直後の初回描画には元の内容（未変更のディスク内容、または空の無題）が残ったままになり、他のタブに切り替える等の再描画が起きるまで反映されなかった。

### 閉じるときの保存確認の抑制（`CEditDoc::OnFileClose()`）

`CloseAllEditor()`は「セッションの復元」機能とは独立して、閉じる前に各ウィンドウの`CEditDoc::OnFileClose()`（`sakura_core/doc/CEditDoc.cpp:869`）で「変更を保存しますか？」の確認ダイアログを出す（既存の仕組み）。バッファ内容の復元がONのとき、この確認は以下のように振る舞いを変える：

- **名前付きファイル**（パス確定済み）: 変更があれば従来通り確認する。バッファへ退避されるからといって、ユーザーが実際にファイルへ保存したいかどうかは別問題のため。
- **無題バッファ**（パス未確定）: 変更の有無に関わらず確認しない。閉じても内容はバッファとして自動的に退避され、次回起動時に復元されるため、確認する意味が無い。

判定条件は`!IsValidPath() && m_bSessionHandledByCloseAll && m_bSessionBufferCaptured`（詳細は次項「OSシャットダウン対応との統合」参照）。個別クローズやグループだけ閉じる操作、バッファ機能OFFの場合は、これらの条件が揃わないため、従来通り確認ダイアログが出る。

### OSシャットダウン対応との統合（レース条件の発見と対処、20260815）

`NKMM_SESSION_RESTORE`にOSシャットダウン（WM_QUERYENDSESSION/WM_ENDSESSION）対応が別途追加された後、本バッファ機能との組み合わせを精査したところ、**保存確認の抑制ロジックに理論上のデータロス系のレース条件**が見つかったため修正した。

**問題**: 当初の抑制条件は`!IsValidPath() && m_bSessionHandledByCloseAll && m_bRestoreSessionBuffer`だった。`CloseAllEditor()`経由の場合、`CControlTray::SaveSessionSnapshot()`が全ウィンドウのダンプを完了させてから初めて各ウィンドウの`OnClose()`/`OnFileClose()`を呼ぶため（単一プロセス内の同期ループ）問題は起きない。しかしOSシャットダウン経由の場合、各ウィンドウ（＝別プロセス）が個別にWM_QUERYENDSESSIONを受け取り、OSがその配送順序をシリアライズする保証はない。そのため、最初にCASで勝ったウィンドウAが他ウィンドウBのバッファをダンプし終える**前**に、B自身のWM_QUERYENDSESSION（OSから直接）が先に処理され、Bの`OnFileClose()`が「`m_bSessionHandledByCloseAll`が立っている＝どうせ退避される」と判断して確認を抑制し、そのままウィンドウが閉じてしまう競合があり得た。この場合、Bの内容は実際にはまだダンプされておらず、確認も出ないため、未保存の変更がそのまま失われる。

**対処**: 「誰かが保存処理を始めた」ことしか示さない共有（複数プロセス）フラグの`m_bSessionHandledByCloseAll`だけに頼るのをやめ、「自分の内容が実際にダンプされたか」を示すローカルな（プロセス内、`CEditDoc`の）フラグ`m_bSessionBufferCaptured`を新設した：

- `MYWM_GETFILEINFO`ハンドラ（`SaveSessionSnapshot()`が各ウィンドウの情報を問い合わせる際、ループの先頭で必ず呼ばれる）で、まず`false`にリセットする。以前の（中断された等の理由で使われなかった）試みのダンプ実績が今回の試みに紛れ込むのを防ぐため。
- `MYWM_DUMPBUFFER`ハンドラで、ダンプに実際に成功した時だけ`true`にする。
- `CEditDoc::OnFileClose()`の抑制条件を`!IsValidPath() && m_bSessionHandledByCloseAll && m_bSessionBufferCaptured`に変更（`m_bRestoreSessionBuffer`の直接チェックは削除。`m_bSessionBufferCaptured`がtrueになる時点で当時ONだったことが保証されるため）。

`m_bSessionBufferCaptured`単独ではなく`m_bSessionHandledByCloseAll`との組み合わせで見ている点が重要：`m_bSessionHandledByCloseAll`は全終了処理の開始/終了（成功・中断いずれも）で確実にFALSEへ戻る（`CloseAllEditor()`は`RequestCloseEditor()`の戻り値に関わらず必ず戻す。WM_QUERYENDSESSION側も拒否時・WM_ENDSESSION(wParam==FALSE)時に戻す）ため、無関係な後日の個別クローズで過去のダンプ実績が誤って抑制に使われることもない。

### 復元時のチェックボックス解釈（`CNormalProcess::InitializeProcess()`）

`LoadSessionFileList()`はチェックボックスの状態に関わらず常にフル情報（`bModified`/`bufFile`込み）を返す。**チェックボックスの状態が解釈を決める**：

- `m_bRestoreSessionBuffer`がONなら、`bModified`なエントリのバッファ復元パスを`CCommandLine`へ橋渡しし、パスが空（無題）のエントリもそのまま復元対象に含める。
- OFFなら`bModified`/`bufFile`を無視し、パスが空（無題）のエントリは開く意味が無いので除外する。これにより、iniが以前バッファ機能ONで保存されたものであっても、チェックボックスをOFFに戻せば`NKMM_SESSION_RESTORE`単体と同じ「パス一覧のみ復元」の挙動に戻る。

## 確定した処理フロー（20260815時点）

ここまでの実装・修正を踏まえた、全終了時の保存確認〜バッファ退避〜（次回起動時の）復元までの一連の流れを、経路別にまとめる。前提として、以降で出てくる状態は全てこの2つ：

- `DLLSHAREDATA::m_sFlags::m_bSessionHandledByCloseAll`（共有メモリ＝**全プロセスから見える**フラグ）：「（自分か他のウィンドウかを問わず）誰かが全終了のセッション保存処理を始めた」ことを示す。
- `CEditDoc::m_bSessionBufferCaptured`（**そのプロセス＝そのウィンドウ内だけ**のローカル変数）：「このウィンドウ自身の内容が、実際にSessionBuffers\へダンプされ終わった」ことを示す。

### A. `CloseAllEditor()`経由（「ファイル→サクラエディタの終了」「編集の全終了」など、アプリ自身の操作）

1. `CAppNodeManager::GetOpenedWindowArr(&pWndArr, TRUE)`で、閉じ始める前に、**タブの実際の表示順**（`bSort=TRUE`。ドラッグ操作の並べ替えを反映した`EditNode::m_nIndex`順）で全ウィンドウの一覧を取得する。
2. 「実質的に全終了か」（`bClosingEverything`）を判定する。
3. 全終了かつ「セッションの復元」ONなら、`CControlTray::SaveSessionSnapshot(pWndArr, n)`を呼ぶ。この関数が保存処理の本体：
   1. バッファ内容の復元がONなら、`CShareData_IO::ClearSessionBufferDir()`で`SessionBuffers\`フォルダの中身を無条件に全消去する（唯一の掃除ポイント）。
   2. 一覧の各ウィンドウへ`MYWM_GETFILEINFO`を送ってパス・変更フラグ等を取得する。**この時点で、送信先ウィンドウ自身の`m_bSessionBufferCaptured`を`false`にリセットする**（過去の中断された試行の実績が紛れ込まないように）。
   3. 変更ありのウィンドウには`MYWM_DUMPBUFFER`を送り、同期的にバッファ内容をSessionBuffers\へダンプさせる。**ダンプに成功したら、送信先ウィンドウ自身が`m_bSessionBufferCaptured`を`true`にする**。
   4. 集めた情報を`CShareData_IO::SaveSessionFileList()`でini`[Session]`セクションへ書き込む。
   5. `m_bSessionHandledByCloseAll`を`TRUE`にする。
4. `CAppNodeGroupHandle::RequestCloseEditor(pWndArr, n, ...)`が、一覧の全ウィンドウへ**同期的に1枚ずつ順番に**`MYWM_CLOSE`を送る。この時点で、手順3のダンプは（同じスレッド内で完全に完了してから4に進むため）**全ウィンドウ分すでに終わっている**。
5. 各ウィンドウの`MYWM_CLOSE`ハンドラ（`CEditWnd::OnClose()`）が`CEditDoc::OnFileClose()`を呼ぶ。無題（パス未確定）バッファの場合、`!IsValidPath() && m_bSessionHandledByCloseAll && m_bSessionBufferCaptured`が全て真なら「変更を保存しますか？」の確認ダイアログを出さずに閉じてよいと判断する。3.のダンプが必ず先に完了しているため、ここでレース（後述）は起きない。
6. `RequestCloseEditor()`が戻ったら（ユーザーが全終了自体をキャンセルした場合も含め、成否に関わらず）`m_bSessionHandledByCloseAll`を`FALSE`に戻す。

### B. `WM_QUERYENDSESSION`経由（Windowsのシャットダウン・ログオフ）

「タブ＝別プロセス」構成のため、`CloseAllEditor()`のような単一の集約点が存在せず、**開いている各ウィンドウ（＝別プロセス）が、OSから個別に**`WM_QUERYENDSESSION`**を受け取る**。

1. 各ウィンドウの`CEditWnd::DispatchEvent()`が`WM_QUERYENDSESSION`を受け取ったら、まず`::InterlockedCompareExchange()`で`m_bSessionHandledByCloseAll`をFALSE→TRUEへ**アトミックに**遷移させようとする。これに**最初に成功した1つのウィンドウ（＝プロセス）だけ**が「自分がこの終了処理の代表」と判定される（他のウィンドウが同じ判定を同時に試みても、勝てるのは1つだけ）。
2. 代表になったウィンドウだけが、`GetOpenedWindowArr(&p, TRUE)`で改めて全ウィンドウ一覧を取得し、`SaveSessionSnapshot(p, n)`を呼ぶ。中身はA.の3と全く同じ（掃除→ダンプ→ini書き込み→フラグON）。
3. **全ての**ウィンドウ（代表になったかどうかを問わない）が、自分自身の`WM_QUERYENDSESSION`ハンドラの中で`CEditWnd::OnClose()`→`CEditDoc::OnFileClose()`を呼ぶ。ここでA.5と同じ3条件をチェックする。
4. `OnClose()`が「閉じてよい」を返せばそのウィンドウを`DestroyWindow`。「拒否する」を返した場合は、そのウィンドウが`m_bSessionHandledByCloseAll`を`FALSE`に戻す（次回の通常終了に備える）。
5. `WM_ENDSESSION`を`wParam==FALSE`（シャットダウン/ログオフ自体がキャンセルされた）で受け取った場合も、念のため同様に`m_bSessionHandledByCloseAll`を`FALSE`に戻す（二重の安全策）。

**なぜB.3にレースの危険があるか**：OSが`WM_QUERYENDSESSION`を全ウィンドウへ厳密に1つずつ順番通り配送する保証はない（各ウィンドウは別プロセスであり、応答が遅いウィンドウをOS側がタイムアウトさせて他へ問い合わせを進める、といった状況が実際にあり得る）。そのため、代表ウィンドウがB.2のダンプを**まだB自身に対して行っていない**うちに、ウィンドウB自身の`WM_QUERYENDSESSION`が先に処理されてしまう可能性がある。もしB.3の判定が`m_bSessionHandledByCloseAll`（＝「誰かが処理を始めた」）だけを見ていたら、「代表が処理中だからどうせ退避される」と誤判定して確認を抑制し、実際にはまだダンプされていないBの内容がそのまま失われる。**これを防ぐために、ローカルな`m_bSessionBufferCaptured`（「自分自身が実際にダンプされたか」）を追加で見ている**。ダンプがまだなら`false`のままなので、レースが起きても確認ダイアログは正しく表示される（安全側に倒れる）。

## 既知の制約

- 復元は非消費型：バックアップファイルと`[Session]`メタデータは、次に「全終了（保存フックが走る）」するまでディスクに残り続ける。復元してから一度も完全終了せずに長期間使い続けた場合、その間ずっと未保存内容の平文バックアップが`SessionBuffers\`に残る。
- 強制終了（タスクキル等、WM_DESTROYを経由しない終了）では保存されない（`NKMM_SESSION_RESTORE`既存の制約と同じ）。
- 大きく変更したファイルの全終了は、その場でファイルサイズ分のダンプI/Oが発生する。サイズ上限は設けていない。
