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

function Build($src, $exe, $extra) {
    $a = @($src, "-o", $exe) + $extra
    $log = & $compyler @a 2>&1 | Out-String
    if (-not (Test-Path $exe)) { return $log }
    return $null
}

function Diff-Against-Python($name, $src, $extra) {
    $exe = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', '')
    $exe = $exe + "_" + ($name -replace '[^A-Za-z0-9]', '') + ".exe"
    $err = Build $src $exe $extra
    if ($err) { No $name ("build failed: " + ($err -split "`n" | Select-Object -Last 2)); return }
    $want = (& $Python $src 2>&1 | Out-String)
    $got = (& $exe 2>&1 | Out-String)
    if ($want -eq $got) { Ok $name }
    else {
        $w = $want -split "`n"; $g = $got -split "`n"
        $d = ""
        for ($i = 0; $i -lt [Math]::Max($w.Count, $g.Count); $i++) {
            if ($w[$i] -ne $g[$i]) { $d = "line $($i+1): py=[$($w[$i])] exe=[$($g[$i])]"; break }
        }
        No $name $d
    }
}

Write-Host ""
Write-Host "differential semantics (native codegen vs interpreter)"
Diff-Against-Python "semantics native"    "$root\tests\suite\semantics.py" @()
Diff-Against-Python "semantics bytecode"  "$root\tests\suite\semantics.py" @("--no-native")
Diff-Against-Python "semantics -O"        "$root\tests\suite\semantics.py" @("-O")
Diff-Against-Python "mandelbrot"          "$root\tests\mb\d.py" @()
Diff-Against-Python "nested loops"        "$root\tests\mb\n.py" @()
Diff-Against-Python "while/break"         "$root\tests\mb\b.py" @()
Diff-Against-Python "float paths"         "$root\tests\mb\a.py" @()
Diff-Against-Python "tuple build"         "$root\tests\mb\t2.py" @()
Diff-Against-Python "smoke"               "$root\tests\nat\t.py" @()
Diff-Against-Python "bit ops and shifts"  "$root\tests\suite\bits.py" @()
Diff-Against-Python "bit ops bytecode"    "$root\tests\suite\bits.py" @("--no-native")
Diff-Against-Python "math intrinsics"     "$root\tests\suite\mathfns.py" @()
Diff-Against-Python "math bytecode"       "$root\tests\suite\mathfns.py" @("--no-native")
Diff-Against-Python "f-strings"           "$root\tests\samples\fstr.py" @()
Diff-Against-Python "f-strings bytecode"  "$root\tests\samples\fstr.py" @("--no-native")
Diff-Against-Python "dicts and slices"     "$root\tests\suite\dictslice.py" @()
Diff-Against-Python "dicts slices bytecode" "$root\tests\suite\dictslice.py" @("--no-native")
Diff-Against-Python "typed str and dict"   "$root\tests\suite\strdict.py" @()
Diff-Against-Python "str dict bytecode"    "$root\tests\suite\strdict.py" @("--no-native")
Diff-Against-Python "wrap-int mode"        "$root\tests\suite\arrays.py" @("--wrap-int")
Diff-Against-Python "typed arrays"        "$root\tests\suite\arrays.py" @()
Diff-Against-Python "typed arrays bytecode" "$root\tests\suite\arrays.py" @("--no-native")
Diff-Against-Python "bounds elision"      "$root\tests\suite\bounds.py" @()
Diff-Against-Python "bounds bytecode"     "$root\tests\suite\bounds.py" @("--no-native")
Diff-Against-Python "builtin intrinsics"   "$root\tests\suite\builtins1.py" @()
Diff-Against-Python "builtins bytecode"    "$root\tests\suite\builtins1.py" @("--no-native")

Write-Host ""
Write-Host "packaging"
$prog = "$root\tests\t3\prog.exe"
$err = Build "$root\tests\t3\prog.py" $prog @()
if ($err) { No "build prog" $err } else { Ok "build prog" }

$o = (& $prog 2>&1 | Out-String)
if ($o -match "local import : 42") { Ok "local module import" } else { No "local module import" $o }

& $prog --exit 2>&1 | Out-Null
if ($LASTEXITCODE -eq 7) { Ok "sys.exit code" } else { No "sys.exit code" "got $LASTEXITCODE" }

& $prog --raise 2>&1 | Out-Null
if ($LASTEXITCODE -eq 1) { Ok "uncaught exception exit 1" } else { No "uncaught exception exit 1" "got $LASTEXITCODE" }

$job = Start-Job -ArgumentList $prog { param($p) & $p --mp 2>&1 }
$mp = if (Wait-Job $job -Timeout 120) { (Receive-Job $job) -join "`n" } else { Stop-Job $job; "TIMEOUT" }
Remove-Job $job -Force
if ($mp -match "\[0, 1, 4, 9, 16, 25\]") { Ok "multiprocessing spawn" } else { No "multiprocessing spawn" $mp }

$dataexe = "$out\dataapp.exe"
$err = Build "$root\tests\t4\dataapp.py" $dataexe @("--add-data", "$root\tests\t4\assets;assets")
if ($err) { No "--add-data build" $err }
else {
    $o = (& $dataexe 2>&1 | Out-String)
    if ($o -match "'mode': 'packed'") { Ok "--add-data" } else { No "--add-data" $o }
}

Write-Host ""
Write-Host "samples"
$tk = Join-Path $out "gui_tk_sample.exe"
$err = Build "$root\tests\samples\gui_tk.py" $tk @()
if ($err) { No "tkinter build" $err } else {
    $want = (& $Python "$root\tests\samples\gui_tk.py" 2>&1 | Out-String)
    $got = (& $tk 2>&1 | Out-String)
    $want = $want -replace "frozen=False", "frozen=X" -replace "frozen=True", "frozen=X"
    $got = $got -replace "frozen=False", "frozen=X" -replace "frozen=True", "frozen=X"
    if ($got -match "TK_INIT_FAILED") { No "tkinter sample" "Tcl/Tk init failed in bundle" }
    elseif ($want -eq $got) { Ok "tkinter sample" }
    else { No "tkinter sample" "output differs" }
}

$ce = Join-Path $out "cext_sample.exe"
$err = Build "$root\tests\samples\cext.py" $ce @()
if ($err) { No "c extension build" $err } else {
    $want = (& $Python "$root\tests\samples\cext.py" 2>&1 | Out-String)
    $got = (& $ce 2>&1 | Out-String)
    if ($want -eq $got) { Ok "c extension sample (numpy sqlite lxml pil zlib lzma)" }
    else {
        $w = $want -split "`n"; $g = $got -split "`n"; $d = ""
        for ($i = 0; $i -lt [Math]::Max($w.Count, $g.Count); $i++) {
            if ($w[$i] -ne $g[$i]) { $d = "line $($i+1): py=[$($w[$i])] exe=[$($g[$i])]"; break }
        }
        No "c extension sample" $d
    }
}

$df = Join-Path $out "datafiles_app.exe"
$err = Build "$root\tests\samples\datafiles.py" $df @()
if ($err) { No "data files build" $err } else {
    $want = (& $Python "$root\tests\samples\datafiles.py" 2>&1 | Out-String)
    $got = (& $df 2>&1 | Out-String)
    if ($want -eq $got) { Ok "library data files (certifi pygments jinja2 scipy pywin32 cffi crypto)" }
    else {
        $w = $want -split "`n"; $g = $got -split "`n"; $d = ""
        for ($i = 0; $i -lt [Math]::Max($w.Count, $g.Count); $i++) {
            if ($w[$i] -ne $g[$i]) { $d = "line $($i+1): py=[$($w[$i])] exe=[$($g[$i])]"; break }
        }
        No "library data files" $d
    }
}

Write-Host ""
Write-Host "third party packages with C extensions"
$appdir = "$out\app_dir.exe"
$err = Build "$root\tests\t2\app.py" $appdir @("--onedir")
if ($err) { No "numpy+scapy+requests build" $err }
else {
    $o = (& $appdir 2>&1 | Out-String)
    if ($o -match "matmul  : 1134.0" -and $o -match "pkt len : 62" -and $o -match "requests: 2.32.5") {
        Ok "numpy + scapy + requests"
    } else { No "numpy + scapy + requests" $o }
}

Write-Host ""
Write-Host "flags"
foreach ($lv in @("none", "fast", "max")) {
    $e = "$out\cl_$lv.exe"
    $err = Build "$root\tests\t1\main.py" $e @("--compress", $lv)
    if ($err) { No "--compress $lv" $err }
    else {
        $o = (& $e 2>&1 | Out-String)
        $mb = [math]::Round((Get-Item $e).Length / 1MB, 2)
        if ($o -match "hello from compyler") { Ok "--compress $lv ($mb MB)" } else { No "--compress $lv" $o }
    }
}

$e = "$out\onedir_flag.exe"
$err = Build "$root\tests\t1\main.py" $e @("--onedir")
if ($err) { No "--onedir" $err }
else { if ((& $e 2>&1 | Out-String) -match "hello") { Ok "--onedir" } else { No "--onedir" } }

$ico = "C:\Users\Office\AppData\Local\Programs\Python\Python312\DLLs\py.ico"
if (Test-Path $ico) {
    $e = "$out\icon_flag.exe"
    $err = Build "$root\tests\t1\main.py" $e @("--icon", $ico)
    if ($err) { No "--icon" $err }
    else {
        Add-Type -AssemblyName System.Drawing
        $i = [System.Drawing.Icon]::ExtractAssociatedIcon((Resolve-Path $e))
        if ($i.Width -gt 0) { Ok "--icon ($($i.Width)x$($i.Height))" } else { No "--icon" }
    }
}

$e = "$out\win_flag.exe"
$err = Build "$root\tests\t1\main.py" $e @("--windowed")
if ($err) { No "--windowed" $err }
else {
    $b = [IO.File]::ReadAllBytes($e); $pe = [BitConverter]::ToInt32($b, 0x3C)
    $sub = [BitConverter]::ToInt16($b, $pe + 24 + 68)
    if ($sub -eq 2) { Ok "--windowed (gui subsystem)" } else { No "--windowed" "subsystem=$sub" }
}

$e = "$out\run_flag.exe"
$o = (& $compyler "$root\tests\t1\main.py" -o $e --run 2>&1 | Out-String)
if ($o -match "hello from compyler") { Ok "--run" } else { No "--run" $o }

$o = (& $compyler "$root\tests\t1\main.py" -o "$out\upx_flag.exe" --upx 2>&1 | Out-String)
if ((Test-Path "$out\upx_flag.exe") -and ($o -match "upx")) { Ok "--upx (handled)" } else { No "--upx" $o }

$e = "$out\strip_flag.exe"
$err = Build "$root\tests\t1\main.py" $e @("--strip-tests")
if ($err) { No "--strip-tests" $err }
else { if ((& $e 2>&1 | Out-String) -match "hello") { Ok "--strip-tests" } else { No "--strip-tests" } }

Write-Host ""
Write-Host "pruning"

function Check-Prune($label, $src, $normalize) {
    $full = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', '_full.exe')
    $lean = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', '_lean.exe')
    $e1 = Build $src $full @("--no-prune")
    $e2 = Build $src $lean @()
    if ($e1 -or $e2) { No $label "build failed"; return }
    $want = (& $Python $src 2>&1 | Out-String)
    $got = ""
    for ($try = 0; $try -lt 4; $try++) {
        if (-not (Test-Path $lean)) {
            $e2 = Build $src $lean @()
            if ($e2) { No $label "build failed (rebuild after quarantine)"; return }
        }
        $got = (& $lean 2>&1 | Out-String)
        if ($got -notmatch "cannot open own image") { break }
        Start-Sleep -Seconds 2
    }
    if ($normalize) {
        $want = $want -replace "frozen=False", "frozen=X" -replace "frozen=True", "frozen=X"
        $got = $got -replace "frozen=False", "frozen=X" -replace "frozen=True", "frozen=X"
    }
    if ($want -ne $got) {
        $w = $want -split "`n"; $g = $got -split "`n"; $d = ""
        for ($i = 0; $i -lt [Math]::Max($w.Count, $g.Count); $i++) {
            if ($w[$i] -ne $g[$i]) { $d = "line $($i+1): py=[$($w[$i])] exe=[$($g[$i])]"; break }
        }
        No $label $d
        return
    }
    $a = (Get-Item $full).Length
    $b = (Get-Item $lean).Length
    if ($b -gt $a) { No $label "pruned build is larger"; return }
    Ok ("$label ({0:N2} MB -> {1:N2} MB)" -f ($a / 1MB), ($b / 1MB))
}

Check-Prune "prune semantics"     "$root\tests\suite\semantics.py" $false
Check-Prune "prune dicts slices"  "$root\tests\suite\dictslice.py" $false
Check-Prune "prune math"          "$root\tests\suite\mathfns.py" $false
Check-Prune "prune tkinter"       "$root\tests\samples\gui_tk.py" $true
Check-Prune "prune c extensions"  "$root\tests\samples\cext.py" $false
Check-Prune "prune library data"  "$root\tests\samples\datafiles.py" $false

function Check-Lazy($label, $src) {
    $lean = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', '_lazy.exe')
    $plain = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', '_lean.exe')
    $err = Build $src $lean @("--prune-lazy")
    if ($err) { No $label $err; return }
    $want = (& $Python $src 2>&1 | Out-String)
    $got = (& $lean 2>&1 | Out-String)
    if ($want -ne $got) { No $label "output differs from interpreter"; return }
    $b = (Get-Item $lean).Length
    $a = if (Test-Path $plain) { (Get-Item $plain).Length } else { $b }
    if ($b -gt $a) { No $label "lazy build larger than prune build"; return }
    Ok ("$label ({0:N2} MB vs {1:N2} MB pruned)" -f ($b / 1MB), ($a / 1MB))
}

Check-Lazy "prune-lazy semantics"    "$root\tests\suite\semantics.py"
Check-Lazy "prune-lazy dicts slices" "$root\tests\suite\dictslice.py"
Check-Lazy "prune-lazy math"         "$root\tests\suite\mathfns.py"

$rep = & $compyler "$root\tests\suite\bits.py" "-o" (Join-Path $out "bits_rep.exe") "--size-report" 2>&1 | Out-String
if ($rep -match "size report" -and $rep -match "total payload" -and $rep -match "python standard library") {
    Ok "--size-report"
} else { No "--size-report" "report missing or incomplete" }

Write-Host ""
Write-Host "virtual environments"
$venvRoot = Join-Path $out "venvs"
Remove-Item -Recurse -Force $venvRoot -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $venvRoot | Out-Null

$vsrc = Join-Path $venvRoot "vapp.py"
Set-Content -Encoding utf8 $vsrc @'
import venvonly
print("venvonly:", venvonly.VALUE)
try:
    import requests
    print("requests: IMPORTED")
except ImportError:
    print("requests: ImportError")
'@

function Make-Venv($name, $extra) {
    $d = Join-Path $venvRoot $name
    $a = @("-m", "venv", "--without-pip") + $extra + @($d)
    & $Python @a 2>&1 | Out-Null
    $sp = Join-Path $d "Lib\site-packages\venvonly"
    New-Item -ItemType Directory -Force $sp | Out-Null
    Set-Content -Encoding utf8 (Join-Path $sp "__init__.py") ('VALUE = "' + $name + '"')
    return $d
}

function Check-Venv($label, $venv, $pyarg) {
    $vpy = Join-Path $venv "Scripts\python.exe"
    if (-not (Test-Path $vpy)) { No $label "venv not created"; return }
    $exe = Join-Path $venvRoot ((Split-Path -Leaf $venv) + ".exe")
    $log = & $compyler $vsrc "-o" $exe "--python" $pyarg 2>&1 | Out-String
    if (-not (Test-Path $exe)) { No $label ("build failed: " + ($log -split "`n" | Select-Object -Last 2)); return }
    $want = (& $vpy $vsrc 2>&1 | Out-String)
    $got = (& $exe 2>&1 | Out-String)
    if ($want -eq $got) { Ok $label }
    else { No $label ("venv=[" + ($want -replace "`r?`n", " | ") + "] exe=[" + ($got -replace "`r?`n", " | ") + "]") }
}

$viso = Make-Venv "isolated" @()
Check-Venv "isolated venv (no system site-packages)" $viso (Join-Path $viso "Scripts\python.exe")
Check-Venv "venv given as a directory" $viso $viso

$vsys = Make-Venv "systemsite" @("--system-site-packages")
Check-Venv "venv with --system-site-packages" $vsys (Join-Path $vsys "Scripts\python.exe")

$isoExe = Join-Path $venvRoot "isolated.exe"
if (Test-Path $isoExe) {
    $sysExe = Join-Path $venvRoot "systemsite.exe"
    if ((Test-Path $sysExe) -and ((Get-Item $sysExe).Length -gt (Get-Item $isoExe).Length)) {
        Ok ("isolated venv bundles less ({0:N2} MB vs {1:N2} MB)" -f ((Get-Item $isoExe).Length/1MB), ((Get-Item $sysExe).Length/1MB))
    } else { No "isolated venv bundles less" "isolated build is not smaller" }
}

$oldPath = $env:PATH
$env:PATH = (Join-Path $viso "Scripts") + ";" + $env:PATH
$actExe = Join-Path $venvRoot "activated.exe"
& $compyler $vsrc "-o" $actExe 2>&1 | Out-Null
$env:PATH = $oldPath
if (Test-Path $actExe) {
    $want = (& (Join-Path $viso "Scripts\python.exe") $vsrc 2>&1 | Out-String)
    $got = (& $actExe 2>&1 | Out-String)
    if ($want -eq $got) { Ok "activated venv found on PATH" } else { No "activated venv found on PATH" "output differs" }
} else { No "activated venv found on PATH" "build failed" }

Write-Host ""
Write-Host "other interpreters"

$interps = [ordered]@{}
foreach ($cand in @(
    "C:\Users\Office\AppData\Local\Programs\Python\Python311\python.exe",
    "C:\Users\Office\AppData\Local\Programs\Python\Python312\python.exe",
    "C:\Users\Office\AppData\Local\Programs\Python\Python313\python.exe")) {
    if (Test-Path $cand) { $interps[(Split-Path -Leaf (Split-Path -Parent $cand))] = $cand }
}
$uvroot = Join-Path $env:APPDATA "uv\python"
if (Test-Path $uvroot) {
    foreach ($d in Get-ChildItem $uvroot -Directory -ErrorAction SilentlyContinue) {
        $c = Join-Path $d.FullName "python.exe"
        if ((Test-Path $c) -and -not ($d.Name -match "freethreaded")) { $interps[$d.Name] = $c }
    }
}

$corpora = @(
    "$root\tests\suite\semantics.py",
    "$root\tests\suite\bits.py",
    "$root\tests\suite\mathfns.py",
    "$root\tests\suite\dictslice.py",
    "$root\tests\suite\builtins1.py",
    "$root\tests\samples\fstr.py"
)

$seenVer = @{}
foreach ($name in $interps.Keys) {
    $py = $interps[$name]
    $ver = (& $py -c "import sys;print('%d.%d' % sys.version_info[:2])" 2>&1 | Out-String).Trim()
    if ($seenVer.ContainsKey($ver)) { continue }
    $seenVer[$ver] = $true
    $bad = @()
    $native = 0
    foreach ($src in $corpora) {
        $exe = Join-Path $out ((Split-Path -Leaf $src) -replace '\.py$', ("_" + ($name -replace '[^A-Za-z0-9]', '') + ".exe"))
        $log = & $compyler $src "-o" $exe "--python" $py 2>&1 | Out-String
        if (-not (Test-Path $exe)) { $bad += (Split-Path -Leaf $src); continue }
        if ($log -match "(\d+) function\(s\) native") { $native += [int]$Matches[1] }
        $want = (& $py $src 2>&1 | Out-String)
        $got = (& $exe 2>&1 | Out-String)
        if ($want -ne $got) { $bad += (Split-Path -Leaf $src) }
    }
    if ($bad.Count -eq 0) { Ok ("python $ver differential, $($corpora.Count) corpora, $native functions native") }
    else { No "python $ver differential" ("mismatch in: " + ($bad -join ", ")) }
}

Write-Host ""
Write-Host "build and startup"

$nsrc = Join-Path $out "ncc.py"
Set-Content -Encoding utf8 $nsrc @'
def hot(n):
    s = 0
    i = 0
    while i < n:
        s = s + i * i
        i = i + 1
    return s
print(hot(1000))
'@
Remove-Item -Recurse -Force (Join-Path $env:LOCALAPPDATA "Compyler\nc") -ErrorAction SilentlyContinue
$nexe = Join-Path $out "ncc.exe"
$t1 = [Diagnostics.Stopwatch]::StartNew()
$log1 = & $compyler $nsrc "-o" $nexe 2>&1 | Out-String
$t1.Stop()
$t2 = [Diagnostics.Stopwatch]::StartNew()
$log2 = & $compyler $nsrc "-o" $nexe 2>&1 | Out-String
$t2.Stop()
if ($log1 -notmatch "function\(s\) native") { No "native module cache" "no native build" }
elseif ($t2.Elapsed.TotalSeconds -lt $t1.Elapsed.TotalSeconds * 0.6) {
    Ok ("native module cache ({0:N1}s -> {1:N1}s)" -f $t1.Elapsed.TotalSeconds, $t2.Elapsed.TotalSeconds)
} else { No "native module cache" ("no speedup: {0:N1}s then {1:N1}s" -f $t1.Elapsed.TotalSeconds, $t2.Elapsed.TotalSeconds) }
if ((& $nexe 2>&1 | Out-String).Trim() -eq "332833500") { Ok "cached native module runs correctly" }
else { No "cached native module runs correctly" "wrong output" }

$hsrc = Join-Path $out "hstart.py"
Set-Content -Encoding utf8 $hsrc 'print("hello")'
$hexe = Join-Path $out "hstart.exe"
& $compyler $hsrc "-o" $hexe 2>&1 | Out-Null
& $hexe 2>&1 | Out-Null
$env:COMPYLER_TRACE = "1"
$tr = (& $hexe 2>&1 | Out-String)
Remove-Item Env:\COMPYLER_TRACE
if ($tr -match "hook imported\s+([\d.]+) ms\s+ws\s+([\d.]+) MB") {
    $ms = [double]$Matches[1]; $mb = [double]$Matches[2]
    if ($ms -lt 30 -and $mb -lt 12) { Ok ("hello world starts in {0:N1} ms using {1:N1} MB" -f $ms, $mb) }
    else { No "hello world startup" ("{0:N1} ms / {1:N1} MB, expected under 30 ms and 12 MB" -f $ms, $mb) }
} else { No "hello world startup" "trace not produced" }

Write-Host ""
Write-Host "cache behaviour"
$e = "$out\cache.exe"
$err = Build "$root\tests\t1\main.py" $e @()
Get-ChildItem "$env:LOCALAPPDATA\Compyler" -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "main-*" } | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
$sw = [Diagnostics.Stopwatch]::StartNew(); & $e *> $null; $cold = $sw.Elapsed.TotalMilliseconds
$sw = [Diagnostics.Stopwatch]::StartNew(); & $e *> $null; $warm = $sw.Elapsed.TotalMilliseconds
if ($warm -lt $cold) { Ok ("cache reuse (cold {0:N0} ms, warm {1:N0} ms)" -f $cold, $warm) }
else { No "cache reuse" ("cold {0:N0} warm {1:N0}" -f $cold, $warm) }

Write-Host ""
Write-Host ("PASSED {0}   FAILED {1}" -f $script:pass, $script:fail)
if ($script:fail -gt 0) { exit 1 }
exit 0
