;
; common.i 
;

; merlin mx macro
macro mx mx
{
	if (0 == (mx&%10))
		A16
	else
		A8
	endif
	if (0 == (mx&%01))
		I16
	else
		I8
	endif
}
				 
macro _shadowON
{
	lda >$00C035
	and #$FFF7
	sta >$00C035
}

macro _shadowOFF
{
	lda >$00C035
	ora #$0008
	sta >$00C035
}

macro _auxON
{
	lda >$00C068
	ora #$0030
	sta >$00C068
}

macro _auxOFF
{
	lda >$00C068
	and #$FFCF
	sta >$00C068
}
				 

