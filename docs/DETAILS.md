# Compyler

Turns `main.py` into `main.exe` on Windows x64, translating your Python
functions into native machine code along the way. The compiler and the runtime
are written in C.

```bat
bin\compyler.exe main.py
main.exe
```

Packages with C extensions come along: `numpy`, `scapy`, `requests`, `pandas`,
`pywin32` are collected with their `.pyd` files, transitive DLL dependencies and
data files.

## Two halves

**A code generator.** Every function in your own modules is translated from
CPython bytecode into C, then compiled to a `.pyd` by clang-cl when LLVM is
installed, or MSVC otherwise. What is left after
that is not an interpreter loop: no opcode dispatch, no bytecode pointer, and
for integer and float work no heap allocation and no reference counting either.

**A packager.** Everything the program imports, plus the interpreter itself, is
gathered into a single executable that unpacks once into a content hashed cache
and starts in tens of milliseconds thereafter.

## How the code generator works

CPython already ships a complete Python compiler written in C. Compyler loads
`pythonXY.dll` with `LoadLibraryEx`, resolves the C API through `GetProcAddress`,
and drives it directly: `Py_CompileStringExFlags` for source to bytecode,
`dis.get_instructions` for a version normalised instruction stream, and
`dis.stack_effect` for stack depths. Nothing about opcode numbering or argument
encoding is hardcoded, which is why one binary targets 3.11, 3.12 and 3.13.

The parts that do differ between versions are handled explicitly rather than
assumed away. 3.11 accounts for a call's arguments in `PRECALL` rather than in
`CALL`, so the callable sits two slots below the modelled depth instead of
`argc + 2`. 3.13 reversed the calling convention, pushing the callable before
the `NULL` sentinel where 3.12 pushes `NULL` first, and it introduced the
`LOAD_FAST_LOAD_FAST` family of superinstructions along with `TO_BOOL`. Each of
those is a real behaviour change that silently produces wrong code if you assume
one version's layout, so the differential corpora are run against every
interpreter found on the machine.

Two code paths come out of that.

**A typed path.** When inference can prove that every parameter and local in a
function is an `int` or a `float`, and the body only does arithmetic, comparisons,
branches, `range` loops and self-recursion, the function is emitted as ordinary C
over `int64_t` and `double` locals. No tags, no boxing, no reference counting.
`s = s + i * i` becomes an `imul`, an `add` and two overflow branches.

Small strings and integer dicts get the same treatment. An f-string of
integers becomes a stack character buffer filled by a native integer formatter,
`ord(s[j])` is a bounds checked byte read, and `len` is a field access, so a
hashing loop over formatted strings runs at C speed. A dict created inside the
function and used with integer keys and values becomes an open addressing hash
table whose probe sequence Compyler controls, which is how `dictops` beats both
`std::unordered_map` and Go's map. A list built by appending freshly created
rows becomes an array of row pointers with ownership transferred on append, so
`a.append(row)` costs a pointer store and `a[i][j]` two indexed loads, while a
row still aliased by a local stays mutable through either name exactly as in
Python. Binding a row to a name, `ai = a[i]`, types as an alias of the row's
flat buffer: `ai[k]` is one indexed load, writes through `ai` are visible
through `a[i]` and vice versa, and `matmul`'s inner product runs over two
contiguous float rows, which is what lets LLVM vectorize it past the C++
build. Anything outside these shapes, a missing key, a mixed list, a string
past 63 bytes, a non ASCII constant, falls back to the general path with the
interpreter's exact behavior.

Lists of numbers become native arrays. A list parameter whose elements are all
`int` or all `float` is snapshotted into a flat `int64_t` or `double` buffer at
entry and written back only if the function completed and actually stored into
it, so a bail can always discard the copies and rerun the general path against
the untouched originals. A list created inside the function as `[0] * n` or
`[0.0] * n` never becomes a Python list at all: it is a `malloc`, its subscripts
are bounds checked loads and stores, and `len` is a field read. Aliasing two
array locals, returning an array, or storing a wrong typed element all reject
typing at compile time, and a mixed or bigint list falls back at run time.
When every value stored into a locally created int array is provably within
`[-128, 127]` and the array never escapes the function, the buffer is
allocated at one byte per element and filled with a single `memset`, so
`sieve`'s flag array carries an eighth of the memory traffic and runs at the
speed of the C++ `vector<char>` version.

Typed functions also call each other directly. When one typed function calls
another whose signature matches, the call compiles to a plain C call with
unboxed arguments, arrays passed as pointer and length. The binding is guarded
once per entry: the callee's global must still be the original function,
verified against the trampoline it was compiled into, so rebinding `sift` at
runtime falls back to the general path and keeps working. This is what lets
`heapsort` drive `sift` three million times without leaving C.

Calls to `math.sqrt`, `math.sin`, `math.log`, `atan2`, `hypot` and eighteen other
`math` entries become the corresponding C library call on unboxed doubles, as do
`float()`, `int()` and `abs()`, and two argument `min()` and `max()` compile to
a bare compare and select with Python's exact NaN and signed zero behavior. The
binding is checked once per call into the
function, not once per iteration: the guard proves the global is the real module
and that the attribute is still the original C function, so rebinding
`math.sqrt` at runtime falls back to the general path and keeps working.
Domain errors that Python raises (`sqrt(-1)`, `log(0)`, `exp(710)`) are detected
and bail rather than producing a NaN.

Overflow, division by zero, a domain error, or a `range` global that is no
longer the builtin make the typed body return a bail code. Because such a
function is pure by construction, the caller simply re-runs it on the general
path, so speculation is always safe.

**A general path.** Everything else uses a tagged value held in C locals:

```c
typedef struct { int32_t t; int64_t b; } cv;
```

An `int` that fits in 64 bits lives in a register as a plain integer, a `float`
as a double. Only values that escape or overflow become `PyObject *`. Operand
folding means `s + i * i` reads its operands straight out of locals instead of
pushing and popping a stack.

Overflow is checked on every integer operation and promotes to a real Python
`int`, so `2**62 + 2**62` still gives `9223372036854775808`.

Other things the generator does:

- Functions in the same module call each other as direct C calls, guarded by an
  identity check on the callee, so recursion never re-enters the interpreter.
  In the typed path a self-call is a plain C call with unboxed arguments.
- `for x in range(...)` becomes a counted native loop when the object really is
  a `range`, decided at runtime by type, not by name.
- `LOAD_GLOBAL` is cached per call site against the module and builtins dict
  versions, and `LOAD_ATTR` on a module gets the same treatment, so `math.pi`
  inside a loop costs a version compare rather than a dict lookup.
- Subscripting has direct paths for `list`, `tuple`, `str` and `dict`, on both
  load and store, so container code never enters the generic mapping protocol.
- f-strings compile natively: conversions (`!r`, `!s`, `!a`), format specs,
  nested specs and the `__format__` protocol all go through C.
- `len`, `ord`, `abs`, `chr`, `float` and `int` are recognised at call sites on
  the general path too. The emitted code compares the resolved callee against
  the real builtin and only then takes the direct route, so shadowing the name
  anywhere falls back to a normal call. This is what takes `len(s)` inside a
  loop from a vectorcall to a field read.
- Comparisons feeding a branch skip materialising a `bool`.
- Constant operands are folded into the operation, so a multiply by a literal
  gets a two-compare overflow check instead of a 128-bit one.

Each compiled function keeps a real Python function object whose body forwards
to the native code, so signatures, defaults, keyword arguments, decorators and
`inspect` keep working. Anything the generator does not support stays as
bytecode, per function, and still runs.

## Measurements

Windows 11 x64, CPython 3.12.10. The same 22 workloads are written four
times: `tests/bench/bench.py`, `bench.cpp`, `bench.go` and `bench_codon.py`
are line for line equivalent, and the Python source runs as CPython, Nuitka
4.1, Compyler and Compyler `--wrap-int`. Codon 0.19.6 publishes no Windows
build at all, so it runs on the same machine under WSL2 Ubuntu, and the same
`bench.cpp` compiled with the same `g++ -O2` on both sides calibrates what
that environment is worth. Every runner is interleaved in one window, three
passes, each case reporting its own in process best of three. Milliseconds.

| case | CPython 3.12 | Nuitka 4.1 | Compyler | Compyler --wrap-int | Codon 0.19.6 (WSL2) | Go 1.26 | C++ (g++ -O2) | C++ (WSL2, g++ -O2) |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| fib(30), recursive | 51.0 | 37.7 | 1.9 | 1.1 | 1.5 | 1.6 | 0.6 | 0.6 |
| forsum, 2M | 45.6 | 44.1 | 0.9 | 0.0 | 0.0 | opt | 0.5 | 0.5 |
| intloop, 2M | 94.9 | 69.4 | 1.1 | 0.5 | 0.3 | 0.5 | 0.8 | 0.8 |
| collatz, 50k | 175.0 | 81.0 | 5.0 | 4.1 | 5.1 | 5.4 | 4.7 | 5.2 |
| primes, 80k | 53.3 | 25.5 | 2.6 | 2.3 | 2.5 | 3.2 | 4.3 | 2.8 |
| mandelbrot, 300x200 | 117.7 | 69.8 | 2.2 | 2.3 | 2.3 | 2.0 | 2.3 | 2.3 |
| listsum, 50k x40 | 53.1 | 36.7 | 0.6 | 0.2 | 0.1 | opt | 0.4 | 0.4 |
| nbody, 20k steps | 62.9 | 54.7 | 0.9 | 0.9 | 0.8 | 1.0 | 0.9 | 0.8 |
| spectralnorm, 500 | 396.8 | 324.2 | 3.7 | 3.4 | 3.5 | 3.1 | 3.4 | 3.5 |
| matmul, 110 | 46.7 | 32.2 | 0.6 | 0.5 | 1.1 | opt | 0.5 | 0.4 |
| queens, 10 | 71.8 | 35.0 | 3.2 | 2.6 | 2.2 | 2.0 | 2.0 | 2.0 |
| heapsort, 200k | 429.9 | 345.0 | 14.6 | 14.0 | 16.2 | 14.1 | 14.0 | 14.4 |
| bitops, 5M | 638.9 | 475.0 | 5.2 | 5.0 | 4.9 | 6.5 | 4.8 | 4.9 |
| gcdloop, 400k | 96.9 | 51.1 | 16.4 | 16.4 | 13.8 | 19.6 | 19.1 | 20.1 |
| binarytrees, 16 | 40.4 | 39.4 | 15.7 | 15.9 | 9.1 | 6.5 | 22.6 | 5.8 |
| sieve, 2M | 157.3 | 113.0 | 2.8 | 2.1 | 9.6 | 2.0 | 2.4 | 2.1 |
| dotprod, 100k x20 | 73.5 | 62.9 | 1.0 | 1.1 | 1.1 | opt | 0.9 | 0.9 |
| trig, 1M | 91.8 | 116.7 | 6.7 | 6.8 | 6.8 | 6.6 | 34.3 | 7.1 |
| fannkuch, 9 | 338.6 | 253.0 | 11.8 | 12.5 | 10.8 | 10.6 | 10.6 | 11.0 |
| taylor, 10M | 488.6 | 413.6 | 6.8 | 6.9 | 6.9 | 6.0 | 6.8 | 6.9 |
| strhash, 700k | 797.4 | 597.4 | 23.9 | 23.0 | 48.7 | 26.9 | 28.3 | 28.0 |
| dictops, 700k | 121.7 | 108.6 | 22.7 | 25.9 | 15.6 | 44.0 | 44.5 | 21.7 |
| **total** | **4443.6** | **3385.6** | **150.1** | **147.4** | **162.7** | **161.5** | **208.5** | **142.1** |

All figures are milliseconds with full Python semantics on the Compyler
columns: arbitrary precision integers, `IndexError` on a bad subscript,
`KeyError` on a missing key. `fannkuch` and `taylor` come from the benchmark
suite Codon uses. The C++ and Go builds route their parameters through run
time values because both compilers otherwise delete whole workloads; `opt`
marks cases where Go still did. Codon's 0.0 on `forsum` is real under its own
semantics: with wrapping integers LLVM evaluates the loop in closed form.
Compyler's 0.9 is the checked version of the same loop with every overflow
accounted for; a compiler that reports 0.0 there with claimed checked
semantics is not running its checks, which is exactly the class of defect the
corpora here exist to catch. Every integer result in the table was cross
checked against CPython's and they all agree, Codon included.

The number that matters: **Compyler 150.1 ms, Codon 162.7 ms**. Full checked
Python semantics against Codon's wrapping 64 bit dialect, and Compyler is
faster over the whole suite. The comparison bends in Codon's favor, not ours:
Codon ran in the faster environment. The identical `bench.cpp` compiled with
the identical compiler runs 1.47x faster under WSL2 than natively on Windows,
142.1 against 208.5 ms, mostly glibc's math library and allocator, and Codon
gets all of that for free while Compyler's numbers are native Windows. Codon's
two largest remaining wins sit exactly where that environment gap is widest:
on `binarytrees` the same C++ is 3.9x faster under WSL2, on `dictops` 2.1x.
Compyler wins `sieve` 3.4x, `strhash` 2.0x, `matmul` 1.9x, `heapsort` 1.12x,
and `mandelbrot`, `dotprod`, `trig` and `taylor` outright, and ties `collatz`.
Checked `fib` is Codon's clearest real compute win and it is down to 1.9
against 1.5: the emitter performs the accumulator transformation on checked
self recursion itself, so the remaining gap is one overflow checked addition
per node, the exact price of never printing a wrong sum. Codon's
`-numerics=py` mode, which fixes division semantics but still wraps at 64
bits, totals 167.8 ms. Against the rest of the field in the same window: 30x
faster than CPython, 23x faster than Nuitka, 1.39x faster than the native C++
build, and 19 of 22 cases within 1.5x of it or better. The wrapping flag has
become nearly pointless, 147.4 against 150.1 checked, under two percent,
because everything the analysis can prove safe is already unchecked, deferred
accumulator flags keep summation loops branchless, and the accumulator
transform keeps recursion in the value function class with its checks
intact.

Native modules compile with clang-cl when an LLVM installation is found (LLVM,
the Visual Studio Clang component, PATH, or an MSYS2 install are all probed),
falling back to MSVC otherwise; `--cc cl` forces MSVC, `--cc clang` or an
explicit clang-cl path forces LLVM. On this suite the LLVM backend is about 15
percent faster overall, measured in the same window with identical output.

Typed functions whose generated bodies contain no bail sites at all are
emitted a second time as direct value returning C functions, with the status
form reduced to a one line wrapper. Recursive integer functions used to be
kept out of this class by their overflow checks; now a checked add, subtract
or multiply whose result flows only into the returned value accumulates into a
sticky per function overflow flag instead of branching out, the wrapper reads
the flag once per call, and a call that actually overflowed is discarded and
rerun on the general path with real Python integers. Because the poisoned
value can never reach a branch, a subscript or a call argument, control flow
is identical to the true execution and the bail stays safe by construction.
On top of that, the emitter performs the accumulator transformation on
checked self recursion itself: `return f(n-1) + f(n-2)` compiles to a loop
carrying an accumulator plus one recursive call per node, with every addition
still overflow checked. Reassociating checked additions is sound under the
bail and rerun model, because a checked add either produces the exact sum or
bails to the general path, so changing the association can never change a
successful answer, only which inputs take the slow path. Compilers cannot
legally make that argument about a sticky flag, which is why neither g++ nor
LLVM will do this transformation on exactly detected arithmetic; the emitter
can, because the fallback interpreter is part of its semantics. Checked `fib`
lives in the value function class with its checks intact: fib(40) runs in
0.24 s checked on this machine, where CPython needs 7.9 s and Codon's
published figure for its own hardware is around 0.45 s.

For code whose values can genuinely overflow 64 bits, the remaining gap against
C is the overflow branch on every arithmetic op that Python's arbitrary
precision integers demand. A sound interval analysis removes those branches
wherever bounds can be proven at compile time, and it reads the program's own
comparisons: on the edge where `n < 2` failed, `n` is known to be at least 2,
which is why `fib` pays nothing for `n - 1` and `n - 2`. Any typed function
additionally asserts, once at entry, that the integer parameters it indexes
or compares against lie in `[0, 2^31]`, and a call outside that envelope
simply falls back; the envelope is small enough that the product of two such
values still fits in 64 bits, which is what deletes the multiply checks in
squaring loops and lets `sift` compile `root * 2 + 1` to a plain multiply
with every bound arriving as an argument. The assertion is only emitted when
a second interval pass proves it actually removes checks or bounds tests, so
a function that would not benefit, `fib` among them, pays nothing at entry
and keeps its full argument domain. Masked hash chains and constant arithmetic remain the common
static wins, and division or modulo by a power of two compiles to a shift or
mask with exact floored semantics. Where analysis cannot prove safety,
`--wrap-int` opts into Codon's own convention, a wrapping 64 bit integer;
unlike Codon this is a per build choice rather than the language's definition.

Accumulators get their checks kept and moved rather than paid per iteration.
A local whose value can reach only itself and the function's return, never a
branch, a subscript index, a call argument or a stored element, has its
additions and multiplications compiled as wrapped operations that fold an
overflow bit into a local flag, tested once at every return. A poisoned value
cannot alter control flow, so an overflowing run is discarded and rerun on the
general path exactly like any other bail, and the branchless body is what lets
LLVM vectorize summation loops whose checks are all still accounted for.

Wrapping stays a flag and never becomes the default, on purpose. A build that
wraps silently keeps running and prints wrong numbers, which is the worst
failure mode a compiler can pick, and it would void the guarantee everything
else here is built on: the exe's output is the interpreter's output. The
measured price of that guarantee is now under two percent on this suite, 150.1
against 147.4 ms, because interval analysis, branch refinement, entry
contracts, deferred accumulator flags and the checked accumulator transform
together delete or amortize every check that matters; what remains
concentrates in `fib` (1.9 against 1.1 ms). Under two percent is a good trade
for never shipping a wrong answer, and it is why the flag has stopped
mattering: there is no longer a workload here where wrapping buys more than
noise.

Bounds checks are no longer a blanket cost. A loop of the form `while i < n`
whose body subscripts `a[i]` gets one hoisted check at loop entry, `n <=
len(a)`, after which every subscript in the loop compiles to a bare indexed
load or store: the loop condition proves `i < n`, interval analysis proves `i
>= 0`, and if a caller ever violates the hoisted check the function falls back
to the general path, so semantics are preserved by construction. The analysis
verifies the loop region has no stray entry points and that neither the index
nor the bound nor the array is reassigned where it would matter, including
across the duplicated bottom tests CPython emits for rotated loops. The same
treatment covers the inverted shape `while 1: ... if i >= n: break` that heap
code writes, and `for i in range(...)` loops over arrays: a range loop checks
once at entry that its start is not negative and its stop does not exceed the
array length, and every subscript in the body is a bare access for any
positive step, because the iterator itself proves `i < stop` on every
iteration. When the guarded bound and the array are parameters that the
function never reassigns, the check moves all the way to function entry and
executes once per call.

The three cases still behind the native C++ build are honest ones. Checked
`fib` costs 1.9 against 0.6: the emitter's accumulator transform gives it the
same loop shape g++ produces, and the remaining difference is one overflow
checked addition per node plus the recursion depth g++ additionally inlines
away. `forsum` pays 0.9 against 0.5 for checks LLVM would otherwise fold away
entirely, and `queens` sits at 1.6x on shuffle heavy index code, with
`fannkuch` within 1.15x. Everything else is at or past the C++ build: `sieve` runs at `vector<char>` speed because small
integer arrays are stored one byte per element, `matmul` beats the C++ build
outright now that rows bind to flat float buffers and vectorize, and
`heapsort` wins because `sift`'s entry contract turns its whole inner loop
into unchecked machine code.

Reproduce all of it with `tests\benchmark.ps1`, which also prints the resource
tables below.

Whole process cost for that same 22 workload run:

| runner | wall | CPU | user | kernel | peak RAM | page faults | handles |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CPython | 15540 ms | 15469 ms | 15328 ms | 141 ms | 116.4 MB | 117590 | 114 |
| Nuitka | 11638 ms | 11563 ms | 11375 ms | 188 ms | 120.2 MB | 118448 | 112 |
| Compyler | 456 ms | 453 ms | 406 ms | 47 ms | 40.4 MB | 40505 | 121 |
| Go | 521 ms | 547 ms | 453 ms | 94 ms | 39.3 MB | 12671 | 161 |
| C++ | 627 ms | 609 ms | 516 ms | 94 ms | 31.8 MB | 40906 | 78 |

Build time for the benchmark module is about 8 seconds the
first time, almost all of it the single compiler invocation for the generated
code, and **1.7 seconds** on any rebuild that produces the same C. The built
extension is cached under `%LOCALAPPDATA%\Compyler\nc`, keyed on the generated
source, the runtime header, the target interpreter, the code backend and the
compiler's own timestamp, so it invalidates itself rather than going stale.
Nuitka takes roughly 40 seconds on the same input, every time.

Peak memory is 40 MB against CPython's 116 MB on the same workloads, because
the typed engine's arrays, strings and maps never touch the object heap at
all, and kernel time drops with it, so the speedup is real user time and not
traded against system overhead.

Startup, for a one file build of a hello world:

| | size | first run | later runs |
| --- | --- | --- | --- |
| PyInstaller 6, onefile | 6.95 MB | 423 ms | 424 ms |
| Compyler, onefile, defaults | 6.03 MB | 198 ms | 15 ms |
| Compyler, onefile, `--compress fast` | 7.79 MB | 170 ms | 16 ms |
| Compyler, onefile, `--upx` | 5.93 MB | 202 ms | 19 ms |
| Compyler, onefile, `--compress none` | 17.94 MB | 173 ms | 16 ms |
| Compyler, onefile, `--prune-lazy` | 3.24 MB | 139 ms | 16 ms |
| Compyler, `--onedir` | 0.17 MB + folder | 15 ms | 15 ms |

First runs on a Windows Defender protected machine additionally pay a one time
scan of the freshly extracted payload, which can reach seconds on the very
first launch of a never seen binary; the figures above are steady state. A
warm start is 15 ms using 10.6 MB, against 21 ms for the same script under
`python.exe`: the frozen build now starts faster than the interpreter it
ships. That is deliberate rather than incidental. A
frozen build runs isolated, so `site` never runs and the import system never
warms up; anything the runtime needs to configure is therefore done from C,
before any Python module is imported. Setting `sys.path`, registering DLL
directories and clearing the bootstrap environment variables used to be a
generated Python module, and importing it cost 31 ms and 4 MB on a program that
does nothing but print. Now that work reads a small config block out of the
archive and calls `AddDllDirectory` and `PySys_GetObject` directly, and a
generated module is only emitted when something genuinely needs Python at
startup: a native module to install, or a `multiprocessing` patch for an
application that actually imports it.

Set `COMPYLER_TRACE=1` to see where a start goes:

```
compyler trace: read tail                  0.1 ms  ws   4.92 MB
compyler trace: python dll loaded          4.6 ms  ws   6.05 MB
compyler trace: interpreter init          12.3 ms  ws  10.62 MB
compyler trace: about to run              13.0 ms  ws  10.64 MB
```

Application level, compiled executable against the interpreter running the same
source:

| workload | runner | wall | CPU | peak RAM | exe |
| --- | --- | --- | --- | --- | --- |
| numpy, sqlite3, lxml, PIL, zlib, lzma, hashlib | compyler | 154 ms | 156 ms | 46.2 MB | 33.2 MB |
| | python | 177 ms | 203 ms | 47.1 MB | |
| tkinter, ttk, canvas, fonts | compyler | 141 ms | 156 ms | 25.0 MB | 10.3 MB |
| | python | 171 ms | 109 ms | 25.6 MB | |
| the differential corpora | compyler | 21 ms | 47 ms | 11.0 MB | 7.8 MB |
| | python | 31 ms | 16 ms | 11.4 MB | |

### Size

`--size-report` prints what the payload is actually made of, grouped by
component and sorted by packed contribution. Use it before guessing.

A minimal one file build is about 6 MB with the default pruning, and over a
third of that is `pythonXY.dll` itself. Note that `--upx` only packs the 170 KB loader, not the
payload, which is already LZMS compressed; it saves about 90 KB, so it is not
the lever it looks like. `-O` strips asserts and `-OO` additionally drops
docstrings, neither moves the size needle much.

Pruning is the default: only the modules the import graph actually reaches
are bundled, instead of taking a whole package tree once any part of it is
imported. `--no-prune` restores whole tree bundling when a package's dynamic
imports defeat the scanner and `--hidden-import` is too fiddly. It resolves dotted
and relative imports, so `from . import x` inside a package pulls in exactly `x`.

It stays correct by refusing to prune what it cannot prove:

- A package whose code builds a module name at run time, such as `__import__(
  "encodings." + name)`, is detected and kept whole. The signal is a literal that
  ends in a dot or is concatenated, not a list of package names.
- A directory containing an extension module is kept whole, because a `.pyd`
  imports its Python siblings from C where no scanner can see it. `lxml.etree`
  reaching for `lxml._elementpath` is the usual case.

What is left is real: 13% off a pure Python program, 10% off a tkinter app, and
very little off something dominated by numpy or scipy binaries, where almost
everything bundled is genuinely reachable. `--hidden-import` remains the fix for
anything a dynamic import still hides, and the pruned build is worth running
before shipping.

`--why` answers the question pruning always raises. `--why hashlib` prints
the import chain that dragged `hashlib` in (`hashlib <- random <- tempfile <-
...`), and `--why all` prints every module's chain. It is how the pruner
itself gets audited.

`--prune-lazy` goes further: it skips imports that sit inside function bodies,
which only execute if the function is called. That is where most stdlib weight
hides. `os.popen` lazily imports `subprocess`, `functools.singledispatch`
lazily imports `typing` which drags `inspect`, and one method of the import
machinery lazily imports `importlib.metadata` which drags `email`. None of
that runs in a program that never calls those functions. A script that
computes fib drops from 5.8 MB to **3.2 MB** and 50 bundled files, and starts
the same. The contract is sharper than the default prune: a function level
import of a module nothing else reaches will fail at call time, so test the
app and use `--hidden-import` for anything dynamic. Package init imports,
module level try/except imports, and dynamic imports with a literal package
prefix (the `encodings` codec machinery) are all still followed; imports under
`if TYPE_CHECKING:` are always skipped since they never execute at runtime,
and packages that ship extension modules anywhere in their tree are scanned
with the lazy rule off entirely, because binary packages like numpy hide
Python imports behind machinery no scanner should trust. Reaching
`importlib.resources` pulls its reader machinery along explicitly, since the
resource loaders import each other from inside functions; a hello world that
never touches it still builds at 3.24 MB.

### Virtual environments

If a venv is active, Compyler finds it on `PATH` and no flag is needed. Otherwise
`--python` accepts the venv's `python.exe`, or just the venv folder.

A venv holds no standard library and no `pythonXY.dll` of its own, so `pyvenv.cfg`
is read for `home` and the interpreter, stdlib and `DLLs` come from the base
installation while packages come from the venv. `include-system-site-packages` is
honoured: with the default `false` the base `site-packages` is not searched at
all, so the executable resolves imports exactly the way the venv interpreter
does. A package present in both wins from the venv, including its `.dist-info`,
so `importlib.metadata.version()` reports the venv's version.

That isolation is worth stating plainly, because the alternative is a silent
correctness bug rather than a cosmetic one. Compyler used to search the base
`site-packages` unconditionally. A script that failed with `ImportError` under
its own venv would build and run anyway, quietly picking up a different copy of
the dependency from the machine's global installation. On the test venv here
that was the difference between a 12.8 MB executable that imported the base
`requests` and a 7.78 MB one that behaves like the venv.

Stdlib `venv` and `uv venv` are both covered by the test suite, with and without
`--system-site-packages`.

## Why this and not Codon, Nuitka, or PyInstaller

The honest comparison, with the speed column citing the suite measured above
on this machine in one window:

| | what runs | numpy, scapy, pandas | integer semantics | suite above | Windows | ships as |
| --- | --- | --- | --- | --- | --- | --- |
| Compyler | your program, unchanged | native, unmodified | interpreter identical | 150 ms | native | one 3 to 8 MB exe, 15 ms warm start |
| Codon | a Python like dialect | over a CPython bridge | 64 bit, wraps by design | 163 ms, under WSL2 | no build exists | native binary, dialect code only |
| Nuitka | your program, unchanged | native, unmodified | interpreter identical | 3386 ms | native | exe or dist folder, minutes long builds |
| PyInstaller | your program, unchanged | native, unmodified | interpreter identical | interpreter speed | native | one exe, 400 ms every start |

Codon is the interesting rival, and it is now a measured one rather than a
cited one. How the measurement was made, because every detail favors Codon:
Codon 0.19.6 ships no Windows binary, so it ran on this same machine under
WSL2 Ubuntu with `codon build -release`, inside the Linux environment that the
identical C++ source proves is 1.47x faster on this suite than native Windows.
The workloads are line for line the same source, with two forced exceptions
that are themselves the story: `binarytrees` builds its tree from tuples in
Python, a recursive tuple type Codon cannot express, so the Codon port uses a
class; and every call had to be pinned inside the timing window with a global
sink, because Codon's optimizer otherwise moves the whole pure workload past
its own timers and reports 0.00 ms for work it does elsewhere. All integer
results were verified identical to the interpreter's. Result: Compyler wins
the suite, 150.1 ms checked against Codon's 162.7 wrapping, from the slower
side of the fence, with every integer check Python's semantics demand still
in place.

The design difference stands regardless of the score. Codon asks how fast a
Python shaped language can be, and answers it by defining a new language: its
`int` is a wrapping 64 bit machine word, its standard library is its own
reimplementation, the dynamic parts of Python are restricted, and NumPy or any
other C extension is reachable only through an interop bridge that round trips
through CPython objects. The price of that decision is your program. Most real
code, and effectively all code with C extension dependencies, has to be ported
before Codon will compile it, and once ported it can produce different numbers
than Python does.

Compyler asks a different question: how fast can the program you already have
be, producing the output you already get. The interpreter ships inside the
exe, so anything that runs under CPython runs in the build. Functions the
typed engine can prove out compile to the same class of machine code Codon
emits, which the table above now puts ahead of Codon itself, and everything it
cannot prove falls back to bytecode instead of failing the build. When Codon's
integer convention is genuinely wanted, `--wrap-int` provides exactly it, per
build instead of per language.

What Codon has that this does not: `@par` OpenMP parallelism, GPU codegen, and
Linux and macOS targets. Those are real, and out of scope here for now. This
project spends its complexity budget on being a drop in replacement first, on
the platform Codon does not run on.

## Correctness

Generated code has to mean exactly what the interpreter means, so the build
verifies it rather than assuming it. Before any of your code is transformed,
Compyler compiles a canary function through the full pipeline, runs it both
natively and under the interpreter, and compares. If they disagree, native
compilation is switched off for the build and you get a working bytecode
executable plus a message. The result is cached per toolchain, so the extra
compile is paid once.

This is not hypothetical. Two real compiler defects were caught this way:

- MSVC 19.51 miscompiles the tagged value fast paths at plain `/O2`: after scalar
  replacement it drops one of the two type guards, so `y * 2.0 / h` silently
  produces `inf` when `h` is an int. The generated module is built with
  `/d2SSAOptimizer-`, which keeps the rest of `/O2` and fixes it.
- `/fp:fast` let the compiler reassociate float expressions, so a typed
  `out * 1000.0 + acc` drifted by one ulp from the interpreter. The generated
  module is built `/fp:precise`.

If the generated module fails to compile or fails the canary, the affected
modules are rebuilt as plain bytecode and the executable still works.

The differential suite earns its keep on the generated code too. Building out
the benchmark set surfaced four real defects in this code generator, all of
which the suite now pins: integer constants were converted through a 32 bit
`long`, so any mask above 2^31 silently left the typed path; the typed left
shift bailed on any operand above 2^22 instead of testing for actual overflow,
which cost 18x on bit manipulation; a dict miss raised `KeyError` with the wrong
arguments when the key was `None` or a tuple, because `PyErr_SetObject` treats a
tuple value as an argument list; and a failed native build could leave
substituted trampolines in the emitted bytecode. The worst one hid in the
interval analysis itself: its finiteness test used an OR where it needed an
AND, so a half open interval, which is what every loop accumulator has, counted
as bounded and its overflow checks were silently deleted; a checked build could
print a wrapped number instead of the interpreter's big integer. No earlier
corpus had an accumulator that actually overflowed, so it survived every suite
until one did. It is fixed, the corpora now overflow accumulators on purpose,
and the deferred check mechanism above is what won the speed back honestly.
Building out the bounds and
row machinery pinned two more: the interval pass never seeded parameter
ranges, so an index parameter that was never reassigned read as provably non
negative and the eliser dropped its negative index check, an out of bounds
read on `f(a, -3, n)` shapes; and the list of rows matcher could bind the
element type of the wrong list when a conditional expression chose between two
of them. Both are fixed and both shapes now sit in the corpora.

A full dead code and logic audit of every compiler source file, with each
finding adversarially re-verified before being touched, closed out three more
real defects alongside thirty odd pure cleanups: the import scanner mishandled
backslash line continuations in CRLF files, both inside string literals, where
the leftover newline ended the string early and the rest of the literal was
scanned as code, and at statement level, where it reset the indentation state
mid statement; the launcher passed an invalid flag to
`SetDefaultDllDirectories`, so the call had failed silently on every launch
since it was written and the bundled DLL directories were never actually
registered with the loader; and a runtime list append helper would have handed
`PyList_Append` a null pointer on its unreachable defensive arm instead of
raising. All three are fixed and the whole gate, 67 suite checks, three
interpreter corpora and the venv stress builds, passes on the result.

## Building the toolchain

Needs Visual Studio with the C++ workload. The script finds it.

```bat
build.bat
```

Produces `bin\compyler.exe`, `bin\stub.exe` and `bin\cpyrt.h`. Keep them together.

## Usage

```
compyler <script.py> [options]

  -o, --out PATH        output executable (default: <script>.exe)
      --name NAME       application name used for the runtime cache
      --onedir          emit exe plus _internal folder instead of one file
      --windowed        gui subsystem, no console window
      --icon FILE       embed an .ico file
      --python PATH     target interpreter (python.exe or its folder)
  -O, -OO               bytecode optimization level 1 or 2
      --hidden-import M force a module into the bundle
      --exclude M       drop a module from the bundle
      --add-data S;D    copy an extra file or folder into the bundle
      --compress LEVEL  none, fast or max (default max, lzms)
      --no-compress     store the payload uncompressed
      --upx             run upx on the loader when it is available
      --run             launch the produced executable once it is built
      --no-prune        bundle whole package trees (default: prune to the import graph)
      --prune-lazy      prune, and also skip imports inside function bodies
      --why M           print why module M was bundled (or: all)
      --no-strip        keep headers, stubs and build artifacts, scan test dirs
      --strip-tests     omit bundled test suites entirely
      --no-default-excludes  keep build and test tooling in the bundle
      --no-native       skip native code generation, ship bytecode only
      --wrap-int        codon style 64 bit wrapping ints in native code
      --cc NAME         native code backend: clang, cl, or a clang-cl path
      --keep-build      keep the native build dir with the generated C
      --size-report     print what the payload is made of
  -j N                  worker threads (default: cpu count)
  -v                    verbose, lists every function compiled natively
```

## What gets compiled natively

Your own modules, the ones next to the entry script. Third party packages stay
as bytecode, because replacing function objects across an entire dependency tree
trades a lot of compatibility for speed you usually do not need there.

A function is compiled when it has a plain positional signature, no closure, no
`*args` or `**kwargs`, is not a generator or coroutine, and contains no
`try`/`except`/`with`. Everything else keeps its bytecode. `-v` prints what was
compiled and, at the end, how many functions were left alone and why.

Inside a compiled function the generator handles arithmetic and comparisons on
every numeric type, bit operations, `is` and `in`, unary operators, attribute
load and store, subscript load and store including slices and augmented forms
like `a[i] += 1` on arrays, rows and int keyed dicts, list, tuple, dict and
string construction, dict comprehensions, f-strings with conversions and format
specs, `for`/`while`/`break`/`continue`, sequence unpacking, calls, and
recursion. On the benchmark module, 26 of 27 functions compile.

## What gets bundled

Analysis starts at the entry script and follows imports. Resolution is top level
only: once a name resolves to a package, the whole package tree is taken, which
is what makes packages that import lazily or by string work without per package
hooks. Alongside a package Compyler also takes every `.dist-info` that claims the
name, so `importlib.metadata` works even when two distributions provide the same
top level module, and any sibling `<name>.libs` directory from delvewheel.

Native dependencies are resolved generically. Every `.pyd` and `.dll` is walked
for its PE imports; each import is looked for next to the importing module, then
in the registered DLL directories, and finally against an index of every `.dll`
below the collection roots. That last step is what finds libraries a package
keeps outside itself, such as `pywin32_system32`. Whatever directory a dependency
lands in is registered for `os.add_dll_directory` at startup, and each source
file is copied exactly once no matter how many modules import it, so a 19 MB
OpenBLAS shared by three numpy subpackages is stored once rather than three
times.

Data that no import and no PE header points at needs a rule, because nothing in
the file system says it is required. `DATA_RULES` in `src/compiler/main.c` is
that registry: one row names a trigger module, a directory under the Python
root, where it should land, which subdirectory prefixes to take, and which
environment variable should point at each. The only row today is Tcl/Tk, which
`_tkinter` needs and which lives outside `sys.path` entirely. Nothing in the row
is version specific, so it picks up `tcl8.6` or `tcl9.0` without changing, and
adding another library of this shape is one row rather than new code. Anything
else is reachable with `--add-data SRC;DEST` and `--hidden-import`.

Three rules keep the closure from exploding: imports inside
`if __name__ == "__main__":` are ignored, `tests/` directories are collected but
not scanned, and build tooling such as `setuptools`, `pip`, `pytest` and CPython's
own `test` package is excluded by default. The exclude list holds build time
tools only. Modules that libraries import at run time are never on it, because
excluding one breaks the library rather than shrinking the build: `scipy`
imports `pydoc`, and excluding it cost a `ModuleNotFoundError` in exchange for
230 KB.

Unresolved imports are listed at the end of the build.

## Runtime layout

```
<cache>/
  python312.dll  python3.dll  vcruntime140.dll
  python312._pth          sys.path, exactly, in isolated mode
  app/                    entry module, your modules, _compyler_native.pyd
  Lib/                    stdlib .pyc
  Lib/site-packages/      third party packages
  DLLs/                   extension modules
```

A real directory layout is why C extensions behave: `__file__` resolves, data
files sit where packages expect them, and CPython's own loader does the
importing. `pythonXY._pth` puts the interpreter in isolated mode so the host
machine's `PYTHONPATH`, user site-packages and `sitecustomize` cannot leak in.

`sys.frozen`, `sys._MEIPASS` and `sys._COMPYLER_ROOT` are set. `multiprocessing`
spawn works without the usual `freeze_support()` boilerplate.

## Testing

```powershell
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
```

67 checks, all differential where that is possible: the compiled executable's
output must match the interpreter's byte for byte, on the same source.

- `tests/suite/semantics.py` exercises every operation the code generator
  supports, roughly a thousand results covering both operand orders across int,
  negative int, big int, float, mixed, string, list, tuple and dict, plus every
  error path. Run three ways: native, `--no-native`, and `-O`.
- `tests/suite/bits.py`, 864 results, covers every shift count from 0 to 65 on
  positive, negative, 64 bit boundary and arbitrary precision operands, masks
  above 2^31, promotion to big int, and the `ValueError` on a negative shift.
- `tests/suite/mathfns.py`, 1388 results, covers all 23 math intrinsics over 30
  operands including the domain errors, `OverflowError` on `exp`, and the case
  where `math.sqrt` is monkeypatched at runtime and the guard has to fall back.
- `tests/suite/strdict.py`, 27 results, covers the typed string and dict
  engines: formatted string length and content across signs and magnitudes,
  `ord` on positive and negative indices, out of range reads, non ASCII
  fallback, dict growth across rehash, negative keys, overwrite, `KeyError`,
  and dicts recreated in loops. Also run under `--wrap-int`.
- `tests/suite/arrays.py`, 35 results, covers the typed array engine: negative
  and out of range indices, write back visibility, aliased parameters, mixed and
  bigint lists falling back, bool elements, list subclasses, arrays recreated in
  loops, the cross function heapsort, sift and queens shapes, row aliases with
  writes visible through both names, and a conditional expression choosing
  between an int rowed and a float rowed list.
- `tests/suite/dictslice.py`, 285 results, covers dict literals, comprehensions,
  unhashable and missing keys, and slicing of list, str and tuple across
  negative and out of range bounds plus slice assignment.
- `tests/suite/bounds.py`, 46 results, attacks the loop guard bounds elision:
  an index mutated inside an inner loop, a bound larger than the array so the
  hoisted guard must bail, negative and zero bounds, a bound reassigned mid
  loop, `while 1` with a `break` guard, the sieve stride shape, nested list
  rows, empty arrays, exact length edges, arrays recreated between loops,
  index and bound parameters that are never reassigned, negative index
  parameters that must still raise or wrap exactly, range loops over arrays
  with negative starts, oversized stops, empty and strided and backward
  ranges, a loop variable reassigned inside a range loop, byte narrowed
  arrays mixed with wide ones in the same function, and accumulator loops
  whose sums and products genuinely overflow 64 bits mid loop and must come
  back as the interpreter's big integers.
  The elision must never fire where any of that could reach an element out of
  range, and the differential harness proves the output identical either way.
- `tests/samples/fstr.py` covers f-string conversions, format specs, nested
  specs and the `__format__` protocol.
- `tests/samples/gui_tk.py` builds a real tkinter window with ttk widgets, a
  canvas and font enumeration, and checks it initialises inside the bundle.
- `tests/suite/builtins1.py`, 153 results, covers len, ord, abs, chr, float,
  int, min and max over every operand shape including the dunder protocols,
  NaN and signed zero ordering through the typed min and max, mixed type
  arguments falling back, and the case where the name is shadowed in module
  globals.
- `tests/samples/datafiles.py` runs 56 checks against libraries whose data
  does not travel with their imports: the certifi PEM bundle, pygments
  lexers and styles, jinja2, scipy over its own shared libraries, pywin32
  over pywin32_system32, cryptography, cffi, PIL image codecs, lxml schema
  validation, importlib metadata and eighteen text codecs.
- `tests/samples/cext.py` runs 63 checks across numpy, sqlite3, lxml, PIL,
  zlib, bz2, lzma, hashlib, struct, array, json, pickle, re, decimal and socket.

The rest covers packaging, numpy plus scapy plus requests, multiprocessing,
every flag, the 3.11 and 3.13 interpreters alongside the main 3.12, and cache
reuse. `tests\corpus.ps1` runs the differential corpora against every
interpreter installed on the machine, and `tests\stress.ps1` builds two
simulated real projects out of live virtual environments, one made with
`uv venv` and one with stdlib `venv`.

`tests\benchmark.ps1` is separate and produces the Windows side of the tables
above: 22 compute workloads across six runners, whole process CPU, memory,
page fault, disk and handle counts, startup and footprint for every
compression mode, and application level comparisons. `tests\codon_bench.ps1`
runs the full eight runner window including Codon and the WSL2 C++ calibration
build, and cross checks every integer result against CPython.

## Limits

- Windows x64 only. The produced exe targets the CPython version it was built
  against. Native compilation requires 3.11 to 3.13; on other versions the build
  falls back to bytecode automatically.
- Static analysis cannot see imports built from computed strings. Use
  `--hidden-import`.
- `-OO` strips docstrings, which breaks libraries that read `__doc__` at runtime,
  numpy among them. Prefer the default or `-O`.
- The first run of a one file build pays the extraction cost. Use `--onedir` when
  that matters more than shipping a single file.
- Windows Defender heuristics sometimes flag self extracting executables,
  including this one and PyInstaller's. Signing the output, or an exclusion for
  your build directory, is the practical answer. `--onedir` does not trip it.

## Layout

```
src/common/    archive format, filesystem and compression helpers, C API binding
src/compiler/  driver, import scanner, PE walker, packer
src/nc/        bytecode to C code generator, tagged value runtime, cc driver
src/stub/      runtime bootstrap that becomes every produced exe
build.bat      builds both binaries with MSVC
tests/run_tests.ps1   the 67 check suite
tests/corpus.ps1      differential corpora against every installed interpreter
tests/stress.ps1      big project simulations built from live venvs
tests/benchmark.ps1   compute, memory, cpu, io and startup measurements
tests/measure.ps1     process resource probe used by the benchmark runner
tests/suite/          differential corpora: semantics, bits, math, dict and slice
tests/samples/        tkinter, c extension and f-string samples
tests/bench/          the same 22 workloads in python, c++, go and codon
```
