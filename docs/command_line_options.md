# Command Line Options for x65

These options control how x65 assembles, links, and emits output from the command line. The parser accepts option names case-insensitively, so you can mix uppercase and lowercase as you like.

A few common examples:

```bat
x65 demo.s demo.prg
x65 demo.s demo.prg -lst=lst\demo.lst -symfull demo.sym
x65 src\link.s build\demo.prg -srcdbg=build\demo.dbg -vice build\demo.vs
```

## Include paths and defines

- `-i(path)` adds a folder to the include search path.
- `-D(label)[=value]` defines a label with an optional value; if no value is supplied it is defined as `1`.

## Listing and debug output

- `-lst` / `-lst=(file.lst)` generates listing text on stdout or in a file.
- `-tsl=(file)` generates a TASS-style listing file.
- `-tl=(file)` generates labels in TASS style.
- `-srcdbg` / `-srcdbg=(file.dbg)` emits source-level debugging data for object files or linked output.
- `-sect` displays the sections that were loaded and built.

## CPU and syntax

- `-cpu=6502/6502ill/65c02/65c02wdc/65816` selects the target CPU.
- `-acc=8/16` sets the 65816 accumulator mode at startup.
- `-xy=8/16` sets the 65816 index-register mode at startup.
- `-endm` makes macros end with `ENDM` or `ENDMACRO` instead of using `{ ... }` blocks.
- `-merlin` enables Merlin syntax mode.
- `-kickasm` enables Kick Assembler syntax mode (in progress).

## Output format

- `-org=$2000` or `-org=4096` forces the first non-specific section to start at the given address.
- `-bin` produces a raw binary.
- `-c64` produces a C64 binary with the standard two-byte load address.
- `-a2b` produces an Apple II DOS 3.3 binary.
- `-a2p` produces an Apple II ProDOS binary.
- `-a2o` produces an Apple II GS/OS relocatable executable.
- `-mrg` forces section merging when used with `-a2o`.
- `-obj (file.x65)` writes an x65 object file for later linking.
- `-sym (file.sym)` writes a symbol file.
- `-symfull (file.sym)` writes a fuller symbol map.
- `-vice (file.vs)` exports a VICE monitor command file including symbols.
- `-refs` shows label dependencies before linking.
- `-opcodes` / `-opcodes=(file.s)` dumps the supported opcodes for the selected CPU.
- `-xrefimp` makes `IMPORT` behave like `XREF` and `EXPORT` behave like `XDEF`.

