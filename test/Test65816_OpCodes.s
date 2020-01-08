cpu 65816

TestOpcodes:

	brk
	jsr $2120
	jsr ($2120,x)
	jsr.l $222120
	jsl $222120
	rti
	rts
	rtl
	ora ($21,x)
	ora $21
	ora.b #$21
	ora.w #$2322
	ora $2120
	ora ($21),y
	ora $21,x
	ora $2120,y
	ora $2120,x
	ora ($21)
	ora [$21]
	ora [$21],y
	ora.l $222120
	ora.l $222120,x
	ora $21,s
	ora ($21,s),y
	and ($21,x)
	and $21
	and.b #$21
	and.w #$2322
	and $2120
	and ($21),y
	and $21,x
	and $2120,y
	and $2120,x
	and ($21)
	and [$21]
	and [$21],y
	and.l $222120
	and.l $222120,x
	and $21,s
	and ($21,s),y
	eor ($21,x)
	eor $21
	eor.b #$21
	eor.w #$2322
	eor $2120
	eor ($21),y
	eor $21,x
	eor $2120,y
	eor $2120,x
	eor ($21)
	eor [$21]
	eor [$21],y
	eor.l $222120
	eor.l $222120,x
	eor $21,s
	eor ($21,s),y
	adc ($21,x)
	adc $21
	adc.b #$21
	adc.w #$2322
	adc $2120
	adc ($21),y
	adc $21,x
	adc $2120,y
	adc $2120,x
	adc ($21)
	adc [$21]
	adc [$21],y
	adc.l $222120
	adc.l $222120,x
	adc $21,s
	adc ($21,s),y
	sta ($21,x)
	sta $21
	sta $2120
	sta ($21),y
	sta $21,x
	sta $2120,y
	sta $2120,x
	sta ($21)
	sta [$21]
	sta [$21],y
	sta.l $222120
	sta.l $222120,x
	sta $21,s
	sta ($21,s),y
	lda ($21,x)
	lda $21
	lda.b #$21
	lda.w #$2322
	lda $2120
	lda ($21),y
	lda $21,x
	lda $2120,y
	lda $2120,x
	lda ($21)
	lda [$21]
	lda [$21],y
	lda.l $222120
	lda.l $222120,x
	lda $21,s
	lda ($21,s),y
	cmp ($21,x)
	cmp $21
	cmp.b #$21
	cmp.w #$2322
	cmp $2120
	cmp ($21),y
	cmp $21,x
	cmp $2120,y
	cmp $2120,x
	cmp ($21)
	cmp [$21]
	cmp [$21],y
	cmp.l $222120
	cmp.l $222120,x
	cmp $21,s
	cmp ($21,s),y
	sbc ($21,x)
	sbc $21
	sbc.b #$21
	sbc.w #$2322
	sbc $2120
	sbc ($21),y
	sbc $21,x
	sbc $2120,y
	sbc $2120,x
	sbc ($21)
	sbc [$21]
	sbc [$21],y
	sbc.l $222120
	sbc.l $222120,x
	sbc $21,s
	sbc ($21,s),y
	oral $222120
	oral $222120,x
	andl $222120
	andl $222120,x
	eorl $222120
	eorl $222120,x
	adcl $222120
	adcl $222120,x
	stal $222120
	stal $222120,x
	ldal $222120
	ldal $222120,x
	cmpl $222120
	cmpl $222120,x
	sbcl $222120
	sbcl $222120,x
	asl $21
	asl $2120
	asl $21,x
	asl $2120,x
	asl A
	asl
	rol $21
	rol $2120
	rol $21,x
	rol $2120,x
	rol A
	rol
	lsr $21
	lsr $2120
	lsr $21,x
	lsr $2120,x
	lsr A
	lsr
	ror $21
	ror $2120
	ror $21,x
	ror $2120,x
	ror A
	ror
	stx $21
	stx $2120
	stx $21,y
	ldx $21
	ldx.b #$21
	ldx.w #$2322
	ldx $2120
	ldx $21,y
	ldx $2120,y
	dec $21
	dec $2120
	dec $21,x
	dec $2120,x
	dec A
	dec
	inc $21
	inc $2120
	inc $21,x
	inc $2120,x
	inc A
	inc
	dea
	ina
	php
	plp
	pha
	pla
	phy
	ply
	phx
	plx
	dey
	tay
	iny
	inx
	bpl *+5
	bmi *+5
	bvc *+5
	bvs *+5
	bra *+5
	brl $2120
	bcc *+5
	bcs *+5
	bne *+5
	beq *+5
	clc
	sec
	cli
	sei
	tya
	clv
	cld
	sed
	bit $21
	bit.b #$21
	bit.w #$2322
	bit $2120
	bit $21,x
	bit $2120,x
	stz $21
	stz $2120
	stz $21,x
	stz $2120,x
	trb $21
	trb $2120
	tsb $21
	tsb $2120
	jmp $2120
	jmp ($2120)
	jmp ($2120,x)
	jmp.l $222120
	jmp [$2120]
	jml.l $222120
	jml [$2120]
	sty $21
	sty $2120
	sty $21,x
	ldy $21
	ldy.b #$21
	ldy.w #$2322
	ldy $2120
	ldy $21,x
	ldy $2120,x
	cpy $21
	cpy.b #$21
	cpy.w #$2322
	cpy $2120
	cpx $21
	cpx.b #$21
	cpx.w #$2322
	cpx $2120
	txa
	txs
	tax
	tsx
	dex
	nop
	cop
	wdm
	mvp $21,$20
	mvn $21,$20
	pea $2120
	pei $21
	per $2120
	rep $21
	rep #$21
	sep $21
	sep #$21
	phd
	tcs
	pld
	tsc
	phk
	tcd
	tdc
	phb
	txy
	plb
	tyx
	wai
	stp
	xba
	xce
