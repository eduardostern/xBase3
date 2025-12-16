# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

xBase3 is a dBASE III+ compatible database system written in C. It provides:
- Full dBASE III+ interpreter with programming constructs
- Standard .DBF file format support
- Custom .XDX B-tree index format
- Cross-platform support (MacOS, Linux)

## Build Commands

```bash
# Configure (Debug build)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Configure (Release build)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Run
./build/xbase3

# Run all tests
ctest --test-dir build

# Run single test
./build/tests/test_dbf
./build/tests/test_lexer
./build/tests/test_parser
./build/tests/test_expr

# Clean build
rm -rf build && cmake -B build && cmake --build build
```

## Architecture

```
REPL (main.c)
  └── Parser (parser.c) ← Lexer (lexer.c)
        └── Interpreter
              ├── Expression Evaluator (expr.c) ← Functions (functions.c)
              ├── Command Executor (commands.c)
              └── Variable Manager (variables.c)
                    └── Storage Layer
                          ├── DBF Engine (dbf.c)
                          └── XDX Index Engine (xdx.c)
```

## Key Source Files

- `src/main.c` - REPL loop, command dispatch
- `src/lexer.c` - Tokenizer for dBASE commands
- `src/parser.c` - Recursive descent parser, builds AST
- `src/ast.c` - AST node definitions and memory management
- `src/expr.c` - Expression evaluation (arithmetic, string, logical)
- `src/functions.c` - Built-in functions (TRIM, UPPER, STR, VAL, etc.)
- `src/variables.c` - Memory variable management (PUBLIC, PRIVATE)
- `src/commands.c` - Command implementations (USE, LIST, APPEND, etc.)
- `src/dbf.c` - DBF file format read/write
- `src/xdx.c` - B-tree index implementation

## Coding Conventions

- C11 standard, compiled with `-Wall -Wextra -pedantic`
- All strings are fixed-length, space-padded (dBASE style)
- Commands and field names are case-insensitive (use `strcasecmp`)
- Dates stored as YYYYMMDD strings internally
- Numeric values use `double` internally, formatted on output
- Error recovery uses `setjmp/longjmp` for REPL continuity
- Memory: careful cleanup, AST nodes freed after execution

## DBF File Format

Header (32 bytes) + Field descriptors (32 bytes each) + 0x0D terminator + Records + 0x1A EOF

Field types: C (Character), N (Numeric), D (Date), L (Logical), M (Memo)

## XDX Index Format

512-byte header with magic "XDX\0", followed by B-tree nodes. Standard B-tree with configurable order.

## dBASE Commands Supported

Navigation: GO, SKIP, LOCATE, CONTINUE, EOF(), BOF(), RECNO()
Data: USE, LIST, DISPLAY, APPEND, DELETE, RECALL, PACK, REPLACE
Output: ?, ??
Variables: STORE, PUBLIC, PRIVATE, RELEASE
