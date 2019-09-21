cpu 65816
Test65816_ForceAddrMode:
	sep #$30
i8
	ldx #$1234
a8
	lda #$1234
	lda.w #$1234

i16
	ldx #$1234
a16
	lda #$1234
	lda.b #$1234

	{
		jmp >$123456
		lda >$123456,x
		lda (<$1234,s),y
		ora ($21,x)
		jsr ($2120,x)

		lda [<$1234],y
		lda (<$1234,x)

		lda <$101030
	}

	{
		lda.z $101030 // .z zero page
		if !((*-!) == 2)
			err Expected zero page instruction
		endif
	}

	{
		lda.a $101030 // .a force absolute
		if !((*-!) == 3)
			err Expected zero page instruction
		endif
	}
	{
		lda.l $30 // .l force long
			if !((*-!) == 4)
			err Expected zero page instruction
		endif
	}

	{
		lda	<$101030 // <addr zero page
		if !((*-!) == 2)
			err Expected zero page instruction
		endif
	}

	{
		lda	|$101030 // |addr force absolute
		if !((*-!) == 3)
			err Expected zero page instruction
		endif
	}
	{
		lda	>$101030 // .l force long
			if !((*-!) == 4)
			err Expected zero page instruction
		endif
	}

	{
		jmp [$1010]
	}

	rts

