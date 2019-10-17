; Test macro defining a label
macro LabelMacro( lbl, str, len ) {
lbl:
	TEXT str
const len = * - lbl
}

dc.w Label1
dc.b Label1_Len


LabelMacro Label1, "Hey!", Label1_Len

	include "../macros/x65macro.i"

	sec
	bcs Begin

CopyCode
	inx
	dey
	nop
CodeEnd

CodeSegLen = CodeEnd-CopyCode
	; $fc = CopyCode
	; for ($fe=$2000; $fe<$4000; $fe += (CodeEnd-CopyCode)) {
	;  memcpy($fe, $fc, CodeEnd-CopyCode)
	; }
Begin:
	set.w CopyCode, $fc
	for.wsp $2000, $4000, $fe, CodeSegLen
	 copy.ry128 $fc, $fe, CodeSegLen
	forend

	nop
	nop
	nop

	; int $fc
	; $fc >>= 1
	asrm.n $fc,4

	nop
	nop
	nop

	ldx #$0c
	aslm.nx $f0,4

	nop
	nop
	nop

	; int $fc
	; $fc = -$fc

	neg.n $fc,4

	nop
	nop
	nop

	; int $fc = abs($fc)
	abs.n $fc, 4

