# X65 Directives

Directives control the assembler on a line-by-line basis and cover conditional assembly, exports, imports, object-file generation, linking, scoping, and more. Directives are case-insensitive and may be prefixed with a dot, which makes them convenient to use in both compact and more readable source files.

A small example:

```asm
SECTION Code, code
org $0800

LABEL start
lda #$01
sta $d020
rts
```

	.rept 8 { dc.b 1<<rept }

is equivalent to:

	REPT 8 { dc.b 1<<rept }

Some directives change behavior based on [command line options](command_line_options.md), such as `-endm`, `-xrefimp`, `-kickasm`, and `-merlin`.

### CPU, PROCESSOR

Assemble for this target. The current parser accepts:

* `6502`
* `6502ill` (illegal opcodes)
* `65C02`
* `65C02WDC` (adds WDC-specific instructions such as `STP`, `WAI`, `BBR`, and `BBS` forms)
* `65816`

Example:

	CPU 6502ill

### PC, ORG

Assemble as though the current section were loaded at this address.

### LOAD

If applicable, instruct the output format to load the generated binary at this address.

### EXPORT

Export the current section or disable export. When the `-xrefimp` option is used, `EXPORT` is treated as `XDEF` instead.

### SECTION, SEG, SEGMENT

Start a new section that can later be assigned an address during the link step, or use its own load address. BSS and zero-page sections are handled differently from normal data sections, and sections can be exported separately with `EXPORT`.

### MERGE

Merge named sections in the order listed.

### LINK

Place sections with this name at a given address. This is only valid for fixed-address sections.

### XDEF

Externally declare a symbol.

### XREF

Reference an external symbol. With `-xrefimp`, `IMPORT` is treated as `XREF` instead.

### INCOBJ

Read in an object file produced previously with `-obj`.

### ALIGN

Align the current address to a multiple of the given value. This works at the start of a section or within a fixed-address section.

### MACRO, MAC

Create a macro. With `-endm`, the macro body ends with `ENDM` or `ENDMACRO`; otherwise it is delimited with braces.

	; standard macro usage
	MACRO ldaneg(x) {
		lda #-x
	}

	; -endm macro usage
	MACRO ldaneg(x)
		lda #-x
	ENDM

### FUNCTION

A user-defined function is a one-line expression that can be used like a macro, but it returns a single integer value rather than emitting bytes.

	FUNCTION alignto(address, alignment) (address + alignment-1) & (~alignment)

Functions must resolve at the point of use; unresolved symbols cause an error.

### EVAL, PRINT, ECHO

Print an expression to stdout during assembly:

	EVAL Current Address: *

	EVAL Checking referenced function, Should be 0: .referenced(test_stack)
	EVAL Checking defined function, Should be 1: .defined(test_stack)

### DC, DV

Declare constants or values. The directive accepts a size suffix: `.b` for byte, `.w` for word, `.t` for triple, and `.l` for long. The default size is one byte.

	DC.B $20, *-Test

### BYTE, BYTES

Same as `DC.B`.

### WORD, WORDS

Same as `DC.W`.

### LONG

Same as `DC.L`.

### TEXT

Emit text to the output. The character order can be altered through a string symbol.

	STRING FontOrder = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&*"
	TEXT [FontOrder] "HELLO #1!"

### HEX

Emit hexadecimal bytes from a string of hex digits. The number of digits must be even, and the assembler reports an error if it is not.

	HEX 0A1B2C3D

### INCLUDE

Assemble another file in place.

### INCBIN

Insert another file as raw binary data at the current address.

### INCSYM

Load symbols from a `.sym` file:

	INCSYM "Main.Sym"

Symbols can also be selected by a list on the same line:

	INCSYM InitMain, UpdateMain, ShutdownMain, "Main.Sym"

### INCDIR

Add a folder to the include search path.

### IMPORT

A generic form of include-style directives with optional type hints:

	IMPORT "data.bin"
	IMPORT binary "data.bin"
	IMPORT source "defines.i"
	IMPORT c64 "main.prg"
	IMPORT text "text.txt"

With `-xrefimp`, `IMPORT` behaves as `XREF` instead.

### CONST

Declare a symbol as const. Assigning it again causes an error.

	CONST VICBank = $4000

The constness of a symbol can be tested with `IFCONST` or the `CONST()` eval function.

### LABEL

Create a mutable label explicitly. This is effectively a non-const label declaration.

### STRING

Declare a string symbol. Strings can be used for ordered text in `TEXT` directives or as assembler source.

### UNDEF

Remove a symbol.

	like_bananas = 1
	UNDEF like_bananas

### LABPOOL, POOL

Create a pool of addresses to assign as labels dynamically. This acts as a linear stack allocator for temporary storage and is deallocated when the enclosing scope ends if declared as local.

### IF, IFDEF, IFNDEF, IFCONST, IFBLANK, IFNBLANK, ELSE, ELIF, ENDIF

Use conditional assembly to include or exclude blocks of source. These directives are available in the same family as `IF` and `IFDEF`.

### STRUCT, ENUM

Declare structured or enumerated label groups.

### REPT, REPEAT

Repeat the enclosed block a given number of times. `REPEAT` is accepted as an alias for `REPT`, and Merlin syntax accepts `LUP` as a repeat alias.

### A16, A8, XY16, XY8, I16, I8

Set the accumulator or index register size for 65816 mode.

### DUMMY, DEND, DUMMY_END

Start and end a dummy section. The `DUMMY_END` name is accepted as a synonym for ending a dummy section, and Merlin syntax also recognizes `DUM` and `DEND`.

### DS, RES

Reserve storage or rewind the address if the count is negative.

### SCOPE, ENDSCOPE

Start and end a ca65-style scope.

### PUSH, PULL

Push and pull values for variable symbols.

### ABORT, ERR

Stop the assembly and report an error.

### Merlin-specific aliases

When Merlin syntax is enabled, the parser also accepts `MX`, `STR`, `DA`, `DW`, `ASC`, `PUT`, `DDB`, `DB`, `DFB`, `HEX`, `DO`, `FIN`, `EJECT`, `OBJ`, `TR`, `END`, `REL`, `USR`, `DUM`, `DEND`, `LST`, `LSTDO`, `LUP`, `SAV`, `DSK`, `LNK`, `XC`, `ENT`, `EXT`, `ADR`, `ADRL`, and `CYC`.

When Merlin syntax is enabled, the parser also accepts directives such as `MAC`, `LUP`, `DB`, `DW`, `DDB`, `ASC`, `PUT`, `DUM`, `DEND`, `LST`, `LSTDO`, `USR`, `LNK`, `XC`, `ENT`, `EXT`, `ADR`, `ADRL`, `CYC`, and `SAV`.
