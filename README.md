# Compyler

Compyler compiles Python programs into single native Windows executables.
Your own functions are translated from CPython bytecode into machine code;
everything else, including packages with C extensions, ships inside the exe
and runs on the bundled interpreter. The output is byte for byte what CPython
prints, arbitrary precision integers included.

```bat
compyler main.py
main.exe
```

On a 22 workload compute suite it runs 30x faster than CPython, 1.4x faster
than the same workloads written in C++ and built with g++ -O2, while staying a drop in
replacement: no dialect, no porting, numpy and scapy and tkinter come along.
A warm start takes 15 ms, less than `python.exe` itself.

## Building

Needs Visual Studio with the C++ workload. LLVM's clang-cl is picked up
automatically when installed and is worth about 15 percent.

```bat
build.bat
```

Produces `bin\compyler.exe`, `bin\stub.exe` and `bin\cpyrt.h`. Keep them
together.

## Usage

```
compyler <script.py> [options]
```

The defaults are already the right flags: import graph pruning, maximum
compression, native code generation, and every check Python's semantics
demand. `compyler --help` lists the rest. `docs\DETAILS.md` explains how the
compiler works, the benchmark methodology, and the honest numbers, including
the cases it loses.

## Requirements

Windows x64. CPython 3.11, 3.12 or 3.13. Linux is not supported yet.

## Testing

```
powershell -ExecutionPolicy Bypass -File tests\run_tests.ps1
powershell -ExecutionPolicy Bypass -File tests\corpus.ps1
powershell -ExecutionPolicy Bypass -File tests\stress.ps1
```

Everything is differential: the compiled executable's output must match the
interpreter's byte for byte, on the same source, or the test fails.

## License

Apache 2.0, copyright Pygot. If you publish a project built with Compyler, a
visible mention of Compyler is appreciated; see `NOTICE`.
