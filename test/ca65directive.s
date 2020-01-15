; TEST CODE FROM EXOMIZER
cpu 65816
.org $52000

.REPT 7
        dc.b rept
.ENDR

eval Checking defined function, Should be 0: .defined(test_stack)
test_stack = 0
eval Checking referenced function, Should be 0: .referenced(test_stack)
eval Checking defined function, Should be 1: .defined(test_stack)
PUSH test_stack
eval Checking referenced function, Should be 1: .referenced(test_stack)
test_stack = 10
eval Push Before Pull: test_stack
PULL test_stack
eval Pull original: test_stack

eval Checking symbol is not const (0): .const(test_stack)
const ConstAddress = $1000
eval Checking symbol is const (1): .const(ConstAddress)

eval This should be blank (1): .blank()
eval This should be blank (1): .blank({})
eval This should be not be blank (0): .blank({monkeys})

.ifconst test_stack
eval Checking ifconst with non-const symbol, should not print:
.endif

.ifconst ConstAddress
eval Checking ifconst with const symbol, this should print:
.endif

struct MyStruct {
        word addr
        byte value
}

eval Size of MyStruct (3): .sizeof(MyStruct)

DISP_BRIGHTNESS_MASK    = $f0
DISP_BLANKING_SHIFT     = 7

.function inidisp(blanking, brightness) (<(((~blanking & 1) << DISP_BLANKING_SHIFT) | (brightness & ~DISP_BRIGHTNESS_MASK)))

eval Function test, should be (<(((~7&1)<<7)|(12&~$f0) = <((0<<7)|(c&f)) = $c: inidisp(7, 12)

zp_len_lo = $a7
zp_len_hi = $a8

zp_src_lo  = $ae
zp_src_hi  = zp_src_lo + 1

zp_bits_hi = $fc

zp_bitbuf  = $fd
zp_dest_lo = zp_bitbuf + 1      ; dest addr lo
zp_dest_hi = zp_bitbuf + 2      ; dest addr hi

.MACRO mac_refill_bits
        pha
        jsr get_crunched_byte
        rol
        sta zp_bitbuf
        pla
.ENDMACRO
.MACRO mac_get_bits
.SCOPE
        adc #$80                ; needs c=0, affects v
        asl
        bpl gb_skip
gb_next:
        asl zp_bitbuf
        bne gb_ok
        mac_refill_bits
gb_ok:
        rol
        bmi gb_next
gb_skip:
        bvc skip
gb_get_hi:
        sec
        sta zp_bits_hi
        jsr get_crunched_byte
skip:
.ENDSCOPE
.ENDMACRO


.ifdef UNDEFINED_SYMBOL
	dc.b -1 ; should not be assembled
	error 1
.else
	dc.b 1 ; should be assembled
.endif

const CONSTANT = 32

.eval CONSTANT

	mac_get_bits
	mac_get_bits

get_crunched_byte:
	rts