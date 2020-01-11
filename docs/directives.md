# X65 Directives

Directives are commands that control the assembler and include controls for conditional assembly, exporting multible binary files, creating linkable object files etc.

The directives are case insensitive and can be preceeded by a dot

	.rept 8 { dc.b 1<<rept }

is the same as

	REPT 8 { dc.b 1<<rept }

Some directives change behavior based on [command line options](command_line_options.md), such as -endm, -xrefimp, -kickasm and -merlin.


### CPU, PROCESSOR

Assemble for this target, valid options are:
 * 6502
 * 6502ill (illegal opcodes)
 * 65c02
 * 6502wdc (adds 18 extra instructions: stp, wai, bbr0-7 & bbs0-7)
 * 65816

example:

	cpu 6502ill

### PC, ORG

Assemble as if loaded at this address

### LOAD

If applicable, instruct to load at this address

### EXPORT

Export this section or disable export Note that with the -xdefimp command line option this means XDEF instead and the EXPORT directive is not available.

### SECTION, SEG, SEGMENT

Enable code that will be assigned a start address during a link step, or alternatively its own load address. BSS and ZP sections will not be included in the binary output, and sections can be separately exported using the EXPORT directive.

### MERGE

Merge named sections in order listed

### LINK

Put sections with this name at this address (must be ORG / fixed address section)

### XDEF

Externally declare a symbol. When using the command line option -xdefimp EXPORT means the same thing.

### XREF

Reference an external symbol. When using the command line option -xdefimp IMPORT means the same thing.

### INCOBJ

Read in an object file saved from a previous build (that was assembled using the -obj command line option).

### ALIGN

Add to address to make it evenly divisible by this. This only works at the start of a SECTION or in the middle of a section that is assembled to a fixed address.

### MACRO, MAC

Create a macro. When used with the command line option -endm the macro ends with a ENDMACRO or ENDM directive, and if not using -endm the macro is defined within braces ( { and } ).

	; standard macro usage
	MACRO ldaneg(x) {
		lda #-x
	}

	; -endm macro usage
	MACRO ldaneg(x)
		lda #-x
	ENDM


### EVAL, PRINT, ECHO

Print expression to stdout during assemble. The syntax is:

	EVAL <message>: <expression>

for example

	EVAL Current Address: *

	test_stack = 0
	eval Checking referenced function, Should be 0: .referenced(test_stack)
	eval Checking defined function, Should be 1: .defined(test_stack)

### DC, DV

Declare constant / Declare Value. The directive can be specific by appending .b for byte size, .w for word size, .t for triple size or .l for long size. The default size is 1 byte.

	Test:
	  dc.b $20, *-Test

### BYTE, BYTES

Same as dc.b

### WORD, WORDS

Same as dc.w

### LONG

Same as dc.l

### TEXT

Add text to output, the order of characters can be changed with a string symbol, for instance:

	STRING FontOrder = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&*"
	TEXT [FontOrder] "HELLO #1!"

### INCLUDE

Load and assemble another file at this address.

### INCBIN

Load another file and include as binary data at this address.

### INCSYM

Load symbols from a .sym file

	INCSYM "Main.Sym"

Symbols can also be selected by a list on the same line:

	INCSYM InitMain, UpdateMain, ShutdownMain, "Main.Sym"

### INCDIR

Add a folder to search for include files.

### IMPORT

Generic version of INCLUDE, INCBIN with custom arguments

	; include a raw binary file
	IMPORT "data.bin"
	IMPORT binary "data.bin"

	; include a source file
	IMPORT source "defines.i"

	; include a binary C64 file omitting the load address (first 2 bytes)
	IMPORT c64 "main.prg"

	; include a text file
	IMPORT text "text.txt"
	IMPORT text petscii "petscii.txt"
	IMPORT text petscii_shifted "petscii.txt"
	IMPORT text <string symbol> "custom.txt"	; see TEXT directive

Note that if the command line argument -xdefimp is used then IMPORT is equivalent to XREF instead.

### CONST

Declare a symbol as const, assgning it again will cause an error.

	CONST VICBank = $4000

The constness of a symbol can be tested with the IFCONST directive or the CONST() eval function.

### LABEL

Optional directive create a mutable label, a way to specify non-CONST. It has no actual function.

### STRING

Declare a string symbol. Strings are a little bit limited but can be used for ordering characters in a TEXT declaration, or it can be used as assembler source.

	; Some custom ordered text
	TEXT [FontOrder] "MAKE IT SO!"

	; Macro for (x=start; x<end; x++)
	macro for.x Start, End {
	ldx #Start
	if Start < End
		string _ForEnd = "inx\ncpx #End\nbne _ForLoop"
	elif Start > End
	{
		if (-1 == End) & (Start<129)
		string _ForEnd = "dex\nbpl _ForLoop"
		else
		string _ForEnd = "dex\ncpx #End\nbne _ForLoop"
		endif
	}
	else
		string _ForEnd = ""
	endif
	_ForLoop
	}

	macro forend {
		_ForEnd			; _ForEnd defined by a variation of the for macro
		undef _ForEnd
		undef _ForLoop
	}

	for.x(5, 1)
		lda buf1,x
		sta buf2,x
	forend

### UNDEF

Remove a symbol

	like_bananas = 1
	UNDEF like_bananas

### LABPOOL, POOL

Create a pool of addresses to assign as labels dynamically. This acts as a linear stack allocator for temporary storage and is deallocated when the scope ends if declared as a local symbol.

Pools can be defined as part of a larger pool.

	pool zpGlobal $40-$f8		; all zero page usage
	zpGlobal pool zpLocal 16	; temporary storage for regular functions
	zpGlobal pool zpUtility 16	; temporary storage for utility functions
	zpGlobal pool zpInterrupt 8	; temporary storage for interrupts
	zpGlobal pool zpBuffer 64	; per module storage

Allocate from a pool by using the pool name

	zpBuffer zpIntroTimer.w		; frame counter, 2 bytes
	zpBuffer zpScrollChar.8		; 8 bytes of rol char for scroll

	{
		zpLocal .zpSrc.w		; 2 bytes source address
		zpLocal .zpDst.w		; 2 bytes dest address
		..
	}							; at this point .zpSrc and .zpDst are deallocated and can be reused by other code.
	{
		zpLocal .zpCount		; 1 byte, same address as .zpSrc used above
	}

### IF

Begin conditional code. Whatever lines follow will be assembled only if the expression following the IF evaluates to a non-zero value, the conditional block ends with ELSE, ELSEIF or ENDIF.
	
	conditional_code = 1
	IF conditional_code
		...				; this will be assembled because conditional_code is not zero
	ENDIF

### IFDEF, IFNDEF

Similar to IF but only takes one symbol and the following lines will be assembled only if the symbol was defined previously (IFDEF) or not defined previously (IFNDEF)

	defined_symbol = 0
	IFDEF defined_symbol
		...				; this will be assembled because defined_symbol exists
	ENDIF

### IFCONST

Similar to IF but like IFDEF only takes one symbol and the following lines will be assembled if the symbol is CONST. The symbol should be defined prior to testing it.

CONST() is also an Eval Function that can be used to form more complex expressions using IF. IFCONST is equivalent to IF CONST(<symbol>)

### IFBLANK, IFNBLANK

Checks if the argument exists, mostly for use in macros to test if an argument exists.

BLANK() is also an Eval Function, IFBLANK is equivalent to IF BLANK(...)

### ELSE

Requires a prior IF, the following line will be assembled only if the prior conditional block was not assembled. ELSE must be terminated by an ENDIF

	IF 1
		lda #0
	ELSE
		lda #2
	ENDIF

### ELIF

Requires a prior IF and allows another expression check before ending the conditional blocks

	IFDEF monkey
		lda #monkey_value
	ELIF DEFINED(zebra)
		lda #zebra_value
	ELSE
		lda #human_value
	ENDIF

### ENDIF

Terminated a conditional segment of blocks.

### STRUCT

Declare a set of labels offset from a base address.

Example:

	STRUCT ArtSet {
		word ArtTiles
		word ArtColors
		word ArtMasks
		byte bgColor
	}

Members of the structure can be referenced by the struct name dot member name:

	lda ArtSetData + ArtSet.bgColor

	ArtSetData:
		ds SIZEOF(ArtSet)

### ENUM

Declare a set of incremental labels. Values can either be assigned or one more than the previous. The default first value is 0.

	enum PlayerIndex {
		None = -1,
		One,
		Two
		Three,
		Four,
		Count		; there are this many players
	}

Enum values can be referenced by enum name dot value name:

	ldx #PlayerIndex.One
	{
		inx
		cpx #PlayerIndex.Count
		bcc !
	}

	lda #PlayerIndex.Four

### REPT, REPEAT

Repeats the code within { and } following the REPT directive and counter. Within the REPT code the symbol REPT has the current iteration count, starting at 0.

	const words = 10

	.rept words * 2 { dc.b rept / 2 }

If the command line option -endm is used then REPT uses ENDR instead of the braced scope so the equivalent to the above would be
	
	.rept words * 2
		dc.b rept / 2
	.endr

### A16, A8, XY16, XY8, I16, I8

Specific to 65816 assembly, controls the current accumulator and index register width (8 or 16 bits). Different assemblers use different names so various alternatives are allowed.

### DUMMY, DUMMY_END

Creates a dummy section between DUMMY and DUMMY_END directives.

### DS, RES

Define "section", Reserve. Reserves a number of bytes at the current address. The first argument is the number of bytes and the second argument is optional and is the byte to fill with. The main purpose is to reserve space in a BSS or ZP section.

### SCOPE, ENDSCOPE

A specialized version of a scope, does the same this as a brace scope (code between { and }) but additionally marks all labels defined within as local. An unimplemented feature is that the scope can be named and then labels defined can be accessed outside the scope as

<scope name>::<label> or <scope name>.label (TODO!)

### PUSH, PULL

Creates a stack for a mutable symbol so that it can temporarily be redefined and then restored.

	do_thing = 1

	IF do_thing
		..			; do thing
	ENDIF

	PUSH do_thing
	do_thing = 0

	IF do_thing
		..			; do not do thing
	ENDIF

	PULL do_thing

	IF do_thing
		..			; restored symbol, let's do thing again!
	ENDIF

### ABORT, ERR

Stops assembly with an error if encountered and prints the rest of the line to the output.

---


# Merlin Specific Directives

### MX

### STR

### DA

### DN

### ASC

### PUT

### DDB

### DB

### DFB

### HEX

### DO

### FIN

### EJECT

### OBJ

### TR

### END

### REL

### USR

### DUM

### DEND

### LST, LSTDO

### LUP

### SAV, DSK

### LNK

### XC

### ENT

### EXT

### ADR

### ADRL

### CYC
