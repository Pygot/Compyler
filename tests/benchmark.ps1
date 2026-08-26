param(
    [string]$Python = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot "measure.ps1")

$compyler = Join-Path $root "bin\compyler.exe"
$out = Join-Path $root "tests\out"
$bench = Join-Path $root "tests\bench"
New-Item -ItemType Directory -Force $out | Out-Null

function Rule($t) {
    Write-Host ""
    Write-Host $t
}

function Sub($t) { Write-Host ""; Write-Host ("  " + $t) }

function ExeMB($p) {
    if (Test-Path $p) { return [math]::Round((Get-Item $p).Length / 1MB, 2) }
    return 0
}

$benchPy = Join-Path $bench "bench.py"
$benchExe = Join-Path $out "bench_cpy.exe"
$benchWrap = Join-Path $out "bench_wrap.exe"
$benchGo = Join-Path $bench "bench_go.exe"
$benchCpp = Join-Path $bench "bench_cpp.exe"
$benchNui = Join-Path $root "build\nuitka\bench_nuitka.dist\bench_nuitka.exe"

if (-not $SkipBuild) {
    Sub "building"
    $t0 = [Diagnostics.Stopwatch]::StartNew()
    & $compyler $benchPy -o $benchExe 2>&1 | Select-Object -Last 1
    $t0.Stop()
    Write-Host ("   compyler build: {0:N2} s, {1} MB" -f ($t0.Elapsed.TotalSeconds), (ExeMB $benchExe))
    & $compyler $benchPy -o $benchWrap --wrap-int 2>&1 | Select-Object -Last 1
}

Rule "1. COMPUTE BENCHMARKS (20 workloads, best of 3 in-process, 2 interleaved passes)"

$runners = [ordered]@{
    CPython  = @{ exe = $Python;   arg = $benchPy }
    Nuitka   = @{ exe = $benchNui; arg = "" }
    Compyler = @{ exe = $benchExe; arg = "" }
    Wrap     = @{ exe = $benchWrap; arg = "" }
    Go       = @{ exe = $benchGo;  arg = "" }
    Cpp      = @{ exe = $benchCpp; arg = "" }
}

$res = @{}
$order = @()
$avail = @()
foreach ($k in $runners.Keys) { if (Test-Path $runners[$k].exe) { $avail += $k } }

foreach ($pass in 1..2) {
    foreach ($k in $avail) {
        $r = $runners[$k]
        $m = Measure-Proc -Exe $r.exe -Argv @($r.arg | Where-Object { $_ }) -Capture
        foreach ($ln in ($m.Stdout -split "`n")) {
            if ($ln -match '^(\S+)\s+([\d.]+)\s') {
                $n = $Matches[1]; $v = [double]$Matches[2]
                if ($n -eq "TOTAL") { continue }
                if ($pass -eq 1 -and $k -eq $avail[0] -and $order -notcontains $n) { $order += $n }
                $key = "$k|$n"
                if (-not $res.ContainsKey($key) -or $res[$key] -gt $v) { $res[$key] = $v }
            }
        }
    }
}

$hdr = "{0,-13}" -f "case"
foreach ($k in $avail) { $hdr += "{0,10}" -f $k }
$hdr += "{0,12}" -f "vs CPython"
Write-Host $hdr
Write-Host ("-" * $hdr.Length)

$tot = @{}
$fair = @{}
foreach ($k in $avail) { $tot[$k] = 0.0; $fair[$k] = 0.0 }
$dropped = @()

foreach ($n in $order) {
    $usable = $true
    foreach ($k in $avail) {
        $v = $res["$k|$n"]
        if ($null -eq $v -or $v -le 0.0) { $usable = $false }
    }
    if (-not $usable) { $dropped += $n }

    $line = "{0,-13}" -f $n
    foreach ($k in $avail) {
        $v = $res["$k|$n"]
        if ($null -eq $v) { $line += "{0,10}" -f "-" }
        elseif ($v -le 0.0) { $line += "{0,10}" -f "opt" }
        else {
            $tot[$k] += $v
            if ($usable) { $fair[$k] += $v }
            $line += "{0,10:N2}" -f $v
        }
    }
    $c = $res["Compyler|$n"]; $q = $res["CPython|$n"]
    if ($c -and $q -and $c -gt 0) { $line += "{0,11:N1}x" -f ($q / $c) } else { $line += "{0,12}" -f "-" }
    Write-Host $line
}

Write-Host ("-" * $hdr.Length)
$line = "{0,-13}" -f "TOTAL"
foreach ($k in $avail) { $line += "{0,10:N2}" -f $tot[$k] }
if ($tot["Compyler"] -gt 0) { $line += "{0,11:N1}x" -f ($tot["CPython"] / $tot["Compyler"]) }
Write-Host $line

if ($dropped.Count -gt 0) {
    Write-Host ""
    Write-Host ("  opt = the compiler eliminated the workload entirely (reported 0.0000 ms).")
    Write-Host ("  affected: " + ($dropped -join ", "))
    Write-Host ("  the ratios below use only the {0} cases every runner actually executed." -f ($order.Count - $dropped.Count))
}

Sub "ratios on the comparable subset"
if ($fair["Compyler"] -gt 0) {
    "  vs CPython : {0,6:N2}x faster" -f ($fair["CPython"] / $fair["Compyler"])
    if ($fair["Nuitka"] -gt 0) { "  vs Nuitka  : {0,6:N2}x faster" -f ($fair["Nuitka"] / $fair["Compyler"]) }
    if ($fair["Go"] -gt 0) {
        $rg = $fair["Compyler"] / $fair["Go"]
        if ($rg -le 1.0) { "  vs Go      : {0,6:N2}x faster" -f (1.0 / $rg) }
        else             { "  vs Go      : {0,6:N2}x slower" -f $rg }
    }
    if ($fair["Cpp"] -gt 0) {
        $rc = $fair["Compyler"] / $fair["Cpp"]
        if ($rc -le 1.0) { "  vs C++     : {0,6:N2}x faster" -f (1.0 / $rc) }
        else             { "  vs C++     : {0,6:N2}x slower" -f $rc }
    }
}

$wins = 0; $near = 0
foreach ($n in $order) {
    $c = $res["Compyler|$n"]; $g = $res["Cpp|$n"]
    if ($c -and $g -and $g -gt 0) {
        if ($c -le $g) { $wins++ }
        elseif ($c -le $g * 1.5) { $near++ }
    }
}
"  cases at or beating C++      : {0} of {1}" -f $wins, $order.Count
"  cases within 1.5x of C++     : {0} of {1}" -f ($wins + $near), $order.Count

Rule "2. RESOURCE USAGE ON THE SAME WORKLOAD (whole process, best of 3)"

$fmt = "{0,-10} {1,9} {2,9} {3,9} {4,9} {5,10} {6,11} {7,8} {8,8} {9,8}"
Write-Host ($fmt -f "runner", "wallMs", "cpuMs", "userMs", "kernMs", "peakRamMB", "peakPrivMB", "faults", "threads", "handles")
Write-Host ("-" * 105)
foreach ($k in $avail) {
    $r = $runners[$k]
    $m = Measure-Best -Exe $r.exe -Argv @($r.arg | Where-Object { $_ }) -Runs 3
    Write-Host ($fmt -f $k, $m.WallMs, $m.CpuMs, $m.UserMs, $m.KernelMs, $m.PeakRamMB, $m.PeakPrivMB, $m.PageFaults, $m.Threads, $m.Handles)
}

Sub "disk io on the same workload"
$fmt2 = "{0,-10} {1,10} {2,10} {3,10} {4,10} {5,10} {6,10}"
Write-Host ($fmt2 -f "runner", "readMB", "writeMB", "readOps", "writeOps", "otherOps", "sizeMB")
Write-Host ("-" * 74)
foreach ($k in $avail) {
    $r = $runners[$k]
    $m = Measure-Proc -Exe $r.exe -Argv @($r.arg | Where-Object { $_ })
    $sz = if ($k -eq "CPython") { ExeMB $benchPy } else { ExeMB $r.exe }
    Write-Host ($fmt2 -f $k, $m.ReadMB, $m.WriteMB, $m.ReadOps, $m.WriteOps, $m.OtherOps, $sz)
}

Rule "3. STARTUP AND FOOTPRINT (hello world)"

$hello = Join-Path $out "hello_bench.py"
Set-Content -Encoding utf8 $hello 'print("hello")'

$variants = [ordered]@{
    "onefile max"  = @("--compress", "max")
    "onefile fast" = @("--compress", "fast")
    "onefile none" = @("--compress", "none")
    "onefile upx"  = @("--compress", "max", "--upx")
    "onedir"       = @("--onedir")
}

$fmt3 = "{0,-14} {1,9} {2,9} {3,9} {4,9} {5,10} {6,9}"
Write-Host ($fmt3 -f "variant", "buildS", "sizeMB", "coldMs", "warmMs", "peakRamMB", "cpuMs")
Write-Host ("-" * 74)
foreach ($v in $variants.Keys) {
    $exe = Join-Path $out ("hello_" + ($v -replace '\s', '_') + ".exe")
    Remove-Item $exe -ErrorAction SilentlyContinue
    Get-ChildItem "$env:LOCALAPPDATA\Compyler" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "hello_*" } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $a = @($hello, "-o", $exe) + $variants[$v]
    & $compyler @a 2>&1 | Out-Null
    $sw.Stop()
    if (-not (Test-Path $exe)) { Write-Host ($fmt3 -f $v, "-", "-", "-", "-", "-", "-"); continue }
    $cold = Measure-Proc -Exe $exe
    $warm = Measure-Best -Exe $exe -Runs 5
    Write-Host ($fmt3 -f $v, ([math]::Round($sw.Elapsed.TotalSeconds, 2)), (ExeMB $exe), $cold.WallMs, $warm.WallMs, $warm.PeakRamMB, $warm.CpuMs)
}

$b = Measure-Best -Exe $Python -Argv @($hello) -Runs 5
Write-Host ($fmt3 -f "python .py", "-", (ExeMB $hello), $b.WallMs, $b.WallMs, $b.PeakRamMB, $b.CpuMs)

Rule "4. APPLICATION WORKLOADS (compiled exe vs interpreter)"

$apps = [ordered]@{
    "c extensions"  = "$root\tests\samples\cext.py"
    "tkinter gui"   = "$root\tests\samples\gui_tk.py"
    "f-strings"     = "$root\tests\samples\fstr.py"
    "semantics"     = "$root\tests\suite\semantics.py"
    "bit ops"       = "$root\tests\suite\bits.py"
    "math fns"      = "$root\tests\suite\mathfns.py"
}

$fmt4 = "{0,-14} {1,-9} {2,9} {3,9} {4,10} {5,10} {6,9} {7,8}"
Write-Host ($fmt4 -f "app", "runner", "wallMs", "cpuMs", "peakRamMB", "faults", "readMB", "sizeMB")
Write-Host ("-" * 84)
foreach ($a in $apps.Keys) {
    $src = $apps[$a]
    if (-not (Test-Path $src)) { continue }
    $exe = Join-Path $out (($a -replace '\s', '_') + "_app.exe")
    if (-not (Test-Path $exe)) { & $compyler $src -o $exe 2>&1 | Out-Null }
    if (-not (Test-Path $exe)) { continue }
    & $exe 2>&1 | Out-Null
    $mc = Measure-Best -Exe $exe -Runs 3
    $mp = Measure-Best -Exe $Python -Argv @($src) -Runs 3
    Write-Host ($fmt4 -f $a, "compyler", $mc.WallMs, $mc.CpuMs, $mc.PeakRamMB, $mc.PageFaults, $mc.ReadMB, (ExeMB $exe))
    Write-Host ($fmt4 -f "", "python", $mp.WallMs, $mp.CpuMs, $mp.PeakRamMB, $mp.PageFaults, $mp.ReadMB, "-")
}

Rule "5. BUILD AND DISK FOOTPRINT"

$cache = (Get-ChildItem "$env:LOCALAPPDATA\Compyler" -Recurse -File -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
$cdirs = @(Get-ChildItem "$env:LOCALAPPDATA\Compyler" -Directory -ErrorAction SilentlyContinue).Count
$tmp = @(Get-ChildItem "$env:TEMP\compyler-*" -Directory -ErrorAction SilentlyContinue).Count
"  extraction cache : {0:N2} MB in {1} app dirs" -f ($cache / 1MB), $cdirs
"  leaked temp dirs : {0}" -f $tmp
"  compiler binary  : {0:N2} MB" -f (ExeMB $compyler)
"  stub binary      : {0:N2} MB" -f (ExeMB (Join-Path $root "bin\stub.exe"))
Write-Host ""
