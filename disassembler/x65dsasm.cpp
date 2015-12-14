//
//  x65dsasm.cpp
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

const char *AddressModeNames[] {
	// address mode bit index

	// 6502

	"AM_ZP_REL_X",	// 0 ($12",x)
	"AM_ZP",			// 1 $12
	"AM_IMM",			// 2 #$12
	"AM_ABS",			// 3 $1234
	"AM_ZP_Y_REL",	// 4 ($12)",y
	"AM_ZP_X",		// 5 $12",x
	"AM_ABS_Y",		// 6 $1234",y
	"AM_ABS_X",		// 7 $1234",x
	"AM_REL",			// 8 ($1234)
	"AM_ACC",			// 9 A
	"AM_NON",			// a

	// 65C02

	"AM_ZP_REL",		// b ($12)
	"AM_REL_X",		// c ($1234",x)
	"AM_ZP_ABS",		// d $12", *+$12

	// 65816

	"AM_ZP_REL_L",	// e [$02]
	"AM_ZP_REL_Y_L",	// f [$00]",y
	"AM_ABS_L",		// 10 $bahilo
	"AM_ABS_L_X",		// 11 $123456",x
	"AM_STK",			// 12 $12",s
	"AM_STK_REL_Y",	// 13 ($12",s)",y
	"AM_REL_L",		// 14 [$1234]
	"AM_BLK_MOV",		// 15 $12",$34

	"AM_ZP_Y",		// 16 stx/ldx
	"AM_ZP_REL_Y",	// 17 sax/lax/ahx

	"AM_IMM_DBL_A",	// 18 #$12/#$1234
	"AM_IMM_DBL_I",	// 19 #$12/#$1234

	"AM_BRANCH",		// 1a beq $1234
	"AM_BRANCH_L",	// 1b brl $1234
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

	AM_BRANCH,		// 1a beq $1234
	AM_BRANCH_L,	// 1b brl $1234

	AM_COUNT,
};

enum MNM_Base {
	mnm_brk,
	mnm_ora,
	mnm_cop,
	mnm_tsb,
	mnm_asl,
	mnm_php,
	mnm_phd,
	mnm_bpl,
	mnm_trb,
	mnm_clc,
	mnm_inc,
	mnm_tcs,
	mnm_jsr,
	mnm_and,
	mnm_bit,
	mnm_rol,
	mnm_plp,
	mnm_pld,
	mnm_bmi,
	mnm_sec,
	mnm_dec,
	mnm_tsc,
	mnm_rti,
	mnm_eor,
	mnm_wdm,
	mnm_mvp,
	mnm_lsr,
	mnm_pha,
	mnm_phk,
	mnm_jmp,
	mnm_bvc,
	mnm_mvn,
	mnm_cli,
	mnm_phy,
	mnm_tcd,
	mnm_rts,
	mnm_adc,
	mnm_per,
	mnm_stz,
	mnm_ror,
	mnm_rtl,
	mnm_bvs,
	mnm_sei,
	mnm_ply,
	mnm_tdc,
	mnm_bra,
	mnm_sta,
	mnm_brl,
	mnm_sty,
	mnm_stx,
	mnm_dey,
	mnm_txa,
	mnm_phb,
	mnm_bcc,
	mnm_tya,
	mnm_txs,
	mnm_txy,
	mnm_ldy,
	mnm_lda,
	mnm_ldx,
	mnm_tay,
	mnm_tax,
	mnm_plb,
	mnm_bcs,
	mnm_clv,
	mnm_tsx,
	mnm_tyx,
	mnm_cpy,
	mnm_cmp,
	mnm_rep,
	mnm_iny,
	mnm_dex,
	mnm_wai,
	mnm_bne,
	mnm_pei,
	mnm_cld,
	mnm_phx,
	mnm_stp,
	mnm_cpx,
	mnm_sbc,
	mnm_sep,
	mnm_inx,
	mnm_nop,
	mnm_xba,
	mnm_beq,
	mnm_pea,
	mnm_sed,
	mnm_plx,
	mnm_xce,
	mnm_inv,
	mnm_pla,

	mnm_wdc_and_illegal_instructions,

	mnm_bbs0 = mnm_wdc_and_illegal_instructions,
	mnm_bbs1,
	mnm_bbs2,
	mnm_bbs3,
	mnm_bbs4,
	mnm_bbs5,
	mnm_bbs6,
	mnm_bbs7,
	mnm_bbr0,
	mnm_bbr1,
	mnm_bbr2,
	mnm_bbr3,
	mnm_bbr4,
	mnm_bbr5,
	mnm_bbr6,
	mnm_bbr7,

	mnm_ahx,
	mnm_anc,
	mnm_aac,
	mnm_alr,
	mnm_axs,
	mnm_dcp,
	mnm_isc,
	mnm_lax,
	mnm_lax2,
	mnm_rla,
	mnm_rra,
	mnm_sre,
	mnm_sax,
	mnm_slo,
	mnm_xaa,
	mnm_arr,
	mnm_tas,
	mnm_shy,
	mnm_shx,
	mnm_las,
	mnm_sbi,

	mnm_count
};

const char *zsMNM[mnm_count] {
	"brk",
	"ora",
	"cop",
	"tsb",
	"asl",
	"php",
	"phd",
	"bpl",
	"trb",
	"clc",
	"inc",
	"tcs",
	"jsr",
	"and",
	"bit",
	"rol",
	"plp",
	"pld",
	"bmi",
	"sec",
	"dec",
	"tsc",
	"rti",
	"eor",
	"wdm",
	"mvp",
	"lsr",
	"pha",
	"phk",
	"jmp",
	"bvc",
	"mvn",
	"cli",
	"phy",
	"tcd",
	"rts",
	"adc",
	"per",
	"stz",
	"ror",
	"rtl",
	"bvs",
	"sei",
	"ply",
	"tdc",
	"bra",
	"sta",
	"brl",
	"sty",
	"stx",
	"dey",
	"txa",
	"phb",
	"bcc",
	"tya",
	"txs",
	"txy",
	"ldy",
	"lda",
	"ldx",
	"tay",
	"tax",
	"plb",
	"bcs",
	"clv",
	"tsx",
	"tyx",
	"cpy",
	"cmp",
	"rep",
	"iny",
	"dex",
	"wai",
	"bne",
	"pei",
	"cld",
	"phx",
	"stp",
	"cpx",
	"sbc",
	"sep",
	"inx",
	"nop",
	"xba",
	"beq",
	"pea",
	"sed",
	"plx",
	"xce",
	"???",
	"pla",
	"bbs0",
	"bbs1",
	"bbs2",
	"bbs3",
	"bbs4",
	"bbs5",
	"bbs6",
	"bbs7",
	"bbr0",
	"bbr1",
	"bbr2",
	"bbr3",
	"bbr4",
	"bbr5",
	"bbr6",
	"bbr7",
	"ahx",
	"anc",
	"aac",
	"alr",
	"axs",
	"dcp",
	"isc",
	"lax",
	"lax2",
	"rla",
	"rra",
	"sre",
	"sax",
	"slo",
	"xaa",
	"arr",
	"tas",
	"shy",
	"shx",
	"las",
	"sbi",
};

struct dismnm {
	MNM_Base mnemonic;
	unsigned char addrMode;
	unsigned char arg_size;
};

struct dismnm a6502_ops[256] = {
	{ mnm_brk, AM_NON, 0},
	{ mnm_ora, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ZP, 1 },
	{ mnm_asl, AM_ZP, 1 },
	{ mnm_slo, AM_ZP, 1 },
	{ mnm_php, AM_NON, 0 },
	{ mnm_ora, AM_IMM, 1 },
	{ mnm_asl, AM_NON, 0 },
	{ mnm_anc, AM_IMM, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ABS, 2 },
	{ mnm_asl, AM_ABS, 2 },
	{ mnm_slo, AM_ABS, 2 },
	{ mnm_bpl, AM_BRANCH, 1 },
	{ mnm_ora, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ZP_X, 1 },
	{ mnm_asl, AM_ZP_X, 1 },
	{ mnm_slo, AM_ZP_X, 1 },
	{ mnm_clc, AM_NON, 0 },
	{ mnm_ora, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_slo, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ora, AM_ABS_X, 2 },
	{ mnm_asl, AM_ABS_X, 2 },
	{ mnm_slo, AM_ABS_X, 2 },
	{ mnm_jsr, AM_ABS, 2 },
	{ mnm_and, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ZP_REL_X, 1 },
	{ mnm_bit, AM_ZP, 1 },
	{ mnm_and, AM_ZP, 1 },
	{ mnm_rol, AM_ZP, 1 },
	{ mnm_rla, AM_ZP, 1 },
	{ mnm_plp, AM_NON, 0 },
	{ mnm_and, AM_IMM, 1 },
	{ mnm_rol, AM_NON, 0 },
	{ mnm_aac, AM_IMM, 1 },
	{ mnm_bit, AM_ABS, 2 },
	{ mnm_and, AM_ABS, 2 },
	{ mnm_rol, AM_ABS, 2 },
	{ mnm_rla, AM_ABS, 2 },
	{ mnm_bmi, AM_BRANCH, 1 },
	{ mnm_and, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_and, AM_ZP_X, 1 },
	{ mnm_rol, AM_ZP_X, 1 },
	{ mnm_rla, AM_ZP_X, 1 },
	{ mnm_sec, AM_NON, 0 },
	{ mnm_and, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rla, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_and, AM_ABS_X, 2 },
	{ mnm_rol, AM_ABS_X, 2 },
	{ mnm_rla, AM_ABS_X, 2 },
	{ mnm_rti, AM_NON, 0 },
	{ mnm_eor, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP, 1 },
	{ mnm_lsr, AM_ZP, 1 },
	{ mnm_sre, AM_ZP, 1 },
	{ mnm_pha, AM_NON, 0 },
	{ mnm_eor, AM_IMM, 1 },
	{ mnm_lsr, AM_NON, 0 },
	{ mnm_alr, AM_IMM, 1 },
	{ mnm_jmp, AM_ABS, 2 },
	{ mnm_eor, AM_ABS, 2 },
	{ mnm_lsr, AM_ABS, 2 },
	{ mnm_sre, AM_ABS, 2 },
	{ mnm_bvc, AM_BRANCH, 1 },
	{ mnm_eor, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP_X, 1 },
	{ mnm_lsr, AM_ZP_X, 1 },
	{ mnm_sre, AM_ZP_X, 1 },
	{ mnm_cli, AM_NON, 0 },
	{ mnm_eor, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sre, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ABS_X, 2 },
	{ mnm_lsr, AM_ABS_X, 2 },
	{ mnm_sre, AM_ABS_X, 2 },
	{ mnm_rts, AM_NON, 0 },
	{ mnm_adc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ZP, 1 },
	{ mnm_ror, AM_ZP, 1 },
	{ mnm_rra, AM_ZP, 1 },
	{ mnm_pla, AM_NON, 0 },
	{ mnm_adc, AM_IMM, 1 },
	{ mnm_ror, AM_NON, 0 },
	{ mnm_arr, AM_IMM, 1 },
	{ mnm_jmp, AM_REL, 2 },
	{ mnm_adc, AM_ABS, 2 },
	{ mnm_ror, AM_ABS, 2 },
	{ mnm_rra, AM_ABS, 2 },
	{ mnm_bvs, AM_BRANCH, 1 },
	{ mnm_adc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ZP_X, 1 },
	{ mnm_ror, AM_ZP_X, 1 },
	{ mnm_rra, AM_ZP_X, 1 },
	{ mnm_sei, AM_NON, 0 },
	{ mnm_adc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_rra, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_adc, AM_ABS_X, 2 },
	{ mnm_ror, AM_ABS_X, 2 },
	{ mnm_rra, AM_ABS_X, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sta, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sax, AM_ZP_REL_Y, 1 },
	{ mnm_sty, AM_ZP, 1 },
	{ mnm_sta, AM_ZP, 1 },
	{ mnm_stx, AM_ZP, 1 },
	{ mnm_sax, AM_ZP, 1 },
	{ mnm_dey, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_txa, AM_NON, 0 },
	{ mnm_xaa, AM_IMM, 1 },
	{ mnm_sty, AM_ABS, 2 },
	{ mnm_sta, AM_ABS, 2 },
	{ mnm_stx, AM_ABS, 2 },
	{ mnm_sax, AM_ABS, 2 },
	{ mnm_bcc, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ahx, AM_ZP_REL_Y, 1 },
	{ mnm_sty, AM_ZP_X, 1 },
	{ mnm_sta, AM_ZP_X, 1 },
	{ mnm_stx, AM_ZP_Y, 1 },
	{ mnm_sax, AM_ZP_Y, 1 },
	{ mnm_tya, AM_NON, 0 },
	{ mnm_sta, AM_ABS_Y, 2 },
	{ mnm_txs, AM_NON, 0 },
	{ mnm_tas, AM_ABS_Y, 2 },
	{ mnm_shy, AM_ABS_X, 2 },
	{ mnm_sta, AM_ABS_X, 2 },
	{ mnm_shx, AM_ABS_Y, 2 },
	{ mnm_ahx, AM_ABS_Y, 2 },
	{ mnm_ldy, AM_IMM, 1 },
	{ mnm_lda, AM_ZP_REL_X, 1 },
	{ mnm_ldx, AM_IMM, 1 },
	{ mnm_lax, AM_ZP_REL_Y, 1 },
	{ mnm_ldy, AM_ZP, 1 },
	{ mnm_lda, AM_ZP, 1 },
	{ mnm_ldx, AM_ZP, 1 },
	{ mnm_lax, AM_ZP, 1 },
	{ mnm_tay, AM_NON, 0 },
	{ mnm_lda, AM_IMM, 1 },
	{ mnm_tax, AM_NON, 0 },
	{ mnm_lax2, AM_IMM, 1 },
	{ mnm_ldy, AM_ABS, 2 },
	{ mnm_lda, AM_ABS, 2 },
	{ mnm_ldx, AM_ABS, 2 },
	{ mnm_lax, AM_ABS, 2 },
	{ mnm_bcs, AM_BRANCH, 1 },
	{ mnm_lda, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ZP_X, 1 },
	{ mnm_lda, AM_ZP_X, 1 },
	{ mnm_ldx, AM_ZP_Y, 1 },
	{ mnm_lax, AM_ZP_Y, 1 },
	{ mnm_clv, AM_NON, 0 },
	{ mnm_lda, AM_ABS_Y, 2 },
	{ mnm_tsx, AM_NON, 0 },
	{ mnm_las, AM_ABS_Y, 2 },
	{ mnm_ldy, AM_ABS_X, 2 },
	{ mnm_lda, AM_ABS_X, 2 },
	{ mnm_ldx, AM_ABS_Y, 2 },
	{ mnm_lax, AM_ABS_Y, 2 },
	{ mnm_cpy, AM_IMM, 1 },
	{ mnm_cmp, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ZP_REL_X, 1 },
	{ mnm_cpy, AM_ZP, 1 },
	{ mnm_cmp, AM_ZP, 1 },
	{ mnm_dec, AM_ZP, 1 },
	{ mnm_dcp, AM_ZP, 1 },
	{ mnm_iny, AM_NON, 0 },
	{ mnm_cmp, AM_IMM, 1 },
	{ mnm_dex, AM_NON, 0 },
	{ mnm_axs, AM_IMM, 1 },
	{ mnm_cpy, AM_ABS, 2 },
	{ mnm_cmp, AM_ABS, 2 },
	{ mnm_dec, AM_ABS, 2 },
	{ mnm_dcp, AM_ABS, 2 },
	{ mnm_bne, AM_BRANCH, 1 },
	{ mnm_cmp, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ZP_X, 1 },
	{ mnm_dec, AM_ZP_X, 1 },
	{ mnm_dcp, AM_ZP_X, 1 },
	{ mnm_cld, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dcp, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_X, 2 },
	{ mnm_dec, AM_ABS_X, 2 },
	{ mnm_dcp, AM_ABS_X, 2 },
	{ mnm_cpx, AM_IMM, 1 },
	{ mnm_sbc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ZP_REL_X, 1 },
	{ mnm_cpx, AM_ZP, 1 },
	{ mnm_sbc, AM_ZP, 1 },
	{ mnm_inc, AM_ZP, 1 },
	{ mnm_isc, AM_ZP, 1 },
	{ mnm_inx, AM_NON, 0 },
	{ mnm_sbc, AM_IMM, 1 },
	{ mnm_nop, AM_NON, 0 },
	{ mnm_sbi, AM_IMM, 1 },
	{ mnm_cpx, AM_ABS, 2 },
	{ mnm_sbc, AM_ABS, 2 },
	{ mnm_inc, AM_ABS, 2 },
	{ mnm_isc, AM_ABS, 2 },
	{ mnm_beq, AM_BRANCH, 1 },
	{ mnm_sbc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ZP_Y_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ZP_X, 1 },
	{ mnm_inc, AM_ZP_X, 1 },
	{ mnm_isc, AM_ZP_X, 1 },
	{ mnm_sed, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_isc, AM_ABS_Y, 2 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_X, 2 },
	{ mnm_inc, AM_ABS_X, 2 },
	{ mnm_isc, AM_ABS_X, 2 },
};

struct dismnm a65C02_ops[256] = {
	{ mnm_brk, AM_NON, 0},
	{ mnm_ora, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_tsb, AM_ZP, 1 },
	{ mnm_ora, AM_ZP, 1 },
	{ mnm_asl, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_php, AM_NON, 0 },
	{ mnm_ora, AM_IMM, 1 },
	{ mnm_asl, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_tsb, AM_ABS, 2 },
	{ mnm_ora, AM_ABS, 2 },
	{ mnm_asl, AM_ABS, 2 },
	{ mnm_bbr0, AM_ZP_ABS, 2 },
	{ mnm_bpl, AM_BRANCH, 1 },
	{ mnm_ora, AM_ZP_Y_REL, 1 },
	{ mnm_ora, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_trb, AM_ZP, 1 },
	{ mnm_ora, AM_ZP_X, 1 },
	{ mnm_asl, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_clc, AM_NON, 0 },
	{ mnm_ora, AM_ABS_Y, 2 },
	{ mnm_inc, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_trb, AM_ABS, 2 },
	{ mnm_ora, AM_ABS_X, 2 },
	{ mnm_asl, AM_ABS_X, 2 },
	{ mnm_bbr1, AM_ZP_ABS, 2 },
	{ mnm_jsr, AM_ABS, 2 },
	{ mnm_and, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_bit, AM_ZP, 1 },
	{ mnm_and, AM_ZP, 1 },
	{ mnm_rol, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_plp, AM_NON, 0 },
	{ mnm_and, AM_IMM, 1 },
	{ mnm_rol, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_bit, AM_ABS, 2 },
	{ mnm_and, AM_ABS, 2 },
	{ mnm_rol, AM_ABS, 2 },
	{ mnm_bbr2, AM_ZP_ABS, 2 },
	{ mnm_bmi, AM_BRANCH, 1 },
	{ mnm_and, AM_ZP_Y_REL, 1 },
	{ mnm_and, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_bit, AM_ZP_X, 1 },
	{ mnm_and, AM_ZP_X, 1 },
	{ mnm_rol, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sec, AM_NON, 0 },
	{ mnm_and, AM_ABS_Y, 2 },
	{ mnm_dec, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_bit, AM_ABS_X, 2 },
	{ mnm_and, AM_ABS_X, 2 },
	{ mnm_rol, AM_ABS_X, 2 },
	{ mnm_bbr3, AM_ZP_ABS, 2 },
	{ mnm_rti, AM_NON, 0 },
	{ mnm_eor, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP, 1 },
	{ mnm_lsr, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_pha, AM_NON, 0 },
	{ mnm_eor, AM_IMM, 1 },
	{ mnm_lsr, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_jmp, AM_ABS, 2 },
	{ mnm_eor, AM_ABS, 2 },
	{ mnm_lsr, AM_ABS, 2 },
	{ mnm_bbr4, AM_ZP_ABS, 2 },
	{ mnm_bvc, AM_BRANCH, 1 },
	{ mnm_eor, AM_ZP_Y_REL, 1 },
	{ mnm_eor, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ZP_X, 1 },
	{ mnm_lsr, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cli, AM_NON, 0 },
	{ mnm_eor, AM_ABS_Y, 2 },
	{ mnm_phy, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_eor, AM_ABS_X, 2 },
	{ mnm_lsr, AM_ABS_X, 2 },
	{ mnm_bbr5, AM_ZP_ABS, 2 },
	{ mnm_rts, AM_NON, 0 },
	{ mnm_adc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_stz, AM_ZP, 1 },
	{ mnm_adc, AM_ZP, 1 },
	{ mnm_ror, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_pla, AM_NON, 0 },
	{ mnm_adc, AM_IMM, 1 },
	{ mnm_ror, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_jmp, AM_REL, 2 },
	{ mnm_adc, AM_ABS, 2 },
	{ mnm_ror, AM_ABS, 2 },
	{ mnm_bbr6, AM_ZP_ABS, 2 },
	{ mnm_bvs, AM_BRANCH, 1 },
	{ mnm_adc, AM_ZP_Y_REL, 1 },
	{ mnm_adc, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_stz, AM_ZP_X, 1 },
	{ mnm_adc, AM_ZP_X, 1 },
	{ mnm_ror, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sei, AM_NON, 0 },
	{ mnm_adc, AM_ABS_Y, 2 },
	{ mnm_ply, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_jmp, AM_REL_X, 2 },
	{ mnm_adc, AM_ABS_X, 2 },
	{ mnm_ror, AM_ABS_X, 2 },
	{ mnm_bbr7, AM_ZP_ABS, 2 },
	{ mnm_bra, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sty, AM_ZP, 1 },
	{ mnm_sta, AM_ZP, 1 },
	{ mnm_stx, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_dey, AM_NON, 0 },
	{ mnm_bit, AM_IMM, 1 },
	{ mnm_txa, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sty, AM_ABS, 2 },
	{ mnm_sta, AM_ABS, 2 },
	{ mnm_stx, AM_ABS, 2 },
	{ mnm_bbs0, AM_ZP_ABS, 2 },
	{ mnm_bcc, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_Y_REL, 1 },
	{ mnm_sta, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sty, AM_ZP_X, 1 },
	{ mnm_sta, AM_ZP_X, 1 },
	{ mnm_stx, AM_ZP_Y, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_tya, AM_NON, 0 },
	{ mnm_sta, AM_ABS_Y, 2 },
	{ mnm_txs, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_stz, AM_ABS, 2 },
	{ mnm_sta, AM_ABS_X, 2 },
	{ mnm_stz, AM_ABS_X, 2 },
	{ mnm_bbs1, AM_ZP_ABS, 2 },
	{ mnm_ldy, AM_IMM, 1 },
	{ mnm_lda, AM_ZP_REL_X, 1 },
	{ mnm_ldx, AM_IMM, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ZP, 1 },
	{ mnm_lda, AM_ZP, 1 },
	{ mnm_ldx, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_tay, AM_NON, 0 },
	{ mnm_lda, AM_IMM, 1 },
	{ mnm_tax, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ABS, 2 },
	{ mnm_lda, AM_ABS, 2 },
	{ mnm_ldx, AM_ABS, 2 },
	{ mnm_bbs2, AM_ZP_ABS, 2 },
	{ mnm_bcs, AM_BRANCH, 1 },
	{ mnm_lda, AM_ZP_Y_REL, 1 },
	{ mnm_lda, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ZP_X, 1 },
	{ mnm_lda, AM_ZP_X, 1 },
	{ mnm_ldx, AM_ZP_Y, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_clv, AM_NON, 0 },
	{ mnm_lda, AM_ABS_Y, 2 },
	{ mnm_tsx, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_ldy, AM_ABS_X, 2 },
	{ mnm_lda, AM_ABS_X, 2 },
	{ mnm_ldx, AM_ABS_Y, 2 },
	{ mnm_bbs3, AM_ZP_ABS, 2 },
	{ mnm_cpy, AM_IMM, 1 },
	{ mnm_cmp, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cpy, AM_ZP, 1 },
	{ mnm_cmp, AM_ZP, 1 },
	{ mnm_dec, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_iny, AM_NON, 0 },
	{ mnm_cmp, AM_IMM, 1 },
	{ mnm_dex, AM_NON, 0 },
	{ mnm_wai, AM_NON, 0 },
	{ mnm_cpy, AM_ABS, 2 },
	{ mnm_cmp, AM_ABS, 2 },
	{ mnm_dec, AM_ABS, 2 },
	{ mnm_bbs4, AM_ZP_ABS, 2 },
	{ mnm_bne, AM_BRANCH, 1 },
	{ mnm_cmp, AM_ZP_Y_REL, 1 },
	{ mnm_cmp, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ZP_X, 1 },
	{ mnm_dec, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cld, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_Y, 2 },
	{ mnm_phx, AM_NON, 0 },
	{ mnm_stp, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_X, 2 },
	{ mnm_dec, AM_ABS_X, 2 },
	{ mnm_bbs5, AM_ZP_ABS, 2 },
	{ mnm_cpx, AM_IMM, 1 },
	{ mnm_sbc, AM_ZP_REL_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cpx, AM_ZP, 1 },
	{ mnm_sbc, AM_ZP, 1 },
	{ mnm_inc, AM_ZP, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inx, AM_NON, 0 },
	{ mnm_sbc, AM_IMM, 1 },
	{ mnm_nop, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_cpx, AM_ABS, 2 },
	{ mnm_sbc, AM_ABS, 2 },
	{ mnm_inc, AM_ABS, 2 },
	{ mnm_bbs6, AM_ZP_ABS, 2 },
	{ mnm_beq, AM_BRANCH, 1 },
	{ mnm_sbc, AM_ZP_Y_REL, 1 },
	{ mnm_sbc, AM_ZP_REL, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ZP_X, 1 },
	{ mnm_inc, AM_ZP_X, 1 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sed, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_Y, 2 },
	{ mnm_plx, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_inv, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_X, 2 },
	{ mnm_inc, AM_ABS_X, 2 },
	{ mnm_bbs7, AM_ZP_ABS, 2 },
};

struct dismnm a65816_ops[256] = {
	{ mnm_brk, AM_NON, 0},
	{ mnm_ora, AM_ZP_REL_X, 1 },
	{ mnm_cop, AM_NON, 0 },
	{ mnm_ora, AM_STK, 1 },
	{ mnm_tsb, AM_ZP, 1 },
	{ mnm_ora, AM_ZP, 1 },
	{ mnm_asl, AM_ZP, 1 },
	{ mnm_ora, AM_ZP_REL_L, 1 },
	{ mnm_php, AM_NON, 0 },
	{ mnm_ora, AM_IMM_DBL_A, 1 },
	{ mnm_asl, AM_NON, 0 },
	{ mnm_phd, AM_NON, 0 },
	{ mnm_tsb, AM_ABS, 2 },
	{ mnm_ora, AM_ABS, 2 },
	{ mnm_asl, AM_ABS, 2 },
	{ mnm_ora, AM_ABS_L, 3 },
	{ mnm_bpl, AM_BRANCH, 1 },
	{ mnm_ora, AM_ZP_Y_REL, 1 },
	{ mnm_ora, AM_ZP_REL, 1 },
	{ mnm_ora, AM_STK_REL_Y, 1 },
	{ mnm_trb, AM_ZP, 1 },
	{ mnm_ora, AM_ZP_X, 1 },
	{ mnm_asl, AM_ZP_X, 1 },
	{ mnm_ora, AM_ZP_REL_Y_L, 1 },
	{ mnm_clc, AM_NON, 0 },
	{ mnm_ora, AM_ABS_Y, 2 },
	{ mnm_inc, AM_NON, 0 },
	{ mnm_tcs, AM_NON, 0 },
	{ mnm_trb, AM_ABS, 2 },
	{ mnm_ora, AM_ABS_X, 2 },
	{ mnm_asl, AM_ABS_X, 2 },
	{ mnm_ora, AM_ABS_L_X, 3 },
	{ mnm_jsr, AM_ABS, 2 },
	{ mnm_and, AM_ZP_REL_X, 1 },
	{ mnm_jsr, AM_ABS_L, 3 },
	{ mnm_and, AM_STK, 1 },
	{ mnm_bit, AM_ZP, 1 },
	{ mnm_and, AM_ZP, 1 },
	{ mnm_rol, AM_ZP, 1 },
	{ mnm_and, AM_ZP_REL_L, 1 },
	{ mnm_plp, AM_NON, 0 },
	{ mnm_and, AM_IMM_DBL_A, 1 },
	{ mnm_rol, AM_NON, 0 },
	{ mnm_pld, AM_NON, 0 },
	{ mnm_bit, AM_ABS, 2 },
	{ mnm_and, AM_ABS, 2 },
	{ mnm_rol, AM_ABS, 2 },
	{ mnm_and, AM_ABS_L, 3 },
	{ mnm_bmi, AM_BRANCH, 1 },
	{ mnm_and, AM_ZP_Y_REL, 1 },
	{ mnm_and, AM_ZP_REL, 1 },
	{ mnm_and, AM_STK_REL_Y, 1 },
	{ mnm_bit, AM_ZP_X, 1 },
	{ mnm_and, AM_ZP_X, 1 },
	{ mnm_rol, AM_ZP_X, 1 },
	{ mnm_and, AM_ZP_REL_Y_L, 1 },
	{ mnm_sec, AM_NON, 0 },
	{ mnm_and, AM_ABS_Y, 2 },
	{ mnm_dec, AM_NON, 0 },
	{ mnm_tsc, AM_NON, 0 },
	{ mnm_bit, AM_ABS_X, 2 },
	{ mnm_and, AM_ABS_X, 2 },
	{ mnm_rol, AM_ABS_X, 2 },
	{ mnm_and, AM_ABS_L_X, 3 },
	{ mnm_rti, AM_NON, 0 },
	{ mnm_eor, AM_ZP_REL_X, 1 },
	{ mnm_wdm, AM_NON, 0 },
	{ mnm_eor, AM_STK, 1 },
	{ mnm_mvp, AM_BLK_MOV, 2 },
	{ mnm_eor, AM_ZP, 1 },
	{ mnm_lsr, AM_ZP, 1 },
	{ mnm_eor, AM_ZP_REL_L, 1 },
	{ mnm_pha, AM_NON, 0 },
	{ mnm_eor, AM_IMM_DBL_A, 1 },
	{ mnm_lsr, AM_NON, 0 },
	{ mnm_phk, AM_NON, 0 },
	{ mnm_jmp, AM_ABS, 2 },
	{ mnm_eor, AM_ABS, 2 },
	{ mnm_lsr, AM_ABS, 2 },
	{ mnm_eor, AM_ABS_L, 3 },
	{ mnm_bvc, AM_BRANCH, 1 },
	{ mnm_eor, AM_ZP_Y_REL, 1 },
	{ mnm_eor, AM_ZP_REL, 1 },
	{ mnm_eor, AM_STK_REL_Y, 1 },
	{ mnm_mvn, AM_BLK_MOV, 2 },
	{ mnm_eor, AM_ZP_X, 1 },
	{ mnm_lsr, AM_ZP_X, 1 },
	{ mnm_eor, AM_ZP_REL_Y_L, 1 },
	{ mnm_cli, AM_NON, 0 },
	{ mnm_eor, AM_ABS_Y, 2 },
	{ mnm_phy, AM_NON, 0 },
	{ mnm_tcd, AM_NON, 0 },
	{ mnm_jmp, AM_ABS_L, 3 },
	{ mnm_eor, AM_ABS_X, 2 },
	{ mnm_lsr, AM_ABS_X, 2 },
	{ mnm_eor, AM_ABS_L_X, 3 },
	{ mnm_rts, AM_NON, 0 },
	{ mnm_adc, AM_ZP_REL_X, 1 },
	{ mnm_per, AM_BRANCH_L, 2 },
	{ mnm_adc, AM_STK, 1 },
	{ mnm_stz, AM_ZP, 1 },
	{ mnm_adc, AM_ZP, 1 },
	{ mnm_ror, AM_ZP, 1 },
	{ mnm_adc, AM_ZP_REL_L, 1 },
	{ mnm_pla, AM_NON, 0 },
	{ mnm_adc, AM_IMM_DBL_A, 1 },
	{ mnm_ror, AM_NON, 0 },
	{ mnm_rtl, AM_NON, 0 },
	{ mnm_jmp, AM_REL, 2 },
	{ mnm_adc, AM_ABS, 2 },
	{ mnm_ror, AM_ABS, 2 },
	{ mnm_adc, AM_ABS_L, 3 },
	{ mnm_bvs, AM_BRANCH, 1 },
	{ mnm_adc, AM_ZP_Y_REL, 1 },
	{ mnm_adc, AM_ZP_REL, 1 },
	{ mnm_adc, AM_STK_REL_Y, 1 },
	{ mnm_stz, AM_ZP_X, 1 },
	{ mnm_adc, AM_ZP_X, 1 },
	{ mnm_ror, AM_ZP_X, 1 },
	{ mnm_adc, AM_ZP_REL_Y_L, 1 },
	{ mnm_sei, AM_NON, 0 },
	{ mnm_adc, AM_ABS_Y, 2 },
	{ mnm_ply, AM_NON, 0 },
	{ mnm_tdc, AM_NON, 0 },
	{ mnm_jmp, AM_REL_X, 2 },
	{ mnm_adc, AM_ABS_X, 2 },
	{ mnm_ror, AM_ABS_X, 2 },
	{ mnm_adc, AM_ABS_L_X, 3 },
	{ mnm_bra, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_REL_X, 1 },
	{ mnm_brl, AM_BRANCH_L, 2 },
	{ mnm_sta, AM_STK, 1 },
	{ mnm_sty, AM_ZP, 1 },
	{ mnm_sta, AM_ZP, 1 },
	{ mnm_stx, AM_ZP, 1 },
	{ mnm_sta, AM_ZP_REL_L, 1 },
	{ mnm_dey, AM_NON, 0 },
	{ mnm_bit, AM_IMM_DBL_A, 1 },
	{ mnm_txa, AM_NON, 0 },
	{ mnm_phb, AM_NON, 0 },
	{ mnm_sty, AM_ABS, 2 },
	{ mnm_sta, AM_ABS, 2 },
	{ mnm_stx, AM_ABS, 2 },
	{ mnm_sta, AM_ABS_L, 3 },
	{ mnm_bcc, AM_BRANCH, 1 },
	{ mnm_sta, AM_ZP_Y_REL, 1 },
	{ mnm_sta, AM_ZP_REL, 1 },
	{ mnm_sta, AM_STK_REL_Y, 1 },
	{ mnm_sty, AM_ZP_X, 1 },
	{ mnm_sta, AM_ZP_X, 1 },
	{ mnm_stx, AM_ZP_Y, 1 },
	{ mnm_sta, AM_ZP_REL_Y_L, 1 },
	{ mnm_tya, AM_NON, 0 },
	{ mnm_sta, AM_ABS_Y, 2 },
	{ mnm_txs, AM_NON, 0 },
	{ mnm_txy, AM_NON, 0 },
	{ mnm_stz, AM_ABS, 2 },
	{ mnm_sta, AM_ABS_X, 2 },
	{ mnm_stz, AM_ABS_X, 2 },
	{ mnm_sta, AM_ABS_L_X, 3 },
	{ mnm_ldy, AM_IMM_DBL_I, 1 },
	{ mnm_lda, AM_ZP_REL_X, 1 },
	{ mnm_ldx, AM_IMM_DBL_I, 1 },
	{ mnm_lda, AM_STK, 1 },
	{ mnm_ldy, AM_ZP, 1 },
	{ mnm_lda, AM_ZP, 1 },
	{ mnm_ldx, AM_ZP, 1 },
	{ mnm_lda, AM_ZP_REL_L, 1 },
	{ mnm_tay, AM_NON, 0 },
	{ mnm_lda, AM_IMM_DBL_A, 1 },
	{ mnm_tax, AM_NON, 0 },
	{ mnm_plb, AM_NON, 0 },
	{ mnm_ldy, AM_ABS, 2 },
	{ mnm_lda, AM_ABS, 2 },
	{ mnm_ldx, AM_ABS, 2 },
	{ mnm_lda, AM_ABS_L, 3 },
	{ mnm_bcs, AM_BRANCH, 1 },
	{ mnm_lda, AM_ZP_Y_REL, 1 },
	{ mnm_lda, AM_ZP_REL, 1 },
	{ mnm_lda, AM_STK_REL_Y, 1 },
	{ mnm_ldy, AM_ZP_X, 1 },
	{ mnm_lda, AM_ZP_X, 1 },
	{ mnm_ldx, AM_ZP_Y, 1 },
	{ mnm_lda, AM_ZP_REL_Y_L, 1 },
	{ mnm_clv, AM_NON, 0 },
	{ mnm_lda, AM_ABS_Y, 2 },
	{ mnm_tsx, AM_NON, 0 },
	{ mnm_tyx, AM_NON, 0 },
	{ mnm_ldy, AM_ABS_X, 2 },
	{ mnm_lda, AM_ABS_X, 2 },
	{ mnm_ldx, AM_ABS_Y, 2 },
	{ mnm_lda, AM_ABS_L_X, 3 },
	{ mnm_cpy, AM_IMM_DBL_I, 1 },
	{ mnm_cmp, AM_ZP_REL_X, 1 },
	{ mnm_rep, AM_IMM, 1 },
	{ mnm_cmp, AM_STK, 1 },
	{ mnm_cpy, AM_ZP, 1 },
	{ mnm_cmp, AM_ZP, 1 },
	{ mnm_dec, AM_ZP, 1 },
	{ mnm_cmp, AM_ZP_REL_L, 1 },
	{ mnm_iny, AM_NON, 0 },
	{ mnm_cmp, AM_IMM_DBL_A, 1 },
	{ mnm_dex, AM_NON, 0 },
	{ mnm_wai, AM_NON, 0 },
	{ mnm_cpy, AM_ABS, 2 },
	{ mnm_cmp, AM_ABS, 2 },
	{ mnm_dec, AM_ABS, 2 },
	{ mnm_cmp, AM_ABS_L, 3 },
	{ mnm_bne, AM_BRANCH, 1 },
	{ mnm_cmp, AM_ZP_Y_REL, 1 },
	{ mnm_cmp, AM_ZP_REL, 1 },
	{ mnm_cmp, AM_STK_REL_Y, 1 },
	{ mnm_pei, AM_ZP_REL, 1 },
	{ mnm_cmp, AM_ZP_X, 1 },
	{ mnm_dec, AM_ZP_X, 1 },
	{ mnm_cmp, AM_ZP_REL_Y_L, 1 },
	{ mnm_cld, AM_NON, 0 },
	{ mnm_cmp, AM_ABS_Y, 2 },
	{ mnm_phx, AM_NON, 0 },
	{ mnm_stp, AM_NON, 0 },
	{ mnm_jmp, AM_REL_L, 2 },
	{ mnm_cmp, AM_ABS_X, 2 },
	{ mnm_dec, AM_ABS_X, 2 },
	{ mnm_cmp, AM_ABS_L_X, 3 },
	{ mnm_cpx, AM_IMM_DBL_I, 1 },
	{ mnm_sbc, AM_ZP_REL_X, 1 },
	{ mnm_sep, AM_IMM, 1 },
	{ mnm_sbc, AM_STK, 1 },
	{ mnm_cpx, AM_ZP, 1 },
	{ mnm_sbc, AM_ZP, 1 },
	{ mnm_inc, AM_ZP, 1 },
	{ mnm_sbc, AM_ZP_REL_L, 1 },
	{ mnm_inx, AM_NON, 0 },
	{ mnm_sbc, AM_IMM_DBL_A, 1 },
	{ mnm_nop, AM_NON, 0 },
	{ mnm_xba, AM_NON, 0 },
	{ mnm_cpx, AM_ABS, 2 },
	{ mnm_sbc, AM_ABS, 2 },
	{ mnm_inc, AM_ABS, 2 },
	{ mnm_sbc, AM_ABS_L, 3 },
	{ mnm_beq, AM_BRANCH, 1 },
	{ mnm_sbc, AM_ZP_Y_REL, 1 },
	{ mnm_sbc, AM_ZP_REL, 1 },
	{ mnm_sbc, AM_STK_REL_Y, 1 },
	{ mnm_pea, AM_ABS, 2 },
	{ mnm_sbc, AM_ZP_X, 1 },
	{ mnm_inc, AM_ZP_X, 1 },
	{ mnm_sbc, AM_ZP_REL_Y_L, 1 },
	{ mnm_sed, AM_NON, 0 },
	{ mnm_sbc, AM_ABS_Y, 2 },
	{ mnm_plx, AM_NON, 0 },
	{ mnm_xce, AM_NON, 0 },
	{ mnm_jsr, AM_REL_X, 2 },
	{ mnm_sbc, AM_ABS_X, 2 },
	{ mnm_inc, AM_ABS_X, 2 },
	{ mnm_sbc, AM_ABS_L_X, 3 },
};

enum RefType {
	RT_NONE,
	RT_BRANCH,	// bne, etc.
	RT_BRA_L,	// brl
	RT_JUMP,	// jmp
	RT_JSR,		// jsr
	RT_DATA,	// lda $...
	RT_ZP,		// using a zero page / direct page instruction
	RT_COUNT
};

enum DataType {
	DT_CODE,
	DT_DATA,
	DT_PTRS,
	DT_PTRS_DATA
};

const char *aRefNames[RT_COUNT] = { "???", "branch", "long branch", "jump", "subroutine", "data", "zp" };

struct RefLink {
	int instr_addr;
	RefType type;
};

struct RefAddr {
	int address:29;					// address
	int data:3;						// 0 if code, 1 if data, 2 if pointers
	int size:16;					// user specified size
	int local:1;					// nonzero if local label
	int separator:1;				// nonzero if following a separator
	int number:15;					// label count
	strref label;					// user defined label
	strref comment;
	strref read_only_label;			// for labels that have a different meaning between reading and writing
	std::vector<RefLink> *pRefs;	// what is referencing this address

	RefAddr() : address(-1), data(0), size(0), local(0), separator(0), number(-1), pRefs(nullptr) {}
	RefAddr(int addr) : address(addr), data(0), size(0), local(0), separator(0), number(-1), pRefs(nullptr) {}
};

std::vector<RefAddr> refs;

static int _sortRefs(const void *A, const void *B)
{
	return ((const RefAddr*)A)->address - ((const RefAddr*)B)->address;
}

static const strref kw_data("data");
static const strref kw_code("code");
static const strref kw_read("read");
static const strref kw_pointers("pointers");

int GetLabelIndex(int addr) {
	for (int i = 0; i<(int)refs.size(); i++) {
		if (addr == refs[i].address)
			return i;
	}
	return -1;
}


void GetReferences(unsigned char *mem, size_t bytes, bool acc_16, bool ind_16, int addr, bool ill_wdc, const dismnm *opcodes, int init_data, strref labels)
{
	int start_addr = addr;
	int end_addr = addr + (int)bytes;

	while (strref lab_line = labels.line()) {
		strref name = lab_line.split_token_trim('=');
		if (lab_line.get_first()=='$')
			++lab_line;
		unsigned int address = lab_line.ahextoui_skip();
		int size = 0;
		lab_line.skip_whitespace();
		if (lab_line.get_first()=='-') {
			++lab_line;
			if (lab_line.get_first()=='$')
				++lab_line;
			size = lab_line.ahextoui_skip() - address;
			if (size<0)
				size = 0;
		}
		if (lab_line.get_first()==',')
			++lab_line;
		lab_line.skip_whitespace();

		if (kw_read.is_prefix_word(lab_line)) {
			int label_index = GetLabelIndex(address);
			if (label_index>=0)
				refs[label_index].read_only_label = name;
		} else {
			refs.push_back(RefAddr(address));
			RefAddr &r = refs[refs.size()-1];
			r.pRefs = new std::vector<RefLink>();
			r.size = size;
			if (kw_data.is_prefix_word(lab_line)) {
				r.data = DT_DATA;
				lab_line += kw_data.get_len();
			} else if (kw_code.is_prefix_word(lab_line)) {
				r.data = DT_CODE;
				lab_line += kw_code.get_len();
			} else if (kw_pointers.is_prefix_word(lab_line)) {
				lab_line += kw_pointers.get_len();
				lab_line.skip_whitespace();
				if (kw_data.is_prefix_word(lab_line))
					r.data = DT_PTRS_DATA;
				else
					r.data = DT_PTRS;
			}
			lab_line.trim_whitespace();
			r.label = name;
			r.comment = lab_line;
		}
	}

	if (GetLabelIndex(start_addr)<0) {
		refs.push_back(RefAddr(start_addr));
		refs[refs.size()-1].pRefs = new std::vector<RefLink>();
		refs[refs.size()-1].data = DT_DATA;
	} if (init_data && GetLabelIndex(start_addr + init_data)<0) {
		refs.push_back(RefAddr(start_addr+init_data));
		refs[refs.size()-1].pRefs = new std::vector<RefLink>();
	}

	if (refs.size())
		qsort(&refs[0], refs.size(), sizeof(RefAddr), _sortRefs);

	int last_user = (int)refs.size();
	for (int i = 0; i<last_user; ++i) {
		if (refs[i].data==DT_PTRS || refs[i].data==DT_PTRS_DATA) {
			int num = refs[i].size ? (refs[i].size/2) : ((refs[i+1].address - refs[i].address)/2);
			if (refs[i].address>=addr && (refs[i].address+refs[i].size)<=(addr+bytes)) {
				unsigned char *p = mem + refs[i].address - addr;
				for (int l = 0; l<num; l++) {
					int a = p[0] + ((unsigned short)p[1]<<8);
					int n = GetLabelIndex(a);
					int nr = (int)refs.size();
					struct RefLink ref = { 2*l + refs[i].address, refs[i].data==DT_PTRS ? RT_JSR : RT_DATA };
					if (n<0) {
						refs.push_back(RefAddr(a));
						refs[nr].pRefs = new std::vector<RefLink>();
						refs[nr].data = refs[i].data==DT_PTRS_DATA ? DT_DATA : DT_CODE;
						refs[nr].pRefs->push_back(ref);
					} else
						refs[n].pRefs->push_back(ref);
					p += 2;
				}
			}
		}
	}

	unsigned char *mem_orig = mem;
	size_t bytes_orig = bytes;
	int addr_orig = addr;

	if (size_t(init_data)>bytes)
		return;

	if (refs.size())
		qsort(&refs[0], refs.size(), sizeof(RefAddr), _sortRefs);

	bool curr_data = !!init_data;
	int curr_label = 0;
	int start_labels = (int)refs.size();
	while (curr_label<start_labels && addr>refs[curr_label].address) {
		curr_data = refs[curr_label].data == DT_CODE;
		++curr_label;
	}

	while (bytes) {
		if (curr_label<start_labels) {
			if (addr>=refs[curr_label].address) {
				while (curr_label<start_labels && addr>=refs[curr_label].address) {
					curr_data = refs[curr_label].data != DT_CODE;
					if (addr>=(refs[curr_label].address+refs[curr_label].size))
						++curr_label;
					else
						break;
				}
			} else if (curr_label && addr>(refs[curr_label-1].address + refs[curr_label-1].size))
				curr_data = false;
		} else
			curr_data = false;

		unsigned char op = *mem++;
		int curr = addr;
		bytes--;
		addr++;
		int reference = -1;
		RefType type = RT_NONE;
		int arg_size = 0;
		int mode = 0;

		if (!curr_data) {
			if (opcodes == a65816_ops) {
				if (op == 0xe2) {	// sep
					if ((*mem)&0x20) acc_16 = false;
					if ((*mem)&0x10) ind_16 = false;
				} else if (op == 0xc2) { // rep
					if ((*mem)&0x20) acc_16 = true;
					if ((*mem)&0x10) ind_16 = true;
				}
			}

			bool not_valid = opcodes[op].mnemonic==mnm_inv || (!ill_wdc && opcodes[op].mnemonic>=mnm_wdc_and_illegal_instructions);

			arg_size = not_valid ? 0 : opcodes[op].arg_size;;
			mode = not_valid ? AM_NON : opcodes[op].addrMode;
			switch (mode) {
				case AM_IMM_DBL_A:
					arg_size = acc_16 ? 2 : 1;
					break;
				case AM_IMM_DBL_I:
					arg_size = ind_16 ? 2 : 1;
					break;
			}

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
				else if (op == 0x4c || op == 0x7c || op == 0x5c || op == 0xdc)	// jmp opcodes
					type = RT_JUMP;
				else
					type = RT_DATA;
			} else if (mode == AM_ZP || mode == AM_ZP_REL || mode == AM_ZP_REL_L || mode == AM_ZP_REL_X || mode == AM_ZP_Y_REL ||
					   mode == AM_ZP_REL_Y || mode == AM_ZP_X || mode == AM_ZP_Y || mode == AM_ZP_REL_Y_L) {
				reference = mem[0];
				type = RT_ZP;
			}
		}

		if (reference>=0 && type != RT_NONE) {
			bool found = false;
			for (std::vector<RefAddr>::iterator i = refs.begin(); i != refs.end(); ++i) {
				if (i->address == reference || (reference>i->address && (reference-i->size)<i->address)) {
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
	curr_label = 0;
	int prev_addr = -1;
	bool was_data = init_data>0;
	bool separator = false;
	int prev_op = 0xff;
	addr += init_data;
	bytes -= init_data;
	mem += init_data;
	bool cutoff = false;

	while (curr_label<(int)refs.size() && refs[curr_label].address<addr) {
		refs[curr_label].data = DT_DATA;
		++curr_label;
	}

	if (curr_label<(int)refs.size())
		refs[curr_label].data = init_data ? DT_DATA : DT_CODE;

	while (bytes && curr_label<(int)refs.size()) {
		unsigned char op = *mem++;
		int curr = addr;
		bytes--;
		addr++;
		if (!was_data) {
			if (opcodes == a65816_ops) {
				if (op == 0xe2) {	// sep
					if ((*mem)&0x20) acc_16 = false;
					if ((*mem)&0x10) ind_16 = false;
				} else if (op == 0xc2) { // rep
					if ((*mem)&0x20) acc_16 = true;
					if ((*mem)&0x10) ind_16 = true;
				}
			}

			bool not_valid = opcodes[op].mnemonic==mnm_inv || (!ill_wdc && opcodes[op].mnemonic>=mnm_wdc_and_illegal_instructions);

			int arg_size = not_valid ? 0 : opcodes[op].arg_size;;
			int mode = not_valid ? AM_NON : opcodes[op].addrMode;
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
		}
		if (separator && curr_label>0 && refs[curr_label-1].data!=DT_PTRS && refs[curr_label-1].data!=DT_PTRS_DATA &&
				curr!=refs[curr_label].address && !cutoff) {
			int end_addr = curr_label<(int)refs.size() ? refs[curr_label].address : (int)(addr_orig + bytes_orig);
			for (std::vector<RefAddr>::iterator k = refs.begin(); k!= refs.end(); ++k) {
				std::vector<RefLink> &l = *k->pRefs;
				std::vector<RefLink>::iterator r = l.begin();
				while (r!=l.end()) {
					if (r->instr_addr>=curr && r->instr_addr<end_addr) {
						r = l.erase(r);
					}  else
						++r;
				}
			}
			cutoff = true;
		}

		if (curr == refs[curr_label].address || (curr > refs[curr_label].address && prev_addr>=0)) {
			if (curr != refs[curr_label].address && !refs[curr_label].label) {
				int curr_look_ahead = curr_label;
				while ((curr_look_ahead+1)<(int)refs.size() && curr>refs[curr_look_ahead].address && curr>=refs[curr_look_ahead+1].address)
					curr_look_ahead++;
				if (curr_look_ahead>curr_label)
					curr_label = curr_look_ahead;
				if (curr>refs[curr_label].address)
					refs[curr_label].address = prev_addr;
			}
			std::vector<RefLink> &pRefs = *refs[curr_label].pRefs;
			if (curr_label < (int)refs.size()) {
				if (refs[curr_label].label) {
					refs[curr_label].separator = 1;	// user labels are always global
					was_data = refs[curr_label].data!=DT_CODE;
				} else {
					bool prev_data = was_data;
					was_data = separator && !(!was_data && op==0x4c && prev_op==0x4c);
					if (!prev_data) {
						for (size_t j = 0; j<pRefs.size(); ++j) {
							RefType type = pRefs[j].type;
							if (!(pRefs[j].instr_addr>curr && type == RT_BRANCH) && type != RT_DATA) {
								was_data = false;
								prev_data = false;
								break;
							}
						}
					}

					if (!was_data && prev_data) {
						bool only_data_ref = pRefs.size() ? true : false;
						if (!curr_label || refs[curr_label-1].data == DT_CODE) {
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
						} else
							was_data = true;
					}

					// check all references
					if (was_data) {
						for (size_t j = 0; j<pRefs.size(); ++j) {
							RefLink &rl = pRefs[j];
							if (rl.type == RT_BRA_L || rl.type == RT_JSR || rl.type == RT_JUMP) {
								was_data = false;
								break;
							}
						}
					}

					refs[curr_label].separator = separator;
					refs[curr_label].data = was_data ? DT_DATA : DT_CODE;
				}
				if (was_data && refs[curr_label].data != DT_PTRS && refs[curr_label].data != DT_PTRS_DATA) {
					int start = curr;
					int end = (curr_label+1)<(int)refs.size() ? refs[curr_label+1].address : (int)(addr + bytes);
					for (int k = 0; k<(int)refs.size(); ++k) {
						std::vector<RefLink> &pRefs = *refs[k].pRefs;
						std::vector<RefLink>::iterator r = pRefs.begin();
						while (r != pRefs.end()) {
							if (r->instr_addr>=start && r->instr_addr<end) {
								r = pRefs.erase(r);
							} else
								++r;
						}
					}
				}
				if (refs[curr_label].pRefs->size()==0 && !refs[curr_label].label) {
					int lbl = curr_label;
					while (lbl && refs[lbl].pRefs->size()==0)
						lbl--;
					was_data = refs[lbl].data != DT_CODE;
				}
			}
			curr_label++;
		}
		separator = false;

		// after a separator if there is no jmp, jsr, brl begin data block
		if (!was_data) {
			if (op == 0x60 || op == 0x40 || op == 0x6b ||
				((op == 0x4c || op == 0x6c) && (prev_op!=0x4c && prev_op!=0x6c)) ||
				op == 0x6c || op == 0x7c || op == 0x5c || op == 0xdc) {	// rts, rti, rtl or jmp
				bool exit_instr = false;
				int lbl = GetLabelIndex(addr);
				if (lbl>=0) {
					std::vector<RefLink> &links = *refs[lbl].pRefs;
					for (std::vector<RefLink>::iterator lnk = links.begin(); lnk!=links.end(); ++lnk) {
						if (lnk->type == RT_BRANCH && lnk->instr_addr<addr) {
							exit_instr = true;
							break;
						}
					}
				} // branch over rts/jmp or sequential jmp doesn't count as separator
				if (!exit_instr && op!=0x4c && bytes && *mem!=0x4c) {
					separator = true;
					cutoff = false;
				}
			}
		}

		prev_op = op;
		prev_addr = curr;
	}

	

	int last = (int)refs.size()-1;
	while (last>1 && refs[last-1].data!=DT_CODE) {
		int start_addr = refs[last].address;
		for (int k = 0; k<(int)refs.size(); ++k) {
			std::vector<RefLink> &pRefs = *refs[k].pRefs;
			std::vector<RefLink>::iterator r = pRefs.begin();
			while (r != pRefs.end()) {
				if (r->instr_addr>=start_addr && r->instr_addr<end_addr) {
					r = pRefs.erase(r);
				}
				else
					++r;
			}
		}
		last--;
		if (last<=1 || (refs[last].data==DT_CODE || refs[last].data==DT_PTRS))
			break;
	}

	std::vector<RefAddr>::iterator k = refs.begin();
	while (k!=refs.end()) {
		if (k->pRefs && k->pRefs->size()==0 && !k->label && !k->separator && k->address!=addr_orig) {
			delete k->pRefs;
			k = refs.erase(k);
		} else
			++k;
	}

	// remove duplicate labels
	k = refs.begin();
	while (k!=refs.end()) {
		std::vector<RefAddr>::iterator n = k;
		++n;
		if (n != refs.end() && k->address == n->address) {
			std::vector<RefLink> &pRefs = *k->pRefs;
			std::vector<RefLink> &pRefs2 = *n->pRefs;
			std::vector<RefLink>::iterator r = pRefs2.begin();
			while (r != pRefs2.end()) {
				pRefs.push_back(*r);
				r = pRefs2.erase(r);
			}
			if (n->separator)
				k->separator = 1;
			delete &pRefs2;
			refs.erase(n);
		}
		++k;
	}

	// figure out which labels can be local

	k = refs.begin();	// make all labels that are not assigned something local
	bool was_sep = false;
	while (k!=refs.end()) {
		if (k->label || k->address<0x200 || k->separator)
			k->local = false;
		else
			k->local = true;
		was_sep = !!k->separator;
		++k;
	}

	// check over and over for global labels breaking local boundaries
	bool check_locals_completed = false;
	while (!check_locals_completed) {
		check_locals_completed = true;
		k = refs.begin();	// make all labels that are not assigned something local
		while (k!=refs.end()) {
			if (k->local) {
				std::vector<RefLink> &links = *k->pRefs;
				for (std::vector<RefLink>::iterator l = links.begin(); k->local && l!=links.end(); ++l) {
					int trg_addr = l->instr_addr;
					bool skip_global = false;
					std::vector<RefAddr>::iterator j = k;
					if (trg_addr<k->address) {	// backward check
						for (;;) {
							if (!j->local)
								skip_global = true;
							if (j!=refs.begin())
								--j;
							if (trg_addr>=j->address || j == refs.begin())
								break;
						}
					} else { // forward check
						while (j!=refs.end()) {
							++j;
							if (j==refs.end() || trg_addr<j->address)
								break;
							if (j!=refs.end() && !j->local)
								skip_global = true;
						}
					}
					if (skip_global) {
						if (j!=refs.end()) {
							k->local = false;
							check_locals_completed = false;
						}
					}
				}
			}
			++k;
		}
	}

	// now simply assign label numbers according to locality
	k = refs.begin();
	int label_count_code_global = 1;
	int label_count_data_global = 1;
	int label_count_local = 1;
	while (k!=refs.end()) {
		if (k->local) {
			k->number = label_count_local++;
		} else if (k->data==DT_CODE) {
			label_count_local = 1;
			k->number = label_count_data_global++;
		} else {
			label_count_local = 1;
			k->number = label_count_code_global++;
		}
		++k;
	}

	// re-check for code references to code being mislabeled
	bool adjusted_code_ref = true;
	while (adjusted_code_ref) {
		adjusted_code_ref = false;
		k = refs.begin();
		while (k!=refs.end()) {
			if (k->data != DT_CODE && !k->label) {
				std::vector<RefLink> &links = *k->pRefs;
				for (std::vector<RefLink>::iterator l = links.begin(); l!=links.end(); ++l) {
					RefType t = l->type;
					if (t==RT_BRANCH || t==RT_BRA_L || t==RT_JUMP || t==RT_JSR) {
						int trg_addr = l->instr_addr;
						std::vector<RefAddr>::iterator j = k;
						if (trg_addr<k->address) {
							while (j!=refs.begin() && trg_addr<j->address)
								--j;
						} else {
							std::vector<RefAddr>::iterator n = j;
							while (n!=refs.end() && trg_addr>n->address) {
								j = n;
								++n;
							}
						}
						if (j->data == DT_CODE) {
							k->data = DT_CODE;
							adjusted_code_ref = true;
						}
						break;
					}
				}
			}
			++k;
		}
	}
}

// returns true if the instruction accessing memory can ONLY read that memory, not write back or jump to it
// used to distinguish read-only labels when there is a different definition between reading and writing to the
// same physical address
bool IsReadOnlyInstruction(MNM_Base op_base)
{
	switch (op_base) {
		case mnm_ora:
		case mnm_and:
		case mnm_bit:
		case mnm_eor:
		case mnm_adc:
		case mnm_sbc:
		case mnm_lda:
		case mnm_ldx:
		case mnm_ldy:
		case mnm_cmp:
		case mnm_cpy:
		case mnm_cpx:
			return true;
	}
	return false;
}

static const char spacing[] = "                ";
void Disassemble(strref filename, unsigned char *mem, size_t bytes, bool acc_16, bool ind_16, int addr, bool ill_wdc, const dismnm *opcodes, bool src, int init_data, strref labels)
{
	const char *spc = src ? "" : spacing;

	strref prev_src;
	int start_addr = addr;
	int end_addr = addr + (int)bytes;

	refs.clear();
	GetReferences(mem, bytes, acc_16, ind_16, addr, ill_wdc, opcodes, init_data, labels);

	int curr_label_index = 0;
	bool separator = false;
	bool is_data = refs.size() ? (refs[0].data==DT_DATA || refs[0].data==DT_PTRS || refs[0].data==DT_PTRS_DATA) : false;
	bool is_ptrs = is_data && (refs[0].data==DT_PTRS || refs[0].data==DT_PTRS_DATA);
	strown<256> out;

	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f)
			return;
		opened = true;
	}

	while (bytes) {
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
						if (refs[lbl].label)
							out.sprintf("%s; Referenced from " STRREF_FMT " + $%x (%s, $%02x)\n", spc,
										STRREF_ARG(refs[lbl].label), ref_addr - prv_addr,
										aRefNames[(*ref.pRefs)[j].type], ref_addr);
						else {
							out.sprintf("%s; Referenced from", spc);
							if (refs[lbl].local) {
								int lbl_glb = lbl;
								while (lbl_glb>0 && refs[lbl_glb].local)
									lbl_glb--;
								if (refs[lbl].label)
									out.append(refs[lbl].label);
								else {
									RefAddr &ra = refs[lbl_glb];
									out.sprintf_append(" %s_%d /", ra.local ? ".l" : (ra.data==DT_CODE ? "Code" : (ra.address>=0 && ra.address<0x100 ? "zp" : "Data")), ra.number);
								}
							}
							RefAddr &ra = refs[lbl];
							out.sprintf_append(" %s_%d + $%x (%s, $%02x)\n",	ra.local ? ".l" : (ra.data==DT_CODE ? "Code" : (ra.address>=0 && ra.address<0x100 ? "zp" : "Data")),
										ra.number, ref_addr - prv_addr, aRefNames[(*ref.pRefs)[j].type], ra.address);
						}
					} else
						out.sprintf("%s; Referenced from $%04x (%s)\n", spc, (*ref.pRefs)[j].instr_addr,
									aRefNames[(*ref.pRefs)[j].type]);
					fputs(out.c_str(), f);
				}
			}
			if (addr>refs[curr_label_index].address) {
				if (ref.label)
					out.sprintf(STRREF_FMT " = $%02x\n", STRREF_ARG(ref.label), ref.address);
				else
					out.sprintf("%s_%d = $%02x\n", ref.local ? ".l" : (ref.data==DT_CODE ? "Code" : (ref.address>=0 && ref.address<0x100 ? "zp" : "Data")),
						ref.number, ref.address);
			} else {
				out.clear();
				if (ref.comment)
					out.sprintf_append("%s; " STRREF_FMT"\n", spc, STRREF_ARG(ref.comment));
				if (ref.label)
					out.sprintf_append("%s" STRREF_FMT ": ; $%04x\n", spc,
									   STRREF_ARG(ref.label), ref.address);
				else
					out.sprintf_append("%s%s_%d: ; $%04x\n", spc,
									   ref.local ? ".l" : (ref.data==DT_CODE ? "Code" : (ref.address>=0 && ref.address<0x100 ? "zp" : "Data")),
									   ref.number, ref.address);
			}
			fputs(out.c_str(), f);
			is_data = !!ref.data;
			is_ptrs = is_data && (ref.data==DT_PTRS || ref.data==DT_PTRS_DATA);
			separator = false;
			curr_label_index++;
		}
		if (src && (is_data || separator)) {
			out.clear();
			int left = end_addr - addr;
			if (curr_label_index<(int)refs.size() && refs[curr_label_index].address<end_addr)
				left = refs[curr_label_index].address - addr;
			is_data = true;
			if (is_ptrs) {
				int blk = refs[curr_label_index-1].size ? refs[curr_label_index-1].size : left;
				while (left>=2 && bytes>=2 && blk) {
					out.clear();
					out.copy("  dc.w ");
					int a = mem[0] + (((unsigned short)mem[1])<<8);
					int lbl = GetLabelIndex(a);
					if (lbl>=0) {
						if (refs[lbl].label) {
							out.append(refs[lbl].label);
							out.sprintf_append(" ; $%04x " STRREF_FMT "\n", a, STRREF_ARG(refs[lbl].comment));
						} else {
							RefAddr &ra = refs[lbl];
							out.sprintf_append("%s%s_%d: ; $%04x" STRREF_FMT "\n", spc,
								ra.local ? ".l" : (ra.data==DT_CODE ? "Code" : (ra.address>=0 && ra.address<0x100 ? "zp" : "Data")),
								ra.number, a, STRREF_ARG(ra.comment));
						}
					} else
						out.sprintf_append("$%04x\n", a);
					fputs(out.c_str(), f);
					mem += 2;
					addr += 2;
					bytes -= 2;
					left -= 2;
					blk -= 2;
				}
			}
			int addr_line = addr;
			out.copy("  dc.b ");
			for (int i = 0; i<left; i++) {
				if ((addr-addr_line)>=16) {
					out.sprintf_append(" ; $%04x-$%04x\n", addr_line, addr);
					addr_line = addr;
					fputs(out.c_str(), f);
					out.copy("  dc.b ");
				}
				out.sprintf_append("$%02x", *mem++);
				if ((i&0xf)!=0xf && i<(left-1))
					out.append(", ");
				addr++;
				bytes--;
			}
			if (addr!=addr_line) {
				out.sprintf_append(" ; $%04x-$%04x\n", addr_line, addr);
				fputs(out.c_str(), f);
			}
			separator = false;
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

			bool not_valid = opcodes[op].mnemonic==mnm_inv || (!ill_wdc && opcodes[op].mnemonic>=mnm_wdc_and_illegal_instructions);

			int arg_size = not_valid ? 0 : opcodes[op].arg_size;;
			int mode = not_valid ? AM_NON : opcodes[op].addrMode;
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
			if (op == 0x60 || op == 0x40 || op == 0x6b ||
				((op == 0x4c || op == 0x6c) && mem[arg_size] != 0x4c && mem[arg_size] != 0x6c) ||
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

			bool read_only_instruction = false;

			if (mode == AM_BRANCH)
				reference = addr + (char)*mem;
			else if (mode == AM_BRANCH_L)
				reference = addr + (short)(unsigned short)mem[0] + ((unsigned short)mem[1]<<8);
			else if (mode == AM_ABS || mode == AM_ABS_Y || mode == AM_ABS_X || mode == AM_REL || mode == AM_REL_X || mode == AM_REL_L) {
				read_only_instruction = IsReadOnlyInstruction(opcodes[op].mnemonic);
				reference = (int)mem[0] | ((int)mem[1])<<8;
			} else if (mode == AM_ZP || mode == AM_ZP_REL || mode == AM_ZP_REL_L || mode == AM_ZP_REL_X || mode == AM_ZP_Y_REL ||
					   mode == AM_ZP_REL_Y || mode == AM_ZP_X || mode == AM_ZP_Y || mode == AM_ZP_REL_Y_L) {
				read_only_instruction = IsReadOnlyInstruction(opcodes[op].mnemonic);
				reference = mem[0];
			}

			strown<64> lblname;
			strref lblcmt;
			for (size_t i = 0; i<refs.size(); ++i) {
				if (reference == refs[i].address || (reference>=refs[i].address && reference<(refs[i].address+refs[i].size)) ||
					(reference>=start_addr && reference<=end_addr && reference>=refs[i].address)) {
					if (i==(refs.size()-1) || reference<refs[i+1].address) {
						RefAddr &ra = refs[i];
						if (ra.label)
							lblname.copy((read_only_instruction && ra.read_only_label) ? ra.read_only_label : ra.label);
						else
							lblname.sprintf("%s_%d",
											ra.local ? ".l" : (ra.data==DT_CODE ? "Code" :
											(ra.address>=0 && ra.address<0x100 ? "zp" : "Data")), ra.number);
						lblcmt = ra.comment;
						if (reference > ra.address)
							lblname.sprintf_append(" + $%x", reference - ra.address);
						break;
					}
				}
			}

			if (not_valid)
				out.sprintf_append("dc.b $%02x ; $%04x ", op, curr_addr);

			const char *fmt = (src && lblname) ? aAddrModeFmtSrc[mode] : aAddrModeFmt[mode];
			const char *mnemonic = zsMNM[opcodes[op].mnemonic];
			switch (mode) {
				case AM_ABS:		// 3 $1234
				case AM_ABS_Y:		// 6 $1234,y
				case AM_ABS_X:		// 7 $1234,x
				case AM_REL:		// 8 ($1234)
				case AM_REL_X:		// c ($1234,x)
				case AM_REL_L:		// 14 [$1234]
					reference = (int)mem[0] | ((int)mem[1])<<8;
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname.c_str());
					else
						out.sprintf_append(fmt, mnemonic, (int)mem[0] | ((int)mem[1])<<8);
					break;

				case AM_ABS_L:		// 10 $bahilo
				case AM_ABS_L_X:	// 11 $123456,x
					reference = (int)mem[0] | ((int)mem[1])<<8 | ((int)mem[2])<<16;
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname);
					else
						out.sprintf_append(fmt, mnemonic, (int)mem[0] | ((int)mem[1])<<8 | ((int)mem[2])<<16);
					break;

				case AM_IMM_DBL_A:	// 18 #$12/#$1234
				case AM_IMM_DBL_I:	// 19 #$12/#$1234
					if (arg_size==2) {
						if (src && lblname)
							out.sprintf_append("%s #%s", mnemonic, lblname.c_str());
						else
							out.sprintf_append("%s #$%04x", mnemonic, (int)mem[0] | ((int)mem[1])<<8);
					} else {
						if (src && lblname)
							out.sprintf_append(fmt, mnemonic, lblname.c_str());
						else
							out.sprintf_append(fmt, mnemonic, mem[0]);
					}
					break;

				case AM_BRANCH:		// beq $1234
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname.c_str());
					else
						out.sprintf_append(fmt, mnemonic, addr + (char)mem[0]);
					break;

				case AM_BRANCH_L:	// brl $1234
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname.c_str());
					else
						out.sprintf_append(fmt, mnemonic, addr + ((short)(char)mem[0] + (((short)(char)mem[1])<<8)));
					break;

				case AM_ZP_ABS:		// d $12, *+$12
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname.c_str());
					else
						out.sprintf_append(fmt, mnemonic, mem[0], addr + (char)mem[1]);
					break;

				case AM_ZP:
				case AM_ZP_REL:
				case AM_ZP_REL_L:
				case AM_ZP_REL_X:
				case AM_ZP_REL_Y:
				case AM_ZP_X:
				case AM_ZP_Y:
				case AM_ZP_REL_Y_L:
				case AM_ZP_Y_REL:
					if (src && lblname)
						out.sprintf_append(fmt, mnemonic, lblname.c_str());
					else
						out.sprintf_append(fmt, mnemonic, mem[0], mem[1]);
					break;

				default:
					out.sprintf_append(fmt, mnemonic, mem[0], mem[1]);
					break;
			}
			if (!src && lblname)
				out.sprintf_append(" ; %s " STRREF_FMT, lblname.c_str(), STRREF_ARG(lblcmt));
			else if (src && lblname)
				out.sprintf_append(" ; $%04x " STRREF_FMT, reference, STRREF_ARG(lblcmt));

			mem += arg_size;
			out.append('\n');
			fputs(out.c_str(), f);
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

strref read_text(const char *filename)
{
	if (filename) {
		if (FILE *f = fopen(strown<512>(filename).c_str(), "rb")) {
			fseek(f, 0, SEEK_END);
			size_t size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (char *buf = (char*)malloc(size)) {
				fread(buf, size, 1, f);
				fclose(f);
				return strref(buf, (strl_t)size);
			}
		}
	}
	return  strref();
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
	bool ill_wdc = false;

	const dismnm *opcodes = a6502_ops;
	strref labels;

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
				} else if (var.same_str("labels")) {
					labels = read_text(arg.get());
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
					else if (arg.same_str("65C02WDC")) {
						opcodes = a65C02_ops;
						ill_wdc = true;
					} else if (arg.same_str("6502ILL")) {
						opcodes = a6502_ops;
						ill_wdc = true;
					}
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
				Disassemble(out, mem + skip, bytes, acc_16, ind_16, addr, ill_wdc, opcodes, src, data, labels);
			}
			free(mem);
		}
	} else {
		puts("Usage:\n"
			 "x65dsasm binary disasm.txt [$skip[-$end]] [addr=$xxxx] [cpu=6502/65C02/65816]\n"
			 "         [mx=0-3] [src] [labels=(labels.txt)]\n"
			 " * binary: file which contains some 65xx series instructions\n"
			 " * disasm.txt: output file (default is stdout)\n"
			 " * $skip-$end: first byte offset to disassemble to last byte offset to disassemble\n"
			 " * addr: disassemble as if loaded at addr\n"
			 " * prg: file is a c64 program file starting with the load address\n"
			 " * cpu: set which cpu to disassemble for (default is 6502)\n"
			 " * src: export near assemblable source with guesstimated data blocks\n"
			 " * mx: set the mx flags which control accumulator and index register size\n"
			 " * labels: define some labels with address and type (lbl=$addr,[code]/[data])\n");
	}
	if (labels)
		free(const_cast<char*>(labels.get()));
	return 0;
}