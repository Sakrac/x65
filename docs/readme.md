# x65 Assembler

x65 is an open source 6502 series assembler that supports object files,
linking, fixed address assembling and a relocatable executable.

Assemblers have existed for a long time and what they do is well documented,
x65 tries to accomodate most expectations of syntax from Kick Assembler (a
Java 6502 assembler) to Merlin (an Apple II assembler).

For debugging, dump_x65 is a tool that will show all content of x65 object
files, and x65dsasm is a disassembler intended to review the assembled
result. 

## Noteworthy features:

* Code with sections, object files and linking or single file fixed
  address, or mix it up with fixed address sections in object files.
* Assembler listing with cycle counting for code review.
* Export multiple binaries with a single link operation.
* C style scoping within '{' and '}' with local and pool labels
  respecting scopes.
* Conditional assembly with if/ifdef/else etc.
* Assembler directives representing a variety of features.
* Local labels can be defined in a number of ways, such as leading
  period (.label) or leading at-sign (@label) or terminating
  dollar sign (label$).
* String Symbols system allows building user expressions and macros
  during assembly.
* Reassignment of symbols and labels by default.
* No indentation required for instructions, meaning that labels can't
  be mnemonics, macros or directives.
* Supporting the syntax of other 6502 assemblers (Merlin syntax
  requires command line argument, -endm adds support for sources
  using macro/endmacro and repeat/endrepeat combos rather
  than scoeps).
* Apple II GS executable output.

## Command Line Options

See [command_line_options.md]

## Directives

See [directives.md]

## Macro examples

See [macro_samples.md]