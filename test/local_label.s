CheckLocalPeriod:
    lda #0
.loop
    dex
    sta $1000,x
    bne .loop

CheckLocalColon:
    lda #0
:loop
    dex
    sta $1000,x
    bne :loop

CheckLocalSnabelA:
    lda #0
@loop
    dex
    sta $1000,x
    bne @loop

CheckLocalExclamation:
    lda #0
!loop
    dex
    sta $1000,x
    bne !loop

CheckLocalDallasSign:
    lda #0
loop$
    dex
    dex
    sta $1000,x
    bne loop$



CheckLocalPeriod2:
    lda #0
.loop
    dex
    sta $1000,x
    bne .loop

CheckLocalColon2:
    lda #0
:loop
    dex
    sta $1000,x
    bne :loop

CheckLocalSnabelA2:
    lda #0
@loop
    dex
    sta $1000,x
    bne @loop

CheckLocalExclamation2:
    lda #0
!loop
    dex
    sta $1000,x
    bne !loop

CheckLocalDallasSign2:
    lda #0
loop$
    dex
    sta $1000,x
    bne loop$





