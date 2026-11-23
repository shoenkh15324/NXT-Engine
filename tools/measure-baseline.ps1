param(
    [string]$Exe = 'build\msvc-bench\apps\sandbox\sandbox.exe',
    [int]$Objects = 100000,
    [int]$Seconds = 40,
    [int]$Runs = 7,
    [string]$Present = 'immediate'
)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class NxtWin32 {
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string className, string windowName);
    [DllImport("user32.dll")]
    public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
}
"@

function Get-Median([double[]]$values) {
    if ($values.Count -eq 0) { return 0.0 }
    $sorted = @($values | Sort-Object)
    $middle = [int]($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) { return [double]$sorted[$middle] }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

$exePath = (Resolve-Path $Exe).Path
$logDir = Join-Path (Split-Path $exePath) 'logs'
$rows = @()

# 회차를 바깥에 두고 off/on을 교차한다. 한 설정을 몰아서 재면 세션 후반의 클럭·백그라운드
# 변화가 그 설정에만 붙는다.
for ($run = 1; $run -le $Runs; ++$run) {
    foreach ($culling in @('off', 'on')) {
        $started = Get-Date
        $process = Start-Process -FilePath $exePath `
            -ArgumentList @('--objects', "$Objects", '--culling', $culling, '--present', $Present, '--log-level', 'info') -PassThru

        Start-Sleep -Seconds $Seconds

        $window = [NxtWin32]::FindWindow($null, 'NXT Sandbox')
        if ($window -ne [IntPtr]::Zero) {
            [void][NxtWin32]::PostMessage($window, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) # WM_CLOSE
        }
        $null = $process.WaitForExit(5000)
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }

        $log = Get-ChildItem -Path $logDir -Filter '*.log' |
            Where-Object { $_.LastWriteTime -ge $started } |
            Sort-Object LastWriteTime | Select-Object -Last 1
        if (-not $log) { Write-Warning "run $run ($culling): no log"; continue }

        $pattern = 'frame: n=(\d+) cpu p50=([\d.]+) p95=([\d.]+)ms gpu~? p50=([\d.]+) p95=([\d.]+)ms ' +
                   'fence p50=([\d.]+) p95=([\d.]+)ms draws=(\d+)'
        $windows = @()
        foreach ($line in Get-Content $log.FullName) {
            $match = [regex]::Match($line, $pattern)
            if (-not $match.Success) { continue }
            $windows += [pscustomobject]@{
                Frames   = [double]$match.Groups[1].Value
                CpuP50   = [double]$match.Groups[2].Value
                CpuP95   = [double]$match.Groups[3].Value
                GpuP50   = [double]$match.Groups[4].Value
                GpuP95   = [double]$match.Groups[5].Value
                FenceP50 = [double]$match.Groups[6].Value
                FenceP95 = [double]$match.Groups[7].Value
                Draws    = [double]$match.Groups[8].Value
            }
        }

        # 워밍업 직후 partial 창과 종료 직전 partial 창을 버린다.
        if ($windows.Count -gt 2) { $windows = $windows[1..($windows.Count - 2)] }
        if ($windows.Count -eq 0) { Write-Warning "run $run ($culling): no frame windows"; continue }

        $fps = Get-Median $windows.Frames
        $rows += [pscustomobject]@{
            Run         = $run
            Culling     = $culling
            Fps         = [math]::Round($fps, 1)
            Draws       = [int](Get-Median $windows.Draws)
            CpuP50      = [math]::Round((Get-Median $windows.CpuP50), 3)
            CpuP95      = [math]::Round(($windows.CpuP95 | Measure-Object -Maximum).Maximum, 3)
            GpuP50      = [math]::Round((Get-Median $windows.GpuP50), 3)
            GpuP95      = [math]::Round(($windows.GpuP95 | Measure-Object -Maximum).Maximum, 3)
            FenceP50    = [math]::Round((Get-Median $windows.FenceP50), 3)
            FenceP95    = [math]::Round(($windows.FenceP95 | Measure-Object -Maximum).Maximum, 3)
            Throughput  = [int]($fps * $Objects)
            Windows     = $windows.Count
        }
    }
}

$rows | Format-Table -AutoSize

Write-Output "present=$Present objects=$Objects seconds=$Seconds runs=$Runs"
foreach ($row in $rows) {
    '| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} | {8} | {9} | {10} |' -f `
        $row.Run, $row.Culling, $row.Fps, $row.Draws, $row.CpuP50, $row.CpuP95,
        $row.GpuP50, $row.GpuP95, $row.FenceP50, $row.FenceP95, $row.Throughput
}
