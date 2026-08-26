$py = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe"
$root = "C:\Users\Office\Desktop\Compyler\Compyler"
Set-Location $root

$runners = [ordered]@{
    CPython  = @{ cmd = { & $py "$root\tests\bench\bench.py" } }
    Nuitka   = @{ cmd = { & "$root\build\nuitka\bench_nuitka.dist\bench_nuitka.exe" } }
    Compyler = @{ cmd = { & "$root\tests\out\bench_cpy.exe" } }
    Wrap     = @{ cmd = { & "$root\tests\out\bench_wrap.exe" } }
    Codon    = @{ cmd = { wsl -e bash -c "~/bench_codon" } }
    Go       = @{ cmd = { & "$root\tests\bench\bench_go.exe" } }
    CppWin   = @{ cmd = { & "$root\tests\bench\bench_cpp.exe" } }
    CppWsl   = @{ cmd = { wsl -e bash -c "~/bench_cpp_wsl" } }
}

$res = @{}
$vals = @{}
$order = @()

foreach ($pass in 1..3) {
    foreach ($k in $runners.Keys) {
        $outp = (& $runners[$k].cmd 2>&1 | Out-String)
        foreach ($ln in ($outp -split "`n")) {
            if ($ln -match '^(\S+)\s+([\d.]+)\s+(\S+)') {
                $n = $Matches[1]; $v = [double]$Matches[2]; $r = $Matches[3]
                if ($n -eq "TOTAL" -or $n -eq "sink") { continue }
                if ($pass -eq 1 -and $k -eq "CPython" -and $order -notcontains $n) { $order += $n }
                $key = "$k|$n"
                if (-not $res.ContainsKey($key) -or $res[$key] -gt $v) { $res[$key] = $v }
                $vals[$key] = $r
            }
        }
    }
}

$hdr = "{0,-13}" -f "case"
foreach ($k in $runners.Keys) { $hdr += "{0,10}" -f $k }
Write-Host $hdr
Write-Host ("-" * $hdr.Length)
$tot = @{}
foreach ($k in $runners.Keys) { $tot[$k] = 0.0 }
foreach ($n in $order) {
    $line = "{0,-13}" -f $n
    foreach ($k in $runners.Keys) {
        $v = $res["$k|$n"]
        if ($null -eq $v) { $line += "{0,10}" -f "-" }
        else { $tot[$k] += $v; $line += "{0,10:N2}" -f $v }
    }
    Write-Host $line
}
Write-Host ("-" * $hdr.Length)
$line = "{0,-13}" -f "TOTAL"
foreach ($k in $runners.Keys) { $line += "{0,10:N2}" -f $tot[$k] }
Write-Host $line

Write-Host ""
Write-Host "result mismatches vs CPython (integer results only):"
$mm = 0
foreach ($n in $order) {
    $ref = $vals["CPython|$n"]
    if ($null -eq $ref -or $ref -match '\.') { continue }
    foreach ($k in $runners.Keys) {
        $v = $vals["$k|$n"]
        if ($null -ne $v -and $v -ne $ref) { Write-Host "  $k $n : $v vs $ref"; $mm++ }
    }
}
if ($mm -eq 0) { Write-Host "  none" }
