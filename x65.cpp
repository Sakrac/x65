//
//  x65.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/23/15.
//
//
//	A simple 6502 assembler
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
#include <vector>
#include <stdio.h>
#include <stdlib.h>

// if the number of resolved labels exceed this in one late eval then skip
//	checking for relevance and just eval all unresolved expressions.
#define MAX_LABELS_EVAL_ALL 16

// Max number of nested scopes (within { and })
#define MAX_SCOPE_DEPTH 32

// Max number of nested conditional expressions
#define MAX_CONDITIONAL_DEPTH 64

// The maximum complexity of expressions to be evaluated
#define MAX_EVAL_VALUES 32
#define MAX_EVAL_OPER 64

// Max capacity of each label pool
#define MAX_POOL_RANGES 4
#define MAX_POOL_BYTES 128

// Max number of exported binary files from a single source
#define MAX_EXPORT_FILES 64

// To simplify some syntax disambiguation the preferred
// ruleset can be specified on the command line.
enum AsmSyntax {
	SYNTAX_SANE,
	SYNTAX_MERLIN
};

// Internal status and error type
enum StatusCode {
	STATUS_OK,			// everything is fine
	STATUS_RELATIVE_SECTION, // value is relative to a single section
	STATUS_NOT_READY,	// label could not be evaluated at this time
	STATUS_NOT_STRUCT,	// return is not a struct.
	FIRST_ERROR,
	ERROR_UNDEFINED_CODE = FIRST_ERROR,
	ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION,
	ERROR_TOO_MANY_VALUES_IN_EXPRESSION,
	ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION,
	ERROR_UNBALANCED_RIGHT_PARENTHESIS,
	ERROR_EXPRESSION_OPERATION,
	ERROR_EXPRESSION_MISSING_VALUES,
	ERROR_INSTRUCTION_NOT_ZP,
	ERROR_INVALID_ADDRESSING_MODE,
	ERROR_BRANCH_OUT_OF_RANGE,
	ERROR_LABEL_MISPLACED_INTERNAL,
	ERROR_BAD_ADDRESSING_MODE,
	ERROR_UNEXPECTED_CHARACTER_IN_ADDRESSING_MODE,
	ERROR_UNEXPECTED_LABEL_ASSIGMENT_FORMAT,
	ERROR_MODIFYING_CONST_LABEL,
	ERROR_OUT_OF_LABELS_IN_POOL,
	ERROR_INTERNAL_LABEL_POOL_ERROR,
	ERROR_POOL_RANGE_EXPRESSION_EVAL,
	ERROR_LABEL_POOL_REDECLARATION,
	ERROR_POOL_LABEL_ALREADY_DEFINED,
	ERROR_STRUCT_ALREADY_DEFINED,
	ERROR_REFERENCED_STRUCT_NOT_FOUND,
	ERROR_BAD_TYPE_FOR_DECLARE_CONSTANT,
	ERROR_REPT_COUNT_EXPRESSION,
	ERROR_HEX_WITH_ODD_NIBBLE_COUNT,
	ERROR_DS_MUST_EVALUATE_IMMEDIATELY,
	ERROR_NOT_AN_X65_OBJECT_FILE,

	ERROR_STOP_PROCESSING_ON_HIGHER,	// errors greater than this will stop execution
	
	ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY,
	ERROR_TOO_DEEP_SCOPE,
	ERROR_UNBALANCED_SCOPE_CLOSURE,
	ERROR_BAD_MACRO_FORMAT,
	ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY,
	ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION,
	ERROR_CONDITION_COULD_NOT_BE_RESOLVED,
	ERROR_ENDIF_WITHOUT_CONDITION,
	ERROR_ELSE_WITHOUT_IF,
	ERROR_STRUCT_CANT_BE_ASSEMBLED,
	ERROR_ENUM_CANT_BE_ASSEMBLED,
	ERROR_UNTERMINATED_CONDITION,
	ERROR_REPT_MISSING_SCOPE,
	ERROR_LINKER_MUST_BE_IN_FIXED_ADDRESS_SECTION,
	ERROR_LINKER_CANT_LINK_TO_DUMMY_SECTION,
	ERROR_UNABLE_TO_PROCESS,
	ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE,
	ERROR_CPU_NOT_SUPPORTED,
	ERROR_CANT_APPEND_SECTION_TO_TARGET,

	STATUSCODE_COUNT
};

// The following strings are in the same order as StatusCode
const char *aStatusStrings[STATUSCODE_COUNT] = {
	"ok",
	"relative section",
	"not ready",
	"name is not a struct",
	"Undefined code",
	"Unexpected character in expression",
	"Too many values in expression",
	"Too many operators in expression",
	"Unbalanced right parenthesis in expression",
	"Expression operation",
	"Expression missing values",
	"Instruction can not be zero page",
	"Invalid addressing mode for instruction",
	"Branch out of range",
	"Internal label organization mishap",
	"Bad addressing mode",
	"Unexpected character in addressing mode",
	"Unexpected label assignment format",
	"Changing value of label that is constant",
	"Out of labels in pool",
	"Internal label pool release confusion",
	"Label pool range evaluation failed",
	"Label pool was redeclared within its scope",
	"Pool label already defined",
	"Struct already defined",
	"Referenced struct not found",
	"Declare constant type not recognized (dc.?)",
	"rept count expression could not be evaluated",
	"hex must be followed by an even number of hex numbers",
	"DS directive failed to evaluate immediately",
	"File is not a valid x65 object file",

	"Errors after this point will stop execution",

	"Target address must evaluate immediately for this operation",
	"Scoping is too deep",
	"Unbalanced scope closure",
	"Unexpected macro formatting",
	"Align must evaluate immediately",
	"Out of memory for macro expansion",
	"Conditional could not be resolved",
	"#endif encountered outside conditional block",
	"#else or #elif outside conditional block",
	"Struct can not be assembled as is",
	"Enum can not be assembled as is",
	"Conditional assembly (#if/#ifdef) was not terminated in file or macro",
	"rept is missing a scope ('{ ... }')",
	"Link can only be used in a fixed address section",
	"Link can not be used in dummy sections",
	"Can not process this line",
	"Unexpected target offset for reloc or late evaluation",
	"CPU is not supported",
	"Can't append sections",
};

// Assembler directives
enum AssemblerDirective {
	AD_CPU,			// CPU: Assemble for this target,
	AD_ORG,			// ORG: Assemble as if loaded at this address
	AD_EXPORT,		// EXPORT: export this section or disable export
	AD_LOAD,		// LOAD: If applicable, instruct to load at this address
	AD_SECTION,		// SECTION: Enable code that will be assigned a start address during a link step
	AD_LINK,		// LINK: Put sections with this name at this address (must be ORG / fixed address section)
	AD_XDEF,		// XDEF: Externally declare a label
	AD_INCOBJ,		// INCOBJ: Read in an object file saved from a previous build
	AD_ALIGN,		// ALIGN: Add to address to make it evenly divisible by this
	AD_MACRO,		// MACRO: Create a macro
	AD_EVAL,		// EVAL: Print expression to stdout during assemble
	AD_BYTES,		// BYTES: Add 8 bit values to output
	AD_WORDS,		// WORDS: Add 16 bit values to output
	AD_DC,			// DC.B/DC.W: Declare constant (same as BYTES/WORDS)
	AD_TEXT,		// TEXT: Add text to output
	AD_INCLUDE,		// INCLUDE: Load and assemble another file at this address
	AD_INCBIN,		// INCBIN: Load and directly insert another file at this address
	AD_CONST,		// CONST: Prevent a label from mutating during assemble
	AD_LABEL,		// LABEL: Create a mutable label (optional)
	AD_INCSYM,		// INCSYM: Reference labels from another assemble
	AD_LABPOOL,		// POOL: Create a pool of addresses to assign as labels dynamically
	AD_IF,			// #IF: Conditional assembly follows based on expression
	AD_IFDEF,		// #IFDEF: Conditional assembly follows based on label defined or not
	AD_ELSE,		// #ELSE: Otherwise assembly
	AD_ELIF,		// #ELIF: Otherwise conditional assembly follows
	AD_ENDIF,		// #ENDIF: End a block of #IF/#IFDEF
	AD_STRUCT,		// STRUCT: Declare a set of labels offset from a base address
	AD_ENUM,		// ENUM: Declare a set of incremental labels
	AD_REPT,		// REPT: Repeat the assembly of the bracketed code a number of times
	AD_INCDIR,		// INCDIR: Add a folder to search for include files
	AD_HEX,			// HEX: LISA assembler data block
	AD_EJECT,		// EJECT: Page break for printing assembler code, ignore
	AD_LST,			// LST: Controls symbol listing
	AD_DUMMY,		// DUM: Start a dummy section (increment address but don't write anything???)
	AD_DUMMY_END,	// DEND: End a dummy section
	AD_DS,			// DS: Define section, zero out # bytes or rewind the address if negative
	AD_USR,			// USR: MERLIN user defined pseudo op, runs some code at a hard coded address on apple II, on PC does nothing.
};

// Operators are either instructions or directives
enum OperationType {
	OT_NONE,
	OT_MNEMONIC,
	OT_DIRECTIVE
};

// These are expression tokens in order of precedence (last is highest precedence)
enum EvalOperator {
	EVOP_NONE,
	EVOP_VAL='a',	// a, value => read from value queue
	EVOP_LPR,		// b, left parenthesis
	EVOP_RPR,		// c, right parenthesis
	EVOP_ADD,		// d, +
	EVOP_SUB,		// e, -
	EVOP_MUL,		// f, * (note: if not preceded by value or right paren this is current PC)
	EVOP_DIV,		// g, /
	EVOP_AND,		// h, &
	EVOP_OR,		// i, |
	EVOP_EOR,		// j, ^
	EVOP_SHL,		// k, <<
	EVOP_SHR,		// l, >>
	EVOP_LOB,		// m, low byte of 16 bit value
	EVOP_HIB,		// n, high byte of 16 bit value
	EVOP_STP,		// o, Unexpected input, should stop and evaluate what we have
	EVOP_NRY,		// p, Not ready yet
	EVOP_ERR,		// q, Error
};

// Opcode encoding
typedef struct {
	unsigned int op_hash;
	unsigned char index;	// ground index
	unsigned char type;		// mnemonic or
} OP_ID;

enum AddrMode {
	AMB_ZP_REL_X,		// 0 ($12,x) address mode bit index
	AMB_ZP,				// 1 $12
	AMB_IMM,			// 2 #$12
	AMB_ABS,			// 3 $1234
	AMB_ZP_Y_REL,		// 4 ($12),y
	AMB_ZP_X,			// 5 $12,x
	AMB_ABS_Y,			// 6 $1234,y
	AMB_ABS_X,			// 7 $1234,x
	AMB_REL,			// 8 ($1234)
	AMB_ACC,			// 9 A
	AMB_NON,			// a
	AMB_ZP_REL,			// b ($12)
	AMB_REL_X,			// c ($1234,x)
	AMB_ZP_ABS,			// d $12, *+$12
	AMB_COUNT,

	AMB_FLIPXY = AMB_COUNT,	// e
	AMB_BRANCH,				// f
						// address mode masks
	AMM_NON = 1<<AMB_NON,
	AMM_IMM = 1<<AMB_IMM,
	AMM_ABS = 1<<AMB_ABS,
	AMM_REL = 1<<AMB_REL,
	AMM_ACC = 1<<AMB_ACC,
	AMM_ZP = 1<<AMB_ZP,
	AMM_ABS_X = 1<<AMB_ABS_X,
	AMM_ABS_Y = 1<<AMB_ABS_Y,
	AMM_ZP_X = 1<<AMB_ZP_X,
	AMM_ZP_REL_X = 1<<AMB_ZP_REL_X,
	AMM_ZP_Y_REL = 1<<AMB_ZP_Y_REL,
	AMM_FLIPXY = 1<<AMB_FLIPXY,
	AMM_BRANCH = 1<<AMB_BRANCH,

						// instruction group specific masks
	AMM_BRA = AMM_BRANCH | AMM_ABS,
	AMM_ORA = AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_Y | AMM_ABS_X | AMM_ZP_REL_X | AMM_ZP_Y_REL,
	AMM_STA = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_Y | AMM_ABS_X | AMM_ZP_REL_X | AMM_ZP_Y_REL,
	AMM_ASL = AMM_ACC | AMM_NON | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMM_STX = AMM_FLIPXY | AMM_ZP | AMM_ZP_X | AMM_ABS, // note: for x ,x/,y flipped for this instr.
	AMM_LDX = AMM_FLIPXY | AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X, // note: for x ,x/,y flipped for this instr.
	AMM_STY = AMM_ZP | AMM_ZP_X | AMM_ABS, // note: for x ,x/,y flipped for this instr.
	AMM_LDY = AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X, // note: for x ,x/,y flipped for this instr.
	AMM_DEC = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMM_BIT = AMM_ZP | AMM_ABS,
	AMM_JMP = AMM_ABS | AMM_REL,
	AMM_CPY = AMM_IMM | AMM_ZP | AMM_ABS,
};

struct mnem {
	const char *instr;
	unsigned int modes;
	unsigned char aCodes[AMB_COUNT];
};

struct mnem opcodes_6502[] = {
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty
	{ "brk", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jsr", AMM_ABS, { 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rti", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40 } },
	{ "rts", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60 } },
	{ "ora", AMM_ORA, { 0x01, 0x05, 0x09, 0x0d, 0x11, 0x15, 0x19, 0x1d, 0x00, 0x00, 0x00 } },
	{ "and", AMM_ORA, { 0x21, 0x25, 0x29, 0x2d, 0x31, 0x35, 0x39, 0x3d, 0x00, 0x00, 0x00 } },
	{ "eor", AMM_ORA, { 0x41, 0x45, 0x49, 0x4d, 0x51, 0x55, 0x59, 0x5d, 0x00, 0x00, 0x00 } },
	{ "adc", AMM_ORA, { 0x61, 0x65, 0x69, 0x6d, 0x71, 0x75, 0x79, 0x7d, 0x00, 0x00, 0x00 } },
	{ "sta", AMM_STA, { 0x81, 0x85, 0x00, 0x8d, 0x91, 0x95, 0x99, 0x9d, 0x00, 0x00, 0x00 } },
	{ "lda", AMM_ORA, { 0xa1, 0xa5, 0xa9, 0xad, 0xb1, 0xb5, 0xb9, 0xbd, 0x00, 0x00, 0x00 } },
	{ "cmp", AMM_ORA, { 0xc1, 0xc5, 0xc9, 0xcd, 0xd1, 0xd5, 0xd9, 0xdd, 0x00, 0x00, 0x00 } },
	{ "sbc", AMM_ORA, { 0xe1, 0xe5, 0xe9, 0xed, 0xf1, 0xf5, 0xf9, 0xfd, 0x00, 0x00, 0x00 } },
	{ "asl", AMM_ASL, { 0x00, 0x06, 0x00, 0x0e, 0x00, 0x16, 0x00, 0x1e, 0x00, 0x0a, 0x0a } },
	{ "rol", AMM_ASL, { 0x00, 0x26, 0x00, 0x2e, 0x00, 0x36, 0x00, 0x3e, 0x00, 0x2a, 0x2a } },
	{ "lsr", AMM_ASL, { 0x00, 0x46, 0x00, 0x4e, 0x00, 0x56, 0x00, 0x5e, 0x00, 0x4a, 0x4a } },
	{ "ror", AMM_ASL, { 0x00, 0x66, 0x00, 0x6e, 0x00, 0x76, 0x00, 0x7e, 0x00, 0x6a, 0x6a } },
	{ "stx", AMM_STX, { 0x00, 0x86, 0x00, 0x8e, 0x00, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldx", AMM_LDX, { 0x00, 0xa6, 0xa2, 0xae, 0x00, 0xb6, 0x00, 0xbe, 0x00, 0x00, 0x00 } },
	{ "dec", AMM_DEC, { 0x00, 0xc6, 0x00, 0xce, 0x00, 0xd6, 0x00, 0xde, 0x00, 0x00, 0x00 } },
	{ "inc", AMM_DEC, { 0x00, 0xe6, 0x00, 0xee, 0x00, 0xf6, 0x00, 0xfe, 0x00, 0x00, 0x00 } },
	{ "php", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08 } },
	{ "plp", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28 } },
	{ "pha", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48 } },
	{ "pla", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68 } },
	{ "dey", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88 } },
	{ "tay", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8 } },
	{ "iny", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8 } },
	{ "inx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8 } },
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty
	{ "bpl", AMM_BRA, { 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bmi", AMM_BRA, { 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvc", AMM_BRA, { 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvs", AMM_BRA, { 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcc", AMM_BRA, { 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcs", AMM_BRA, { 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bne", AMM_BRA, { 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "beq", AMM_BRA, { 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "clc", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18 } },
	{ "sec", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38 } },
	{ "cli", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58 } },
	{ "sei", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78 } },
	{ "tya", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98 } },
	{ "clv", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8 } },
	{ "cld", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8 } },
	{ "sed", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8 } },
	{ "bit", AMM_BIT, { 0x00, 0x24, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jmp", AMM_JMP, { 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00 } },
	{ "sty", AMM_STY, { 0x00, 0x84, 0x00, 0x8c, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldy", AMM_LDY, { 0x00, 0xa4, 0xa0, 0xac, 0x00, 0xb4, 0x00, 0xbc, 0x00, 0x00, 0x00 } },
	{ "cpy", AMM_CPY, { 0x00, 0xc4, 0xc0, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cpx", AMM_CPY, { 0x00, 0xe4, 0xe0, 0xec, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "txa", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a } },
	{ "txs", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a } },
	{ "tax", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa } },
	{ "tsx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba } },
	{ "dex", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca } },
	{ "nop", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea } }
};

static const int num_opcodes_6502 = sizeof(opcodes_6502) / sizeof(opcodes_6502[0]);

// 65C02
// http://6502.org/tutorials/65c02opcodes.html
// http://www.oxyron.de/html/opcodesc02.html

// 65816
// http://softpixel.com/~cwright/sianse/docs/65816NFO.HTM


// How instruction argument is encoded
enum CODE_ARG {
	CA_NONE,			// single byte instruction
	CA_ONE_BYTE,		// instruction carries one byte
	CA_TWO_BYTES,		// instruction carries two bytes
	CA_BRANCH			// instruction carries a relative address
};

// hardtexted strings
static const strref c_comment("//");
static const strref word_char_range("!0-9a-zA-Z_@$!#");
static const strref label_end_char_range("!0-9a-zA-Z_@$!.");
static const strref label_end_char_range_merlin("!0-9a-zA-Z_@$]:?");
static const strref filename_end_char_range("!0-9a-zA-Z_!@#$%&()/\\-.");
static const strref keyword_equ("equ");
static const strref str_label("label");
static const strref str_const("const");
static const strref struct_byte("byte");
static const strref struct_word("word");
static const char* aAddrModeFmt[] = {
					"%s ($%02x,x)",
					"%s $%02x",
					"%s #$%02x",
					"%s $%04x",
					"%s ($%02x),y",
					"%s $%02x,x",
					"%s $%04x,y",
					"%s $%04x,x",
					"%s ($%04x)",
					"%s A",
					"%s " };


// Binary search over an array of unsigned integers, may contain multiple instances of same key
unsigned int FindLabelIndex(unsigned int hash, unsigned int *table, unsigned int count)
{
	unsigned int max = count;
	unsigned int first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		unsigned int read = table[index];
		if (hash==read) {
			while (index && table[index-1]==hash)
				index--;	// guarantee first identical index returned on match
			return index;
		} else if (hash>read)
			first = index+1;
		else
			count = index;
	}
	if (count<max && table[count]<hash)
		count++;
	else if (count && table[count-1]>hash)
		count--;
	return count;
}



//
//
// ASSEMBLER STATE
//
//



// pairArray is basically two vectors sharing a size without constructors on growth or insert
template <class H, class V> class pairArray {
protected:
	H *keys;
	V *values;
	unsigned int _count;
	unsigned int _capacity;
public:
	pairArray() : keys(nullptr), values(nullptr), _count(0), _capacity(0) {}
	void reserve(unsigned int size) {
		if (size>_capacity) {
			H *new_keys = (H*)malloc(sizeof(H) * size); if (!new_keys) { return; }
			V *new_values = (V*)malloc(sizeof(V) * size); if (!new_values) { free(new_keys); return; }
			if (keys && values) {
				memcpy(new_keys, keys, sizeof(H) * _count);
				memcpy(new_values, values, sizeof(V) * _count);
				free(keys); free(values);
			}
			keys = new_keys;
			values = new_values;
			_capacity = size;
		}
	}
	bool insert(unsigned int pos) {
		if (pos>_count)
			return false;
		if (_count==_capacity)
			reserve(_capacity+64);
		if (pos<_count) {
			memmove(keys+pos+1, keys+pos, sizeof(H) * (_count-pos));
			memmove(values+pos+1, values+pos, sizeof(V) * (_count-pos));
		}
		memset(keys+pos, 0, sizeof(H));
		memset(values+pos, 0, sizeof(V));
		_count++;
		return true;
	}
	bool insert(unsigned int pos, H key) {
		if (insert(pos) && keys) {
			keys[pos] = key;
			return true;
		}
		return false;
	}
	void remove(unsigned int pos) {
		if (pos<_count) {
			_count--;
			if (pos<_count) {
				memmove(keys+pos, keys+pos+1, sizeof(H) * (_count-pos));
				memmove(values+pos, values+pos+1, sizeof(V) * (_count-pos));
			}
		}
	}
	H* getKeys() { return keys; }
	H& getKey(unsigned int pos) { return keys[pos]; }
	V* getValues() { return values; }
	V& getValue(unsigned int pos) { return values[pos];  }
	unsigned int count() const { return _count; }
	unsigned int capacity() const { return _capacity; }
	void clear() {
		if (keys!=nullptr)
			free(keys);
		keys = nullptr;
		if (values!=nullptr)
			free(values);
		values = nullptr;
		_capacity = 0;
		_count = 0;
	}
};

// relocs are cheaper than full expressions and work with
// local labels for relative sections which could otherwise
// be out of scope at link time.

struct Reloc {
	enum Type {
		NONE,
		WORD,
		LO_BYTE,
		HI_BYTE
	};
	int base_value;
	int section_offset;		// offset into this section
	int target_section;		// which section does this reloc target?
	Type value_type;		// byte or word size

	Reloc() : base_value(0), section_offset(-1), target_section(-1), value_type(NONE) {}
	Reloc(int base, int offs, int sect, Type t = WORD) :
		base_value(base), section_offset(offs), target_section(sect), value_type(t) {}
};
typedef std::vector<struct Reloc> relocList;

// For assembly listing this remembers the location of each line
struct ListLine {
	strref source_name;		// source file index name
	strref code;			// line of code this represents
	int address;			// start address of this line
	int size;				// number of bytes generated for this line
	int line_offs;			// offset into code
	bool was_mnemonic;		// only output code if generated by code
};
typedef std::vector<struct ListLine> Listing;

// start of data section support
// Default is a fixed address section at $1000
// Whenever org or dum with address is encountered => new section
// If org is fixed and < $200 then it is a dummy section Otherwise clear dummy section
typedef struct Section {
	// section name, same named section => append
	strref name;			// name of section for comparison
	strref export_append;	// append this name to export of file

	// generated address status
	int load_address;		// if assigned a load address
	int start_address;
	int address;			// relative or absolute PC
	int align_address;		// for relative sections that needs alignment

	// merged sections
	int merged_offset;		// -1 if not merged
	int merged_section;		// which section merged with

	// data output
	unsigned char *output;	// memory for this section
	unsigned char *curr;	// current pointer for this section
	size_t output_capacity;	// current output capacity

	// reloc data
	relocList *pRelocs;		// link time resolve (not all sections need this)
	Listing *pListing;		// if list output

	bool address_assigned;	// address is absolute if assigned
	bool dummySection;		// true if section does not generate data, only labels

	void reset() {			// explicitly cleaning up sections, not called from Section destructor
		name.clear(); export_append.clear();
		start_address = address = load_address = 0x0;
		address_assigned = false; output = nullptr; curr = nullptr;
		dummySection = false; output_capacity = 0; merged_offset = -1;
		align_address = 1; if (pRelocs) delete pRelocs;
		pRelocs = nullptr;
		if (pListing) delete pListing;
		pListing = nullptr;
	}

	void Cleanup() { if (output) free(output); reset(); }
	bool empty() const { return merged_offset<0 && curr==output; }

	int DataOffset() const { return int(curr - output); }
	size_t size() const { return curr - output; }
	const unsigned char *get() { return output; }

	int GetPC() const { return address; }
	void AddAddress(int value) { address += value; }
	void SetLoadAddress(int addr) { load_address = addr; }
	int GetLoadAddress() const { return load_address; }

	void SetDummySection(bool enable) { dummySection = enable; }
	bool IsDummySection() const { return dummySection;  }
	bool IsRelativeSection() const { return address_assigned == false; }
	bool IsMergedSection() const { return merged_offset >= 0;  }
	void AddReloc(int base, int offset, int section, Reloc::Type type = Reloc::WORD);

	Section() : pRelocs(nullptr), pListing(nullptr) { reset(); }
	Section(strref _name, int _address) : pRelocs(nullptr), pListing(nullptr)
		{ reset(); name = _name; start_address = load_address = address = _address;
		  address_assigned = true; }
	Section(strref _name) : pRelocs(nullptr), pListing(nullptr) { reset(); name = _name;
		start_address = load_address = address = 0; address_assigned = false; }
	~Section() { }

	// Appending data to a section
	void CheckOutputCapacity(unsigned int addSize);
	void AddByte(int b);
	void AddWord(int w);
	void AddBin(unsigned const char *p, int size);
	void SetByte(size_t offs, int b) { output[offs] = b; }
	void SetWord(size_t offs, int w) { output[offs] = w; output[offs+1] = w>>8; }
} Section;

// Symbol list entry (in order of parsing)
struct MapSymbol {
    strref name;    // string name
    short value;
	short section;
    bool local;     // local variables
};
typedef std::vector<struct MapSymbol> MapSymbolArray;

// Data related to a label
typedef struct {
public:
	strref label_name;		// the name of this label
	strref pool_name;		// name of the pool that this label is related to
	int value;
	int section;			// rel section address labels belong to a section, -1 if fixed address or assigned
    int mapIndex;           // index into map symbols in case of late resolve
	bool evaluated;			// a value may not yet be evaluated
	bool pc_relative;		// this is an inline label describing a point in the code
	bool constant;			// the value of this label can not change
	bool external;			// this label is globally accessible
} Label;

// If an expression can't be evaluated immediately, this is required
// to reconstruct the result when it can be.
typedef struct {
	enum Type {				// When an expression is evaluated late, determine how to encode the result
		LET_LABEL,			// this evaluation applies to a label and not memory
		LET_ABS_REF,		// calculate an absolute address and store at 0, +1
		LET_BRANCH,			// calculate a branch offset and store at this address
		LET_BYTE,			// calculate a byte and store at this address
	};
	int target;				// offset into output buffer
	int address;			// current pc
	int scope;				// scope pc
	int section;			// which section to apply to.
	int file_ref;			// -1 if current or xdef'd otherwise index of file for label
	strref label;			// valid if this is not a target but another label
	strref expression;
	strref source_file;
	Type type;
} LateEval;

// A macro is a text reference to where it was defined
typedef struct {
	strref name;
	strref macro;
	strref source_name;		// source file name (error output)
	strref source_file;		// entire source file (req. for line #)
} Macro;

// All local labels are removed when a global label is defined but some when a scope ends
typedef struct {
	strref label;
	int scope_depth;
	bool scope_reserve;		// not released for global label, only scope	
} LocalLabelRecord;

// Label pools allows C like stack frame label allocation
typedef struct {
	strref pool_name;
	short numRanges;		// normally 1 range, support multiple for ease of use
	short scopeDepth;		// Required for scope closure cleanup
	unsigned short ranges[MAX_POOL_RANGES*2];		// 2 shorts per range
	unsigned int usedMap[(MAX_POOL_BYTES+15)>>4];	// 2 bits per byte to store byte count of label
	StatusCode Reserve(int numBytes, unsigned int &addr);
	StatusCode Release(unsigned int addr);
} LabelPool;

// One member of a label struct
struct MemberOffset {
	unsigned short offset;
	unsigned int name_hash;
	strref name;
	strref sub_struct;
};

// Label struct
typedef struct {
	strref name;
	unsigned short first_member;
	unsigned short numMembers;
	unsigned short size;
} LabelStruct;

// object file labels that are not xdef'd end up here
struct ExtLabels {
	pairArray<unsigned int, Label> labels;
};

struct EvalContext {
	int pc;					// current address at point of eval
	int scope_pc;			// current scope open at point of eval
	int scope_end_pc;		// late scope closure after eval
	int relative_section;	// return can be relative to this section
	int file_ref;			// can access private label from this file or -1
	EvalContext(int _pc, int _scope, int _close, int _sect) :
		pc(_pc), scope_pc(_scope), scope_end_pc(_close), relative_section(_sect),
		file_ref(-1) {}
};

// Source context is current file (include file, etc.) or current macro.
typedef struct {
	strref source_name;		// source file name (error output)
	strref source_file;		// entire source file (req. for line #)
	strref code_segment;	// the segment of the file for this context
	strref read_source;		// current position/length in source file
	strref next_source;		// next position/length in source file
	int repeat;				// how many times to repeat this code segment
	void restart() { read_source = code_segment; }
	bool complete() { repeat--; return repeat <= 0; }
} SourceContext;

// Context stack is a stack of currently processing text
class ContextStack {
private:
	std::vector<SourceContext> stack;
	SourceContext *currContext;
public:
	ContextStack() : currContext(nullptr) { stack.reserve(32); }
	SourceContext& curr() { return *currContext; }
	void push(strref src_name, strref src_file, strref code_seg, int rept=1) {
		if (currContext)
			currContext->read_source = currContext->next_source;
		SourceContext context;
		context.source_name = src_name;
		context.source_file = src_file;
		context.code_segment = code_seg;
		context.read_source = code_seg;
		context.next_source = code_seg;
		context.repeat = rept;
		stack.push_back(context);
		currContext = &stack[stack.size()-1];
	}
	void pop() { stack.pop_back(); currContext = stack.size() ? &stack[stack.size()-1] : nullptr; }
	bool has_work() { return currContext!=nullptr; }
};

// The state of the assembler
class Asm {
public:
	pairArray<unsigned int, Label> labels;
	pairArray<unsigned int, Macro> macros;
	pairArray<unsigned int, LabelPool> labelPools;
	pairArray<unsigned int, LabelStruct> labelStructs;
	// labels matching xdef names will be marked as external
	pairArray<unsigned int, strref> xdefs;

	std::vector<LateEval> lateEval;
	std::vector<LocalLabelRecord> localLabels;
	std::vector<char*> loadedData;	// free when assembler is completed
	std::vector<MemberOffset> structMembers; // labelStructs refer to sets of structMembers
	std::vector<strref> includePaths;
	std::vector<Section> allSections;
	std::vector<ExtLabels> externals; // external labels organized by object file
    MapSymbolArray map;
	
	// context for macros / include files
	ContextStack contextStack;
	
	// Current section (temp private so I don't access this directly and forget about it)
private:
	Section *current_section;
public:

	// Special syntax rules
	AsmSyntax syntax;

	// Conditional assembly vars
	int conditional_depth;
	strref conditional_source[MAX_CONDITIONAL_DEPTH];	// start of conditional for error fixing
	char conditional_nesting[MAX_CONDITIONAL_DEPTH];
	bool conditional_consumed[MAX_CONDITIONAL_DEPTH];

	// Scope info
	int scope_address[MAX_SCOPE_DEPTH];
	int scope_depth;

	// Eval relative result (only valid if EvalExpression returs STATUS_RELATIVE_SECTION)
	int lastEvalSection;
	int lastEvalValue;
	Reloc::Type lastEvalPart;

	bool last_label_local;
	bool errorEncountered;
	bool list_assembly;

	// Convert source to binary
	void Assemble(strref source, strref filename, bool obj_target);

	// Generate assembler listing if requested
	bool List(strref filename);

	// Generate source for all valid instructions
	bool AllOpcodes(strref filename);

	// Clean up memory allocations, reset assembler state
	void Cleanup();
	
	// Make sure there is room to write more code
	void CheckOutputCapacity(unsigned int addSize);

	// Operations on current section
	void SetSection(strref name, int address);	// fixed address section
	void SetSection(strref name); // relative address section
	StatusCode AppendSection(Section &relSection, Section &trgSection);
	StatusCode LinkSections(strref name); // link relative address sections with this name here
	void LinkLabelsToAddress(int section_id, int section_address);
	StatusCode LinkRelocs(int section_id, int section_address);
	void DummySection(int address);
	void DummySection();
	void EndSection();
	Section& CurrSection() { return *current_section; }
	unsigned char* BuildExport(strref append, int &file_size, int &addr);
	int GetExportNames(strref *aNames, int maxNames);
	int SectionId() { return int(current_section - &allSections[0]); }
	void AddByte(int b) { CurrSection().AddByte(b); }
	void AddWord(int w) { CurrSection().AddWord(w); }
	void AddBin(unsigned const char *p, int size) { CurrSection().AddBin(p, size); }

	// Object file handling
	StatusCode WriteObjectFile(strref filename);
	StatusCode ReadObjectFile(strref filename);

	// Macro management
	StatusCode AddMacro(strref macro, strref source_name, strref source_file, strref &left);
	StatusCode BuildMacro(Macro &m, strref arg_list);

	// Structs
	StatusCode BuildStruct(strref name, strref declaration);
	StatusCode EvalStruct(strref name, int &value);
	StatusCode BuildEnum(strref name, strref declaration);

	// Calculate a value based on an expression.
	EvalOperator RPNToken_Merlin(strref &expression, const struct EvalContext &etx,
		EvalOperator prev_op, short &section, int &value);
	EvalOperator RPNToken(strref &expression, const struct EvalContext &etx,
		EvalOperator prev_op, short &section, int &value);
	StatusCode EvalExpression(strref expression, const struct EvalContext &etx, int &result);

	// Access labels
	Label* GetLabel(strref label);
	Label* GetLabel(strref label, int file_ref);
	Label* AddLabel(unsigned int hash);
	bool MatchXDEF(strref label);
	StatusCode AssignLabel(strref label, strref line, bool make_constant = false);
	StatusCode AddressLabel(strref label);
	void LabelAdded(Label *pLabel, bool local=false);
	void IncludeSymbols(strref line);

	// Manage locals
	void MarkLabelLocal(strref label, bool scope_label = false);
	void FlushLocalLabels(int scope_exit = -1);
	
	// Label pools
	LabelPool* GetLabelPool(strref pool_name);
	StatusCode AddLabelPool(strref name, strref args);
	StatusCode AssignPoolLabel(LabelPool &pool, strref args);
	void FlushLabelPools(int scope_exit);

	// Late expression evaluation
	void AddLateEval(int target, int pc, int scope_pc, strref expression,
					 strref source_file, LateEval::Type type);
	void AddLateEval(strref label, int pc, int scope_pc,
					 strref expression, LateEval::Type type);
	StatusCode CheckLateEval(strref added_label=strref(), int scope_end = -1);

	// Assembler steps
	StatusCode ApplyDirective(AssemblerDirective dir, strref line, strref source_file);
	AddrMode GetAddressMode(strref line, bool flipXY,
								  StatusCode &error, strref &expression);
	StatusCode AddOpcode(strref line, int index, strref source_file);
	StatusCode BuildLine(OP_ID *pInstr, int numInstructions, strref line);
	StatusCode BuildSegment(OP_ID *pInstr, int numInstructions);
	
	// Display error in stderr
	void PrintError(strref line, StatusCode error);

	// Conditional Status
	bool ConditionalAsm();			// Assembly is currently enabled
	bool NewConditional();			// Start a new conditional block
	void CloseConditional();		// Close a conditional block
	void CheckConditionalDepth();	// Check if this conditional will nest the assembly (a conditional is already consumed)
	void ConsumeConditional();		// This conditional block is going to be assembled, mark it as consumed
	bool ConditionalConsumed();		// Has a block of this conditional already been assembled?
	void SetConditional();			// This conditional block is not going to be assembled so mark that it is nesting
	bool ConditionalAvail();		// Returns true if this conditional can be consumed
	void ConditionalElse();	// Conditional else that does not enable block
	void EnableConditional(bool enable); // This conditional block is enabled and the prior wasn't

	// Conditional statement evaluation (A==B? A?)
	StatusCode EvalStatement(strref line, bool &result);
	
	// Add include folder
	void AddIncludeFolder(strref path);
	char* LoadText(strref filename, size_t &size);
	char* LoadBinary(strref filename, size_t &size);

	// constructor
	Asm() { Cleanup(); localLabels.reserve(256); loadedData.reserve(16); lateEval.reserve(64); }
};

// Clean up work allocations
void Asm::Cleanup() {
	for (std::vector<char*>::iterator i = loadedData.begin(); i != loadedData.end(); ++i) {
		if (char *data = *i)
			free(data);
	}
    map.clear();
	labelPools.clear();
	loadedData.clear();
	labels.clear();
	macros.clear();
	allSections.clear();
	for (std::vector<ExtLabels>::iterator exti = externals.begin(); exti !=externals.end(); ++exti)
		exti->labels.clear();
	externals.clear();
	SetSection(strref("default"));		// this section is relocatable but is assigned address $1000 if exporting without directives
	current_section = &allSections[0];
	syntax = SYNTAX_SANE;
	scope_depth = 0;
	conditional_depth = 0;
	conditional_nesting[0] = 0;
	conditional_consumed[0] = false;
	last_label_local = false;
	errorEncountered = false;
	list_assembly = false;
}

// Read in text data (main source, include, etc.)
char* Asm::LoadText(strref filename, size_t &size) {
	strown<512> file(filename);
	std::vector<strref>::iterator i = includePaths.begin();
	for(;;) {
		if (FILE *f = fopen(file.c_str(), "rb")) {
			fseek(f, 0, SEEK_END);
			size_t _size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (char *buf = (char*)calloc(_size, 1)) {
				fread(buf, _size, 1, f);
				fclose(f);
				size = _size;
				return buf;
			}
			fclose(f);
		}
		if (i==includePaths.end())
			break;
		file.copy(*i);
		if (file.get_last()!='/' && file.get_last()!='\\')
			file.append('/');
		file.append(filename);
		++i;
	}
	size = 0;
	return nullptr;
}

// Read in binary data (incbin)
char* Asm::LoadBinary(strref filename, size_t &size) {
	strown<512> file(filename);
	std::vector<strref>::iterator i = includePaths.begin();
	for(;;) {
		if (FILE *f = fopen(file.c_str(), "rb")) {
			fseek(f, 0, SEEK_END);
			size_t _size = ftell(f);
			fseek(f, 0, SEEK_SET);
			if (char *buf = (char*)malloc(_size)) {
				fread(buf, _size, 1, f);
				fclose(f);
				size = _size;
				return buf;
			}
			fclose(f);
		}
		if (i==includePaths.end())
			break;
		file.copy(*i);
		if (file.get_last()!='/' && file.get_last()!='\\')
			file.append('/');
		file.append(filename);
#ifdef WIN32
		file.replace('/', '\\');
#endif
		++i;
	}
	size = 0;
	return nullptr;
}

void Asm::SetSection(strref name, int address)
{
	if (name) {
		for (std::vector<Section>::iterator i = allSections.begin(); i!=allSections.end(); ++i) {
			if (i->name && name.same_str(i->name)) {
				current_section = &*i;
				return;
			}
		}
	}
	if (allSections.size()==allSections.capacity())
		allSections.reserve(allSections.size() + 16);
	Section newSection(name, address);
	if (address < 0x200)	// don't compile over zero page and stack frame (may be bad assumption)
		newSection.SetDummySection(true);
	allSections.push_back(newSection);
	current_section = &allSections[allSections.size()-1];
}

void Asm::SetSection(strref name)
{
// should same name section within the same file be the same section?
/*	if (name) {
		for (std::vector<Section>::iterator i = allSections.begin(); i!=allSections.end(); ++i) {
			if (i->name && name.same_str(i->name)) {
				current_section = &*i;
				return;
			}
		}
	}*/
	if (allSections.size()==allSections.capacity())
		allSections.reserve(allSections.size() + 16);
	int align = 1;
	strref align_str = name.after(',');
	align_str.trim_whitespace();
	if (align_str.get_first()=='$') {
		++align_str;
		align = align_str.ahextoui();
	} else
		align = align_str.atoi();
	Section newSection(name.before_or_full(','));
	newSection.align_address = align;
	allSections.push_back(newSection);
	current_section = &allSections[allSections.size()-1];
}

// Fixed address dummy section
void Asm::DummySection(int address) {
	if (CurrSection().empty() && address == CurrSection().GetPC()) {
		CurrSection().SetDummySection(true);
		return;
	}

	if (allSections.size()==allSections.capacity())
		allSections.reserve(allSections.size() + 16);
	Section newSection(strref(), address);
	newSection.SetDummySection(true);
	allSections.push_back(newSection);
	current_section = &allSections[allSections.size()-1];
}

// Current address dummy section
void Asm::DummySection() {
	DummySection(CurrSection().GetPC());
}

void Asm::EndSection() {
	int section = (int)(current_section - &allSections[0]);
	if (section)
		current_section = &allSections[section-1];
}

// list all export append names
// for each valid export append name build a binary fixed address code
//	- find lowest and highest address
//	- alloc & 0 memory
//	- any matching relative sections gets linked in after
//	- go through all section that matches export_append in order and copy over memory
unsigned char* Asm::BuildExport(strref append, int &file_size, int &addr)
{
	int start_address = 0x7fffffff;
	int end_address = 0;

	bool has_relative_section = false;
	bool has_fixed_section = false;
	int last_fixed_section = -1;

	// find address range
	while (!has_relative_section && !has_fixed_section) {
		int section_id = 0;
		for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
			if ((!append && !i->export_append) || append.same_str_case(i->export_append)) {
				if (!i->IsMergedSection()) {
					if (i->IsRelativeSection())
						has_relative_section = true;
					else if (i->start_address >= 0x200 && i->size() > 0) {
						has_fixed_section = true;
						if (i->start_address < start_address)
							start_address = i->start_address;
						if ((i->start_address + (int)i->size()) > end_address) {
							end_address = i->start_address + (int)i->size();
							last_fixed_section = section_id;
						}
					}
				}
			}
			section_id++;
		}
		if (has_relative_section) {
			if (!has_fixed_section) {
				SetSection(strref(), 0x1000);
				CurrSection().export_append = append;
				last_fixed_section = SectionId();
			}
			for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
				if ((!append && !i->export_append) || append.same_str_case(i->export_append)) {
					if (i->IsRelativeSection()) {
						StatusCode status = AppendSection(*i, allSections[last_fixed_section]);
						if (status != STATUS_OK)
							return nullptr;
						end_address = allSections[last_fixed_section].start_address +
							(int)allSections[last_fixed_section].size();
					}
				}
			}
		}
	}

	// check if valid
	if (end_address <= start_address)
		return nullptr;

	// get memory for output buffer
	unsigned char *output = (unsigned char*)calloc(1, end_address - start_address);

	// copy over in order
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if ((!append && !i->export_append) || append.same_str_case(i->export_append)) {
			if (i->merged_offset == -1 && i->start_address >= 0x200 && i->size() > 0)
				memcpy(output + i->start_address - start_address, i->output, i->size());
		}
	}
	
	// return the result
	file_size = end_address - start_address;
	addr = start_address;
	return output;
}

// Collect all the export names
int Asm::GetExportNames(strref *aNames, int maxNames)
{
	int count = 0;
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if (!i->IsMergedSection()) {
			bool found = false;
			unsigned int hash = i->export_append.fnv1a_lower();
			for (int n = 0; n < count; n++) {
				if (aNames[n].fnv1a_lower() == hash) {
					found = true;
					break;
				}
			}
			if (!found && count < maxNames)
				aNames[count++] = i->export_append;
		}
	}
	return count;
}

// Apply labels assigned to addresses in a relative section a fixed address
void Asm::LinkLabelsToAddress(int section_id, int section_address)
{
	Label *pLabels = labels.getValues();
	int numLabels = labels.count();
	for (int l = 0; l < numLabels; l++) {
		if (pLabels->section == section_id) {
			pLabels->value += section_address;
			pLabels->section = -1;
			if (pLabels->mapIndex>=0 && pLabels->mapIndex<(int)map.size()) {
				struct MapSymbol &msym = map[pLabels->mapIndex];
				msym.value = pLabels->value;
				msym.section = -1;
			}
			CheckLateEval(pLabels->label_name);
		}
		++pLabels;
	}
}

// go through relocs in all sections to see if any targets this section
// relocate section to address!
StatusCode Asm::LinkRelocs(int section_id, int section_address)
{
	for (std::vector<Section>::iterator j = allSections.begin(); j != allSections.end(); ++j) {
		Section &s2 = *j;
		if (s2.pRelocs) {
			relocList *pList = s2.pRelocs;
			relocList::iterator i = pList->end();
			while (i != pList->begin()) {
				--i;
				if (i->target_section == section_id) {
					Section *trg_sect = &s2;
					size_t output_offs = 0;
					while (trg_sect->merged_offset>=0) {
						output_offs += trg_sect->merged_offset;
						trg_sect = &allSections[trg_sect->merged_section];
					}
					unsigned char *trg = trg_sect->output + output_offs + i->section_offset;
					int value = i->base_value + section_address;
					if (i->value_type == Reloc::WORD) {
						if ((trg+1) >= (trg_sect->output + trg_sect->size()))
							return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
						*trg++ = (unsigned char)value;
						*trg = (unsigned char)(value >> 8);
					} else if (i->value_type == Reloc::LO_BYTE) {
						if (trg >= (trg_sect->output + trg_sect->size()))
							return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
						*trg = (unsigned char)value;
					} else if (i->value_type == Reloc::HI_BYTE) {
						if (trg >= (trg_sect->output + trg_sect->size()))
							return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
						*trg = (unsigned char)(value >> 8);
					}
					i = pList->erase(i);
					if (i != pList->end())
						++i;
				}
			}
			if (pList->empty()) {
				free(pList);
				s2.pRelocs = nullptr;
			}
		}
	}
	return STATUS_OK;
}

// Append one section to the end of another
StatusCode Asm::AppendSection(Section &s, Section &curr)
{
	int section_id = int(&s - &allSections[0]);
	if (s.IsRelativeSection() && !s.IsMergedSection()) {
		int section_size = (int)s.size();

		int section_address = curr.GetPC();
		curr.CheckOutputCapacity((int)s.size());
		unsigned char *section_out = curr.curr;
		if (s.output)
			memcpy(section_out, s.output, s.size());
		curr.address += (int)s.size();
		curr.curr += s.size();
		free(s.output);
		s.output = 0;
		s.curr = 0;
		s.output_capacity = 0;

		// calculate space for alignment
		int align_size = s.align_address <= 1 ? 0 :
			((section_address + s.align_address - 1) % s.align_address);

		// Get base addresses
		curr.CheckOutputCapacity(section_size + align_size);

		// add 0's
		for (int a = 0; a<align_size; a++)
			curr.AddByte(0);

		section_address += align_size;

		// Update address range and mark section as merged
		s.start_address = section_address;
		s.address += section_address;
		s.address_assigned = true;
		s.merged_section = SectionId();
		s.merged_offset = (int)(section_out - CurrSection().output);

		// Merge in the listing at this point
		if (s.pListing) {
			if (!curr.pListing)
				curr.pListing = new Listing;
			if ((curr.pListing->size() + s.pListing->size()) > curr.pListing->capacity())
				curr.pListing->reserve(curr.pListing->size() + s.pListing->size() + 256);
			for (Listing::iterator si = s.pListing->begin(); si != s.pListing->end(); ++si) {
				struct ListLine lst = *si;
				lst.address += s.merged_offset;
				curr.pListing->push_back(lst);
			}
			delete s.pListing;
			s.pListing = nullptr;
		}


		// All labels in this section can now be assigned
		LinkLabelsToAddress(section_id, section_address);

		return LinkRelocs(section_id, section_address);
	}
	return ERROR_CANT_APPEND_SECTION_TO_TARGET;
}

// Link sections with a specific name at this point
StatusCode Asm::LinkSections(strref name) {
	if (CurrSection().IsRelativeSection())
		return ERROR_LINKER_MUST_BE_IN_FIXED_ADDRESS_SECTION;
	if (CurrSection().IsDummySection())
		return ERROR_LINKER_CANT_LINK_TO_DUMMY_SECTION;

	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if ((!name || i->name.same_str_case(name)) && i->IsRelativeSection() && !i->IsMergedSection()) {
			StatusCode status = AppendSection(*i, CurrSection());
			if (status != STATUS_OK)
				return status;
		}
	}
	return STATUS_OK;
}

// Section based output capacity
// Make sure there is room to assemble in
void Section::CheckOutputCapacity(unsigned int addSize) {
	size_t currSize = curr - output;
	if ((addSize + currSize) >= output_capacity) {
		size_t newSize = currSize * 2;
		if (newSize < 64*1024)
			newSize = 64*1024;
		if ((addSize+currSize) > newSize)
			newSize += newSize;
		unsigned char *new_output = (unsigned char*)malloc(newSize);
		curr = new_output + (curr-output);
		free(output);
		output = new_output;
		output_capacity = newSize;
	}
}

// Add one byte to a section
void Section::AddByte(int b) {
	if (!dummySection) {
		CheckOutputCapacity(1);
		*curr++ = (unsigned char)b;
	}
	address++;
}

// Add a 16 bit word to a section
void Section::AddWord(int w) {
	if (!dummySection) {
		CheckOutputCapacity(2);
		*curr++ = (unsigned char)(w&0xff);
		*curr++ = (unsigned char)(w>>8);
	}
	address += 2;
}

// Add arbitrary length data to a section
void Section::AddBin(unsigned const char *p, int size) {
	if (!dummySection) {
		CheckOutputCapacity(size);
		memcpy(curr, p, size);
		curr += size;
	}
	address += size;
}

// Add a relocation marker to a section
void Section::AddReloc(int base, int offset, int section, Reloc::Type type)
{
	if (!pRelocs)
		pRelocs = new relocList;
	if (pRelocs->size() == pRelocs->capacity())
		pRelocs->reserve(pRelocs->size() + 32);
	pRelocs->push_back(Reloc(base, offset, section, type));
}

// Make sure there is room to assemble in
void Asm::CheckOutputCapacity(unsigned int addSize) {
	CurrSection().CheckOutputCapacity(addSize);
}






//
//
// MACROS
//
//



// add a custom macro
StatusCode Asm::AddMacro(strref macro, strref source_name, strref source_file, strref &left)
{
	// name(optional params) { actual macro }
	strref name = macro.split_label();
	macro.skip_whitespace();
	if (macro[0]!='(' && macro[0]!='{')
		return ERROR_BAD_MACRO_FORMAT;
	unsigned int hash = name.fnv1a();
	unsigned int ins = FindLabelIndex(hash, macros.getKeys(), macros.count());
	Macro *pMacro = nullptr;
	while (ins < macros.count() && macros.getKey(ins)==hash) {
		if (name.same_str_case(macros.getValue(ins).name)) {
			pMacro = macros.getValues() + ins;
			break;
		}
		++ins;
	}
	if (!pMacro) {
		macros.insert(ins, hash);
		pMacro = macros.getValues() + ins;
	}
	pMacro->name = name;
	int pos_bracket = macro.find('{');
	if (pos_bracket < 0) {
		pMacro->macro = strref();
		return ERROR_BAD_MACRO_FORMAT;
	}
	strref source = macro + pos_bracket;
	strref macro_body = source.scoped_block_skip();
	pMacro->macro = strref(macro.get(), pos_bracket + macro_body.get_len() + 2);
	pMacro->source_name = source_name;
	pMacro->source_file = source_file;
	source.skip_whitespace();
	left = source;
	return STATUS_OK;
}

// Compile in a macro
StatusCode Asm::BuildMacro(Macro &m, strref arg_list)
{
	strref macro_src = m.macro;
	strref params = macro_src[0]=='(' ? macro_src.scoped_block_skip() : strref();
	params.trim_whitespace();
	arg_list.trim_whitespace();
	macro_src.skip_whitespace();
	if (params) {
		arg_list = arg_list.scoped_block_skip();
		strref pchk = params;
		strref arg = arg_list;
		int dSize = 0;
		while (strref param = pchk.split_token_trim(',')) {
			strref a = arg.split_token_trim(',');
			if (param.get_len() < a.get_len()) {
				int count = macro_src.substr_case_count(param);
				dSize += count * ((int)a.get_len() - (int)param.get_len());
			}
		}
		int mac_size = macro_src.get_len() + dSize + 32;
		if (char *buffer = (char*)malloc(mac_size)) {
			loadedData.push_back(buffer);
			strovl macexp(buffer, mac_size);
			macexp.copy(macro_src);
			while (strref param = params.split_token_trim(',')) {
				strref a = arg_list.split_token_trim(',');
				macexp.replace(param, a);
			}
			contextStack.push(m.source_name, macexp.get_strref(), macexp.get_strref());
			FlushLocalLabels();
			return STATUS_OK;
		} else
			return ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION;
	}
	contextStack.push(m.source_name, m.source_file, macro_src);
	FlushLocalLabels();
	return STATUS_OK;
}



//
//
// STRUCTS AND ENUMS
//
//



// Enums are Structs in disguise
StatusCode Asm::BuildEnum(strref name, strref declaration)
{
	unsigned int hash = name.fnv1a();
	unsigned int ins = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
	LabelStruct *pEnum = nullptr;
	while (ins < labelStructs.count() && labelStructs.getKey(ins)==hash) {
		if (name.same_str_case(labelStructs.getValue(ins).name)) {
			pEnum = labelStructs.getValues() + ins;
			break;
		}
		++ins;
	}
	if (pEnum)
		return ERROR_STRUCT_ALREADY_DEFINED;
	labelStructs.insert(ins, hash);
	pEnum = labelStructs.getValues() + ins;
	pEnum->name = name;
	pEnum->first_member = (unsigned short)structMembers.size();
	pEnum->numMembers = 0;
	pEnum->size = 0;		// enums are 0 sized
	int value = 0;

	struct EvalContext etx(CurrSection().GetPC(),
		scope_address[scope_depth], -1, -1);

	while (strref line = declaration.line()) {
		line = line.before_or_full(',');
		line.trim_whitespace();
		strref name = line.split_token_trim('=');
		if (line) {
			StatusCode error = EvalExpression(line, etx, value);
			if (error == STATUS_NOT_READY)
				return ERROR_ENUM_CANT_BE_ASSEMBLED;
			else if (error != STATUS_OK)
				return error;
		}
		struct MemberOffset member;
		member.offset = value;
		member.name = name;
		member.name_hash = member.name.fnv1a();
		member.sub_struct = strref();
		structMembers.push_back(member);
		++value;
		pEnum->numMembers++;
	}
	return STATUS_OK;
}

StatusCode Asm::BuildStruct(strref name, strref declaration)
{
	unsigned int hash = name.fnv1a();
	unsigned int ins = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
	LabelStruct *pStruct = nullptr;
	while (ins < labelStructs.count() && labelStructs.getKey(ins)==hash) {
		if (name.same_str_case(labelStructs.getValue(ins).name)) {
			pStruct = labelStructs.getValues() + ins;
			break;
		}
		++ins;
	}
	if (pStruct)
		return ERROR_STRUCT_ALREADY_DEFINED;
	labelStructs.insert(ins, hash);
	pStruct = labelStructs.getValues() + ins;
	pStruct->name = name;
	pStruct->first_member = (unsigned short)structMembers.size();

	unsigned int byte_hash = struct_byte.fnv1a();
	unsigned int word_hash = struct_word.fnv1a();
	unsigned short size = 0;
	unsigned short member_count = 0;
	
	while (strref line = declaration.line()) {
		line.trim_whitespace();
		strref type = line.split_label();
		line.skip_whitespace();
		unsigned int type_hash = type.fnv1a();
		unsigned short type_size = 0;
		LabelStruct *pSubStruct = nullptr;
		if (type_hash==byte_hash && struct_byte.same_str_case(type))
			type_size = 1;
		else if (type_hash==word_hash && struct_word.same_str_case(type))
			type_size = 2;
		else {
			unsigned int index = FindLabelIndex(type_hash, labelStructs.getKeys(), labelStructs.count());
			while (index < labelStructs.count() && labelStructs.getKey(index)==type_hash) {
				if (type.same_str_case(labelStructs.getValue(index).name)) {
					pSubStruct = labelStructs.getValues() + index;
					break;
				}
				++index;
			}
			if (!pSubStruct) {
				labelStructs.remove(ins);
				return ERROR_REFERENCED_STRUCT_NOT_FOUND;
			}
			type_size = pSubStruct->size;
		}
		
		// add the new member, don't grow vectors one at a time.
		if (structMembers.size() == structMembers.capacity())
			structMembers.reserve(structMembers.size() + 64);
		struct MemberOffset member;
		member.offset = size;
		member.name = line.get_label();
		member.name_hash = member.name.fnv1a();
		member.sub_struct = pSubStruct ? pSubStruct->name : strref();
		structMembers.push_back(member);

		size += type_size;
		member_count++;
	}
	
	pStruct->numMembers = member_count;
	pStruct->size = size;
	
	return STATUS_OK;
}

// Evaluate a struct offset as if it was a label
StatusCode Asm::EvalStruct(strref name, int &value)
{
	LabelStruct *pStruct = nullptr;
	unsigned short offset = 0;
	while (strref struct_seg = name.split_token('.')) {
		strref sub_struct = struct_seg;
		unsigned int seg_hash = struct_seg.fnv1a();
		if (pStruct) {
			struct MemberOffset *member = &structMembers[pStruct->first_member];
			bool found = false;
			for (int i=0; i<pStruct->numMembers; i++) {
				if (member->name_hash == seg_hash && member->name.same_str_case(struct_seg)) {
					offset += member->offset;
					sub_struct = member->sub_struct;
					found = true;
					break;
				}
				++member;
			}
			if (!found)
				return ERROR_REFERENCED_STRUCT_NOT_FOUND;
		}
		if (sub_struct) {
			unsigned int hash = sub_struct.fnv1a();
			unsigned int index = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
			while (index < labelStructs.count() && labelStructs.getKey(index)==hash) {
				if (sub_struct.same_str_case(labelStructs.getValue(index).name)) {
					pStruct = labelStructs.getValues() + index;
					break;
				}
				++index;
			}
		} else if (name)
			return STATUS_NOT_STRUCT;
	}
	if (pStruct == nullptr)
		return STATUS_NOT_STRUCT;
	value = offset;
	return STATUS_OK;
}


//
//
// EXPRESSIONS AND LATE EVALUATION
//
//


// Get a single token from a merlin expression
EvalOperator Asm::RPNToken_Merlin(strref &expression, const struct EvalContext &etx, EvalOperator prev_op, short &section, int &value)
{
	char c = expression.get_first();
	switch (c) {
		case '$': ++expression; value = expression.ahextoui_skip(); return EVOP_VAL;
		case '-': ++expression; return EVOP_SUB;
		case '+': ++expression;	return EVOP_ADD;
		case '*': // asterisk means both multiply and current PC, disambiguate!
			++expression;
			if (expression[0] == '*') return EVOP_STP; // double asterisks indicates comment
			else if (prev_op==EVOP_VAL || prev_op==EVOP_RPR) return EVOP_MUL;
			value = etx.pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '/': ++expression; return EVOP_DIV;
		case '>': if (expression.get_len() >= 2 && expression[1] == '>') { expression += 2; return EVOP_SHR; }
			++expression; return EVOP_HIB;
		case '<': if (expression.get_len() >= 2 && expression[1] == '<') { expression += 2; return EVOP_SHL; }
			++expression; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (expression[1]=='0' || expression[1]=='1') {
				++expression; value = expression.abinarytoui_skip(); return EVOP_VAL; }
			if (etx.scope_end_pc<0) return EVOP_NRY;
			++expression; value = etx.scope_end_pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '|':
		case '.': ++expression; return EVOP_OR;	// MERLIN: . is or, | is not used
		case '^': ++expression; return EVOP_EOR;
		case '&': ++expression; return EVOP_AND;
		case '(': if (prev_op!=EVOP_VAL) { ++expression; return EVOP_LPR; } return EVOP_STP;
		case ')': ++expression; return EVOP_RPR;
		case '"': if (expression[2] == '"') { value = expression[1];  expression += 3; return EVOP_VAL; } return EVOP_STP;
		case '\'': if (expression[2] == '\'') { value = expression[1];  expression += 3; return EVOP_VAL; } return EVOP_STP;
		case ',':
		case '?':
		default: {	// MERLIN: ! is eor
			if (c == '!' && (prev_op == EVOP_VAL || prev_op == EVOP_RPR)) { ++expression; return EVOP_EOR; }
			else if (c == '!' && !(expression + 1).len_label()) {
				if (etx.scope_pc < 0) return EVOP_NRY;	// ! by itself is current scope, !+label char is a local label
				++expression; value = etx.scope_pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
			} else if (strref::is_number(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP;	// value followed by value doesn't make sense, stop
				value = expression.atoi_skip(); return EVOP_VAL;
			} else if (c == '!' || c == ']' || c==':' || strref::is_valid_label(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
				char e0 = expression[0];
				int start_pos = (e0==']' || e0==':' || e0=='!' || e0=='.') ? 1 : 0;
				strref label = expression.split_range_trim(label_end_char_range_merlin, start_pos);
				Label *pLabel = pLabel = GetLabel(label, etx.file_ref);
				if (!pLabel) {
					StatusCode ret = EvalStruct(label, value);
					if (ret == STATUS_OK) return EVOP_VAL;
					if (ret != STATUS_NOT_STRUCT) return EVOP_ERR;	// partial struct
				}
				if (!pLabel || !pLabel->evaluated) return EVOP_NRY;	// this label could not be found (yet)
				value = pLabel->value; section = pLabel->section; return EVOP_VAL;
			} else
				return EVOP_ERR;
			break;
		}
	}
	return EVOP_NONE; // shouldn't get here normally
}

// Get a single token from most non-apple II assemblers
EvalOperator Asm::RPNToken(strref &expression, const struct EvalContext &etx, EvalOperator prev_op, short &section, int &value)
{
	char c = expression.get_first();
	switch (c) {
		case '$': ++expression; value = expression.ahextoui_skip(); return EVOP_VAL;
		case '-': ++expression; return EVOP_SUB;
		case '+': ++expression;	return EVOP_ADD;
		case '*': // asterisk means both multiply and current PC, disambiguate!
			++expression;
			if (expression[0] == '*') return EVOP_STP; // double asterisks indicates comment
			else if (prev_op == EVOP_VAL || prev_op == EVOP_RPR) return EVOP_MUL;
			value = etx.pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '/': ++expression; return EVOP_DIV;
		case '>': if (expression.get_len() >= 2 && expression[1] == '>') { expression += 2; return EVOP_SHR; }
				  ++expression; return EVOP_HIB;
		case '<': if (expression.get_len() >= 2 && expression[1] == '<') { expression += 2; return EVOP_SHL; }
				  ++expression; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (expression[1] == '0' || expression[1] == '1') { ++expression; value = expression.abinarytoui_skip(); return EVOP_VAL; }
			if (etx.scope_end_pc<0) return EVOP_NRY;
			++expression; value = etx.scope_end_pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '|': ++expression; return EVOP_OR;
		case '^': ++expression; return EVOP_EOR;
		case '&': ++expression; return EVOP_AND;
		case '(': if (prev_op != EVOP_VAL) { ++expression; return EVOP_LPR; } return EVOP_STP;
		case ')': ++expression; return EVOP_RPR;
		case ',':
		case '?':
		case '\'': return EVOP_STP;
		default: {	// ! by itself is current scope, !+label char is a local label
			if (c == '!' && !(expression + 1).len_label()) {
				if (etx.scope_pc < 0) return EVOP_NRY;
				++expression; value = etx.scope_pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
			} else if (strref::is_number(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP;	// value followed by value doesn't make sense, stop
				value = expression.atoi_skip(); return EVOP_VAL;
			} else if (c == '!' || c == ':' || c=='.' || c=='@' || strref::is_valid_label(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
				char e0 = expression[0];
				int start_pos = (e0 == ':' || e0 == '!' || e0 == '.') ? 1 : 0;
				strref label = expression.split_range_trim(label_end_char_range, start_pos);
				Label *pLabel = pLabel = GetLabel(label, etx.file_ref);
				if (!pLabel) {
					StatusCode ret = EvalStruct(label, value);
					if (ret == STATUS_OK) return EVOP_VAL;
					if (ret != STATUS_NOT_STRUCT) return EVOP_ERR;	// partial struct
				}
				if (!pLabel || !pLabel->evaluated) return EVOP_NRY;	// this label could not be found (yet)
				value = pLabel->value; section = pLabel->section; return EVOP_VAL;
			}
			return EVOP_ERR;
		}
	}
	return EVOP_NONE; // shouldn't get here normally
}

//
// EvalExpression
//	Uses the Shunting Yard algorithm to convert to RPN first
//	which makes the actual calculation trivial and avoids recursion.
//	https://en.wikipedia.org/wiki/Shunting-yard_algorithm
//
// Return:
//	STATUS_OK means value is completely evaluated
//	STATUS_NOT_READY means value could not be evaluated right now
//	ERROR_* means there is an error in the expression
//

// Max number of unresolved sections to evaluate in a single expression
#define MAX_EVAL_SECTIONS 4

StatusCode Asm::EvalExpression(strref expression, const struct EvalContext &etx, int &result)
{
	int numValues = 0;
	int numOps = 0;

	char ops[MAX_EVAL_OPER];		// RPN expression
	int values[MAX_EVAL_VALUES];	// RPN values (in order of RPN EVOP_VAL operations)
	short section_ids[MAX_EVAL_SECTIONS];	// local index of each referenced section
	short section_val[MAX_EVAL_VALUES] = { 0 };		// each value can be assigned to one section, or -1 if fixed
	short num_sections = 0;			// number of sections in section_ids (normally 0 or 1, can be up to MAX_EVAL_SECTIONS)
	values[0] = 0;					// Initialize RPN if no expression
	{
		int sp = 0;
		char op_stack[MAX_EVAL_OPER];
		EvalOperator prev_op = EVOP_NONE;
		expression.trim_whitespace();
		while (expression) {
			int value = 0;
			short section = -1, index_section = -1;
			EvalOperator op = EVOP_NONE;
			if (syntax == SYNTAX_MERLIN)
				op = RPNToken_Merlin(expression, etx, prev_op, section, value);
			else
				op = RPNToken(expression, etx, prev_op, section, value);
			if (op == EVOP_ERR)
				return ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION;
			else if (op == EVOP_NRY)
				return STATUS_NOT_READY;
			if (section >= 0) {
				for (int s = 0; s<num_sections && index_section<0; s++) {
					if (section_ids[s] == section) index_section = s;
				}
				if (index_section<0) {
					if (num_sections <= MAX_EVAL_SECTIONS)
						section_ids[index_section = num_sections++] = section;
					else
						return STATUS_NOT_READY;
				}
			}

			// this is the body of the shunting yard algorithm
			if (op == EVOP_VAL) {
				section_val[numValues] = index_section;	// only value operators can be section specific
				values[numValues++] = value;
				ops[numOps++] = op;
			} else if (op == EVOP_LPR) {
				op_stack[sp++] = op;
			} else if (op == EVOP_RPR) {
				while (sp && op_stack[sp-1]!=EVOP_LPR) {
					sp--;
					ops[numOps++] = op_stack[sp];
				}
				// check that there actually was a left parenthesis
				if (!sp || op_stack[sp-1]!=EVOP_LPR)
					return ERROR_UNBALANCED_RIGHT_PARENTHESIS;
				sp--; // skip open paren
			} else if (op == EVOP_STP) {
				break;
			} else {
				while (sp) {
					EvalOperator p = (EvalOperator)op_stack[sp-1];
					if (p==EVOP_LPR || op>p)
						break;
					ops[numOps++] = p;
					sp--;
				}
				op_stack[sp++] = op;
			}
			// check for out of bounds or unexpected input
			if (numValues==MAX_EVAL_VALUES)
				return ERROR_TOO_MANY_VALUES_IN_EXPRESSION;
			else if (numOps==MAX_EVAL_OPER || sp==MAX_EVAL_OPER)
				return ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION;

			prev_op = op;
			expression.skip_whitespace();
		}
		while (sp) {
			sp--;
			ops[numOps++] = op_stack[sp];
		}
	}
	// processing the result RPN will put the completed expression into values[0].
	// values is used as both the queue and the stack of values since reads/writes won't
	// exceed itself.
	{
		int valIdx = 0;
		int ri = 0;		// RPN index (value)
		int preByteVal = 0; // special case for relative reference to low byte / high byte
		short section_counts[MAX_EVAL_SECTIONS][MAX_EVAL_VALUES] = { 0 };
		for (int o = 0; o<numOps; o++) {
			EvalOperator op = (EvalOperator)ops[o];
			if (op!=EVOP_VAL && op!=EVOP_LOB && op!=EVOP_HIB && ri<2)
				break; // ignore suffix operations that are lacking values
			switch (op) {
				case EVOP_VAL:	// value
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri] = i==section_val[ri] ? 1 : 0;
					values[ri++] = values[valIdx++]; break;
				case EVOP_ADD:	// +
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] += section_counts[i][ri];
					values[ri-1] += values[ri]; break;
				case EVOP_SUB:	// -
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] -= section_counts[i][ri];
					values[ri-1] -= values[ri]; break;
				case EVOP_MUL:	// *
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] *= values[ri]; break;
				case EVOP_DIV:	// /
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] /= values[ri]; break;
				case EVOP_AND:	// &
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] &= values[ri]; break;
				case EVOP_OR:	// |
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] |= values[ri]; break;
				case EVOP_EOR:	// ^
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] ^= values[ri]; break;
				case EVOP_SHL:	// <<
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] <<= values[ri]; break;
				case EVOP_SHR:	// >>
					ri--;
					for (int i = 0; i<num_sections; i++)
						section_counts[i][ri-1] |= section_counts[i][ri];
					values[ri-1] >>= values[ri]; break;
				case EVOP_LOB:	// low byte
					if (ri) {
						preByteVal = values[ri - 1];
						values[ri-1] &= 0xff;
					} break;
				case EVOP_HIB:
					if (ri) {
						preByteVal = values[ri - 1];
						values[ri - 1] = (values[ri - 1] >> 8) & 0xff;
					} break;
				default:
					return ERROR_EXPRESSION_OPERATION;
					break;
			}
		}
		int section_index = -1;
		bool curr_relative = false;
		// If relative to any section unless specifically interested in a relative value then return not ready
		for (int i = 0; i<num_sections; i++) {
			if (section_counts[i][0]) {
				if (section_counts[i][0]!=1 || section_index>=0)
					return STATUS_NOT_READY;
				else if (etx.relative_section==section_ids[i])
					curr_relative = true;
				else if (etx.relative_section>=0)
					return STATUS_NOT_READY;
				section_index = i;
			}
		}
		result = values[0];
		if (section_index>=0 && !curr_relative) {
			lastEvalSection = section_ids[section_index];
			lastEvalValue = (ops[numOps - 1] == EVOP_LOB || ops[numOps - 1] == EVOP_HIB) ? preByteVal : result;
			lastEvalPart = ops[numOps - 1] == EVOP_LOB ? Reloc::LO_BYTE : (ops[numOps - 1] == EVOP_HIB ? Reloc::HI_BYTE : Reloc::WORD);
			return STATUS_RELATIVE_SECTION;
		}
	}
			
	return STATUS_OK;
}


// if an expression could not be evaluated, add it along with
// the action to perform if it can be evaluated later.
void Asm::AddLateEval(int target, int pc, int scope_pc, strref expression, strref source_file, LateEval::Type type)
{
	LateEval le;
	le.address = pc;
	le.scope = scope_pc;
	le.target = target;
	le.section = (int)(&CurrSection() - &allSections[0]);
	le.file_ref = -1; // current or xdef'd
	le.label.clear();
	le.expression = expression;
	le.source_file = source_file;
	le.type = type;

	lateEval.push_back(le);
}

void Asm::AddLateEval(strref label, int pc, int scope_pc, strref expression, LateEval::Type type)
{
	LateEval le;
	le.address = pc;
	le.scope = scope_pc;
	le.target = 0;
	le.label = label;
	le.file_ref = -1; // current or xdef'd
	le.expression = expression;
	le.source_file.clear();
	le.type = type;
	
	lateEval.push_back(le);
}

// When a label is defined or a scope ends check if there are
// any related late label evaluators that can now be evaluated.
StatusCode Asm::CheckLateEval(strref added_label, int scope_end)
{
	bool evaluated_label = true;
	strref new_labels[MAX_LABELS_EVAL_ALL];
	int num_new_labels = 0;
	if (added_label)
		new_labels[num_new_labels++] = added_label;

	bool all = !added_label;
	std::vector<LateEval>::iterator i = lateEval.begin();
	while (evaluated_label) {
		evaluated_label = false;
		while (i != lateEval.end()) {
			int value = 0;
			// check if this expression is related to the late change (new label or end of scope)
			bool check = all || num_new_labels==MAX_LABELS_EVAL_ALL;
			for (int l=0; l<num_new_labels && !check; l++)
				check = i->expression.find(new_labels[l]) >= 0;
			if (!check && scope_end>0) {
				int gt_pos = 0;
				while (gt_pos>=0 && !check) {
					gt_pos = i->expression.find_at('%', gt_pos);
					if (gt_pos>=0) {
						if (i->expression[gt_pos+1]=='%')
							gt_pos++;
						else
							check = true;
						gt_pos++;
					}
				}
			}
			if (check) {
				struct EvalContext etx(i->address, i->scope, scope_end,
					i->type == LateEval::LET_BRANCH ? SectionId() : -1);
				etx.file_ref = i->file_ref;
				StatusCode ret = EvalExpression(i->expression, etx, value);
				if (ret == STATUS_OK || ret==STATUS_RELATIVE_SECTION) {
					// Check if target section merged with another section
					int trg = i->target;
					int sec = i->section;
					if (i->type != LateEval::LET_LABEL) {
						if (allSections[sec].IsMergedSection()) {
							trg += allSections[sec].merged_offset;
							sec = allSections[sec].merged_section;
						}
					}
					bool resolved = true;
					switch (i->type) {
						case LateEval::LET_BRANCH:
							value -= i->address+1;
							if (value<-128 || value>127)
								return ERROR_BRANCH_OUT_OF_RANGE;
							if (trg >= allSections[sec].size())
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							allSections[sec].SetByte(trg, value);
							break;
						case LateEval::LET_BYTE:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0)
									resolved = false;
								else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection,
										 lastEvalPart==Reloc::HI_BYTE ? Reloc::HI_BYTE : Reloc::LO_BYTE);
									value = 0;
								}
							}
							if (trg >= allSections[sec].size())
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							allSections[sec].SetByte(trg, value);
							break;
						case LateEval::LET_ABS_REF:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0)
									resolved = false;
								else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, lastEvalPart);
									value = 0;
								}
							}
							if ((trg+1) >= allSections[sec].size())
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							allSections[sec].SetWord(trg, value);
							break;
						case LateEval::LET_LABEL: {
							Label *label = GetLabel(i->label, i->file_ref);
							if (!label)
								return ERROR_LABEL_MISPLACED_INTERNAL;
							label->value = value;
							label->evaluated = true;
							if (num_new_labels<MAX_LABELS_EVAL_ALL)
								new_labels[num_new_labels++] = label->label_name;
							evaluated_label = true;
							char f = i->label[0], l = i->label.get_last();
							LabelAdded(label, f=='.' || f=='!' || f=='@' || f==':' || l=='$');
							break;
						}
						default:
							break;
					}
					if (resolved)
						i = lateEval.erase(i);
				} else
					++i;
			} else
				++i;
		}
		all = false;
		added_label.clear();
	}
	return STATUS_OK;
}



//
//
// LABELS
//
//



// Get a label record if it exists
Label *Asm::GetLabel(strref label)
{
	unsigned int label_hash = label.fnv1a();
	unsigned int index = FindLabelIndex(label_hash, labels.getKeys(), labels.count());
	while (index < labels.count() && label_hash == labels.getKey(index)) {
		if (label.same_str(labels.getValue(index).label_name))
			return labels.getValues() + index;
		index++;
	}
	return nullptr;
}

// Get a protected label record from a file if it exists
Label *Asm::GetLabel(strref label, int file_ref)
{
	if (file_ref>=0 && file_ref<(int)externals.size()) {
		ExtLabels &labs = externals[file_ref];
		unsigned int label_hash = label.fnv1a();
		unsigned int index = FindLabelIndex(label_hash, labs.labels.getKeys(), labs.labels.count());
		while (index < labs.labels.count() && label_hash == labs.labels.getKey(index)) {
			if (label.same_str(labs.labels.getValue(index).label_name))
				return labs.labels.getValues() + index;
			index++;
		}
	}
	return GetLabel(label);
}

// If exporting labels, append this label to the list
void Asm::LabelAdded(Label *pLabel, bool local)
{
	if (pLabel && pLabel->evaluated) {
        if (map.size() == map.capacity())
            map.reserve(map.size() + 256);
        MapSymbol sym;
        sym.name = pLabel->label_name;
        sym.section = pLabel->section;
        sym.value = pLabel->value;
        sym.local = local;
		pLabel->mapIndex = -1;
        map.push_back(sym);
	}
}

// Add a label entry
Label* Asm::AddLabel(unsigned int hash) {
	unsigned int index = FindLabelIndex(hash, labels.getKeys(), labels.count());
	labels.insert(index, hash);
	return labels.getValues() + index;
}

// mark a label as a local label
void Asm::MarkLabelLocal(strref label, bool scope_reserve)
{
	LocalLabelRecord rec;
	rec.label = label;
	rec.scope_depth = scope_depth;
	rec.scope_reserve = scope_reserve;
	localLabels.push_back(rec);
}

// find all local labels or up to given scope level and remove them
void Asm::FlushLocalLabels(int scope_exit)
{
	// iterate from end of local label records and early out if the label scope is lower than the current.
	std::vector<LocalLabelRecord>::iterator i = localLabels.end();
	while (i!=localLabels.begin()) {
		--i;
		if (i->scope_depth < scope_depth)
			break;
		strref label = i->label;
		if (!i->scope_reserve || i->scope_depth<=scope_exit) {
			unsigned int index = FindLabelIndex(label.fnv1a(), labels.getKeys(), labels.count());
			while (index<labels.count()) {
				if (label.same_str_case(labels.getValue(index).label_name)) {
					if (i->scope_reserve) {
						if (LabelPool *pool = GetLabelPool(labels.getValue(index).pool_name)) {
							pool->Release(labels.getValue(index).value);
							break;
						}
					}
					labels.remove(index);
					break;
				}
				++index;
			}
			i = localLabels.erase(i);
		}
	}
}

// Get a label pool by name
LabelPool* Asm::GetLabelPool(strref pool_name)
{
	unsigned int pool_hash = pool_name.fnv1a();
	unsigned int ins = FindLabelIndex(pool_hash, labelPools.getKeys(), labelPools.count());
	while (ins < labelPools.count() && pool_hash == labelPools.getKey(ins)) {
		if (pool_name.same_str(labelPools.getValue(ins).pool_name)) {
			return &labelPools.getValue(ins);
		}
		ins++;
	}
	return nullptr;
}

// When going out of scope, label pools are deleted.
void Asm::FlushLabelPools(int scope_exit)
{
	unsigned int i = 0;
	while (i<labelPools.count()) {
		if (labelPools.getValue(i).scopeDepth >= scope_exit)
			labelPools.remove(i);
		else
			++i;
	}
}

// Add a label pool
StatusCode Asm::AddLabelPool(strref name, strref args)
{
	unsigned int pool_hash = name.fnv1a();
	unsigned int ins = FindLabelIndex(pool_hash, labelPools.getKeys(), labelPools.count());
	unsigned int index = ins;
	while (index < labelPools.count() && pool_hash == labelPools.getKey(index)) {
		if (name.same_str(labelPools.getValue(index).pool_name))
			return ERROR_LABEL_POOL_REDECLARATION;
		index++;
	}

	// check that there is at least one valid address
	int ranges = 0;
	int num32 = 0;
	unsigned short aRng[256];
	struct EvalContext etx(CurrSection().GetPC(), scope_address[scope_depth], -1, -1);
	while (strref arg = args.split_token_trim(',')) {
		strref start = arg[0]=='(' ? arg.scoped_block_skip() : arg.split_token_trim('-');
		int addr0 = 0, addr1 = 0;
		if (STATUS_OK != EvalExpression(start, etx, addr0))
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		if (STATUS_OK != EvalExpression(arg, etx, addr1))
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		if (addr1<=addr0 || addr0<0)
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;

		aRng[ranges++] = addr0;
		aRng[ranges++] = addr1;
		num32 += (addr1-addr0+15)>>4;

		if (ranges >(MAX_POOL_RANGES*2) ||
			num32 > ((MAX_POOL_BYTES+15)>>4))
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
	}

	if (!ranges)
		return ERROR_POOL_RANGE_EXPRESSION_EVAL;

	LabelPool pool;
	pool.pool_name = name;
	pool.numRanges = ranges>>1;
	pool.scopeDepth = scope_depth;

	memset(pool.usedMap, 0, sizeof(unsigned int) * num32);
	for (int r = 0; r<ranges; r++)
		pool.ranges[r] = aRng[r];

	labelPools.insert(ins, pool_hash);
	LabelPool &poolValue = labelPools.getValue(ins);

	poolValue = pool;

	return STATUS_OK;
}

StatusCode Asm::AssignPoolLabel(LabelPool &pool, strref label)
{
	strref type = label;
	label = type.split_token('.');
	int bytes = 1;
	if (strref::tolower(type[0])=='w')
		bytes = 2;
	if (GetLabel(label))
		return ERROR_POOL_LABEL_ALREADY_DEFINED;
	unsigned int addr;
	StatusCode error = pool.Reserve(bytes, addr);
	if (error != STATUS_OK)
		return error;

	Label *pLabel = AddLabel(label.fnv1a());

	pLabel->label_name = label;
	pLabel->pool_name = pool.pool_name;
	pLabel->evaluated = true;
	pLabel->section = -1;	// pool labels are section-less
	pLabel->value = addr;
	pLabel->pc_relative = true;
	pLabel->constant = true;
	pLabel->external = false;

	MarkLabelLocal(label, true);
	return error;
}

// Request a label from a pool
StatusCode LabelPool::Reserve(int numBytes, unsigned int &ret_addr)
{
	unsigned int *map = usedMap;
	unsigned short *pRanges = ranges;
	for (int r = 0; r<numRanges; r++) {
		int sequence = 0;
		unsigned int a0 = *pRanges++, a1 = *pRanges++;
		unsigned int addr = a1-1, *range_map = map;
		while (addr>=a0 && sequence<numBytes) {
			unsigned int chk = *map++, m = 3;
			while (m && addr >= a0) {
				if ((m & chk)==0) {
					sequence++;
					if (sequence == numBytes)
						break;
				} else
					sequence = 0;
				--addr;
				m <<= 2;
			}
		}
		if (sequence == numBytes) {
			unsigned int index = (a1-addr-numBytes);
			unsigned int *addr_map = range_map + (index>>4);
			unsigned int m = numBytes << (index << 1);
			for (int b = 0; b<numBytes; b++) {
				*addr_map |= m;
				unsigned int _m = m << 2;
				if (!_m) { m <<= 30; addr_map++; } else { m = _m; }
			}
			ret_addr = addr;
			return STATUS_OK;
		}
	}
	return ERROR_OUT_OF_LABELS_IN_POOL;
}

// Release a label from a pool (at scope closure)
StatusCode LabelPool::Release(unsigned int addr) {
	unsigned int *map = usedMap;
	unsigned short *pRanges = ranges;
	for (int r = 0; r<numRanges; r++) {
		unsigned short a0 = *pRanges++, a1 = *pRanges++;
		if (addr>=a0 && addr<a1) {
			unsigned int index = (a1-addr-1);
			map += index>>4;
			index &= 0xf;
			unsigned int u = *map, m = 3 << (index << 1);
			unsigned int b = u & m, bytes = b >> (index << 1);
			if (bytes) {
				for (unsigned int f = 0; f<bytes; f++) {
					u &= ~m;
					unsigned int _m = m>>2;
					if (!_m) { m <<= 30; *map-- = u; } else { m = _m; }
				}
				*map = u;
				return STATUS_OK;
			} else
				return ERROR_INTERNAL_LABEL_POOL_ERROR;
		} else
			map += (a1-a0+15)>>4;
	}
	return STATUS_OK;
}

// Check if a label is marked as an xdef
bool Asm::MatchXDEF(strref label)
{
	unsigned int hash = label.fnv1a();
	unsigned int pos = FindLabelIndex(hash, xdefs.getKeys(), xdefs.count());
	while (pos < xdefs.count() && xdefs.getKey(pos) == hash) {
		if (label.same_str_case(xdefs.getValue(pos)))
			return true;
		++pos;
	}
	return false;
}

// assignment of label (<label> = <expression>)
StatusCode Asm::AssignLabel(strref label, strref line, bool make_constant)
{
	line.trim_whitespace();
	int val = 0;
	struct EvalContext etx(CurrSection().GetPC(), scope_address[scope_depth], -1, -1);
	StatusCode status = EvalExpression(line, etx, val);
	if (status != STATUS_NOT_READY && status != STATUS_OK)
		return status;
	
	Label *pLabel = GetLabel(label);
	if (pLabel) {
		if (pLabel->constant && pLabel->evaluated && val != pLabel->value)
			return (status == STATUS_NOT_READY) ? STATUS_OK : ERROR_MODIFYING_CONST_LABEL;
	} else
		pLabel = AddLabel(label.fnv1a());
	
	pLabel->label_name = label;
	pLabel->pool_name.clear();
	pLabel->evaluated = status==STATUS_OK;
	pLabel->section = -1;	// assigned labels are section-less
	pLabel->value = val;
    pLabel->mapIndex = -1;
	pLabel->pc_relative = false;
	pLabel->constant = make_constant;
	pLabel->external = MatchXDEF(label);

	bool local = label[0]=='.' || label[0]=='@' || label[0]=='!' || label[0]==':' || label.get_last()=='$';
	if (!pLabel->evaluated)
		AddLateEval(label, CurrSection().GetPC(), scope_address[scope_depth], line, LateEval::LET_LABEL);
	else {
		if (local)
			MarkLabelLocal(label);
		LabelAdded(pLabel, local);
		return CheckLateEval(label);
	}
	return STATUS_OK;
}

// Adding a fixed address label
StatusCode Asm::AddressLabel(strref label)
{
	Label *pLabel = GetLabel(label);
	bool constLabel = false;
	if (!pLabel)
		pLabel = AddLabel(label.fnv1a());
	else if (pLabel->constant && pLabel->value != CurrSection().GetPC())
		return ERROR_MODIFYING_CONST_LABEL;
	else
		constLabel = pLabel->constant;
	
	pLabel->label_name = label;
	pLabel->pool_name.clear();
	pLabel->section = CurrSection().IsRelativeSection() ? SectionId() : -1;	// address labels are based on section
	pLabel->value = CurrSection().GetPC();
	pLabel->evaluated = true;
	pLabel->pc_relative = true;
	pLabel->external = MatchXDEF(label);
	pLabel->constant = constLabel;
	bool local = label[0]=='.' || label[0]=='@' || label[0]=='!' || label[0]==':' || label.get_last()=='$';
	LabelAdded(pLabel, local);
	if (local)
		MarkLabelLocal(label);
	else if (label[0]!=']')	// MERLIN: Variable label does not invalidate local labels
		FlushLocalLabels();
	return CheckLateEval(label);
}

// include symbols listed from a .sym file or all if no listing
void Asm::IncludeSymbols(strref line)
{
	strref symlist = line.before('"').get_trimmed_ws();
	line = line.between('"', '"');
	size_t size;
	if (char *buffer = LoadText(line, size)) {
		strref symfile(buffer, strl_t(size));
		while (symfile) {
			symfile.skip_whitespace();
			if (symfile[0]=='{')			// don't include local labels
				symfile.scoped_block_skip();
			if (strref symdef = symfile.line()) {
				strref symtype = symdef.split_token(' ');
				strref label = symdef.split_token_trim('=');
				bool constant = symtype.same_str(".const");	// first word is either .label or .const
				if (symlist) {
					strref symchk = symlist;
					while (strref symwant = symchk.split_token_trim(',')) {
						if (symwant.same_str_case(label)) {
							AssignLabel(label, symdef, constant);
							break;
						}
					}
				} else
					AssignLabel(label, symdef, constant);
			}
		}
		loadedData.push_back(buffer);
	}
}



//
//
// CONDITIONAL ASSEMBLY
//
//



// Encountered #if or #ifdef, return true if assembly is enabled
bool Asm::NewConditional() {
	if (conditional_nesting[conditional_depth]) {
		conditional_nesting[conditional_depth]++;
		return false;
	}
	return true;
}

// Encountered #endif, close out the current conditional
void Asm::CloseConditional() {
	if (conditional_depth)
		conditional_depth--;
	else
		conditional_consumed[conditional_depth] = false;
}

// Check if this conditional will nest the assembly (a conditional is already consumed)
void Asm::CheckConditionalDepth() {
	if (conditional_consumed[conditional_depth]) {
		conditional_depth++;
		conditional_source[conditional_depth] = contextStack.curr().read_source.get_line();
		conditional_consumed[conditional_depth] = false;
		conditional_nesting[conditional_depth] = 0;
	}
}

// This conditional block is going to be assembled, mark it as consumed
void Asm::ConsumeConditional()
{
	conditional_source[conditional_depth] = contextStack.curr().read_source.get_line();
	conditional_consumed[conditional_depth] = true;
}

// This conditional block is not going to be assembled so mark that it is nesting
void Asm::SetConditional()
{
	conditional_source[conditional_depth] = contextStack.curr().read_source.get_line();
	conditional_nesting[conditional_depth] = 1;
}

// Returns true if assembly is currently enabled
bool Asm::ConditionalAsm() {
	return conditional_nesting[conditional_depth]==0;
}

// Returns true if this conditional has a block that has already been assembled
bool Asm::ConditionalConsumed() {
	return conditional_consumed[conditional_depth];
}

// Returns true if this conditional can be consumed
bool Asm::ConditionalAvail() {
	return conditional_nesting[conditional_depth]==1 &&
		!conditional_consumed[conditional_depth];
}

// This conditional block is enabled and the prior wasn't
void Asm::EnableConditional(bool enable) {
	if (enable) {
		conditional_nesting[conditional_depth] = 0;
		conditional_consumed[conditional_depth] = true;
	}
}

// Conditional else that does not enable block
void Asm::ConditionalElse() {
	if (conditional_consumed[conditional_depth])
		conditional_nesting[conditional_depth]++;
}

// Conditional statement evaluation (true/false)
StatusCode Asm::EvalStatement(strref line, bool &result)
{
	int equ = line.find('=');
	struct EvalContext etx(CurrSection().GetPC(), scope_address[scope_depth], -1, -1);
	if (equ >= 0) {
		// (EXP) == (EXP)
		strref left = line.get_clipped(equ);
		bool equal = left.get_last()!='!';
		left.trim_whitespace();
		strref right = line + equ + 1;
		if (right.get_first()=='=')
			++right;
		right.trim_whitespace();
		int value_left, value_right;
		if (STATUS_OK != EvalExpression(left, etx, value_left))
			return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
		if (STATUS_OK != EvalExpression(right, etx, value_right))
			return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
		result = (value_left==value_right && equal) || (value_left!=value_right && !equal);
	} else {
		bool invert = line.get_first()=='!';
		if (invert)
			++line;
		int value;
		if (STATUS_OK != EvalExpression(line, etx, value))
			return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
		result = (value!=0 && !invert) || (value==0 && invert);
	}
	return STATUS_OK;
}

// Add a folder for including files
void Asm::AddIncludeFolder(strref path)
{
	if (!path)
		return;
	for (std::vector<strref>::const_iterator i=includePaths.begin(); i!=includePaths.end(); ++i) {
		if (path.same_str(*i))
			return;
	}
	if (includePaths.size()==includePaths.capacity())
		includePaths.reserve(includePaths.size() + 16);
	includePaths.push_back(path);
}

// unique key binary search
int LookupOpCodeIndex(unsigned int hash, OP_ID *lookup, int count)
{
	int first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		unsigned int read = lookup[index].op_hash;
		if (hash==read) {
			return index;
		} else if (hash>read)
			first = index+1;
		else
			count = index;
	}
	return -1;	// index not found
}

typedef struct {
	const char *name;
	AssemblerDirective directive;
} DirectiveName;

DirectiveName aDirectiveNames[] {
	{ "CPU", AD_CPU },
	{ "PROCESSOR", AD_CPU },
	{ "PC", AD_ORG },
	{ "ORG", AD_ORG },
	{ "LOAD", AD_LOAD },
	{ "EXPORT", AD_EXPORT },
	{ "SECTION", AD_SECTION },
	{ "SEG", AD_SECTION },		// DASM version of SECTION
	{ "LINK", AD_LINK },
	{ "XDEF", AD_XDEF },
	{ "INCOBJ", AD_INCOBJ },
	{ "ALIGN", AD_ALIGN },
	{ "MACRO", AD_MACRO },
	{ "EVAL", AD_EVAL },
	{ "PRINT", AD_EVAL },
	{ "BYTE", AD_BYTES },
	{ "BYTES", AD_BYTES },
	{ "WORD", AD_WORDS },
	{ "WORDS", AD_WORDS },
	{ "DC", AD_DC },
	{ "TEXT", AD_TEXT },
	{ "INCLUDE", AD_INCLUDE },
	{ "INCBIN", AD_INCBIN },
	{ "CONST", AD_CONST },
	{ "LABEL", AD_LABEL },
	{ "INCSYM", AD_INCSYM },
	{ "LABPOOL", AD_LABPOOL },
	{ "POOL", AD_LABPOOL },
	{ "#IF", AD_IF },
	{ "#IFDEF", AD_IFDEF },
	{ "#ELSE", AD_ELSE },
	{ "#ELIF", AD_ELIF },
	{ "#ENDIF", AD_ENDIF },
	{ "IF", AD_IF },
	{ "IFDEF", AD_IFDEF },
	{ "ELSE", AD_ELSE },
	{ "ELIF", AD_ELIF },
	{ "ENDIF", AD_ENDIF },
	{ "STRUCT", AD_STRUCT },
	{ "ENUM", AD_ENUM },
	{ "REPT", AD_REPT },
	{ "INCDIR", AD_INCDIR },
	{ "DO", AD_IF },			// MERLIN
	{ "DA", AD_WORDS },			// MERLIN
	{ "DW", AD_WORDS },			// MERLIN
	{ "ASC", AD_TEXT },			// MERLIN
	{ "PUT", AD_INCLUDE },		// MERLIN
	{ "DDB", AD_WORDS },		// MERLIN
	{ "DB", AD_BYTES },			// MERLIN
	{ "DFB", AD_BYTES },		// MERLIN
	{ "HEX", AD_HEX },			// MERLIN
	{ "DO", AD_IF },			// MERLIN
	{ "FIN", AD_ENDIF },		// MERLIN
	{ "EJECT", AD_EJECT },		// MERLIN
	{ "OBJ", AD_EJECT },		// MERLIN
	{ "TR", AD_EJECT },			// MERLIN
	{ "USR", AD_USR },			// MERLIN
	{ "DUM", AD_DUMMY },		// MERLIN
	{ "DEND", AD_DUMMY_END },	// MERLIN
	{ "LST", AD_LST },			// MERLIN
	{ "LSTDO", AD_LST },		// MERLIN
	{ "DS", AD_DS },			// MERLIN
};

static const int nDirectiveNames = sizeof(aDirectiveNames) / sizeof(aDirectiveNames[0]);

// Action based on assembler directive
StatusCode Asm::ApplyDirective(AssemblerDirective dir, strref line, strref source_file)
{
	StatusCode error = STATUS_OK;
	if (!ConditionalAsm()) {	// If conditionally blocked from assembling only check conditional directives
		if (dir!=AD_IF && dir!=AD_IFDEF && dir!=AD_ELSE && dir!=AD_ELIF && dir!=AD_ELSE && dir!=AD_ENDIF)
			return STATUS_OK;
	}
	struct EvalContext etx(CurrSection().GetPC(), scope_address[scope_depth], -1, -1);
	switch (dir) {
		case AD_CPU:
			if (!line.same_str("6502"))
				return ERROR_CPU_NOT_SUPPORTED;
			break;
			
		case AD_EXPORT:
			line.trim_whitespace();
			CurrSection().export_append = line.split_label();
			break;
			
		case AD_ORG: {		// org / pc: current address of code
			int addr;
			if (line[0]=='=')
				++line;
			else if(keyword_equ.is_prefix_word(line))	// optional '=' or equ
				line.next_word_ws();
			line.skip_whitespace();
			if ((error = EvalExpression(line, etx, addr))) {
				error = error == STATUS_NOT_READY ? ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;
				break;
			}
			// Section immediately followed by ORG reassigns that section to be fixed
			if (CurrSection().size()==0 && !CurrSection().IsDummySection()) {
				CurrSection().start_address = addr;
				CurrSection().load_address = addr;
				CurrSection().address = addr;
				CurrSection().address_assigned  = true;
				LinkLabelsToAddress(SectionId(), addr);	// in case any labels were defined prior to org & data
			} else
				SetSection(strref(), addr);
			break;
		}
		case AD_LOAD: {		// load: address for target to load code at
			int addr;
			if (line[0]=='=' || keyword_equ.is_prefix_word(line))
				line.next_word_ws();
			if ((error = EvalExpression(line, etx, addr))) {
				error = error == STATUS_NOT_READY ? ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;
				break;
			}
			CurrSection().SetLoadAddress(addr);
			break;
		}

		case AD_SECTION:
			line.trim_whitespace();
			SetSection(line);
			break;

		case AD_LINK:
			line.trim_whitespace();
			error = LinkSections(line);
			break;

		case AD_INCOBJ: {
			strref file = line.between('"', '"');
			if (!file)								// MERLIN: No quotes around PUT filenames
				file = line.split_range(filename_end_char_range);
			error = ReadObjectFile(file);
			break;
		}

		case AD_XDEF: {
			// this stores a string that when matched with a label will make that label external
			line.skip_whitespace();
			strref xdef = line.split_range(syntax == SYNTAX_MERLIN ? label_end_char_range_merlin : label_end_char_range);
			if (xdef) {
				char f = xdef.get_first();
				char e = xdef.get_last();
				if (f != '.' && f != '!' && f != '@' && e != '$') {
					unsigned int hash = xdef.fnv1a();
					unsigned int pos = FindLabelIndex(hash, xdefs.getKeys(), xdefs.count());
					while (pos < xdefs.count() && xdefs.getKey(pos) == hash) {
						if (xdefs.getValue(pos).same_str_case(xdef))
							return STATUS_OK;
						++pos;
					}
					xdefs.insert(pos, hash);
					xdefs.getValues()[pos] = xdef;
				}
			}
			break;
		}

		case AD_ALIGN:		// align: align address to multiple of value, fill space with 0
			if (line) {
				if (line[0]=='=' || keyword_equ.is_prefix_word(line))
					line.next_word_ws();
				int value;
				int status = EvalExpression(line, etx, value);
				if (status == STATUS_NOT_READY)
					error = ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY;
				else if (status == STATUS_OK && value>0) {
					if (CurrSection().address_assigned) {
						int add = (CurrSection().GetPC() + value-1) % value;
						for (int a = 0; a < add; a++)
							AddByte(0);
					} else
						CurrSection().align_address = value;
				}
			}
			break;
		case AD_EVAL: {		// eval: display the result of an expression in stdout
			int value = 0;
			strref description = line.find(':')>=0 ? line.split_token_trim(':') : strref();
			line.trim_whitespace();
			if (line && EvalExpression(line, etx, value) == STATUS_OK) {
				if (description) {
					printf("EVAL(%d): " STRREF_FMT ": \"" STRREF_FMT "\" = $%x\n",
						   contextStack.curr().source_file.count_lines(description)+1, STRREF_ARG(description), STRREF_ARG(line), value);
				} else {
					printf("EVAL(%d): \"" STRREF_FMT "\" = $%x\n",
						   contextStack.curr().source_file.count_lines(line)+1, STRREF_ARG(line), value);
				}
			} else if (description) {
				printf("EVAL(%d): \"" STRREF_FMT ": " STRREF_FMT"\"\n",
					   contextStack.curr().source_file.count_lines(description)+1, STRREF_ARG(description), STRREF_ARG(line));
			} else {
				printf("EVAL(%d): \"" STRREF_FMT "\"\n",
					   contextStack.curr().source_file.count_lines(line)+1, STRREF_ARG(line));
			}
			break;
		}
		case AD_BYTES:		// bytes: add bytes by comma separated values/expressions
			if (syntax==SYNTAX_MERLIN && line.get_first()=='#')	// MERLIN allows for an immediate declaration on data
				++line;
			while (strref exp = line.split_token_trim(',')) {
				int value;
				if (syntax==SYNTAX_MERLIN && exp.get_first()=='#')	// MERLIN allows for an immediate declaration on data
					++exp;
				error = EvalExpression(exp, etx, value);
				if (error>STATUS_NOT_READY)
					break;
				else if (error==STATUS_NOT_READY)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], exp, source_file, LateEval::LET_BYTE);
				else if (error == STATUS_RELATIVE_SECTION)
					CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset(), lastEvalSection,
						lastEvalPart == Reloc::HI_BYTE ? Reloc::HI_BYTE : Reloc::LO_BYTE);
				AddByte(value);
			}
			break;
		case AD_WORDS:		// words: add words (16 bit values) by comma separated values
			while (strref exp_w = line.split_token_trim(',')) {
				int value = 0;
				if (!CurrSection().IsDummySection()) {
					if (syntax==SYNTAX_MERLIN && exp_w.get_first()=='#')	// MERLIN allows for an immediate declaration on data
						++exp_w;
					error = EvalExpression(exp_w, etx, value);
					if (error>STATUS_NOT_READY)
						break;
					else if (error==STATUS_NOT_READY)
						AddLateEval(CurrSection().DataOffset(), CurrSection().DataOffset(), scope_address[scope_depth], exp_w, source_file, LateEval::LET_ABS_REF);
					else if (error == STATUS_RELATIVE_SECTION) {
						CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset(), lastEvalSection, lastEvalPart);
						value = 0;
					}
				}
				AddWord(value);
			}
			break;
		case AD_DC: {
			bool words = false;
			if (line[0]=='.') {
				++line;
				if (line[0]=='b' || line[0]=='B') {
				} else if (line[0]=='w' || line[0]=='W')
					words = true;
				else
					return ERROR_BAD_TYPE_FOR_DECLARE_CONSTANT;
				++line;
				line.skip_whitespace();
			}
			while (strref exp_dc = line.split_token_trim(',')) {
				int value = 0;
				if (!CurrSection().IsDummySection()) {
					if (syntax==SYNTAX_MERLIN && exp_dc.get_first()=='#')	// MERLIN allows for an immediate declaration on data
						++exp_dc;
					error = EvalExpression(exp_dc, etx, value);
					if (error > STATUS_NOT_READY)
						break;
					else if (error == STATUS_NOT_READY)
						AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], exp_dc, source_file, words ? LateEval::LET_ABS_REF : LateEval::LET_BYTE);
					else if (error == STATUS_RELATIVE_SECTION) {
						value = 0;
						if (words)
							CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset(), lastEvalSection, lastEvalPart);
						else 
							CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset(), lastEvalSection,
								lastEvalPart == Reloc::HI_BYTE ? Reloc::HI_BYTE : Reloc::LO_BYTE);
					}
				}
				AddByte(value);
				if (words)
					AddByte(value>>8);
			}
			break;
		}
		case AD_HEX: {
			unsigned char b = 0, v = 0;
			while (line) {	// indeterminable length, can't read hex to int
				char c = *line.get();
				++line;
				if (c == ',') {
					if (b) // probably an error but seems safe
						AddByte(v);
					b = 0;
					line.skip_whitespace();
				}
				else {
					if (c >= '0' && c <= '9') v = (v << 4) + (c - '0');
					else if (c >= 'A' && c <= 'Z') v = (v << 4) + (c - 'A' + 10);
					else if (c >= 'a' && c <= 'z') v = (v << 4) + (c - 'a' + 10);
					else break;
					b ^= 1;
					if (!b)
						AddByte(v);
				}
			}
			if (b)
				error = ERROR_HEX_WITH_ODD_NIBBLE_COUNT;
			break;
		}
		case AD_EJECT:
			line.clear();
			break;
		case AD_USR:
			line.clear();
			break;
		case AD_TEXT: {		// text: add text within quotes
			// for now just copy the windows ascii. TODO: Convert to petscii.
			// https://en.wikipedia.org/wiki/PETSCII
			// ascii: no change
			// shifted: a-z => $41.. A-Z => $61..
			// unshifted: a-z, A-Z => $41
			strref text_prefix = line.before('"').get_trimmed_ws();
			line = line.between('"', '"');
			CheckOutputCapacity(line.get_len());
			{
				if (!text_prefix || text_prefix.same_str("ascii")) {
					AddBin((unsigned const char*)line.get(), line.get_len());
				} else if (text_prefix.same_str("petscii")) {
					while (line) {
						char c = line[0];
						AddByte((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : (c > 0x60 ? ' ' : line[0]));
						++line;
					}
				} else if (text_prefix.same_str("petscii_shifted")) {
					while (line) {
						char c = line[0];
						AddByte((c >= 'a' && c <= 'z') ? (c - 'a' + 0x61) :
							((c >= 'A' && c <= 'Z') ? (c - 'A' + 0x61) : (c > 0x60 ? ' ' : line[0])));
						++line;
					}
				}
			}
			break;
		}
		case AD_MACRO: {	// macro: create an assembler macro
			strref read_source = contextStack.curr().read_source;
			if (read_source.is_substr(line.get())) {
				read_source.skip(strl_t(line.get()-read_source.get()));
				error = AddMacro(read_source, contextStack.curr().source_name,
								 contextStack.curr().source_file, read_source);
				contextStack.curr().next_source = read_source;
			}
			break;
		}
		case AD_INCLUDE: {	// include: assemble another file in place
			strref file = line.between('"', '"');
			if (!file)								// MERLIN: No quotes around PUT filenames
				file = line.split_range(filename_end_char_range);
			size_t size = 0;
			char *buffer = nullptr;
			if ((buffer = LoadText(file, size))) {
				loadedData.push_back(buffer);
				strref src(buffer, strl_t(size));
				contextStack.push(file, src, src);
			} else if (file[0]>='!' && file[0]<='&' && (buffer = LoadText(file+1, size))) {
				loadedData.push_back(buffer);		// MERLIN: prepend with !-& to not auto-prepend with T.
				strref src(buffer, strl_t(size));
				contextStack.push(file+1, src, src);
			} else {
				strown<512> fileadd(file[0]>='!' && file[0]<='&' ? (file+1) : file);
				fileadd.append(".s");
				if ((buffer = LoadText(fileadd.get_strref(), size))) {
					loadedData.push_back(buffer);	// MERLIN: !+filename appends .S to filenames
					strref src(buffer, strl_t(size));
					contextStack.push(file, src, src);
				} else {
					fileadd.copy("T.");				// MERLIN: just filename prepends T. to filenames
					fileadd.append(file[0]>='!' && file[0]<='&' ? (file+1) : file);
					if ((buffer = LoadText(fileadd.get_strref(), size))) {
						loadedData.push_back(buffer);
						strref src(buffer, strl_t(size));
						contextStack.push(file, src, src);
					}
				}
			}
			break;
		}
		case AD_INCBIN: {	// incbin: import binary data in place
			line = line.between('"', '"');
			strown<512> filename(line);
			size_t size = 0;
			if (char *buffer = LoadBinary(line, size)) {
				AddBin((const unsigned char*)buffer, (int)size);
				free(buffer);
			}
			break;
		}
		case AD_LABEL:
		case AD_CONST: {
			line.trim_whitespace();
			strref label = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
			if (line[0]=='=' || keyword_equ.is_prefix_word(line)) {
				line.next_word_ws();
				AssignLabel(label, line, dir==AD_CONST);
			} else
				error = ERROR_UNEXPECTED_LABEL_ASSIGMENT_FORMAT;
			break;
		}
		case AD_INCSYM: {
			IncludeSymbols(line);
			break;
		}
		case AD_LABPOOL: {
			strref label = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
			AddLabelPool(label, line);
			break;
		}
		case AD_IF:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
				bool conditional_result;
				error = EvalStatement(line, conditional_result);
				if (conditional_result)
					ConsumeConditional();
				else
					SetConditional();
			}
			break;
		case AD_IFDEF:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
				bool conditional_result;
				error = EvalStatement(line, conditional_result);
				if (GetLabel(line.get_trimmed_ws()) != nullptr)
					ConsumeConditional();
				else
					SetConditional();
			}
			break;
		case AD_ELSE:
			if (ConditionalAsm()) {
				if (ConditionalConsumed())
					ConditionalElse();
				else
					error = ERROR_ELSE_WITHOUT_IF;
			} else if (ConditionalAvail())
				EnableConditional(true);
			break;
		case AD_ELIF:
			if (ConditionalAsm()) {
				if (ConditionalConsumed())
					ConditionalElse();
				else
					error = ERROR_ELSE_WITHOUT_IF;
			}
			else if (ConditionalAvail()) {
				bool conditional_result;
				error = EvalStatement(line, conditional_result);
				EnableConditional(conditional_result);
			}
			break;
		case AD_ENDIF:
			if (ConditionalAsm()) {
				if (ConditionalConsumed())
					CloseConditional();
				else
					error = ERROR_ENDIF_WITHOUT_CONDITION;
			} else {
				conditional_nesting[conditional_depth]--;
				if (ConditionalAsm())
					CloseConditional();
			}
			break;
		case AD_ENUM:
		case AD_STRUCT: {
			strref read_source = contextStack.curr().read_source;
			if (read_source.is_substr(line.get())) {
				strref struct_name = line.get_word();
				line.skip(struct_name.get_len());
				line.skip_whitespace();
				read_source.skip(strl_t(line.get() - read_source.get()));
				if (read_source[0]=='{') {
					if (dir == AD_STRUCT)
						BuildStruct(struct_name, read_source.scoped_block_skip());
					else
						BuildEnum(struct_name, read_source.scoped_block_skip());
				} else
					error = dir == AD_STRUCT ? ERROR_STRUCT_CANT_BE_ASSEMBLED :
					ERROR_ENUM_CANT_BE_ASSEMBLED;
				contextStack.curr().next_source = read_source;
			} else
				error = ERROR_STRUCT_CANT_BE_ASSEMBLED;
			break;
		}
		case AD_REPT: {
			SourceContext &ctx = contextStack.curr();
			strref read_source = ctx.read_source;
			if (read_source.is_substr(line.get())) {
				read_source.skip(strl_t(line.get() - read_source.get()));
				int block = read_source.find('{');
				if (block<0) {
					error = ERROR_REPT_MISSING_SCOPE;
					break;
				}
				strref expression = read_source.get_substr(0, block);
				read_source += block;
				read_source.skip_whitespace();
				expression.trim_whitespace();
				int count;
				if (STATUS_OK != EvalExpression(expression, etx, count)) {
					error = ERROR_REPT_COUNT_EXPRESSION;
					break;
				}
				strref recur = read_source.scoped_block_skip();
				ctx.next_source = read_source;
				contextStack.push(ctx.source_name, ctx.source_file, recur, count);
			}
			break;
		}
		case AD_INCDIR:
			AddIncludeFolder(line.between('"', '"'));
			break;
		case AD_LST:
			line.clear();
			break;
		case AD_DUMMY:
			line.trim_whitespace();
			if (line) {
				int reorg;
				if (STATUS_OK == EvalExpression(line, etx, reorg)) {
					DummySection(reorg);
					break;
				}
			}
			DummySection();
			break;
		case AD_DUMMY_END:
			while (CurrSection().IsDummySection()) {
				EndSection();
				if (SectionId()==0)
					break;
			}
			break;
		case AD_DS: {
			int value;
			if (STATUS_OK != EvalExpression(line, etx, value))
				error = ERROR_DS_MUST_EVALUATE_IMMEDIATELY;
			else {
				if (value > 0) {
					for (int n = 0; n < value; n++)
						AddByte(0);
				} else
					CurrSection().AddAddress(value);
			}
			break;
		}
	}
	return error;
}

int sortHashLookup(const void *A, const void *B) {
	const OP_ID *_A = (const OP_ID*)A;
	const OP_ID *_B = (const OP_ID*)B;
	return _A->op_hash > _B->op_hash ? 1 : -1;
}

int BuildInstructionTable(OP_ID *pInstr, int maxInstructions)
{
	// create an instruction table (mnemonic hash lookup)
	int numInstructions = 0;
	for (int i = 0; i < num_opcodes_6502; i++) {
		OP_ID &op = pInstr[numInstructions++];
		op.op_hash = strref(opcodes_6502[i].instr).fnv1a_lower();
		op.index = i;
		op.type = OT_MNEMONIC;
	}

	// add assembler directives
	for (int d=0; d<nDirectiveNames; d++) {
		OP_ID &op_hash = pInstr[numInstructions++];
		op_hash.op_hash = strref(aDirectiveNames[d].name).fnv1a_lower();
//		op_hash.group = 0xff;
		op_hash.index = (unsigned char)aDirectiveNames[d].directive;
		op_hash.type = OT_DIRECTIVE;
	}
	
	// sort table by hash for binary search lookup
	qsort(pInstr, numInstructions, sizeof(OP_ID), sortHashLookup);
	return numInstructions;
}

AddrMode Asm::GetAddressMode(strref line, bool flipXY, StatusCode &error, strref &expression)
{
	bool force_zp = false;
	bool need_more = true;
	strref arg, deco;
	AddrMode addrMode = AMB_COUNT;
    
	while (need_more) {
		need_more = false;
		switch (line.get_first()) {
			case 0:		// empty line, empty addressing mode
				addrMode = AMB_NON;
				break;
			case '(':	// relative (jmp (addr), (zp,x), (zp),y)
				deco = line.scoped_block_skip();
				line.skip_whitespace();
				expression = deco.split_token_trim(',');
				addrMode = AMB_REL;
				if (deco[0]=='x' || deco[0]=='X')
					addrMode = AMB_ZP_REL_X;
				else if (line[0]==',') {
					++line;
					line.skip_whitespace();
					if (line[0]=='y' || line[0]=='Y') {
						addrMode = AMB_ZP_Y_REL;
						++line;
					}
				}
				break;
			case '#':	// immediate, determine if value is ok
				++line;
				addrMode = AMB_IMM;
				expression = line;
				break;
			default: {	// accumulator or absolute
				if (line) {
                    if (strref(".z").is_prefix_word(line)) {
                        force_zp = true;
                        line += 3;
                        need_more = true;
                    } else if (strref("A").is_prefix_word(line)) {
						addrMode = AMB_ACC;
					} else {	// absolute (zp, offs x, offs y)
						addrMode = force_zp ? AMB_ZP : AMB_ABS;
						expression = line.split_token_trim(',');
						bool relX = line && (line[0]=='x' || line[0]=='X');
						bool relY = line && (line[0]=='y' || line[0]=='Y');
						if ((flipXY && relY) || (!flipXY && relX))
							addrMode = addrMode==AMB_ZP ? AMB_ZP_X : AMB_ABS_X;
						else if ((flipXY && relX) || (!flipXY && relY)) {
							if (force_zp) {
								error = ERROR_INSTRUCTION_NOT_ZP;
								break;
							}
							addrMode = AMB_ABS_Y;
						}
					}
				}
				break;
			}
		}
	}
	return addrMode;
}



// Push an opcode to the output buffer
StatusCode Asm::AddOpcode(strref line, int index, strref source_file)
{
	StatusCode error = STATUS_OK;
	strref expression;
	
	// Get the addressing mode and the expression it refers to
	AddrMode addrMode = GetAddressMode(line,
		!!(opcodes_6502[index].modes & AMM_FLIPXY), error, expression);
	
	int value = 0;
	int target_section = -1;
	int target_section_offs = -1;
	Reloc::Type target_section_type = Reloc::NONE;
	bool evalLater = false;
	if (expression) {
		struct EvalContext etx(CurrSection().GetPC(), scope_address[scope_depth], -1,
			!!(opcodes_6502[index].modes & AMM_BRA) ? SectionId() : -1);
		error = EvalExpression(expression, etx, value);
		if (error == STATUS_NOT_READY) {
			evalLater = true;
			error = STATUS_OK;
		} else if (error == STATUS_RELATIVE_SECTION) {
			target_section = lastEvalSection;
			target_section_offs = lastEvalValue;
			target_section_type = lastEvalPart;
		} else if (error != STATUS_OK)
			return error;
	}
	
	// check if address is in zero page range and should use a ZP mode instead of absolute
	if (!evalLater && value>=0 && value<0x100 && error != STATUS_RELATIVE_SECTION) {
		switch (addrMode) {
			case AMB_ABS:
				if (opcodes_6502[index].modes & AMM_ZP)
					addrMode = AMB_ZP;
				break;
			case AMB_ABS_X:
				if (opcodes_6502[index].modes & AMM_ZP_X)
					addrMode = AMB_ZP_X;
				break;
			default:
				break;
		}
	}
	
	CODE_ARG codeArg = CA_NONE;
	unsigned char opcode = opcodes_6502[index].aCodes[addrMode];

	if (!(opcodes_6502[index].modes & (1 << addrMode)))
		error = ERROR_INVALID_ADDRESSING_MODE;
	else {
		if (opcodes_6502[index].modes & AMM_BRANCH)
			codeArg = CA_BRANCH;
		else if (addrMode == AMB_ABS || addrMode == AMB_REL || addrMode == AMB_ABS_X || addrMode == AMB_ABS_Y)
			codeArg = CA_TWO_BYTES;
		else if (addrMode != AMB_NON && addrMode != AMB_ACC)
			codeArg = CA_ONE_BYTE;
	}
	// Add the instruction and argument to the code
	if (error == STATUS_OK || error == STATUS_RELATIVE_SECTION) {
		CheckOutputCapacity(4);
		switch (codeArg) {
			case CA_BRANCH:
				AddByte(opcode);
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BRANCH);
				else if (((int)value - (int)CurrSection().GetPC()-1) < -128 || ((int)value - (int)CurrSection().GetPC()-1) > 127) {
					error = ERROR_BRANCH_OUT_OF_RANGE;
					break;
				}
				AddByte(evalLater ? 0 : (unsigned char)((int)value - (int)CurrSection().GetPC()) - 1);
				break;
			case CA_ONE_BYTE:
				AddByte(opcode);
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BYTE);
				else if (error == STATUS_RELATIVE_SECTION)
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section,
						target_section_type == Reloc::HI_BYTE ? Reloc::HI_BYTE : Reloc::LO_BYTE);
				AddByte(value);
				break;
			case CA_TWO_BYTES:
				AddByte(opcode);
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_ABS_REF);
				else if (error == STATUS_RELATIVE_SECTION) {
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(),
						target_section, target_section_type);
					value = 0;
				}
				AddWord(value);
				break;
			case CA_NONE:
				AddByte(opcode);
				break;
		}
	}
	return error;
}

// Build a line of code
void Asm::PrintError(strref line, StatusCode error)
{
	strown<512> errorText;
	if (contextStack.has_work()) {
		errorText.sprintf("Error " STRREF_FMT "(%d): ", STRREF_ARG(contextStack.curr().source_name),
						  contextStack.curr().source_file.count_lines(line)+1);
	} else
		errorText.append("Error: ");
	errorText.append(aStatusStrings[error]);
	errorText.append(" \"");
	errorText.append(line.get_trimmed_ws());
	errorText.append("\"\n");
	errorText.c_str();
	fwrite(errorText.get(), errorText.get_len(), 1, stderr);
	errorEncountered = true;
}

// Build a line of code
StatusCode Asm::BuildLine(OP_ID *pInstr, int numInstructions, strref line)
{
	StatusCode error = STATUS_OK;

	// MERLIN: First char of line is * means comment
	if (syntax==SYNTAX_MERLIN && line[0]=='*')
		return STATUS_OK;

	// remember for listing
	int start_section = SectionId();
	int start_address = CurrSection().address;
	strref code_line = line;
	bool built_opcode = false;
	while (line && error == STATUS_OK) {
		strref line_start = line;
		char char0 = line[0]; // first char including white space
		line.skip_whitespace();	// skip to first character
		line = line.before_or_full(';'); // clip any line comments
		line = line.before_or_full(c_comment);
		line.clip_trailing_whitespace();
		if (line[0]==':' && syntax!=SYNTAX_MERLIN)	// Kick Assembler macro prefix (incompatible with merlin)
			++line;
		strref line_nocom = line;
		strref operation = line.split_range(syntax==SYNTAX_MERLIN ? label_end_char_range_merlin : label_end_char_range);
		char char1 = operation[0];			// first char of first word
		char charE = operation.get_last();	// end char of first word

		line.trim_whitespace();
		bool force_label = charE==':' || charE=='$';
		if (!force_label && syntax==SYNTAX_MERLIN && line) // MERLIN fixes and PoP does some naughty stuff like 'and = 0'
			force_label = !strref::is_ws(char0) || char1==']' || charE=='?';
		else if (!force_label && syntax!=SYNTAX_MERLIN && line[0]==':')
			force_label = true;
		if (!operation && !force_label) {
			if (ConditionalAsm()) {
				// scope open / close
				switch (line[0]) {
					case '{':
						if (scope_depth>=(MAX_SCOPE_DEPTH-1))
							error = ERROR_TOO_DEEP_SCOPE;
						else {
							scope_address[++scope_depth] = CurrSection().GetPC();
							++line;
							line.skip_whitespace();
						}
						break;
					case '}':
						// check for late eval of anything with an end scope
						CheckLateEval(strref(), CurrSection().GetPC());
						FlushLocalLabels(scope_depth);
						FlushLabelPools(scope_depth);
						--scope_depth;
						if (scope_depth<0)
							error = ERROR_UNBALANCED_SCOPE_CLOSURE;
						++line;
						line.skip_whitespace();
						break;
					case '*':
						// if first char is '*' this seems like a line comment on some assemblers
						line.clear();
						break;
					case 127:
						++line;	// bad character?
						break;
				}
			}
		} else  {
			// ignore leading period for instructions and directives - not for labels
			strref label = operation;
			if ((syntax != SYNTAX_MERLIN && operation[0]==':') || operation[0]=='.')
				++operation;
			operation = operation.before_or_full('.');

			int op_idx = LookupOpCodeIndex(operation.fnv1a_lower(), pInstr, numInstructions);
			if (op_idx >= 0 && !force_label && (pInstr[op_idx].type==OT_DIRECTIVE || line[0]!='=')) {
				if (pInstr[op_idx].type==OT_DIRECTIVE) {
					if (line_nocom.is_substr(operation.get())) {
						line = line_nocom + strl_t(operation.get()+operation.get_len()-line_nocom.get());
						line.skip_whitespace();
					}
					error = ApplyDirective((AssemblerDirective)pInstr[op_idx].index, line, contextStack.curr().source_file);
				} else if (ConditionalAsm() && pInstr[op_idx].type == OT_MNEMONIC) {
					error = AddOpcode(line, pInstr[op_idx].index, contextStack.curr().source_file);
					built_opcode = true;
				}
				line.clear();
			} else if (!ConditionalAsm()) {
				line.clear(); // do nothing if conditional nesting so clear the current line
			} else if (line.get_first()=='=') {
				++line;
				error = AssignLabel(label, line);
				line.clear();
			} else if (keyword_equ.is_prefix_word(line)) {
				line += keyword_equ.get_len();
				line.skip_whitespace();
				error = AssignLabel(label, line);
				line.clear();
			} else {
				unsigned int nameHash = label.fnv1a();
				unsigned int macro = FindLabelIndex(nameHash, macros.getKeys(), macros.count());
				bool gotConstruct = false;
				while (macro < macros.count() && nameHash==macros.getKey(macro)) {
					if (macros.getValue(macro).name.same_str_case(label)) {
						error = BuildMacro(macros.getValue(macro), line);
						gotConstruct = true;
						line.clear();	// don't process codes from here
						break;
					}
					macro++;
				}
				if (!gotConstruct) {
					unsigned int labPool = FindLabelIndex(nameHash, labelPools.getKeys(), labelPools.count());
					gotConstruct = false;
					while (labPool < labelPools.count() && nameHash==labelPools.getKey(labPool)) {
						if (labelPools.getValue(labPool).pool_name.same_str_case(label)) {
							error = AssignPoolLabel(labelPools.getValue(labPool), line);
							gotConstruct = true;
							line.clear();	// don't process codes from here
							break;
						}
						labPool++;
					}
					if (!gotConstruct) {
						if (syntax==SYNTAX_MERLIN && strref::is_ws(line_start[0])) {
							error = ERROR_UNDEFINED_CODE;
						} else if (label[0]=='$' || strref::is_number(label[0]))
							line.clear();
						else {
							if (label.get_last()==':')
								label.clip(1);
							error = AddressLabel(label);
							line = line_start + int(label.get() + label.get_len() -line_start.get());
							if (line[0]==':' || line[0]=='?')
								++line;	// there may be codes after the label
						}
					}
				}
			}
		}
		// Check for unterminated condition in source
		if (!contextStack.curr().next_source &&
			(!ConditionalAsm() || ConditionalConsumed() || conditional_depth)) {
			if (syntax == SYNTAX_MERLIN) {	// this isn't a listed feature,
				conditional_nesting[0] = 0;	//	some files just seem to get away without closing
				conditional_consumed[0] = 0;
				conditional_depth = 0;
			} else {
				PrintError(conditional_source[conditional_depth], error);
				return ERROR_UNTERMINATED_CONDITION;
			}
		}
		
		if (line.same_str_case(line_start))
			error = ERROR_UNABLE_TO_PROCESS;
		
		if (error > STATUS_NOT_READY)
			PrintError(line_start, error);
		
		// dealt with error, continue with next instruction unless too broken
		if (error < ERROR_STOP_PROCESSING_ON_HIGHER)
			error = STATUS_OK;
	}
	// update listing
	if (error == STATUS_OK && list_assembly) {
		Section &curr = CurrSection();
		if (!curr.pListing)
			curr.pListing = new Listing;
		if (curr.pListing && curr.pListing->size() == curr.pListing->capacity())
			curr.pListing->reserve(curr.pListing->size() + 256);
		if (SectionId() == start_section) {
			if (curr.address != start_address && curr.size() && !curr.IsDummySection()) {
				struct ListLine lst;
				lst.address = start_address - curr.start_address;
				lst.size = curr.address - start_address;
				lst.code = contextStack.curr().source_file;
				lst.source_name = contextStack.curr().source_name;
				lst.line_offs = int(code_line.get() - lst.code.get());
				lst.was_mnemonic = built_opcode;
				curr.pListing->push_back(lst);
			}
		}
	}
	return error;
}

// Build a segment of code (file or macro)
StatusCode Asm::BuildSegment(OP_ID *pInstr, int numInstructions)
{
	StatusCode error = STATUS_OK;
	while (contextStack.curr().read_source) {
		contextStack.curr().next_source = contextStack.curr().read_source;
		error = BuildLine(pInstr, numInstructions, contextStack.curr().next_source.line());
		if (error > ERROR_STOP_PROCESSING_ON_HIGHER)
			break;
		contextStack.curr().read_source = contextStack.curr().next_source;
	}
	if (error == STATUS_OK) {
		error = CheckLateEval(strref(), CurrSection().GetPC());
	}
	return error;
}

// Produce the assembler listing
bool Asm::List(strref filename)
{
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f)
			return false;
		opened = true;
	}

	// Build a disassembly lookup table
	unsigned char mnemonic[256];
	unsigned char addrmode[256];
	memset(mnemonic, 255, sizeof(mnemonic));
	memset(addrmode, 255, sizeof(addrmode));
	for (int i = 0; i < num_opcodes_6502; i++) {
		for (int j = 0; j < AMB_COUNT; j++) {
			if (opcodes_6502[i].modes & (1 << j)) {
				unsigned char op = opcodes_6502[i].aCodes[j];
				mnemonic[op] = i;
				addrmode[op] = j;
			}
		}
	}

	strref prev_src;
	int prev_offs = 0;
	for (std::vector<Section>::iterator si = allSections.begin(); si != allSections.end(); ++si) {
		if (!si->pListing)
			continue;
		for (Listing::iterator li = si->pListing->begin(); li != si->pListing->end(); ++li) {
			strown<256> out;
			const struct ListLine &lst = *li;
			if (prev_src.fnv1a() != lst.source_name.fnv1a() || lst.line_offs < prev_offs) {
				fprintf(f, STRREF_FMT "(%d):\n", STRREF_ARG(lst.source_name), lst.code.count_lines(lst.line_offs));
				prev_src = lst.source_name;
			}
			else {
				strref prvline = lst.code.get_substr(prev_offs, lst.line_offs - prev_offs);
				prvline.next_line();
				if (prvline.count_lines() < 5) {
					while (strref space_line = prvline.line()) {
						space_line.clip_trailing_whitespace();
						strown<128> line_fix(space_line);
						for (strl_t pos = 0; pos < line_fix.len(); ++pos) {
							if (line_fix[pos] == '\t')
								line_fix.exchange(pos, 1, pos & 1 ? strref(" ") : strref("  "));
						}
						out.append_to(' ', 30);
						out.append(line_fix.get_strref());
						fprintf(f, STRREF_FMT "\n", STRREF_ARG(out));
						out.clear();
					}
				}
				else {
					fprintf(f, STRREF_FMT "(%d):\n", STRREF_ARG(lst.source_name), lst.code.count_lines(lst.line_offs));
				}
			}

			out.sprintf_append("$%04x ", lst.address + si->start_address);
			int s = lst.size < 4 ? lst.size : 4;
			if (si->output && si->output_capacity >= (lst.address + s)) {
				for (int b = 0; b < s; ++b)
					out.sprintf_append("%02x ", si->output[lst.address + b]);
			}
			out.append_to(' ', 18);
			if (lst.size && lst.was_mnemonic) {
				unsigned char *buf = si->output + lst.address;
				unsigned char op = mnemonic[*buf];
				unsigned char am = addrmode[*buf];
				if (op != 255 && am != 255) {
					const char *fmt = aAddrModeFmt[am];
					if (opcodes_6502[op].modes & AMM_FLIPXY) {
						if (am == AMB_ZP_X)	fmt = "%s $%02x,y";
						else if (am == AMB_ABS_X) fmt = "%s $%04x,y";
					}
					if (opcodes_6502[op].modes & AMM_BRANCH)
						out.sprintf_append(fmt, opcodes_6502[op].instr, (char)buf[1] + lst.address + si->start_address + 2);
					else if (am == AMB_NON || am == AMB_ACC)
						out.sprintf_append(fmt, opcodes_6502[op].instr);
					else if (am == AMB_ABS || am == AMB_ABS_X || am == AMB_ABS_Y || am == AMB_REL)
						out.sprintf_append(fmt, opcodes_6502[op].instr, buf[1] | (buf[2] << 8));
					else
						out.sprintf_append(fmt, opcodes_6502[op].instr, buf[1]);
				}
			}
			out.append_to(' ', 30);
			strref line = lst.code.get_skipped(lst.line_offs).get_line();
			line.clip_trailing_whitespace();
			strown<128> line_fix(line);
			for (strl_t pos = 0; pos < line_fix.len(); ++pos) {
				if (line_fix[pos] == '\t')
					line_fix.exchange(pos, 1, pos & 1 ? strref(" ") : strref("  "));
			}
			out.append(line_fix.get_strref());

			fprintf(f, STRREF_FMT "\n", STRREF_ARG(out));
			prev_offs = lst.line_offs;
		}
	}
	if (opened)
		fclose(f);
	return true;
}

// Create a listing of all valid instructions and addressing modes
bool Asm::AllOpcodes(strref filename)
{
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f)
			return false;
		opened = true;
	}
	for (int i = 0; i < num_opcodes_6502; i++) {
		for (int a = 0; a < AMB_COUNT; a++) {
			if (opcodes_6502[i].modes & (1 << a)) {
				const char *fmt = aAddrModeFmt[a];
				if (opcodes_6502[i].modes & AMM_BRANCH)
					fprintf(f, "%s *+%d", opcodes_6502[i].instr, 5);
				else {
					if (opcodes_6502[i].modes & AMM_FLIPXY) {
						if (a == AMB_ZP_X)	fmt = "%s $%02x,y";
						else if (a == AMB_ABS_X) fmt = "%s $%04x,y";
					}
					if (a==AMB_ABS || a==AMB_ABS_X || a==AMB_ABS_Y || a==AMB_REL)
						fprintf(f, fmt, opcodes_6502[i].instr, 0x2120);
					else
						fprintf(f, fmt, opcodes_6502[i].instr, 0x21, 0x20, 0x1f);
				}
				fputs("\n", f);
			}
		}
	}
	if (opened)
		fclose(f);
	return true;
}

// create an instruction table (mnemonic hash lookup + directives)
void Asm::Assemble(strref source, strref filename, bool obj_target)
{
	OP_ID *pInstr = new OP_ID[256];
	int numInstructions = BuildInstructionTable(pInstr, 256);

	StatusCode error = STATUS_OK;
	contextStack.push(filename, source, source);

	scope_address[scope_depth] = CurrSection().GetPC();
	while (contextStack.has_work()) {
		error = BuildSegment(pInstr, numInstructions);
		if (contextStack.curr().complete())
			contextStack.pop();
		else
			contextStack.curr().restart();
	}
	if (error == STATUS_OK) {
		error = CheckLateEval();
		if (error > STATUS_NOT_READY) {
			strown<512> errorText;
			errorText.copy("Error: ");
			errorText.append(aStatusStrings[error]);
			fwrite(errorText.get(), errorText.get_len(), 1, stderr);
		}

		if (!obj_target) {
			for (std::vector<LateEval>::iterator i = lateEval.begin(); i!=lateEval.end(); ++i) {
				strown<512> errorText;
				int line = i->source_file.count_lines(i->expression);
				errorText.sprintf("Error (%d): ", line+1);
				errorText.append("Failed to evaluate label \"");
				errorText.append(i->expression);
				if (line>=0) {
					errorText.append("\" : \"");
					errorText.append(i->source_file.get_line(line).get_trimmed_ws());
				}
				errorText.append("\"\n");
				fwrite(errorText.get(), errorText.get_len(), 1, stderr);
			}
		}
	}
}



//
//
// OBJECT FILE HANDLING
//
//

struct ObjFileHeader {
	short id;	// 'o6'
	short sections;
	short relocs;
	short labels;
	short late_evals;
	short map_symbols;
	unsigned int stringdata;
	int bindata;
};

struct ObjFileStr {
	int offs;				// offset into string table
};

struct ObjFileSection {
	enum SectionFlags {
		OFS_DUMMY,
		OFS_FIXED,
		OFS_MERGED,
	};
	struct ObjFileStr name;
	struct ObjFileStr exp_app;
	int start_address;
	int output_size;		// assembled binary size
	int align_address;
	short relocs;
	short flags;
};

struct ObjFileReloc {
	int base_value;
	int section_offset;
	short target_section;
	short value_type;		// Reloc::Type
};

struct ObjFileLabel {
	enum LabelFlags {
		OFL_EVAL = (1<<15),			// Evaluated (may still be relative)
		OFL_ADDR = (1<<14),			// Address or Assign
		OFL_CNST = (1<<13),			// Constant
		OFL_XDEF = OFL_CNST-1		// External (index into file array)
	};
	struct ObjFileStr name;
	int value;
	int flags;				// 1<<(LabelFlags)
	short section;			// -1 if resolved, file section # if section rel
	short mapIndex;			// -1 if resolved, index into map if relative
};

struct ObjFileLateEval {
	struct ObjFileStr label;
	struct ObjFileStr expression;
	struct ObjFileStr source_file;
	int address;			// PC relative to section or fixed
	short section;			// section to target
	short target;			// offset into section memory
	short scope;			// PC start of scope
	short type;				// label, byte, branch, word (LateEval::Type)
};

struct ObjFileMapSymbol {
	struct ObjFileStr name;	// symbol name
	int value;
	short section;
	bool local;				// local labels are probably needed
};

// Simple string pool, converts strref strings to zero terminated strings and returns the offset to the string in the pool.
static int _AddStrPool(const strref str, pairArray<unsigned int, int> *pLookup, char **strPool, unsigned int &strPoolSize, unsigned int &strPoolCap)
{
	if (!str.get() || !str.get_len())
		return -1;	// empty string
	unsigned int hash = str.fnv1a();
	unsigned int index = FindLabelIndex(hash, pLookup->getKeys(), pLookup->count());
	if (index<pLookup->count() && str.same_str_case(*strPool + pLookup->getValue(index)))
		return pLookup->getValue(index);

	if ((strPoolSize + str.get_len() + 1) > strPoolCap) {
		strPoolCap += 4096;
		char *strPoolGrow = (char*)malloc(strPoolCap);
		if (*strPool && strPoolGrow) {
			memcpy(strPoolGrow, *strPool, strPoolSize);
			free(*strPool);
		}
		*strPool = strPoolGrow;
	}
	int ret = strPoolSize;
	if (*strPool) {
		char *dest = *strPool + strPoolSize;
		memcpy(dest, str.get(), str.get_len());
		dest[str.get_len()] = 0;
		strPoolSize += str.get_len()+1;
		pLookup->insert(index, hash);
		pLookup->getValues()[index] = ret;
	}
	return ret;
}

StatusCode Asm::WriteObjectFile(strref filename)
{
	if (FILE *f = fopen(strown<512>(filename).c_str(), "wb")) {
		struct ObjFileHeader hdr = { 0 };
		hdr.id = 0x6f36;
		hdr.sections = (short)allSections.size();
		hdr.relocs = 0;
		hdr.bindata = 0;
		for (std::vector<Section>::iterator s = allSections.begin(); s!=allSections.end(); ++s) {
			if (s->pRelocs)
				hdr.relocs += short(s->pRelocs->size());
			hdr.bindata += (int)s->size();
		}
		hdr.labels = labels.count();
		hdr.late_evals = (short)lateEval.size();
		hdr.map_symbols = (short)map.size();
		hdr.stringdata = 0;

		// include space for external protected labels
		for (std::vector<ExtLabels>::iterator el = externals.begin(); el != externals.end(); ++el)
			hdr.labels += el->labels.count();

		char *stringPool = nullptr;
		unsigned int stringPoolCap = 0;
		pairArray<unsigned int, int> stringArray;
		stringArray.reserve(hdr.labels * 2 + hdr.sections + hdr.late_evals*2);

		struct ObjFileSection *aSects = hdr.sections ? (struct ObjFileSection*)calloc(hdr.sections, sizeof(struct ObjFileSection)) : nullptr;
		struct ObjFileReloc *aRelocs = hdr.relocs ? (struct ObjFileReloc*)calloc(hdr.relocs, sizeof(struct ObjFileReloc)) : nullptr;
		struct ObjFileLabel *aLabels = hdr.labels ? (struct ObjFileLabel*)calloc(hdr.labels, sizeof(struct ObjFileLabel)) : nullptr;
		struct ObjFileLateEval *aLateEvals = hdr.late_evals ? (struct ObjFileLateEval*)calloc(hdr.late_evals, sizeof(struct ObjFileLateEval)) : nullptr;
		struct ObjFileMapSymbol *aMapSyms = hdr.map_symbols ? (struct ObjFileMapSymbol*)calloc(hdr.map_symbols, sizeof(struct ObjFileMapSymbol)) : nullptr;
		int sect = 0, reloc = 0, labs = 0, late = 0, map_sym = 0;

		// write out sections and relocs
		if (hdr.sections) {
			for (std::vector<Section>::iterator si = allSections.begin(); si!=allSections.end(); ++si) {
				struct ObjFileSection &s = aSects[sect++];
				s.name.offs = _AddStrPool(si->name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				s.exp_app.offs = _AddStrPool(si->export_append, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				s.output_size = (short)si->size();
				s.align_address = si->align_address;
				s.relocs = si->pRelocs ? (short)(si->pRelocs->size()) : 0;
				s.start_address = si->start_address;
				s.flags =
					(si->IsDummySection() ? (1 << ObjFileSection::OFS_DUMMY) : 0) |
					(si->IsMergedSection() ? (1 << ObjFileSection::OFS_MERGED) : 0) |
					(si->address_assigned ? (1 << ObjFileSection::OFS_FIXED) : 0);
				if (si->pRelocs && si->pRelocs->size()) {
					for (relocList::iterator ri = si->pRelocs->begin(); ri!=si->pRelocs->end(); ++ri) {
						struct ObjFileReloc &r = aRelocs[reloc++];
						r.base_value = ri->base_value;
						r.section_offset = ri->section_offset;
						r.target_section = ri->target_section;
						r.value_type = ri->value_type;
					}
				}
			}
		}

		// write out labels
		if (hdr.labels) {
			for (unsigned int li = 0; li<labels.count(); li++) {
				Label &lo = labels.getValue(li);
				struct ObjFileLabel &l = aLabels[labs++];
				l.name.offs = _AddStrPool(lo.label_name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				l.value = lo.value;
				l.section = lo.section;
				l.mapIndex = lo.mapIndex;
				l.flags =
					(lo.constant ? ObjFileLabel::OFL_CNST : 0) |
					(lo.pc_relative ? ObjFileLabel::OFL_ADDR : 0) |
					(lo.evaluated ? ObjFileLabel::OFL_EVAL : 0) |
					(lo.external ? ObjFileLabel::OFL_XDEF : 0);
			}
		}
		
		// protected labels included from other object files
		if (hdr.labels) {
			int file_index = 1;
			for (std::vector<ExtLabels>::iterator el = externals.begin(); el != externals.end(); ++el) {
				for (unsigned int li = 0; li < el->labels.count(); ++li) {
					Label &lo = el->labels.getValue(li);
					struct ObjFileLabel &l = aLabels[labs++];
					l.name.offs = _AddStrPool(lo.label_name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
					l.value = lo.value;
					l.section = lo.section;
					l.mapIndex = lo.mapIndex;
					l.flags =
						(lo.constant ? ObjFileLabel::OFL_CNST : 0) |
						(lo.pc_relative ? ObjFileLabel::OFL_ADDR : 0) |
						(lo.evaluated ? ObjFileLabel::OFL_EVAL : 0) |
						file_index;
				}
				file_index++;
			}
		}

		// write out late evals
		if (aLateEvals) {
			for (std::vector<LateEval>::iterator lei = lateEval.begin(); lei != lateEval.end(); ++lei) {
				struct ObjFileLateEval &le = aLateEvals[late++];
				le.label.offs = _AddStrPool(lei->label, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				le.expression.offs = _AddStrPool(lei->expression, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				le.source_file.offs = _AddStrPool(lei->source_file, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				le.section = lei->section;
				le.target = (short)lei->target;
				le.address = lei->address;
				le.scope = lei->scope;
				le.type = lei->type;
			}
		}

		// write out map symbols
		if (aMapSyms) {
			for (MapSymbolArray::iterator mi = map.begin(); mi != map.end(); ++mi) {
				struct ObjFileMapSymbol &ms = aMapSyms[map_sym++];
				ms.name.offs = _AddStrPool(mi->name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				ms.value = mi->value;
				ms.local = mi->local;
				ms.section = mi->section;
			}
		}

		// write out the file
		fwrite(&hdr, sizeof(hdr), 1, f);
		fwrite(aSects, sizeof(aSects[0]), sect, f);
		fwrite(aRelocs, sizeof(aRelocs[0]), reloc, f);
		fwrite(aLabels, sizeof(aLabels[0]), labs, f);
		fwrite(aLateEvals, sizeof(aLateEvals[0]), late, f);
		fwrite(aMapSyms, sizeof(aMapSyms[0]), map_sym, f);
		fwrite(stringPool, hdr.stringdata, 1, f);
		for (std::vector<Section>::iterator si = allSections.begin(); si!=allSections.end(); ++si) {
			if (!si->IsDummySection() && !si->IsMergedSection() && si->size()!=0)
				fwrite(si->output, si->size(), 1, f);
		}

		// done with I/O
		fclose(f);

		if (stringPool)
			free(stringPool);
		if (aMapSyms)
			free(aMapSyms);
		if (aLateEvals)
			free(aLateEvals);
		if (aLabels)
			free(aLabels);
		if (aRelocs)
			free(aRelocs);
		if (aSects)
			free(aSects);
		stringArray.clear();

	}
	return STATUS_OK;
}

StatusCode Asm::ReadObjectFile(strref filename)
{
	size_t size;
	int file_index = (int)externals.size();
	if (char *data = LoadBinary(filename, size)) {
		struct ObjFileHeader &hdr = *(struct ObjFileHeader*)data;
		size_t sum = sizeof(hdr) + hdr.sections*sizeof(struct ObjFileSection) +
			hdr.relocs * sizeof(struct ObjFileReloc) + hdr.labels * sizeof(struct ObjFileLabel) +
			hdr.late_evals * sizeof(struct ObjFileLateEval) +
			hdr.map_symbols * sizeof(struct ObjFileMapSymbol) + hdr.stringdata + hdr.bindata;
		if (hdr.id == 0x6f36 && sum == size) {
			struct ObjFileSection *aSect = (struct ObjFileSection*)(&hdr + 1);
			struct ObjFileReloc *aReloc = (struct ObjFileReloc*)(aSect + hdr.sections);
			struct ObjFileLabel *aLabels = (struct ObjFileLabel*)(aReloc + hdr.relocs);
			struct ObjFileLateEval *aLateEval = (struct ObjFileLateEval*)(aLabels + hdr.labels);
			struct ObjFileMapSymbol *aMapSyms = (struct ObjFileMapSymbol*)(aLateEval + hdr.late_evals);
			const char *str_orig = (const char*)(aMapSyms + hdr.map_symbols);
			const char *bin_data = str_orig + hdr.stringdata;

			char *str_pool = (char*)malloc(hdr.stringdata);
			memcpy(str_pool, str_orig, hdr.stringdata);
			loadedData.push_back(str_pool);

			int prevSection = SectionId();

			short *aSctRmp = (short*)malloc(hdr.sections * sizeof(short));

			// for now just append to existing assembler data

			// sections
			int reloc_idx = 0;
			for (int si = 0; si < hdr.sections; si++) {
				short f = aSect[si].flags;
				aSctRmp[si] = (short)allSections.size();
				if (f & (1 << ObjFileSection::OFS_MERGED))
					continue;
				if (f & (1 << ObjFileSection::OFS_DUMMY)) {
					if (f&(1 << ObjFileSection::OFS_FIXED))
						DummySection(aSect[si].start_address);
					else
						DummySection();
				} else {
					if (f&(1 << ObjFileSection::OFS_FIXED))
						SetSection(aSect[si].name.offs>=0 ? strref(str_pool + aSect[si].name.offs) : strref(), aSect[si].start_address);
					else
						SetSection(aSect[si].name.offs >= 0 ? strref(str_pool + aSect[si].name.offs) : strref());
					CurrSection().export_append = aSect[si].exp_app.offs>=0 ? strref(str_pool + aSect[si].name.offs) : strref();
					CurrSection().align_address = aSect[si].align_address;
					CurrSection().address = CurrSection().start_address + aSect[si].output_size;
					if (aSect[si].output_size) {
						CurrSection().output = (unsigned char*)malloc(aSect[si].output_size);
						memcpy(CurrSection().output, bin_data, aSect[si].output_size);
						CurrSection().curr = CurrSection().output + aSect[si].output_size;
						CurrSection().output_capacity = aSect[si].output_size;

						bin_data += aSect[si].output_size;
					}
				}
			}

			for (int si = 0; si < hdr.sections; si++) {
				for (int r = 0; r < aSect[si].relocs; r++) {
					struct ObjFileReloc &rs = aReloc[reloc_idx++];
					allSections[aSctRmp[si]].AddReloc(rs.base_value, rs.section_offset, aSctRmp[rs.target_section], Reloc::Type(rs.value_type));
				}
			}

			for (int mi = 0; mi < hdr.map_symbols; mi++) {
				struct ObjFileMapSymbol &m = aMapSyms[mi];
				if (map.size() == map.capacity())
					map.reserve(map.size() + 256);
				MapSymbol sym;
				sym.name = m.name.offs>=0 ? strref(str_pool + m.name.offs) : strref();
				sym.section = m.section >=0 ? aSctRmp[m.section] : m.section;
				sym.value = m.value;
				sym.local = m.local;
				map.push_back(sym);
			}

			for (int li = 0; li < hdr.labels; li++) {
				struct ObjFileLabel &l = aLabels[li];
				strref name = l.name.offs >= 0 ? strref(str_pool + l.name.offs) : strref();
				Label *lbl = GetLabel(name);
				if (!lbl) {
					short f = l.flags;
					int external = f & ObjFileLabel::OFL_XDEF;
					if (external == ObjFileLabel::OFL_XDEF)
						lbl = AddLabel(name.fnv1a());	// insert shared label
					else {								// insert protected label
						while ((file_index + external) >= (int)externals.size()) {
							if (externals.size() == externals.capacity())
								externals.reserve(externals.size() + 32);
							externals.push_back(ExtLabels());
						}
						unsigned int hash = name.fnv1a();
						unsigned int index = FindLabelIndex(hash, externals[file_index].labels.getKeys(), externals[file_index].labels.count());
						externals[file_index].labels.insert(index, hash);
						lbl = externals[file_index].labels.getValues() + index;
					}
					lbl->label_name = name;
					lbl->pool_name.clear();
					lbl->value = l.value;
					lbl->evaluated = !!(f & ObjFileLabel::OFL_EVAL);
					lbl->constant = !!(f & ObjFileLabel::OFL_CNST);
					lbl->pc_relative = !!(f & ObjFileLabel::OFL_ADDR);
					lbl->external = external == ObjFileLabel::OFL_XDEF;
					lbl->section = l.section >= 0 ? aSctRmp[l.section] : l.section;
					lbl->mapIndex = l.mapIndex + (int)map.size();
				}
			}

			if (file_index==(int)externals.size())
				file_index = -1;	// no protected labels => don't track as separate file

			for (int li = 0; li < hdr.late_evals; ++li) {
				struct ObjFileLateEval &le = aLateEval[li];
				strref name = le.label.offs >= 0 ? strref(str_pool + le.label.offs) : strref();
				Label *pLabel = GetLabel(name);
				if (pLabel) {
					if (pLabel->evaluated) {
						AddLateEval(name, le.address, le.scope, strref(str_pool + le.expression.offs), (LateEval::Type)le.type);
						LateEval &last = lateEval[lateEval.size()-1];
						last.section = le.section >= 0 ? aSctRmp[le.section] : le.section;
						last.source_file = le.source_file.offs >= 0 ? strref(str_pool + le.source_file.offs) : strref();
						last.file_ref = file_index;
					}
				} else {
					AddLateEval(le.target, le.address, le.scope, strref(str_pool + le.expression.offs), strref(str_pool + le.source_file.offs), (LateEval::Type)le.type);
					LateEval &last = lateEval[lateEval.size()-1];
					last.file_ref = file_index;
				}
			}

			free(aSctRmp);

			// restore previous section
			current_section = &allSections[prevSection];
		} else
			return ERROR_NOT_AN_X65_OBJECT_FILE;

	}
	return STATUS_OK;
}


int main(int argc, char **argv)
{
	const strref listing("lst");
	const strref allinstr("opcodes");
	int return_value = 0;
	bool load_header = true;
	bool size_header = false;
	bool info = false;
	bool gen_allinstr = false;
	Asm assembler;

	const char *source_filename = nullptr, *obj_out_file = nullptr;
	const char *binary_out_name = nullptr;
	const char *sym_file = nullptr, *vs_file = nullptr;
	strref list_file, allinstr_file;
	for (int a=1; a<argc; a++) {
		strref arg(argv[a]);
		if (arg.get_first()=='-') {
			++arg;
			if (arg.get_first()=='i')
				assembler.AddIncludeFolder(arg+1);
			else if (arg.same_str("merlin"))
				assembler.syntax = SYNTAX_MERLIN;
			else if (arg.get_first()=='D' || arg.get_first()=='d') {
				++arg;
				if (arg.find('=')>0)
					assembler.AssignLabel(arg.before('='), arg.after('='));
				else
					assembler.AssignLabel(arg, "1");
			} else if (arg.same_str("c64")) {
				load_header = true;
				size_header = false;
			} else if (arg.same_str("a2b")) {
				load_header = true;
				size_header = true;
			} else if (arg.same_str("bin")) {
				load_header = false;
				size_header = false;
			} else if (arg.same_str("sect"))
				info = true;
			else if (arg.has_prefix(listing) && (arg.get_len() == listing.get_len() || arg[listing.get_len()] == '=')) {
				assembler.list_assembly = true;
				list_file = arg.after('=');
			} else if (arg.has_prefix(allinstr) && (arg.get_len() == allinstr.get_len() || arg[allinstr.get_len()] == '=')) {
				gen_allinstr = true;
				allinstr_file = arg.after('=');
			} else if (arg.same_str("sym") && (a + 1) < argc)
					sym_file = argv[++a];
				else if (arg.same_str("obj") && (a + 1) < argc)
					obj_out_file = argv[++a];
				else if (arg.same_str("vice") && (a + 1) < argc)
					vs_file = argv[++a];
			} else if (!source_filename)
				source_filename = arg.get();
			else if (!binary_out_name)
				binary_out_name = arg.get();
	}

	if (gen_allinstr) {
		assembler.AllOpcodes(allinstr_file);
	} else if (!source_filename) {
		puts(	"Usage:\n"
				" x65 filename.s code.prg [options]\n"
				"  * -i(path) : Add include path\n"
				"  * -D(label)[=<value>] : Define a label with an optional value(otherwise defined as 1)\n"
				"  * -obj(file.x65) : generate object file for later linking\n"
				"  * -bin : Raw binary\n"
				"  * -c64 : Include load address(default)\n"
				"  * -a2b : Apple II Dos 3.3 Binary\n"
				"  * -sym(file.sym) : symbol file\n"
				"  * -lst / -lst = (file.lst) : generate disassembly text from result(file or stdout)\n"
				"  * -opcodes / -opcodes = (file.s) : dump all available opcodes(file or stdout)\n"
				"  * -sect: display sections loaded and built\n"
				"  * -vice(file.vs) : export a vice symbol file\n");
		return 0;
	}

	// Load source
	if (source_filename) {
		size_t size = 0;
		strref srcname(source_filename);
		if (char *buffer = assembler.LoadText(srcname, size)) {
			// if source_filename contains a path add that as a search path for include files
			assembler.AddIncludeFolder(srcname.before_last('/', '\\'));

			assembler.Assemble(strref(buffer, strl_t(size)), srcname, obj_out_file != nullptr);
			
			if (assembler.errorEncountered)
				return_value = 1;
			else {
				// export object file
				if (obj_out_file)
					assembler.WriteObjectFile(obj_out_file);

				// if exporting binary, complete the build
				if (binary_out_name && !srcname.same_str(binary_out_name)) {
					strref binout(binary_out_name);
					strref ext = binout.after_last('.');
					if (ext)
						binout.clip(ext.get_len() + 1);
					strref aAppendNames[MAX_EXPORT_FILES];
					int numExportFiles = assembler.GetExportNames(aAppendNames, MAX_EXPORT_FILES);
					for (int e = 0; e < numExportFiles; e++) {
						strown<512> file(binout);
						file.append(aAppendNames[e]);
						file.append('.');
						file.append(ext);
						int size;
						int addr;
						if (unsigned char *buf = assembler.BuildExport(aAppendNames[e], size, addr)) {
							if (FILE *f = fopen(file.c_str(), "wb")) {
								if (load_header) {
									char load_addr[2] = { (char)addr, (char)(addr >> 8) };
									fwrite(load_addr, 2, 1, f);
								}
								if (size_header) {
									char byte_size[2] = { (char)size, (char)(size >> 8) };
									fwrite(byte_size, 2, 1, f);
								}
								fwrite(buf, size, 1, f);
								fclose(f);
							}
							free(buf);
						}
					}
				}

				// print encountered sections info
				if (info) {
					printf("SECTIONS SUMMARY\n================\n");
					for (size_t i = 0; i < assembler.allSections.size(); ++i) {
						Section &s = assembler.allSections[i];
						if (s.address > s.start_address) {
							printf("Section %d: \"" STRREF_FMT "\" Dummy: %s Relative: %s Merged: %s Start: 0x%04x End: 0x%04x\n",
								(int)i, STRREF_ARG(s.name), s.dummySection ? "yes" : "no",
								s.IsRelativeSection() ? "yes" : "no", s.IsMergedSection() ? "yes" : "no", s.start_address, s.address);
							if (s.pRelocs) {
								for (relocList::iterator i = s.pRelocs->begin(); i != s.pRelocs->end(); ++i)
									printf("\tReloc value $%x at offs $%x section %d\n", i->base_value, i->section_offset, i->target_section);
							}
						}
					}
				}

				// listing after export since addresses are now resolved
				if (assembler.list_assembly)
					assembler.List(list_file);

				// export .sym file
				if (sym_file && !srcname.same_str(sym_file) && !assembler.map.empty()) {
					if (FILE *f = fopen(sym_file, "w")) {
                        bool wasLocal = false;
                        for (MapSymbolArray::iterator i = assembler.map.begin(); i!=assembler.map.end(); ++i) {
                            unsigned int value = (unsigned int)i->value;
							int section = i->section;
							while (section >= 0 && section < (int)assembler.allSections.size()) {
								if (assembler.allSections[section].IsMergedSection()) {
									value += assembler.allSections[section].merged_offset;
									section = assembler.allSections[section].merged_section;
								} else {
									value += assembler.allSections[section].start_address;
									break;
								}
							}
							fprintf(f, "%s.label " STRREF_FMT " = $%04x",
									wasLocal==i->local ? "\n" : (i->local ? " {\n" : "\n}\n"),
									STRREF_ARG(i->name), value);
							wasLocal = i->local;
                        }
                        fputs(wasLocal ? "\n}\n" : "\n", f);
						fclose(f);
					}
				}

				// export vice label file
				if (vs_file && !srcname.same_str(vs_file) && !assembler.map.empty()) {
					if (FILE *f = fopen(vs_file, "w")) {
                        for (MapSymbolArray::iterator i = assembler.map.begin(); i!=assembler.map.end(); ++i) {
                            unsigned int value = (unsigned int)i->value;
							int section = i->section;
							while (section >= 0 && section < (int)assembler.allSections.size()) {
								if (assembler.allSections[section].IsMergedSection()) {
									value += assembler.allSections[section].merged_offset;
									section = assembler.allSections[section].merged_section;
								}
								else {
									value += assembler.allSections[section].start_address;
									break;
								}
							}
							fprintf(f, "al $%04x %s" STRREF_FMT "\n",
										value, i->name[0]=='.' ? "" : ".",
										STRREF_ARG(i->name));
                        }
						fclose(f);
					}
				}
			}
			// free some memory
			assembler.Cleanup();
		}
	}
	return return_value;
}
