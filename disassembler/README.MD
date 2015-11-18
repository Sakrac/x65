#x65 Disassembler

Simple disassembler for 6502, 65C02 and 65816 (default).

## Command Line Options

Typical command line ([*] = optional):

**Usage**

```
x65dsasm binary disasm.txt [$skip[-$end]] [addr=$xxxx] [cpu=6502/65C02/65816] [mx=0-3] [src] [prg]
```

* binary: file which contains some 65xx series instructions
* disasm.txt: output file (default is stdout)
* $skip-$end: first byte offset to disassemble to last byte offset to disassemble
* addr: disassemble as if loaded at addr
* prg: file is a c64 program file starting with the load address
* cpu: set which cpu to disassemble for (default is 6502)
* src: export near assemblable source with guesstimated data blocks
* mx: set the mx flags which control accumulator and index register size


Sample output:

```
; -------------------------------- ;

; Referenced from Label_14 + $7 (subroutine)
; Referenced from Label_14 + $c (subroutine)
Label_11:
  lda $03,x
  bpl Label_13
; Referenced from Label_14 + $18 (subroutine)
Label_12:
  sec 
  lda #$00
  sbc $00,x
  sta $00,x
  lda #$00
  sbc $01,x
  sta $01,x
  lda #$00
  sbc $02,x
  sta $02,x
  lda #$00
  sbc $03,x
  sta $03,x
; Referenced from Label_11 + $2 (branch)
Label_13:
  rts 

; -------------------------------- ;

; Referenced from Label_1 + $1e (subroutine)
; Referenced from Label_16 + $1 (subroutine)
; Referenced from Label_24 + $b (subroutine)
; Referenced from Label_28 + $b (subroutine)
Label_14:
  lda $f7
  eor $f3
```


```
  lda #$e4
  jsr Label_16
  bne Label_27
  beq Label_27
; Referenced from Label_28 + $29 (branch)
Label_29:
  rts 

; -------------------------------- ;

; Referenced from Label_24 + $0 (data)
Label_30:
  dc.b $55, $55, $d5, $ff, $22, $22, $02, $00, $ff, $f2, $ff, $ff, $2e, $00, $00, $00
```