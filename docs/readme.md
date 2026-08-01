# x65 Assembler

x65 is an open-source 6502-family assembler that supports object files,
linking, fixed-address assembly, and relocatable executables. The current
implementation targets 6502, 65C02, 65C02WDC, and 65816 sources and includes
support for multiple syntax styles, including Kick Assembler and Merlin. It
also supports listing generation, source-level debug output, opcode dumps,
VICE monitor exports, and dependency reporting through the command-line
interface.

![x65](x65.png)

For debugging, dump_x65 can inspect the contents of x65 object files, and
x65dsasm is a separate disassembler intended to review the assembled result.

x65 is built in sync with [IceBro Lite](https://github.com/sakrac/IceBroLite), but any debugger will work if needed.

## Noteworthy features:

* Code with sections, object files, and linking, or single-file fixed-address assembly.
* Assembler listing with cycle counting for code review.
* Export multiple binaries with a single link operation.
* C-style scoping with `{` and `}` and local/pool labels that respect scopes.
* Conditional assembly with `if`, `ifdef`, `else`, and related directives.
* A broad set of assembler directives for exports, imports, labels, strings, structs, enums, and conditional assembly.
* Local labels can be defined with leading period, leading at-sign, trailing dollar sign, or Merlin-style colon prefixes.
* String symbols for building user expressions and macros during assembly.
* Reassignment of symbols and labels by default.
* Support for Merlin syntax with `-merlin` and `-endm`.
* Apple II GS/OS executable output.

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
