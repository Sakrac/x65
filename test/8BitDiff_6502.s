// EXAMPLE KICK ASSEMBLER FILE

//
// 6502 Decoder for 8BitDiff
//
//	Copyright 2015 Carl-Henrik Skårstedt. All rights reserved.
//			https://github.com/sakrac/8BitDiff/
//
// 8BDIFF FORMAT
// -------------
// 4 bits: size of offset bit sizes
// 4 bits: size of length bit sizes
// 1 byte length bit sizes
// 1 byte offset bit sizes
// 2 bytes size of injected bytes (8 bit specific)
// injected bytes
// loop until end of inject buffer:
//  bit: 0=inject, 1=source or target
//  length bit cnt+length bits
//  if source or target:
//   buffer offset bit cnt + buffer offset bits
//	 sign of offset (not instead of negate)
//	 bit: 0=source, 1=target
// repeat loop
//
// target/source/inject source pointers start
// at the start of each buffer.
// any pointer is set to the end of the run after
// copy and the offset increments/decrements for source
// for a negative offset, invert the number, don't negate.
//
// USAGE
// -----
// Use the 8BDIFF tool (https://github.com/sakrac/8BitDiff)
// to generate a patch between two files. Load the
// original file and the patch file and assign them as parameters:
// z8BDiff = Address of patch
// z8BSrc = Address of original data
// z8BDst = Address to decode updated data
// jsr Patch_8BDiff
//

.label zParam8B = $f0 		// 6 bytes
.label zWork8B = $f6 		// 10 bytes

// input (will be changed)
.label z8BDiff = zParam8B 			// start of diff
.label z8BSrc = z8BDiff+2			// start of source
.label z8BDst = z8BSrc+2 			// start of destination

// work (will be changed)
.label z8BOff = zWork8B				// 2 bytes current instruction offset
.label z8BTemp = z8BOff 			// 1 byte temporary address
.label z8BLen = z8BOff+2			// 2 bytes current instruction length
.label z8BOffsSize = z8BLen+2		// 1 byte hot many bits to read for offset
.label z8BLenSize = z8BOffsSize+1 	// 1 byte how many bits to read for length
.label z8BInj = z8BLenSize+1		// 2 bytes current injection address
.label z8BTrg = z8BInj+2			// start of target 2 bytes

.label z8BInjSize = z8BTrg 			// 2 bytes size of injection buffer
.label z8BInjEnd = z8BDiff 			// 2 bytes keeps track of end condition

// macro for reading one bit
.macro GetBit() {
	asl
	bne !BitOk+
	jsr GetByte
!BitOk:
}

Patch_8BDiff:
	ldy #0 				// clear Y
	lda (z8BDiff),y 	// first byte is prefix length bits | offset bits<<4
	tax
	lsr
	lsr
	lsr
	lsr
	sta z8BOffsSize
	txa
	and #$0f
	sta z8BLenSize
	inc z8BDiff
	bne NotLastByte
	inc z8BDiff+1
NotLastByte:
	lda z8BDiff 		// store off address to length bit sizes
	sta GetLenBits+1
	lda z8BDiff+1
	sta GetLenBits+2
	ldx z8BLenSize		// prefix bits for length
	jsr BitX 			// A = 1<<X, does not change Y
	ldx #z8BDiff		// add A to z8BDiff
	jsr ApplyOffsetA 	// does not change Y
	lda z8BDiff 		// store off address to offset bit sizes
	sta GetOffsBits+1
	lda z8BDiff+1
	sta GetOffsBits+2
	ldx z8BOffsSize		// prefix bits for offset
	jsr BitX 			// A = 1<<X, does not change Y
	ldx #z8BDiff		// add A to z8BDiff
	jsr ApplyOffsetA 	// does not change Y

	// read out injected bytes size (only up to $7fff supported)
	lda (z8BDiff),y
	pha
	iny
	lda (z8BDiff),y
	pha
	iny
	ldx #z8BDiff 		// skip inject size bytes (2)
	jsr ApplyOffsetY
	clc
	lda z8BDiff 		// store off inject buffer address high
	sta z8BInj
	pla
	adc z8BDiff
	sta DiffPtr+1 		// store off instruction start low
	sta z8BInjEnd 		// store off inject buffer end low
	lda z8BDiff+1
	sta z8BInj+1 		// store off inject buffer address high
	pla
	adc z8BDiff+1
	sta DiffPtr+2 		// store off instruction start high
	sta z8BInjEnd+1 	// store off inject buffer end high

	lda z8BDst 			// store off target buffer
	sta z8BTrg
	lda z8BDst+1
	sta z8BTrg+1

	// read instructions!
	lda #0 				// clear bit shift byte => force fetch
NextInstruction:
	:GetBit() 			// bit is 0 => inject, otherwise source or target buffer
	bcs SrcTrg
	ldx z8BInj 			// check if complete
	cpx z8BInjEnd
	bcc NotEnd
	ldx z8BInj+1
	cpx z8BInjEnd+1
	bcc NotEnd
	rts 				// patching is complete, return to caller
NotEnd:
	jsr GetLen 			// number of bytes to inject y = 0 here
	pha 				// save bit shift byte
	ldx #z8BInj 		// use inject buffer
	bne MoveToDest		// always branch!
	// PC will not cross this line
SrcTrg:
	jsr GetLen 			// number of bytes to copy
	jsr GetOffs 		// offset in buffer, y = 0 here
	:GetBit() 			// check sign bit
	bcc PositiveOffs
	pha
	lda #$ff 			// apply negativity to offset
	eor z8BOff
	sta z8BOff
	lda #$ff
	eor z8BOff+1
	sta z8BOff+1
	pla
PositiveOffs:
	:GetBit()
	ldx #z8BSrc 		// use source buffer
	bcc Src
	ldx #z8BTrg 		// use target buffer
Src:
	pha 				// save bit shift byte
	clc
	lda z8BOff 			// apply offset to current buffer
	adc $00,x
	sta $00,x
	lda z8BOff+1
	adc $01,x
	sta $01,x

MoveToDest:
	stx BufferCopyTrg+1 // x = zero page source buffer to copy from

PageCopy:				// copy pages (256 bytes) at a time
	dec z8BLen+1
	bmi PageCopyDone
	jsr BufferCopy 		// y is 0 here so copy 256 bytes
	inc $01,x
	inc z8BDst+1
	bne PageCopy
	// PC will not cross this line
PageCopyDone:
	ldy z8BLen 			// get remaining number of bytes in y
	beq NoLowLength
	jsr BufferCopy
	lda z8BLen 			// apply remainder of bytes to buffer
	jsr ApplyOffsetA
	lda z8BLen
	ldx #z8BDst 		// apply remainder of bytes to destination
	jsr ApplyOffsetA
NoLowLength:
	pla 				// retrieve bit shift byte to parse next bit
	bne NextInstruction
	// PC will not cross this line

// BufferCopy copies bytes forward from 0 to y (256 if y is 0)
BufferCopy:
	sty BufferCopyLength+1
	ldy #0
BufferCopyTrg:
	lda (z8BInj),y
	sta (z8BDst),y
	iny
BufferCopyLength:
	cpy #0
	bne BufferCopyTrg		// Y returns unchanged
	rts

// Gets Y number of bits and return the value in Y
GetLenOffBits:
	:GetBit()
	rol z8BTemp
	dey
	bne GetLenOffBits
	ldy z8BTemp
	rts

// Gets the next length
GetLen:
	ldy #0
	sty z8BTemp
	ldy z8BLenSize
	jsr GetLenOffBits
	pha 			// save bit shift byte
GetLenBits:
	lda $1234,y
	ldx #z8BLen 	// X is the zero page address to store in
GetLenOffValue:
	tay
	lda #0 			// clear target indirect address
	sta $00,x
	sta $01,x
	pla 			// retrieve bit shift byte
GetLenOffLoop:
	:GetBit()
	rol $00,x
	rol $01,x
	dey
	bne GetLenOffLoop // y returns 0
	rts

// Gets the next buffer offset
GetOffs:
	ldy #0
	sty z8BTemp
	ldy z8BOffsSize
	jsr GetLenOffBits
	pha 			// save bit shift byte
GetOffsBits:
	lda $1234,y
	ldx #z8BOff 	// X is the zero page address to store in
	bne GetLenOffValue

// Gets one byte of bits and returns top bit in C
GetByte:
	sec
DiffPtr:
	lda $1234
	rol
	inc DiffPtr+1
	bne DiffPage
	inc DiffPtr+2
DiffPage:
	rts

// Returns 1<<X
BitX:			// does not change Y
	lda #0
	sec
BitXShift:
	rol
	dex
	bpl BitXShift
	rts

// Apply an offset to a zero page indirect address in X
ApplyOffsetY:
	tya
ApplyOffsetA:
	clc
	adc $00,x
	sta $00,x
	bcc ApplyLow
	inc $01,x
ApplyLow:
	rts
