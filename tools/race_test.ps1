<#
.SYNOPSIS
  NKMM_SESSION_RESTORE_BUFFER のWM_QUERYENDSESSIONレース条件を手動で再現するためのテストツール。

.DESCRIPTION
  実際にWindowsをシャットダウンせず、sakura.exeのウィンドウへ直接WM_QUERYENDSESSIONメッセージを
  送りつけることで、狙ったタイミングでレース条件を発生させる。

.USAGE
  1. sakuraを起動し、複数タブを開く（少なくとも1つは無題バッファに未保存の文字を入力しておく）。
     「セッションの復元」「バッファ内容の復元」は両方ONにしておくこと。
  2. sakura_core/_main/CControlTray.cpp の SaveSessionSnapshot() に仕込んだ一時Sleep(5000)を
     含むビルドを実行しておく（本番コードには絶対に残さないこと）。
  3. まずウィンドウ一覧を確認:
       .\race_test.ps1 -List
  4. 先に処理させたい側（A）のウィンドウへ、非同期(PostMessage)でWM_QUERYENDSESSIONを送る:
       .\race_test.ps1 -Hwnd 0x00123456 -Post
     Aの処理が始まり、CASに成功した直後、ダンプ対象が複数あればSleep(5000)に入る
     （Aと同じプロセスの別ウィンドウでも、別プロセスの他ウィンドウでもよい。SaveSessionSnapshot
     のループが「自分より後の配列要素」をダンプする直前でSleepするので、Aを一番手前の
     ウィンドウにしておくと確実に他ウィンドウのダンプ前にSleepへ入る）。
  5. そのSleep中(5秒以内)に、レースさせたい側（B、未保存の無題バッファ）へ同じくPostMessageで送る:
       .\race_test.ps1 -Hwnd 0x00456789 -Post
  6. Bの画面を目視:
       - 修正後のコード: 「変更を保存しますか？」の確認ダイアログが出る（正しい）
       - 修正を外した状態で試すと: 確認なしでそのままウィンドウが消える（不具合の再現）
  7. 確認できたら、CControlTray.cppの一時Sleepと、このスクリプトでの検証作業を終了する。
     Sleep(5000)の行は必ず削除してから通常のビルド・コミットに戻すこと。

.NOTES
  WM_QUERYENDSESSION = 0x0011。PostMessageは送信元をブロックしないため、Aへ送った直後に
  すぐBへ送れる（SendMessageだと応答が返るまでブロックしてしまい、意図したタイミングを作れない）。
  実際のOSシャットダウンは発生しない。あくまでメッセージを1つ送りつけるだけの操作。
#>

param(
    [switch]$List,
    [string]$Hwnd,
    [switch]$Post,
    [switch]$Send
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Collections.Generic;

public class Win32RaceTest {
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);

    [DllImport("user32.dll")]
    public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll", CharSet = CharSet.Auto)]
    public static extern IntPtr SendMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    public class WinInfo {
        public IntPtr Hwnd;
        public uint Pid;
        public string Title;
    }

    public static List<WinInfo> EnumerateVisible(HashSet<uint> targetPids) {
        var result = new List<WinInfo>();
        EnumWindows(delegate(IntPtr hWnd, IntPtr lParam) {
            if (!IsWindowVisible(hWnd)) return true;
            uint pid;
            GetWindowThreadProcessId(hWnd, out pid);
            if (!targetPids.Contains(pid)) return true;
            var sb = new StringBuilder(256);
            GetWindowText(hWnd, sb, 256);
            string title = sb.ToString();
            if (string.IsNullOrEmpty(title)) return true;
            result.Add(new WinInfo { Hwnd = hWnd, Pid = pid, Title = title });
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
"@

$WM_QUERYENDSESSION = 0x0011

function Get-SakuraWindowList {
    $procs = Get-Process -Name "sakura" -ErrorAction SilentlyContinue
    if (-not $procs) {
        Write-Host "sakura.exe のプロセスが見つかりません。先にsakuraを起動してください。" -ForegroundColor Yellow
        return
    }
    $pidSet = New-Object 'System.Collections.Generic.HashSet[uint32]'
    foreach ($p in $procs) { [void]$pidSet.Add([uint32]$p.Id) }

    $windows = [Win32RaceTest]::EnumerateVisible($pidSet)
    if ($windows.Count -eq 0) {
        Write-Host "可視ウィンドウが見つかりませんでした。" -ForegroundColor Yellow
        return
    }
    $windows | ForEach-Object {
        [PSCustomObject]@{
            HWND  = ("0x{0:X8}" -f $_.Hwnd.ToInt64())
            PID   = $_.Pid
            Title = $_.Title
        }
    } | Format-Table -AutoSize
}

function Send-QueryEndSession {
    param([IntPtr]$TargetHwnd, [bool]$UsePost)
    if ($UsePost) {
        $ok = [Win32RaceTest]::PostMessage($TargetHwnd, $WM_QUERYENDSESSION, [IntPtr]::Zero, [IntPtr]::Zero)
        Write-Host ("PostMessage(WM_QUERYENDSESSION) -> {0:X8} : {1}" -f $TargetHwnd.ToInt64(), $ok)
    } else {
        Write-Host ("SendMessage(WM_QUERYENDSESSION) -> {0:X8} : 応答待ちでブロックします..." -f $TargetHwnd.ToInt64())
        $ret = [Win32RaceTest]::SendMessage($TargetHwnd, $WM_QUERYENDSESSION, [IntPtr]::Zero, [IntPtr]::Zero)
        Write-Host ("戻り値: {0}" -f $ret.ToInt64())
    }
}

if ($List) {
    Get-SakuraWindowList
    return
}

if ($Hwnd) {
    $hwndValue = [IntPtr]([Convert]::ToInt64($Hwnd, 16))
    $usePost = $Post -or (-not $Send)   # 既定はPost（非ブロッキング）
    Send-QueryEndSession -TargetHwnd $hwndValue -UsePost:$usePost
    return
}

Write-Host "使い方:"
Write-Host "  .\race_test.ps1 -List                    ウィンドウ一覧を表示"
Write-Host "  .\race_test.ps1 -Hwnd 0x00123456 -Post    非同期でWM_QUERYENDSESSIONを送る(既定)"
Write-Host "  .\race_test.ps1 -Hwnd 0x00123456 -Send    同期(ブロッキング)で送る"
