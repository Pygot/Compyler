$py = "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe"
$root = "C:\Users\Office\Desktop\Compyler\Compyler"
$compyler = Join-Path $root "bin\compyler.exe"
$w = Join-Path $root "tests\out\startup"
Remove-Item -Recurse -Force $w -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $w | Out-Null
Set-Content -Encoding utf8 (Join-Path $w "hello.py") 'print("hello from compyler")'

function MB($p) { [math]::Round((Get-Item $p).Length / 1MB, 2) }

function Runs($exe, $n) {
    $best = [double]::MaxValue
    for ($i = 0; $i -lt $n; $i++) {
        $t = Measure-Command { & $exe | Out-Null }
        if ($t.TotalMilliseconds -lt $best) { $best = $t.TotalMilliseconds }
    }
    return [math]::Round($best, 1)
}

$hello = Join-Path $w "hello.py"

$variants = [ordered]@{
    "onefile fast"  = @("--compress", "fast")
    "onefile max"   = @()
    "onefile upx"   = @("--upx")
    "onefile none"  = @("--compress", "none")
    "prune-lazy"    = @("--prune-lazy")
    "onedir"        = @("--onedir")
}

$rows = @()
$idx = 0
foreach ($k in $variants.Keys) {
    $exe = Join-Path $w ("h" + $idx + ".exe")
    $bt = Measure-Command { & $compyler $hello -o $exe --name ("hello" + $idx) @($variants[$k]) 2>&1 | Out-Null }
    $cold = [double]::MaxValue
    for ($c = 0; $c -lt 3; $c++) {
        Remove-Item -Recurse -Force "$env:LOCALAPPDATA\Compyler\hello$idx-*" -ErrorAction SilentlyContinue
        $v = Runs $exe 1
        if ($v -lt $cold) { $cold = $v }
    }
    $warm = Runs $exe 6
    $rows += "{0,-14} build {1,5:N1}s size {2,6:N2}MB cold {3,7:N1}ms warm {4,6:N1}ms" -f $k, $bt.TotalSeconds, (MB $exe), $cold, $warm
    $idx++
}

& $py -m PyInstaller --version 2>&1 | Out-Null
$pd = Join-Path $w "pyi"
$bt = Measure-Command { & $py -m PyInstaller --onefile --distpath $pd --workpath (Join-Path $w "pyib") --specpath $w -y $hello 2>&1 | Out-Null }
$pexe = Join-Path $pd "hello.exe"
if (Test-Path $pexe) {
    $cold = [double]::MaxValue
    for ($c = 0; $c -lt 3; $c++) { $v = Runs $pexe 1; if ($v -lt $cold) { $cold = $v } }
    $warm = Runs $pexe 6
    $rows += "{0,-14} build {1,5:N1}s size {2,6:N2}MB cold {3,7:N1}ms warm {4,6:N1}ms" -f "pyinstaller", $bt.TotalSeconds, (MB $pexe), $cold, $warm
} else {
    $rows += "pyinstaller build failed"
}

$best = [double]::MaxValue
for ($i = 0; $i -lt 6; $i++) {
    $t = Measure-Command { & $py $hello | Out-Null }
    if ($t.TotalMilliseconds -lt $best) { $best = $t.TotalMilliseconds }
}
$rows += "{0,-14} warm {1,6:N1}ms" -f "python .py", [math]::Round($best, 1)
$rows | ForEach-Object { $_ }
