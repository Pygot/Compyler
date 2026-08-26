$root = Split-Path -Parent $PSScriptRoot
$py = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe"
$runners = [ordered]@{
    CPython  = @{ exe = $py; arg = (Join-Path $root "tests\bench\bench.py") }
    Nuitka   = @{ exe = (Join-Path $root "build\nuitka\bench_nuitka.dist\bench_nuitka.exe"); arg = "" }
    Compyler = @{ exe = (Join-Path $root "tests\bench\bench.exe"); arg = "" }
    Go       = @{ exe = (Join-Path $root "tests\bench\bench_go.exe"); arg = "" }
    Cpp      = @{ exe = (Join-Path $root "tests\bench\bench_cpp.exe"); arg = "" }
}
$res = @{}
foreach ($pass in 1..2) {
    foreach ($k in $runners.Keys) {
        $r = $runners[$k]
        if (-not (Test-Path $r.exe)) { continue }
        $psi = New-Object Diagnostics.ProcessStartInfo
        $psi.FileName = $r.exe
        if ($r.arg) { $psi.Arguments = '"' + $r.arg + '"' }
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $p = [Diagnostics.Process]::Start($psi)
        $out = $p.StandardOutput.ReadToEnd()
        $p.WaitForExit()
        foreach ($ln in ($out -split "`n")) {
            if ($ln -match '^(\S[\S ]*?)\s{2,}([\d.]+)\s') {
                $n = $Matches[1].Trim(); $v = [double]$Matches[2]
                $key = "$k|$n"
                if (-not $res.ContainsKey($key) -or $res[$key] -gt $v) { $res[$key] = $v }
            }
        }
    }
}
$names = @("fib(27)", "forsum(3M)", "intloop(3M)", "collatz(30k)", "primes(60k)",
           "mandel(200x150)", "listsum(20k*40)")
"{0,-17} {1,9} {2,9} {3,10} {4,8} {5,8}" -f "case", "CPython", "Nuitka", "Compyler", "Go", "C++"
$t = @{}
foreach ($k in $runners.Keys) { $t[$k] = 0.0 }
foreach ($n in $names) {
    foreach ($k in $runners.Keys) { $t[$k] += [double]$res["$k|$n"] }
    "{0,-17} {1,9:N1} {2,9:N1} {3,10:N1} {4,8:N1} {5,8:N1}" -f $n, $res["CPython|$n"], $res["Nuitka|$n"], $res["Compyler|$n"], $res["Go|$n"], $res["Cpp|$n"]
}
"{0,-17} {1,9:N1} {2,9:N1} {3,10:N1} {4,8:N1} {5,8:N1}" -f "TOTAL", $t["CPython"], $t["Nuitka"], $t["Compyler"], $t["Go"], $t["Cpp"]
""
"vs CPython : {0,5:N1}x faster" -f ($t["CPython"] / $t["Compyler"])
"vs Nuitka  : {0,5:N1}x faster" -f ($t["Nuitka"] / $t["Compyler"])
if ($t["Go"] -gt 0)  { "vs Go      : {0,5:N1}x slower" -f ($t["Compyler"] / $t["Go"]) }
if ($t["Cpp"] -gt 0) { "vs C++     : {0,5:N1}x slower" -f ($t["Compyler"] / $t["Cpp"]) }
