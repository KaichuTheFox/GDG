# Architecture
Source -> UTF-8 lexer -> recursive-descent parser -> owning AST (`unique_ptr`) -> semantic analyzer and nested symbol scopes -> pluggable `CodeGenerator` -> self-contained C++17 runtime -> host native compiler. Platform process creation is isolated in `Process.cpp`. Direct LLVM and native backends can implement `CodeGenerator` later.
