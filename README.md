# tinyCompile

**tinyCompile** is a lightweight, self-contained C compiler written in C. It implements a complete compilation pipeline, from lexical analysis to x86-64 machine code generation and executes the compiled code immediately via JIT (Just-In-Time) compilation.

Designed as a study in compiler construction, it features a custom linear IR, a linear-scan register allocator, and a custom memory arena allocator.

## Features

* **JIT Execution:** Compiles C source directly into memory (`mmap` with `PROT_EXEC`) and executes it immediately.

* **Data Types:** Currently supports `int64` (signed 64-bit integers).

* **Control Flow:** Full support for `if/else` conditionals and `while` loops.

* **Functions:** Support for function declarations, recursive calls, and stack frame management.

* **Operators:** Arithmetic (`+`, `-`, `*`, `/`) and logical/comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`).

* **Memory Management:** Built on a custom `memarena` implementation for efficient, region-based memory management with ASAN poisoning support.

* **Error Reporting:** Colored, contextual error messages pointing to specific file lines and columns.



## Architecture

tinyCompile follows a classic multi-pass compiler design.

1. **Lexer:** Tokenizes source input, handling whitespace and comments.
2. **Parser:** A recursive descent parser that constructs an Abstract Syntax Tree (AST).
3. **Semantic Analysis:** Performs scope resolution, variable declaration checking, and type validation.
4. **IR Generation:** Lowers the AST into a linear Intermediate Representation (IR) using virtual registers.
5. **JIT Backend:**
	* **Register Allocation:** Uses a Linear Scan allocator to map virtual registers to physical x86-64 registers (or spills to stack).
	* **Encoding:** Emits binary x86-64 machine code (prologues, ALUs, jumps, epilogues).
	* **Linking:** Resolves function calls and jumps.

## Example Usage

### Input Code (`tests/success/10_factorial.c`)

tinyCompile supports standard C syntax for the implemented features:

```c
int factorial(int n) {
    if (n < 2) {
        return 1;
    }
    return n * factorial(n - 1);
}

int main(void) {
	return (factorial(5));
}
```

### Running the Compiler

The compiler parses the file, generates the IR, compiles it to machine code, and runs the `main` function.

```bash
$ ./tinyCompile tests/success/10_factorial.c
╔══════════════════════════════════════════╗
║  _   _                                   ║
║ | |_(_)_ __  _   _                       ║
║ | __| | '_ \| | | |  tinyCompile v0.1    ║
║ | |_| | | | | |_| |	Build: release     ║
║  \__|_|_| |_|\__, |                      ║
║               |___/                      ║
╚══════════════════════════════════════════╝

[01] INITIALIZATION........................
  > LOAD SOURCE: tests/success/10_factorial.c (143 bytes)

[02] PARSING...............................
  > parsing tests/success/10_factorial.c

[03] SEMANTICS.............................
  > collecting function declarations
  > analyzing function bodies
   0 | tests/success/10_factorial.c

[04] JIT...................................
  :: compiling symbol 'factorial'

  ╔ IR DUMP (factorial) ══════════════════╗
  ║ 0000 | %v1 = CONST 2                  ║
  ║ 0001 | %v2 = LT %v0, %v1              ║
  ║ 0002 | JZ %v2, L0                     ║
  ║ 0003 | %v3 = CONST 1                  ║
  ║ 0004 | RET %v3                        ║
  ║ 0005 | L0:                            ║
  ║ 0006 | %v4 = CONST 1                  ║
  ║ 0007 | %v5 = SUB %v0, %v4             ║
  ║ 0008 | ARG 0 = %v5                    ║
  ║ 0009 | %v6 = CALL factorial           ║
  ║ 0010 | %v7 = MUL %v0, %v6             ║
  ║ 0011 | RET %v7                        ║
  ╚═══════════════════════════════════════╝
  :: compiling symbol 'main'

  ╔ IR DUMP (main) ═══════════════════════╗
  ║ 0000 | %v0 = CONST 5                  ║
  ║ 0001 | ARG 0 = %v0                    ║
  ║ 0002 | %v1 = CALL factorial           ║
  ║ 0003 | RET %v1                        ║
  ╚═══════════════════════════════════════╝
```

## Internals: Intermediate Representation

tinyCompile generates a readable IR dump for debugging.

## Roadmap

* [x] Integer arithmetic and logic
* [x] Control flow (if/while)
* [x] Function calls and recursion
* [ ] Support for `char` and `void` types 

* [ ] Pointer arithmetic
* [ ] Constant folding optimization
* [ ] Struct support

## Building

Ensure you have a C compiler (GCC/Clang) and Make installed.

```bash
# Clone the repository
git clone https://github.com/juusokasperi/tinyCompile

# Build
make
```
