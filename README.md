# x65

6502 Macro Assembler in a single c++ file using the struse single file text parsing library. Supports most syntaxes. x65 was recently named Asm6502 but was renamed because Asm6502 is too generic, x65 has no particular meaning.

Every assembler seems to add or change its own quirks to the 6502 syntax. This implementation aims to support all of them at once as long as there is no contradiction.

To keep up with this trend x65 is adding the following features to the mix:

* Full expression evaluation everywhere values are used: [Expressions](#expressions)
* Basic relative sections and linking.
* Apple II GS executable output
* C style scoping within '{' and '}': [Scopes](#scopes)
* Reassignment of labels. This means there is no error if you declare the same label twice, but on the other hand you can do things like label = label + 2.
* [Local labels](#labels) can be defined in a number of ways, such as leading period (.label) or leading at-sign (@label) or terminating dollar sign (label$).
* [Directives](#directives) support both with and without leading period.
* Labels don't need to end with colon, but they can.
* No indentation required for instructions, meaning that labels can't be mnemonics, macros or directives.
* Conditional assembly with if/ifdef/else etc.
* As far as achievable, support the syntax of other 6502 assemblers (Merlin syntax now requires command line argument, -endm adds support for sources using macro/endmacro and repeat/endrepeat combos rather than scoeps).

In summary, if you are familiar with any 6502 assembler syntax you should feel at home with x65. If you're familiar with C programming expressions you should be familiar with '{', '}' scoping and complex expressions.

There are no hard limits on binary size so if the address exceeds $ffff it will just wrap around to $0000. I'm not sure about the best way to handle that or if it really is a problem.

There is a sublime package for coding/building in Sublime Text 3 in the *sublime* subfolder.

## Features

* **Code**
* **Linking**
* **Comments**
* **Labels**
* **Directives**
* **Macros**
* **Expressions**
* **List File with Cycle Count**

## Prerequisite

x65.cpp requires struse.h which is a single file text parsing library that can be retrieved from https://github.com/Sakrac/struse.

### References

* [6502 opcodes](http://www.6502.org/tutorials/6502opcodes.html)
* [6502 opcode grid](http://www.llx.com/~nparker/a2/opcodes.html)
* [Codebase64 CPU section](http://codebase64.org/doku.php?id=base:6502_6510_coding)
* [6502 illegal opcodes](http://www.oxyron.de/html/opcodes02.html)
* [65816 opcodes](http://wiki.superfamicom.org/snes/show/65816+Reference#fn:14)

## Command Line Options

The command line options specifies the source file, the destination file and what type of file to generate, such as c64 or apple II dos 3.3 or binary or an x65 specific object file. You can also generate a disassembly listing with inline source code or dump the available set of opcodes as a source file. The command line can also set labels for conditional assembly to allow for distinguishing debug builds from shippable builds.

Typical command line ([*] = optional):

```
x65 [-DLabel] [-iIncDir] (source.s) (dest.prg) [-lst[=file.lst]] [-opcodes[=file.s]]
    [-sym dest.sym] [-vice dest.vs] [-obj]/[-c64]/[-bin]/[-a2b] [-merlin] [-endm]
```

**Usage**
x65 filename.s code.prg [options]
* -i(path) : Add include path
* -D(label)[=value] : Define a label with an optional value (otherwise defined as 1)
* -cpu=6502/65c02/65c02wdc/65816: assemble with opcodes for a different cpu
* -acc=8/16: set the accumulator mode for 65816 at start, default is 8 bits
* -xy=8/16: set the index register mode for 65816 at start, default is 8 bits
* -org = $2000 or - org = 4096: set the default start address of fixed address code
* -obj : generate object file for later linking instead of executable binary (.x65)
* -bin : Raw binary (no load address or size included before code)
* -c64 : Include load address (default, default org is $1000)
* -a2b : Apple II Dos 3.3 Binary (changes default org to $803, adds load addr+size)
* -a2p : Apple II ProDos Binary (changed default org to $2000, sets to binary)
* -a2o : Apple II GS OS executable (writes relocatable executable binary)
* -mrg : Force merge all sections (use with -a2o)
* -sym (file.sym) : symbol file
* -lst / -lst = (file.lst) : generate disassembly text from result(file or stdout)
* -opcodes / -opcodes = (file.s) : dump all available opcodes(file or stdout)
* -sect: display sections loaded and built
* -vice (file.vs) : export a vice symbol file
* -merlin: use Merlin syntax
* -endm : macros end with endm or endmacro instead of scoped ('{' - '}')

### Code

Code is any valid mnemonic/opcode and addressing mode. At the moment only one opcode per line is assembled.

### Linking

In order to manage more complex projects linking multiple assembled object files is desirable and x65 builds object files that can be included in a final linking step.
Simply build code with or without a fixed address and the -obj filename.x65 command line argument, then use INCOBJ filename.x65 in a final linking source. The linking source can be assigned a fixed address for most targets or exported as a relocatable executable for Apple II GS.

### Relocatable executable

For Apple II GS OS executable. This output requires 65816 instructions to handle the larger memory and the entry point for code needs to be implemented correctly. Using the -mrg option merges all sections together so that 16 bit addressing is safe, otherwise different code or data segments could be loaded in different banks and 3 byte referencing is required. An important note is that I have not been significantly exposed to Apple II GS or 65816 so this feature is only guaranteed as far as being able to ensure the correctness without actually building a running piece of code.

### Comments

Comments are currently line based and both ';' and '//' are accepted as delimiters.

### Expressions

Anywhere a number can be entered it can also be interpreted as a full expression, for example:

```
Get123:
    bytes Get1-*, Get2-*, Get3-*
Get1:
	lda #1
	rts
Get2:
	lda #2
	rts
Get3:
	lda #3
	rts
```

Would yield 3 bytes where the address of a label can be calculated by taking the address of the byte plus the value of the byte.

### <a name="labels">Labels

Labels come in two flavors: **Addresses** (PC based) or **Values** (Evaluated from an expression).  An address label is simply placed somewhere in code and a value label is followed by '**=**' and an expression. All labels are rewritable so it is fine to do things like NumInstance = NumInstance+1. Value assignments can be prefixed with '.const' or '.label' but is not required to be prefixed by anything, the CONST keyword should cause an error if the label is modified in the same source file.

*Local labels* exist inbetween *global labels* and gets discarded whenever a new global label is added. The syntax for local labels are one of: prefix with period, at-sign, exclamation mark or suffix with $, as in: **.local** or **!local** or **@local** or **local$**. Both value labels and address labels can be local labels.

```
Function:			; global label
	ldx #32
.local_label		; local label
	dex
	bpl .local_label
	rts

Next_Function:		; next global label, the local label above is now erased.
	rts
```

### <a name="directives">Directives

Directives are assembler commands that control the code generation but that does not generate code by itself. Some assemblers prefix directives with a period (.org instead of org) so a leading period is accepted but not required for directives.

* [**CPU**](#cpu) Set the CPU to assemble for.
* [**ORG**](#org) (same as **PC**): Set the current compiling address.
* [**LOAD**](#load) Set the load address for binary formats that support it.
* [**SECTION**](#section) Start a relative section
* [**LINK**](#link) Link a relative section at this address
* [**XDEF**](#xdef) Make a label available globally
* [**XREF**](#xref) Reference a label declared globally in a different object file (.x65)
* [**INCOBJ**](#incobj) Include an object file (.x65) to this file
* [**EXPORT**](#export) Save out additional binary files with argument appended to filename
* [**ALIGN**](#align) Align the address to a multiple by filling with 0s
* [**MACRO**](#macro) Declare a macro
* [**EVAL**](#eval) Log an expression during assembly.
* [**BYTES**](#bytes) Insert comma separated bytes at this address (same as **BYTE** or **DC.B**)
* [**WORDS**](#words) Insert comma separated 16 bit values at this address (same as **WORD** or **DC.W**)
* [**LONG**](#long) Insert comma separated 32 bit values at this address
* [**TEXT**](#text) Insert text at this address
* [**INCLUDE**](#include) Include another source file and assemble at this address
* [**INCBIN**](#incbin) Include a binary file at this address
* [**IMPORT**](#import) Catch-all file inclusion (source, bin, text, object, symbols)
* [**CONST**](#const) Assign a value to a label and make it constant (error if reassigned with other value)
* [**LABEL**](#label) Decorative directive to assign an expression to a label
* [**INCSYM**](#incsym) Include a symbol file with an optional set of wanted symbols.
* [**POOL**](#pool) Add a label pool for temporary address labels
* [**IF / ELSE / IFDEF / ELIF / ENDIF**](#conditional) Conditional assembly
* [**STRUCT**](#struct) Hierarchical data structures (dot separated sub structures)
* [**REPT**](#rept) Repeat a scoped block of code a number of times.
* [**INCDIR**](#incdir) Add a directory to look for binary and text include files in.
* [**65816**](#65816) A16/A8/I16/I8 Directives to control the immediate mode size
* [**MERLIN**](#merlin) A variety of directives and label rules to support Merlin assembler sources

<a name="cpu">**CPU**

Set the CPU to assemble for. This can be updated throughout the source file as needed. **PROCESSOR** is also accepted as an alias.

```
    CPU 65816
```

<a name="org">**ORG**

```

org $2000
(or pc $2000)

```
Start a section with a fixed addresss. Note that source files with fixed address sections can be exported to object files and will be placed at their location in the final binary output when loaded with **INCOBJ**.

<a name="section">**SECTION**

```
    section Code
Start:
	lda #<Data
	sta $fe
	lda #>Data
	sta $ff
	rts

	section BSS
Data:
	byte 1,2,3,4
```

Starts a relative code section. Relative sections require a name and sections that share the same name will be linked sequentially. The labels will be evaluated at link time.

Sections can be aligned by adding a comma separated argument:

```
	section Data,$100
```

Sections can be names and assigned a fixed address by immediately following with an ORG directive

```
	section Code
	org $4000
```

If there is any code or data between the SECTION and ORG directives the ORG directive will begin a new section.

The primary purpose of relative sections (sections that are not assembled at a fixed address) is to generate object files (.x65) that can be referenced from a linking source file by using **INCOBJ** and assigned an address at that point using the **LINK** directive. Object files can mix and match relative and fixed address sections and only the relative sections need to be linked using the **LINK** directive.

Sections can be named anything and still be assigned a section type:

```
    section Gameplay, Code			; code section named Gameplay, unaligned
	...
	section GameBinary, Data, $100	; data section named GameBinary, aligned
	...
	section Work, Zeropage			; Zeropage or Direct page section
	...
	section FixedZP, Zeropage
	org $a0							; Make zero page section as a fixed address
```

Section types include:

* Code: binary code
* Data: binary data
* BSS: uninitialized memory, for fixed address projects the 
* Zeropage: uninitialized memory restricted to the range $00 - $ff

Additional section directive styles include:

```
	SEG segname
	SEG.U segname
	SEGMENT "segname": segtype
	.SEGMENT "segname"
```

For creating relocatable files (OMF) certain sections can not be fixed address.

<a name="xdef">**XDEF**

Used in files assembled to object files to share a label globally. All labels that are not xdef'd are still processed but protected so that other objects can use the same label name without colliding. **XDEF <labelname>** must be specified before the label is defined, such as at the top of the file.

Non-xdef'd labels are kept private to the object file for the purpose of late evaluations that may refer to them, and those labels should also show up in .sym and vice files.

```
	XDEF InitBobs

InitBobs:
	rts
```

<a name="xref">**XREF**

In order to reference a label that was globally declared in another object file using XDEF the label must be declared by using XREF.

<a name="incobj">**INCOBJ**

Include an object file for linking into this file. Object files are generated by the *-obj* command line option followed by a filename ("file.x65"). Any linked segments will be linked, and multiple linked files can be generated by using the [**EXPORT**](#export) directive.

<a name="link">**LINK**

Link a set of relative sections (sharing the same name) at this address

The following lines will place all sections named Code sequentially at location $1000, followed by all sections named BSS:

```
	org $1000
    link Code
	link BSS
```

There is currently object file support (use -obj <filename> argument to generate), the recommended file extension for object files is .x65. In order to access symbols from object file code use **XDEF** <labelname> prior to declaring a label within the object.

To inspect the contents of x65 objects files there is a 'dump_x65' tool included in this archive.

Note that the assembler will link all segments in a reasonable order (first code segments from current file, then code from other files, then data, then BSS segments), so using the **LINK** directive is intended to give more control but is not necessary for the linking process. **INCOBJ** is necessary for bringing in external objects though otherwise the linker won't know how to find the segments to link.

<a name="load">**LOAD**

```
load $2000
```

For c64 .prg files this prefixes the binary file with this address.

<a name="export">**EXPORT**

Allows saving multiple binary files (prg, a2b, bin, etc.) from a single source file build

```
	section gamecode_level1
	export _level1
```

will export the section "gamecode_level1" to (output_file)_level1.prg while other sections would be grouped together into (output_file).prg. This allows a single linking source to combine multiple loads overlapping the same memory area ending up in separate files.

<a name="align">**ALIGN**

```
align $100
```

Add bytes of 0 up to the next address divisible by the alignment. If the section is a fixed address (using an ORG directive) align will be applied at the location it was specified, but if the section is relative (using the SECTION directive) the alignment will apply to the start of the section.

**MACRO**

See the '[Macro](#macro)' section below

<a name="eval">**EVAL**

Example:
```
eval Current PC: *
```
Might yield the following in stdout:
```
Eval (15): Current PC : "*" = $2010
```

When eval is encountered on a line print out "EVAL (\<line#\>) \<message\>: \<expression\> = \<evaluated expression\>" to stdout. This can be useful to see the size of things or debugging expressions.

<a name="bytes">**BYTES**

Adds the comma separated values on the current line to the assembled output, for example

```
RandomBytes:
	bytes NumRandomBytes
	{
	    bytes 13,1,7,19,32
		NumRandomBytes = * - !
	}
```

**byte** or **dc.b** are also recognized.

<a name="words">**WORDS**

Adds comma separated 16 bit values similar to how **BYTES** work. **word** or **dc.w** are also recognized.

<a name="long">**LONGS**

Adds comma separated 32 bit values similar to how **WORDS** work.

<a name="text">**TEXT**

Copies the string in quotes on the same line. The plan is to do a petscii conversion step. Use the modifier 'petscii' or 'petscii_shifted' to convert alphabetic characters to range.

Example:

```
text petscii_shifted "This might work"
```

<a name="include">**INCLUDE**

Include another source file. This should also work with .sym files to import labels from another build. The plan is for x65 to export .sym files as well.

Example:

```
include "wizfx.s"
```


<a name="incbin">**INCBIN**

Include binary data from a file, this inserts the binary data at the current address.

Example:

```
incbin "wizfx.gfx"
```

<a name="import">**IMPORT**

Insert multiple types of data or code at the current address. Import takes an additional parameter to determine what to do with the file data, and can accept reading in a portion of binary data.

The options for import are:
* source: same as **INCLUDE**
* binary: same as **INCBIN**
* c64: same as **INCBIN** but skip first two bytes of file as if this was a c64 prg file
* text: include text data from another file, default is petscii otherwise add another directive from the **TEXT** directive
* object: same as **INCOBJ**
* symbols: same as **INCSYM**, specify list of desired symbols prior to filename.

After the filename for binary and c64 files follow comma separated values for skip data size and max load size. c64 mode will add the two extra bytes to the skip size.

```
	import source "EQ.S"
	import binary "GFX.BIN",0,256
	import c64 "FONT.BIN",8,8*26
	import text petscii_shifted "LICENSE.TXT"
	import object "engine.x65"
	import symbols InitEffect, UpdateEffect "effect.sym"
```

<a name="const">**CONST**

Prefix a label assignment with 'const' or '.const' to cause an error if the label gets reassigned.

```
const zpData = $fe
```

<a name="label">**LABEL**

Decorative directive to assign an expression to a label, label assignments are followed by '=' and an expression.

These two assignments do the same thing (with different values):
```
label zpDest = $fc
zpDest = $fa
```

<a name="incsym">**INCSYM**

Include a symbol file with an optional set of wanted symbols.

Open a symbol file and extract a set of symbols, or all symbols if no set was specified. Local labels will be discarded if possible.

```
incsym Part1_Init, Part1_Update, Part1_Exit "part1.sym"
```

<a name="pool">**POOL**

Add a label pool for temporary address labels. This is similar to how stack frame variables are assigned in C.

A label pool is a mini stack of addresses that can be assigned as temporary labels with a scope ('{' and '}'). This can be handy for large functions trying to minimize use of zero page addresses, the function can declare a range (or set of ranges) of available zero page addresses and labels can be assigned within a scope and be deleted on scope closure. The format of a label pool is: "pool <pool name> start-end, start-end" and labels can then be allocated from that range by '<pool name> <label name>[.b][.w]' where .b means allocate one byte and .w means allocate two bytes. The label pools themselves are local to the scope they are defined in so you can have label pools that are only valid for a section of your code. Label pools works with any addresses, not just zero page addresses.

Example:
```
Function_Name: {
	pool zpWork $f6-$100		; these zero page addresses are available for temporary labels
	zpWork zpTrg.w				; zpTrg will be $fe
	zpWork zpSrc.w				; zpSrc will be $fc

	lda #>Src
	sta zpSrc
	lda #<Src
	sta zpSrc+1
	lda #>Dest
	sta zpDst
	lda #<Dest
	sta zpDst+1

	{
		zpWork zpLen			; zpLen will be $fb
		lda #Length
		sta zpLen
	}
	nop
	{
		zpWork zpOff			; zpOff will be $fb (shared with previous scope zpLen)
	}
	rts
```

The following extensions are recognized:

* [pool name] var (no extension is one byte)
* [pool name] var.w (2 bytes)
* [pool name] var.d (2 bytes)
* [pool name] var.t (3 bytes)
* [pool name] var.l (4 bytes)

<a name="conditional">**IF / ELSE / IFDEF / ELIF / ENDIF**

Conditional code parsing is very similar to C directive conditional compilation.

Example:

```
DEBUG = 1

if DEBUG
    lda #2
else
    lda #0
endif
```

<a name="struct">**STRUCT**

Hierarchical data structures (dot separated sub structures)

Structs helps define complex data types, there are two basic types to define struct members, and as long as a struct is declared it can be used as a member type of another struct.

The result of a struct is that each member is an offset from the start of the data block in memory. Each substruct is referenced by separating the struct names with dots.

Example:

```
struct MyStruct {
	byte	count
	word	pointer
}

struct TwoThings {
	MyStruct thing_one
	MyStruct thing_two
}

struct Mixed {
	word banana
	TwoThings things
}

Eval Mixed.things
Eval Mixed.things.thing_two
Eval Mixed.things.thing_two.pointer
Eval Mixed.things.thing_one.count
```

results in the output:

```
EVAL(16): "Mixed.things" = $2
EVAL(27): "Mixed.things.thing_two" = $5
EVAL(28): "Mixed.things.thing_two.pointer" = $6
EVAL(29): "Mixed.things.thing_one.count" = $2
```

<a name="rept">**REPT**

Repeat a scoped block of code a number of times. The syntax is rept \<count\> { \<code\> }. The full word **REPEAT** is also recognized.

Example:

```
columns = 40
rows = 25
rept columns {
	screen_addr = $400 + rept		; rept is the repeat counter
	ldx $1000+rept
	dest = screen_addr
	remainder = 3
	rept (rows+remainder)/4 {
		stx dest
		dest = dest + 4*40
	}
	rept 3 {
		inx
		remainder = remainder-1
		screen_addr = screen_addr + 40
		dest = screen_addr
		rept (rows+remainder)/4 {
			stx dest
			dest = dest + 4*40
		}
	}
}
```

Note that if the -endm command line option is used (macros are not defined with curly brackets but inbetween macro and endm*) this also affects rept so the syntax for a repeat block changes to

```
.REPEAT 4
	lsr
.ENDREPEAT
```

The symbol 'REPT' is the current repeat count within a REPT (0 outside of repeat blocks).


<a name="incdir">**INCDIR**

Adds a folder to search for INCLUDE, INCBIN, etc. files in

###<a name="65816">**65816**

* **A16** Set immediate mode for accumulator to be 16 bits
* **A8** Set immediate mode for accumulator to be 8 bits
* **I16** Set immediate mode for index registers to be 16 bits. **XY16** is also accepted.
* **I8** Set immediate mode for index registers to be 8 bits. **XY8** is also accepted.

The accumulator and index register mode will be reset to 8 bits if the CPU is switched to something other than 65816.

An alternative method is to add .b/.w to immediate mode instructions that support 16 bit modes such as:

```
	ora.b #$21
	ora.w #$2322
```
This alleviates some confusion about which mode a certain line might be assembled for when looking at source code.

Note that in case a 4 digit hex value is used in 8 bit mode and an immediate mode is allowed but is not currently enable a two byte value will be emitted

```
	lda #$0043	; will be 16 bit regardless of accumulator mode if in 65816 mode
	lda #$43	; will be 8 bit or 16 bit depending on accumulator mode
```

Similarly for instructions that accept 3 byte addresses (bank + address) adding .l instructs the assembler to choose a bank address:

```
	and.l $222120
	and.l $222120,x
	jsr.l $203212
```
Although if six hexadecimal digits are specified the bank + address instruction will be assembled without decoration.

Alternatively Merlin simply adds an 'l' to the instruction:

```
	andl $222120
	andl $222120,x
	jsrl $203212
```

x65 Labels are not restricted to 16 bits, the bank byte can be extracted from a label with the '^' operator:

```
	lda #^label
```


###<a name="merlin">**MERLIN**

A variety of directives and label rules to support Merlin assembler sources. Merlin syntax is supported in x65 since there is historic relevance and readily available publicly release source.

* [Pinball Construction Set source](https://github.com/billbudge/PCS_AppleII) (Bill Budge)
* [Prince of Persia source](https://github.com/jmechner/Prince-of-Persia-Apple-II) (Jordan Mechner)

To enable Merlin 8.16 syntax use the '-merlin' command line argument. Where it causes no harm, Merlin directives are supported for non-merlin mode.

*LABELS*

]label means mutable address label, also does not seem to invalidate local labels.

:label is perfectly valid, currently treating as a local variable

labels can include '?'

Merlin labels are not allowed to include '.' as period means logical or in merlin, which also means that enums and structs are not supported when assembling with merlin syntax.

*Expressions*

Merlin may not process expressions (probably left to right, parenthesis not allowed) the same as x65 but given that it wouldn't be intuitive to read the code that way, there are probably very few cases where this would be an issue.

**XC**

Change processor. The first instance of XC will switch from 6502 to 65C02, the second switches from 65C02 to 65816. To return to 6502 use **XC OFF**. To go directly to 65816 **XC XC** is supported.

**MX**

MX sets the immediate mode accumulator instruction size, it takes a number and uses the lowest two bits. Bit 0 applies to index registers (x, y) where 0 means 16 bits and 1 means 8 bits, bit 1 applies to the accumulator. Normally it is specified in binary using the '%' prefix.

```
    MX %11
``` 

**LUP**

LUP is Merlingo for loop. The lines following the LUP directive to the keyword --^ are repeated the number of times that follows LUP.

**MAC**

MAC is short for Macro. Merlin macros are defined on line inbetween MAC and <<< or EOM. Macro arguments are listed on the same line as MAC and the macro identifier is the label preceeding the MAC directive on the same line.

**EJECT**

An old assembler directive that does not affect the assembler but if printed would insert a page break at that point.

**DS**

Define section, followed by a number of bytes. If number is positive insert this amount of 0 bytes, if negative, reduce the current PC.

**DUM**, **DEND**

Dummy section, this will not write any opcodes or data to the binary output but all code and data will increment the PC addres up to the point of DEND.

**PUT**

A variation of **INCLUDE** that applies an oddball set of filename rules. These rules apply to **INCLUDE** as well just in case they make sense.

**USR**

In Merlin USR calls a function at a fixed address in memory, x65 safely avoids this. If there is a requirement for a user defined macro you've got the source code to do it in.

**SAV**

SAV causes Merlin to save the result it has generated so far, which is somewhat similar to the [EXPORT](#export) directive. If the SAV name is different than the source name the section will have a different EXPORT name appended and exported to a separate binary file.

**DSK**

DSK is similar to SAV

**ENT**

ENT defines the label that preceeds it as external, same as [**XDEF**](#xdef).

**EXT**

EXT imports an external label, same as [**XREF**](#xref).

**LNK** / **STR**

LNK links the contents of an object file, to fit with the named section method of linking in x65 this keyword has been reworked to have a similar result, the actual linking doesn't begin until the current section is complete.

**CYC**

CYC starts and stops a cycle counter, x65 scoping allows for hierarchical cycle listings but the first merlin directive CYC starts the counter and the next CYC stops the counter and shows the result. This is 6502 only until data is entered for other CPUs.

**ADR**

Define byte triplets (like **DA** but three bytes instead of 2)

**ADRL**

Define values of four bytes.

## List File

This is a typical list file. Columns from left to right are:

* Address
* Bytes (up to 4) generated by this line
* Instruction (disassembled)
* Cycles for this instruction, + indicates this instruction can be an extra cycle due to condition or for 65816 multiple extra cycles.
* Source code that generated this line

For scope lines ('{' - '}') the sum of the cycles within the scope is added up as are the additional cycles.

```
      c>1                               Sin {
$0000 a2 03       ldx #$03       2        ldx #3
      c>2                                 {
$0002 b5 e8       lda $e8,x      4          lda SinP.Ang,x
$0004 95 ec       sta $ec,x      4          sta SinP.R,x        ; result starts with x
$0006 95 e4       sta $e4,x      4          sta SinP.W0,x
$0008 95 f4       sta $f4,x      4          sta Mul824.A,x
$000a 95 f0       sta $f0,x      4          sta Mul824.B,x
$000c ca          dex            2          dex
$000d 10 f3       bpl $0002      2+         bpl !
      c<2 = 24 + 1                        }
                                          ; x^2, copy to W1
$000f a9 e0       lda #$e0       2        lda #SinP.W1
$0011 20 00 00    jsr $0000      6        jsr Multiply824S_Copy
                                          ; iterate value
$0014 a0 00       ldy #$00       2        ldy #0
                                        .SinIterate
      c>2                                 {
                                            ; W0 *= W1
$0016 a2 03       ldx #$03       2          ldx #3
      c>3                                   {
$0018 b5 e4       lda $e4,x      4            lda SinP.W0,x     ; x^(1+2n)
$001a 95 f4       sta $f4,x      4            sta Mul824.A,x
$001c b5 e0       lda $e0,x      4            lda SinP.W1,x     ; x^2
$001e 95 f0       sta $f0,x      4            sta Mul824.B,x
$0020 ca          dex            2            dex
$0021 10 f5       bpl $0018      2+           bpl !
      c<3 = 20 + 1                          }
$0023 a9 e4       lda #$e4       2          lda #SinP.W0        ; Copy to W0
$0025 20 00 00    jsr $0000      6          jsr Multiply824S_Copy
$0028 a2 e4       ldx #$e4       2          ldx #SinP.W0        ; Copy W0 to A
$002a a9 f4       lda #$f4       2          lda #Mul824.A
$002c 20 00 00    jsr $0000      6          jsr Cpy824Z
$002f a2 00       ldx #$00       2          ldx #0
      c>3                                   {
$0031 b9 00 00    lda $0000,y    4+           lda SinInvPermute,y
$0034 95 f0       sta $f0,x      4            sta Mul824.B,x
$0036 c8          iny            2            iny
$0037 e8          inx            2            inx
$0038 e0 04       cpx #$04       2            cpx #4
$003a d0 f5       bne $0031      2+           bne !
      c<3 = 16 + 2                          }
```

## <a name="expressions">Expression syntax

Expressions contain values, such as labels or raw numbers and operators including +, -, \*, /, & (and), | (or), ^ (eor), << (shift left), >> (shift right) similar to how expressions work in C. Parenthesis are supported for managing order of operations where C style precedence needs to be overridden. In addition there are some special characters supported:

* \*: Current address (PC). This conflicts with the use of \* as multiply so multiply will be interpreted only after a value or right parenthesis
* <: If less than is not followed by another '<' in an expression this evaluates to the low byte of a value (and $ff)
* >: If greater than is not followed by another '>' in an expression this evaluates to the high byte of a value (>>8)
* ^: Inbetween two values '^' is an eor operation, as a prefix to values it extracts the bank byte (v>>24).
* !: Start of scope (use like an address label in expression)
* %: First address after scope (use like an address label in expression)
* $: Precedes hexadecimal value
* %: If immediately followed by '0' or '1' this is a binary value and not scope closure address

**Conditional operators**

* ==: Double equal signs yields 1 if left value is the same as the right value
* <: If inbetween two values, less than will yield 1 if left value is less than right value
* >: If inbetween two values, greater than will yield 1 if left value is greater than right value
* <=: If inbetween two values, less than or equal will yield 1 if left value is less than or equal to right value
* >=: If inbetween two values, greater than or equal will yield 1 if left value is greater than or equal to right value

Example:

```
lda #(((>SCREEN_MATRIX)&$3c)*4)+8
sta $d018
```

Avoid using parenthesis as the first character of the parameter of an opcode that can be relative addressed instead of an absolute address. This can be avoided by 

```
	jmp (a+b)	; generates a relative jump
	jmp.a (a+b)	; generates an absolute jump
	jmp +(a+b)	; generates an absolute jump

c = (a+b)
	jmp c		; generates an absolute jump
	jmp a+b		; generates an absolute jump
```

## <a name="macro">Macros

A macro can be defined by the using the directive macro and includes the line within the following scope:

Example:
```
macro ShiftLeftA(Source) {
    rol Source
    rol A
}
```

The macro will be instantiated anytime the macro name is encountered:
```
lda #0
ShiftLeftA($a0)
```

The parameter field is optional for both the macro declaration and instantiation, if there is a parameter in the declaration but not in the instantiation the parameter will be removed from the macro. If there are no parameters in the declaration the parenthesis can be omitted and will be slightly more efficient to assemble, as in:

```
.macro GetBit {
    asl
    bne %
    jsr GetByte
}
```

Alternative syntax for macros:

To support the syntax of other assemblers macro parameters can also be defined through space separated arguments:

```
	macro loop_end op lbl {
		op
		bne lbl
	}

	ldx #4
	{
	    sta buf,x
	    loop_end dex !
	} 
```

Other assemblers use a directive to end macros rather than a scope (inbetween { and }). This is supported by adding '-endm' to the command line:

```
macro ShiftLeftA source
	rol source
	asl
endm
```

As long as the macro end directive starts with endm it will be accepted, so endmacro will work as well.

Currently macros with parameters use search and replace without checking if the parameter is a whole word, the plan is to fix this. 

## <a name="scopes">Scopes

Scopes are lines inbetween '{' and '}' including macros. The purpose of scopes is to reduce the need for local labels and the scopes nest just like C code to support function level and loops and inner loop scoping. '!' is a label that is the first address of the scope and '%' the first address after the scope.

Additionally scopes have a meaning for counting cycles when exporting a .lst file, each open scope '{' will add a new counter of CPU cycles that will accumulate until the corresponding '}' which will be shown on that line in the listing file. Use -lst as a command line option to generate a listing file.

This means you can write
```
{
    lda #0
    ldx #8
    {
        sta Label,x
    	dex
    	bpl !
    }
}
```	
to construct a loop without adding a label.

##Examples

Using scoping to avoid local labels

```
; set zpTextPtr to a memory location with text
; return: y is the offset to the first space.
;  (y==0 means either first is space or not found.)
FindFirstSpace
  ldy #0
  {
    lda (zpTextPtr),y
    cmp #$20
    beq %             ; found, exit
    iny
    bne !             ; not found, keep searching
  }
  rts
```
### Acknowledgments

This project would not be completed without the direct or indirect support of great people, some which I can currently remember:

* Marc dePeo, helping me uncover the strange and unique world of Merlin’s assembler syntax (and working together with me on True Crime NY gameplay code and more)
* Che Lalic, explaining the murky bits of 65816 (and a Ninja on SNES NBA Hangtime and other projects)
* John Brooks, sharing the Rastan Apple II source code so I could test 65816 and figure out a number of issues with my initial linker, and encouraging the implementation of Apple II GS OS executable file format / OMF export (and helping out with Playstation All-Stars)
* [Brutal Deluxe](http://www.brutaldeluxe.fr) for releasing the excellent OMF Analyzer tool and the source, which was a significant help generating Apple II GS OS executables.
* The C64 demo scene for sharing a great deal of 6502 programming resources and overall inspiration.
* Jordan Mechner, sharing the Prince of Persia Apple II source code so I could test out a significant part of the assembler and the Merlin syntax mode
* Bill Budge, sharing the Pinball Construction Set Apple II source code, although at the point I tried it, all of it just assembled without any assembler issues at all.

### Development Status

Looking for help testing various features of the assembler, I have a large number of tests that pass without fail but there are so many ways for assemblers to break.
Primarily tested with personal archive of sources written for Kick assmebler, DASM, TASM, XASM, etc. and passing most of Apple II Prince of Persia and Pinball Construction set.

**TODO**
* irp (indefinite repeat)

**FIXED**
* OMF export for Apple II GS/OS executables
* More DASM directives supported (ERR, DV, DS.B, DS.W, DS.L)
* Removed the concept of linking by merging sections and instead keeping the sections separate and individually assigned memory addresses so they won't overlap.
* Fixed up Merlin LNK directive to work with new linker
* Fixed linker merged section reloc confusion.
* -org command line argument to override the built-in assumption of org $1000, to avoid ever having to use the ORG directive inlined in code.
* dump_x65 now shows the code offset of each section into the .x65 file which can be copied and pasted into the disassembler in case the object file assembler output needs to be inspected.
* A linker export summary is shown when building binary fixed address, this shows how the linker re-arranged the sections in memory. The section addresses are also included in the .lst file even if the section didn't generate any listing information, such as included object files.
* BSS sections are handled similar to CODE and DATA sections but will not write out BSS bytes at end of binary data. This should complete the section handling necessary to build a relocatable executable.
* Replaced the fixed address linker so it doesn't merge sections but just assigns addresses. This is more similar to how a relocatable code loader would handle it. I may need to merge sections for OMF to reduce the number of code sections.
* dc.t (3 bytes) dc.l (4 bytes) for data declaration
* Linking of zero page / direct page sections
* Added section types, should cover most intuitive formats (seg.type; segment name; segment "name": type; etc. etc.)
* Changed the data for relocs to better match Apple II GS OMF format which also changes the object file format.
* Added a disassembler (disassembler/x65dsasm.cpp)
* % evaluates to the current end of scope instead of whatever scope ends first
* rept crash fix if not resolved until assembly completed
* rept and symbol reference with forward reference label was not taking section into account
* Link append sections target confusion cleared up (caused crash/link errors/freeze)
* XREF prevented linking with same name symbol included from .x65 object causing a linker failure
* << was mistakenly interpreted as shift right
* REPT is also a value that can be used in expressions as a repeat counter
* LONG allows for adding 4 byte values like BYTE/WORD
* Cycle listing for 65816 - required more complex visualization due to more than 1 cycle extra

[(older fixes)](../../wiki/fixes)

Revisions:
* 9 - Apple II GS OS executable
* 8 - Fish food / Linking tested and passed with external project (Apple II gs Rastan)
* 7 - 65816 support
* 6 - 65C02 support
* 5 - Merlin syntax
* 4 - Object files, relative sections and linking
* 3 - 6502 full support
* 2 - Moved file out of struse samples
* 1 - Built a sample of a simple assembler within struse
