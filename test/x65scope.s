// Test various features of scoping.

ClearVICPage:
{
	lda #0
	tax
	ldy #$40
	{
		sta $4000,x
		inx
		bne !
		inc !+2
		dey
		bne !
	}
	rts
}

LenStr:
{
	dc.b %-!	; length of string plus this byte
	TEXT "String"
}

GetSkip:
{
	clc
	tya
	adc .addr+1
	sta .addr+1
	{
		bcc %
		inc .addr+1
	}
.addr
	lda $1234
	rts
}
