XCC
===

[![Action Status](https://github.com/tyfkda/xcc/workflows/AllTests/badge.svg)](https://github.com/tyfkda/xcc)
[![Open in Gitpod](https://gitpod.io/button/open-in-gitpod.svg)](https://gitpod.io/#https://github.com/tyfkda/xcc)

  * C compiler for [XV6 (64bit)](https://github.com/tyfkda/xv6)
    * Also work on Linux
  * Assembler
  * Output ELF64 (x86-64) file format
  * Register allocation (Linear scan)
  * Self hosting


### Requirements

  * Linux
  * C compiler (gcc or clang)
  * make


### Build

```sh
$ make
```

Generated files:

  * `xcc`: Compiler entry
  * `cpp`: Preprocessor
  * `cc1`: C compiler
  * `as`:  Assembler
  * `ld`:  Linker


### Usage

```sh
$ ./xcc -o hello examples/hello.c
$ ./hello
```

#### Command line options

  * `-o <filename>`: Set output filename (default: `a.out`)
  * `-I <path>`:     Add include path
  * `-D <label>(=value)`:  Define macro
  * `-S`:            Output assembly code
  * `-E`:            Preprocess only
  * `-c`:            Output object file
  * `-nodefaultlibs`:  Ignore libc
  * `-nostdlib`:  Ignore libc and crt0


### TODO

  * Optimization
  * Archiver


### Missing features

C compiler:

  * Bit field


### WebAssembly

[WCC](https://github.com/tyfkda/xcc/blob/main/wasm/README.md): Compile C to WebAssembly.


### Reference

  * [低レイヤを知りたい人のためのCコンパイラ作成入門](https://www.sigbus.info/compilerbook)
  * [rui314/9cc: A Small C Compiler](https://github.com/rui314/9cc)
