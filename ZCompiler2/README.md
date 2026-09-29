# Z Language Compiler 0.2.0 MVP
A Python-free C++17 compiler frontend and C++ transpilation backend. It performs lexing, parsing, AST construction, semantic checks, C++ generation, and native compilation. The produced program does not contain an interpreter or require Python. A C++ toolchain is still required at build time. Python independence and native-toolchain independence are different goals.

## Build
Linux/macOS: `cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure`

Windows x64: `cmake -S . -B build-x64 -A x64` then `cmake --build build-x64 --config Release`.
Windows x86: `cmake -S . -B build-x86 -A Win32` then `cmake --build build-x86 --config Release`.
Use a Visual Studio Developer Command Prompt for MSVC. Clang and MinGW are also supported. The MVP builds for the host architecture; one binary does not cross-compile x86/x64.

## CLI
`zlang build hello.z -o hello.exe`, `--emit-cpp [file]`, `--cxx path`, `--quiet`, `--help`, `--version`.

## Language
Types: `box`, `line`, `bool`, `cpx`. Statements: declaration, assignment, print, if/else-if/else, while, `for i in +(N)`, return, calls. Newline or semicolon terminates a statement. Newlines inside parentheses are ignored. Identifiers are ASCII. Strings and comments may contain UTF-8. Source BOM is accepted. Large Korean text is not embedded in the compiler binary; only user literals flow into generated code. This keeps compiler size independent of localization payload.

`box` uses int64/double in the generated runtime; division returns double and checks zero. `cpx` uses `std::complex<double>` and currently supports cpx-to-cpx arithmetic. Function parameters use dynamic `ZValue`; static parameter type syntax is deferred.

## Toolchain and UTF-8
MSVC receives `/utf-8`. Generated Windows programs call `SetConsoleOutputCP(CP_UTF8)` and `SetConsoleCP(CP_UTF8)`. Compiler process launch uses `CreateProcessW`; POSIX uses a quoted command.

## Limits
No LLVM, PE/COFF writer, classes, arrays, pointers, structs, modules, debug info, package manager, or cross-architecture driver. Arithmetic overflow is the host C++ behavior in this MVP. Return inference across forward calls is conservative. Windows x86/x64 source paths are implemented but must be validated on actual Windows CI.
