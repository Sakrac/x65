;x65macros.i
;
; letter definition
; -----------------
; The letters after the period has the following meanings:
; - b: byte
; - w: word (2 bytes)
; - t: triple (3 bytes)
; - l: long (4 bytes)
; - n: number of bytes in value
; - c: copy result to target
; - i: immediate, for example add a value to the contents of an address
; - x: use the x register for operation as a counter or an offset
; - y: use the y register for operation
; - r: relative; ry=(zp),y
; - a: use the contents of an address for operation (16 bits)
; - s: custom step size (instead of +1 or -1) for loops
; - p: positive
; - m: negative
; - o: use label pool for counter
;
; operations
; ----------
; The base operations provided by these macros are:
; - set: Assign a value to the contents of an address
; - move: Move the contents of an address to another address
; - add: addition
; - sub: subtraction
; - asrm: arithmetic shift right
; - aslm: arithmetic shift left
; - neg: negate a number
; - abs: make a number positive
; - copy: copy memory from one location to another
; - for: iterate between two numbers with optional step size
; - mnop: insert multiple nop at this point
;
; set.b / .w / .t / .l Value, Target
;   - set the contents of an 1-4 byte location to a value
;   - uses accumulator
;
; move.b / .w / .t / .l / .n Src,Trg
;   - copy 1-4 (or n) bytes from Src location to Trg location
;   - uses accumulator
;
; asrm.n Target, Size
;   - shift a signed multi byte number right
;   - uses accumulator
;
; asrm.nx Target, Size
;   - shift a signed multi byte number right offset by the x register
;   - no registers touched
;
; aslm.n Target, Size
;   - shift a multi byte number left
;   - no registers touched
;
; aslm.nx Target, Size
;   - shift a multi byte number left offset by the x register
;   - no registers changed
;
; neg.cn Source, Target, Size
;   - negate and copy a multi byte number
;   - uses accumulator
;
; neg.n Target, Size
;   - negate a number in place
;   - uses accumulator
;
; abs.n Trg, Size
;   - make a number absolute
;   - uses accumulator
;
; neg.nx Trg, Size
;   - negate a number in place offset by the x register
;   - uses accumulator
;
; add.n Address1, Address2, Target, Bytes
;   - add contents of two memory locations into a target lcoation
;   - uses accumulator
;
; sub.n Address1, Address2, Target, Bytes
;   - Target = Address1 - Address2
;   - uses accumulator
;
; add.ni Address, Value, Target, Bytes
;   - add a fixed value to a memory location into a target
;   - uses accumulator
;
; sub.ni Address, Value, Target, Bytes
;   - Target = Address - Value
;   - uses accumulator
;
; add.wi Address, Value, Target
;   - Subtract 16 bit Value from contents of Address and store at Target
;   - uses accumulator
;
; sub.wi Address1, Address2, Target
;   - add contents of two 16 bit addresses into a target 16 bit location
;   - uses accumulator
;
; mnop Count
;   - add Count nops
;
; copy.x Source, Target, Size
;   - copy up to 256 bytes using the x register as a counter
;   - uses accumulator and x register
;
; copy.y Source, Target, Size
;   - copy up to 256 bytes using the y register as a counter
;   - uses accumulator and y register
;
; copy.ry zpSrcPtr,zpTrgPtr,Size
;   - copy a fixed length buffer using relative zp y indexing
;   - size is up to a page, changing Y and A
;
; copy.ry128 zpSrcPtr,zpTrgPtr,Size
;   - copy up to 128 bytes using the y register
;
; copy.o Src,Trg,Size,PoolZP
;   - copy more than 256 bytes using zero page label pool addresses
;   - uses accumulator, x and y register
;
; copy.a Src,Trg,Size
;   - copy more than 256 bytes using absolute indexed in a loop
;   - uses accumulator, x and y register
;
; copy.zp Src,Trg,Size,zpTmp1,zpTmp2
;   - copy more than 256 bytes using two pairs of zero page values
;   - uses accumulator, x and y register
;
; for.x Start, End
;   - iterate using the x register from Start to End, End is not inclusive
;     so to iterate from 31 to 0 use for.x 31, -1
;   - uses x register
;   - end for loop with forend macro
;
; for.y Start, End
;   - same as for.x but with the y register
;   - uses y register
;   - end for loop with forend macro
;
; for.w Start, End, Counter
;   - for loop for 16 bit counter
;   - uses accumulator
;   - end for loop with forend macro
;
; for.ws Start, End, Counter, Step
;   - for loop for 16 bit counter with a step value
;   - uses accumulator
;   - end for loop with forend macro
;
;
; for.wsp Start, End, Counter, Step {
;   - for (word Counter=start; Counter<end; Counter += Step), Step>0
;   - uses accumulator
;
; for.wsm Start, End, Counter, Step {
;   - for (word Counter=start; Counter<end; Counter += Step), Step<0
;   - uses accumulator
;
; forend
;   - terminates for loops
;

macro set.b Value,Trg
{
 lda #Value
 sta Trg
}

; Set two bytes to a 16 bit value
macro set.w Value,Trg
{
 lda #<Value
 sta Trg
 lda #>Value
 sta Trg+1
}

; Set three bytes to a 24 bit value
macro set.t Value,Trg
{
  rept 3 {
   lda #Value>>(rept*8)
   sta Trg+rept
 }
}

; Set three bytes to a 24 bit value
macro set.l Value,Trg
{
  rept 4 {
   lda #Value>>(rept*8)
   sta Trg+rept
 }
}

macro move.b Src,Trg
{
  lda Src
  sta Trg
}

macro move.w Src,Trg
{
  rept 2 {
    lda Src + rept
    sta Trg + rept
  }
}

macro move.t Src,Trg
{
  rept 3 {
    lda Src + rept
    sta Trg + rept
  }
}

macro move.l Src,Trg
{
  rept 4 {
    lda Src + rept
    sta Trg + rept
  }
}

macro move.n Src,Trg,Size
{
  rept Size {
    lda Src + rept
    sta Trg + rept
  }
}

; shift a signed multi byte number right
macro asrm.n Trg,Size
{
  lda Trg+Size-1
  asl
  rept Size {
    ror 0 + (Trg - 1 + Size - rept)
  }
}

; shift a signed multi byte number right offset by the x register
macro asrm.nx Trg,Size
{
  lda Trg+Size-1,x
  asl
  rept Size {
    ror 0 + (Trg + Size - 1 - rept), x
  }
}

; shift a multi byte number left
macro aslm.n Trg,Size
{
  asl Trg
  rept Size-1 {
    rol Trg+1+rept
  }
}

; shift a multi byte number left offset by the x register
macro aslm.nx Trg,Size
{
  asl Trg,x
  rept Size-1 {
    rol Trg+1+rept,x
  }
}

; negate and copy a multi byte number
macro neg.cn Src, Trg, Size
{
  sec
  rept Size {
    lda #0
    sbc Src + rept
    sta Trg + rept
  }
}

; negate a number in place
macro neg.n Trg, Size
{
  sec
  rept Size {
    lda #0
    sbc Trg + rept
    sta Trg + rept
  }
}

; negate a number in place offset by the x register
macro neg.nx Trg, Size
{
  sec
  rept Size {
    lda #0
    sbc Trg + rept,x
    sta Trg + rept,x
  }
}

; make a number absolute
macro abs.n Trg, Size
{
  lda Trg+Size-1
  bpl %
  sec
  rept Size {
    lda #0
    sbc Trg + rept
    sta Trg + rept
  }
}

; add two numbers together (A and B and Trg are addresses)
macro add.n A,B,Trg,NumSize
{
 clc
 rept NumSize {
  lda A+rept
  adc B+rept
  sta Trg+rept
 }
}

; add two numbers together (A and B and Trg are addresses)
macro sub.n A,B,Trg,NumSize
{
 sec
 rept NumSize {
  lda A+rept
  sbc B+rept
  sta Trg+rept
 }
}

; add a fixed value to an N byte number and store at Trg
macro add.ni Src,Value,Trg,NumSize
{
 clc
 rept NumSize {
  lda Src+rept
  adc #Value>>(8*rept)
  sta Trg+rept
 }
}

; add a fixed value to an N byte number and store at Trg
macro sub.ni Src,Value,Trg,NumSize
{
 sec
 rept NumSize {
  lda Src+rept
  sbc #Value>>(8*rept)
  sta Trg+rept
 }
}

; add a fixed value to a two byte number and store at Trg
macro add.wi Src,Value,Trg
{
 clc
 lda #<Value
 adc Src
 sta Trg
 lda #>Value
 adc Src+1
 sta Trg+1
}

; add a fixed value to a two byte number and store at Trg
macro sub.wi Src,Value,Trg
{
 sec
 lda Src
 sbc #<Value
 sta Trg
 lda Src+1
 sbc #>Value
 sta Trg+1
}

; insert multiple nops
macro mnop Count {
  rept Count {
    nop
  }
}

; copy a fixed length buffer from one place to another
; size is up to a page, changing X and A
macro copy.x Src,Trg,Size
{
 if Size==0
 elif Size==1
  lda Src
  sta Trg
 elif Size<129
  ldx #Size-1
  {
   lda Src,x
   sta Trg,x
   dex
   bpl !
  }
 elif Size<256
  ldx #0
  {
   lda Src,x
   sta Trg,x
   inx
   cpx #size
   bne !
  }
 else
  error copy.x can only copy up to 256 bytes, use copy.p to copy Size bytes
 endif
}

; copy a fixed length buffer from one place to another
; size is up to a page, changing Y and A
macro copy.y Src,Trg,Size
{
 if Size==0
 elif Size==1
  lda Src
  sta Trg
 elif Size<129
  ldy #Size-1
  {
   lda Src,y
   sta Trg,y
   dey
   bpl !
  }
 elif Size<256
  ldy #0
  {
   lda Src,y
   sta Trg,y
   iny
   cpy #size
   bne !
  }
 else
  error copy.x can only copy up to 256 bytes, use copy.p to copy Size bytes
 endif
}

; copy a fixed length buffer using relative zp y indexing
; size is up to a page, changing Y and A
macro copy.ry zpSrcPtr,zpTrgPtr,Size
{
 if (Size) > 256
  error copy.ry can only copy up to 256 bytes
 elif (Size) > 0
  ldy #Size-1
  {
   lda (zpSrcPtr),y
   sta (zpTrgPtr),y
   dey
   if (Size) > 128
    cpy #$ff
    bne !
   else
    bpl !
   endif
  }
 endif
}

; copy up to 128 bytes using the y register
macro copy.ry128 zpSrcPtr,zpTrgPtr,Size
{
  ldy #Size-1
  {
   lda (zpSrcPtr),y
   sta (zpTrgPtr),y
   dey
   bpl !
  }
}

; copy pages using temp zero page registers
; falls back on CopyF if less than or equal to a page
; changes x, y and A
macro copy.o Src,Trg,Size,PoolZP
{
 if (Size<256)
  copy.x Src,Trg,Size
 else
 {
  PoolZP zpSrc.w
  PoolZP zpTrg.w
  set.w zpSrc,Src
  set.w zpTrg,Trg
  ldx #>Size
  ldy #0
  {
   {
    lda (zpSrc),y
    sta (zpTrg),y
    iny
    bne !
   }
   inc zpSrc+1
   inc zpTrg+1
   dex
   bne !
  }
  if Size & $ff
   ldy #Size-1
   {
    lda (zpSrc),y
    sta (zpTrg),y
    dey
    if (Size & $ff)<129
     bpl !
    else
     cpy #$ff
     bne !
    endif
   }
  endif
 }
 endif
}

macro copy.zp Src,Trg,Size,zpTmp1,zpTmp2
{
 if (Size<256)
  copy.x Src,Trg,Size
 else
 {
  set.w zpTmp1,Src
  set.w zpTmp2,Trg
  ldx #>Size
  ldy #0
  {
   {
    lda (zpTmp1),y
    sta (zpTmp2),y
    iny
    bne !
   }
   inc zpTmp1+1
   inc zpTmp2+1
   dex
   bne !
  }
  if Size & $ff
   ldy #Size-1
   {
    lda (zpTmp1),y
    sta (zpTmp2),y
    dey
    if (Size & $ff)<129
     bpl !
    else
     cpy #$ff
     bne !
    endif
   }
  endif
 }
 endif
}

macro copy.a Src,Trg,Size
{
 if (Size<256)
  copy.x Src,Trg,Size
 else
 {
  set.b >Src, ._addr+2
  set.b >Trg, ._addr+5
  ldy #>Size
  ldx #0
._addr
  {
   {
    lda Src,x
    sta Trg,x
    inx
    bne !
   }
   inc ._addr+2
   inc ._addr+5
   dey
   bne !
  }
  if Size & $ff
   ldx #(Size&$ff)-1
   {
    lda Src+(Size & $ff00),x
    sta Trg+(Size & $ff00),x
    dex
    if (Size & $ff)<129
     bpl !
    else
     cpx #$ff
     bne !
    endif
   }
  endif
 }
 endif
}

; for (x=start; x<end; x++)
macro for.x Start, End {
  ldx #Start
  if Start < End
    string _ForEnd = "inx\ncpx #End\nbne _ForLoop"
  elif Start > End
  {
    if (-1 == End) & (Start<129)
      string _ForEnd = "dex\nbpl _ForLoop"
    else
      string _ForEnd = "dex\ncpx #End\nbne _ForLoop"
    endif
  }
  else
    string _ForEnd = ""
  endif
_ForLoop
}

; for (y=start; y<end; y++)
macro for.y Start, End {
  ldx #Start
  if Start < End
    string _ForEnd = "iny\ncpy #End\nbne _ForLoop"
  elif Start > End
  {
    if (-1 == End) & (Start<129)
      string _ForEnd = "dey\nbpl _ForLoop"
    else
      string _ForEnd = "dey\ncpy #End\nbne _ForLoop"
    endif
  }
  else
    string _ForEnd = ""
  endif
_ForLoop
}

; for (Counter=start; Counter<end; Counter++)
macro for.w Start, End, Counter {
  set.w Start, Counter
  if (Start) < (End)
    string _ForEnd = "{\ninc Counter\nbne %\ninc Counter+1\n}\nlda Counter+1\ncmp #>End\nbne _ForLoop\nlda Counter\ncmp #<End\nbne _ForLoop"
  elif (Start) > (End)
    string _ForEnd = "{\ndec Counter\nbne %\ndec Counter+1\n}\nlda Counter+1\ncmp #>End\nbne _ForLoop\nlda Counter\ncmp #<End\nbne _ForLoop"
  else
    string _ForEnd = ""
  endif
_ForLoop
}

macro forend {
  _ForEnd
  undef _ForEnd
}

; for (word Counter=start; Counter<end; Counter += Step), check Step sign to determine direction
macro for.ws Start, End, Counter, Step {
  set.w Start, Counter
  if Start < End
    if ((Step)<1)
     error Step is not a valid iterator for range Start to End
    endif
    string _ForEnd = "clc\nlda #<Step\nadc Counter\nsta Counter\nlda #>Step\n adc Counter+1\nsta Counter+1\ncmp #>End\nbcc _ForLoop\nlda Counter\ncmp #<End\nbcc _ForLoop"
  elif Start > End
    if ((Step)>-1)
     error Step is not a valid iterator for range Start to End
    endif
    string _ForEnd = "sec\nlda Counter\n sbc #<(-Step)\nsta Counter\nlda Counter+1\nsbc #>(-Step)\nsta Counter+1\ncmp #(>End)+1\nbcs _ForLoop\nlda Counter\ncmp #(<End)+1\n\nbcs _ForLoop"
  else
    string _ForEnd = ""
  endif
_ForLoop
}

; for (word Counter=start; Counter<end; Counter += Step), Step>0
macro for.wsp Start, End, Counter, Step {
  set.w Start, Counter
  string _ForEnd = "clc\nlda #<Step\nadc Counter\nsta Counter\nlda #>Step\n adc Counter+1\nsta Counter+1\ncmp #>End\nbcc _ForLoop\nlda Counter\ncmp #<End\nbcc _ForLoop"
_ForLoop
}

; for (word Counter=start; Counter<end; Counter += Step), Step<0
macro for.wsm Start, End, Counter, Step {
  set.w Start, Counter
  string _ForEnd = "sec\nlda Counter\n sbc #<(-Step)\nsta Counter\nlda Counter+1\nsbc #>(-Step)\nsta Counter+1\ncmp #(>End)+1\nbcs _ForLoop\nlda Counter\ncmp #(<End)+1\n\nbcs _ForLoop"
_ForLoop
}

macro forend {
  _ForEnd
  undef _ForEnd
  undef _ForLoop
}
