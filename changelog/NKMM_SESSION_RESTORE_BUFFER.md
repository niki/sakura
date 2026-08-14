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
- 名前付きファイルは`NKMM_SESSION_RESTORE`既存のルール通り、ウィンドウが1枚だけの終了（`n==1`）は対象外（通常の単一ファイル起動と同じ意味なので）。ただし**無題（パス無し）で変更ありのバッファはn==1でも対象**とする（20260814(2)）。名前付きファイルと違い、パスによる代替の復元手段が無く、保存しなければ内容そのものが失われるため。

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

`SaveSessionFileList()`自体はiniの`[Session]`セクションの読み書きのみを行い、`SessionBuffers\`フォルダの中身には触れない。フォルダの掃除は`CShareData_IO::ClearSessionBufferDir()`が担い、呼び出し側（`CloseAllEditor()`、`CEditWnd`の個別クローズ時セッションクリア）が**新しいバックアップファイルをダンプする前**に呼んで、フォルダの中身を無条件で全消去する（20260814(3)）。

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

このため、タブをドラッグで並べ替えた状態で「すべて閉じる」を行うと、セッション保存の並び順（＝閉じる処理自体の順序も）が実際の表示順と食い違い、復元後にタブの並びが変わってしまっていた。`bSort=TRUE`に変更し、`m_nIndex`順（＝実際のタブ表示順）でウィンドウ一覧を取得するようにした（20260814(3)）。これにより閉じる処理自体もタブの端から順に行われるようになり、セッション保存時の並び順も表示順と一致する。

### 復元直後の再描画（`CDocFileOperation::RestoreBufferOverlay()`）

`RestoreBufferOverlay()`は`DoLoadFlow()`（→`NotifyFinalLoad()`→`CLoadAgent::OnFinalLoad()`）を通らないため、通常のファイル読込の最後に行われる再描画・スクロールバー更新処理もバイパスしてしまう。`CLoadAgent::OnFinalLoad()`にならい、`CEditWnd::Views_RedrawAll()`・`InvalidateRect()`・キャレット位置再設定・`CEditView::AdjustScrollBars()`を明示的に呼ぶようにした。これが無いと、復元直後の初回描画には元の内容（未変更のディスク内容、または空の無題）が残ったままになり、他のタブに切り替える等の再描画が起きるまで反映されなかった。

### 閉じるときの保存確認の抑制（`CEditDoc::OnFileClose()`）

`CloseAllEditor()`は「セッションの復元」機能とは独立して、閉じる前に各ウィンドウの`CEditDoc::OnFileClose()`（`sakura_core/doc/CEditDoc.cpp:869`）で「変更を保存しますか？」の確認ダイアログを出す（既存の仕組み）。バッファ内容の復元がONのとき、この確認は以下のように振る舞いを変える（20260814(4)）：

- **名前付きファイル**（パス確定済み）: 変更があれば従来通り確認する。バッファへ退避されるからといって、ユーザーが実際にファイルへ保存したいかどうかは別問題のため。
- **無題バッファ**（パス未確定）: 変更の有無に関わらず確認しない。閉じても内容はバッファとして自動的に退避され、次回起動時に復元されるため、確認する意味が無い。

判定には`CloseAllEditor()`が全終了処理中に立てる既存の一時フラグ`DLLSHAREDATA::m_sFlags::m_bSessionHandledByCloseAll`を流用し、「本当にこのウィンドウの内容がバッファとして退避される場面かどうか」（`m_bRestoreSessionBuffer`ON かつ 全終了処理中）を確認してから抑制する。個別クローズやグループだけ閉じる操作、バッファ機能OFFの場合は、このフラグが立たない／条件を満たさないため、従来通り確認ダイアログが出る。

### 復元時のチェックボックス解釈（`CNormalProcess::InitializeProcess()`）

`LoadSessionFileList()`はチェックボックスの状態に関わらず常にフル情報（`bModified`/`bufFile`込み）を返す。**チェックボックスの状態が解釈を決める**：

- `m_bRestoreSessionBuffer`がONなら、`bModified`なエントリのバッファ復元パスを`CCommandLine`へ橋渡しし、パスが空（無題）のエントリもそのまま復元対象に含める。
- OFFなら`bModified`/`bufFile`を無視し、パスが空（無題）のエントリは開く意味が無いので除外する。これにより、iniが以前バッファ機能ONで保存されたものであっても、チェックボックスをOFFに戻せば`NKMM_SESSION_RESTORE`単体と同じ「パス一覧のみ復元」の挙動に戻る。

## 既知の制約

- 復元は非消費型：バックアップファイルと`[Session]`メタデータは、次に「全終了（保存フックが走る）」するまでディスクに残り続ける。復元してから一度も完全終了せずに長期間使い続けた場合、その間ずっと未保存内容の平文バックアップが`SessionBuffers\`に残る。
- 強制終了（タスクキル等、WM_DESTROYを経由しない終了）では保存されない（`NKMM_SESSION_RESTORE`既存の制約と同じ）。
- 大きく変更したファイルの全終了は、その場でファイルサイズ分のダンプI/Oが発生する。サイズ上限は設けていない。
- OSシャットダウン（WM_QUERYENDSESSION/WM_ENDSESSION）は`NKMM_SESSION_RESTORE`既存の制約（未対応、検討中）がそのまま引き継がれる。
