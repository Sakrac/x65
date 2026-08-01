# x65 Assembler

x65 is a compact, flexible assembler for the 6502 family that can move from a tiny one-file demo to a larger linked project without needing a separate runtime environment. It targets 6502, 65C02, 65C02WDC, and 65816 sources and can emit C64, Apple II, and raw binary outputs.

![x65](x65.png)

x65 is especially handy when you want to build quickly and keep the workflow simple. It supports fixed-address assembly, section-based linking, listings with cycle counts, source-level debug output, symbol maps, opcode dumps, VICE exports, and dependency reporting.

x65 is built in sync with [IceBro Lite](https://sakrac.github.io/IceBroLite/), but any debugger will work if needed.

## Why x65 is enjoyable to use

- A single executable workflow: assemble directly from source with one command.
- Flexible build styles: fixed-address assembly, object files, or a mix of both.
- Useful tooling: listings, debug information, symbol maps, opcode dumps, and VICE output.
- Friendly syntax features: local labels, string symbols, recursive macros, and conditional assembly.
- Broad compatibility: Kick Assembler, Merlin, and ca65-style scoping are all supported.

## A quick example

A small program can be assembled in one step:

```asm
; demo.s
org $0801

start:
    lda #$01
    sta $d020
    rts
```

```bat
x65 demo.s demo.prg
```

If you want a more detailed build, add a listing and symbol map:

```bat
x65 demo.s demo.prg -lst=lst\demo.lst -symfull demo.sym
```

## From one file to a linked project

x65 also shines when you split work into several build units and link them together. A simple link script can place each section where you want it and pull in the object files you already built:

```asm
; link.s
SECTION Boot, code
org $0801

SECTION Code, code
org $0a00

SECTION Data, data
org $4000

SECTION BSS, bss
org $c000

incobj "obj/startup.x65"
incobj "obj/main.x65"
incobj "obj/data.x65"
```

```bat
x65 src\link.s Project.prg -srcdbg=Project.dbg -lst=lst\Project.lst -symfull Project.sym -vice Project.vs
```

That is the same basic pattern used in many batch-driven build pipelines: compile pieces, link them together, and emit the outputs you need for testing or debugging.

## A build script example

Many projects use x65 from a small batch file so the assembler, linker, symbols, and debug files all happen in one repeatable step:

```bat
@echo off
setlocal
mkdir obj lst build 2>nul

x65 src\main.s obj\main.x65 -lst=lst\main.lst -srcdbg
x65 src\link.s build\demo.prg -srcdbg=build\demo.dbg -lst=lst\demo.lst -symfull build\demo.sym -vice build\demo.vs
```

That makes it easy to rebuild a project, inspect listings, and keep the output folder tidy while you iterate.

## Example command line

To build a Commodore 64 prg file from a single source file with a listing file:

```bat
x65 src\cart\cartmove.s obj\cartmove.prg -symfull obj\cartmove.sym -lst=lst\cartmove.lst
```

To build a linkable object file with source debug information and a listing file:

```bat
x65 src\Game.s -lst=lst\Game.lst -obj obj\Game.x65 -srcdbg
```

## Command Line Options

Controls the assembler for the entire file.

See [Command Line Options](command_line_options.md)

## Directives

Controls the assembler on a line-by-line basis.

See [Directives](directives.md)

## Eval Functions

Functions that return values for use in expressions.

See [Eval Functions](eval_functions.md)

## Macro examples

Examples and notes for the included x65macro.i file.

See [Macro Samples](macro_samples.md)

## List of Errors

See [Error List](errors.md)
