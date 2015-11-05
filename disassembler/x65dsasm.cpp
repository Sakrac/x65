
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
	{ "???", AM_NON, 0x00 },
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


void Disassemble(strref filename, unsigned char *mem, size_t bytes, bool acc_16, bool ind_16, int addr, const dismnm *opcodes)
{
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f)
			return;
		opened = true;
	}

	strref prev_src;
	int prev_offs = 0;

	strown<256> out;
	while (bytes) {
		unsigned char op = *mem++;
		bytes--;
		out.sprintf("$%04x ", addr);
		addr++;

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

		if (arg_size > bytes)
			return;
		bytes -= arg_size;

		out.sprintf_append("%02x ", op);
		for (int n = 0; n < arg_size; n++)
			out.sprintf_append("%02x ", mem[n]);

		out.append_to(' ', 18);

		const char *fmt = aAddrModeFmt[mode];
		switch (mode) {
			case AM_ABS:		// 3 $1234
			case AM_ABS_Y:		// 6 $1234,y
			case AM_ABS_X:		// 7 $1234,x
			case AM_REL:		// 8 ($1234)
			case AM_REL_X:		// c ($1234,x)
			case AM_REL_L:		// 14 [$1234]
				out.sprintf_append(fmt, opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8);
				break;

			case AM_ABS_L:		// 10 $bahilo
			case AM_ABS_L_X:	// 11 $123456,x
				out.sprintf_append(fmt, opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8 | ((int)mem[2])<<16);
				break;

			case AM_IMM_DBL_A:	// 18 #$12/#$1234
			case AM_IMM_DBL_I:	// 19 #$12/#$1234
				if (arg_size==2)
					out.sprintf_append("%s #$%04x", opcodes[op].name, (int)mem[0] | ((int)mem[1])<<8);
				else
					out.sprintf_append(fmt, opcodes[op].name, mem[0]);
				break;

			case AM_BRANCH:		// beq $1234
				out.sprintf_append(fmt, opcodes[op].name, addr + (char)mem[0]);
				break;

			case AM_BRANCH_L:	// brl $1234
				out.sprintf_append(fmt, opcodes[op].name, addr + ((short)(char)mem[0] + (((short)(char)mem[1])<<8)));
				break;

			case AM_ZP_ABS:		// d $12, *+$12
				out.sprintf_append(fmt, opcodes[op].name, mem[0], addr + (char)mem[1]);
				break;

			default:
				out.sprintf_append(fmt, opcodes[op].name, mem[0], mem[1]);
				break;
		}

		mem += arg_size;
		out.append('\n');
		fputs(out.c_str(), f);
	}
	if (opened)
		fclose(f);
}


int main(int argc, char **argv)
{
	const char *bin = nullptr;
	const char *out = nullptr;
	int skip = 0;
	int end = 0;
	int addr = 0x1000;
	bool acc_16 = false;
	bool ind_16 = false;

	const dismnm *opcodes = a65816_ops;

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
			if (!arg) {
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
			if (size > (size_t)skip && (end == 0 || end > skip)) {
				size_t bytes = size - skip;
				if (end && bytes > size_t(end - skip))
					bytes = size_t(end - skip);
				Disassemble(out, mem + skip, bytes, acc_16, ind_16, addr, opcodes);
			}
			free(mem);
		}
	}
	return 0;
}