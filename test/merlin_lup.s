; merlin_lup.s

  lup 6
  lda (sprt,x)
  sta ($06),y
  inc sprt
  bne :c0
  inc sprt+1
:c0   iny
  --^
  rts

sprt