$root = Split-Path -Parent $PSScriptRoot
$compyler = Join-Path $root "bin\compyler.exe"
$out = Join-Path $root "tests\out"
$vers = [ordered]@{
    "3.11" = "C:\Users\Office\AppData\Roaming\uv\python\cpython-3.11.16-windows-x86_64-none\python.exe"
    "3.12" = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe"
    "3.13" = "C:\Users\Office\AppData\Roaming\uv\python\cpython-3.13.15-windows-x86_64-none\python.exe"
}
$corpora = @("tests\suite\semantics.py","tests\suite\bits.py","tests\suite\mathfns.py",
             "tests\suite\dictslice.py","tests\suite\builtins1.py","tests\samples\fstr.py")
if (Test-Path (Join-Path $root "tests\suite\arrays.py")) { $corpora += "tests\suite\arrays.py" }
if (Test-Path (Join-Path $root "tests\suite\strdict.py")) { $corpora += "tests\suite\strdict.py" }
if (Test-Path (Join-Path $root "tests\suite\bounds.py")) { $corpora += "tests\suite\bounds.py" }
foreach ($v in $vers.Keys) {
    $py = $vers[$v]
    if (-not (Test-Path $py)) { continue }
    $fail = @(); $nat = 0
    foreach ($t in $corpora) {
        $src = Join-Path $root $t
        $n = [IO.Path]::GetFileNameWithoutExtension($t)
        $exe = Join-Path $out ("c_" + ($v -replace '\.', '') + "_" + $n + ".exe")
        Remove-Item $exe -Force -ErrorAction SilentlyContinue
        & $py $src > "$exe.ref" 2>&1
        $log = & $compyler $src "-o" $exe "--python" $py 2>&1 | Out-String
        if ($log -match "(\d+) function\(s\) native") { $nat += [int]$Matches[1] }
        if (-not (Test-Path $exe)) { $fail += "$n(build)"; continue }
        $okrun = $false
        for ($try = 0; $try -lt 5; $try++) {
            & $exe > "$exe.got" 2>&1
            $gt = Get-Content "$exe.got" -Raw -ErrorAction SilentlyContinue
            if ($null -ne $gt -and $gt -notmatch "cannot open own image") { $okrun = $true; break }
            Start-Sleep -Milliseconds 500
        }
        if (-not $okrun) { $fail += "$n(locked)"; continue }
        if ((Get-FileHash "$exe.got").Hash -ne (Get-FileHash "$exe.ref").Hash) { $fail += $n }
    }
    "{0}  failures={1} {2}  native={3}" -f $v, $fail.Count, ($fail -join ","), $nat
}
