
RAGDAG MAC
	lda #]1
	sta ]2
	bcc :CONT_B
	jmp ]3
:CONT_B
	EOM

BEQ_FAR 	MAC  	]1
	nop
	{
		BNE    	%
		JMP    	]1
	}
	nop
	EOM

MOCMAC MAC
{
	bvc %
	jmp ]1
}
	EOM


JUST_NOPNOP MAC
	nop
	nop
	EOM

	RAGDAG 63; $d800; START

START	
	MOCMAC :NEXT

	JSR	INIT

	JUST_NOPNOP

:CONT	LDA	#00	
	BEQ_FAR	:NEXT

	LDA	#00	
	BEQ_FAR	:NEXT

	STA	TEMP
:NEXT	
	JSR	REDUCESTARSPEED

	rts

INIT
	rts
TEMP
	dc.b 0
REDUCESTARSPEED:
	rts


