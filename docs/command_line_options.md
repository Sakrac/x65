# Command Line Options for x65

These are the current options for controlling x65 from the command line.

## lst
	-lst / -lst=(file.lst)

Generate disassembly text from result(file or stdout)

## tsl
	-tsl=(file)
	
generate listing file in TASS style

## tl
	
	-tl=(file)
	
Generate labels in TASS style

## opcodes
	-opcodes / -opcodes=(file.s)
	
Use with -cpu=... to dump all available opcodes for that CPU (file or stdout)

## endm
	-endm
	
macros end with endm or endmacro instead of scoped('{' - '}') and rept/repeat emds with endr instead of being scoped.

## cpu
	-cpu=[6502/6502ill/65c02/65c02wdc/65816]

declare CPU type, use with argument

## acc [65816]
	-acc=[8/16]
	
set the accumulator mode for 65816 at start, default is 8 bits

## xy [65816]
	-xy=8/16
	
set the index register mode for 65816 at start, default is 8 bits


## org
	-org=$2000 or -org=4096
	
force assembly for first encountered non-specific address section at given address

## kickasm
	-kickasm

use Kick Assembler syntax (in progress)

## merlin
	-merlin

use Merlin syntax

## c64
	-c64

(default) Include 2 byte load address in binary output

## a2b
	-a2b
	
Produce an Apple II Dos 3.3 Binary

## bin
	-bin

Produce raw binary

## a2p
	-a2p
	
Produce an Apple II ProDos Binary

## a2o
	-a2o

Produce an Apple II GS OS executable (relocatable)

## mrg
	-mrg
	
Force merge all sections (use with -a2o)

## sect
	-sect

display sections loaded and built

## sym
	-sym (file.sym)
	
generate symbol file

## obj
	-obj (file.x65)
	
Produce an object file instead of a binary for later linking

## vice
	-vice (file.vs)

export a vice monitor command file (including vice symbols)

## xrefimp
	-xrefimp
	
import directive means xref, not include/incbin and export directive means xdef, not export section.

