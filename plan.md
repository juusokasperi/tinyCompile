## Basic flow

1. Open input file, fstat its length, call mmap, close fd
2. Allocate memory arena
3. Lex
4. AST
5. JIT

## Notes

Possibly do StringBuilder for building lines?
