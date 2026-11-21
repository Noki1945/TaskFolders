# Run in an interactive desktop session with TaskFolders closed.
param([Parameter(Mandatory)][string]$Exe)
$ErrorActionPreference = 'Stop'
if (Get-Process TaskFolders -ErrorAction SilentlyContinue) { throw 'Close TaskFolders before running the desktop test.' }
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class SurfaceTest {
  [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent,IntPtr after,string cls,string name);
  [DllImport("user32")] public static extern int GetSystemMetrics(int id);
  [DllImport("user32")] public static extern IntPtr GetParent(IntPtr w);
  [DllImport("user32",EntryPoint="SendMessageW")] public static extern IntPtr Query(IntPtr w,uint msg,IntPtr a,IntPtr b);
  public struct Rect { public int left,top,right,bottom; }
  public struct Point { public int x,y; }
  [DllImport("user32")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr mode);
  [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
  [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr w);
  [DllImport("user32")] public static extern bool GetWindowRect(IntPtr w,out Rect r);
  [DllImport("user32")] public static extern IntPtr WindowFromPoint(Point p);
  [DllImport("user32")] public static extern bool GetCursorPos(out Point p);
  [DllImport("user32")] public static extern bool SetCursorPos(int x,int y);
  [DllImport("user32")] public static extern uint GetDpiForWindow(IntPtr w);
  [DllImport("user32")] public static extern bool PostMessage(IntPtr w,uint msg,IntPtr a,IntPtr b);
  [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr w,uint msg,IntPtr arg,string text);
  [DllImport("user32")] public static extern IntPtr GetDlgItem(IntPtr w,int id);
}
"@
function Until([scriptblock]$Condition,[string]$Failure) {
    for ($i=0; $i -lt 80; $i++) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 100
    }
    throw $Failure
}
function HostWindow { [SurfaceTest]::FindWindow('Shell_TrayWnd',[NullString]::Value) }
function Strip { [SurfaceTest]::FindWindowEx((HostWindow),[IntPtr]::Zero,'TaskFolders.Surface','TaskFolders · Папки панели задач') }
function Window([string]$name) { if ($name -like '*Настройки') { [SurfaceTest]::FindWindow('#32770',$name) } else { [SurfaceTest]::FindWindow('TaskFolders.Surface',$name) } }
function RectOf($w) {
    $r=New-Object SurfaceTest+Rect
    [void][SurfaceTest]::GetWindowRect($w,[ref]$r)
    $r
}
function Tap($w,[int]$x,[int]$y) {
    $pos=[IntPtr](($y -shl 16) -bor $x)
    [void][SurfaceTest]::PostMessage($w,513,[IntPtr]1,$pos)
    [void][SurfaceTest]::PostMessage($w,514,[IntPtr]0,$pos)
}
$previous=[SurfaceTest]::SetThreadDpiAwarenessContext([IntPtr](-4))
$cursor=New-Object SurfaceTest+Point
[void][SurfaceTest]::GetCursorPos([ref]$cursor)
$tmp=Join-Path ([IO.Path]::GetTempPath()) ('TaskFolders-test-'+[guid]::NewGuid())
[IO.Directory]::CreateDirectory($tmp) | Out-Null
try {
    $copy=Join-Path $tmp 'TaskFolders.exe'
    Copy-Item -LiteralPath $Exe -Destination $copy
    $deviceDir=Join-Path (Split-Path $Exe) 'BatteryHub'
    Copy-Item -LiteralPath $deviceDir -Destination (Join-Path $tmp 'BatteryHub') -Recurse
    $link=Join-Path $tmp 'Probe.lnk'
    $marker=Join-Path $tmp 'launched.txt'
    $shortcut=(New-Object -ComObject WScript.Shell).CreateShortcut($link)
    $shortcut.TargetPath=$env:ComSpec
    $shortcut.Arguments='/d /c echo launched>"'+$marker+'"'
    $shortcut.WindowStyle=7
    $shortcut.Save()
    $writer=New-Object IO.BinaryWriter([IO.File]::Create((Join-Path $tmp 'folders.dat')))
    try {
        $writer.Write([uint32]0x31444654)
        $writer.Write([uint32]2)
        $writer.Write([uint32]4)
        $writer.Write([Text.Encoding]::Unicode.GetBytes('Test'))
        $writer.Write([uint32]5)
        foreach($i in 1..4) {
            $other=Join-Path $tmp ("Other$i.lnk")
            $writer.Write([uint32]$other.Length)
            $writer.Write([Text.Encoding]::Unicode.GetBytes($other))
        }
        $writer.Write([uint32]$link.Length)
        $writer.Write([Text.Encoding]::Unicode.GetBytes($link))
        $writer.Write([uint32]5)
        $writer.Write([Text.Encoding]::Unicode.GetBytes('Empty'))
        $writer.Write([uint32]0)
    } finally { $writer.Close() }
    $app=Start-Process -FilePath $copy -ArgumentList '--portable' -PassThru
    $hostBar=HostWindow
    $hostRect=RectOf $hostBar
    Until { [SurfaceTest]::IsWindowVisible((Strip)) } 'Taskbar child missing'
    Until {
        [void][SurfaceTest]::SetCursorPos($hostRect.right-35,([SurfaceTest]::GetSystemMetrics(1)-1))
        (RectOf (Strip)).bottom -le [SurfaceTest]::GetSystemMetrics(1)
    } 'Taskbar did not reveal'
    # Allow the monitor's first device scan and the taskbar slide animation to settle.
    Start-Sleep -Milliseconds 1200
    $bar=Strip
    $r=RectOf $bar
    $hostRect=RectOf $hostBar
    $tray=RectOf ([SurfaceTest]::FindWindowEx($hostBar,[IntPtr]::Zero,'TrayNotifyWnd',[NullString]::Value))
    if([SurfaceTest]::GetParent($bar) -ne $hostBar -or $r.left -lt $hostRect.left -or $r.top -lt $hostRect.top -or $r.bottom -gt $hostRect.bottom -or $r.right -gt $tray.left){throw 'Invalid child placement'}
    $scale=[SurfaceTest]::GetDpiForWindow($bar)/96.0
    [void][SurfaceTest]::SetCursorPos($r.left+20*$scale,$r.top+15*$scale)
    Until { [SurfaceTest]::IsWindowVisible((Window 'TaskFolders · Приложения')) } 'Hover did not open folder'
    $pop=Window 'TaskFolders · Приложения'
    $p=RectOf $pop
    if($p.bottom -gt $r.top -and $p.top -lt $r.bottom){throw 'Popup overlaps strip'}
    [void][SurfaceTest]::SetCursorPos($p.left+20*$scale,$p.top+50*$scale)
    Start-Sleep -Milliseconds 450
    if(![SurfaceTest]::IsWindowVisible($pop)){throw 'Popup closed during crossing'}
    Tap $pop ($p.right-$p.left-20*$scale) ($p.bottom-$p.top-15*$scale)
    Tap $pop (20*$scale) (50*$scale)
    Until { Test-Path -LiteralPath $marker } 'Shortcut launch failed'
    Tap $bar (66*$scale) (15*$scale)
    Until { [SurfaceTest]::IsWindowVisible($pop) } 'Second folder missing'
    Tap $pop (20*$scale) (50*$scale)
    Until { [SurfaceTest]::IsWindowVisible((Window 'TaskFolders · Настройки')) } 'Editor missing'
    $dlg=Window 'TaskFolders · Настройки'
    [void][SurfaceTest]::Query($dlg,273,[IntPtr]124,[IntPtr]0)
    Until { [SurfaceTest]::IsWindowVisible([SurfaceTest]::GetDlgItem($dlg,770)) } 'Media tab missing'
    [void][SurfaceTest]::PostMessage($dlg,273,[IntPtr]112,[IntPtr]0)
    $tree=[SurfaceTest]::GetDlgItem($dlg,113)
    Until { [SurfaceTest]::Query($tree,4357,[IntPtr]0,[IntPtr]0).ToInt32() -gt 12 } 'Battery settings tree missing'
    [void][SurfaceTest]::PostMessage($dlg,273,[IntPtr]122,[IntPtr]0)
    [void][SurfaceTest]::PostMessage($dlg,273,[IntPtr]111,[IntPtr]0)
    Until { [SurfaceTest]::IsWindowVisible([SurfaceTest]::GetDlgItem($dlg,102)) } 'Folder tab did not return'
    [void][SurfaceTest]::SendMessage([SurfaceTest]::GetDlgItem($dlg,102),12,[IntPtr]0,'Second')
    [void][SurfaceTest]::Query($dlg,273,[IntPtr]103,[IntPtr]0)
    $label=[SurfaceTest]::GetDlgItem($dlg,123)
    if([SurfaceTest]::Query($label,240,[IntPtr]0,[IntPtr]0).ToInt32() -ne 0){throw 'New label enabled by default'}
    [void][SurfaceTest]::Query($label,245,[IntPtr]0,[IntPtr]0)
    [void][SurfaceTest]::PostMessage($dlg,273,[IntPtr]1,[IntPtr]0)
    Until { (Window 'TaskFolders · Настройки') -eq [IntPtr]::Zero } 'Save dialog still open'
    $bytes=[IO.File]::ReadAllBytes((Join-Path $tmp 'folders.dat'))
    if([BitConverter]::ToUInt32($bytes,4) -ne 3 -or ![Text.Encoding]::Unicode.GetString($bytes).Contains('Second')) { throw 'Folder creation failed' }
    if([BitConverter]::ToUInt32($bytes,$bytes.Length-4) -ne 1){ throw 'Label setting did not persist' }
    $app.Refresh()
    $cpu=$app.TotalProcessorTime.TotalMilliseconds
    Start-Sleep -Seconds 5
    $app.Refresh()
    "Desktop test passed. Working set {0:N1} MiB. CPU over 5 s {1:N1} ms." -f ($app.WorkingSet64/1MB),($app.TotalProcessorTime.TotalMilliseconds-$cpu)
} finally {
    if($app -and !$app.HasExited) {
    $bar=Window 'TaskFolders.Controller'
        if($bar){ [void][SurfaceTest]::PostMessage($bar,16,[IntPtr]0,[IntPtr]0) }
        if(!$app.WaitForExit(2000)){ $app.Kill(); $app.WaitForExit() }
    }
    $ownedMonitor=Get-Process BatteryHub -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq (Join-Path $tmp 'BatteryHub\BatteryHub.exe') }
    if($ownedMonitor){[void]$ownedMonitor.WaitForExit(6000)}
    [void][SurfaceTest]::SetCursorPos($cursor.x,$cursor.y)
    [void][SurfaceTest]::SetThreadDpiAwarenessContext($previous)
    if((Split-Path -Parent $tmp) -eq [IO.Path]::GetTempPath().TrimEnd('\') -and (Split-Path -Leaf $tmp) -match '^TaskFolders-test-[0-9a-f-]{36}$') {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
}