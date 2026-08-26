$ErrorActionPreference = "Continue"

if (-not ("Compyler.Probe" -as [type])) {
    Add-Type -Namespace Compyler -Name Probe -MemberDefinition @'
[StructLayout(LayoutKind.Sequential)]
public struct PMC {
    public uint cb;
    public uint PageFaultCount;
    public IntPtr PeakWorkingSetSize;
    public IntPtr WorkingSetSize;
    public IntPtr QuotaPeakPagedPoolUsage;
    public IntPtr QuotaPagedPoolUsage;
    public IntPtr QuotaPeakNonPagedPoolUsage;
    public IntPtr QuotaNonPagedPoolUsage;
    public IntPtr PagefileUsage;
    public IntPtr PeakPagefileUsage;
    public IntPtr PrivateUsage;
}

[StructLayout(LayoutKind.Sequential)]
public struct IOC {
    public ulong ReadOperationCount;
    public ulong WriteOperationCount;
    public ulong OtherOperationCount;
    public ulong ReadTransferCount;
    public ulong WriteTransferCount;
    public ulong OtherTransferCount;
}

[DllImport("psapi.dll", SetLastError=true)]
public static extern bool GetProcessMemoryInfo(IntPtr h, out PMC c, uint cb);

[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool GetProcessIoCounters(IntPtr h, out IOC c);

public static PMC Mem(IntPtr h) {
    PMC c = new PMC();
    c.cb = (uint)Marshal.SizeOf(typeof(PMC));
    GetProcessMemoryInfo(h, out c, c.cb);
    return c;
}

public static IOC Io(IntPtr h) {
    IOC c = new IOC();
    GetProcessIoCounters(h, out c);
    return c;
}
'@
}

function Measure-Proc {
    param(
        [string]$Exe,
        [string[]]$Argv = @(),
        [string]$WorkDir = $null,
        [switch]$Capture
    )

    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $Exe
    if ($Argv -and $Argv.Count -gt 0) {
        $q = $Argv | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }
        $psi.Arguments = ($q -join " ")
    }
    if ($WorkDir) { $psi.WorkingDirectory = $WorkDir }
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $p = [Diagnostics.Process]::Start($psi)
    $h = $p.Handle

    $peakWs = 0L; $peakPriv = 0L; $peakThreads = 0; $peakHandles = 0
    $so = $p.StandardOutput.ReadToEndAsync()
    $se = $p.StandardError.ReadToEndAsync()

    while (-not $p.HasExited) {
        try {
            $p.Refresh()
            if ($p.PeakWorkingSet64 -gt $peakWs) { $peakWs = $p.PeakWorkingSet64 }
            if ($p.PeakPagedMemorySize64 -gt $peakPriv) { $peakPriv = $p.PeakPagedMemorySize64 }
            if ($p.Threads.Count -gt $peakThreads) { $peakThreads = $p.Threads.Count }
            if ($p.HandleCount -gt $peakHandles) { $peakHandles = $p.HandleCount }
        } catch {}
        Start-Sleep -Milliseconds 1
    }
    $p.WaitForExit()
    $sw.Stop()

    $stdout = $so.Result
    $stderr = $se.Result

    $mem = [Compyler.Probe]::Mem($h)
    $io = [Compyler.Probe]::Io($h)

    $userMs = 0.0; $kernMs = 0.0; $totMs = 0.0
    try {
        $userMs = $p.UserProcessorTime.TotalMilliseconds
        $kernMs = $p.PrivilegedProcessorTime.TotalMilliseconds
        $totMs = $p.TotalProcessorTime.TotalMilliseconds
    } catch {}

    $pk = [int64]$mem.PeakWorkingSetSize
    if ($pk -gt $peakWs) { $peakWs = $pk }
    $pp = [int64]$mem.PeakPagefileUsage
    if ($pp -gt $peakPriv) { $peakPriv = $pp }

    $r = [pscustomobject]@{
        Exit       = $p.ExitCode
        WallMs     = [math]::Round($sw.Elapsed.TotalMilliseconds, 1)
        CpuMs      = [math]::Round($totMs, 1)
        UserMs     = [math]::Round($userMs, 1)
        KernelMs   = [math]::Round($kernMs, 1)
        PeakRamMB  = [math]::Round($peakWs / 1MB, 2)
        PeakPrivMB = [math]::Round($peakPriv / 1MB, 2)
        PageFaults = [int64]$mem.PageFaultCount
        ReadMB     = [math]::Round($io.ReadTransferCount / 1MB, 2)
        WriteMB    = [math]::Round($io.WriteTransferCount / 1MB, 2)
        ReadOps    = [int64]$io.ReadOperationCount
        WriteOps   = [int64]$io.WriteOperationCount
        OtherOps   = [int64]$io.OtherOperationCount
        Threads    = $peakThreads
        Handles    = $peakHandles
    }
    if ($Capture) {
        Add-Member -InputObject $r -NotePropertyName Stdout -NotePropertyValue $stdout
        Add-Member -InputObject $r -NotePropertyName Stderr -NotePropertyValue $stderr
    }
    $p.Dispose()
    return $r
}

function Measure-Best {
    param([string]$Exe, [string[]]$Argv = @(), [int]$Runs = 3, [string]$WorkDir = $null)
    $best = $null
    for ($i = 0; $i -lt $Runs; $i++) {
        $m = Measure-Proc -Exe $Exe -Argv $Argv -WorkDir $WorkDir
        if ($null -eq $best -or $m.WallMs -lt $best.WallMs) { $best = $m }
    }
    return $best
}

if ($MyInvocation.InvocationName -ne '.') {
    $root = Split-Path -Parent $PSScriptRoot
    $py = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe"
    $exe = Join-Path $root "tests\out\mem.exe"
    if (-not (Test-Path $exe)) {
        & (Join-Path $root "bin\compyler.exe") (Join-Path $root "tests\t1\main.py") -o $exe 2>&1 | Out-Null
    }
    & $exe 2>&1 | Out-Null
    $a = Measure-Best $exe @() 3
    $b = Measure-Best $py @((Join-Path $root "tests\t1\main.py")) 3
    $fmt = "{0,-22} {1,8} {2,8} {3,8} {4,9} {5,10} {6,8} {7,8}"
    $fmt -f "", "wallMs", "cpuMs", "userMs", "peakRamMB", "pageFaults", "readMB", "handles"
    $fmt -f "compyler exe (warm)", $a.WallMs, $a.CpuMs, $a.UserMs, $a.PeakRamMB, $a.PageFaults, $a.ReadMB, $a.Handles
    $fmt -f "python main.py", $b.WallMs, $b.CpuMs, $b.UserMs, $b.PeakRamMB, $b.PageFaults, $b.ReadMB, $b.Handles
}
