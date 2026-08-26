param([string]$Python = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe")

$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$compyler = Join-Path $root "bin\compyler.exe"
$out = Join-Path $root "tests\out"
New-Item -ItemType Directory -Force $out | Out-Null

$script:pass = 0
$script:fail = 0

function Ok($name) { Write-Host ("  PASS  " + $name); $script:pass++ }
function No($name, $detail) {
    Write-Host ("  FAIL  " + $name) -ForegroundColor Red
    if ($detail) { Write-Host ("        " + $detail) -ForegroundColor DarkRed }
    $script:fail++
}

function DiffRun($name, $py, $src, $exe, $extraArgs) {
    $want = (& $py $src @extraArgs 2>&1 | Out-String)
    $got = (& $exe @extraArgs 2>&1 | Out-String)
    if ($want -eq $got) { Ok $name; return }
    $w = $want -split "`n"; $g = $got -split "`n"; $d = ""
    for ($i = 0; $i -lt [Math]::Max($w.Count, $g.Count); $i++) {
        if ($w[$i] -ne $g[$i]) { $d = "line $($i+1): py=[$($w[$i])] exe=[$($g[$i])]"; break }
    }
    No $name $d
}

Write-Host ""
Write-Host "stress A: uv venv, numpy pipeline, default flags"

$venvA = Join-Path $out "venvA"
$pyA = Join-Path $venvA "Scripts\python.exe"
if (-not (Test-Path $pyA)) {
    uv venv $venvA --python $Python 2>&1 | Out-Null
    uv pip install --python $pyA numpy jinja2 requests 2>&1 | Select-Object -Last 1
}
$srcA = Join-Path $root "tests\stressA\app.py"

$exeA = Join-Path $out "stressA.exe"
$oldPath = $env:PATH
$env:PATH = (Join-Path $venvA "Scripts") + ";" + $env:PATH
$logA = & $compyler $srcA -o $exeA --add-data "$root\tests\stressA\assets;assets" 2>&1 | Out-String
$env:PATH = $oldPath
if (-not (Test-Path $exeA)) { No "stressA build (activated venv, defaults)" ($logA -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressA build (activated venv, defaults)"
    DiffRun "stressA output" $pyA $srcA $exeA @()
    DiffRun "stressA output --net" $pyA $srcA $exeA @("--net")
}

$exeA2 = Join-Path $out "stressA_dir.exe"
$logA2 = & $compyler $srcA -o $exeA2 --onedir --python $pyA --add-data "$root\tests\stressA\assets;assets" 2>&1 | Out-String
if (-not (Test-Path $exeA2)) { No "stressA --onedir build" ($logA2 -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressA --onedir build"
    DiffRun "stressA --onedir output" $pyA $srcA $exeA2 @()
}

$exeA3 = Join-Path $out "stressA_lazy.exe"
$logA3 = & $compyler $srcA -o $exeA3 --prune-lazy --hidden-import requests --python $pyA --add-data "$root\tests\stressA\assets;assets" 2>&1 | Out-String
if (-not (Test-Path $exeA3)) { No "stressA --prune-lazy build" ($logA3 -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressA --prune-lazy build"
    DiffRun "stressA --prune-lazy --net" $pyA $srcA $exeA3 @("--net")
}

$szFull = (Get-Item $exeA).Length / 1MB
$szLazy = (Get-Item $exeA3).Length / 1MB
Write-Host ("        sizes: default {0:N2} MB, prune-lazy {1:N2} MB" -f $szFull, $szLazy)

Write-Host ""
Write-Host "stress B: python -m venv, PIL and jinja2 report app, default flags"

$venvB = Join-Path $out "venvB"
$pyB = Join-Path $venvB "Scripts\python.exe"
if (-not (Test-Path $pyB)) {
    & $Python -m venv $venvB 2>&1 | Out-Null
    & $pyB -m pip install -q pillow jinja2 2>&1 | Select-Object -Last 1
}
$srcB = Join-Path $root "tests\stressB\main.py"

$exeB = Join-Path $out "stressB.exe"
$logB = & $compyler $srcB -o $exeB --python $pyB 2>&1 | Out-String
if (-not (Test-Path $exeB)) { No "stressB build (--python venv, defaults)" ($logB -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressB build (--python venv, defaults)"
    DiffRun "stressB output" $pyB $srcB $exeB @()
}

$exeB2 = Join-Path $out "stressB_nonative.exe"
$logB2 = & $compyler $srcB -o $exeB2 --no-native --python $pyB 2>&1 | Out-String
if (-not (Test-Path $exeB2)) { No "stressB --no-native build" ($logB2 -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressB --no-native build"
    DiffRun "stressB --no-native output" $pyB $srcB $exeB2 @()
}

$exeB3 = Join-Path $out "stressB_noprune.exe"
$logB3 = & $compyler $srcB -o $exeB3 --no-prune --python $pyB 2>&1 | Out-String
if (-not (Test-Path $exeB3)) { No "stressB --no-prune build" ($logB3 -split "`n" | Select-Object -Last 2) }
else {
    Ok "stressB --no-prune build"
    DiffRun "stressB --no-prune output" $pyB $srcB $exeB3 @()
    $a = (Get-Item $exeB).Length
    $b = (Get-Item $exeB3).Length
    if ($a -le $b) { Ok ("default prune not larger ({0:N2} MB vs {1:N2} MB)" -f ($a / 1MB), ($b / 1MB)) }
    else { No "default prune not larger" ("pruned {0} > full {1}" -f $a, $b) }
}

Write-Host ""
Write-Host ("STRESS PASSED {0}   FAILED {1}" -f $script:pass, $script:fail)
if ($script:fail -gt 0) { exit 1 }
exit 0
