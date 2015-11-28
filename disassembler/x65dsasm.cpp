
//
//  x65.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/23/15.
//
//
//	x65 companion disassembler
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 Carl-Henrik Skårstedt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Details, source and documentation at https://github.com/Sakrac/x65.
//
// "struse.h" can be found at https://github.com/Sakrac/struse, only the header file is required.
//

#define _CRT_SECURE_NO_WARNINGS		// Windows shenanigans
#define STRUSE_IMPLEMENTATION		// include implementation of struse in this file
#include "struse.h"					// https://github.com/Sakrac/struse/blob/master/struse.h
#include <stdio.h>
#include <stdlib.h>
#include <vector>

static const char* aAddrModeFmt[] = {
	"%s ($%02x,x)",			// 00
	"%s $%02x",				// 01
	"%s #$%02x",			// 02
	"%s $%04x",				// 03
	"%s ($%02x),y",			// 04
	"%s $%02x,x",			// 05
	"%s $%04x,y",			// 06
	"%s $%04x,x",			// 07
	"%s ($%04x)",			// 08
	"%s A",					// 09
	"%s ",					// 0a
	"%s ($%02x)",			// 0b
	"%s ($%04x,x)",			// 0c
	"%s $%02x, $%04x",		// 0d
	"%s [$%02x]",			// 0e
	"%s [$%02x],y",			// 0f
	"%s $%06x",				// 10
	"%s $%06x,x",			// 11
	"%s $%02x,s",			// 12
	"%s ($%02x,s),y",		// 13
	"%s [$%04x]",			// 14
	"%s $%02x,$%02x",		// 15

	"%s $%02x,y",			// 16
	"%s ($%02x,y)",			// 17

	"%s #$%02x",			// 18
	"%s #$%02x",			// 19

	"%s $%04x",				// 1a
	"%s $%04x",				// 1b
};

static const char* aAddrModeFmtSrc[] = {
	"%s (%s,x)",			// 00
	"%s %s",				// 01
	"%s #%s",				// 02
	"%s %s",				// 03
	"%s (%s),y",			// 04
	"%s %s,x",				// 05
	"%s %s,y",				// 06
	"%s %s,x",				// 07
	"%s (%s)",				// 08
	"%s A",					// 09
	"%s ",					// 0a
	"%s (%s)",				// 0b
	"%s (%s,x)",			// 0c
	"%s $%02x, %s",			// 0d
	"%s [%s]",				// 0e
	"%s [%s],y",			// 0f
	"%s %s",				// 10
	"%s %s,x",				// 11
	"%s %s,s",				// 12
	"%s (%s,s),y",			// 13
	"%s [%s]",				// 14
	"%s $%02x,%s",			// 15

	"%s %s,y",				// 16
	"%s (%s,y)",			// 17

	"%s #%s",				// 18
	"%s #%s",				// 19

	"%s %s",				// 1a
	"%s %s",				// 1b
};

enum AddressModes {
	// address mode bit index

	// 6502

	AM_ZP_REL_X,	// 0 ($12,x)
	AM_ZP,			// 1 $12
	AM_IMM,			// 2 #$12
	AM_ABS,			// 3 $1234
	AM_ZP_Y_REL,	// 4 ($12),y
	AM_ZP_X,		// 5 $12,x
	AM_ABS_Y,		// 6 $1234,y
	AM_ABS_X,		// 7 $1234,x
	AM_REL,			// 8 ($1234)
	AM_ACC,			// 9 A
	AM_NON,			// a

	// 65C02

	AM_ZP_REL,		// b ($12)
	AM_REL_X,		// c ($1234,x)
	AM_ZP_ABS,		// d $12, *+$12

	// 65816

	AM_ZP_REL_L,	// e [$02]
	AM_ZP_REL_Y_L,	// f [$00],y
	AM_ABS_L,		// 10 $bahilo
	AM_ABS_L_X,		// 11 $123456,x
	AM_STK,			// 12 $12,s
	AM_STK_REL_Y,	// 13 ($12,s),y
	AM_REL_L,		// 14 [$1234]
	AM_BLK_MOV,		// 15 $12,$34

	AM_ZP_Y,		// 16 stx/ldx
	AM_ZP_REL_Y,	// 17 sax/lax/ahx

	AM_IMM_DBL_A,	// 18 #$12/#$1234
	AM_IMM_DBL_I,	// 19 #$12/#$1234

	AM_BRANCH,		// beq $1234
	AM_BRANCH_L,	// brl $1234

	AM_COUNT,
};

struct dismnm {
	const char *name;
	unsigned char addrMode;
	unsigned char arg_size;
};

struct dismnm a6502_ops[256] = {
	{ "brk", AM_NON, 0x00 },
	{ "ora", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "slo", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ora", AM_ZP, 0x01 },
	{ "asl", AM_ZP, 0x01 },
	{ "slo", AM_ZP, 0x01 },
	{ "php", AM_NON, 0x00 },
	{ "ora", AM_IMM, 0x01 },
	{ "asl", AM_NON, 0x00 },
	{ "anc", AM_IMM, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ora", AM_ABS, 0x02 },
	{ "asl", AM_ABS, 0x02 },
	{ "slo", AM_ABS, 0x02 },
	{ "bpl", AM_BRANCH, 0x01 },
	{ "ora", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "slo", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ora", AM_ZP_X, 0x01 },
	{ "asl", AM_ZP_X, 0x01 },
	{ "slo", AM_ZP_X, 0x01 },
	{ "clc", AM_NON, 0x00 },
	{ "ora", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "slo", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "ora", AM_ABS_X, 0x02 },
	{ "asl", AM_ABS_X, 0x02 },
	{ "slo", AM_ABS_X, 0x02 },
	{ "jsr", AM_ABS, 0x02 },
	{ "and", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "rla", AM_ZP_REL_X, 0x01 },
	{ "bit", AM_ZP, 0x01 },
	{ "and", AM_ZP, 0x01 },
	{ "rol", AM_ZP, 0x01 },
	{ "rla", AM_ZP, 0x01 },
	{ "plp", AM_NON, 0x00 },
	{ "and", AM_IMM, 0x01 },
	{ "rol", AM_NON, 0x00 },
	{ "aac", AM_IMM, 0x01 },
	{ "bit", AM_ABS, 0x02 },
	{ "and", AM_ABS, 0x02 },
	{ "rol", AM_ABS, 0x02 },
	{ "rla", AM_ABS, 0x02 },
	{ "bmi", AM_BRANCH, 0x01 },
	{ "and", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "rla", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "and", AM_ZP_X, 0x01 },
	{ "rol", AM_ZP_X, 0x01 },
	{ "rla", AM_ZP_X, 0x01 },
	{ "sec", AM_NON, 0x00 },
	{ "and", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "rla", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "and", AM_ABS_X, 0x02 },
	{ "rol", AM_ABS_X, 0x02 },
	{ "rla", AM_ABS_X, 0x02 },
	{ "rti", AM_NON, 0x00 },
	{ "eor", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sre", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ZP, 0x01 },
	{ "lsr", AM_ZP, 0x01 },
	{ "sre", AM_ZP, 0x01 },
	{ "pha", AM_NON, 0x00 },
	{ "eor", AM_IMM, 0x01 },
	{ "lsr", AM_NON, 0x00 },
	{ "alr", AM_IMM, 0x01 },
	{ "jmp", AM_ABS, 0x02 },
	{ "eor", AM_ABS, 0x02 },
	{ "lsr", AM_ABS, 0x02 },
	{ "sre", AM_ABS, 0x02 },
	{ "bvc", AM_BRANCH, 0x01 },
	{ "eor", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sre", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ZP_X, 0x01 },
	{ "lsr", AM_ZP_X, 0x01 },
	{ "sre", AM_ZP_X, 0x01 },
	{ "cli", AM_NON, 0x00 },
	{ "eor", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "sre", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ABS_X, 0x02 },
	{ "lsr", AM_ABS_X, 0x02 },
	{ "sre", AM_ABS_X, 0x02 },
	{ "rts", AM_NON, 0x00 },
	{ "adc", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "rra", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "adc", AM_ZP, 0x01 },
	{ "ror", AM_ZP, 0x01 },
	{ "rra", AM_ZP, 0x01 },
	{ "pla", AM_NON, 0x00 },
	{ "adc", AM_IMM, 0x01 },
	{ "ror", AM_NON, 0x00 },
	{ "arr", AM_IMM, 0x01 },
	{ "jmp", AM_REL, 0x02 },
	{ "adc", AM_ABS, 0x02 },
	{ "ror", AM_ABS, 0x02 },
	{ "rra", AM_ABS, 0x02 },
	{ "bvs", AM_BRANCH, 0x01 },
	{ "adc", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "rra", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "adc", AM_ZP_X, 0x01 },
	{ "ror", AM_ZP_X, 0x01 },
	{ "rra", AM_ZP_X, 0x01 },
	{ "sei", AM_NON, 0x00 },
	{ "adc", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "rra", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "adc", AM_ABS_X, 0x02 },
	{ "ror", AM_ABS_X, 0x02 },
	{ "rra", AM_ABS_X, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "sta", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sax", AM_ZP_REL_Y, 0x01 },
	{ "sty", AM_ZP, 0x01 },
	{ "sta", AM_ZP, 0x01 },
	{ "stx", AM_ZP, 0x01 },
	{ "sax", AM_ZP, 0x01 },
	{ "dey", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "txa", AM_NON, 0x00 },
	{ "xaa", AM_IMM, 0x01 },
	{ "sty", AM_ABS, 0x02 },
	{ "sta", AM_ABS, 0x02 },
	{ "stx", AM_ABS, 0x02 },
	{ "sax", AM_ABS, 0x02 },
	{ "bcc", AM_BRANCH, 0x01 },
	{ "sta", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ahx", AM_ZP_REL_Y, 0x01 },
	{ "sty", AM_ZP_X, 0x01 },
	{ "sta", AM_ZP_X, 0x01 },
	{ "stx", AM_ZP_Y, 0x01 },
	{ "sax", AM_ZP_Y, 0x01 },
	{ "tya", AM_NON, 0x00 },
	{ "sta", AM_ABS_Y, 0x02 },
	{ "txs", AM_NON, 0x00 },
	{ "tas", AM_ABS_Y, 0x02 },
	{ "shy", AM_ABS_X, 0x02 },
	{ "sta", AM_ABS_X, 0x02 },
	{ "shx", AM_ABS_Y, 0x02 },
	{ "ahx", AM_ABS_Y, 0x02 },
	{ "ldy", AM_IMM, 0x01 },
	{ "lda", AM_ZP_REL_X, 0x01 },
	{ "ldx", AM_IMM, 0x01 },
	{ "lax", AM_ZP_REL_Y, 0x01 },
	{ "ldy", AM_ZP, 0x01 },
	{ "lda", AM_ZP, 0x01 },
	{ "ldx", AM_ZP, 0x01 },
	{ "lax", AM_ZP, 0x01 },
	{ "tay", AM_NON, 0x00 },
	{ "lda", AM_IMM, 0x01 },
	{ "tax", AM_NON, 0x00 },
	{ "lax2", AM_IMM, 0x01 },
	{ "ldy", AM_ABS, 0x02 },
	{ "lda", AM_ABS, 0x02 },
	{ "ldx", AM_ABS, 0x02 },
	{ "lax", AM_ABS, 0x02 },
	{ "bcs", AM_BRANCH, 0x01 },
	{ "lda", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "ldy", AM_ZP_X, 0x01 },
	{ "lda", AM_ZP_X, 0x01 },
	{ "ldx", AM_ZP_Y, 0x01 },
	{ "lax", AM_ZP_Y, 0x01 },
	{ "clv", AM_NON, 0x00 },
	{ "lda", AM_ABS_Y, 0x02 },
	{ "tsx", AM_NON, 0x00 },
	{ "las", AM_ABS_Y, 0x02 },
	{ "ldy", AM_ABS_X, 0x02 },
	{ "lda", AM_ABS_X, 0x02 },
	{ "ldx", AM_ABS_Y, 0x02 },
	{ "lax", AM_ABS_Y, 0x02 },
	{ "cpy", AM_IMM, 0x01 },
	{ "cmp", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "dcp", AM_ZP_REL_X, 0x01 },
	{ "cpy", AM_ZP, 0x01 },
	{ "cmp", AM_ZP, 0x01 },
	{ "dec", AM_ZP, 0x01 },
	{ "dcp", AM_ZP, 0x01 },
	{ "iny", AM_NON, 0x00 },
	{ "cmp", AM_IMM, 0x01 },
	{ "dex", AM_NON, 0x00 },
	{ "axs", AM_IMM, 0x01 },
	{ "cpy", AM_ABS, 0x02 },
	{ "cmp", AM_ABS, 0x02 },
	{ "dec", AM_ABS, 0x02 },
	{ "dcp", AM_ABS, 0x02 },
	{ "bne", AM_BRANCH, 0x01 },
	{ "cmp", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "dcp", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "cmp", AM_ZP_X, 0x01 },
	{ "dec", AM_ZP_X, 0x01 },
	{ "dcp", AM_ZP_X, 0x01 },
	{ "cld", AM_NON, 0x00 },
	{ "cmp", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "dcp", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "cmp", AM_ABS_X, 0x02 },
	{ "dec", AM_ABS_X, 0x02 },
	{ "dcp", AM_ABS_X, 0x02 },
	{ "cpx", AM_IMM, 0x01 },
	{ "sbc", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "isc", AM_ZP_REL_X, 0x01 },
	{ "cpx", AM_ZP, 0x01 },
	{ "sbc", AM_ZP, 0x01 },
	{ "inc", AM_ZP, 0x01 },
	{ "isc", AM_ZP, 0x01 },
	{ "inx", AM_NON, 0x00 },
	{ "sbc", AM_IMM, 0x01 },
	{ "nop", AM_NON, 0x00 },
	{ "sbi", AM_IMM, 0x01 },
	{ "cpx", AM_ABS, 0x02 },
	{ "sbc", AM_ABS, 0x02 },
	{ "inc", AM_ABS, 0x02 },
	{ "isc", AM_ABS, 0x02 },
	{ "beq", AM_BRANCH, 0x01 },
	{ "sbc", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "isc", AM_ZP_Y_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sbc", AM_ZP_X, 0x01 },
	{ "inc", AM_ZP_X, 0x01 },
	{ "isc", AM_ZP_X, 0x01 },
	{ "sed", AM_NON, 0x00 },
	{ "sbc", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "isc", AM_ABS_Y, 0x02 },
	{ "???", AM_NON, 0x00 },
	{ "sbc", AM_ABS_X, 0x02 },
	{ "inc", AM_ABS_X, 0x02 },
	{ "isc", AM_ABS_X, 0x02 },
};

struct dismnm a65C02_ops[256] = {
	{ "brk", AM_NON, 0x00 },
	{ "ora", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "tsb", AM_ZP, 0x01 },
	{ "ora", AM_ZP, 0x01 },
	{ "asl", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "php", AM_NON, 0x00 },
	{ "ora", AM_IMM, 0x01 },
	{ "asl", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "tsb", AM_ABS, 0x02 },
	{ "ora", AM_ABS, 0x02 },
	{ "asl", AM_ABS, 0x02 },
	{ "bbr0", AM_ZP_ABS, 0x02 },
	{ "bpl", AM_BRANCH, 0x01 },
	{ "ora", AM_ZP_Y_REL, 0x01 },
	{ "ora", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "trb", AM_ZP, 0x01 },
	{ "ora", AM_ZP_X, 0x01 },
	{ "asl", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "clc", AM_NON, 0x00 },
	{ "ora", AM_ABS_Y, 0x02 },
	{ "inc", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "trb", AM_ABS, 0x02 },
	{ "ora", AM_ABS_X, 0x02 },
	{ "asl", AM_ABS_X, 0x02 },
	{ "bbr1", AM_ZP_ABS, 0x02 },
	{ "jsr", AM_ABS, 0x02 },
	{ "and", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "bit", AM_ZP, 0x01 },
	{ "and", AM_ZP, 0x01 },
	{ "rol", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "plp", AM_NON, 0x00 },
	{ "and", AM_IMM, 0x01 },
	{ "rol", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "bit", AM_ABS, 0x02 },
	{ "and", AM_ABS, 0x02 },
	{ "rol", AM_ABS, 0x02 },
	{ "bbr2", AM_ZP_ABS, 0x02 },
	{ "bmi", AM_BRANCH, 0x01 },
	{ "and", AM_ZP_Y_REL, 0x01 },
	{ "and", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "bit", AM_ZP_X, 0x01 },
	{ "and", AM_ZP_X, 0x01 },
	{ "rol", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sec", AM_NON, 0x00 },
	{ "and", AM_ABS_Y, 0x02 },
	{ "dec", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "bit", AM_ABS_X, 0x02 },
	{ "and", AM_ABS_X, 0x02 },
	{ "rol", AM_ABS_X, 0x02 },
	{ "bbr3", AM_ZP_ABS, 0x02 },
	{ "rti", AM_NON, 0x00 },
	{ "eor", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ZP, 0x01 },
	{ "lsr", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "pha", AM_NON, 0x00 },
	{ "eor", AM_IMM, 0x01 },
	{ "lsr", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "jmp", AM_ABS, 0x02 },
	{ "eor", AM_ABS, 0x02 },
	{ "lsr", AM_ABS, 0x02 },
	{ "bbr4", AM_ZP_ABS, 0x02 },
	{ "bvc", AM_BRANCH, 0x01 },
	{ "eor", AM_ZP_Y_REL, 0x01 },
	{ "eor", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ZP_X, 0x01 },
	{ "lsr", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "cli", AM_NON, 0x00 },
	{ "eor", AM_ABS_Y, 0x02 },
	{ "phy", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "eor", AM_ABS_X, 0x02 },
	{ "lsr", AM_ABS_X, 0x02 },
	{ "bbr5", AM_ZP_ABS, 0x02 },
	{ "rts", AM_NON, 0x00 },
	{ "adc", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "stz", AM_ZP, 0x01 },
	{ "adc", AM_ZP, 0x01 },
	{ "ror", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "pla", AM_NON, 0x00 },
	{ "adc", AM_IMM, 0x01 },
	{ "ror", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "jmp", AM_REL, 0x02 },
	{ "adc", AM_ABS, 0x02 },
	{ "ror", AM_ABS, 0x02 },
	{ "bbr6", AM_ZP_ABS, 0x02 },
	{ "bvs", AM_BRANCH, 0x01 },
	{ "adc", AM_ZP_Y_REL, 0x01 },
	{ "adc", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "stz", AM_ZP_X, 0x01 },
	{ "adc", AM_ZP_X, 0x01 },
	{ "ror", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sei", AM_NON, 0x00 },
	{ "adc", AM_ABS_Y, 0x02 },
	{ "ply", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "jmp", AM_REL_X, 0x02 },
	{ "adc", AM_ABS_X, 0x02 },
	{ "ror", AM_ABS_X, 0x02 },
	{ "bbr7", AM_ZP_ABS, 0x02 },
	{ "bra", AM_BRANCH, 0x01 },
	{ "sta", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "sty", AM_ZP, 0x01 },
	{ "sta", AM_ZP, 0x01 },
	{ "stx", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "dey", AM_NON, 0x00 },
	{ "bit", AM_IMM, 0x01 },
	{ "txa", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "sty", AM_ABS, 0x02 },
	{ "sta", AM_ABS, 0x02 },
	{ "stx", AM_ABS, 0x02 },
	{ "bbs0", AM_ZP_ABS, 0x02 },
	{ "bcc", AM_BRANCH, 0x01 },
	{ "sta", AM_ZP_Y_REL, 0x01 },
	{ "sta", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sty", AM_ZP_X, 0x01 },
	{ "sta", AM_ZP_X, 0x01 },
	{ "stx", AM_ZP_Y, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "tya", AM_NON, 0x00 },
	{ "sta", AM_ABS_Y, 0x02 },
	{ "txs", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "stz", AM_ABS, 0x02 },
	{ "sta", AM_ABS_X, 0x02 },
	{ "stz", AM_ABS_X, 0x02 },
	{ "bbs1", AM_ZP_ABS, 0x02 },
	{ "ldy", AM_IMM, 0x01 },
	{ "lda", AM_ZP_REL_X, 0x01 },
	{ "ldx", AM_IMM, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ldy", AM_ZP, 0x01 },
	{ "lda", AM_ZP, 0x01 },
	{ "ldx", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "tay", AM_NON, 0x00 },
	{ "lda", AM_IMM, 0x01 },
	{ "tax", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "ldy", AM_ABS, 0x02 },
	{ "lda", AM_ABS, 0x02 },
	{ "ldx", AM_ABS, 0x02 },
	{ "bbs2", AM_ZP_ABS, 0x02 },
	{ "bcs", AM_BRANCH, 0x01 },
	{ "lda", AM_ZP_Y_REL, 0x01 },
	{ "lda", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "ldy", AM_ZP_X, 0x01 },
	{ "lda", AM_ZP_X, 0x01 },
	{ "ldx", AM_ZP_Y, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "clv", AM_NON, 0x00 },
	{ "lda", AM_ABS_Y, 0x02 },
	{ "tsx", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "ldy", AM_ABS_X, 0x02 },
	{ "lda", AM_ABS_X, 0x02 },
	{ "ldx", AM_ABS_Y, 0x02 },
	{ "bbs3", AM_ZP_ABS, 0x02 },
	{ "cpy", AM_IMM, 0x01 },
	{ "cmp", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "cpy", AM_ZP, 0x01 },
	{ "cmp", AM_ZP, 0x01 },
	{ "dec", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "iny", AM_NON, 0x00 },
	{ "cmp", AM_IMM, 0x01 },
	{ "dex", AM_NON, 0x00 },
	{ "wai", AM_NON, 0x00 },
	{ "cpy", AM_ABS, 0x02 },
	{ "cmp", AM_ABS, 0x02 },
	{ "dec", AM_ABS, 0x02 },
	{ "bbs4", AM_ZP_ABS, 0x02 },
	{ "bne", AM_BRANCH, 0x01 },
	{ "cmp", AM_ZP_Y_REL, 0x01 },
	{ "cmp", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "cmp", AM_ZP_X, 0x01 },
	{ "dec", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "cld", AM_NON, 0x00 },
	{ "cmp", AM_ABS_Y, 0x02 },
	{ "phx", AM_NON, 0x00 },
	{ "stp", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "cmp", AM_ABS_X, 0x02 },
	{ "dec", AM_ABS_X, 0x02 },
	{ "bbs5", AM_ZP_ABS, 0x02 },
	{ "cpx", AM_IMM, 0x01 },
	{ "sbc", AM_ZP_REL_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "cpx", AM_ZP, 0x01 },
	{ "sbc", AM_ZP, 0x01 },
	{ "inc", AM_ZP, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "inx", AM_NON, 0x00 },
	{ "sbc", AM_IMM, 0x01 },
	{ "nop", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "cpx", AM_ABS, 0x02 },
	{ "sbc", AM_ABS, 0x02 },
	{ "inc", AM_ABS, 0x02 },
	{ "bbs6", AM_ZP_ABS, 0x02 },
	{ "beq", AM_BRANCH, 0x01 },
	{ "sbc", AM_ZP_Y_REL, 0x01 },
	{ "sbc", AM_ZP_REL, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "sbc", AM_ZP_X, 0x01 },
	{ "inc", AM_ZP_X, 0x01 },
	{ "???", AM_NON, 0x00 },
	{ "sed", AM_NON, 0x00 },
	{ "sbc", AM_ABS_Y, 0x02 },
	{ "plx", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "???", AM_NON, 0x00 },
	{ "sbc", AM_ABS_X, 0x02 },
	{ "inc", AM_ABS_X, 0x02 },
	{ "bbs7", AM_ZP_ABS, 0x02 },
};

struct dismnm a65816_ops[256] = {
	{ "brk", AM_NON, 0x00 },
	{ "ora", AM_ZP_REL_X, 0x01 },
	{ "cop", AM_NON, 0x00 },
	{ "ora", AM_STK, 0x01 },
	{ "tsb", AM_ZP, 0x01 },
	{ "ora", AM_ZP, 0x01 },
	{ "asl", AM_ZP, 0x01 },
	{ "ora", AM_ZP_REL_L, 0x01 },
	{ "php", AM_NON, 0x00 },
	{ "ora", AM_IMM_DBL_A, 0x01 },
	{ "asl", AM_NON, 0x00 },
	{ "phd", AM_NON, 0x00 },
	{ "tsb", AM_ABS, 0x02 },
	{ "ora", AM_ABS, 0x02 },
	{ "asl", AM_ABS, 0x02 },
	{ "ora", AM_ABS_L, 0x03 },
	{ "bpl", AM_BRANCH, 0x01 },
	{ "ora", AM_ZP_Y_REL, 0x01 },
	{ "ora", AM_ZP_REL, 0x01 },
	{ "ora", AM_STK_REL_Y, 0x01 },
	{ "trb", AM_ZP, 0x01 },
	{ "ora", AM_ZP_X, 0x01 },
	{ "asl", AM_ZP_X, 0x01 },
	{ "ora", AM_ZP_REL_Y_L, 0x01 },
	{ "clc", AM_NON, 0x00 },
	{ "ora", AM_ABS_Y, 0x02 },
	{ "inc", AM_NON, 0x00 },
	{ "tcs", AM_NON, 0x00 },
	{ "trb", AM_ABS, 0x02 },
	{ "ora", AM_ABS_X, 0x02 },
	{ "asl", AM_ABS_X, 0x02 },
	{ "ora", AM_ABS_L_X, 0x03 },
	{ "jsr", AM_ABS, 0x02 },
	{ "and", AM_ZP_REL_X, 0x01 },
	{ "jsr", AM_ABS_L, 0x03 },
	{ "and", AM_STK, 0x01 },
	{ "bit", AM_ZP, 0x01 },
	{ "and", AM_ZP, 0x01 },
	{ "rol", AM_ZP, 0x01 },
	{ "and", AM_ZP_REL_L, 0x01 },
	{ "plp", AM_NON, 0x00 },
	{ "and", AM_IMM_DBL_A, 0x01 },
	{ "rol", AM_NON, 0x00 },
	{ "pld", AM_NON, 0x00 },
	{ "bit", AM_ABS, 0x02 },
	{ "and", AM_ABS, 0x02 },
	{ "rol", AM_ABS, 0x02 },
	{ "and", AM_ABS_L, 0x03 },
	{ "bmi", AM_BRANCH, 0x01 },
	{ "and", AM_ZP_Y_REL, 0x01 },
	{ "and", AM_ZP_REL, 0x01 },
	{ "and", AM_STK_REL_Y, 0x01 },
	{ "bit", AM_ZP_X, 0x01 },
	{ "and", AM_ZP_X, 0x01 },
	{ "rol", AM_ZP_X, 0x01 },
	{ "and", AM_ZP_REL_Y_L, 0x01 },
	{ "sec", AM_NON, 0x00 },
	{ "and", AM_ABS_Y, 0x02 },
	{ "dec", AM_NON, 0x00 },
	{ "tsc", AM_NON, 0x00 },
	{ "bit", AM_ABS_X, 0x02 },
	{ "and", AM_ABS_X, 0x02 },
	{ "rol", AM_ABS_X, 0x02 },
	{ "and", AM_ABS_L_X, 0x03 },
	{ "rti", AM_NON, 0x00 },
	{ "eor", AM_ZP_REL_X, 0x01 },
	{ "wdm", AM_NON, 0x00 },
	{ "eor", AM_STK, 0x01 },
	{ "mvp", AM_BLK_MOV, 0x02 },
	{ "eor", AM_ZP, 0x01 },
	{ "lsr", AM_ZP, 0x01 },
	{ "eor", AM_ZP_REL_L, 0x01 },
	{ "pha", AM_NON, 0x00 },
	{ "eor", AM_IMM_DBL_A, 0x01 },
	{ "lsr", AM_NON, 0x00 },
	{ "phk", AM_NON, 0x00 },
	{ "jmp", AM_ABS, 0x02 },
	{ "eor", AM_ABS, 0x02 },
	{ "lsr", AM_ABS, 0x02 },
	{ "eor", AM_ABS_L, 0x03 },
	{ "bvc", AM_BRANCH, 0x01 },
	{ "eor", AM_ZP_Y_REL, 0x01 },
	{ "eor", AM_ZP_REL, 0x01 },
	{ "eor", AM_STK_REL_Y, 0x01 },
	{ "mvn", AM_BLK_MOV, 0x02 },
	{ "eor", AM_ZP_X, 0x01 },
	{ "lsr", AM_ZP_X, 0x01 },
	{ "eor", AM_ZP_REL_Y_L, 0x01 },
	{ "cli", AM_NON, 0x00 },
	{ "eor", AM_ABS_Y, 0x02 },
	{ "phy", AM_NON, 0x00 },
	{ "tcd", AM_NON, 0x00 },
	{ "jmp", AM_ABS_L, 0x03 },
	{ "eor", AM_ABS_X, 0x02 },
	{ "lsr", AM_ABS_X, 0x02 },
	{ "eor", AM_ABS_L_X, 0x03 },
	{ "rts", AM_NON, 0x00 },
	{ "adc", AM_ZP_REL_X, 0x01 },
	{ "per", AM_BRANCH_L, 0x02 },
	{ "adc", AM_STK, 0x01 },
	{ "stz", AM_ZP, 0x01 },
	{ "adc", AM_ZP, 0x01 },
	{ "ror", AM_ZP, 0x01 },
	{ "adc", AM_ZP_REL_L, 0x01 },
	{ "rtl", AM_NON, 0x00 },
	{ "adc", AM_IMM_DBL_A, 0x01 },
	{ "ror", AM_NON, 0x00 },
	{ "rtl", AM_NON, 0x00 },
	{ "jmp", AM_REL, 0x02 },
	{ "adc", AM_ABS, 0x02 },
	{ "ror", AM_ABS, 0x02 },
	{ "adc", AM_ABS_L, 0x03 },
	{ "bvs", AM_BRANCH, 0x01 },
	{ "adc", AM_ZP_Y_REL, 0x01 },
	{ "adc", AM_ZP_REL, 0x01 },
	{ "adc", AM_STK_REL_Y, 0x01 },
	{ "stz", AM_ZP_X, 0x01 },
	{ "adc", AM_ZP_X, 0x01 },
	{ "ror", AM_ZP_X, 0x01 },
	{ "adc", AM_ZP_REL_Y_L, 0x01 },
	{ "sei", AM_NON, 0x00 },
	{ "adc", AM_ABS_Y, 0x02 },
	{ "ply", AM_NON, 0x00 },
	{ "tdc", AM_NON, 0x00 },
	{ "jmp", AM_REL_X, 0x02 },
	{ "adc", AM_ABS_X, 0x02 },
	{ "ror", AM_ABS_X, 0x02 },
	{ "adc", AM_ABS_L_X, 0x03 },
	{ "bra", AM_BRANCH, 0x01 },
	{ "sta", AM_ZP_REL_X, 0x01 },
	{ "brl", AM_BRANCH_L, 0x02 },
	{ "sta", AM_STK, 0x01 },
	{ "sty", AM_ZP, 0x01 },
	{ "sta", AM_ZP, 0x01 },
	{ "stx", AM_ZP, 0x01 },
	{ "sta", AM_ZP_REL_L, 0x01 },
	{ "dey", AM_NON, 0x00 },
	{ "bit", AM_IMM_DBL_A, 0x01 },
	{ "txa", AM_NON, 0x00 },
	{ "phb", AM_NON, 0x00 },
	{ "sty", AM_ABS, 0x02 },
	{ "sta", AM_ABS, 0x02 },
	{ "stx", AM_ABS, 0x02 },
	{ "sta", AM_ABS_L, 0x03 },
	{ "bcc", AM_BRANCH, 0x01 },
	{ "sta", AM_ZP_Y_REL, 0x01 },
	{ "sta", AM_ZP_REL, 0x01 },
	{ "sta", AM_STK_REL_Y, 0x01 },
	{ "sty", AM_ZP_X, 0x01 },
	{ "sta", AM_ZP_X, 0x01 },
	{ "stx", AM_ZP_Y, 0x01 },
	{ "sta", AM_ZP_REL_Y_L, 0x01 },
	{ "tya", AM_NON, 0x00 },
	{ "sta", AM_ABS_Y, 0x02 },
	{ "txs", AM_NON, 0x00 },
	{ "txy", AM_NON, 0x00 },
	{ "stz", AM_ABS, 0x02 },
	{ "sta", AM_ABS_X, 0x02 },
	{ "stz", AM_ABS_X, 0x02 },
	{ "sta", AM_ABS_L_X, 0x03 },
	{ "ldy", AM_IMM_DBL_I, 0x01 },
	{ "lda", AM_ZP_REL_X, 0x01 },
	{ "ldx", AM_IMM_DBL_I, 0x01 },
	{ "lda", AM_STK, 0x01 },
	{ "ldy", AM_ZP, 0x01 },
	{ "lda", AM_ZP, 0x01 },
	{ "ldx", AM_ZP, 0x01 },
	{ "lda", AM_ZP_REL_L, 0x01 },
	{ "tay", AM_NON, 0x00 },
	{ "lda", AM_IMM_DBL_A, 0x01 },
	{ "tax", AM_NON, 0x00 },
	{ "plb", AM_NON, 0x00 },
	{ "ldy", AM_ABS, 0x02 },
	{ "lda", AM_ABS, 0x02 },
	{ "ldx", AM_ABS, 0x02 },
	{ "lda", AM_ABS_L, 0x03 },
	{ "bcs", AM_BRANCH, 0x01 },
	{ "lda", AM_ZP_Y_REL, 0x01 },
	{ "lda", AM_ZP_REL, 0x01 },
	{ "lda", AM_STK_REL_Y, 0x01 },
	{ "ldy", AM_ZP_X, 0x01 },
	{ "lda", AM_ZP_X, 0x01 },
	{ "ldx", AM_ZP_Y, 0x01 },
	{ "lda", AM_ZP_REL_Y_L, 0x01 },
	{ "clv", AM_NON, 0x00 },
	{ "lda", AM_ABS_Y, 0x02 },
	{ "tsx", AM_NON, 0x00 },
	{ "tyx", AM_NON, 0x00 },
	{ "ldy", AM_ABS_X, 0x02 },
	{ "lda", AM_ABS_X, 0x02 },
	{ "ldx", AM_ABS_Y, 0x02 },
	{ "lda", AM_ABS_L_X, 0x03 },
	{ "cpy", AM_IMM_DBL_I, 0x01 },
	{ "cmp", AM_ZP_REL_X, 0x01 },
	{ "rep", AM_IMM, 0x01 },
	{ "cmp", AM_STK, 0x01 },
	{ "cpy", AM_ZP, 0x01 },
	{ "cmp", AM_ZP, 0x01 },
	{ "dec", AM_ZP, 0x01 },
	{ "cmp", AM_ZP_REL_L, 0x01 },
	{ "iny", AM_NON, 0x00 },
	{ "cmp", AM_IMM_DBL_A, 0x01 },
	{ "dex", AM_NON, 0x00 },
	{ "wai", AM_NON, 0x00 },
	{ "cpy", AM_ABS, 0x02 },
	{ "cmp", AM_ABS, 0x02 },
	{ "dec", AM_ABS, 0x02 },
	{ "cmp", AM_ABS_L, 0x03 },
	{ "bne", AM_BRANCH, 0x01 },
	{ "cmp", AM_ZP_Y_REL, 0x01 },
	{ "cmp", AM_ZP_REL, 0x01 },
	{ "cmp", AM_STK_REL_Y, 0x01 },
	{ "pei", AM_ZP_REL, 0x01 },
	{ "cmp", AM_ZP_X, 0x01 },
	{ "dec", AM_ZP_X, 0x01 },
	{ "cmp", AM_ZP_REL_Y_L, 0x01 },
	{ "cld", AM_NON, 0x00 },
	{ "cmp", AM_ABS_Y, 0x02 },
	{ "phx", AM_NON, 0x00 },
	{ "stp", AM_NON, 0x00 },
	{ "jmp", AM_REL_L, 0x02 },
	{ "cmp", AM_ABS_X, 0x02 },
	{ "dec", AM_ABS_X, 0x02 },
	{ "cmp", AM_ABS_L_X, 0x03 },
	{ "cpx", AM_IMM_DBL_I, 0x01 },
	{ "sbc", AM_ZP_REL_X, 0x01 },
	{ "sep", AM_IMM, 0x01 },
	{ "sbc", AM_STK, 0x01 },
	{ "cpx", AM_ZP, 0x01 },
	{ "sbc", AM_ZP, 0x01 },
	{ "inc", AM_ZP, 0x01 },
	{ "sbc", AM_ZP_REL_L, 0x01 },
	{ "inx", AM_NON, 0x00 },
	{ "sbc", AM_IMM_DBL_A, 0x01 },
	{ "nop", AM_NON, 0x00 },
	{ "xba", AM_NON, 0x00 },
	{ "cpx", AM_ABS, 0x02 },
	{ "sbc", AM_ABS, 0x02 },
	{ "inc", AM_ABS, 0x02 },
	{ "sbc", AM_ABS_L, 0x03 },
	{ "beq", AM_BRANCH, 0x01 },
	{ "sbc", AM_ZP_Y_REL, 0x01 },
	{ "sbc", AM_ZP_REL, 0x01 },
	{ "sbc", AM_STK_REL_Y, 0x01 },
	{ "pea", AM_ABS, 0x02 },
	{ "sbc", AM_ZP_X, 0x01 },
	{ "inc", AM_ZP_X, 0x01 },
	{ "sbc", AM_ZP_REL_Y_L, 0x01 },
	{ "sed", AM_NON, 0x00 },
	{ "sbc", AM_ABS_Y, 0x02 },
	{ "plx", AM_NON, 0x00 },
	{ "xce", AM_NON, 0x00 },
	{ "jsr", AM_REL_X, 0x02 },
	{ "sbc", AM_ABS_X, 0x02 },
	{ "inc", AM_ABS_X, 0x02 },
	{ "sbc", AM_ABS_L_X, 0x03 },
};

enum RefType {
	RT_NONE,
	RT_BRANCH,	// bne, etc.
	RT_BRA_L,	// brl
	RT_JUMP,	// jmp
	RT_JSR,		// jsr
	RT_DATA,	// lda $...
	RT_COUNT
};

const char *aRefNames[RT_COUNT] = { "???", "branch", "long branch", "jump", "subroutine", "data" };

struct RefLink {
	int instr_addr;
	RefType type;
};

struct RefAddr {
	int address:30;					// address
	int data:2;						// 1 if data, 0 if code
	std::vector<RefLink> *pRefs;	// what is referencing this address

	RefAddr() : address(-1), data(0), pRefs(nullptr) {}
	RefAddr(int addr) : address(addr), data(0), pRefs(nullptr) {}
};

std::vector<RefAddr> refs;

static int _sortRefs(const void *A, const void *B)
{
	return ((const RefAddr*)A)->address - ((const RefAddr*)B)->address;
}

void GetReferences(unsigned char *mem, size_t bytes, bool acc_16, bool ind_16, int addr, const dismnm *opcodes, int init_data)
{
	int start_addr = addr;
	int end_addr = addr + (int)bytes;
	refs.push_back(RefAddr(start_addr));
	refs[0].pRefs = new std::vector<RefLink>();
	if (init_data) {
		refs[0].data = 1;
		refs.push_back(RefAddr(start_addr+init_data));
		refs[1].pRefs = new std::vector<RefLink>();
	}

	unsigned char *mem_orig = mem;
	size_t bytes_orig = bytes;
	int addr_orig = addr;

	if (size_t(init_data)>bytes)
		return;

	addr += init_data;
	bytes -= init_data;

	while (bytes) {
		unsigned char op = *mem++;
		int curr = addr;
		bytes--;
		addr++;
		if (opcodes == a65816_ops) {
			if (op == 0xe2) {	// sep
				if ((*mem)&0x20) acc_16 = false;
				if ((*mem)&0x10) ind_16 = false;
			} else if (op == 0xc2) { // rep
				if ((*mem)&0x20) acc_16 = true;
				if ((*mem)&0x10) ind_16 = true;
			}
		}

		int arg_size = opcodes[op].arg_size;;
		int mode = opcodes[op].addrMode;
		switch (mode) {
			case AM_IMM_DBL_A:
				arg_size = acc_16 ? 2 : 1;
				break;
			case AM_IMM_DBL_I:
				arg_size = ind_16 ? 2 : 1;
				break;
		}

		int reference = -1;
		RefType type = RT_NONE;

		if (mode == AM_BRANCH) {
			reference = curr + 2 + (char)*mem;
			type = RT_BRANCH;
		} else if (mode == AM_BRANCH_L) {
			reference = curr + 2 + (short)(unsigned short)mem[0] + ((unsigned short)mem[1]<<8);
			type = RT_BRA_L;
		} else if (mode == AM_ABS || mode == AM_ABS_Y || mode == AM_ABS_X || mode == AM_REL || mode == AM_REL_X || mode == AM_REL_L) {
			reference = (unsigned short)mem[0] + ((unsigned short)mem[1]<<8);
			if (op == 0x20 || op == 0xfc || op == 0x22)	// jsr opcodes
				type = RT_JSR;
			else if (op == 0x4c || op == 0x6c || op == 0x7c || op == 0x5c || op == 0xdc)	// jmp opcodes
				type = RT_JUMP;
			else
				type = RT_DATA;
		}

		if (reference >= start_addr && reference <= end_addr && type != RT_NONE) {
			bool found = false;
			for (std::vector<RefAddr>::iterator i = refs.begin(); i != refs.end(); ++i) {
				if (i->address == reference) {
					struct RefLink ref = { curr, type };
					i->pRefs->push_back(ref);
					found = true;
					break;
				}
			}
			if (!found) {
				refs.push_back(RefAddr(reference));
				struct RefAddr &last = refs[refs.size()-1];
				struct RefLink ref = { curr, type };
				last.pRefs = new std::vector<RefLink>();
				last.pRefs->push_back(ref);
			}
		}

		addr += arg_size;
		mem += arg_size;
		if (arg_size > (int)bytes)
			break;
		bytes -= arg_size;
	}

	// sort the order of the labels by address
	if (refs.size())
		qsort(&refs[0], refs.size(), sizeof(RefAddr), _sortRefs);

	// validate the label addresses
	mem = mem_orig;
	bytes = bytes_orig;
	addr = addr_orig;
	int curr_label = init_data ? 1 : 0;
	int prev_addr = -1;
	bool was_data = init_data>0;
	refs[curr_label].data = 0;
	bool separator = false;
	int prev_op = 0xff;
	addr += init_data;
	bytes -= init_data;
	while (bytes && curr_label<(int)refs.size()) {
		unsigned char op = *mem++;
		int curr = addr;
		bytes--;
		addr++;
		if (opcodes == a65816_ops) {
			if (op == 0xe2) {	// sep
				if ((*mem)&0x20) acc_16 = false;
				if ((*mem)&0x10) ind_16 = false;
			} else if (op == 0xc2) { // rep
				if ((*mem)&0x20) acc_16 = true;
				if ((*mem)&0x10) ind_16 = true;
			}
		}

		int arg_size = opcodes[op].arg_size;;
		int mode = opcodes[op].addrMode;
		switch (mode) {
			case AM_IMM_DBL_A:
				arg_size = acc_16 ? 2 : 1;
				break;
			case AM_IMM_DBL_I:
				arg_size = ind_16 ? 2 : 1;
				break;
		}
		if (arg_size > (int)bytes)
			break;	// ended on partial instruction
		addr += arg_size;
		bytes -= arg_size;
		mem += arg_size;

		if (curr == refs[curr_label].address) {
			std::vector<RefLink> &pRefs = *refs[curr_label].pRefs;
			if (curr_label < (int)refs.size()) {
				bool prev_data = was_data;
				was_data = separator && !(!was_data && op==0x4c && prev_op==0x4c);
				for (size_t j = 0; j<pRefs.size(); ++j) {
					RefType type = pRefs[j].type;
					if (!(pRefs[j].instr_addr>curr && type == RT_BRANCH) && type != RT_DATA) {
						separator = false;
						was_data = false;
						prev_data = false;
						break;
					}
				}

				if (!was_data && prev_data) {
					bool only_data_ref = pRefs.size() ? true : false;
					for (size_t j = 0; j<pRefs.size(); ++j) {
						RefType type = pRefs[j].type;
						int ref_addr = pRefs[j].instr_addr;
						if (type != RT_DATA && (type != RT_BRANCH || ref_addr<addr)) {
							only_data_ref = false;
							if (type != RT_BRANCH)
								separator = false;
						}
					}
					was_data = only_data_ref;
				}
				refs[curr_label].data = was_data;
				if (was_data) {
					int start = curr;
					int end = (curr_label+1)<(int)refs.size() ? refs[curr_label+1].address : (int)(addr + bytes);
					for (int k = 0; k<(int)refs.size(); ++k) {
						std::vector<RefLink> &pRefs = *refs[k].pRefs;
						std::vector<RefLink>::iterator r = pRefs.begin();
						while (r != pRefs.end()) {
							if (r->instr_addr>=start && r->instr_addr<end)
								r = pRefs.erase(r);
							else
								++r;
						}
					}
				}
			}
			curr_label++;
		}
		else if (curr > refs[curr_label].address && prev_addr>=0) {
			refs[curr_label].address = prev_addr;
			refs[curr_label].data = was_data;
			curr_label++;
		}

		// after a separator if there is no jmp, jsr, brl begin data block
		if (!was_data) {
			if (op == 0x60 || op == 0x40 || op == 0x6b || op == 0x4c || op == 0x6c ||
				op == 0x7c || op == 0x5c || op == 0xdc) {	// rts, rti, rtl or jmp
				separator = true;
				for (size_t i = curr_label+1; i<refs.size() && (refs[i].address-curr)<0x7e; i++) {	// check no branch crossing
					if (refs[i].address<=curr) {
						std::vector<RefLink> &pRefs = *refs[i].pRefs;
						for (size_t j = 0; j<pRefs.size() && separator; ++j) {
							if (pRefs[j].type == RT_BRANCH && pRefs[j].instr_addr>curr) {
								separator = false;
								break;
							}
						}
					}
				}
			}
		}

		prev_op = op;
		prev_addr = curr;
	}


	int last = (int)refs.size()-1;
	while (last>1 && refs[last-1].data) {
		int start_addr = refs[last].address;
		for (int k = 0; k<(int)refs.size(); ++k) {
			std::vector<RefLink> &pRefs = *refs[k].pRefs;
			std::vector<RefLink>::iterator r = pRefs.begin();
			while (r != pRefs.end()) {
				if (r->instr_addr>=start_addr && r->instr_addr<end_addr)
					r = pRefs.erase(r);
				else
					++r;
			}
		}
		last--;
		if (last<=1 || !refs[last].data)
			break;
	}

	std::vector<RefAddr>::iterator k = refs.begin();
	if (k != refs.end()) {
		++k;	// don't delete the initial label
		if (init_data && k != refs.end())
			++k;	// don't delete the initial label
	}
	while (k!=refs.end()) {
		if (k->pRefs && k->pRefs->size()==0) {
			delete k->pRefs;
			k = refs.erase(k);
		} else
			++k;
	}
}

static const char spacing[] = "                ";
void Disassemble(strref filename, unsigned char *mem, size_t bytes, bool acc_16, bool ind_16, int addr, const dismnm *opcodes, bool src, int init_data)
{
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f)
			return;
		opened = true;
	}

	const char *spc = src ? "" : spacing;

	strref prev_src;
	int prev_offs = 0;
	int start_addr = addr;
	int end_addr = addr + (int)bytes;

	refs.clear();
	GetReferences(mem, bytes, acc_16, ind_16, addr, opcodes, init_data);

	int curr_label_index = 0;
	bool separator = false;
	bool is_data = refs.size() ? !!refs[0].data : false;
	strown<256> out;
	int prev_op = 255;
	while (bytes) {
		bool data_to_code = false;
		// Determine if current address is referenced from somewhere
		while (curr_label_index<(int)refs.size() && addr >= refs[curr_label_index].address) {
			struct RefAddr &ref = refs[curr_label_index];
			if (ref.pRefs) {
				for (size_t j = 0; j<ref.pRefs->size(); ++j) {
					if (src) {
						struct RefLink &lnk = (*ref.pRefs)[j];
						int lbl = -1;
						int prv_addr = 0;
						int ref_addr = lnk.instr_addr;
						for (size_t k = 0; k<refs.size(); k++) {
							if (refs[k].address>ref_addr)
								break;
							lbl = (int)k;
							prv_addr = refs[k].address;
						}
						out.sprintf("%s; Referenced from Label_%d + $%x (%s)\n", spc, lbl, ref_addr - prv_addr, aRefNames[(*ref.pRefs)[j].type]);
					} else
						out.sprintf("%s; Referenced from $%04x (%s)\n", spc, (*ref.pRefs)[j].instr_addr, aRefNames[(*ref.pRefs)[j].type]);
					fputs(out.c_str(), f);
				}
			}
			out.sprintf("%sLabel_%d: ; $%04x\n", spc, curr_label_index, addr);
			fputs(out.c_str(), f);
			is_data = !!refs[curr_label_index].data;
			data_to_code = !is_data;
			separator = false;
			curr_label_index++;
			if (curr_label_index < (int)refs.size() && refs[curr_label_index].data)
				data_to_code = false;
		}
		if (src && (is_data || separator)) {
			out.clear();
			int left = end_addr - addr;
			if (curr_label_index<(int)refs.size())
				left = refs[curr_label_index].address - addr;
			is_data = true;
			for (int i = 0; i<left; i++) {
				if (!(i&0xf)) {
					if (i) {
						out.append("\n");
						fputs(out.c_str(), f);
					}
					out.copy("  dc.b ");
				}
				out.sprintf_append("$%02x", *mem++);
				if ((i&0xf)!=0xf && i<(left-1))
					out.append(", ");
				addr++;
				bytes--;
			}
			if (left&0xf) {
				out.append("\n");
				fputs(out.c_str(), f);
			}
			separator = false;
			prev_op = 255;
		} else {
			int curr_addr = addr;
			unsigned char op = *mem++;
			bytes--;
			out.clear();
			if (!src)
				out.sprintf("$%04x ", addr);
			addr++;

			if (opcodes == a65816_ops) {
				if (op == 0xe2) {	// sep
					if ((*mem)&0x20) acc_16 = false;
					if ((*mem)&0x10) ind_16 = false;
				} else if (op == 0xc2) { // rep
					if ((*mem)&0x20) acc_16 = true;
					if ((*mem)&0x10) ind_16 = true;
				}
			}

			int arg_size = opcodes[op].arg_size;;
			int mode = opcodes[op].addrMode;
			switch (mode) {
				case AM_IMM_DBL_A:
					arg_size = acc_16 ? 2 : 1;
					break;
				case AM_IMM_DBL_I:
					arg_size = ind_16 ? 2 : 1;
					break;
			}
			addr += arg_size;

			if (arg_size > (int)bytes)
				return;
			bytes -= arg_size;

			if (!src) {
				out.sprintf_append("%02x ", op);
				for (int n = 0; n < arg_size; n++)
					out.sprintf_append("%02x ", mem[n]);
			}

			out.append_to(' ', src ? 2 : 18);

			int reference = -1;
			separator = false;
			if (op == 0x60 || op == 0x40 || op == 0x6b || (op == 0x4c && mem[arg_size] != 0x4c) ||
				op == 0x6c || op == 0x7c || op == 0x5c || op == 0xdc) {	// rts, rti, rtl or jmp
				separator = true;
				for (size_t i = 0; i<refs.size(); i++) {
					std::vector<RefLink> &pRefs = *refs[i].pRefs;
					if (refs[i].address<=curr_addr) {
						for (size_t j = 0; j<pRefs.size() && separator; ++j) {
							if (pRefs[j].type == RT_BRANCH && pRefs[j].instr_addr>curr_addr)
								separator = false;
						}
					} else {
						for (size_t j = 0; j<pRefs.size() && separator; ++j) {
							if (pRefs[j].type == RT_BRANCH && pRefs[j].instr_addr<curr_addr)
								separator = false;
						}
					}
				}
			}

			if (mode == AM_BRANCH)
				reference = addr + (char)*mem;
			else if (mode == AM_BRANCH_L)
				reference = addr + (short)(unsigned short)mem[0] + ((unsigned short)mem[1]<<8);
			else if (mode == AM_ABS || mode == AM_ABS_Y || mode == AM_ABS_X || mode == AM_REL || mode == AM_REL_X || mode == AM_REL_L)
				reference = (int)mem[0] | ((int)mem[1])<<8;

			strown<64> lblname;
			if (reference>=start_addr && reference<=end_addr) {
				for (size_t i = 0; i<refs.size(); ++i) {
					if (reference >= refs[i].address) {
						if (i==(refs.size()-1) || reference<refs[i+1].address) {
							lblname.sprintf("Label_%d", i);
							if (reference > refs[i].address)
								lblname.sprintf_append(" + $%x", reference - refs[i].address);
							break;
						}
					}
				}
			}

			const char *fmt = (src && lblname) ? aAddrModeFmtSrc[mode] : aAddrModeFmt[mode];
			switch (mode) {
				case AM_ABS:		// 3 $1234
				case AM_ABS_Y:		// 6 $1234,y
				case AM_ABS_X:		// 7 $1234,x
				case AM_REL:		// 8 ($1234)
				case AM_REL_X:		// c ($1234,x)
				case AM_REL_L:		// 14 [$1234]
					reference = (int)mem[0] | ((int)mem[1])<<8;
					if (src && lblname)
						out.sprintf_append(fmt, opcodes[op].name, lblname.c_str());
					else
						out.sprintf_append(fmt, opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8);
					break;

				case AM_ABS_L:		// 10 $bahilo
				case AM_ABS_L_X:	// 11 $123456,x
					reference = (int)mem[0] | ((int)mem[1])<<8 | ((int)mem[2])<<16;
					if (src && lblname)
						out.sprintf_append(fmt, opcodes[op].name, lblname);
					else
						out.sprintf_append(fmt, opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8 | ((int)mem[2])<<16);
					break;

				case AM_IMM_DBL_A:	// 18 #$12/#$1234
				case AM_IMM_DBL_I:	// 19 #$12/#$1234
					if (arg_size==2) {
						if (src && lblname)
							out.sprintf_append("%s #%s", opcodes[op].name, lblname.c_str());
						else
							out.sprintf_append("%s #$%04x", opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8);
					} else {
						if (src && lblname)
							out.sprintf_append(fmt, opcodes[op].name, lblname.c_str());
						else
							out.sprintf_append(fmt, opcodes[op].name, mem[0]);
					}
					break;

				case AM_BRANCH:		// beq $1234
					if (src && lblname)
						out.sprintf_append(fmt, opcodes[op].name, lblname.c_str());
					else
						out.sprintf_append(fmt, opcodes[op].name, addr + (char)mem[0]);
					break;

				case AM_BRANCH_L:	// brl $1234
					if (src && lblname)
						out.sprintf_append(fmt, opcodes[op].name, lblname.c_str());
					else
						out.sprintf_append(fmt, opcodes[op].name, addr + ((short)(char)mem[0] + (((short)(char)mem[1])<<8)));
					break;

				case AM_ZP_ABS:		// d $12, *+$12
					if (src && lblname)
						out.sprintf_append(fmt, opcodes[op].name, lblname.c_str());
					else
						out.sprintf_append(fmt, opcodes[op].name, mem[0], addr + (char)mem[1]);
					break;

				default:
					out.sprintf_append(fmt, opcodes[op].name, mem[0], mem[1]);
					break;
			}
			if (!src && lblname)
				out.sprintf_append(" ; %s", lblname.c_str());
			else if (src && lblname)
				out.sprintf_append(" ; $%04x", reference);

			mem += arg_size;
			out.append('\n');
			fputs(out.c_str(), f);
			prev_op = op;
		}
		if (separator || (curr_label_index<(int)refs.size() &&
				refs[curr_label_index].address==addr && is_data &&
				!refs[curr_label_index].data)) {
			fputs("\n", f);
			fputs(spc, f);
			fprintf(f, "; ------------- $%04x ------------- ;\n\n", addr);
		}
	}
	if (opened)
		fclose(f);

	for (int i = (int)refs.size()-1; i>=0; --i) {
		if (refs[i].pRefs)
			delete refs[i].pRefs;
		refs.erase(refs.begin() + i);
	}

}


int main(int argc, char **argv)
{
	const char *bin = nullptr;
	const char *out = nullptr;
	int skip = 0;
	int end = 0;
	int addr = 0x1000;
	int data = 0;
	bool acc_16 = true;
	bool ind_16 = true;
	bool src = false;
	bool prg = false;

	const dismnm *opcodes = a6502_ops;

	for (int i = 1; i < argc; i++) {
		strref arg(argv[i]);
		if (arg[0] == '$') {
			++arg;
			skip = arg.ahextoui_skip();
			if (arg.get_first() == '-') {
				++arg;
				if (arg.get_first() == '$') {
					++arg;
					end = arg.ahextoui();
				}
			}
		} else {
			strref var = arg.split_token('=');
			if (var.same_str("src"))
				src = true;
			else if (var.same_str("prg"))
				prg = true;
			else if (!arg) {
				if (!bin)
					bin = argv[i];
				else if (!out)
					out = argv[i];
			} else {
				if (var.same_str("mx")) {
					int mx = arg.atoi();
					ind_16 = !!(mx & 1);
					acc_16 = !!(mx & 2);
				} else if (var.same_str("addr")) {
					if (arg.get_first() == '$')
						++arg;
					addr = arg.ahextoui();
				} else if (var.same_str("data")) {
					if (arg.get_first() == '$') {
						++arg;
						data = arg.ahextoui();
					} else
						data = arg.atoi();
				} else if (var.same_str("cpu")) {
					if (arg.same_str("65816"))
						opcodes = a65816_ops;
					else if (arg.same_str("65C02"))
						opcodes = a65C02_ops;
					else if (arg.same_str("6502"))
						opcodes = a6502_ops;
				}
			}
		}
	}

	if (bin) {
		FILE *f = fopen(bin, "rb");
		if (!f)
			return -1;
		fseek(f, 0, SEEK_END);
		size_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		if (unsigned char *mem = (unsigned char*)malloc(size)) {
			fread(mem, size, 1, f);
			fclose(f);
			if (prg) {
				addr = mem[0] + ((int)mem[1]<<8);
				skip += 2;
				if (end)
					end += 2;
			}
			if (size > (size_t)skip && (end == 0 || end > skip)) {
				size_t bytes = size - skip;
				if (end && bytes > size_t(end - skip))
					bytes = size_t(end - skip);
				Disassemble(out, mem + skip, bytes, acc_16, ind_16, addr, opcodes, src, data);
			}
			free(mem);
		}
	} else {
		puts("Usage:\nx65dsasm binary disasm.txt [$skip[-$end]] [addr=$xxxx] [cpu=6502/65C02/65816] [mx=0-3] [src]\n"
			 " * binary: file which contains some 65xx series instructions\n"
			 " * disasm.txt: output file (default is stdout)\n"
			 " * $skip-$end: first byte offset to disassemble to last byte offset to disassemble\n"
			 " * addr: disassemble as if loaded at addr\n"
			 " * prg: file is a c64 program file starting with the load address\n"
			 " * cpu: set which cpu to disassemble for (default is 6502)\n"
			 " * src: export near assemblable source with guesstimated data blocks\n"
			 " * mx: set the mx flags which control accumulator and index register size\n");
	}
	return 0;
}