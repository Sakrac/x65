//
//  x65.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/23/15.
//
//
//	A "simple" 6502/65C02/65816 assembler
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
#include <inttypes.h>
#include <assert.h>

#ifndef _WIN32
#define _strdup strdup
#endif

// Command line arguments
static const strref cmdarg_listing("lst");		// -lst / -lst=(file.lst) : generate disassembly text from result(file or stdout)
static const strref cmdarg_srcdebug("srcdbg");	// -srcdbg : generate debug, -srcdbg=(file) : save debug file
static const strref cmdarg_tass_listing("tsl");	// -tsl=(file) : generate listing file in TASS style
static const strref cmdarg_tass_labels("tl");	// -tl=(file) : generate labels in TASS style
static const strref cmdarg_allinstr("opcodes");	// -opcodes / -opcodes=(file.s) : dump all available opcodes(file or stdout)
static const strref cmdarg_endmacro("endm");	// -endm : macros end with endm or endmacro instead of scoped('{' - '}')
static const strref cmdarg_cpu("cpu");			// declare CPU type, use with argument: -cpu=6502/65c02/65c02wdc/65816
static const strref cmdarg_acc("acc");			// [65816] -acc=8/16: set the accumulator mode for 65816 at start, default is 8 bits
static const strref cmdarg_xy("xy");			// [65816] -xy=8/16: set the index register mode for 65816 at start, default is 8 bits
static const strref cmdarg_org("org");			// -org = $2000 or - org = 4096: force fixed address code at address
static const strref cmdarg_kickasm("kickasm");	// -kickasm: use Kick Assembler syntax
static const strref cmdarg_merlin("merlin");	// -merlin: use Merlin syntax
static const strref cmdarg_c64("c64");			// -c64 : Include load address(default)
static const strref cmdarg_a2b("a2b");			// -a2b : Apple II Dos 3.3 Binary
static const strref cmdarg_bin("bin");			// -bin : Produce raw binary\n"
static const strref cmdarg_a2p("a2p");			// -a2p : Apple II ProDos Binary
static const strref cmdarg_a2o("a2o");			// -a2o : Apple II GS OS executable (relocatable)
static const strref cmdarg_mrg("mrg");			// -mrg : Force merge all sections (use with -a2o)
static const strref cmdarg_sect("sect");		// -sect: display sections loaded and built
static const strref cmdarg_sym("sym");			// -sym (file.sym) : generate symbol file
static const strref cmdarg_symfull("symfull");			// -sym (file.sym) : generate symbol file
static const strref cmdarg_obj("obj");			// -obj (file.x65) : generate object file for later linking
static const strref cmdarg_vice("vice");		// -vice (file.vs) : export a vice symbol file
static const strref cmdarg_xrefimp("xrefimp");	// -xrefimp : import directive means xref, not include/incbin
static const strref cmdarg_references("refs");	// -refs: show label dependencies before linking

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
#define MAX_POOL_BYTES 255

// Max number of exported binary files from a single source
#define MAX_EXPORT_FILES 64

// Maximum number of opcodes, aliases and directives
#define MAX_OPCODES_DIRECTIVES 320

// minor variation of 6502
#define NUM_ILLEGAL_6502_OPS 21

// minor variation of 65C02
#define NUM_WDC_65C02_SPECIFIC_OPS 18


// To simplify some syntax disambiguation the preferred
// ruleset can be specified on the command line.
enum AsmSyntax {
	SYNTAX_SANE,
	SYNTAX_KICKASM,
	SYNTAX_MERLIN
};

// Internal status and error type
enum StatusCode {
	STATUS_OK,			// everything is fine
	STATUS_RELATIVE_SECTION, // value is relative to a single section
	STATUS_NOT_READY,	// label could not be evaluated at this time
	STATUS_XREF_DEPENDENT,	// evaluated but relied on an XREF label to do so
	STATUS_NOT_STRUCT,	// return is not a struct.
	STATUS_EXPORT_NO_CODE_OR_DATA_SECTION,
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
	ERROR_COULD_NOT_INCLUDE_FILE,
	ERROR_PULL_WITHOUT_PUSH,
	ERROR_USER,

	ERROR_STOP_PROCESSING_ON_HIGHER,	// errors greater than this will stop execution

	ERROR_BRANCH_OUT_OF_RANGE,
	ERROR_INCOMPLETE_FUNCTION,
	ERROR_FUNCTION_DID_NOT_RESOLVE,
	ERROR_EXPRESSION_RECURSION,
	ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY,
	ERROR_TOO_DEEP_SCOPE,
	ERROR_UNBALANCED_SCOPE_CLOSURE,
	ERROR_BAD_MACRO_FORMAT,
	ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY,
	ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION,
	ERROR_MACRO_ARGUMENT,
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
	ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE,
	ERROR_NOT_A_SECTION,
	ERROR_CANT_REASSIGN_FIXED_SECTION,
	ERROR_CANT_LINK_ZP_AND_NON_ZP,
	ERROR_OUT_OF_MEMORY,
	ERROR_CANT_WRITE_TO_FILE,
	ERROR_ABORTED,
	ERROR_CONDITION_TOO_NESTED,

	STATUSCODE_COUNT
};

// The following strings are in the same order as StatusCode
const char *aStatusStrings[STATUSCODE_COUNT] = {
	"ok",														// STATUS_OK,			// everything is fine
	"relative section",											// STATUS_RELATIVE_SECTION, // value is relative to a single section
	"not ready",												// STATUS_NOT_READY,	// label could not be evaluated at this time
	"XREF dependent result",									// STATUS_XREF_DEPENDENT,	// evaluated but relied on an XREF label to do so
	"name is not a struct",										// STATUS_NOT_STRUCT,	// return is not a struct.
	"Exporting binary without code or data section",			// STATUS_EXPORT_NO_CODE_OR_DATA_SECTION,
	"Undefined code",											// ERROR_UNDEFINED_CODE = FIRST_ERROR,
	"Unexpected character in expression",						// ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION,
	"Too many values in expression",							// ERROR_TOO_MANY_VALUES_IN_EXPRESSION,
	"Too many operators in expression",							// ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION,
	"Unbalanced right parenthesis in expression",				// ERROR_UNBALANCED_RIGHT_PARENTHESIS,
	"Expression operation",										// ERROR_EXPRESSION_OPERATION,
	"Expression missing values",								// ERROR_EXPRESSION_MISSING_VALUES,
	"Instruction can not be zero page",							// ERROR_INSTRUCTION_NOT_ZP,
	"Invalid addressing mode for instruction",					// ERROR_INVALID_ADDRESSING_MODE,
	"Internal label organization mishap",						// ERROR_LABEL_MISPLACED_INTERNAL,
	"Bad addressing mode",										// ERROR_BAD_ADDRESSING_MODE,
	"Unexpected character in addressing mode",					// ERROR_UNEXPECTED_CHARACTER_IN_ADDRESSING_MODE,
	"Unexpected label assignment format",						// ERROR_UNEXPECTED_LABEL_ASSIGMENT_FORMAT,
	"Changing value of label that is constant",					// ERROR_MODIFYING_CONST_LABEL,
	"Out of labels in pool",									// ERROR_OUT_OF_LABELS_IN_POOL,
	"Internal label pool release confusion",					// ERROR_INTERNAL_LABEL_POOL_ERROR,
	"Label pool range evaluation failed",						// ERROR_POOL_RANGE_EXPRESSION_EVAL,
	"Label pool was redeclared within its scope",				// ERROR_LABEL_POOL_REDECLARATION,
	"Pool label already defined",								// ERROR_POOL_LABEL_ALREADY_DEFINED,
	"Struct already defined",									// ERROR_STRUCT_ALREADY_DEFINED,
	"Referenced struct not found",								// ERROR_REFERENCED_STRUCT_NOT_FOUND,
	"Declare constant type not recognized (dc.?)",				// ERROR_BAD_TYPE_FOR_DECLARE_CONSTANT,
	"rept count expression could not be evaluated",				// ERROR_REPT_COUNT_EXPRESSION,
	"hex must be followed by an even number of hex numbers",	// ERROR_HEX_WITH_ODD_NIBBLE_COUNT,
	"DS directive failed to evaluate immediately",				// ERROR_DS_MUST_EVALUATE_IMMEDIATELY,
	"File is not a valid x65 object file",						// ERROR_NOT_AN_X65_OBJECT_FILE,
	"Failed to read include file",								// ERROR_COULD_NOT_INCLUDE_FILE,
	"Using symbol PULL without first using a PUSH",				// ERROR_PULL_WITHOUT_PUSH
	"User invoked error",										// ERROR_USER,

	"Errors after this point will stop execution",				// ERROR_STOP_PROCESSING_ON_HIGHER,	// errors greater than this will stop execution

	"Branch is out of range",													// ERROR_BRANCH_OUT_OF_RANGE,
	"Function declaration is missing name or expression",						// ERROR_INCOMPLETE_FUNCTION,
	"Function could not resolve the expression",								// ERROR_FUNCTION_DID_NOT_RESOLVE
	"Expression evaluateion recursion too deep",								// ERROR_EXPRESSION_RECURSION
	"Target address must evaluate immediately for this operation",				// ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY,
	"Scoping is too deep",														// ERROR_TOO_DEEP_SCOPE,
	"Unbalanced scope closure",													// ERROR_UNBALANCED_SCOPE_CLOSURE,
	"Unexpected macro formatting",												// ERROR_BAD_MACRO_FORMAT,
	"Align must evaluate immediately",											// ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY,
	"Out of memory for macro expansion",										// ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION,
	"Problem with macro argument",												// ERROR_MACRO_ARGUMENT,
	"Conditional could not be resolved",										// ERROR_CONDITION_COULD_NOT_BE_RESOLVED,
	"#endif encountered outside conditional block",								// ERROR_ENDIF_WITHOUT_CONDITION,
	"#else or #elif outside conditional block",									// ERROR_ELSE_WITHOUT_IF,
	"Struct can not be assembled as is",										// ERROR_STRUCT_CANT_BE_ASSEMBLED,
	"Enum can not be assembled as is",											// ERROR_ENUM_CANT_BE_ASSEMBLED,
	"Conditional assembly (#if/#ifdef) was not terminated in file or macro",	// ERROR_UNTERMINATED_CONDITION,
	"rept is missing a scope ('{ ... }')",										// ERROR_REPT_MISSING_SCOPE,
	"Link can only be used in a fixed address section",							// ERROR_LINKER_MUST_BE_IN_FIXED_ADDRESS_SECTION,
	"Link can not be used in dummy sections",									// ERROR_LINKER_CANT_LINK_TO_DUMMY_SECTION,
	"Can not process this line",												// ERROR_UNABLE_TO_PROCESS,
	"Unexpected target offset for reloc or late evaluation",					// ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE,
	"CPU is not supported",														// ERROR_CPU_NOT_SUPPORTED,
	"Can't append sections",													// ERROR_CANT_APPEND_SECTION_TO_TARGET,
	"Zero page / Direct page section out of range",								// ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE,
	"Attempting to assign an address to a non-existent section",				// ERROR_NOT_A_SECTION,
	"Attempting to assign an address to a fixed address section",				// ERROR_CANT_REASSIGN_FIXED_SECTION,
	"Can not link a zero page section with a non-zp section",					// ERROR_CANT_LINK_ZP_AND_NON_ZP,
	"Out of memory while building",												// ERROR_OUT_OF_MEMORY,
	"Can not write to file",													// ERROR_CANT_WRITE_TO_FILE,
	"Assembly aborted",															// ERROR_ABORTED,
	"Condition too deeply nested",												// ERROR_CONDITION_TOO_NESTED,
};

// Assembler directives
enum AssemblerDirective {
	AD_CPU,			// CPU: Assemble for this target,
	AD_ORG,			// ORG: Assemble as if loaded at this address
	AD_EXPORT,		// EXPORT: export this section or disable export
	AD_LOAD,		// LOAD: If applicable, instruct to load at this address
	AD_SECTION,		// SECTION: Enable code that will be assigned a start address during a link step
	AD_MERGE,		// MERGE: Merge named sections in order listed
	AD_LINK,		// LINK: Put sections with this name at this address (must be ORG / fixed address section)
	AD_XDEF,		// XDEF: Externally declare a symbol
	AD_XREF,		// XREF: Reference an external symbol
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
	AD_IMPORT,		// IMPORT: Include or Incbin or Incobj or Incsym
	AD_CONST,		// CONST: Prevent a label from mutating during assemble
	AD_LABEL,		// LABEL: Create a mutable label (optional)
	AD_STRING,		// STRING: Declare a string symbol
	AD_FUNCTION,	// FUNCTION: Declare a user defined function
	AD_UNDEF,		// UNDEF: remove a string or a label
	AD_INCSYM,		// INCSYM: Reference labels from another assemble
	AD_LABPOOL,		// POOL: Create a pool of addresses to assign as labels dynamically
	AD_IF,			// #IF: Conditional assembly follows based on expression
	AD_IFDEF,		// #IFDEF: Conditional assembly follows based on label defined or not
	AD_IFNDEF,		// #IFNDEF: Conditional assembly inverted from IFDEF
	AD_IFCONST,		// #IFCONST: Conditional assembly follows based on label being const
	AD_IFBLANK,		// #IFBLANK: Conditional assembly follows based on rest of line empty
	AD_IFNBLANK,	// #IFNBLANK: Conditional assembly follows based on rest of line not empty
	AD_ELSE,		// #ELSE: Otherwise assembly
	AD_ELIF,		// #ELIF: Otherwise conditional assembly follows
	AD_ENDIF,		// #ENDIF: End a block of #IF/#IFDEF
	AD_STRUCT,		// STRUCT: Declare a set of labels offset from a base address
	AD_ENUM,		// ENUM: Declare a set of incremental labels
	AD_REPT,		// REPT: Repeat the assembly of the bracketed code a number of times
	AD_INCDIR,		// INCDIR: Add a folder to search for include files
	AD_A16,			// A16: Set 16 bit accumulator mode
	AD_A8,			// A8: Set 8 bit accumulator mode
	AD_XY16,		// A16: Set 16 bit index register mode
	AD_XY8,			// A8: Set 8 bit index register mode
	AD_HEX,			// HEX: LISA assembler data block
	AD_ABORT,		// ABORT: stop assembler and error
	AD_EJECT,		// EJECT: Page break for printing assembler code, ignore
	AD_LST,			// LST: Controls symbol listing
	AD_DUMMY,		// DUM: Start a dummy section (increment address but don't write anything???)
	AD_DUMMY_END,	// DEND: End a dummy section
	AD_SCOPE,		// SCOPE: Begin ca65 style scope
	AD_ENDSCOPE,	// ENDSCOPR: End ca65 style scope
	AD_PUSH,		// PUSH: Push the value of a variable symbol on a stack
	AD_PULL,		// PULL: Pull the value of a variable symbol from its stack, must be pushed first
	AD_DS,			// DS: Define section, zero out # bytes or rewind the address if negative
	AD_USR,			// USR: MERLIN user defined pseudo op, runs some code at a hard coded address on apple II, on PC does nothing.
	AD_SAV,			// SAV: MERLIN version of export but contains full filename, not an appendable name
	AD_XC,			// XC: MERLIN version of setting CPU
	AD_MX,			// MX: MERLIN control accumulator 16 bit mode
	AD_LNK,			// LNK: MERLIN load object and link
	AD_ADR,			// ADR: MERLIN store 3 byte word
	AD_ADRL,		// ADRL: MERLIN store 4 byte word
	AD_ENT,			// ENT: MERLIN extern this address label
	AD_EXT,			// EXT: MERLIN reference this address label from a different file
	AD_CYC,			// CYC: MERLIN start / stop cycle timer
	AD_DBL_BYTES,	// DDB: MERLIN Store 2 bytes, big endian format.
	AD_ERROR,
};

// evaluation functions
enum EvalFuncs {
	EF_DEFINED,		// DEFINED(label) 1 if label is defined
	EF_REFERENCED,	// REFERENCED(label) 1 if label has been referenced in this file
	EF_BLANK,		// BLANK() 1 if the contents within the parenthesis is empty
	EF_CONST,		// CONST(label) 1 if label is a const label
	EF_SIZEOF,		// SIZEOF(struct) returns size of structs
	EF_SIN,			// SIN(index, period, amplitude)
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
	EVOP_VAL = 'a',	// a, value => read from value queue
	EVOP_EQU,		// b, 1 if left equal to right otherwise 0
	EVOP_LT,		// c, 1 if left less than right otherwise 0
	EVOP_GT,		// d, 1 if left greater than right otherwise 0
	EVOP_LTE,		// e, 1 if left less than or equal to right otherwise 0
	EVOP_GTE,		// f, 1 if left greater than or equal to right otherwise 0
	EVOP_LOB,		// g, low byte of 16 bit value
	EVOP_HIB,		// h, high byte of 16 bit value
	EVOP_BAB,		// i, bank byte of 24 bit value
	EVOP_LPR,		// j, left parenthesis
	EVOP_RPR,		// k, right parenthesis
	EVOP_ADD,		// l, +
	EVOP_SUB,		// m, -
	EVOP_MUL,		// n, * (note: if not preceded by value or right paren this is current PC)
	EVOP_DIV,		// o, /
	EVOP_AND,		// p, &
	EVOP_OR,		// q, |
	EVOP_EOR,		// r, ^
	EVOP_SHL,		// s, <<
	EVOP_SHR,		// t, >>
	EVOP_NOT,		// u, ~
	EVOP_NEG,		// v, negate value
	EVOP_STP,		// w, Unexpected input, should stop and evaluate what we have
	EVOP_NRY,		// x, Not ready yet
	EVOP_XRF,		// y, value from XREF label
	EVOP_EXP,		// z, sub expression
	EVOP_ERR,		// z+1, Error
};

// Opcode encoding
typedef struct sOPLookup {
	uint32_t op_hash;
	uint8_t index;	// ground index
	uint8_t type;	// mnemonic or
} OPLookup;

enum AddrMode {
	// address mode bit index

	// 6502

	AMB_ZP_REL_X,		// 0 ($12,x)
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

	// 65C02

	AMB_ZP_REL,			// b ($12)
	AMB_REL_X,			// c ($1234,x)
	AMB_ZP_ABS,			// d $12, *+$12

	// 65816

	AMB_ZP_REL_L,		// e [$02]
	AMB_ZP_REL_Y_L,		// f [$00],y
	AMB_ABS_L,			// 10 $bahilo
	AMB_ABS_L_X,		// 11 $123456,x
	AMB_STK,			// 12 $12,s
	AMB_STK_REL_Y,		// 13 ($12,s),y
	AMB_REL_L,			// 14 [$1234]
	AMB_BLK_MOV,		// 15 $12,$34
	AMB_COUNT,

	AMB_FLIPXY = AMB_COUNT,	// 16 (indexing index using y treat as x address mode)
	AMB_BRANCH,				// 17 (relative address 8 bit)
	AMB_BRANCH_L,			// 18 (relative address 16 bit)
	AMB_IMM_DBL_A,			// 19 (immediate mode can be doubled in 16 bit mode)
	AMB_IMM_DBL_XY,			// 1a (immediate mode can be doubled in 16 bit mode)

	AMB_ILL,			// 1b illegal address mode

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
	AMM_ZP_REL = 1<<AMB_ZP_REL,			// b ($12)
	AMM_REL_X = 1<<AMB_REL_X,			// c ($1234,x)
	AMM_ZP_ABS = 1<<AMB_ZP_ABS,			// d $12, *+$12

	AMM_ZP_REL_L = 1<<AMB_ZP_REL_L,		// e [$02]
	AMM_ZP_REL_Y_L = 1<<AMB_ZP_REL_Y_L,	// f [$00],y
	AMM_ABS_L = 1<<AMB_ABS_L,			// 10 $bahilo
	AMM_ABS_L_X = 1<<AMB_ABS_L_X,		// 11 $123456,x
	AMM_STK = 1<<AMB_STK,				// 12 $12,s
	AMM_STK_REL_Y = 1<<AMB_STK_REL_Y,	// 13 ($12,s),y
	AMM_REL_L = 1<<AMB_REL_L,			// 14 [$1234]
	AMM_BLK_MOV = 1<<AMB_BLK_MOV,		// 15 $12,$34


	AMM_FLIPXY = 1<<AMB_FLIPXY,
	AMM_BRANCH = 1<<AMB_BRANCH,
	AMM_BRANCH_L = 1<<AMB_BRANCH_L,
	AMM_IMM_DBL_A = 1<<AMB_IMM_DBL_A,
	AMM_IMM_DBL_XY = 1<<AMB_IMM_DBL_XY,

	// instruction group specific masks
	AMM_BRK = AMM_NON | AMM_IMM,
	AMM_BRA = AMM_BRANCH | AMM_ABS,
	AMM_ORA = AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_Y | AMM_ABS_X | AMM_ZP_REL_X | AMM_ZP_Y_REL,
	AMM_STA = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_Y | AMM_ABS_X | AMM_ZP_REL_X | AMM_ZP_Y_REL,
	AMM_ASL = AMM_ACC | AMM_NON | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMM_STX = AMM_FLIPXY | AMM_ZP | AMM_ZP_X | AMM_ABS, // note: for x ,x/,y flipped for this instr.
	AMM_LDX = AMM_FLIPXY | AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X, // note: for x ,x/,y flipped for this instr.
	AMM_STY = AMM_ZP | AMM_ZP_X | AMM_ABS,
	AMM_LDY = AMM_IMM | AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMM_DEC = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMM_BIT = AMM_ZP | AMM_ABS,
	AMM_JMP = AMM_ABS | AMM_REL,
	AMM_CPY = AMM_IMM | AMM_ZP | AMM_ABS,

	// 6502 illegal modes
	AMM_SLO = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_Y | AMM_ABS_X | AMM_ZP_REL_X | AMM_ZP_Y_REL,
	AMM_AXS = AMM_FLIPXY | AMM_ZP | AMM_ZP_X | AMM_ZP_REL_X | AMM_ABS, 
	AMM_LAX = AMM_FLIPXY | AMM_ZP | AMM_ZP_X | AMM_ZP_REL_X | AMM_ABS | AMM_ABS_X | AMM_ZP_Y_REL,
	AMM_AHX = AMM_FLIPXY | AMM_ZP_REL_X | AMM_ABS_X,
	AMM_SHY = AMM_ABS_X,
	AMM_SHX = AMM_ABS_Y,

	// 65C02 groups
	AMC_ORA = AMM_ORA | AMM_ZP_REL,
	AMC_STA = AMM_STA | AMM_ZP_REL,
	AMC_BIT = AMM_BIT | AMM_IMM | AMM_ZP_X | AMM_ABS_X,
	AMC_DEC = AMM_DEC | AMM_NON | AMM_ACC,
	AMC_JMP = AMM_JMP | AMM_REL_X,
	AMC_STZ = AMM_ZP | AMM_ZP_X | AMM_ABS | AMM_ABS_X,
	AMC_TRB = AMM_ZP | AMM_ABS,
	AMC_BBR = AMM_ZP_ABS,

	// 65816 groups
	AM8_JSR = AMM_ABS | AMM_ABS_L | AMM_REL_X,
	AM8_JSL = AMM_ABS_L,
	AM8_BIT = AMM_IMM_DBL_A | AMC_BIT,
	AM8_ORA = AMM_IMM_DBL_A | AMC_ORA | AMM_STK | AMM_ZP_REL_L | AMM_ABS_L | AMM_STK_REL_Y | AMM_ZP_REL_Y_L | AMM_ABS_L_X,
	AM8_STA = AMC_STA | AMM_STK | AMM_ZP_REL_L | AMM_ABS_L | AMM_STK_REL_Y | AMM_ZP_REL_Y_L | AMM_ABS_L_X,
	AM8_ORL = AMM_ABS_L | AMM_ABS_L_X,
	AM8_STL = AMM_ABS_L | AMM_ABS_L_X,
	AM8_LDX = AMM_IMM_DBL_XY | AMM_LDX,
	AM8_LDY = AMM_IMM_DBL_XY | AMM_LDY,
	AM8_CPY = AMM_IMM_DBL_XY | AMM_CPY,
	AM8_JMP = AMC_JMP | AMM_REL_L | AMM_ABS_L | AMM_REL_X,
	AM8_JML = AMM_REL_L | AMM_ABS_L,
	AM8_BRL = AMM_BRANCH_L | AMM_ABS,
	AM8_MVN = AMM_BLK_MOV,
	AM8_PEI = AMM_ZP_REL | AMM_ZP,
	AM8_PER = AMM_BRANCH_L | AMM_ABS,
	AM8_REP = AMM_IMM | AMM_ZP,	// Merlin allows this to look like a zp access
};

struct mnem {
	const char *instr;
	uint32_t modes;
	uint8_t aCodes[AMB_COUNT];
};

struct mnem opcodes_6502[] = {
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty
	{ "brk", AMM_BRK, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
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
	{ "nop", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea } },

	// 21 ILLEGAL 6502 OPCODES (http://www.oxyron.de/html/opcodes02.html)
	// NOTE: If adding or removing, update NUM_ILLEGAL_6502_OPS
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty
	{ "slo", AMM_SLO, { 0x03, 0x07, 0x00, 0x0f, 0x13, 0x17, 0x1b, 0x1f, 0x00, 0x00, 0x00 } },
	{ "rla", AMM_SLO, { 0x23, 0x27, 0x00, 0x2f, 0x33, 0x37, 0x3b, 0x3f, 0x00, 0x00, 0x00 } },
	{ "sre", AMM_SLO, { 0x43, 0x47, 0x00, 0x4f, 0x53, 0x57, 0x5b, 0x5f, 0x00, 0x00, 0x00 } },
	{ "rra", AMM_SLO, { 0x63, 0x67, 0x00, 0x6f, 0x73, 0x77, 0x7b, 0x7f, 0x00, 0x00, 0x00 } },
	{ "sax", AMM_IMM, { 0x00, 0x00, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "lax", AMM_LAX, { 0xa3, 0xa7, 0x00, 0xaf, 0xb3, 0xb7, 0x00, 0xbf, 0x00, 0x00, 0x00 } },
	{ "dcp", AMM_SLO, { 0xc3, 0xc7, 0x00, 0xcf, 0xd3, 0xd7, 0xdb, 0xdf, 0x00, 0x00, 0x00 } },
	{ "isc", AMM_SLO, { 0xe3, 0xe7, 0x00, 0xef, 0xf3, 0xf7, 0xfb, 0xff, 0x00, 0x00, 0x00 } },
	{ "anc", AMM_IMM, { 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "aac", AMM_IMM, { 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "alr", AMM_IMM, { 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "arr", AMM_IMM, { 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "xaa", AMM_IMM, { 0x00, 0x00, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{"lax2", AMM_IMM, { 0x00, 0x00, 0xab, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "axs", AMM_AXS, { 0x83, 0x87, 0x00, 0x8f, 0x00, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "sbi", AMM_IMM, { 0x00, 0x00, 0xeb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ahx", AMM_AHX, { 0x93, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9f, 0x00, 0x00, 0x00 } },
	{ "shy", AMM_SHY, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x00, 0x00, 0x00 } },
	{ "shx", AMM_SHX, { 0x00, 0x00, 0x00, 0x00, 0x93, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00 } },
	{ "tas", AMM_SHX, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9b, 0x00, 0x00, 0x00, 0x00 } },
	{ "las", AMM_SHX, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbb, 0x00, 0x00, 0x00, 0x00 } },
};

const char* aliases_6502[] = {
	"bcc", "blt",
	"bcs", "bge",
	nullptr, nullptr
};

uint8_t timing_6502[] = {
//	   0	 1	   2	 3	   4	 5	   6	 7	   8	 9	   A	 B	   C	 D	   E	 F
	0x0e, 0x0c, 0xff, 0x0f, 0xff, 0x06, 0x0a, 0x0a, 0x06, 0x04, 0x04, 0x04, 0x04, 0x08, 0x0c, 0x0c,	// 0
	0x05, 0x0b, 0xff, 0x0f, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0x0e, 0x04, 0x09, 0x0e, 0x0f,	// 1
	0x0c, 0x0c, 0xff, 0x0f, 0x06, 0x06, 0x0a, 0x0a, 0x08, 0x04, 0x04, 0x04, 0x08, 0x08, 0x0c, 0x0c,	// 2
	0x05, 0x0b, 0xff, 0x0f, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0x0e, 0x04, 0x09, 0x0e, 0x0f,	// 3
	0x0c, 0x0c, 0xff, 0x0f, 0xff, 0x06, 0x0a, 0x0a, 0x06, 0x04, 0x04, 0x04, 0x06, 0x08, 0x0c, 0x0c,	// 4
	0x05, 0x0b, 0xff, 0x0f, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0xff, 0x04, 0x09, 0x0e, 0x0f,	// 5
	0x0c, 0x0c, 0xff, 0x0f, 0xff, 0x06, 0x0a, 0x0a, 0x08, 0x04, 0x04, 0x04, 0x0a, 0x08, 0x0c, 0x0c,	// 6
	0x05, 0x0b, 0xff, 0x0f, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0x0e, 0x04, 0x09, 0x0e, 0x0f,	// 7
	0xff, 0x0c, 0xff, 0x0c, 0x06, 0x06, 0x06, 0x06, 0x04, 0xff, 0x04, 0x04, 0x08, 0x08, 0x08, 0x08,	// 8
	0x05, 0x0c, 0xff, 0x0c, 0x08, 0x08, 0x08, 0x08, 0x04, 0x0a, 0x04, 0x0a, 0x05, 0x0a, 0x05, 0x0a,	// 9
	0x04, 0x0c, 0x04, 0x0c, 0x06, 0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x08, 0x08,	// A
	0x05, 0x0b, 0xff, 0x0a, 0x08, 0x08, 0x08, 0x08, 0x04, 0x09, 0x04, 0x09, 0x09, 0x09, 0x09, 0x0a,	// B
	0x04, 0x0c, 0xff, 0x0f, 0x06, 0x06, 0x0a, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x0c, 0x0c,	// C
	0x05, 0x0b, 0xff, 0x0f, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0x0e, 0x04, 0x09, 0x0e, 0x0e,	// D
	0x04, 0x0c, 0xff, 0xff, 0x06, 0x06, 0x0a, 0x0a, 0x04, 0x04, 0x04, 0x04, 0x08, 0x08, 0x0c, 0x0c,	// E
	0x05, 0x0b, 0xff, 0xff, 0xff, 0x08, 0x0c, 0x0c, 0x04, 0x09, 0x02, 0x0e, 0x04, 0x09, 0x0e, 0x0e 	// F
};

static const int num_opcodes_6502 = sizeof(opcodes_6502) / sizeof(opcodes_6502[0]);

struct mnem opcodes_65C02[] = {
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs
	{ "brk", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jsr", AMM_ABS, { 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rti", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00 } },
	{ "rts", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00 } },
	{ "ora", AMC_ORA, { 0x01, 0x05, 0x09, 0x0d, 0x11, 0x15, 0x19, 0x1d, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00 } },
	{ "and", AMC_ORA, { 0x21, 0x25, 0x29, 0x2d, 0x31, 0x35, 0x39, 0x3d, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00 } },
	{ "eor", AMC_ORA, { 0x41, 0x45, 0x49, 0x4d, 0x51, 0x55, 0x59, 0x5d, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00 } },
	{ "adc", AMC_ORA, { 0x61, 0x65, 0x69, 0x6d, 0x71, 0x75, 0x79, 0x7d, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00 } },
	{ "sta", AMC_STA, { 0x81, 0x85, 0x00, 0x8d, 0x91, 0x95, 0x99, 0x9d, 0x00, 0x00, 0x00, 0x92, 0x00, 0x00 } },
	{ "lda", AMC_ORA, { 0xa1, 0xa5, 0xa9, 0xad, 0xb1, 0xb5, 0xb9, 0xbd, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x00 } },
	{ "cmp", AMC_ORA, { 0xc1, 0xc5, 0xc9, 0xcd, 0xd1, 0xd5, 0xd9, 0xdd, 0x00, 0x00, 0x00, 0xd2, 0x00, 0x00 } },
	{ "sbc", AMC_ORA, { 0xe1, 0xe5, 0xe9, 0xed, 0xf1, 0xf5, 0xf9, 0xfd, 0x00, 0x00, 0x00, 0xf2, 0x00, 0x00 } },
	{ "asl", AMM_ASL, { 0x00, 0x06, 0x00, 0x0e, 0x00, 0x16, 0x00, 0x1e, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00 } },
	{ "rol", AMM_ASL, { 0x00, 0x26, 0x00, 0x2e, 0x00, 0x36, 0x00, 0x3e, 0x00, 0x2a, 0x2a, 0x00, 0x00, 0x00 } },
	{ "lsr", AMM_ASL, { 0x00, 0x46, 0x00, 0x4e, 0x00, 0x56, 0x00, 0x5e, 0x00, 0x4a, 0x4a, 0x00, 0x00, 0x00 } },
	{ "ror", AMM_ASL, { 0x00, 0x66, 0x00, 0x6e, 0x00, 0x76, 0x00, 0x7e, 0x00, 0x6a, 0x6a, 0x00, 0x00, 0x00 } },
	{ "stx", AMM_STX, { 0x00, 0x86, 0x00, 0x8e, 0x00, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldx", AMM_LDX, { 0x00, 0xa6, 0xa2, 0xae, 0x00, 0xb6, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "dec", AMC_DEC, { 0x00, 0xc6, 0x00, 0xce, 0x00, 0xd6, 0x00, 0xde, 0x00, 0x3a, 0x3a, 0x00, 0x00, 0x00 } },
	{ "inc", AMC_DEC, { 0x00, 0xe6, 0x00, 0xee, 0x00, 0xf6, 0x00, 0xfe, 0x00, 0x1a, 0x1a, 0x00, 0x00, 0x00 } },
	{ "dea", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xde, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00 } },
	{ "ina", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00 } },
	{ "php", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00 } },
	{ "plp", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00 } },
	{ "pha", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00 } },
	{ "pla", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00 } },
	{ "phy", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00 } },
	{ "ply", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7a, 0x00, 0x00, 0x00 } },
	{ "phx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0x00, 0x00, 0x00 } },
	{ "plx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x00 } },
	{ "dey", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00 } },
	{ "tay", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00 } },
	{ "iny", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00 } },
	{ "inx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00 } },
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs
	{ "bpl", AMM_BRA, { 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bmi", AMM_BRA, { 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvc", AMM_BRA, { 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvs", AMM_BRA, { 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bra", AMM_BRA, { 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcc", AMM_BRA, { 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcs", AMM_BRA, { 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bne", AMM_BRA, { 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "beq", AMM_BRA, { 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "clc", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00 } },
	{ "sec", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00 } },
	{ "cli", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00 } },
	{ "sei", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00 } },
	{ "tya", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00 } },
	{ "clv", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00 } },
	{ "cld", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00 } },
	{ "sed", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00 } },
	{ "bit", AMC_BIT, { 0x00, 0x24, 0x89, 0x2c, 0x00, 0x34, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "stz", AMC_STZ, { 0x00, 0x64, 0x00, 0x9c, 0x00, 0x74, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "trb", AMC_TRB, { 0x00, 0x14, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tsb", AMC_TRB, { 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jmp", AMC_JMP, { 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x7c, 0x00 } },
	{ "sty", AMM_STY, { 0x00, 0x84, 0x00, 0x8c, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldy", AMM_LDY, { 0x00, 0xa4, 0xa0, 0xac, 0x00, 0xb4, 0x00, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cpy", AMM_CPY, { 0x00, 0xc4, 0xc0, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cpx", AMM_CPY, { 0x00, 0xe4, 0xe0, 0xec, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "txa", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00 } },
	{ "txs", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00 } },
	{ "tax", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0x00, 0x00 } },
	{ "tsx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba, 0x00, 0x00, 0x00 } },
	{ "dex", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00, 0x00, 0x00 } },
	{ "nop", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x00, 0x00, 0x00 } },

	// WDC specific (18 instructions)
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs

	{ "stp", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00 } },
	{ "wai", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcb, 0x00, 0x00, 0x00 } },
	{ "bbr0", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f } },
	{ "bbr1", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f } },
	{ "bbr2", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2f } },
	{ "bbr3", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f } },
	{ "bbr4", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f } },
	{ "bbr5", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f } },
	{ "bbr6", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f } },
	{ "bbr7", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f } },
	{ "bbs0", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f } },
	{ "bbs1", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9f } },
	{ "bbs2", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaf } },
	{ "bbs3", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbf } },
	{ "bbs4", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcf } },
	{ "bbs5", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdf } },
	{ "bbs6", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef } },
	{ "bbs7", AMC_BBR, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x00, 0x00, 0xff } },
};

const char* aliases_65C02[] = {
	"bcc", "blt",
	"bcs", "bge",
	nullptr, nullptr
};

static const int num_opcodes_65C02 = sizeof(opcodes_65C02) / sizeof(opcodes_65C02[0]);

struct mnem opcodes_65816[] = {
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs [zp] [zp],y absl absl,x b,s (b,s),y[$000] b,b
	{ "brk", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jsr", AM8_JSR, { 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "jsl", AM8_JSL, { 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rti", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rts", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rtl", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ora", AM8_ORA, { 0x01, 0x05, 0x09, 0x0d, 0x11, 0x15, 0x19, 0x1d, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x07, 0x17, 0x0f, 0x1f, 0x03, 0x13, 0x00, 0x00 } },
	{ "and", AM8_ORA, { 0x21, 0x25, 0x29, 0x2d, 0x31, 0x35, 0x39, 0x3d, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x27, 0x37, 0x2f, 0x3f, 0x23, 0x33, 0x00, 0x00 } },
	{ "eor", AM8_ORA, { 0x41, 0x45, 0x49, 0x4d, 0x51, 0x55, 0x59, 0x5d, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x47, 0x57, 0x4f, 0x5f, 0x43, 0x53, 0x00, 0x00 } },
	{ "adc", AM8_ORA, { 0x61, 0x65, 0x69, 0x6d, 0x71, 0x75, 0x79, 0x7d, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x67, 0x77, 0x6f, 0x7f, 0x63, 0x73, 0x00, 0x00 } },
	{ "sta", AM8_STA, { 0x81, 0x85, 0x00, 0x8d, 0x91, 0x95, 0x99, 0x9d, 0x00, 0x00, 0x00, 0x92, 0x00, 0x00, 0x87, 0x97, 0x8f, 0x9f, 0x83, 0x93, 0x00, 0x00 } },
	{ "lda", AM8_ORA, { 0xa1, 0xa5, 0xa9, 0xad, 0xb1, 0xb5, 0xb9, 0xbd, 0x00, 0x00, 0x00, 0xb2, 0x00, 0x00, 0xa7, 0xb7, 0xaf, 0xbf, 0xa3, 0xb3, 0x00, 0x00 } },
	{ "cmp", AM8_ORA, { 0xc1, 0xc5, 0xc9, 0xcd, 0xd1, 0xd5, 0xd9, 0xdd, 0x00, 0x00, 0x00, 0xd2, 0x00, 0x00, 0xc7, 0xd7, 0xcf, 0xdf, 0xc3, 0xd3, 0x00, 0x00 } },
	{ "sbc", AM8_ORA, { 0xe1, 0xe5, 0xe9, 0xed, 0xf1, 0xf5, 0xf9, 0xfd, 0x00, 0x00, 0x00, 0xf2, 0x00, 0x00, 0xe7, 0xf7, 0xef, 0xff, 0xe3, 0xf3, 0x00, 0x00 } },
	{"oral", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x1f, 0x00, 0x00, 0x00, 0x00 } },
	{"andl", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2f, 0x3f, 0x00, 0x00, 0x00, 0x00 } },
	{"eorl", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4f, 0x5f, 0x00, 0x00, 0x00, 0x00 } },
	{"adcl", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f, 0x7f, 0x00, 0x00, 0x00, 0x00 } },
	{"stal", AM8_STL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x9f, 0x00, 0x00, 0x00, 0x00 } },
	{"ldal", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaf, 0xbf, 0x00, 0x00, 0x00, 0x00 } },
	{"cmpl", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcf, 0xdf, 0x00, 0x00, 0x00, 0x00 } },
	{"sbcl", AM8_ORL, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef, 0xff, 0x00, 0x00, 0x00, 0x00 } },
	{ "asl", AMM_ASL, { 0x00, 0x06, 0x00, 0x0e, 0x00, 0x16, 0x00, 0x1e, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rol", AMM_ASL, { 0x00, 0x26, 0x00, 0x2e, 0x00, 0x36, 0x00, 0x3e, 0x00, 0x2a, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "lsr", AMM_ASL, { 0x00, 0x46, 0x00, 0x4e, 0x00, 0x56, 0x00, 0x5e, 0x00, 0x4a, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ror", AMM_ASL, { 0x00, 0x66, 0x00, 0x6e, 0x00, 0x76, 0x00, 0x7e, 0x00, 0x6a, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "stx", AMM_STX, { 0x00, 0x86, 0x00, 0x8e, 0x00, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldx", AM8_LDX, { 0x00, 0xa6, 0xa2, 0xae, 0x00, 0xb6, 0x00, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "dec", AMC_DEC, { 0x00, 0xc6, 0x00, 0xce, 0x00, 0xd6, 0x00, 0xde, 0x00, 0x3a, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "inc", AMC_DEC, { 0x00, 0xe6, 0x00, 0xee, 0x00, 0xf6, 0x00, 0xfe, 0x00, 0x1a, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "dea", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xde, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ina", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "php", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "plp", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "pha", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs [zp] [zp],y absl absl,x b,s (b,s),y[$0000]b,b
	{ "pla", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "phy", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ply", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "phx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "plx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "dey", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tay", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "iny", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "inx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bpl", AMM_BRA, { 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bmi", AMM_BRA, { 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvc", AMM_BRA, { 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bvs", AMM_BRA, { 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bra", AMM_BRA, { 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "brl", AM8_BRL, { 0x00, 0x00, 0x00, 0x82, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcc", AMM_BRA, { 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bcs", AMM_BRA, { 0x00, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bne", AMM_BRA, { 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "beq", AMM_BRA, { 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "clc", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "sec", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cli", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "sei", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tya", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "clv", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cld", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "sed", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "bit", AM8_BIT, { 0x00, 0x24, 0x89, 0x2c, 0x00, 0x34, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "stz", AMC_STZ, { 0x00, 0x64, 0x00, 0x9c, 0x00, 0x74, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "trb", AMC_TRB, { 0x00, 0x14, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tsb", AMC_TRB, { 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
//	   nam   modes     (zp,x)   zp     # $0000 (zp),y zp,x  abs,y abs,x (xx)     A  empty (zp)(abs,x)zp,abs [zp] [zp],y absl absl,x b,s (b,s),y[$0000]b,b
	{ "jmp", AM8_JMP, { 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0xdc, 0x00 } },
	{ "jml", AM8_JML, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0xdc, 0x00 } },
	{ "sty", AMM_STY, { 0x00, 0x84, 0x00, 0x8c, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "ldy", AM8_LDY, { 0x00, 0xa4, 0xa0, 0xac, 0x00, 0xb4, 0x00, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cpy", AM8_CPY, { 0x00, 0xc4, 0xc0, 0xcc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cpx", AM8_CPY, { 0x00, 0xe4, 0xe0, 0xec, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "txa", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "txs", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tax", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xaa, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tsx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xba, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "dex", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "nop", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "cop", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "wdm", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "mvp", AM8_MVN, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44 } },
	{ "mvn", AM8_MVN, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54 } },
	{ "pea", AMM_ABS, { 0x00, 0x00, 0x00, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "pei", AM8_PEI, { 0x00, 0xd4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "per", AM8_PER, { 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "rep", AM8_REP, { 0x00, 0xc2, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "sep", AM8_REP, { 0x00, 0xe2, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "phd", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tcs", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "pld", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tsc", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "phk", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tcd", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tdc", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "phb", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "txy", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "plb", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "tyx", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xBB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "wai", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xcb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "stp", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xdb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "xba", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xeB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
	{ "xce", AMM_NON, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
};

const char* aliases_65816[] = {
	"bcc", "blt",
	"bcs", "bge",
	"tcs", "tas",
	"tsc", "tsa",
	"xba", "swa",
	"tcd", "tad",
	"tdc", "tda",
	nullptr, nullptr
};

static const int num_opcodes_65816 = sizeof(opcodes_65816) / sizeof(opcodes_65816[0]);

uint8_t timing_65816[] = {
	0x4e, 0x1c, 0x4e, 0x28, 0x3a, 0x26, 0x3a, 0x1c, 0x46, 0x24, 0x44, 0x48, 0x4c, 0x28, 0x5c, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x3a, 0x18, 0x6c, 0x1c, 0x44, 0x28, 0x44, 0x44, 0x4c, 0x28, 0x5e, 0x2a,
	0x4c, 0x1c, 0x50, 0x28, 0x16, 0x26, 0x3a, 0x1c, 0x48, 0x24, 0x44, 0x4a, 0x28, 0x28, 0x4c, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x18, 0x18, 0x3c, 0x1c, 0x44, 0x28, 0x44, 0x44, 0x28, 0x28, 0x4e, 0x2a,
	0x4c, 0x1c, 0x42, 0x28, 0x42, 0x16, 0x6a, 0x1c, 0x26, 0x24, 0x44, 0x46, 0x46, 0x28, 0x5c, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x42, 0x18, 0x6c, 0x1c, 0x44, 0x28, 0x76, 0x44, 0x48, 0x28, 0x5e, 0x2a,
	0x4c, 0x1c, 0x4c, 0x28, 0x16, 0x26, 0x3a, 0x1c, 0x28, 0x24, 0x44, 0x4c, 0x4a, 0x28, 0x4c, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x28, 0x18, 0x3c, 0x1c, 0x44, 0x28, 0x78, 0x44, 0x4c, 0x28, 0x4e, 0x2a,
	0x46, 0x1c, 0x48, 0x28, 0x86, 0x16, 0x86, 0x1c, 0x44, 0x24, 0x44, 0x46, 0x78, 0x28, 0x78, 0x2a,
	0x44, 0x1c, 0x1a, 0x2e, 0x88, 0x18, 0x88, 0x1c, 0x44, 0x2a, 0x44, 0x44, 0x28, 0x2a, 0x2a, 0x2a,
	0x74, 0x1c, 0x74, 0x28, 0x86, 0x16, 0x86, 0x1c, 0x44, 0x24, 0x44, 0x48, 0x78, 0x28, 0x78, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x88, 0x18, 0x88, 0x1c, 0x44, 0x28, 0x44, 0x44, 0x78, 0x28, 0x78, 0x2a,
	0x74, 0x1c, 0x46, 0x28, 0x86, 0x16, 0x6a, 0x1c, 0x44, 0x24, 0x44, 0x26, 0x78, 0x28, 0x5c, 0x2a,
	0x44, 0x1a, 0x1a, 0x2e, 0x4c, 0x18, 0x6c, 0x1c, 0x44, 0x28, 0x76, 0x46, 0x4c, 0x28, 0x5e, 0x2a,
	0x74, 0x3c, 0x46, 0x48, 0x86, 0x36, 0x6a, 0x3c, 0x44, 0x44, 0x44, 0x46, 0x78, 0x48, 0x5c, 0x4a,
	0x44, 0x3a, 0x3a, 0x4e, 0x4a, 0x38, 0x6c, 0x3c, 0x44, 0x48, 0x78, 0x44, 0x50, 0x48, 0x5e, 0x4a
};

// m=0, i=0, dp!=0
uint8_t timing_65816_plus[9][3] = {
	{ 0, 0, 0 },	// 6502 plus timing check bit 0
	{ 1, 0, 1 },	// acc 16 bit + dp!=0
	{ 1, 0, 0 },	// acc 16 bit
	{ 0, 0, 1 },	// dp != 0
	{ 0, 0, 0 },	// no plus
	{ 2, 0, 0 },	// acc 16 bit yields 2+
	{ 2, 0, 1 },	// acc 16 bit yields 2+ + dp!=0
	{ 0, 1, 0 },	// idx 16 bit
	{ 0, 1, 1 }		// idx 16 bit + dp!=0
};

// 65C02
// http://6502.org/tutorials/65c02opcodes.html
// http://www.oxyron.de/html/opcodesc02.html

// 65816
// http://wiki.superfamicom.org/snes/show/65816+Reference#fn:14
// http://softpixel.com/~cwright/sianse/docs/65816NFO.HTM
// http://www.oxyron.de/html/opcodes816.html

// How instruction argument is encoded
enum CODE_ARG {
	CA_NONE,			// single byte instruction
	CA_ONE_BYTE,		// instruction carries one byte
	CA_TWO_BYTES,		// instruction carries two bytes
	CA_THREE_BYTES,		// instruction carries three bytes
	CA_BRANCH,			// instruction carries an 8 bit relative address
	CA_BRANCH_16,		// instruction carries a 16 bit relative address
	CA_BYTE_BRANCH,		// instruction carries one byte and one branch
	CA_TWO_ARG_BYTES,	// two separate values
};

enum CPUIndex {
	CPU_6502,
	CPU_6502_ILLEGAL,
	CPU_65C02,
	CPU_65C02_WDC,
	CPU_65816
};

// CPU by index
struct CPUDetails {
	mnem *opcodes;
	int num_opcodes;
	const char* name;
	const char** aliases;
	const uint8_t *timing;
} aCPUs[] = {
	{ opcodes_6502, num_opcodes_6502 - NUM_ILLEGAL_6502_OPS, "6502", aliases_6502, timing_6502 },
	{ opcodes_6502, num_opcodes_6502, "6502ill", aliases_6502, timing_6502 },
	{ opcodes_65C02, num_opcodes_65C02 - NUM_WDC_65C02_SPECIFIC_OPS, "65C02", aliases_65C02, nullptr },
	{ opcodes_65C02, num_opcodes_65C02, "65C02WDC", aliases_65C02, nullptr },
	{ opcodes_65816, num_opcodes_65816, "65816", aliases_65816, timing_65816 },
};
static const int nCPUs = sizeof(aCPUs) / sizeof(aCPUs[0]);


// hardtexted strings
static const strref c_comment("//");
static const strref word_char_range("!0-9a-zA-Z_@$!#");
static const strref macro_arg_bookend("!0-9a-zA-Z_@$!.");
static const strref label_end_char_range("!0-9a-zA-Z_@$!.!:");
static const strref label_end_char_range_merlin("!0-9a-zA-Z_@$]:?");
static const strref filename_end_char_range("!0-9a-zA-Z_!@#$%&()/\\-.");
static const strref keyword_equ("equ");
static const strref str_label("label");
static const strref str_const("const");
static const strref struct_byte("byte");
static const strref struct_word("word");
static const strref import_source("source");
static const strref import_binary("binary");
static const strref import_c64("c64");
static const strref import_text("text");
static const strref import_object("object");
static const strref import_symbols("symbols");
static const strref pool_subpool("pool");
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
};
static const char *str_section_type[] = {
	"UNDEFINED",		// not set
	"CODE",				// default type
	"DATA",				// data section (matters for GS/OS OMF)
	"BSS",				// uninitialized data section
	"ZEROPAGE"			// uninitialized data section in zero page / direct page
};
static const int num_section_type_str = sizeof(str_section_type) / sizeof(str_section_type[0]);

typedef struct sDirectiveName {
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
	{ "SEGMENT", AD_SECTION },	// CA65 version of SECTION
	{ "MERGE", AD_MERGE },
	{ "LINK", AD_LINK },
	{ "XDEF", AD_XDEF },
	{ "XREF", AD_XREF },
	{ "INCOBJ", AD_INCOBJ },
	{ "ALIGN", AD_ALIGN },
	{ "MACRO", AD_MACRO },
	{ "MAC", AD_MACRO },		// MERLIN
	{ "EVAL", AD_EVAL },
	{ "PRINT", AD_EVAL },
	{ "ECHO", AD_EVAL },		// DASM version of EVAL/PRINT
	{ "BYTE", AD_BYTES },
	{ "BYTES", AD_BYTES },
	{ "WORD", AD_WORDS },
	{ "WORDS", AD_WORDS },
	{ "LONG", AD_ADRL },
	{ "DC", AD_DC },
	{ "DV", AD_DC },			// DASM variation of DC which allows expressions
	{ "TEXT", AD_TEXT },
	{ "INCLUDE", AD_INCLUDE },
	{ "INCBIN", AD_INCBIN },
	{ "IMPORT", AD_IMPORT },
	{ "CONST", AD_CONST },
	{ "LABEL", AD_LABEL },
	{ "STRING", AD_STRING },
	{ "FUNCTION", AD_FUNCTION },
	{ "UNDEF", AD_UNDEF },
	{ "INCSYM", AD_INCSYM },
	{ "LABPOOL", AD_LABPOOL },
	{ "POOL", AD_LABPOOL },
	{ "IF", AD_IF },
	{ "IFDEF", AD_IFDEF },
	{ "IFNDEF", AD_IFNDEF },
	{ "IFCONST", AD_IFCONST },
	{ "IFBLANK", AD_IFBLANK },		// #IFBLANK: Conditional assembly follows based on rest of line empty
	{ "IFNBLANK", AD_IFNBLANK },	// #IFDEF: Conditional assembly follows based on rest of line not empty
	{ "ELSE", AD_ELSE },
	{ "ELIF", AD_ELIF },
	{ "ENDIF", AD_ENDIF },
	{ "STRUCT", AD_STRUCT },
	{ "ENUM", AD_ENUM },
	{ "REPT", AD_REPT },
	{ "REPEAT", AD_REPT },		// ca65 version of rept
	{ "INCDIR", AD_INCDIR },
	{ "A16", AD_A16 },			// A16: Set 16 bit accumulator mode
	{ "A8", AD_A8 },			// A8: Set 8 bit accumulator mode
	{ "XY16", AD_XY16 },		// XY16: Set 16 bit index register mode
	{ "XY8", AD_XY8 },			// XY8: Set 8 bit index register mode
	{ "I16", AD_XY16 },			// I16: Set 16 bit index register mode
	{ "I8", AD_XY8 },			// I8: Set 8 bit index register mode
	{ "DUMMY", AD_DUMMY },
	{ "DUMMY_END", AD_DUMMY_END },
	{ "DS", AD_DS },			// Define space
	{ "RES", AD_DS },			// Reserve space
	{ "SCOPE", AD_SCOPE },		// SCOPE: Begin ca65 style scope
	{ "ENDSCOPE", AD_ENDSCOPE },// ENDSCOPR: End ca65 style scope
	{ "PUSH", AD_PUSH },
	{ "PULL", AD_PULL },
	{ "ABORT", AD_ABORT },
	{ "ERR", AD_ABORT },		// DASM version of ABORT
};

// Merlin specific directives separated from regular directives to avoid confusion
DirectiveName aDirectiveNamesMerlin[] {
	{ "MX", AD_MX },			// MERLIN
	{ "STR", AD_LNK },			// MERLIN
	{ "DA", AD_WORDS },			// MERLIN
	{ "DW", AD_WORDS },			// MERLIN
	{ "ASC", AD_TEXT },			// MERLIN
	{ "PUT", AD_INCLUDE },		// MERLIN
	{ "DDB", AD_DBL_BYTES },		// MERLIN
	{ "DB", AD_BYTES },			// MERLIN
	{ "DFB", AD_BYTES },		// MERLIN
	{ "HEX", AD_HEX },			// MERLIN
	{ "DO", AD_IF },			// MERLIN
	{ "FIN", AD_ENDIF },		// MERLIN
	{ "EJECT", AD_EJECT },		// MERLIN
	{ "OBJ", AD_EJECT },		// MERLIN
	{ "TR", AD_EJECT },			// MERLIN
	{ "END", AD_EJECT },		// MERLIN
	{ "REL", AD_EJECT },		// MERLIN
	{ "USR", AD_USR },			// MERLIN
	{ "DUM", AD_DUMMY },		// MERLIN
	{ "DEND", AD_DUMMY_END },	// MERLIN
	{ "LST", AD_LST },			// MERLIN
	{ "LSTDO", AD_LST },		// MERLIN
	{ "LUP", AD_REPT },			// MERLIN
	{ "SAV", AD_SAV },			// MERLIN
	{ "DSK", AD_SAV },			// MERLIN
	{ "LNK", AD_LNK },			// MERLIN
	{ "XC", AD_XC },			// MERLIN
	{ "ENT", AD_ENT },			// MERLIN (xdef, but label on same line)
	{ "EXT", AD_EXT },			// MERLIN (xref, which are implied in x65 object files)
	{ "ADR", AD_ADR },			// ADR: MERLIN store 3 byte word
	{ "ADRL", AD_ADRL },		// ADRL: MERLIN store 4 byte word
	{ "CYC", AD_CYC },			// MERLIN: Start and stop cycle counter
};

struct EvalFuncNames {
	const char* name;
	EvalFuncs function;
};

EvalFuncNames aEvalFunctions[] = {
	{ "DEFINED", EF_DEFINED },			// DEFINED(label) 1 if label is defined
	{ "DEF", EF_DEFINED },				// DEFINED(label) 1 if label is defined
	{ "REFERENCED", EF_REFERENCED },	// REFERENCED(label) 1 if label has been referenced in this file
	{ "BLANK", EF_BLANK },				// BLANK() 1 if the contents within the parenthesis is empty
	{ "CONST", EF_CONST },				// CONST(label) 1 if label is a const label
	{ "SIZEOF", EF_SIZEOF},				// SIZEOF(struct) returns size of structs
	{ "TRIGSIN", EF_SIN },				// TRIGSIN(index, period, amplitude)
};

static const int nDirectiveNames = sizeof(aDirectiveNames) / sizeof(aDirectiveNames[0]);
static const int nDirectiveNamesMerlin = sizeof(aDirectiveNamesMerlin) / sizeof(aDirectiveNamesMerlin[0]);
static const int nEvalFuncs = sizeof(aEvalFunctions) / sizeof(aEvalFunctions[0]);

// Binary search over an array of unsigned integers, may contain multiple instances of same key
uint32_t FindLabelIndex(uint32_t hash, uint32_t *table, uint32_t count)
{
	uint32_t max = count;
	uint32_t first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		uint32_t read = table[index];
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

char* StringCopy(strref str)
{
	char* buf = (char*)calloc(1, (size_t)str.get_len() + 1);
	if (buf && str.get_len()) { memcpy(buf, str.get(), str.get_len()); }
	return buf;
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
	uint32_t _count;
	uint32_t _capacity;
public:
	pairArray() : keys(nullptr), values(nullptr), _count(0), _capacity(0) {}
	void reserve(uint32_t size) {
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
	bool insert(uint32_t pos) {
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
	bool insert(uint32_t pos, H key) {
		if (insert(pos) && keys) {
			keys[pos] = key;
			return true;
		}
		return false;
	}
	void remove(uint32_t pos) {
		if (pos<_count) {
			_count--;
			if (pos<_count) {
				memmove(keys+pos, keys+pos+1, sizeof(H) * (_count-pos));
				memmove(values+pos, values+pos+1, sizeof(V) * (_count-pos));
			}
		}
	}
	H* getKeys() { return keys; }
	H& getKey(uint32_t pos) { return keys[pos]; }
	V* getValues() { return values; }
	V& getValue(uint32_t pos) { return values[pos]; }
	uint32_t count() const { return _count; }
	uint32_t capacity() const { return _capacity; }
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



template< class KeyType, class ValueType, class CountType = size_t > struct HashTable {
	CountType size, maxSteps, used;
	KeyType* keys;
	ValueType* values;

	static CountType HashFunction(KeyType v) { return CountType(((v + (v >> 27) + (v << 29)) + 14695981039346656037UL) * 1099511628211UL); }
	static CountType HashIndex(KeyType hash, CountType tableSize) { return hash & (tableSize - 1); }
	static CountType GetNextIndex(KeyType hash, CountType tableSize) { return (hash + 1) & (tableSize - 1); }
	static CountType KeyToIndex(KeyType key, CountType tableSize) { return HashIndex(HashFunction(key), tableSize); }
	static CountType FindKeyIndex(KeyType hash, CountType hashTableSize, KeyType* hashKeys, CountType maxKeySteps) {
		CountType index = KeyToIndex(hash, hashTableSize);
		while (hashKeys) {
			KeyType key = hashKeys[index];
			if (!key || key == hash) { return index; }
			index = GetNextIndex(index, hashTableSize);
			if (!maxKeySteps--) { break; }
		}
		return index;
	}

	CountType KeyToIndex(KeyType key) { return KeyToIndex(key, size); }

	CountType InsertKey(KeyType key, CountType index) {
		const KeyType* hashKeys = keys;
		CountType currSize = size;
		CountType insertSteps = 0;
		while (KeyType k = hashKeys[index]) {
			if (k == key) { return index; }  // key already exists
			CountType kfirst = KeyToIndex(k, currSize);
			CountType ksteps = kfirst > index ? (currSize + index - kfirst) : (index - kfirst);
			if (insertSteps > ksteps) { return index; }
			index = GetNextIndex(index, size);
			++insertSteps;
		}
		return index;
	}

	CountType FindKeyIndex(KeyType hash) const { return FindKeyIndex(hash, size, keys, maxSteps); }

	CountType Steps(KeyType hash) {
		CountType slot = KeyToIndex(hash, size);
		CountType numSteps = 0;
		while (keys[slot] && keys[slot] != hash) {
			++numSteps;
			slot = GetNextIndex(slot, size);
		}
		return numSteps;
	}

	void UpdateSteps(CountType first, CountType slot) {
		CountType steps = slot > first ? (slot - first) : (size + slot - first);
		if (steps > maxSteps) { maxSteps = steps; }
	}

	ValueType* InsertFitted(KeyType key) {
		assert(key); // key may not be 0
		CountType first = KeyToIndex(key);
		CountType slot = InsertKey(key, first);
		UpdateSteps(first, slot);
		if (keys[slot]) {
			if (keys[slot] == key) { return &values[slot]; } else {
				KeyType prvKey = keys[slot];
				ValueType prev_value = values[slot];
				keys[slot] = key;
				for (;; ) {
					CountType prev_first = KeyToIndex(prvKey);
					CountType slotRH = InsertKey(prvKey, prev_first);
					UpdateSteps(prev_first, slotRH);
					if (keys[slotRH] && keys[slotRH] != prvKey) {
						KeyType tmpKey = keys[slotRH];
						keys[slotRH] = prvKey;
						prvKey = tmpKey;
						ValueType temp_value = values[slotRH];
						values[slotRH] = prev_value;
						prev_value = temp_value;
					} else {
						keys[slotRH] = prvKey;
						values[slotRH] = prev_value;
						++used;
						return &values[slot];
					}
				}
			}
		}
		keys[slot] = key;
		++used;
		return &values[slot];
	}

	HashTable() { Reset(); }

	void Reset() {
		used = 0;
		size = 0;
		maxSteps = 0;
		keys = nullptr;
		values = nullptr;
	}

	~HashTable() { Clear(); }

	void Clear() {
		if (values) {
			for (CountType i = 0, n = size; i < n; ++i) {
				values[i].~ValueType();
			}
			free(values);
		}
		if (keys) { free(keys); }
		Reset();
	}

	CountType GetUsed() const { return used; }
	bool TableMax() const { return used && (used << 4) >= (size * 13); }

	void Grow() {
		KeyType *prevKeys = keys;
		ValueType *prevValues = values;
		CountType prevSize = size, newSize = prevSize ? (prevSize << 1) : 64;
		size = newSize;
		keys = (KeyType*)calloc(1, newSize * sizeof(KeyType));
		values = (ValueType*)calloc(1, newSize * sizeof(ValueType));
		maxSteps = 0;
		for (CountType i = 0; i < newSize; ++i) { new (values + i) ValueType; }
		if (used) {
			used = 0;
			for (CountType i = 0; i < prevSize; i++) {
				if (KeyType key = prevKeys[i]) { *InsertFitted(key) = prevValues[i]; }
			}
		}
		if (prevKeys) { free(prevKeys); }
		if (prevValues) {
			for (CountType i = 0; i != prevSize; ++i) { prevValues[i].~ValueType(); }
			free(prevValues);
		}
	}

	ValueType* InsertKey(KeyType key)
	{
		if (!size || TableMax()) { Grow(); }
		return InsertFitted(key);
	}

	ValueType* InsertKeyValue(KeyType key, ValueType& value)
	{
		ValueType* value_ptr = InsertKey(key);
		*value_ptr = value;
		return value_ptr;
	}

	bool KeyExists(KeyType key)
	{
		return size && key && keys[FindKeyIndex(key)] == key;
	}

	ValueType* GetValue(KeyType key)
	{
		if (size && key) {
			CountType slot = FindKeyIndex(key);
			if (keys[slot] == key) {
				return &values[slot];
			}
		}
		return nullptr;
	}
};


// relocs are cheaper than full expressions and work with
// local labels for relative sections which would otherwise
// be out of scope at link time.

struct Reloc {
	int base_value;
	int section_offset;		// offset into this section
	int target_section;		// which section does this reloc target?
	int8_t bytes;				// number of bytes to write
	int8_t shift;				// number of bits to shift to get value

	Reloc() : base_value(0), section_offset(-1), target_section(-1), bytes(0), shift(0) {}
	Reloc(int base, int offs, int sect, int8_t num_bytes, int8_t bit_shift) :
		base_value(base), section_offset(offs), target_section(sect), bytes(num_bytes), shift(bit_shift) {}
};
typedef std::vector<struct Reloc> relocList;

// For assembly listing this remembers the location of each line
struct ListLine {
	enum Flags {
		MNEMONIC = 0x01,
		KEYWORD = 0x02,
		CYCLES_START = 0x04,
		CYCLES_STOP = 0x08,
	};
	strref source_name;		// source file index name
	strref code;			// line of code this represents
	int column;				// column of line
	int address;			// start address of this line
	int size;				// number of bytes generated for this line
	int line_offs;			// offset into code
	int flags;				// only output code if generated by code

	bool wasMnemonic() const { return !!(flags & MNEMONIC);  }
	bool startClock() const { return !!(flags & CYCLES_START); }
	bool stopClock() const { return !!(flags & CYCLES_STOP); }
};
typedef std::vector<struct ListLine> Listing;

// Source level debugging info that can be saved into linkable object files, this is close to ListLine so possibly combinable.
// this belongs in each section so it can be saved with that into the x65 files or generated if linked
struct SourceDebugEntry {
	int source_file_index;	// index into Assembler::source_file vector
	int address;			// local address in section
	int source_file_offset;	// can be converted into line/column while linking
	int size:24;
	int type : 8;
};
enum class SourceDebugType {
	Code,
	Label,
	Breakpoint
};

typedef std::vector<struct SourceDebugEntry> SourceDebug;


enum SectionType : int8_t {	// enum order indicates fixed address linking priority
	ST_UNDEFINED,			// not set
	ST_CODE,				// default type
	ST_DATA,				// data section (matters for GS/OS OMF)
	ST_BSS,					// uninitialized data section
	ST_ZEROPAGE,			// uninitialized data section in zero page / direct page
	ST_REMOVED				// removed, don't export to object file
};

// String data
typedef struct sStringSymbols {
public:
	strref string_name;		// name of the string
	strref string_const;	// string contents if source reference
	strovl string_value;	// string contents if modified, initialized to null string

	StatusCode Append(strref append);

	strref get() { return string_value.valid() ? string_value.get_strref() : string_const; }
	void clear() {
		if (string_value.cap()) {
			free(string_value.charstr());
			string_value.invalidate();
			string_value.clear();
		}
		string_const.clear();
	}
} StringSymbol;

// start of data section support
// Default is a relative section
// Whenever org or dum with address is encountered => new section
// If org is fixed and < $200 then it is a dummy section Otherwise clear dummy section
typedef struct Section {
	// section name, same named section => append
	strref name;			// name of section for comparison
	strref export_append;	// append this name to export of file
	strref include_from;	// which file did this section originate from?

	// generated address status
	int load_address;		// if assigned a load address
	int start_address;
	int address;			// relative or absolute PC
	int align_address;		// for relative sections that needs alignment

	// merged sections
	int merged_at;			// merged into a section at this offset
	int merged_into;		// -1 if not merged otherwise section merged into
	int merged_size;		// how many bytes were merged in

	// data output
	uint8_t *output;	// memory for this section
	uint8_t *curr;	// current pointer for this section
	size_t output_capacity;	// current output capacity

	// reloc data
	relocList *pRelocs;		// link time resolve (not all sections need this)
	Listing *pListing;		// if list output
	SourceDebug *pSrcDbg;	// if source level debugging info generated

	// grouped sections
	int next_group;			// next section of a group of relative sections or -1
	int first_group;		// >=0 if another section is grouped with this section

	bool address_assigned;	// address is absolute if assigned
	bool dummySection;		// true if section does not generate data, only labels
	SectionType type;		// distinguishing section type for relocatable output

	void reset() {			// explicitly cleaning up sections, not called from Section destructor
		name.clear(); export_append.clear(); include_from.clear();
		start_address = address = load_address = 0x0; type = ST_CODE;
		address_assigned = false; output = nullptr; curr = nullptr;
		dummySection = false; output_capacity = 0;
		merged_at = -1; merged_into = -1; merged_size = 0;
		align_address = 1; if (pRelocs) delete pRelocs;
		next_group = first_group = -1;
		pRelocs = nullptr;
		if (pListing) delete pListing;
		pListing = nullptr;
		if (pSrcDbg) delete pSrcDbg;
		pSrcDbg = nullptr;
	}

	void Cleanup() { if (output) free(output); reset(); }
	bool empty() const { return type != ST_REMOVED && curr==output; }
	bool unused() const { return !address_assigned && address == start_address; }

	int DataOffset() const { return int(curr - output); }
	int size() const { return (int)(curr - output); }
	int addr_size() const { return address - start_address; }
	const uint8_t *get() { return output; }

	int GetPC() const { return address; }
	void AddAddress(int value) { address += value; }
	void SetLoadAddress(int addr) { load_address = addr; }
	int GetLoadAddress() const { return load_address; }

	void SetDummySection(bool enable) { dummySection = enable; type = ST_BSS;  }
	bool IsDummySection() const { return dummySection; }
	bool IsRelativeSection() const { return address_assigned == false; }
	bool IsMergedSection() const { return false; }
	void AddReloc(int base, int offset, int section, int8_t bytes, int8_t shift);

	Section() : pRelocs(nullptr), pListing(nullptr) { reset(); }
	Section(strref _name, int _address) : pRelocs(nullptr), pListing(nullptr), pSrcDbg(nullptr) {
		reset(); name = _name; start_address = load_address = address = _address;
		address_assigned = true;
	}
	Section(strref _name) : pRelocs(nullptr), pListing(nullptr), pSrcDbg(nullptr) {
		reset(); name = _name;
		start_address = load_address = address = 0; address_assigned = false;
	}
	~Section() { }

	// Append data to a section
	StatusCode CheckOutputCapacity(uint32_t addSize);
	void AddByte(int b);
	void AddWord(int w);
	void AddTriple(int l);
	void AddBin(const uint8_t *p, int size);
	void AddText(strref line, strref text_prefix);
	void AddIndexText(StringSymbol * strSym, strref text);
	void SetByte(size_t offs, int b) { output[offs] = (uint8_t)b; }
	void SetWord(size_t offs, int w) { output[offs] = (uint8_t)w; output[offs+1] = uint8_t(w>>8); }
	void SetTriple(size_t offs, int w) { output[offs] = (uint8_t)w; output[offs+1] = uint8_t(w>>8); output[offs+2] = uint8_t(w>>16); }
	void SetQuad(size_t offs, int w) { output[offs] = (uint8_t)w; output[offs+1] = uint8_t(w>>8); output[offs+2] = uint8_t(w>>16); output[offs+3] = uint8_t(w>>24); }
} Section;

// Symbol list entry (in order of parsing)
struct MapSymbol {
	strref name;    // string name
	int value;
	int16_t section;
	int16_t orig_section;
	bool local;     // local variables
	bool borrowed;	// borrowed
};
typedef std::vector<struct MapSymbol> MapSymbolArray;

// Data related to a label
typedef struct sLabel {
public:
	strref label_name;		// the name of this label
	strref pool_name;		// name of the pool that this label is related to
	int value;
	int section;			// rel section address labels belong to a section, -1 if fixed address or assigned
	int orig_section;		// original section where the label was defined
	int mapIndex;           // index into map symbols in case of late resolve
	bool evaluated;			// a value may not yet be evaluated
	bool pc_relative;		// this is an inline label describing a point in the code
	bool constant;			// the value of this label can not change
	bool external;			// this label is globally accessible
	bool reference;			// this label is accessed from external and can't be used for evaluation locally
	bool referenced;		// this label has been found via GetLabel and can be assumed to be referenced for some purpose
	bool borrowed;			// this label has been borrowed and this segment can not define it externally
} Label;


// If an expression can't be evaluated immediately, this is required
// to reconstruct the result when it can be.
typedef struct sLateEval {
	enum Type {				// When an expression is evaluated late, determine how to encode the result
		LET_LABEL,			// this evaluation applies to a label and not memory
		LET_ABS_REF,		// calculate an absolute address and store at 0, +1
		LET_ABS_L_REF,		// calculate a bank + absolute address and store at 0, +1, +2
		LET_ABS_4_REF,		// calculate a 32 bit number
		LET_BRANCH,			// calculate a branch offset and store at this address
		LET_BRANCH_16,		// calculate a branch offset of 16 bits and store at this address
		LET_BYTE,			// calculate a byte and store at this address
		LET_DBL_BYTE,		// calculate a 16-bit, big endian number.
	};
	int target;				// offset into output buffer
	int address;			// current pc
	int scope;				// scope pc
	int scope_depth;		// relevant for scope end
	int16_t section;			// which section to apply to.
	int16_t rept;				// value of rept
	int file_ref;			// -1 if current or xdef'd otherwise index of file for label
	char* expression_mem;		// if a temporary string was used for the expression
	strref label;			// valid if this is not a target but another label
	strref expression;
	strref source_file;
	Type type;
} LateEval;

// A macro is a text reference to where it was defined
typedef struct sMacro {
	strref name;
	strref macro;
	strref source_name;		// source file name (error output)
	strref source_file;		// entire source file (req. for line #)
	bool params_first_line;	// the first line of this macro are parameters
} Macro;

// All local labels are removed when a global label is defined but some when a scope ends
typedef struct sLocalLabelRecord {
	strref label;
	int scope_depth;
	bool scope_reserve;		// not released for global label, only scope	
} LocalLabelRecord;




// Label pools allows C like stack frame label allocation
typedef struct sLabelPool {
	strref pool_name;
	int16_t numRanges;		// normally 1 range, support multiple for ease of use
	int16_t depth;			// Required for scope closure cleanup
	uint32_t start;
	uint32_t end;
	uint32_t scopeUsed[MAX_SCOPE_DEPTH][2]; // last address assigned + scope depth
	StatusCode Reserve(uint32_t numBytes, uint32_t &ret_addr, uint16_t scope);
	void ExitScope(uint16_t scope);
} LabelPool;

// One member of a label struct
struct MemberOffset {
	uint16_t offset;
	uint32_t name_hash;
	strref name;
	strref sub_struct;
};

// Label struct
typedef struct sLabelStruct {
	strref name;
	uint16_t first_member;
	uint16_t numMembers;
	uint16_t size;
} LabelStruct;

// object file labels that are not xdef'd end up here
struct ExtLabels {
	pairArray<uint32_t, Label> labels;
};

// EvalExpression needs a location reference to work out some addresses
struct EvalContext {
	int pc;					// current address at point of eval
	int scope_pc;			// current scope open at point of eval
	int scope_end_pc;		// late scope closure after eval
	int scope_depth;		// scope depth for eval (must match current for scope_end_pc to eval)
	int relative_section;	// return can be relative to this section
	int file_ref;			// can access private label from this file or -1
	int rept_cnt;			// current repeat counter
	int recursion;			// track recursion depth
	StatusCode internalErr;	// if an error occured during an internal stage of evaluation
	EvalContext() : pc(0), scope_pc(0), scope_end_pc(0), scope_depth(0), relative_section(-1),
	file_ref(-1), rept_cnt(0), recursion(0), internalErr(STATUS_OK) {}
	EvalContext(int _pc, int _scope, int _close, int _sect, int _rept_cnt) :
		pc(_pc), scope_pc(_scope), scope_end_pc(_close), scope_depth(-1),
		relative_section(_sect), file_ref(-1), rept_cnt(_rept_cnt),
		recursion(0), internalErr(STATUS_OK) {}
};

// Source context is current file (include file, etc.) or current macro.
typedef struct sSourceContext {
	strref source_name;		// source file name (error output)
	strref source_file;		// entire source file (req. for line #)
	strref code_segment;	// the segment of the file for this context
	strref read_source;		// current position/length in source file
	strref next_source;		// next position/length in source file
	int16_t repeat;			// how many times to repeat this code segment
	int16_t repeat_total;	// initial number of repeats for this code segment
	int16_t conditional_ctx;	// conditional depth at root of this context
	void restart() { read_source = code_segment; }
	bool complete() { repeat--; return repeat <= 0; }
} SourceContext;

// Context stack is a stack of currently processing text
class ContextStack {
private:
	std::vector<SourceContext> stack;	// stack of contexts
	SourceContext *currContext;			// current context
public:
	ContextStack() : currContext(nullptr) { stack.reserve(32); }
	SourceContext& curr() { return *currContext; }
	const SourceContext& curr() const { return *currContext; }
	void push(strref src_name, strref src_file, strref code_seg, int rept = 1) {
		if (currContext)
			currContext->read_source = currContext->next_source;
		SourceContext context;
		context.source_name = src_name;
		context.source_file = src_file;
		context.code_segment = code_seg;
		context.read_source = code_seg;
		context.next_source = code_seg;
		context.repeat = (int16_t)rept;
		context.repeat_total = (int16_t)rept;
		stack.push_back(context);
		currContext = &stack[stack.size()-1];
	}
	void pop() { stack.pop_back(); currContext = stack.size() ? &stack[stack.size()-1] : nullptr; }
	bool has_work() { return currContext!=nullptr; }
	bool empty() const { return stack.size() == 0; }
};

// Support for the PULL and PUSH directives
typedef union { int value; char* string; } ValueOrString;
typedef std::vector < ValueOrString > SymbolStack;
class SymbolStackTable : public HashTable< uint64_t, SymbolStack* > {
public:
	void PushSymbol(Label* symbol);
	StatusCode PullSymbol(Label* symbol);
	void PushSymbol(StringSymbol* string);
	StatusCode PullSymbol(StringSymbol* string);
	~SymbolStackTable();
};

// user declared functions
struct UserFunction {
	const char* name;
	const char* params;
	const char* expression;
};
class UserFunctionMap : public HashTable<uint64_t, UserFunction*> {
public:
	UserFunction *Get(strref name);
	StatusCode Add(strref name, strref params, strref expresion);
	~UserFunctionMap();
};

// The state of the assembler
class Asm {
public:
	pairArray<uint32_t, Label> labels;
	pairArray<uint32_t, StringSymbol> strings;
	pairArray<uint32_t, Macro> macros;
	pairArray<uint32_t, LabelPool> labelPools;
	pairArray<uint32_t, LabelStruct> labelStructs;
	pairArray<uint32_t, strref> xdefs;		// labels matching xdef names will be marked as external

	std::vector<char*> source_files;		// all source files encountered while assembling. referenced by source level debugging
	std::vector<LateEval> lateEval;
	std::vector<LocalLabelRecord> localLabels;
	std::vector<char*> loadedData;			// free when assembler is completed
	std::vector<MemberOffset> structMembers; // labelStructs refer to sets of structMembers
	std::vector<strref> includePaths;
	std::vector<Section> allSections;
	std::vector<ExtLabels> externals;		// external labels organized by object file
	MapSymbolArray map;

	SymbolStackTable symbolStacks;			// enable push/pull of symbols
	UserFunctionMap	userFunctions;			// user defined expression functions

	// CPU target
	struct mnem *opcode_table;
	int opcode_count;
	CPUIndex cpu, list_cpu;
	OPLookup aInstructions[MAX_OPCODES_DIRECTIVES];
	int num_instructions;
	int default_org;

	// context for macros / include files
	ContextStack contextStack;

	// Current section
	Section *current_section;

	// Special syntax rules
	AsmSyntax syntax;

	// Conditional assembly vars
	int conditional_depth;		// conditional depth / base depth for context
	strref conditional_source[MAX_CONDITIONAL_DEPTH];	// start of conditional for error report
	int8_t conditional_nesting[MAX_CONDITIONAL_DEPTH];
	bool conditional_consumed[MAX_CONDITIONAL_DEPTH];

	// Scope info
	int scope_address[MAX_SCOPE_DEPTH];
	int scope_depth;
	int brace_depth;			// scope depth defined only by braces, not files

	strref export_base_name;	// binary output name if available
	strref last_label;			// most recently defined label for Merlin macro
	
	// ca65 style scope (for now treat global symbols as local symbols, no outside name lookup)
	int directive_scope_depth;

	// Eval relative result (only valid if EvalExpression returns STATUS_RELATIVE_SECTION)
	int lastEvalSection;
	int lastEvalValue;
	int8_t lastEvalShift;

	int8_t list_flags;			// listing flags accumulating for each line
	bool accumulator_16bit;		// 65816 specific software dependent immediate mode
	bool index_reg_16bit;		// -"-
	int8_t cycle_counter_level;	// merlin toggles the cycle counter rather than hierarchically evals
	bool error_encountered;		// if any error encountered, don't export binary
	bool list_assembly;			// generate assembler listing
	bool src_debug;				// generate source debug info
	bool end_macro_directive;	// whether to use { } or macro / endmacro for macro scope
	bool import_means_xref;
	bool full_map;

	// Convert source to binary
	void Assemble(strref source, strref filename, bool obj_target);

	// Push a new context and handle enter / exit of context
	StatusCode PushContext(strref src_name, strref src_file, strref code_seg, int rept = 1);
	StatusCode PopContext();

	// Generate assembler listing if requested
	bool List(strref filename);

	// Mimic TASS listing
	bool ListTassStyle( strref filename );

	// Export C64Debugger dbg xml file
	bool SourceDebugExport(strref filename);

	// Generate source for all valid instructions and addressing modes for current CPU
	bool AllOpcodes(strref filename);

	// Clean up memory allocations, reset assembler state
	void Cleanup();

	// Make sure there is room to write more code
	StatusCode CheckOutputCapacity(uint32_t addSize);

	// Operations on current section
	void SetSection(strref name, int address);	// fixed address section
	void SetSection(strref name);				// relative address section
	void LinkLabelsToAddress(int section_id, int section_new, int section_address);
	StatusCode LinkRelocs(int section_id, int section_new, int section_address);
	StatusCode AssignAddressToSection(int section_id, int address);
	StatusCode LinkSections(strref name);		// link relative address sections with this name here
	StatusCode MergeSections(int section_id, int section_merge);	// Combine the result of a section onto another
	StatusCode MergeSectionsByName(int first_section);
	StatusCode MergeAllSections(int first_section);
	void DummySection(int address);				// non-data section (fixed)
	void DummySection();						// non-data section (relative)
	void EndSection();							// pop current section
	Section& CurrSection() { return *current_section; }
	void AssignAddressToGroup();				// Merlin LNK support
	uint8_t* BuildExport(strref append, int &file_size, int &addr);
	int GetExportNames(strref *aNames, int maxNames);
	StatusCode LinkZP();
	int SectionId() { return int(current_section - &allSections[0]); }
	int SectionId(Section &s) { return (int)(&s - &allSections[0]); }
	void AddByte(int b) { CurrSection().AddByte(b); }
	void AddWord(int w) { CurrSection().AddWord(w); }
	void AddTriple(int l) { CurrSection().AddTriple(l); }
	void AddBin(const uint8_t *p, int size) { CurrSection().AddBin(p, size); }

	void ShowReferences();

	// Object file handling
	StatusCode WriteObjectFile(strref filename);	// write x65 object file
	StatusCode ReadObjectFile(strref filename, int link_to_section = -1);		// read x65 object file

	// Apple II GS OMF
	StatusCode WriteA2GS_OMF(strref filename, bool full_collapse);

	// Scope management
	StatusCode EnterScope();
	StatusCode ExitScope();

	// Macro management
	StatusCode AddMacro(strref macro, strref source_name, strref source_file, strref &left);
	StatusCode BuildMacro(Macro &m, strref arg_list);

	// Structs
	StatusCode BuildStruct(strref name, strref declaration);
	StatusCode EvalStruct(strref name, int &value);
	StatusCode BuildEnum(strref name, strref declaration);

	// determine a value from a user function with given parameters
	int EvalUserFunction(UserFunction* user, strref params, EvalContext& etx);

	// Check if function is a valid function and if so evaluate the expression
	bool EvalFunction(strref function, strref &expression, EvalContext& etx, int &value);

	// Calculate a value based on an expression.
	EvalOperator RPNToken_Merlin(strref &expression, const struct EvalContext &etx,
								 EvalOperator prev_op, int16_t &section, int &value);
	EvalOperator RPNToken(strref &expression, EvalContext &etx,
		EvalOperator prev_op, int16_t &section, int &value, strref &subexp);
	StatusCode EvalExpression(strref expression, struct EvalContext &etx, int &result);
	char* PartialEval( strref expression );
	void SetEvalCtxDefaults( struct EvalContext &etx );
	int ReptCnt() const;

	// Access labels
	Label* GetLabel(strref label, bool reference_check = false);
	Label* GetLabel(strref label, int file_ref);
	Label* AddLabel(uint32_t hash);
	bool MatchXDEF(strref label);
	StatusCode AssignLabel(strref label, strref line, bool make_constant = false, bool borrowed = false);
	StatusCode AddressLabel(strref label);
	void LabelAdded(Label *pLabel, bool local = false);
	StatusCode IncludeSymbols(strref line);

	// Strings
	StringSymbol *GetString(strref string_name);
	StringSymbol *AddString(strref string_name, strref string_value);
	StatusCode StringAction(StringSymbol *pStr, strref line);
	StatusCode ParseStringOp(StringSymbol *pStr, strref line);

	// Manage locals
	void MarkLabelLocal(strref label, bool scope_label = false);
	StatusCode FlushLocalLabels(int scope_exit = -1);

	// Source debug
	void AddSrcDbg(int address, SourceDebugType type, const char* text);

	// Label pools
	LabelPool* GetLabelPool(strref pool_name);
	StatusCode AddLabelPool(strref name, strref args);
	StatusCode AssignPoolLabel(LabelPool &pool, strref args);

	// Late expression evaluation
	void AddLateEval(int target, int pc, int scope_pc, strref expression,
					 strref source_file, LateEval::Type type);
	void AddLateEval(strref label, int pc, int scope_pc,
					 strref expression, LateEval::Type type);
	StatusCode CheckLateEval(strref added_label = strref(), int scope_end = -1, bool missing_is_error = false);

	// Assembler Directives
	StatusCode ApplyDirective(AssemblerDirective dir, strref line, strref source_file);
	StatusCode Directive_Rept(strref line);
	StatusCode Directive_Macro(strref line);
	StatusCode Directive_String(strref line);
	StatusCode Directive_Function(strref line);
	StatusCode Directive_Undef(strref line);
	StatusCode Directive_Include(strref line);
	StatusCode Directive_Incbin(strref line, int skip=0, int len=0);
	StatusCode Directive_Import(strref line);
	StatusCode Directive_ORG(strref line);
	StatusCode Directive_LOAD(strref line);
	StatusCode Directive_MERGE(strref line);
	StatusCode Directive_LNK(strref line);
	StatusCode Directive_XDEF(strref line);
	StatusCode Directive_XREF(strref label);
	StatusCode Directive_DC(strref line, int width, strref source_file, bool little_endian = true);
	StatusCode Directive_DS(strref line);
	StatusCode Directive_ALIGN(strref line);
	StatusCode Directive_EVAL(strref line);
	StatusCode Directive_HEX(strref line);
	StatusCode Directive_ENUM_STRUCT(strref line, AssemblerDirective dir);

	// Assembler steps
	StatusCode GetAddressMode(strref line, bool flipXY, uint32_t &validModes,
							  AddrMode &addrMode, int &len, strref &expression);
	StatusCode AddOpcode(strref line, int index, strref source_file);
	StatusCode BuildLine(strref line);
	StatusCode BuildSegment();

	// Display error in stderr
	void PrintError(strref line, StatusCode error, strref file = strref());

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

	// Change CPU
	void SetCPU(CPUIndex CPU);

	// Syntax
	bool Merlin() const { return syntax == SYNTAX_MERLIN; }
	bool KickAsm() const { return syntax == SYNTAX_KICKASM; }

	// constructor
	Asm() : opcode_table(opcodes_6502), opcode_count(num_opcodes_6502), num_instructions(0),
		cpu(CPU_6502), list_cpu(CPU_6502), lastEvalSection(-1) {
		Cleanup(); localLabels.reserve(256); loadedData.reserve(16); lateEval.reserve(64); }
};


void SymbolStackTable::PushSymbol(Label* symbol)
{
	uint64_t key = symbol->label_name.fnv1a_64(symbol->pool_name.fnv1a_64());
	SymbolStack** ppStack = InsertKey(key);	// ppStack will exist but contains a pointer that may not exist
	if (!*ppStack) { *ppStack = new SymbolStack; }
	ValueOrString val;
	val.value = symbol->value;
	(*ppStack)->push_back(val);
}

StatusCode SymbolStackTable::PullSymbol(Label* symbol)
{
	uint64_t key = symbol->label_name.fnv1a_64(symbol->pool_name.fnv1a_64());
	SymbolStack** ppStack = GetValue(key);
	if (!ppStack || !(*ppStack)->size()) { return ERROR_PULL_WITHOUT_PUSH; }
	symbol->value = (**ppStack)[(*ppStack)->size() - 1].value;
	(*ppStack)->pop_back();
	return STATUS_OK;
}

void SymbolStackTable::PushSymbol(StringSymbol* string)
{
	uint64_t key = string->string_name.fnv1a_64();
	SymbolStack** ppStack = InsertKey(key);	// ppStack will exist but contains a pointer that may not exist
	if (!*ppStack) { *ppStack = new SymbolStack; }
	ValueOrString val;
	val.string = nullptr;
	if (string->string_value) {
		val.string = StringCopy(string->string_value.get_strref());
	}
	(*ppStack)->push_back(val);
}

StatusCode SymbolStackTable::PullSymbol(StringSymbol* string)
{
	uint64_t key = string->string_name.fnv1a_64();
	SymbolStack** ppStack = GetValue(key);
	if (!ppStack || !(*ppStack)->size()) { return ERROR_PULL_WITHOUT_PUSH; }
	char* str = (**ppStack)[(*ppStack)->size() - 1].string;
	if (!str && string->string_value) {
		free(string->string_value.charstr());
		string->string_value.invalidate();
	} else {
		if (string->string_value.empty() || string->string_value.cap() < (strlen(str) + 1)) {
			if (string->string_value.charstr()) { free(string->string_value.charstr()); }
			string->string_value.set_overlay((char*)malloc(strlen(str) + 1), (strl_t)strlen(str) + 1);
		}
		string->string_value.copy(str);
		free(str);
	}
	(*ppStack)->pop_back();
	return STATUS_OK;
}

SymbolStackTable::~SymbolStackTable()
{
	for (size_t i = 0; i < size; ++i) {
		if (keys[i] && values[i]) {
			delete values[i];
			values[i] = nullptr;
			keys[i] = 0;
		}
	}
}


UserFunction* UserFunctionMap::Get(strref name)
{
	UserFunction** ret = GetValue(name.fnv1a_64());
	if (ret) { return *ret; }
	return nullptr;
}

StatusCode UserFunctionMap::Add(strref name, strref params, strref expresion)
{
	if (!name || !expresion) { return ERROR_INCOMPLETE_FUNCTION; }
	strl_t stringlen = name.get_len() + 1 + (params ? (params.get_len() + 1) : 0) + expresion.get_len() + 1;
	UserFunction *func = (UserFunction*)calloc(1, sizeof(UserFunction) + stringlen);
	char* strings = (char*)(func + 1);
	func->name = strings;
	memcpy(strings, name.get(), name.get_len());
	strings[name.get_len()] = 0;
	strings += name.get_len() + 1;
	if (params) {
		func->params = strings;
		memcpy(strings, params.get(), params.get_len());
		strings[params.get_len()] = 0;
		strings += params.get_len() + 1;
	}
	func->expression = strings;
	memcpy(strings, expresion.get(), expresion.get_len());

	if (UserFunction** existing = GetValue(name.fnv1a_64())) {
		free(*existing);
		*existing = func;
	} else {
		InsertKeyValue(name.fnv1a_64(), func);
	}
	return STATUS_OK;
}

UserFunctionMap::~UserFunctionMap()
{
	for (size_t i = 0; i < size; ++i) {
		if (keys[i] && values[i]) {
			free(values[i]);
			values[i] = nullptr;
			keys[i] = 0;
		}
	}
}


// Clean up work allocations
void Asm::Cleanup() {
	for (std::vector<char*>::iterator i = loadedData.begin(); i != loadedData.end(); ++i) {
		if (char *data = *i)
			free(data);
	}
	for( std::vector<LateEval>::iterator i = lateEval.begin(); i != lateEval.end(); ++i ) {
		if( i->expression_mem ) { free( i->expression_mem ); i->expression_mem = nullptr; i->expression.clear(); }
	}
	map.clear();
	labelPools.clear();
	loadedData.clear();
	labels.clear();
	macros.clear();
	allSections.clear();
	lateEval.clear();
	for (uint32_t i = 0; i < strings.count(); ++i) {
		StringSymbol &str = strings.getValue(i);
		if (str.string_value.cap())
			free(str.string_value.charstr());
	}
	strings.clear();
	for (std::vector<ExtLabels>::iterator exti = externals.begin(); exti != externals.end(); ++exti) {
		exti->labels.clear();
	}
	for (std::vector<char*>::iterator src = source_files.begin(); src != source_files.end(); ++src) {
		free(*src);
	}
	externals.clear();
	// this section is relocatable but is assigned address $1000 if exporting without directives
	SetSection(strref("default,code"));
	current_section = &allSections[0];
	syntax = SYNTAX_SANE;
	default_org = 0x1000;
	scope_depth = 0;
	brace_depth = 0;
	conditional_depth = 0;
	conditional_nesting[0] = 0;
	conditional_consumed[0] = false;
	directive_scope_depth = 0;
	error_encountered = false;
	list_assembly = false;
	src_debug = false;
	end_macro_directive = false;
	import_means_xref = false;
	accumulator_16bit = false;	// default 65816 8 bit immediate mode
	index_reg_16bit = false;	// other CPUs won't be affected.
	cycle_counter_level = 0;
}

int sortHashLookup(const void *A, const void *B) {
	const OPLookup *_A = (const OPLookup*)A;
	const OPLookup *_B = (const OPLookup*)B;
	return _A->op_hash > _B->op_hash ? 1 : -1;
}

int BuildInstructionTable(OPLookup *pInstr, struct mnem *opcodes,
						  int count, const char **aliases, bool merlin)
{
	// create an instruction table (mnemonic hash lookup)
	int numInstructions = 0;
	for (int i = 0; i < count; i++) {
		OPLookup &op = pInstr[numInstructions++];
		op.op_hash = strref(opcodes[i].instr).fnv1a_lower();
		op.index = (uint8_t)i;
		op.type = OT_MNEMONIC;
	}

	// add instruction aliases
	if (aliases) {
		while (*aliases) {
			strref orig(*aliases++);
			strref alias(*aliases++);
			for (int o=0; o<count; o++) {
				if (orig.same_str_case(opcodes[o].instr)) {
					OPLookup &op = pInstr[numInstructions++];
					op.op_hash = alias.fnv1a_lower();
					op.index = (uint8_t)o;
					op.type = OT_MNEMONIC;
					break;
				}
			}
		}
	}
	
	// add assembler directives
	for (int d = 0; d<nDirectiveNames; d++) {
		OPLookup &op_hash = pInstr[numInstructions++];
		op_hash.op_hash = strref(aDirectiveNames[d].name).fnv1a_lower();
		op_hash.index = (uint8_t)aDirectiveNames[d].directive;
		op_hash.type = OT_DIRECTIVE;
	}
	
	if (merlin) {
		for (int d = 0; d<nDirectiveNamesMerlin; d++) {
			OPLookup &op_hash = pInstr[numInstructions++];
			op_hash.op_hash = strref(aDirectiveNamesMerlin[d].name).fnv1a_lower();
			op_hash.index = (uint8_t)aDirectiveNamesMerlin[d].directive;
			op_hash.type = OT_DIRECTIVE;
		}
	}
	
	// sort table by hash for binary search lookup
	qsort(pInstr, numInstructions, sizeof(OPLookup), sortHashLookup);
	return numInstructions;
}

// Change the instruction set
void Asm::SetCPU(CPUIndex CPU) {
	cpu = CPU;
	if (cpu > list_cpu)
		list_cpu = cpu;
	opcode_table = aCPUs[CPU].opcodes;
	opcode_count = aCPUs[CPU].num_opcodes;
	num_instructions = BuildInstructionTable(aInstructions, opcode_table,
											 opcode_count, aCPUs[CPU].aliases, Merlin());
}

// Read in text data (main source, include, etc.)
char* Asm::LoadText(strref filename, size_t &size) {
	strown<512> file(filename);
	std::vector<strref>::iterator i = includePaths.begin();
	for (;;) {
		if (FILE *f = fopen(file.c_str(), "rb")) {	// rb is intended here since OS
			fseek(f, 0, SEEK_END);					// eol conversion can do ugly things
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
	for (;;) {
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
#ifdef _WIN32
		file.replace('/', '\\');
#endif
		++i;
	}
	size = 0;
	return nullptr;
}

// Create a new section with a fixed address
void Asm::SetSection(strref name, int address) {
	if (name) {
		for (std::vector<Section>::iterator i = allSections.begin(); i!=allSections.end(); ++i) {
			if (i->name && name.same_str(i->name)) {
				current_section = &*i;
				return;
			}
		}
	}
	if (allSections.size()==allSections.capacity()) { allSections.reserve(allSections.size()+16); }
	Section newSection(name, address);
	// don't compile over zero page and stack frame (may be bad assumption)
	if (address<0x200) {
		newSection.SetDummySection(true); }
	allSections.push_back(newSection);
	current_section = &allSections[allSections.size()-1];
}

void Asm::SetSection(strref line) {
	if (allSections.size()==allSections.capacity()) {
		allSections.reserve(allSections.size()+16);
	}

	SectionType type = ST_UNDEFINED;
	if (line.get_first() == '.') { 	// SEG.U etc.
		++line;
		switch (strref::tolower(line.get_first())) {
			case 'u': type = ST_BSS; break;
			case 'z': type = ST_ZEROPAGE; break;
			case 'd': type = ST_DATA; break;
			case 'c': type = ST_CODE; break;
		}
	}
	line.trim_whitespace();
	int align = 1;
	strref name;
	while (strref arg = line.split_token_any_trim(",:")) {
		if (arg.get_first() == '$') { ++arg; align = (int)arg.ahextoui(); }
		else if (arg.is_number()) { align = (int)arg.atoi(); }
		else if (arg.get_first()=='"') { name = (arg+1).before_or_full('"'); }
		else if (!name) { name = arg; }
		else if (arg.same_str("code")) { type = ST_CODE; }
		else if (arg.same_str("data")) { type = ST_DATA; }
		else if (arg.same_str("bss")) { type = ST_BSS; }
		else if (arg.same_str("zp")||arg.same_str("dp")||
				 arg.same_str("zeropage")||arg.same_str("direct")) { type = ST_ZEROPAGE; }
	}
	if (type == ST_UNDEFINED) {
		if (name.find("code")>=0) { type = ST_CODE; }
		else if (name.find("data")>=0) { type = ST_DATA; }
		else if (name.find("bss") >= 0 || name.same_str("directpage_stack")) type = ST_BSS;
		else if (name.find("zp")>=0||name.find("zeropage")>=0||name.find("direct")>=0) { type = ST_ZEROPAGE; }
		else { type = ST_CODE; }
	}
	Section newSection(name);
	newSection.align_address = align;
	newSection.type = type;
	allSections.push_back(newSection);
	current_section = &allSections[allSections.size()-1];
}

// Fixed address dummy section
void Asm::DummySection(int address) {
	if (allSections.size()==allSections.capacity()) { allSections.reserve(allSections.size()+16); }
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
	int section = SectionId();
	if (section) { current_section = &allSections[section-1]; }
}

// Iterate through the current group of sections and assign addresses if this section is fixed
// This is to handle the special linking of Merlin where sections are brought together pre-export
void Asm::AssignAddressToGroup() {
	Section &curr = CurrSection();
	if (!curr.address_assigned) { return; }

	// Put in all the sections cared about into either the fixed sections or the relative sections
	std::vector<Section*> FixedExport;
	std::vector<Section*> RelativeExport;
	int seg = SectionId();
	while (seg>=0) {
		Section &s = allSections[seg];
		if (s.address_assigned && s.type != ST_ZEROPAGE && s.start_address >= curr.start_address) {
			bool inserted = false;
			for (std::vector<Section*>::iterator i = FixedExport.begin(); i!=FixedExport.end(); ++i) {
				if (s.start_address < (*i)->start_address) {
					FixedExport.insert(i, &s);
					inserted = true;
					break;
				}
			}
			if (!inserted) { FixedExport.push_back(&s); }
		} else if (!s.address_assigned && s.type != ST_ZEROPAGE) {
			RelativeExport.push_back(&s);
			s.export_append = curr.export_append;
		}
		seg = allSections[seg].next_group;
	}

	// in this case each block should be added individually in order of code / data / bss
	for (int type = ST_CODE; type <= ST_BSS; type++) {
		std::vector<Section*>::iterator i = RelativeExport.begin();
		while (i!=RelativeExport.end()) {
			Section *pSec = *i;
			if (pSec->type == type) {
				int bytes = pSec->address - pSec->start_address;
				size_t insert_after = FixedExport.size()-1;
				for (size_t p = 0; p<insert_after; p++) {
					int end_prev = FixedExport[p]->address;
					int start_next = FixedExport[p+1]->start_address;
					int avail = start_next - end_prev;
					if (avail >= bytes) {
						int addr = end_prev;
						addr += pSec->align_address <= 1 ? 0 :
							(pSec->align_address - (addr % pSec->align_address)) % pSec->align_address;
						if ((addr + bytes) <= start_next) {
							insert_after = p;
							break;
						}
					}
				}
				int address = FixedExport[insert_after]->address;
				address += pSec->align_address <= 1 ? 0 :
					(pSec->align_address - (address % pSec->align_address)) % pSec->align_address;
				AssignAddressToSection(SectionId(*pSec), address);
				FixedExport.insert((FixedExport.begin() + insert_after + 1), pSec);
				i = RelativeExport.erase(i);
			} else { ++i; }
		}
	}
}

// list all export append names
// for each valid export append name build a binary fixed address code
//	- find lowest and highest address
//	- alloc & 0 memory
//	- any matching relative sections gets linked in after
//	- go through all section that matches export_append in order and copy over memory
uint8_t* Asm::BuildExport(strref append, int &file_size, int &addr) {
	int start_address = 0x7fffffff;
	int end_address = 0;
	bool has_relative_section = false;
	bool has_fixed_section = false;
	int first_link_section = -1;
	std::vector<Section*> FixedExport;

	// automatically merge sections with the same name and type if one is relative and other is fixed
	for (size_t section_id = 0; section_id!=allSections.size(); ++section_id) {
		const Section &section = allSections[section_id];
		if (!section.IsMergedSection()&&!section.IsRelativeSection()) {
			for (size_t section_merge_id = 0; section_merge_id!=allSections.size(); ++section_merge_id) {
				const Section &section_merge = allSections[section_merge_id];
				if(!section_merge.IsMergedSection()&&section_merge.IsRelativeSection() &&
				   section_merge.type == section.type && section.name.same_str_case(section_merge.name)) {
					MergeSections((int)section_id, (int)section_merge_id);
				}
			}
		}
	}

	// link any relocs to sections that are fixed
	for (size_t section_id = 0; section_id!=allSections.size(); ++section_id) {
		const Section &section = allSections[section_id];
		if (section.address_assigned) {
			LinkRelocs((int)section_id, -1, section.start_address);
		}
	}

	// find address range
	while (!has_relative_section && !has_fixed_section) {
		int section_id = 0;
		for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
			if (((!append && !i->export_append) || append.same_str_case(i->export_append)) && i->type != ST_ZEROPAGE) {
				if (!i->IsMergedSection()) {
					if (i->IsRelativeSection()) {
						// prioritize code over data, local code over included code for initial binary segment
						if ((i->type == ST_CODE || i->type == ST_DATA) && i->first_group < 0 &&
							(first_link_section < 0 || (i->type == ST_CODE && 
							(allSections[first_link_section].type == ST_DATA ||
							(!i->include_from && allSections[first_link_section].include_from)))))
							first_link_section = SectionId(*i);
						has_relative_section = true;
					} else if (i->size() > 0 || i->addr_size() > 0) {
						has_fixed_section = true;
						bool inserted = false;
						for (std::vector<Section*>::iterator f = FixedExport.begin(); f != FixedExport.end(); ++f) {
							if ((*f)->start_address > i->start_address) {
								FixedExport.insert(f, &*i);
								inserted = true;
								break;
							}
						}
						if (!inserted)
							FixedExport.push_back(&*i);
						if (i->start_address < start_address)
							start_address = i->start_address;
						if ((i->start_address + (int)i->size()) > end_address) {
							end_address = i->start_address + (int)i->size();
						}
					}
				}
			}
			section_id++;
		}
		if (!has_relative_section && !has_fixed_section)
			return nullptr;
		if (has_relative_section) {
			if (!has_fixed_section) {
				// there is not a fixed section so go through and assign addresses to all sections
				// starting with the first reasonable section
				start_address = default_org;
				if (first_link_section<0) { return nullptr; }
				while (first_link_section >= 0) {
					FixedExport.push_back(&allSections[first_link_section]);
					AssignAddressToSection(first_link_section, start_address);
					start_address = allSections[first_link_section].address;
					first_link_section = allSections[first_link_section].next_group;
				}
			}

			// First link code sections, then data sections, then BSS sections
			for (int sectype = ST_CODE; sectype <= ST_BSS; sectype++) {
				// there are fixed sections so fit all relative sections after or inbetween fixed sections in export group
				for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
					if (sectype == i->type && ((!append && !i->export_append) || append.same_str_case(i->export_append))) {
						int id = (int)(&*i - &allSections[0]);
						if (i->IsRelativeSection() && i->first_group < 0) {
							// try to fit this section in between existing sections if possible
							int insert_after = (int)FixedExport.size()-1;
							for (int f = 0; f < insert_after; f++) {
								int start_block = FixedExport[f]->address;
								int end_block = FixedExport[f + 1]->start_address;
								if ((end_block - start_block) >= (i->address - i->start_address)) {
									int addr_block = start_block;
									int sec = id;
									while (sec >= 0) {
										Section &s = allSections[sec];
										addr_block += s.align_address <= 1 ? 0 :
											(s.align_address - (addr_block % s.align_address)) % s.align_address;
										addr_block += s.address - s.start_address;
										sec = s.next_group;
									}
									if (addr_block <= end_block) {
										insert_after = f;
										break;
									}
								}
							}
							int sec = id;
							start_address = FixedExport[insert_after]->address;
							while (sec >= 0) {
								insert_after++;
								if (insert_after<(int)FixedExport.size()) {
									FixedExport.insert(FixedExport.begin()+insert_after, &allSections[sec]);
								} else {
									FixedExport.push_back(&allSections[sec]);
								}
								AssignAddressToSection(sec, start_address);
								start_address = allSections[sec].address;
								sec = allSections[sec].next_group;
							}
						}
					}
				}
			}
			for (size_t section_id = 0; section_id!=allSections.size(); ++section_id) {
				const Section &section = allSections[section_id];
				if (section.address_assigned) {
					LinkRelocs((int)section_id, -1, section.start_address);
				}
			}
		}
	}

	// get memory for output buffer
	start_address = FixedExport[0]->start_address;
	int last_data_export = (int)(FixedExport.size() - 1);
	while (last_data_export>0&&FixedExport[last_data_export]->type==ST_BSS) { last_data_export--; }
	end_address = FixedExport[last_data_export]->address;
	uint8_t *output = (uint8_t*)calloc(1, end_address - start_address);

	// copy over in order
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if (i->type == ST_REMOVED) { continue; }
		if (((!append && !i->export_append) || append.same_str_case(i->export_append)) && i->type != ST_ZEROPAGE) {
			if (i->size()>0) {
				memcpy(output+i->start_address-start_address, i->output, i->size());
			}
		}
	}


	printf("Linker export + \"" STRREF_FMT "\" summary:\n", STRREF_ARG(append));
	for (std::vector<Section*>::iterator f = FixedExport.begin(); f != FixedExport.end(); ++f) {
		if ((*f)->include_from) {
			printf("* $%04x-$%04x: " STRREF_FMT " (%d) included from " STRREF_FMT "\n", (*f)->start_address,
				(*f)->address, STRREF_ARG((*f)->name), (int)(*f - &allSections[0]), STRREF_ARG((*f)->include_from));
		} else {
			printf("* $%04x-$%04x: " STRREF_FMT " (%d)\n", (*f)->start_address,
				(*f)->address, STRREF_ARG((*f)->name), (int)(*f - &allSections[0]));
		}
	}

	// return the result
	file_size = end_address - start_address;
	addr = start_address;
	return output;
}

// Collect all the export names
int Asm::GetExportNames(strref *aNames, int maxNames) {
	int count = 0;
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if (!i->IsMergedSection()) {
			bool found = false;
			uint32_t hash = i->export_append.fnv1a_lower();
			for (int n = 0; n < count; n++) {
				if (aNames[n].fnv1a_lower() == hash) {
					found = true;
					break;
				}
			}
			if (!found && count<maxNames) { aNames[count++] = i->export_append; }
		}
	}
	return count;
}

// Collect all unassigned ZP sections and link them
StatusCode Asm::LinkZP() {
	uint8_t min_addr = 0xff, max_addr = 0x00;
	int num_addr = 0;
	bool has_assigned = false, has_unassigned = false;
	int first_unassigned = -1;

	// determine if any zeropage section has been asseigned
	for (std::vector<Section>::iterator s = allSections.begin(); s!=allSections.end(); ++s) {
		if (s->type==ST_ZEROPAGE&&!s->IsMergedSection()) {
			if (s->address_assigned) {
				has_assigned = true;
				if (s->start_address<(int)min_addr) {
					min_addr = (uint8_t)s->start_address;
				} else if ((int)s->address>max_addr) {
					max_addr = (uint8_t)s->address;
				}
			} else {
				has_unassigned = true;
				first_unassigned = first_unassigned>=0 ? first_unassigned : (int)(&*s-&allSections[0]);
			}
			num_addr += s->address-s->start_address;
		}
	}
	if (num_addr>0x100) { return ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE; }
	// no unassigned zp section, nothing to fix
	if (!has_unassigned) { return STATUS_OK; }

	StatusCode status = STATUS_OK;
	if (!has_assigned) {	// no section assigned => fit together at end
		int address = 0x100 - num_addr;
		for (std::vector<Section>::iterator s = allSections.begin(); status==STATUS_OK && s != allSections.end(); ++s) {
			if (s->type == ST_ZEROPAGE && !s->IsMergedSection()) {
				status = AssignAddressToSection((int)(&*s - &allSections[0]), address);
				address = s->address;
			}
		}
	} else {	// find first fit neighbouring an address assigned zero page section
		for (std::vector<Section>::iterator s = allSections.begin(); s != allSections.end(); ++s) {
			if (s->type == ST_ZEROPAGE && !s->IsMergedSection() && !s->address_assigned) {
				int size = s->address - s->start_address;
				bool found = false;
				// find any assigned address section and try to place before or after
				for (std::vector<Section>::iterator sa = allSections.begin(); sa != allSections.end(); ++sa) {
					if (sa->type == ST_ZEROPAGE && !sa->IsMergedSection() && sa->address_assigned) {
						for (int e = 0; e < 2; ++e) {
							int start = e ? sa->start_address - size : sa->address;
							int align_size = s->align_address <= 1 ? 0 :
								(s->align_address - (start % s->align_address)) % s->align_address;
							start += align_size;
							int end = start + size;
							if (start >= 0 && end <= 0x100) {
								for (std::vector<Section>::iterator sc = allSections.begin(); !found && sc != allSections.end(); ++sc) {
									found = true;
									if (&*sa != &*sc && sc->type == ST_ZEROPAGE && !sc->IsMergedSection() && sc->address_assigned) {
										if (start <= sc->address && sc->start_address <= end)
											found = false;
									}
								}
							}
							if (found) { AssignAddressToSection((int)(&*s-&allSections[0]), start); }
						}
					}
				}
				if (!found) { return ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE; }
			}
		}
	}
	return status;
}

// Apply labels assigned to addresses in a relative section a fixed address or as part of another section
void Asm::LinkLabelsToAddress(int section_id, int section_new, int section_address) {
	Label *pLabels = labels.getValues();
	int numLabels = labels.count();
	for (int l = 0; l < numLabels; l++) {
		if (pLabels->section == section_id) {
			pLabels->value += section_address;
			pLabels->section = section_new;
			if (pLabels->mapIndex>=0 && pLabels->mapIndex<(int)map.size()) {
				struct MapSymbol &msym = map[pLabels->mapIndex];
				msym.value = pLabels->value;
				msym.section = (int16_t)section_new;
			}
			CheckLateEval(pLabels->label_name);
		}
		++pLabels;
	}
}

// go through relocs in all sections to see if any targets this section
// relocate section to address!
StatusCode Asm::LinkRelocs(int section_id, int section_new, int section_address) {
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
					// only finalize the target value if fixed address
					if (section_new == -1 || allSections[section_new].address_assigned) {
						uint8_t *trg = trg_sect->output + output_offs + i->section_offset;
						int value = i->base_value + section_address;
						if (i->shift < 0)
							value >>= -i->shift;
						else if (i->shift)
							value <<= i->shift;
						for (int b = 0; b < i->bytes; b++)
							*trg++ = (uint8_t)(value >> (b * 8));
						i = pList->erase(i);
						if (i!=pList->end()) { ++i; }
					}
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
StatusCode Asm::AssignAddressToSection(int section_id, int address) {
	if (section_id<0||section_id>=(int)allSections.size()) { return ERROR_NOT_A_SECTION; }
	Section &s = allSections[section_id];
	if (s.address_assigned)
		return ERROR_CANT_REASSIGN_FIXED_SECTION;

	// fix up the alignment of the address
	int align_size = s.align_address<=1 ? 0 : (s.align_address - (address%s.align_address))%s.align_address;
	address += align_size;

	s.start_address = address;
	s.address += address;
	s.address_assigned = true;
	LinkLabelsToAddress(section_id, -1, s.start_address);
	return LinkRelocs(section_id, -1, s.start_address);
}

// Link sections with a specific name at this point
// Relative sections will just be appeneded to a grouping list
// Fixed address sections will be merged together
StatusCode Asm::LinkSections(strref name) {
	if (CurrSection().IsDummySection()) { return ERROR_LINKER_CANT_LINK_TO_DUMMY_SECTION; }
	int last_section_group = CurrSection().next_group;
	while (last_section_group > -1 && allSections[last_section_group].next_group > -1)
		last_section_group = allSections[last_section_group].next_group;

	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if ((!name || i->name.same_str_case(name)) && i->IsRelativeSection() && !i->IsMergedSection()) {
			// it is ok to link other sections with the same name to this section
			if (&*i==&CurrSection()) { continue; }
			// Zero page sections can only be linked with zero page sections
			if (i->type != ST_ZEROPAGE || CurrSection().type == ST_ZEROPAGE) {
				i->export_append = CurrSection().export_append;
				if (!i->address_assigned) {
					if (i->first_group < 0) {
						int prev = last_section_group >= 0 ? last_section_group : SectionId();
						int curr = (int)(&*i - &allSections[0]);
						allSections[prev].next_group = curr;
						i->first_group = CurrSection().first_group >= 0 ? CurrSection().first_group : SectionId();
						last_section_group = curr;
					}
				}
			} else { return ERROR_CANT_LINK_ZP_AND_NON_ZP; }
		}
	}
	return STATUS_OK;
}

StatusCode Asm::MergeSections(int section_id, int section_merge) {
	if (section_id==section_merge||section_id<0||section_merge<0) { return STATUS_OK; }

	Section &s = allSections[section_id];
	Section &m = allSections[section_merge];

	// merging section needs to be relative to be appended
	if (!m.IsRelativeSection()) { return ERROR_CANT_APPEND_SECTION_TO_TARGET; }

	// if merging section is aligned and target section is not aligned to that or multiple of then can't merge
	if (m.align_address>1&&(!s.IsRelativeSection()||(s.align_address%m.align_address)!=0)) {
		return ERROR_CANT_APPEND_SECTION_TO_TARGET;
	}

	m.merged_size = m.address - m.start_address;

	// append the binary to the target..
	int addr_start = s.address;
	int align = m.align_address <= 1 ? 0 : (m.align_address - (addr_start % m.align_address)) % m.align_address;
	if (m.size()) {
		if (s.CheckOutputCapacity(m.size() + align) == STATUS_OK) {
			for (int a = 0; a<align; a++) { s.AddByte(0); }
			s.AddBin(m.output, m.size());
		}
	} else if (m.addr_size() && s.type != ST_BSS && s.type != ST_ZEROPAGE && !s.dummySection) {
		if (s.CheckOutputCapacity(m.address - m.start_address) == STATUS_OK) {
			for (int a = (m.start_address-align); a<m.address; a++) { s.AddByte(0); }
		}
	} else if (m.addr_size()) { s.AddAddress(align+m.addr_size()); }

	addr_start += align - s.start_address;

	// append info for result output
	m.merged_at = addr_start;
	m.merged_into = section_id;

	// move the relocs from the merge section to the keep section
	if (m.pRelocs) {
		if (!s.pRelocs) { s.pRelocs = new relocList; }
		if (s.pRelocs->capacity()<(s.pRelocs->size()+m.pRelocs->size())) {
			s.pRelocs->reserve(s.pRelocs->size()+m.pRelocs->size());
		}
		for (relocList::iterator r = m.pRelocs->begin(); r!=m.pRelocs->end(); ++r) {
			struct Reloc rel = *r;
			rel.section_offset += addr_start;
			s.pRelocs->push_back(rel);
		}
		delete m.pRelocs;
		m.pRelocs = nullptr;
	}

	// go through all the relocs referring to merging section and replace
	for (std::vector<Section>::iterator i = allSections.begin(); i!=allSections.end(); ++i) {
		if (relocList *pReloc = i->pRelocs) {
			for (relocList::iterator r = pReloc->begin(); r!=pReloc->end(); ++r) {
				if (r->target_section == section_merge) {
					r->base_value += addr_start;
					r->target_section = section_id;
				}
			}
		}
	}
	if(!s.IsRelativeSection()) { 
		LinkLabelsToAddress(section_merge, -1, s.start_address);
	}

	// go through all labels referencing merging section
	for (uint32_t i = 0; i<labels.count(); i++) {
		Label &lab = labels.getValue(i);
		if (lab.section == section_merge && lab.evaluated) {
			lab.value += addr_start;
			lab.section = section_id;
		}
	}

	// go through map symbols
	for (MapSymbolArray::iterator i = map.begin(); i!=map.end(); ++i) {
		if (i->section == section_merge) {
			i->value += addr_start;
			i->section = (int16_t)section_id;
		}
	}

	// go through all late evals referencing this section
	for (std::vector<LateEval>::iterator i = lateEval.begin(); i!=lateEval.end(); ++i) {
		if (i->section == section_merge) {
			i->section = (int16_t)section_id;
			if (i->target>=0) { i->target += addr_start; }
			i->address += addr_start;
			if (i->scope>=0) { i->scope += addr_start; }
		}
	}

	if (m.output) {
		free(m.output);
		m.output = nullptr;
		m.curr = nullptr;
	}

	// go through listing
	if (m.pListing) {
		if (!s.pListing) { s.pListing = new Listing; }
		if (s.pListing->capacity()<(m.pListing->size()+s.pListing->size())) {
			s.pListing->reserve((m.pListing->size()+s.pListing->size()));
		}
		for (Listing::iterator i = m.pListing->begin(); i!=m.pListing->end(); ++i) {
			ListLine l = *i;
			l.address += addr_start;
			s.pListing->push_back(l);
		}
		delete m.pListing;
		m.pListing = nullptr;
	}
	// go through source debug
	if (m.pSrcDbg) {
		if (!s.pSrcDbg) { s.pSrcDbg = new SourceDebug; }
		if (s.pSrcDbg->capacity() < (m.pSrcDbg->size() + s.pSrcDbg->size())) {
			s.pSrcDbg->reserve((m.pSrcDbg->size() + s.pSrcDbg->size()));
		}
		for (SourceDebug::iterator i = m.pSrcDbg->begin(); i != m.pSrcDbg->end(); ++i) {
			SourceDebugEntry l = *i;
			l.address += addr_start;
			s.pSrcDbg->push_back(l);
		}
		delete m.pSrcDbg;
		m.pSrcDbg = nullptr;
	}
	m.type = ST_REMOVED;
	return STATUS_OK;
}

// Go through sections and merge same name sections together
StatusCode Asm::MergeSectionsByName(int first_section) {
	int first_code_seg = -1;
	StatusCode status = STATUS_OK;
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if (i->type != ST_REMOVED) {
			if (first_code_seg<0 && i->type==ST_CODE)
				first_code_seg = (int)(&*i-&allSections[0]);
			std::vector<Section>::iterator n = i;
			++n;
			while (n != allSections.end()) {
				if (n->name.same_str_case(i->name) && n->type == i->type) {
					int sk = (int)(&*i - &allSections[0]);
					int sm = (int)(&*n - &allSections[0]);
					if (sm == first_section || (n->align_address > i->align_address)) {
						if (n->align_address<i->align_address) { n->align_address = i->align_address; }
						status = MergeSections(sm, sk);
					} else { status = MergeSections(sk, sm); }
					if (status!=STATUS_OK) { return status; }
				}
				++n;
			}
		}
	}
	return STATUS_OK;
}

// Merge all sections in order of code, data, bss and make sure a specific section remains first
#define MERGE_ORDER_CNT (ST_BSS - ST_CODE+1)
StatusCode Asm::MergeAllSections(int first_section)
{
	StatusCode status = STATUS_OK;
	// combine all sections by type first
	for (int t = ST_CODE; t<ST_ZEROPAGE && status == STATUS_OK; t++) {
		for (int i = 0; i<(int)allSections.size() && status == STATUS_OK; ++i) {
			if (allSections[i].type == t) {
				for (int j = i + 1; j<(int)allSections.size() && status == STATUS_OK; ++j) {
					if (allSections[j].type == t) {
						if (j == first_section || (t != ST_CODE && allSections[i].align_address<allSections[j].align_address)) {
							if (allSections[i].align_address>allSections[j].align_address) {
								allSections[i].align_address = allSections[j].align_address;
							}
							status = MergeSections(j, i);
						} else
							status = MergeSections(i, j);
					}
				}
			}
		}
	}
	// then combine by category except zero page
	int merge_order[MERGE_ORDER_CNT] = { -1 };
	for (int t = ST_CODE; t <= ST_BSS; t++) {
		for (int i = 0; i<(int)allSections.size(); ++i) {
			if (allSections[i].type == t) {
				merge_order[t - ST_CODE] = i;
				break;
			}
		}
	}
	for (int n = 1; n < MERGE_ORDER_CNT; n++) {
		if (merge_order[n] == -1) {
			for (int m = n + 1; m < MERGE_ORDER_CNT; m++)
				merge_order[m - 1] = merge_order[m];
		}
	}
	if (merge_order[0]==-1) { return ERROR_NOT_A_SECTION; }
	for (int o = 1; o < MERGE_ORDER_CNT; o++) {
		if (merge_order[o] != -1 && status == STATUS_OK) {
			if (allSections[merge_order[0]].align_address<allSections[merge_order[o]].align_address) {
				allSections[merge_order[0]].align_address = allSections[merge_order[o]].align_address;
			}
			status = MergeSections(merge_order[0], merge_order[o]);
		}
	}
	return status;
}

// Section based output capacity
// Make sure there is room to assemble in
StatusCode Section::CheckOutputCapacity(uint32_t addSize) {
	if (dummySection||type==ST_ZEROPAGE||type==ST_BSS) { return STATUS_OK; }
	size_t currSize = curr - output;
	if ((addSize + currSize) >= output_capacity) {
		size_t newSize = currSize * 2;
		if (newSize<64*1024) { newSize = 64*1024; }
		if ((addSize+currSize)>newSize) { newSize += newSize; }
		if (uint8_t *new_output = (uint8_t*)malloc(newSize)) {
			memcpy(new_output, output, size());
			curr = new_output + (curr - output);
			free(output);
			output = new_output;
			output_capacity = newSize;
		} else { return ERROR_OUT_OF_MEMORY; }
	}
	return STATUS_OK;
}

// Add one byte to a section
void Section::AddByte(int b) {
	if (!dummySection && type != ST_ZEROPAGE && type != ST_BSS) {
		if (CheckOutputCapacity(1)==STATUS_OK) { *curr++ = (uint8_t)b; }
	}
	address++;
}

// Add a 16 bit word to a section
void Section::AddWord(int w) {
	if (!dummySection && type != ST_ZEROPAGE && type != ST_BSS) {
		if (CheckOutputCapacity(2) == STATUS_OK) {
			*curr++ = (uint8_t)(w & 0xff);
			*curr++ = (uint8_t)(w >> 8);
		}
	}
	address += 2;
}

// Add a 24 bit word to a section
void Section::AddTriple(int l) {
	if (!dummySection && type != ST_ZEROPAGE && type != ST_BSS) {
		if (CheckOutputCapacity(3) == STATUS_OK) {
			*curr++ = (uint8_t)(l & 0xff);
			*curr++ = (uint8_t)(l >> 8);
			*curr++ = (uint8_t)(l >> 16);
		}
	}
	address += 3;
}
// Add arbitrary length data to a section
void Section::AddBin(const uint8_t *p, int size) {
	if (!dummySection && type != ST_ZEROPAGE && type != ST_BSS) {
		if (CheckOutputCapacity(size) == STATUS_OK) {
			memcpy(curr, p, size);
			curr += size;
		}
	}
	address += size;
}

// Add text data to a section
void Section::AddText(strref line, strref text_prefix) {
	// https://en.wikipedia.org/wiki/PETSCII
	// ascii: no change
	// shifted: a-z => $41.. A-Z => $61..
	// unshifted: a-z, A-Z => $41

	if (CheckOutputCapacity((uint32_t)line.get_len()) == STATUS_OK) {
		if (!text_prefix || text_prefix.same_str("ascii")) {
			AddBin((const uint8_t*)line.get(), (int)line.get_len());
		} else if (text_prefix.same_str("c64screen")) {
			while (line) {
				char c = line.get_first(), o = c;
				if (c >= 'a' && c <= 'z') { o = c - 'a' + 1; }
				else if (c == '@') { o = 0; }
				else if (c == '[') { o = 27; }
				else if (c == ']') { o = 29; }
				AddByte(o >= 0x60 ? ' ' : o);
				++line;
			}
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
}

void Section::AddIndexText(StringSymbol *strSym, strref text) {
	if (CheckOutputCapacity((uint32_t)text.get_len())==STATUS_OK) {
		const strref lookup = strSym->get();
		while (text) {
			char c = text.pop_first();
			AddByte(lookup.find(c));
		}
	}
}

// Add a relocation marker to a section
void Section::AddReloc(int base, int offset, int section, int8_t bytes, int8_t shift)
{
	if (!pRelocs) { pRelocs = new relocList; }
	if (pRelocs->size()==pRelocs->capacity()) {
		pRelocs->reserve(pRelocs->size()+32);
	}
	pRelocs->push_back(Reloc(base, offset, section, bytes, shift));
}

// Make sure there is room to assemble in
StatusCode Asm::CheckOutputCapacity(uint32_t addSize) {
	return CurrSection().CheckOutputCapacity(addSize);
}

//
//
// SCOPE MANAGEMENT
//
//

StatusCode Asm::EnterScope() {
	if (scope_depth>=(MAX_SCOPE_DEPTH-1)) { return ERROR_TOO_DEEP_SCOPE; }
	scope_address[++scope_depth] = CurrSection().GetPC();
	return STATUS_OK;
}

StatusCode Asm::ExitScope()
{
	StatusCode error = CheckLateEval(strref(), CurrSection().GetPC());
	if( error >= FIRST_ERROR ) { return error; }
	error = FlushLocalLabels(scope_depth);
	if (error>=FIRST_ERROR) { return error; }
	--scope_depth;
	if (scope_depth<0) { return ERROR_UNBALANCED_SCOPE_CLOSURE; }
	return STATUS_OK;
}

//
//
// CONTEXT ISOLATION
//
//

StatusCode Asm::PushContext(strref src_name, strref src_file, strref code_seg, int rept)
{
	if (conditional_depth>=(MAX_CONDITIONAL_DEPTH-1)) { return ERROR_CONDITION_TOO_NESTED; }
	conditional_depth++;
	conditional_nesting[conditional_depth] = 0;
	conditional_consumed[conditional_depth] = false;
	contextStack.push(src_name, src_file, code_seg, rept);
	contextStack.curr().conditional_ctx = (int16_t)conditional_depth;
	if (scope_depth>=(MAX_SCOPE_DEPTH-1)) {
		return ERROR_TOO_DEEP_SCOPE;
	} else {
		scope_address[++scope_depth] = CurrSection().GetPC();
	}
	return STATUS_OK;
}

StatusCode Asm::PopContext() {
	if (scope_depth) {
		StatusCode ret = ExitScope();
		if (ret!=STATUS_OK) { return ret; }
	}
	if (!ConditionalAsm()||ConditionalConsumed()||
		conditional_depth!=contextStack.curr().conditional_ctx) {
		return ERROR_UNTERMINATED_CONDITION;
	}
	conditional_depth = contextStack.curr().conditional_ctx-1;
	contextStack.pop();
	return STATUS_OK;
}

//
//
// MACROS
//
//

// add a custom macro
StatusCode Asm::AddMacro(strref macro, strref source_name, strref source_file, strref &left)
{	//
	// Recommended macro syntax:
	// macro name(optional params) { actual macro }
	//
	// -endm option macro syntax:
	// macro name arg
	//	actual macro
	// endmacro
	//
	// Merlin macro syntax: (TODO: ignore arguments and use ]1, ]2, etc.)
	// name mac arg1 arg2
	//	actual macro
	// [<<<]/[EOM]
	//
	strref name;
	bool params_first_line = false;
	if (Merlin()) {
		if (Label *pLastLabel = GetLabel(last_label)) {
			labels.remove((uint32_t)(pLastLabel - labels.getValues()));
			name = last_label;
			last_label.clear();
			macro.skip_whitespace();
			if (macro.get_first()==';'||macro.has_prefix(c_comment)) {
				macro.line();
			} else {
				params_first_line = true;
			}
		} else { return ERROR_BAD_MACRO_FORMAT; }
	} else {
		name = macro.split_range(label_end_char_range);
		while (macro.get_first() == ' ' || macro.get_first() == '\t') { ++macro; }
		strref left_line = macro.get_line();
		if (left_line.get_first() == ';' || left_line.has_prefix("//")) { left_line.clear(); }
		left_line.skip_whitespace();
		left_line = left_line.before_or_full(';').before_or_full(c_comment);
		if (left_line && left_line[0]!='(' && left_line[0]!='{') {
			params_first_line = true;
		}
	}
	uint32_t hash = name.fnv1a();
	uint32_t ins = FindLabelIndex(hash, macros.getKeys(), macros.count());
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
	if (Merlin()) {
		strref source = macro;
		while (strref next_line = macro.line()) {
			next_line = next_line.before_or_full(';');
			next_line = next_line.before_or_full(c_comment);
			int term = next_line.find("<<<");
			if (term<0) { term = next_line.find("EOM"); }
			if (term >= 0) {
				strl_t macro_len = strl_t(next_line.get() + term - source.get());
				source = source.get_substr(0, macro_len);
				break;
			}
		}
		left = macro;
		pMacro->macro = source;
		source.skip_whitespace();
	} else if (end_macro_directive) {
		int f = -1;
		const strref endm("endm");
		for (;;) {
			f = macro.find(endm, f+1);
			if (f<0) { return ERROR_BAD_MACRO_FORMAT; }
			if (f == 0 || strref::is_ws(macro[f - 1]) || macro[f - 1] == '.') {
				if (f && macro[f - 1] == '.') { --f; }
				break;
			}
		}
		pMacro->macro = macro.get_substr(0, f);
		macro += f;
		macro.line();
		left = macro;
	} else {
		int pos_bracket = macro.find('{');
		if (pos_bracket < 0) {
			pMacro->macro = strref();
			return ERROR_BAD_MACRO_FORMAT;
		}
		strref source = macro + pos_bracket;
		strref macro_body = source.scoped_block_skip();
		pMacro->macro = strref(macro.get(), pos_bracket + macro_body.get_len() + 2);
		source.skip_whitespace();
		left = source;
	}
	pMacro->source_name = source_name;
	pMacro->source_file = source_file;
	pMacro->params_first_line = params_first_line;
	return STATUS_OK;
}

// Compile in a macro
StatusCode Asm::BuildMacro(Macro &m, strref arg_list) {
	strref macro_src = m.macro, params;
	if (m.params_first_line) {
		if (end_macro_directive||Merlin()) {
			params = macro_src.line();
		} else {
			params = macro_src.before('{');
			macro_src += params.get_len();
		}
	} else { params = (macro_src[0]=='(' ? macro_src.scoped_block_skip() : strref()); }
	params.trim_whitespace();
	arg_list.trim_whitespace();
	if (Merlin()) {
		// need to include comment field because separator is ;
		if (contextStack.curr().read_source.is_substr(arg_list.get()))
			arg_list = (contextStack.curr().read_source +
						strl_t(arg_list.get()-contextStack.curr().read_source.get())
						).line();
		arg_list = arg_list.before_or_full(c_comment).get_trimmed_ws();
		strref arg = arg_list;
		strown<16> tag;
		int t_max = 16;
		int dSize = 0;
		for (int t=1; t<t_max; t++) {
			tag.sprintf("]%d", t);
			strref a = arg.split_token_trim(';');
			if (!a) {
				t_max = t;
				break;
			}
			int count = macro_src.substr_case_count(tag.get_strref());
			dSize += count * ((int)a.get_len() - (int)tag.get_len());
		}
		int mac_size = (int)macro_src.get_len() + dSize + 32;
		if (char *buffer = (char*)malloc(mac_size)) {
			loadedData.push_back(buffer);
			strovl macexp(buffer, mac_size);
			macexp.copy(macro_src);
			arg = arg_list;
			if (tag) {
				strref match("]*{0-9}");
				strl_t pos = 0;
				while (strref tag_mac = macexp.find_wildcard(match, pos)) {
					bool success = false;
					strl_t offs = strl_t(tag_mac.get() - macexp.get());
					if (!offs || !strref::is_valid_label(macexp[offs])) {
						int t = (int)(tag_mac + 1).atoi();
						strref args = arg;
						if (t > 0) {
							for (int skip = 1; skip < t; skip++)
								args.split_token_trim(';');
							strref a = args.split_token_trim(';');
							macexp.exchange(offs, tag_mac.get_len(), a);
							pos += a.get_len();
							success = true;
						}
					}
					if (!success) { return ERROR_MACRO_ARGUMENT; }
				}
			}
			PushContext(m.source_name, macexp.get_strref(), macexp.get_strref());
			return STATUS_OK;
		} else { return ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION; }
	} else if (params) {
		if (arg_list[0]=='(')
			arg_list = arg_list.scoped_block_skip();
		strref pchk = params;
		strref arg = arg_list;
		int dSize = 0;
		char token = ',';// arg_list.find(',') >= 0 ? ',' : ' ';
		char token_macro = ',';// m.params_first_line&& params.find(',') < 0 ? ' ' : ',';
		while (strref param = pchk.split_token_trim(token_macro)) {
			strref a = arg.split_token_trim(token);
			if (param.get_len() < a.get_len()) {
				int count = macro_src.substr_case_count(param);
				dSize += count * ((int)a.get_len() - (int)param.get_len());
			}
		}
		int mac_size = (int)macro_src.get_len() + dSize + 32;
		if (char *buffer = (char*)malloc(mac_size)) {
			loadedData.push_back(buffer);
			strovl macexp(buffer, mac_size);
			// new method:
			//	copy char by char, if start of valid label compare with all the paramters and do that instead
			//	if slow, pre-build arrays of params and replacement
			bool paramStart = true; // firstg char might be a param
			while (macro_src) {
				bool expanded = false;
				char first = macro_src.get_first();
				if (paramStart && strref::is_valid_label(first)) {
					strref param_list = params, arg_parse = arg_list;
					while (strref param = param_list.split_token_trim(token_macro)) {
						strref replace = arg_parse.split_token_track_parens(token);
						if (macro_src.prefix_len_case(param) == param.get_len()) {
							// check if macro is whole word
							if (macro_src.get_len() == param.get_len() ||
								!strref::is_valid_label(macro_src[param.get_len()])) {
								macro_src += param.get_len();
								macexp.append(replace);
								expanded = true;
								paramStart = false;
								break;
							}
						}
					}
				}
				if (!expanded) {
					++macro_src;
					macexp.append(first);
					paramStart = !strref::is_valid_label(first);
				}
			}
			PushContext(m.source_name, macexp.get_strref(), macexp.get_strref());
			return STATUS_OK;
		} else { return ERROR_OUT_OF_MEMORY_FOR_MACRO_EXPANSION; }
	}
	PushContext(m.source_name, m.source_file, macro_src);
	return STATUS_OK;
}

//
//
// STRUCTS AND ENUMS
//
//

// Enums are Structs in disguise
StatusCode Asm::BuildEnum(strref name, strref declaration) {
	uint32_t hash = name.fnv1a();
	uint32_t ins = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
	LabelStruct *pEnum = nullptr;
	while (ins < labelStructs.count() && labelStructs.getKey(ins)==hash) {
		if (name.same_str_case(labelStructs.getValue(ins).name)) {
			pEnum = labelStructs.getValues() + ins;
			break;
		}
		++ins;
	}
	if (pEnum) { return ERROR_STRUCT_ALREADY_DEFINED; }
	labelStructs.insert(ins, hash);
	pEnum = labelStructs.getValues() + ins;
	pEnum->name = name;
	pEnum->first_member = (uint16_t)structMembers.size();
	pEnum->numMembers = 0;
	pEnum->size = 0;		// enums are 0 sized
	int value = 0;

	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	while (strref line = declaration.line()) {
		line = line.before_or_full(',');
		line.trim_whitespace();
		strref member_name = line.split_token_trim('=');
		line = line.before_or_full(';').before_or_full(c_comment).get_trimmed_ws();
		if (line) {
			StatusCode error = EvalExpression(line, etx, value);
			if (error==STATUS_NOT_READY||error==STATUS_XREF_DEPENDENT) {
				return ERROR_ENUM_CANT_BE_ASSEMBLED;
			} else if (error!=STATUS_OK) { return error; }
		}
		struct MemberOffset member;
		member.offset = (uint16_t)value;
		member.name = member_name;
		member.name_hash = member.name.fnv1a();
		member.sub_struct = strref();
		structMembers.push_back(member);
		++value;
		pEnum->numMembers++;
	}
	return STATUS_OK;
}

StatusCode Asm::BuildStruct(strref name, strref declaration) {
	uint32_t hash = name.fnv1a();
	uint32_t ins = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
	LabelStruct *pStruct = nullptr;
	while (ins < labelStructs.count() && labelStructs.getKey(ins)==hash) {
		if (name.same_str_case(labelStructs.getValue(ins).name)) {
			pStruct = labelStructs.getValues() + ins;
			break;
		}
		++ins;
	}
	if (pStruct) { return ERROR_STRUCT_ALREADY_DEFINED; }
	labelStructs.insert(ins, hash);
	pStruct = labelStructs.getValues() + ins;
	pStruct->name = name;
	pStruct->first_member = (uint16_t)structMembers.size();

	uint32_t byte_hash = struct_byte.fnv1a();
	uint32_t word_hash = struct_word.fnv1a();
	uint16_t size = 0;
	uint16_t member_count = 0;

	while (strref line = declaration.line()) {
		line.trim_whitespace();
		strref type = line.split_label();
		if (!type) { continue; }
		line.skip_whitespace();
		uint32_t type_hash = type.fnv1a();
		uint16_t type_size = 0;
		LabelStruct *pSubStruct = nullptr;
		if (type_hash==byte_hash && struct_byte.same_str_case(type)) {
			type_size = 1;
		} else if (type_hash==word_hash && struct_word.same_str_case(type)) {
			type_size = 2;
		} else {
			uint32_t index = FindLabelIndex(type_hash, labelStructs.getKeys(), labelStructs.count());
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
		if (structMembers.size()==structMembers.capacity()) {
			structMembers.reserve(structMembers.size()+64);
		}
		struct MemberOffset member;
		member.offset = size;
		member.name = line.get_label();
		member.name_hash = member.name.fnv1a();
		member.sub_struct = pSubStruct ? pSubStruct->name : strref();
		structMembers.push_back(member);

		size += type_size;
		member_count++;
	}
	{	// add a trailing member of 0 bytes to access the size of the structure
		struct MemberOffset bytes_member;
		bytes_member.offset = size;
		bytes_member.name = "bytes";
		bytes_member.name_hash = bytes_member.name.fnv1a();
		bytes_member.sub_struct = strref();
		structMembers.push_back(bytes_member);
		member_count++;
	}
	pStruct->numMembers = member_count;
	pStruct->size = size;
	return STATUS_OK;
}

// Evaluate a struct offset as if it was a label
StatusCode Asm::EvalStruct(strref name, int &value) {
	LabelStruct *pStruct = nullptr;
	uint16_t offset = 0;
	while (strref struct_seg = name.split_token('.')) {
		strref sub_struct = struct_seg;
		uint32_t seg_hash = struct_seg.fnv1a();
		if (pStruct) {
			struct MemberOffset *member = &structMembers[pStruct->first_member];
			bool found = false;
			for (int i = 0; i<pStruct->numMembers; i++) {
				if (member->name_hash == seg_hash && member->name.same_str_case(struct_seg)) {
					offset += member->offset;
					sub_struct = member->sub_struct;
					found = true;
					break;
				}
				++member;
			}
			if (!found) { return ERROR_REFERENCED_STRUCT_NOT_FOUND; }
		}
		if (sub_struct) {
			uint32_t hash = sub_struct.fnv1a();
			uint32_t index = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
			while (index < labelStructs.count() && labelStructs.getKey(index)==hash) {
				if (sub_struct.same_str_case(labelStructs.getValue(index).name)) {
					pStruct = labelStructs.getValues() + index;
					break;
				}
				++index;
			}
		} else if (name) { return STATUS_NOT_STRUCT; }
	}
	if (pStruct==nullptr) { return STATUS_NOT_STRUCT; }
	value = offset;
	return STATUS_OK;
}


//
// 
// USER FUNCTION EVAL
//
//

int Asm::EvalUserFunction(UserFunction* user, strref params, EvalContext& etx)
{
	strref expression(user->expression);
	strref orig_param(user->params);
	strref paraiter = orig_param;
	strref in_params = params;
	int newSize = expression.get_len();
	while (strref param = paraiter.split_token(',')) {
		strref replace = in_params.split_token(',');
		param.trim_whitespace();
		replace.trim_whitespace();

		if (param.get_len() < replace.get_len()) {
			int count = expression.substr_count(param);
			newSize += count * (replace.get_len() - param.get_len());
		}
	}

	char* subst = (char*)malloc(newSize);
	strovl subststr(subst, newSize);
	subststr.copy(expression);
	while (strref param = orig_param.split_token(',')) {
		strref replace = params.split_token(',');
		param.trim_whitespace();
		replace.trim_whitespace();

		if (!subststr.replace_bookend(param, replace, macro_arg_bookend)) {
			return ERROR_MACRO_ARGUMENT;
		}
	}

	int value = 0;
	etx.internalErr = EvalExpression(subststr.get_strref(), etx, value);
	if (etx.internalErr != STATUS_OK && etx.internalErr < FIRST_ERROR) {
		etx.internalErr = ERROR_FUNCTION_DID_NOT_RESOLVE;
	}

	free(subst);

	return value;
}

//
// 
// EVAL FUNCTIONS
//
//

bool Asm::EvalFunction(strref function, strref& expression, EvalContext& etx, int &value)
{
	// all eval functions take a parenthesis with arguments
	if (expression.get_first() != '(') { return false; }

	strref expRet = expression;
	strref params = expRet.scoped_block_comment_skip();
	params.trim_whitespace();
	if (function.get_first() == '.') { ++function; }

	// look up user defined function
	if (UserFunction* user = userFunctions.Get(function)) {
		expression = expRet;
		value = EvalUserFunction(user, params, etx);
		return true;
	}

	// built-in function
	for (int i = 0; i < nEvalFuncs; ++i) {
		if (function.same_str(aEvalFunctions[i].name)) {
			switch (aEvalFunctions[i].function) {
				case EF_DEFINED:
					expression = expRet;
					value = GetLabel(params, true) != nullptr ? 1 : 0;
					return true;
				case EF_REFERENCED:
					expression = expRet;
					if (Label* label = GetLabel(params, true)) { value = label->referenced; return true; }
					return true;
				case EF_BLANK:
					expression = expRet;
					if (params.get_first() == '{') { params = params.scoped_block_comment_skip(); }
					params.trim_whitespace();
					value = params.is_empty();
					return true;
				case EF_CONST:
					expression = expRet;
					if (Label* label = GetLabel(params, true)) {
						value = label->constant ? 1 : 0;
					}
					return true;
				case EF_SIZEOF:
				{
					expression = expRet;
					uint32_t hash = params.fnv1a();
					uint32_t index = FindLabelIndex(hash, labelStructs.getKeys(), labelStructs.count());
					value = 0;
					while (index < labelStructs.count() && labelStructs.getKey(index) == hash) {
						if (params.same_str_case(labelStructs.getValue(index).name)) {
							value = (labelStructs.getValues() + index)->size;
							break;
						}
						++index;
					}
					return true;
				}

				case EF_SIN:
					expression = expRet;
					value = 0; // TODO: implement trigsin
					return true;
			}
			return false;
		}
	}
	return false;
}


//
//
// EXPRESSIONS AND LATE EVALUATION
//
//

int Asm::ReptCnt() const {
	return contextStack.curr().repeat_total - contextStack.curr().repeat;
}

void Asm::SetEvalCtxDefaults(struct EvalContext &etx) {
	etx.pc = CurrSection().GetPC();				// current address at point of eval
	etx.scope_pc = scope_address[scope_depth];	// current scope open at point of eval
	etx.scope_end_pc = -1;						// late scope closure after eval
	etx.scope_depth = scope_depth;				// scope depth for eval (must match current for scope_end_pc to eval)
	etx.relative_section = -1;					// return can be relative to this section
	etx.file_ref = -1;							// can access private label from this file or -1
	etx.rept_cnt = contextStack.empty() ? 0 : ReptCnt();					// current repeat counter
}

// Get a single token from a merlin expression
EvalOperator Asm::RPNToken_Merlin(strref &expression, const struct EvalContext &etx, EvalOperator prev_op, int16_t &section, int &value) {
	char c = expression.get_first();
	switch (c) {
		case '$': ++expression; value = (int)expression.ahextoui_skip(); return EVOP_VAL;
		case '-': ++expression; return EVOP_SUB;
		case '+': ++expression;	return EVOP_ADD;
		case '*': // asterisk means both multiply and current PC, disambiguate!
			++expression;
			if (expression[0]=='*') return EVOP_STP; // double asterisks indicates comment
			else if (prev_op==EVOP_VAL||prev_op==EVOP_RPR) return EVOP_MUL;
			value = etx.pc; section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1); return EVOP_VAL;
		case '/': ++expression; return EVOP_DIV;
		case '>': if (expression.get_len()>=2&&expression[1]=='>') { expression += 2; return EVOP_SHR; }
				  ++expression; return EVOP_HIB;
		case '<': if (expression.get_len()>=2&&expression[1]=='<') { expression += 2; return EVOP_SHL; }
				  ++expression; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (expression[1]=='0'||expression[1]=='1') {
				++expression; value = (int)expression.abinarytoui_skip(); return EVOP_VAL;
			}
			if (etx.scope_end_pc<0||scope_depth!=etx.scope_depth) return EVOP_NRY;
			++expression; value = etx.scope_end_pc;
			section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1);
			return EVOP_VAL;
		case '|':
		case '.': ++expression; return EVOP_OR;	// MERLIN: . is or, | is not used
		case '^': if (prev_op==EVOP_VAL||prev_op==EVOP_RPR) { ++expression; return EVOP_EOR; }
				  ++expression;  return EVOP_BAB;
		case '&': ++expression; return EVOP_AND;
		case '(': if (prev_op!=EVOP_VAL) { ++expression; return EVOP_LPR; } return EVOP_STP;
		case ')': ++expression; return EVOP_RPR;
		case '"': if (expression[2]=='"') { value = expression[1];  expression += 3; return EVOP_VAL; } return EVOP_STP;
		case '\'': if (expression[2]=='\'') { value = expression[1];  expression += 3; return EVOP_VAL; } return EVOP_STP;
		case ',':
		case '?': return EVOP_STP;
	}
	if (c == '!' && (prev_op == EVOP_VAL || prev_op == EVOP_RPR)) { ++expression; return EVOP_EOR; }
	else if (c == '!' && !(expression + 1).len_label()) {
		if (etx.scope_pc < 0) return EVOP_NRY;	// ! by itself is current scope, !+label char is a local label
		++expression; value = etx.scope_pc;
		section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1); return EVOP_VAL;
	} else if (expression.match_chars_str("0-9", "!a-zA-Z_")) {
		if (prev_op == EVOP_VAL) return EVOP_STP;	// value followed by value doesn't make sense, stop
		value = expression.atoi_skip(); return EVOP_VAL;
	} else if (c == '!' || c == ']' || c==':' || strref::is_valid_label(c)) {
		if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
		char e0 = expression[0];
		int start_pos = (e0==']' || e0==':' || e0=='!' || e0=='.') ? 1 : 0;
		strref label = expression.split_range_trim(label_end_char_range_merlin, start_pos);
		Label *pLabel = GetLabel(label, etx.file_ref);
		if (!pLabel) {
			StatusCode ret = EvalStruct(label, value);
			if (ret==STATUS_OK) { return EVOP_VAL; }
			if (ret!=STATUS_NOT_STRUCT) { return EVOP_ERR; }	// partial struct
		}
		if (!pLabel && label.same_str("rept")) { value = etx.rept_cnt; return EVOP_VAL; }
		if (!pLabel||!pLabel->evaluated) { return EVOP_NRY; }	// this label could not be found (yet)
		value = pLabel->value; section = int16_t(pLabel->section); return EVOP_VAL;
	}
	return EVOP_ERR;
}

// Get a single token from most non-apple II assemblers
EvalOperator Asm::RPNToken(strref &exp, EvalContext &etx, EvalOperator prev_op, int16_t &section, int &value, strref &subexp)
{
	char c = exp.get_first();
	switch (c) {
		case '$': ++exp; value = (int)exp.ahextoui_skip(); return EVOP_VAL;
		case '-': ++exp; return EVOP_SUB;
		case '+': ++exp; return EVOP_ADD;
		case '*': ++exp; // asterisk means both multiply and current PC, disambiguate!
			if (exp[0] == '*') return EVOP_STP; // double asterisks indicates comment
			else if (prev_op == EVOP_VAL || prev_op == EVOP_RPR) return EVOP_MUL;
			value = etx.pc; section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1);
			return EVOP_VAL;
		case '/': ++exp; return EVOP_DIV;
		case '=': if (exp[1] == '=') { exp += 2; return EVOP_EQU; } return EVOP_STP;
		case '>': if (exp.get_len() >= 2 && exp[1] == '>') { exp += 2; return EVOP_SHR; }
				  if (prev_op == EVOP_VAL || prev_op == EVOP_RPR) { ++exp;
					if (exp[0] == '=') { ++exp; return EVOP_GTE; } return EVOP_GT; }
				  ++exp; return EVOP_HIB;
		case '<': if (exp.get_len() >= 2 && exp[1] == '<') { exp += 2; return EVOP_SHL; }
				  if (prev_op == EVOP_VAL || prev_op == EVOP_RPR) { ++exp;
					if (exp[0] == '=') { ++exp; return EVOP_LTE; } return EVOP_LT; }
				  ++exp; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (exp[1] == '0' || exp[1] == '1') { ++exp; value = (int)exp.abinarytoui_skip(); return EVOP_VAL; }
			if (etx.scope_end_pc<0 || scope_depth != etx.scope_depth) return EVOP_NRY;
			++exp; value = etx.scope_end_pc; section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1); return EVOP_VAL;
		case '|': ++exp; return EVOP_OR;
		case '^': if (prev_op == EVOP_VAL || prev_op == EVOP_RPR) { ++exp; return EVOP_EOR; }
				  ++exp;  return EVOP_BAB;
		case '&': ++exp; return EVOP_AND;
		case '~': ++exp; return EVOP_NOT;
		case '(': if (prev_op != EVOP_VAL) { ++exp; return EVOP_LPR; } return EVOP_STP;
		case ')': ++exp; return EVOP_RPR;
		case ',':
		case '?': return EVOP_STP;
		case '\'': if( exp[ 2 ] == '\'' ) { value = exp[ 1 ];  exp += 3; return EVOP_VAL; } return EVOP_STP;
	}
	// ! by itself is current scope, !+label char is a local label
	if (c == '!' && !(exp + 1).len_label()) {
		if (etx.scope_pc < 0) return EVOP_NRY;
		++exp; value = etx.scope_pc; 
		section = int16_t(CurrSection().IsRelativeSection() ? SectionId() : -1); return EVOP_VAL;
	} else if (exp.match_chars_str("0-9", "!a-zA-Z_")) {
		if (prev_op == EVOP_VAL) return EVOP_STP; // value followed by value doesn't make sense, stop
		value = exp.atoi_skip(); return EVOP_VAL;
	} else if (c == '!' || c == ':' || c=='.' || c=='@' || strref::is_valid_label(c)) {
		if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
		char e0 = exp[0];
		int start_pos = (e0 == ':' || e0 == '!' || e0 == '.') ? 1 : 0;
		strref label = exp.split_range_trim(label_end_char_range, start_pos);
		Label *pLabel = GetLabel(label, etx.file_ref);
		if (!pLabel) {
			StatusCode ret = EvalStruct(label, value);
			if (ret==STATUS_OK) { return EVOP_VAL; }
			if (ret!=STATUS_NOT_STRUCT) { return EVOP_ERR; }	// partial struct
		}
		if (!pLabel && label.same_str("rept")) { value = etx.rept_cnt; return EVOP_VAL; }
		if (!pLabel) { if (StringSymbol *pStr = GetString(label)) { subexp = pStr->get(); return EVOP_EXP; } }
		if (!pLabel) { if (EvalFunction(label, exp, etx, value)) { return EVOP_VAL; } }
		if (!pLabel || !pLabel->evaluated) return EVOP_NRY;	// this label could not be found (yet)
		value = pLabel->value; section = int16_t(pLabel->section); return pLabel->reference ? EVOP_XRF : EVOP_VAL;
	}
	return EVOP_ERR;
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

// determine if a scalar can be a shift
static int mul_as_shift(int scalar) {
	int shift = 0;
	while (scalar > 1 && (scalar & 1) == 0) {
		shift++;
		scalar >>= 1;
	}
	return scalar == 1 ? shift : 0;
}

#define MAX_EXPR_STACK 2

StatusCode Asm::EvalExpression(strref expression, EvalContext &etx, int &result)
{
	int numValues = 0;
	int numOps = 0;
	strref expression_stack[MAX_EXPR_STACK];
	int exp_sp = 0;

	char ops[MAX_EVAL_OPER];		// RPN expression
	int values[MAX_EVAL_VALUES];	// RPN values (in order of RPN EVOP_VAL operations)
	int16_t section_ids[MAX_EVAL_SECTIONS];			// local index of each referenced section
	int16_t section_val[MAX_EVAL_VALUES] = { 0 };	// each value can be assigned to one section, or -1 if fixed
	int16_t num_sections = 0;		// number of sections in section_ids (normally 0 or 1, can be up to MAX_EVAL_SECTIONS)
	bool xrefd = false;

	// don't allow too deep recursion of this function, this should be rare as it takes a user function or variadic macro to recurse.
	if (etx.recursion > 3) { return ERROR_EXPRESSION_RECURSION; }
	etx.recursion++;				// increment recursion of EvalExpression body
	values[0] = 0;					// Initialize RPN if no expression
	{
		int sp = 0;
		char op_stack[MAX_EVAL_OPER];
		EvalOperator prev_op = EVOP_NONE;
		expression.trim_whitespace();
		while (expression || exp_sp) {
			int value = 0;
			int16_t section = -1, index_section = -1;
			EvalOperator op = EVOP_NONE;
			strref subexp;
			if (!expression && exp_sp) {
				expression = expression_stack[--exp_sp];
				op = EVOP_RPR;
			} else if (Merlin()) {
				op = RPNToken_Merlin(expression, etx, prev_op, section, value);
			} else {
				op = RPNToken(expression, etx, prev_op, section, value, subexp);
			}
			if (op == EVOP_ERR) { etx.recursion--; return ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION; }
			else if (op==EVOP_NRY) { etx.recursion--; return STATUS_NOT_READY; }
			else if (op == EVOP_EXP) {
				if (exp_sp>=MAX_EXPR_STACK) { etx.recursion--; return ERROR_TOO_MANY_VALUES_IN_EXPRESSION; }
				expression_stack[exp_sp++] = expression;
				expression = subexp;
				op = EVOP_LPR;
			} else if (op == EVOP_XRF) {
				xrefd = true;
				op = EVOP_VAL;
			}
			if (section >= 0) {
				for (int s = 0; s<num_sections && index_section<0; s++) {
					if (section_ids[s]==section) { index_section = (int16_t)s; }
				}
				if (index_section<0) {
					if (num_sections<=MAX_EVAL_SECTIONS) {
						section_ids[index_section = num_sections++] = section;
					} else { etx.recursion--; return STATUS_NOT_READY; }
				}
			}

			// this is the body of the shunting yard algorithm
			if (op == EVOP_VAL) {
				section_val[numValues] = index_section;	// only value operators can be section specific
				values[numValues++] = value;
				ops[numOps++] = (char)op;
			} else if (op == EVOP_LPR) {
				op_stack[sp++] = (char)op;
			} else if (op == EVOP_RPR) {
				while (sp && op_stack[sp-1]!=EVOP_LPR) {
					sp--;
					ops[numOps++] = op_stack[sp];
				}
				// check that there actually was a left parenthesis
				if (!sp||op_stack[sp-1]!=EVOP_LPR) { etx.recursion--; return ERROR_UNBALANCED_RIGHT_PARENTHESIS; }
				sp--; // skip open paren
			} else if (op == EVOP_STP) {
				break;
			} else {
				bool skip = false;
				if ((prev_op >= EVOP_EQU && prev_op <= EVOP_GTE) || (prev_op==EVOP_HIB || prev_op==EVOP_LOB)) {
					if (op==EVOP_SUB) { op = EVOP_NEG; }
					else if (op==EVOP_ADD) { skip = true; }
				}
				if (op==EVOP_SUB && sp && prev_op==EVOP_SUB) {
					sp--;
				}  else {
					while (sp && !skip) {
						EvalOperator p = (EvalOperator)op_stack[sp-1];
						if (p==EVOP_LPR||op>p) { break; }
						ops[numOps++] = (char)p;
						sp--;
					}
					op_stack[sp++] = (char)op;
				}
			}
			// check for out of bounds or unexpected input
			if (numValues==MAX_EVAL_VALUES) { etx.recursion--; return ERROR_TOO_MANY_VALUES_IN_EXPRESSION; }
			else if (numOps==MAX_EVAL_OPER||sp==MAX_EVAL_OPER) {
				etx.recursion--;
				return ERROR_TOO_MANY_OPERATORS_IN_EXPRESSION;
			}
			prev_op = op;
			expression.skip_whitespace();
		}
		while (sp) {
			sp--;
			ops[numOps++] = op_stack[sp];
		}
	}
	etx.recursion--; // recursion only occurs in loop above

	// Check if dependent on XREF'd symbol
	if (xrefd) {
		return STATUS_XREF_DEPENDENT;
	}

	// processing the result RPN will put the completed expression into values[0].
	// values is used as both the queue and the stack of values since reads/writes won't
	// exceed itself.
	{
		int valIdx = 0;
		int ri = 0;		// RPN index (value)
		int prev_val = values[0];
		int shift_bits = 0; // special case for relative reference to low byte / high byte
		int16_t section_counts[MAX_EVAL_SECTIONS][MAX_EVAL_VALUES] = { 0 };
		for (int o = 0; o<numOps; o++) {
			EvalOperator op = (EvalOperator)ops[o];
			shift_bits = 0;
			prev_val = ri ? values[ri-1] : prev_val;
			if (op!=EVOP_VAL && op!=EVOP_LOB && op!=EVOP_HIB && op!=EVOP_BAB && op!=EVOP_SUB && op!=EVOP_NOT && ri<2) {
				break; // ignore suffix operations that are lacking values
			}
			switch (op) {
				case EVOP_VAL:	// value
					for (int i = 0; i<num_sections; i++) { section_counts[i][ri] = i==section_val[ri] ? 1 : 0; }
					values[ri++] = values[valIdx++]; break;
				case EVOP_EQU:	// ==
					ri--;
					values[ri - 1] = values[ri - 1] == values[ri];
					break;
				case EVOP_GT:	// >
					ri--;
					values[ri - 1] = values[ri - 1] > values[ri];
					break;
				case EVOP_LT:	// <
					ri--;
					values[ri - 1] = values[ri - 1] < values[ri];
					break;
				case EVOP_GTE:	// >=
					ri--;
					values[ri - 1] = values[ri - 1] >= values[ri];
					break;
				case EVOP_LTE:	// >=
					ri--;
					values[ri - 1] = values[ri - 1] <= values[ri];
					break;
				case EVOP_ADD:	// +
					ri--;
					for (int i = 0; i<num_sections; i++) { section_counts[i][ri-1] += section_counts[i][ri]; }
					values[ri-1] += values[ri]; break;
				case EVOP_SUB:	// -
					if (ri==1) {
						values[ri-1] = -values[ri-1];
					}  else if (ri>1) {
						ri--;
						for (int i = 0; i<num_sections; i++) { section_counts[i][ri-1] -= section_counts[i][ri]; }
						values[ri-1] -= values[ri];
					} break;
				case EVOP_NEG:
					if (ri>=1) { values[ri-1] = -values[ri-1]; }
					break;
				case EVOP_MUL:	// *
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					shift_bits = mul_as_shift(values[ri]);
					prev_val = values[ri - 1];
					values[ri-1] *= values[ri]; break;
				case EVOP_DIV:	// /
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					shift_bits = -mul_as_shift(values[ri]);
					prev_val = values[ri - 1];
					values[ri - 1] /= values[ri]; break;
				case EVOP_AND:	// &
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					values[ri-1] &= values[ri]; break;
				case EVOP_OR:	// |
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					values[ri-1] |= values[ri]; break;
				case EVOP_EOR:	// ^
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					values[ri-1] ^= values[ri]; break;
				case EVOP_SHL:	// <<
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					shift_bits = values[ri];
					prev_val = values[ri - 1];
					values[ri - 1] <<= values[ri]; break;
				case EVOP_SHR:	// >>
					ri--;
					for (int i = 0; i<num_sections; i++) {
						section_counts[i][ri-1] |= section_counts[i][ri];
					}
					shift_bits = -values[ri];
					prev_val = values[ri - 1];
					values[ri - 1] >>= values[ri]; break;
				case EVOP_NOT:
					if (ri) { values[ri - 1] = ~values[ri - 1]; }
					break;
				case EVOP_LOB:	// low byte
					if (ri) { values[ri-1] &= 0xff; }
					break;
				case EVOP_HIB:
					if (ri) {
						shift_bits = -8;
						values[ri - 1] = values[ri - 1] >> 8;
					} break;
				case EVOP_BAB:
					if (ri) {
						shift_bits = -16;
						values[ri - 1] = (values[ri - 1] >> 16);
					}
					break;
				default:
					return ERROR_EXPRESSION_OPERATION;
					break;
			}
			if (shift_bits==0&&ri) { prev_val = values[ri-1]; }
		}
		int section_index = -1;
		bool curr_relative = false;
		// If relative to any section unless specifically interested in a relative value then return not ready
		for (int i = 0; i<num_sections; i++) {
			if (section_counts[i][0]) {
				if (section_counts[i][0]!=1||section_index>=0) {
					return STATUS_NOT_READY;
				} else if (etx.relative_section==section_ids[i]) {
					curr_relative = true;
				} else if (etx.relative_section>=0) { return STATUS_NOT_READY; }
				section_index = i;
			}
		}
		result = values[0];
		if (section_index>=0 && !curr_relative) {
			lastEvalSection = section_ids[section_index];
			lastEvalValue = prev_val;
			lastEvalShift = (int8_t)shift_bits;
			return STATUS_RELATIVE_SECTION;
		}
	}
	return STATUS_OK;
}

// if labels referenced in the expression have been evaluated absolutely
// create a new expression with those labels replaced by their value.
char* Asm::PartialEval( strref expression )
{
	struct EvalContext etx;
	SetEvalCtxDefaults( etx );
	strown< 1024 > partial;

	strref input = expression;

	EvalOperator prev_op = EVOP_NONE;
	bool partial_solved = false;

	while( expression ) {
		int value = 0;
		int16_t section = -1, index_section = -1;
		EvalOperator op = EVOP_NONE;
		strref subexp;
		while( expression.get_len() && strref::is_ws( expression.get_first() ) ) {
			partial.append( expression.get_first() );
			++expression;
		}
		if( expression ) {
			char f = expression.get_first();
			if( strref::is_number( f ) ) {
				strl_t l = expression.len_alphanumeric();
				partial.append( strref( expression.get(), l ) );
				expression += l;
			} else if( f=='.' || f == '_' || strref::is_alphabetic( f ) ) {
				strref label = expression.split_range( label_end_char_range );
				Label *pLabel = GetLabel( label, etx.file_ref );

				if (pLabel && pLabel->evaluated&&!pLabel->external && pLabel->section<0) {
					partial.sprintf_append( "$%x ", pLabel->value );	// insert extra whitespace for separation
					partial_solved = true;
				} else {
					partial.append( label );
				}
			} else {
				partial.append( expression.get_first() );
				++expression;
			}
		}
	}
	if( partial_solved ) {
		//printf( "PARTIAL EXPRESSION SOLVED:\n\t" STRREF_FMT "\n\t" STRREF_FMT "\n", STRREF_ARG( input ), STRREF_ARG( partial.get_strref() ) ); 
		return _strdup( partial.c_str() );
	}
	return nullptr;
}

// if an expression could not be evaluated, add it along with
// the action to perform if it can be evaluated later.
void Asm::AddLateEval(int target, int pc, int scope_pc, strref expression, strref source_file, LateEval::Type type) {
	LateEval le;
	char* partial = PartialEval( expression );
	le.address = pc;
	le.scope = scope_pc;
	le.scope_depth = scope_depth;
	le.target = target;
	le.section = (int16_t)(&CurrSection() - &allSections[0]);
	le.rept = contextStack.curr().repeat_total - contextStack.curr().repeat;
	le.file_ref = -1; // current or xdef'd
	le.label.clear();
	le.expression = partial ? strref( partial ) : expression;
	le.expression_mem = partial;
	le.source_file = source_file;
	le.type = type;

	lateEval.push_back(le);
}

void Asm::AddLateEval(strref label, int pc, int scope_pc, strref expression, LateEval::Type type) {
	LateEval le;
	char* partial = PartialEval( expression );
	le.address = pc;
	le.scope = scope_pc;
	le.scope_depth = scope_depth;
	le.target = -1;
	le.label = label;
	le.section = (int16_t)(&CurrSection() - &allSections[0]);
	le.rept = contextStack.curr().repeat_total - contextStack.curr().repeat;
	le.file_ref = -1; // current or xdef'd
	le.expression = partial ? strref( partial ) : expression;
	le.expression_mem = partial;
	le.source_file.clear();
	le.type = type;

	lateEval.push_back(le);
}

// When a label is defined or a scope ends check if there are
// any related late label evaluators that can now be evaluated.
StatusCode Asm::CheckLateEval(strref added_label, int scope_end, bool print_missing_reference_errors) {
	bool evaluated_label = true;
	strref new_labels[MAX_LABELS_EVAL_ALL];
	int num_new_labels = 0;
	if (added_label) { new_labels[num_new_labels++] = added_label; }

	bool all = !added_label;
	while (evaluated_label) {
		evaluated_label = false;
		std::vector<LateEval>::iterator i = lateEval.begin();
		while (i != lateEval.end()) {
			int value = 0;
			// check if this expression is related to the late change (new label or end of scope)
			bool check = all || num_new_labels==MAX_LABELS_EVAL_ALL;
			for (int l = 0; l<num_new_labels && !check; l++)
				check = i->expression.find(new_labels[l]) >= 0;
			if (!check && scope_end>0) {
				int gt_pos = 0;
				while (gt_pos>=0 && !check) {
					gt_pos = i->expression.find_at('%', gt_pos);
					if (gt_pos>=0) {
						if (i->expression[gt_pos+1]=='%') { gt_pos++; }
						else { check = true; }
						gt_pos++;
					}
				}
			}
			if (check) {
				struct EvalContext etx(i->address, i->scope, scope_end,
						i->type == LateEval::LET_BRANCH ? SectionId() : -1, i->rept);
				etx.scope_depth = i->scope_depth;
				etx.file_ref = i->file_ref;
				StatusCode ret = EvalExpression(i->expression, etx, value);
				if (ret == STATUS_OK || ret==STATUS_RELATIVE_SECTION) {
					// Check if target section merged with another section
					int trg = i->target;
					int sec = i->section;
					if (i->type != LateEval::LET_LABEL) {
					}
					bool resolved = true;
					switch (i->type) {
						case LateEval::LET_BYTE:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0) {
									resolved = false;
								} else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, 1, lastEvalShift);
									value = 0;
								}
							}
							if (trg>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetByte(trg, value);
							break;

						case LateEval::LET_DBL_BYTE:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0) {
									resolved = false;
								} else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, 1, lastEvalShift-8);
									allSections[sec].AddReloc(lastEvalValue, trg+1, lastEvalSection, 1, lastEvalShift);
									value = 0;
								}
							}
							if ((trg+1)>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetByte(trg, value>>8);
							allSections[sec].SetByte(trg+1, value);
							break;

						case LateEval::LET_ABS_REF:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0) {
									resolved = false;
								} else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, 2, lastEvalShift);
									value = 0;
								}
							}
							if ((trg+1)>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetWord(trg, value);
							break;

						case LateEval::LET_ABS_L_REF:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0) {
									resolved = false;
								} else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, 3, lastEvalShift);
									value = 0;
								}
							}
							if ((trg+2)>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetTriple(trg, value);
							break;

						case LateEval::LET_ABS_4_REF:
							if (ret==STATUS_RELATIVE_SECTION) {
								if (i->section<0) {
									resolved = false;
								} else {
									allSections[sec].AddReloc(lastEvalValue, trg, lastEvalSection, 4, lastEvalShift);
									value = 0;
								}
							}
							if ((trg+3)>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetQuad(trg, value);
							break;

						case LateEval::LET_BRANCH:
							value -= i->address+1;
							if (value<-128 || value>127) {
								i = lateEval.erase(i);
								return ERROR_BRANCH_OUT_OF_RANGE;
							} if (trg>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetByte(trg, value);
							break;

						case LateEval::LET_BRANCH_16:
							value -= i->address+2;
							if (trg>=allSections[sec].size()) {
								return ERROR_SECTION_TARGET_OFFSET_OUT_OF_RANGE;
							}
							allSections[sec].SetWord(trg, value);
							break;

						case LateEval::LET_LABEL: {
							Label *label = GetLabel(i->label, i->file_ref);
							if (!label) { return ERROR_LABEL_MISPLACED_INTERNAL; }
							label->value = value;
							label->evaluated = true;
							label->section = ret == STATUS_RELATIVE_SECTION ? i->section : -1;
							label->orig_section = i->section;
							if (num_new_labels<MAX_LABELS_EVAL_ALL) {
								new_labels[num_new_labels++] = label->label_name;
							}
							evaluated_label = true;
							char f = i->label[0], l = i->label.get_last();
							LabelAdded(label, f=='.' || f=='!' || f=='@' || f==':' || l=='$');
							break;
						}
						default:
							break;
					}
					if (resolved) { i = lateEval.erase(i); }
				} else {
					if (print_missing_reference_errors && ret!=STATUS_XREF_DEPENDENT) {
						PrintError(i->expression, ret, i->source_file);
						error_encountered = true;
					}
					++i;
				}
			} else { ++i; }
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
Label *Asm::GetLabel(strref label, bool reference_check) {
	uint32_t label_hash = label.fnv1a();
	uint32_t index = FindLabelIndex(label_hash, labels.getKeys(), labels.count());
	while (index < labels.count() && label_hash == labels.getKey(index)) {
		if (label.same_str(labels.getValue(index).label_name)) {
			Label *label = labels.getValues() + index;
			if (!reference_check) { label->referenced = true; }
			return label;
		}
		index++;
	}
	return nullptr;
}

// Get a protected label record from a file if it exists
Label *Asm::GetLabel(strref label, int file_ref) {
	if (file_ref>=0 && file_ref<(int)externals.size()) {
		ExtLabels &labs = externals[file_ref];
		uint32_t label_hash = label.fnv1a();
		uint32_t index = FindLabelIndex(label_hash, labs.labels.getKeys(), labs.labels.count());
		while (index < labs.labels.count() && label_hash == labs.labels.getKey(index)) {
			if (label.same_str(labs.labels.getValue(index).label_name)) {
				Label *label = labs.labels.getValues()+index;
				label->referenced = true;
				return label;
			}
			index++;
		}
	}
	return GetLabel(label);
}

// If exporting labels, append this label to the list
void Asm::LabelAdded(Label *pLabel, bool local) {
	if (pLabel && pLabel->evaluated) {
		if (map.size()==map.capacity()) {
			map.reserve(map.size()+256);
		}
		MapSymbol sym;
		sym.name = pLabel->label_name;
		sym.section = (int16_t)pLabel->section;
		sym.orig_section = (int16_t)pLabel->orig_section;
		sym.value = pLabel->value;
		sym.local = local;
		sym.borrowed = pLabel->borrowed;
		pLabel->mapIndex = pLabel->evaluated ? -1 : (int)map.size();
		map.push_back(sym);
	}
}

// Add a label entry
Label* Asm::AddLabel(uint32_t hash) {
	uint32_t index = FindLabelIndex(hash, labels.getKeys(), labels.count());
	labels.insert(index, hash);
	return labels.getValues() + index;
}

// mark a label as a local label
void Asm::MarkLabelLocal(strref label, bool scope_reserve) {
	LocalLabelRecord rec;
	rec.label = label;
	rec.scope_depth = scope_depth;
	rec.scope_reserve = scope_reserve;
	localLabels.push_back(rec);
}

// find all local labels or up to given scope level and remove them
StatusCode Asm::FlushLocalLabels(int scope_exit) {
	StatusCode status = STATUS_OK;
	// iterate from end of local label records and early out if the label scope is lower than the current.
	std::vector<LocalLabelRecord>::iterator i = localLabels.end();
	while (i!=localLabels.begin()) {
		--i;
		if (i->scope_depth<scope_depth) { break; }
		strref label = i->label;
		StatusCode this_status = CheckLateEval(label);
		if (this_status>FIRST_ERROR) { status = this_status; }
		if (!i->scope_reserve || i->scope_depth<=scope_exit) {
			uint32_t index = FindLabelIndex(label.fnv1a(), labels.getKeys(), labels.count());
			while (index<labels.count()) {
				if (label.same_str_case(labels.getValue(index).label_name)) {
					labels.remove(index);
					break;
				}
				++index;
			}
			i = localLabels.erase(i);
		}
	}
	return status;
}

// Get a label pool by name
LabelPool* Asm::GetLabelPool(strref pool_name) {
	uint32_t pool_hash = pool_name.fnv1a();
	uint32_t ins = FindLabelIndex(pool_hash, labelPools.getKeys(), labelPools.count());
	while (ins < labelPools.count() && pool_hash == labelPools.getKey(ins)) {
		if (pool_name.same_str(labelPools.getValue(ins).pool_name)) {
			return &labelPools.getValue(ins);
		}
		ins++;
	}
	return nullptr;
}

// Add a label pool
StatusCode Asm::AddLabelPool(strref name, strref args) {
	uint32_t pool_hash = name.fnv1a();
	uint32_t ins = FindLabelIndex(pool_hash, labelPools.getKeys(), labelPools.count());
	uint32_t index = ins;
	while (index < labelPools.count() && pool_hash == labelPools.getKey(index)) {
		if (name.same_str(labelPools.getValue(index).pool_name)) {
			return ERROR_LABEL_POOL_REDECLARATION;
		}
		index++;
	}
	// check that there is at least one valid address
	int ranges = 0;
	int num32 = 0;
	uint32_t aRng[256];
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	while (strref arg = args.split_token_trim(',')) {
		strref start = arg[0]=='(' ? arg.scoped_block_skip() : arg.split_token_trim('-');
		int addr0 = 0, addr1 = 0;
		if (STATUS_OK!=EvalExpression(start, etx, addr0)) {
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		}
		if (STATUS_OK!=EvalExpression(arg, etx, addr1)) {
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		}
		if (addr1<=addr0||addr0<0) {
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		}
		aRng[ranges++] = (uint32_t)addr0;
		aRng[ranges++] = (uint32_t)addr1;
		num32 += (addr1-addr0+15)>>4;
		if (ranges>2||num32>((MAX_POOL_BYTES+15)>>4)) {
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		}
	}

	if (!ranges) { return ERROR_POOL_RANGE_EXPRESSION_EVAL; }

	LabelPool pool;
	pool.pool_name = name;
	pool.numRanges = (int16_t)(ranges>>1);
	pool.depth = 0;
	pool.start = aRng[0];
	pool.end = aRng[1];

	labelPools.insert(ins, pool_hash);
	LabelPool &poolValue = labelPools.getValue(ins);
	poolValue = pool;
	return STATUS_OK;
}


StatusCode Asm::AssignPoolLabel(LabelPool &pool, strref label) {
	if (pool_subpool.is_prefix_word(label)) {		// declaring a pool within another pool?
		label += pool_subpool.get_len();
		label.skip_whitespace();
		strref size = label;
		label = size.split_label();
		if (strref::is_number(size.get_first())) {
			uint32_t bytes = (uint32_t)size.atoi();
			if (!bytes) { return ERROR_POOL_RANGE_EXPRESSION_EVAL; }
			if (!GetLabelPool(label)) {
				uint32_t addr;
				StatusCode error = pool.Reserve(bytes, addr, (uint16_t)brace_depth);
				if( error == STATUS_OK ) {
					// permanently remove this chunk from the parent pool
					pool.end = addr;
					pool.depth = 0;
					uint32_t pool_hash = label.fnv1a();
					uint32_t ins = FindLabelIndex(pool_hash, labelPools.getKeys(), labelPools.count());
					labelPools.insert(ins, pool_hash);
					LabelPool &subPool = labelPools.getValue(ins);
					subPool.pool_name = label;
					subPool.numRanges = 1;
					subPool.depth = 0;
					subPool.start = addr;
					subPool.end = addr+bytes;
				}
				return error;
			} else { return ERROR_LABEL_POOL_REDECLARATION; }
		}
		return ERROR_POOL_RANGE_EXPRESSION_EVAL;
	}
	strref type = label;
	uint32_t bytes = 1;
	int sz = label.find_at( '.', 1 );
	if (sz > 0) {
		label = type.split( sz );
		++type;
		if (strref::is_number(type.get_first())) {
			bytes = (uint32_t)type.atoi();
		} else {
			switch (strref::tolower(type.get_first())) {
				case 'l': bytes = 4; break;
				case 't': bytes = 3; break;
				case 'd':
				case 'w': bytes = 2; break;
			}
		}
	}
	if (GetLabel(label)) { return ERROR_POOL_LABEL_ALREADY_DEFINED; }
	uint32_t addr;
	StatusCode error = pool.Reserve(bytes, addr, (uint16_t)brace_depth);
	if (error!=STATUS_OK) { return error; }
	Label *pLabel = AddLabel(label.fnv1a());
	pLabel->label_name = label;
	pLabel->pool_name = pool.pool_name;
	pLabel->evaluated = true;
	pLabel->section = -1;	// pool labels are section-less
	pLabel->orig_section = SectionId();
	pLabel->value = addr;
	pLabel->pc_relative = true;
	pLabel->constant = true;
	pLabel->external = false;
	pLabel->reference = false;
	pLabel->referenced = false;
	bool local = false;


	if (label[ 0 ] == '.' || label[ 0 ] == '@' || label[ 0 ] == '!' || label[ 0 ] == ':' || label.get_last() == '$') {
		local = true;
		MarkLabelLocal( label, true );
	}
	LabelAdded(pLabel, local);
	return error;
}

// Request a label from a pool
StatusCode sLabelPool::Reserve(uint32_t numBytes, uint32_t &ret_addr, uint16_t scope) {
	if (numBytes>(end-start)||depth==MAX_SCOPE_DEPTH) { return ERROR_OUT_OF_LABELS_IN_POOL; }
	if (!depth||scope!=scopeUsed[depth-1][1]) {
		scopeUsed[depth][0] = end;
		scopeUsed[depth][1] = scope;
		++depth;
	}
	end -= numBytes;
	ret_addr = end;
	return STATUS_OK;
}

// Release a label from a pool (at scope closure)
void sLabelPool::ExitScope(uint16_t scope) {
	if (depth && scopeUsed[depth-1][1]==scope) {
		end = scopeUsed[--depth][0];
	}
}

// Check if a label is marked as an xdef
bool Asm::MatchXDEF(strref label) {
	uint32_t hash = label.fnv1a();
	uint32_t pos = FindLabelIndex(hash, xdefs.getKeys(), xdefs.count());
	while (pos < xdefs.count() && xdefs.getKey(pos) == hash) {
		if (label.same_str_case(xdefs.getValue(pos))) { return true; }
		++pos;
	}
	return false;
}

// assignment of label (<label> = <expression>)
StatusCode Asm::AssignLabel(strref label, strref expression, bool make_constant, bool borrowed) {
	expression.trim_whitespace();
	int val = 0;
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	StatusCode status = EvalExpression(expression, etx, val);
	if (status!=STATUS_NOT_READY && status!=STATUS_OK && status!=STATUS_RELATIVE_SECTION) {
		return status;
	}
	Label *pLabel = GetLabel(label);
	if (pLabel) {
		if (pLabel->constant && pLabel->evaluated && val!=pLabel->value) {
			return (status==STATUS_NOT_READY) ? STATUS_OK : ERROR_MODIFYING_CONST_LABEL;
		}
	} else { pLabel = AddLabel(label.fnv1a()); pLabel->referenced = false; }

	pLabel->label_name = label;
	pLabel->pool_name.clear();
	pLabel->evaluated = status==STATUS_OK || status == STATUS_RELATIVE_SECTION;
	pLabel->section = status == STATUS_RELATIVE_SECTION ? lastEvalSection : -1;	// assigned labels are section-less
	pLabel->orig_section = lastEvalSection;
	pLabel->value = val;
	pLabel->mapIndex = -1;
	pLabel->pc_relative = false;
	pLabel->constant = make_constant;
	pLabel->external = MatchXDEF(label);
	pLabel->reference = false;
	pLabel->borrowed = borrowed;

	bool local = label[0]=='.' || label[0]=='@' || label[0]=='!' || label[0]==':' || label.get_last()=='$';
	if (!pLabel->evaluated) {
		AddLateEval(label, CurrSection().GetPC(), scope_address[scope_depth], expression, LateEval::LET_LABEL);
	}  else {
		if (local) { MarkLabelLocal(label); }
		LabelAdded(pLabel, local);
		return CheckLateEval(label);
	}
	return STATUS_OK;
}

// Adding a fixed address label
StatusCode Asm::AddressLabel(strref label)
{
	StatusCode status = STATUS_OK;
	Label *pLabel = GetLabel(label);
	bool constLabel = false;
	if (!pLabel) {
		pLabel = AddLabel(label.fnv1a());
		pLabel->referenced = false;	// if this label already exists but is changed then it may already have been referenced
	} else if (pLabel->constant && pLabel->value!=CurrSection().GetPC()) {
		return ERROR_MODIFYING_CONST_LABEL;
	} else { constLabel = pLabel->constant; }

	pLabel->label_name = label;
	pLabel->pool_name.clear();
	pLabel->section = CurrSection().IsRelativeSection() ? SectionId() : -1;	// address labels are based on section
	pLabel->orig_section = SectionId();
	pLabel->value = CurrSection().GetPC();
	pLabel->evaluated = true;
	pLabel->pc_relative = true;
	pLabel->external = MatchXDEF(label);
	pLabel->reference = false;
	pLabel->constant = constLabel;
	// Label Source Debug Origin: AddSrcDbg((int)CurrSection().GetPC(), SourceDebugType::Label, label.get());

	last_label = label;
	bool local = label[0]=='.' || label[0]=='@' || label[0]=='!' || label[0]==':' || label.get_last()=='$';
	if (directive_scope_depth > 0) { local = true; }
	LabelAdded(pLabel, local);	// TODO: in named scopes the label can still be referenced outside the scope directive
	if (local) { MarkLabelLocal(label); }
	status = CheckLateEval(label);
	if (!local && label[0]!=']') { // MERLIN: Variable label does not invalidate local labels
		StatusCode this_status = FlushLocalLabels();
		if (status<FIRST_ERROR && this_status>=FIRST_ERROR) {
			status = this_status;
		}
	}
	return status;
}

// include symbols listed from a .sym file or all if no listing
StatusCode Asm::IncludeSymbols(strref line) {
	strref symlist = line.before('"').get_trimmed_ws();
	line = line.between('"', '"');
	size_t size;
	if (char *buffer = LoadText(line, size)) {
		strref symfile(buffer, strl_t(size));
		while (symfile) {
			symfile.skip_whitespace();
			strref symstart = symfile;
			if (strref symline = symfile.line()) {
				int scope_start = symline.find('{');
				if (scope_start != 0) {
					strref symdef = symline.get_substr(0, scope_start);
					symdef.clip_trailing_whitespace();
					strref symtype = symdef.split_token(' ');
					strref label = symdef.split_token_trim('=');
					label = label.get_label();
					bool constant = symtype.same_str(".const");	// first word is either .label or .const
					if (symlist) {
						strref symchk = symlist;
						while (strref symwant = symchk.split_token_trim(',')) {
							if (symwant.same_str_case(label)) {
								Label* prevLabel = GetLabel(label);
								if (!prevLabel || (full_map && prevLabel->external))
									AssignLabel(label, symdef, constant);
								break;
							}
						}
					} else
						AssignLabel(label, symdef, constant);
				}
				if (scope_start >= 0) {
					symfile = symstart + scope_start;
					symfile.scoped_block_skip();
				}
			}
		}
		loadedData.push_back(buffer);
	} else
		return ERROR_COULD_NOT_INCLUDE_FILE;
	return STATUS_OK;
}

// Get a string record if it exists
StringSymbol *Asm::GetString(strref string_name)
{
	uint32_t string_hash = string_name.fnv1a();
	uint32_t index = FindLabelIndex(string_hash, strings.getKeys(), strings.count());
	while (index < strings.count() && string_hash == strings.getKey(index)) {
		if (string_name.same_str(strings.getValue(index).string_name))
			return strings.getValues() + index;
		index++;
	}
	return nullptr;
}

// Add or modify a string record
StringSymbol *Asm::AddString(strref string_name, strref string_value)
{
	StringSymbol *pStr = GetString(string_name);
	if (pStr==nullptr) {
		uint32_t string_hash = string_name.fnv1a();
		uint32_t index = FindLabelIndex(string_hash, strings.getKeys(), strings.count());
		strings.insert(index, string_hash);
		pStr = strings.getValues() + index;
		pStr->string_name = string_name;
		pStr->string_value.invalidate();
		pStr->string_value.clear();
	}
	if (pStr->string_value.cap()) {
		free(pStr->string_value.charstr());
		pStr->string_value.invalidate();
		pStr->string_value.clear();
	}
	pStr->string_const = string_value;
	return pStr;
}

// append a string to another string
StatusCode sStringSymbols::Append(strref append)
{
	if (!append)
		return STATUS_OK;

	strl_t add_len = append.get_len();

	if (!string_value.cap()) {
		strl_t new_len = (add_len + 0xff)&(~(strl_t)0xff);
		char *buf = (char*)malloc(new_len);
		if (!buf)
			return ERROR_OUT_OF_MEMORY;
		string_value.set_overlay(buf, new_len);
		string_value.copy(string_const);
	} else if (string_value.cap() < (string_value.get_len() + add_len)) {
		strl_t new_len = (string_value.get_len() + add_len + 0xff)&(~(strl_t)0xff);
		char *buf = (char*)malloc(new_len);
		if (!buf)
			return ERROR_OUT_OF_MEMORY;
		strovl ovl(buf, new_len);
		ovl.copy(string_value.get_strref());
		free(string_value.charstr());
		string_value.set_overlay(buf, new_len);
	}
	string_const.clear();
	string_value.append(append);
	return STATUS_OK;
}

StatusCode Asm::ParseStringOp(StringSymbol *pStr, strref line)
{
	line.skip_whitespace();
	if (line[0] == '+')
		++line;
	for (;;) {
		line.skip_whitespace();
		if (line[0] == '"') {
			strref substr = line.between('"', '"');
			line += substr.get_len() + 2;
			pStr->Append(substr);
		} else {
			strref label = line.split_range(Merlin() ?
				label_end_char_range_merlin : label_end_char_range);
			if (StringSymbol *pStr2 = GetString(label))
				pStr->Append(pStr2->get());
			else if (Label *pLabel = GetLabel(label)) {
				if (!pLabel->evaluated)
					return ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY;
				strown<32> lblstr;
				lblstr.sprintf("$%x", pLabel->value);
				pStr->Append(lblstr.get_strref());
			} else
				break;
		}
		line.skip_whitespace();
		if (!line || line[0] != '+')
			break;
		++line;
		line.skip_whitespace();
	}
	return STATUS_OK;
}

StatusCode Asm::StringAction(StringSymbol *pStr, strref line)
{
	line.skip_whitespace();
	if (line[0] == '+' && line[1] == '=') {	// append strings
		line += 2;
		line.skip_whitespace();
		return ParseStringOp(pStr, line);
	} else if (line[0] == '=') {
		++line;
		line.skip_whitespace();
		pStr->clear();
		return ParseStringOp(pStr, line);
	}
	strref str = pStr->string_value.valid() ?
		pStr->string_value.get_strref() : pStr->string_const;
	if (!str) { return STATUS_OK; }
	char *macro = (char*)malloc(str.get_len());
	strovl mac(macro, str.get_len());
	mac.copy(str);
	mac.replace("\\n", "\n");
	loadedData.push_back(macro);
	PushContext(contextStack.curr().source_name, mac.get_strref(), mac.get_strref());
	return STATUS_OK;
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
	if (conditional_depth>contextStack.curr().conditional_ctx)
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
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	bool invert = line.get_first()=='!';
	if (invert)
		++line;
	int value;
	if (STATUS_OK != EvalExpression(line, etx, value))
		return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
	result = (value!=0 && !invert) || (value==0 && invert);
	return STATUS_OK;
}

// Add a folder for including files
void Asm::AddIncludeFolder(strref path)
{
	if (!path)
		return;
	for (std::vector<strref>::const_iterator i = includePaths.begin(); i!=includePaths.end(); ++i) {
		if (path.same_str(*i))
			return;
	}
	if (includePaths.size()==includePaths.capacity())
		includePaths.reserve(includePaths.size() + 16);
	includePaths.push_back(path);
}

// unique key binary search
int LookupOpCodeIndex(uint32_t hash, OPLookup *lookup, int count)
{
	int first = 0;
	while (count!=first) {
		int index = (first+count)/2;
		uint32_t read = lookup[index].op_hash;
		if (hash==read) {
			return index;
		} else if (hash>read)
			first = index+1;
		else
			count = index;
	}
	return -1;	// index not found
}

// Encountered a REPT or LUP
StatusCode Asm::Directive_Rept(strref line)
{
	SourceContext &ctx = contextStack.curr();
	strref read_source = ctx.read_source;
	if (read_source.is_substr(line.get())) {
		read_source.skip(strl_t(line.get() - read_source.get()));
		strref expression;
		if (Merlin() || end_macro_directive) {
			expression = line;			// Merlin repeat body begins next line
			read_source.line();
		} else {
			int block = read_source.find('{');
			if (block<0)
				return ERROR_REPT_MISSING_SCOPE;
			expression = read_source.get_substr(0, block);
			read_source += block;
			read_source.skip_whitespace();
		}
		expression.trim_whitespace();
		int count;
		struct EvalContext etx;
		SetEvalCtxDefaults(etx);
		if (STATUS_OK != EvalExpression(expression, etx, count))
			return ERROR_REPT_COUNT_EXPRESSION;
		strref recur;
		if (Merlin() || end_macro_directive) {
			recur = read_source;		// Merlin repeat body ends at "--^"
			while (strref next_line = read_source.line()) {
				next_line = next_line.before_or_full(';');
				next_line = next_line.before_or_full(c_comment);
				int term = next_line.find(end_macro_directive ? "endr" : "--^");
				if (term >= 0) {
					recur = recur.get_substr(0, strl_t(next_line.get() + term - recur.get()));
					break;
				}
			}
		} else
			recur = read_source.scoped_block_skip();
		ctx.next_source = read_source;
		PushContext(ctx.source_name, ctx.source_file, recur, count);
	}
	return STATUS_OK;
}

// macro: create an assembler macro
StatusCode Asm::Directive_Macro(strref line)
{
	strref read_source = contextStack.curr().read_source.get_skip_ws();
	if (!Merlin() && read_source.is_substr(line.get()))
		read_source.skip(strl_t(line.get()-read_source.get()));
	if (read_source) {
		StatusCode error = AddMacro(read_source, contextStack.curr().source_name,
									contextStack.curr().source_file, read_source);
		contextStack.curr().next_source = read_source;
		return error;
	}
	return STATUS_OK;
}

// string: create a symbolic string
StatusCode Asm::Directive_String(strref line)
{
	line.skip_whitespace();
	strref string_name = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
	if (line[0]=='=' || keyword_equ.is_prefix_word(line)) {
		line.next_word_ws();
		strref substr = line;
		if (line[0] == '"') {
			substr = line.between('"', '"');
			line += substr.get_len() + 2;
			StringSymbol *pStr = AddString(string_name, substr);
			if (pStr == nullptr)
				return ERROR_OUT_OF_MEMORY;
			line.skip_whitespace();
			if (line[0] == '+')
				return ParseStringOp(pStr, line);
		} else {
			StringSymbol *pStr = AddString(string_name, strref());
			return ParseStringOp(pStr, line);
		}
	} else {
		if (!AddString(string_name, strref()))
			return ERROR_OUT_OF_MEMORY;
	}
	return STATUS_OK;
}

StatusCode Asm::Directive_Function(strref line)
{
	line.skip_whitespace();
	strref function_name = line.split_label(), params;

	line.skip_whitespace();
	if (line.get_first() == '(') {
		params = line.scoped_block_comment_skip();
		params.trim_whitespace();
	}

	line.skip_whitespace();
	userFunctions.Add(function_name, params, line);

	return STATUS_OK;
}



StatusCode Asm::Directive_Undef(strref line)
{
	strref name = line.split_range_trim(Merlin() ? label_end_char_range_merlin : label_end_char_range);
	uint32_t name_hash = name.fnv1a();
	uint32_t index = FindLabelIndex(name_hash, labels.getKeys(), labels.count());
	while (index < labels.count() && name_hash == labels.getKey(index)) {
		if (name.same_str(labels.getValue(index).label_name)) {
			labels.remove(index);
			return STATUS_OK;
		}
		index++;
	}
	index = FindLabelIndex(name_hash, strings.getKeys(), strings.count());
	while (index < strings.count() && name_hash == strings.getKey(index)) {
		if (name.same_str(strings.getValue(index).string_name)) {
			StringSymbol str = strings.getValue(index);
			if (str.string_value.cap()) {
				free(str.string_value.charstr());
				str.string_value.invalidate();
			}
			strings.remove(index);
			return STATUS_OK;
		}
		index++;
	}
	return STATUS_OK;
}

// include: read in a source file and assemble at this point
StatusCode Asm::Directive_Include(strref line)
{
	strref file = line.between('"', '"');
	if (!file)								// MERLIN: No quotes around PUT filenames
		file = line.split_range(filename_end_char_range);
	size_t size = 0;
	char *buffer = LoadText(file, size);
	if (buffer) {
		loadedData.push_back(buffer);
		strref src(buffer, strl_t(size));
		PushContext(file, src, src);
	} else if (Merlin()) {
		// MERLIN include file name rules
		if (file[0] >= '!' && file[0] <= '&')
			buffer = LoadText(file + 1, size);
		if (buffer) {
			loadedData.push_back(buffer);		// MERLIN: prepend with !-& to not auto-prepend with T.
			strref src(buffer, strl_t(size));
			PushContext(file+1, src, src);
		} else {
			strown<512> fileadd(file[0]>='!' && file[0]<='&' ? (file+1) : file);
			fileadd.append(".s");
			buffer = LoadText(fileadd.get_strref(), size);
			if (buffer) {
				loadedData.push_back(buffer);	// MERLIN: !+filename appends .S to filenames
				strref src(buffer, strl_t(size));
				PushContext(file, src, src);
			} else {
				fileadd.copy("T.");				// MERLIN: just filename prepends T. to filenames
				fileadd.append(file[0]>='!' && file[0]<='&' ? (file+1) : file);
				buffer = LoadText(fileadd.get_strref(), size);
				if (buffer) {
					loadedData.push_back(buffer);
					strref src(buffer, strl_t(size));
					PushContext(file, src, src);
				}
			}
		}
	}
	if (!size)
		return ERROR_COULD_NOT_INCLUDE_FILE;
	return STATUS_OK;
}

// incbin: import binary data in place
StatusCode Asm::Directive_Incbin(strref line, int skip, int len)
{
	line = line.between('"', '"');
	strown<512> filename(line);
	size_t size = 0;
	if (char *buffer = LoadBinary(line, size)) {
		int bin_size = (int)size - skip;
		if (len && bin_size>len)
			bin_size = len;
		if (bin_size>0)
			AddBin((const uint8_t*)buffer+skip, bin_size);
		free(buffer);
		return STATUS_OK;
	}

	return ERROR_COULD_NOT_INCLUDE_FILE;
}

// import is a catch-all file reference
StatusCode Asm::Directive_Import(strref line)
{
	line.skip_whitespace();

	int skip = 0;	// binary import skip this amount
	int len = 0;	// binary import load up to this amount
	strref param;	// read out skip & max len parameters
	int q = line.find('"');
	if (q>=0) {
		param = line + q;
		param.split_lang();
		param.trim_whitespace();
		if (param[0]==',') {
			++param;
			param.skip_whitespace();
			if (param) {
				struct EvalContext etx;
				SetEvalCtxDefaults(etx);
				EvalExpression(param.split_token_trim_track_parens(','), etx, skip);
				if (param) { EvalExpression(param, etx, len); }
			}
		}
	}
	
	if (line[0]=='"')
		return Directive_Incbin(line, skip, len);
	else if (import_source.is_prefix_word(line)) {
		line += import_source.get_len();
		line.skip_whitespace();
		return Directive_Include(line);
	} else if (import_binary.is_prefix_word(line)) {
		line += import_binary.get_len();
		line.skip_whitespace();
		return Directive_Incbin(line, skip, len);
	} else if (import_c64.is_prefix_word(line)) {
		line += import_c64.get_len();
		line.skip_whitespace();
		return Directive_Incbin(line, 2+skip, len); // 2 = load address skip size
	} else if (import_text.is_prefix_word(line)) {
		line += import_text.get_len();
		line.skip_whitespace();
		strref text_type = "petscii";
		while (line[0]!='"') {
			strref word = line.get_word_ws();
			if (word.same_str("petscii") || word.same_str("petscii_shifted")) {
				text_type = line.get_word_ws();
				line += text_type.get_len();
				line.skip_whitespace();
			} else if (StringSymbol *pStr = GetString(line.get_word_ws())) {
				line = pStr->get();
				break;
			}
		}
		CurrSection().AddText(line, text_type);
		return STATUS_OK;
	} else if (import_object.is_prefix_word(line)) {
		line += import_object.get_len();
		line.trim_whitespace();
		return ReadObjectFile(line[0]=='"' ? line.between('"', '"') : line);
	} else if (import_symbols.is_prefix_word(line)) {
		line += import_symbols.get_len();
		line.skip_whitespace();
		return IncludeSymbols(line);
	}
	
	return STATUS_OK;
}

// org / pc: current address of code
StatusCode Asm::Directive_ORG(strref line)
{
	int addr;
	if (line[0]=='=')
		++line;
	else if (keyword_equ.is_prefix_word(line))	// optional '=' or equ
		line.next_word_ws();
	line.skip_whitespace();
	
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	StatusCode error = EvalExpression(line, etx, addr);
	if (error != STATUS_OK)
		return (error == STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT) ?
			ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;

	// Section immediately followed by ORG reassigns that section to be fixed
	Section &currSection = CurrSection();
	if (currSection.size()==0 && !currSection.IsDummySection() && !currSection.address_assigned) {
		if (currSection.type == ST_ZEROPAGE && addr >= 0x100)
			return ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE;
		AssignAddressToSection(SectionId(), addr);
	} else
		SetSection(strref(), addr);
	return STATUS_OK;
}

// load: address for target to load code at
StatusCode Asm::Directive_LOAD(strref line)
{
	int addr;
	if (line[0]=='=' || keyword_equ.is_prefix_word(line))
		line.next_word_ws();

	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	StatusCode error = EvalExpression(line, etx, addr);
	if (error != STATUS_OK)
		return (error == STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT) ?
			ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;

	CurrSection().SetLoadAddress(addr);
	return STATUS_OK;
}

StatusCode Asm::Directive_MERGE(strref line)
{
	int first_section = -1;
	strref section_name = line.split_label();

	// get the first section that matches the first name and has an assigned address
	for (size_t section_id = 0; section_id!=allSections.size(); ++section_id) {
		const Section &section = allSections[section_id];
		if (!section.IsMergedSection()&&section.name.same_str(section_name)) {
			if (first_section<0||!allSections[first_section].IsRelativeSection()) {
				first_section = (int)section_id;
			}
		}
	}
	if (first_section<0) { return ERROR_NOT_A_SECTION; }

	// merge all sections as defined by the line
	while (section_name) {
		for (size_t section_id = 0; section_id!=allSections.size(); ++section_id) {
			const Section &section = allSections[section_id];
			if (section_id!=first_section&&!section.IsMergedSection()&&section.IsRelativeSection()) {
				if (section.name.same_str(section_name)) {
					StatusCode result = MergeSections(first_section, (int)section_id);
					if (result!=STATUS_OK) { return result; }
				}
			}
		}
		if (line[0]==',') { ++line; }
		section_name = line.split_label();
	}
	return STATUS_OK;
}

// MERLIN version of AD_LINK, which is more like AD_INCOBJ + link to current section
StatusCode Asm::Directive_LNK(strref line)
{
	strref file = line.between('"', '"');
	if (!file)		// MERLIN: No quotes around include filenames
		file = line.split_range(filename_end_char_range);
	int section_id = SectionId();
	StatusCode error = ReadObjectFile(file, SectionId());
	// restore current section
	current_section = &allSections[section_id];
	return error;
}

// this stores a string that when matched with a label will make that label external
StatusCode Asm::Directive_XDEF(strref line)
{
	line.trim_whitespace();
	if (strref xdef = line.split_range(Merlin() ?
			label_end_char_range_merlin : label_end_char_range)) {
		char f = xdef.get_first();
		char e = xdef.get_last();
		if (f != '.' && f != '!' && f != '@' && e != '$') {
			uint32_t hash = xdef.fnv1a();
			uint32_t pos = FindLabelIndex(hash, xdefs.getKeys(), xdefs.count());
			while (pos < xdefs.count() && xdefs.getKey(pos) == hash) {
				if (xdefs.getValue(pos).same_str_case(xdef))
					return STATUS_OK;
				++pos;
			}
			xdefs.insert(pos, hash);
			xdefs.getValues()[pos] = xdef;
		}
	}
	return STATUS_OK;
}

StatusCode Asm::Directive_XREF(strref label)
{
	// XREF already defined label => no action
	if (!GetLabel(label)) {
		Label *pLabelXREF = AddLabel(label.fnv1a());
		pLabelXREF->label_name = label;
		pLabelXREF->pool_name.clear();
		pLabelXREF->section = -1;	// address labels are based on section
		pLabelXREF->value = 0;
		pLabelXREF->evaluated = true;
		pLabelXREF->pc_relative = true;
		pLabelXREF->external = true;
		pLabelXREF->constant = false;
		pLabelXREF->reference = true;
		pLabelXREF->referenced = false;	// referenced is only within the current object file
	}
	return STATUS_OK;
}

// dc.b, dc.w, dc.t, dc.l, ADR, ADRL, bytes, words, long
StatusCode Asm::Directive_DC(strref line, int width, strref source_file, bool little_endian)
{
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	line.trim_whitespace();
	while (strref exp_dc = line.split_token_trim_track_parens(',')) {
		int value = 0;
		if (!CurrSection().IsDummySection()) {
			if (Merlin() && exp_dc.get_first() == '#')	// MERLIN allows for an immediate declaration on data
				++exp_dc;
			StatusCode error = EvalExpression(exp_dc, etx, value);
			if (error > STATUS_XREF_DEPENDENT)
				break;
			else if (error == STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT) {
				static LateEval::Type sizes[] = {
					LateEval::LET_BYTE,
					LateEval::LET_ABS_REF,
					LateEval::LET_ABS_L_REF,
					LateEval::LET_ABS_4_REF
				};
				LateEval::Type type = sizes[width - 1];
				if (!little_endian && width == 2) {
					type = LateEval::LET_DBL_BYTE;
				}

				AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], exp_dc, source_file, type);	
			} else if (error == STATUS_RELATIVE_SECTION) {
				value = 0;
				if (little_endian || width == 1) {
					CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset(), lastEvalSection, (int8_t)width, (int8_t)lastEvalShift);
				} else {
					// big endian needs 1 reloc for each byte 
					int shift = lastEvalShift + 8 - width * 8;
					for (int i = 0; i < width; ++i, shift += 8) {
						CurrSection().AddReloc(lastEvalValue, CurrSection().DataOffset() + i, lastEvalSection, 1, (int8_t)shift);
					}
				}
			}
		}
		if (little_endian) {
			uint8_t bytes[4] = {
				(uint8_t)value, (uint8_t)(value >> 8),
				(uint8_t)(value >> 16), (uint8_t)(value >> 24) };
			AddBin(bytes, width);
		} else {
			uint8_t bytes[4] = {
				(uint8_t)(value >> 24), (uint8_t)(value >> 16),
				(uint8_t)(value >> 8), (uint8_t)value };
			AddBin(bytes + 4 - width, width);
		}
	}
	return STATUS_OK;
}

// ds/ds.b/ds.w/ds.t/ds.l
StatusCode Asm::Directive_DS(strref line)
{
	int width = 1;
	int value;
	if (line.get_first() == '.' && strref::is_alphabetic(line[1])) {
		switch (strref::tolower(line[1])) {
			case 'b': break;
			case 'w': width = 2; break;
			case 't': width = 3; break;
			case 'l': width = 4; break;
		}
		line += 2;
		line.skip_whitespace();
	}
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	strref size = line.split_token_trim_track_parens(',');
	if (STATUS_OK != EvalExpression(size, etx, value))
		return ERROR_DS_MUST_EVALUATE_IMMEDIATELY;
	int fill = 0;
	if (line && STATUS_OK != EvalExpression(line, etx, fill))
		return ERROR_DS_MUST_EVALUATE_IMMEDIATELY;
	value *= width;
	if (value > 0) {
		for (int n = 0; n < value; n++)
			AddByte(fill);
	} else if (value) {
		CurrSection().AddAddress(value);
		if (CurrSection().type == ST_ZEROPAGE && CurrSection().address > 0x100)
			return ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE;
	}
	return STATUS_OK;
}

StatusCode Asm::Directive_ALIGN(strref line)
{
	if (line) {
		if (line[0] == '=' || keyword_equ.is_prefix_word(line))
			line.next_word_ws();
		struct EvalContext etx;
		SetEvalCtxDefaults(etx);
		int value;
		int status = EvalExpression(line, etx, value);
		if (status == STATUS_NOT_READY || status == STATUS_XREF_DEPENDENT)
			return ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY;
		if (status == STATUS_OK && value>0) {
			if (CurrSection().address_assigned) {
				int add = (CurrSection().GetPC() + value - 1) % value;
				for (int a = 0; a < add; a++)
					AddByte(0);
			} else
				CurrSection().align_address = value;
		}
	}
	return STATUS_OK;
}

StatusCode Asm::Directive_EVAL(strref line)
{
	int value = 0;
	strref description = line.find(':') >= 0 ? line.split_token_trim(':') : strref();
	line.trim_whitespace();
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	strref lab1 = line;
	lab1 = lab1.split_token_any_trim(Merlin() ? label_end_char_range_merlin : label_end_char_range);
	StringSymbol *pStr = line.same_str_case(lab1) ? GetString(lab1) : nullptr;

	if (line && EvalExpression(line, etx, value) == STATUS_OK) {
		if (description) {
			if (pStr != nullptr) {
				printf("EVAL(%d): " STRREF_FMT ": \"" STRREF_FMT "\" = \"" STRREF_FMT "\" = $%x\n",
					contextStack.curr().source_file.count_lines(description) + 1, STRREF_ARG(description), STRREF_ARG(line), STRREF_ARG(pStr->get()), value);
			} else {
				printf("EVAL(%d): " STRREF_FMT ": \"" STRREF_FMT "\" = $%x\n",
					contextStack.curr().source_file.count_lines(description) + 1, STRREF_ARG(description), STRREF_ARG(line), value);
			}
		} else {
			if (pStr != nullptr) {
				printf("EVAL(%d): \"" STRREF_FMT "\" = \"" STRREF_FMT "\" = $%x\n",
					contextStack.curr().source_file.count_lines(line) + 1, STRREF_ARG(line), STRREF_ARG(pStr->get()), value);
			} else {
				printf("EVAL(%d): \"" STRREF_FMT "\" = $%x\n",
					contextStack.curr().source_file.count_lines(line) + 1, STRREF_ARG(line), value);
			}
		}
	} else if (description) {
		if (pStr != nullptr) {
			printf("EVAL(%d): " STRREF_FMT ": \"" STRREF_FMT "\" = \"" STRREF_FMT "\"\n",
				contextStack.curr().source_file.count_lines(description) + 1, STRREF_ARG(description), STRREF_ARG(line), STRREF_ARG(pStr->get()));
		} else {
			printf("EVAL(%d): \"" STRREF_FMT ": " STRREF_FMT"\"\n",
				contextStack.curr().source_file.count_lines(description) + 1, STRREF_ARG(description), STRREF_ARG(line));
		}
	} else {
		if (pStr != nullptr) {
			printf("EVAL(%d): \"" STRREF_FMT "\" = \"" STRREF_FMT "\"\n",
				contextStack.curr().source_file.count_lines(line) + 1, STRREF_ARG(line), STRREF_ARG(pStr->get()));
		} else {
			printf("EVAL(%d): \"" STRREF_FMT "\"\n",
				contextStack.curr().source_file.count_lines(line) + 1, STRREF_ARG(line));
		}
	}
	return STATUS_OK;
}

StatusCode Asm::Directive_HEX(strref line)
{
	uint8_t b = 0, v = 0;
	while (line) {	// indeterminable length, can't read hex to int
		char c = *line.get();
		++line;
		if (c == ',') {
			if (b) // probably an error but seems safe
				AddByte(v);
			b = 0;
			line.skip_whitespace();
		} else {
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
		return ERROR_HEX_WITH_ODD_NIBBLE_COUNT;
	return STATUS_OK;
}

StatusCode Asm::Directive_ENUM_STRUCT(strref line, AssemblerDirective dir)
{
	strref read_source = contextStack.curr().read_source;
	if (read_source.is_substr(line.get())) {
		strref struct_name = line.get_word();
		line.skip(struct_name.get_len());
		line.skip_whitespace();
		read_source.skip(strl_t(line.get() - read_source.get()));
		if (read_source[0] == '{') {
			if (dir == AD_STRUCT)
				BuildStruct(struct_name, read_source.scoped_block_skip());
			else
				BuildEnum(struct_name, read_source.scoped_block_skip());
		} else
			return dir == AD_STRUCT ? ERROR_STRUCT_CANT_BE_ASSEMBLED :
			ERROR_ENUM_CANT_BE_ASSEMBLED;
		contextStack.curr().next_source = read_source;
	} else
		return ERROR_STRUCT_CANT_BE_ASSEMBLED;
	return STATUS_OK;
}

// Action based on assembler directive
StatusCode Asm::ApplyDirective(AssemblerDirective dir, strref line, strref source_file)
{
	StatusCode error = STATUS_OK;
	if (!ConditionalAsm()) {	// If conditionally blocked from assembling only check conditional directives
		if (dir!=AD_IF && dir!=AD_IFDEF && dir!=AD_ELSE && dir!=AD_ELIF && dir!=AD_ELSE && dir!=AD_ENDIF)
			return STATUS_OK;
	}
	struct EvalContext etx;
	SetEvalCtxDefaults(etx);
	switch (dir) {
		case AD_CPU:
			for (int c = 0; c < nCPUs; c++) {
				if (line.same_str(aCPUs[c].name)) {
					if (c != cpu)
						SetCPU((CPUIndex)c);
					return STATUS_OK;
				}
			}
			return ERROR_CPU_NOT_SUPPORTED;

		case AD_EXPORT:
			if (import_means_xref) { return Directive_XDEF(line); }
			line.trim_whitespace();
			CurrSection().export_append = line.split_label();
			break;

		case AD_ORG:
			return Directive_ORG(line);

		case AD_LOAD:
			return Directive_LOAD(line);

		case AD_SECTION:
			SetSection(line);
			break;

		case AD_MERGE:
			return Directive_MERGE(line);

		case AD_LINK:
			return LinkSections(line.get_trimmed_ws());

		case AD_LNK:
			return Directive_LNK(line);

		case AD_INCOBJ: {
			strref file = line.between('"', '"');
			if (!file)		// MERLIN: No quotes around include filenames
				file = line.split_range(filename_end_char_range);
			error = ReadObjectFile(file);
			break;
		}

		case AD_XDEF:
			return Directive_XDEF(line.get_trimmed_ws());

		case AD_XREF:
			Directive_XREF(line.split_range_trim(
				Merlin() ? label_end_char_range_merlin : label_end_char_range));
			break;

		case AD_ENT:	// MERLIN version of xdef, makes most recently defined label external
			if (Label *pLastLabel = GetLabel(last_label))
				pLastLabel->external = true;
			break;

		case AD_EXT:
			Directive_XREF(last_label);
			break;

		case AD_ALIGN:		// align: align address to multiple of value, fill space with 0
			return Directive_ALIGN(line);

		case AD_EVAL:		// eval: display the result of an expression in stdout
			return Directive_EVAL(line);
		
		case AD_BYTES:		// bytes: add bytes by comma separated values/expressions
			return Directive_DC(line, 1, source_file);

		case AD_WORDS:		// words: add words (16 bit values) by comma separated values
			return Directive_DC(line, 2, source_file);

		case AD_DBL_BYTES:	// DDB: Merlin store 2-byte word, big endian format.
			return Directive_DC(line, 2, source_file, false);
			
		case AD_ADR:		// ADR: MERLIN store 3 byte word
			return Directive_DC(line, 3, source_file);

		case AD_ADRL:		// ADRL: MERLIN store 4 byte word
			return Directive_DC(line, 4, source_file);

		case AD_DC: {
			int width = 1;
			if (line[0]=='.') {
				++line;
				switch (strref::tolower(line.get_first())) {
					case 'b': width = 1; break;
					case 'w': width = 2; break;
					case 't': width = 3; break;
					case 'l': width = 4; break;
					default:
						return ERROR_BAD_TYPE_FOR_DECLARE_CONSTANT;
				}
				++line;
			}
			return Directive_DC(line, width, source_file);
		}
			
		case AD_HEX:
			return Directive_HEX(line);

		case AD_EJECT:
			line.clear();
			break;
		case AD_USR:
			line.clear();
			break;

		case AD_CYC:
			list_flags |= cycle_counter_level ? ListLine::CYCLES_STOP : ListLine::CYCLES_START;
			cycle_counter_level = !!cycle_counter_level;
			break;
			
		case AD_SAV:
			line.trim_whitespace();
			if (line.has_prefix(export_base_name))
				line.skip(export_base_name.get_len());
			if (line)
				CurrSection().export_append = line.split_label();
			AssignAddressToGroup();
			break;
			
		case AD_XC:			// XC: MERLIN version of setting CPU
			if (strref("off").is_prefix_word(line))
				SetCPU(CPU_6502);
			else if (strref("xc").is_prefix_word(line))
				SetCPU(CPU_65816);
			else if (cpu==CPU_65C02)
				SetCPU(CPU_65816);
			else
				SetCPU(CPU_65C02);
			break;
			
		case AD_TEXT: {		// text: add text within quotes
			strref text_prefix;
			if (line[0]=='[') {
				strref str = line.scoped_block_skip().get_trimmed_ws();
				if (StringSymbol *StringSym = GetString(str)) {
					line.skip_whitespace();
					if (line[0] == '"')
						line = line.between('"', '"');
					CurrSection().AddIndexText(StringSym, line);
					break;
				}
			}
			while (line[0] != '"') {
				strref word = line.get_word_ws();
				if (word.same_str("petscii") || word.same_str("petscii_shifted") ||
					word.same_str("c64screen")) {
					text_prefix = line.get_word_ws();
					line += text_prefix.get_len();
					line.skip_whitespace();
				} else if (StringSymbol *pStr = GetString(line.get_word_ws())) {
					line = pStr->get();
					break;
				}
			}
			if (line[0] == '"')
				line = line.between('"', '"');
			CurrSection().AddText(line, text_prefix);
			break;
		}
		case AD_MACRO:
			error = Directive_Macro(line);
			break;

		case AD_INCLUDE:	// assemble another file in place
			return Directive_Include(line);
			
		case AD_INCBIN:
			return Directive_Incbin(line);
			
		case AD_IMPORT:
			if (import_means_xref) { return Directive_XREF(line); }
			return Directive_Import(line);

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

		case AD_STRING:
			return Directive_String(line);

		case AD_FUNCTION:
			return Directive_Function(line);
			
		case AD_UNDEF:
			return Directive_Undef(line);

		case AD_INCSYM:
			return IncludeSymbols(line);

		case AD_LABPOOL: {
			strref name = line.split_range_trim(word_char_range, line[0]=='.' ? 1 : 0);
			AddLabelPool(name, line);
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
				// ifdef doesn't need to evaluate the value, just determine if it exists or not
				strref label = line.split_range_trim(label_end_char_range);
				if( GetLabel(label, etx.file_ref) )
					ConsumeConditional();
				else
					SetConditional();
			}
			break;
			
		case AD_IFNDEF:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
				// ifdef doesn't need to evaluate the value, just determine if it exists or not
				strref label = line.split_range_trim(label_end_char_range);
				if (!GetLabel(label, etx.file_ref))
					ConsumeConditional();
				else
					SetConditional();
			}
			break;

		case AD_IFCONST:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
											// ifdef doesn't need to evaluate the value, just determine if it exists or not
				strref label_name = line.split_range_trim(label_end_char_range);
				if (Label* label = GetLabel(label_name, etx.file_ref)) {
					if (label->constant) { ConsumeConditional(); }
					else { SetConditional(); }
				}
				else { SetConditional(); }
			}
			break;

		case AD_IFBLANK:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
				line.trim_whitespace();
				if (line.is_empty())
					ConsumeConditional();
				else
					SetConditional();
			}
			break;
			
		case AD_IFNBLANK:
			if (NewConditional()) {			// Start new conditional block
				CheckConditionalDepth();	// Check if nesting
				line.trim_whitespace();
				if (!line.is_empty())
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
			} else if (ConditionalAvail()) {
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
		case AD_STRUCT:
			return Directive_ENUM_STRUCT(line, dir);
			
		case AD_REPT:
			return Directive_Rept(line);

		case AD_INCDIR:
			AddIncludeFolder(line.between('"', '"'));
			break;
			
		case AD_A16:			// A16: Set 16 bit accumulator mode
			accumulator_16bit = true;
			break;
			
		case AD_A8:			// A8: Set 8 bit accumulator mode
			accumulator_16bit = false;
			break;
			
		case AD_XY16:			// A16: Set 16 bit accumulator mode
			index_reg_16bit = true;
			break;
			
		case AD_XY8:			// A8: Set 8 bit accumulator mode
			index_reg_16bit = false;
			break;
			
		case AD_MX:
			if (line) {
				line.trim_whitespace();
				int value = 0;
				error = EvalExpression(line, etx, value);
				index_reg_16bit = !(value&1);
				accumulator_16bit = !(value&2);
			}
			break;

		case AD_ABORT:
			line.trim_whitespace();
			if (line)
				printf("Assembler aborted: " STRREF_FMT "\n", STRREF_ARG(line));
			return ERROR_ABORTED;
			
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

		case AD_DS:
			return Directive_DS(line);

		case AD_SCOPE:
			directive_scope_depth++;
			return EnterScope();
			
		case AD_ENDSCOPE:
			directive_scope_depth--;
			return ExitScope();

		case AD_PUSH:
			line.trim_whitespace();
			if (Label *label = GetLabel(line)) {
				symbolStacks.PushSymbol(label);
				return STATUS_OK;
			} else if( StringSymbol* string = GetString(line)) {
				symbolStacks.PushSymbol(string);
				return STATUS_OK;
			}
			return ERROR_UNABLE_TO_PROCESS;

		case AD_PULL:
			line.trim_whitespace();
			if (Label *label = GetLabel(line)) {
				return symbolStacks.PullSymbol(label);
			} else if (StringSymbol* string = GetString(line)) {
				return symbolStacks.PullSymbol(string);
			}
			return ERROR_UNABLE_TO_PROCESS;

	}
	return error;
}

// Make an educated guess at the intended address mode from an opcode argument
StatusCode Asm::GetAddressMode(strref line, bool flipXY, uint32_t &validModes, AddrMode &addrMode, int &len, strref &expression)
{
	bool force_zp = false;
	bool force_24 = false;
	bool force_abs = false;
	bool need_more = true;
	bool first = true;
	strref arg, deco;

	len = 0;
	while (need_more) {
		need_more = false;
		line.skip_whitespace();
		uint8_t c = line.get_first();
		if (!c) { addrMode = AMB_NON; }
		if( c == '[' || c == '(' ) {
			strref block_suffix( line.get(), line.scoped_block_comment_len() );
			strref suffix = line.get_skipped( block_suffix.get_len() );
			suffix.trim_whitespace();
			if( suffix.get_first() == ',' ) { ++suffix; suffix.skip_whitespace(); }
			else { suffix.clear(); }
			++block_suffix; block_suffix.clip(1); block_suffix.trim_whitespace();
			++line; line.skip_whitespace();
			strref block = block_suffix.split_token_trim_track_parens( ',' );
			validModes &= AMM_ZP_REL_X | AMM_ZP_Y_REL | AMM_REL | AMM_ZP_REL | AMM_REL_X | AMM_ZP_REL_L | AMM_ZP_REL_Y_L | AMM_STK_REL_Y | AMM_REL_L;
			if( line.get_first() == '>' ) { // [>$aaaa]
				if( c == '[' ) { addrMode = AMB_REL_L; validModes &= AMM_REL_L; expression = block+1; }
			} else if( line.get_first() == '|' || (line.get_first() == '!' && c == '(' )) { // (|$aaaa) or (|$aaaa,x)
				strref arg = block.after( ',' ); arg.skip_whitespace();
				if( arg && ( arg.get_first() == 'x' || arg.get_first() == 'X' ) ) {
					addrMode = AMB_REL_X; validModes &= AMM_REL_X; expression = block.before( ',' ); }
				else { addrMode = AMB_REL; validModes &= AMM_REL; expression = block; }
			} else if( line.get_first() == '<' ) { // (<$aa) (<$aa),y (<$aa,x) (<$aa,s),y [<$aa] [<$aa],y
				if( suffix ) {
					if( suffix.get_first() == 'y' || suffix.get_first() == 'Y' ) {
						if( c == '(' ) { // (<$aa),y or (<$aa,s),y
							if( block_suffix && ( block_suffix.get_first() == 's' || block_suffix.get_first() == 'S' ) ) {
								expression = block+1;
								addrMode = AMB_STK_REL_Y; validModes &= AMM_STK_REL_Y;
							} else {
								expression = block+1;
								addrMode = AMB_ZP_Y_REL; validModes &= AMM_ZP_Y_REL;
							}
						} else { // [<$aa],y
							expression = block+1;
							addrMode = AMB_ZP_REL_Y_L; validModes &= AMM_ZP_REL_Y_L;
						}
					} else { return ERROR_BAD_ADDRESSING_MODE; }
				} else { // (<$aa) (<$aa,x) [<$aa]
					if( c == '[' ) {
						if( block.find( ',' ) >= 0 || suffix.get_first() == ',' ) { return ERROR_BAD_ADDRESSING_MODE; }
						expression = block+1;
						addrMode = AMB_ZP_REL_L; validModes &= AMM_ZP_REL_L;
					} else {
						if( block_suffix ) {
							if( block_suffix.get_first() != 'x' && block_suffix.get_first() != 'X' ) { return ERROR_BAD_ADDRESSING_MODE; }
							expression = block+1;
							addrMode = AMB_ZP_REL_X; validModes &= AMM_ZP_REL_X;
						} else {
							expression = block+1;
							addrMode = AMB_ZP_REL; validModes &= AMM_ZP_REL;
						}
					}
				}
			} else {	// no <, |, ! or > decorator inside (...) or [...]
				if( c == '[' && ( block_suffix.get_first() == 's' || block_suffix.get_first() == 'S' ) ) {
					if( suffix.get_first() == 'y' || suffix.get_first() == 'Y' ) {
						expression = block;
						addrMode = AMB_STK_REL_Y; validModes &= AMM_STK_REL_Y;
					} else { return ERROR_BAD_ADDRESSING_MODE; }
				} else if( block_suffix.get_first() == 'x' || block_suffix.get_first() == 'X' ) { // ($aa,x) ($aaaa,x)
					if( c == '[' ) { return ERROR_BAD_ADDRESSING_MODE; }
					expression = block;
					switch( validModes & ( AMM_ZP_REL_X | AMM_REL_X ) ) {
						case AMM_ZP_REL_X: addrMode = AMB_ZP_REL_X; validModes = AMM_ZP_REL_X; break;
						case AMM_REL_X: addrMode = AMB_REL_X; validModes = AMM_REL_X; break;
						default: addrMode = force_zp ? AMB_ZP_REL_X : AMB_REL_X; validModes &= force_zp ? AMM_ZP_REL_X : ( force_abs ? AMM_ZP_REL_X : ( AMM_ZP_REL_X | AMM_REL_X ) );
							break;
					}
				} else if( suffix && ( suffix.get_first() == 'y' || suffix.get_first() == 'Y' ) ) {
					if( c == '[' ) {
						expression = block;
						addrMode = AMB_ZP_REL_Y_L; validModes &= AMM_ZP_REL_Y_L;
					} else { // ($aa),y
						expression = block;
						addrMode = AMB_ZP_Y_REL; validModes &= AMM_ZP_Y_REL;
					}
				} else {	// ($aa), ($aaaa), [$aa], [$aaaa]
					if( c == '[' ) { // [$aa], [$aaaa]
						expression = block;
						addrMode = force_zp ? AMB_ZP_REL_L : AMB_REL_L; validModes &= force_zp ? AMM_ZP_REL_L : ( force_abs ? AMM_REL_L : ( AMM_ZP_REL_L | AMM_REL_L ) );
					}
					else { // ($aa), ($aaaa)
						expression = block;
						addrMode = force_zp ? AMB_ZP_REL : AMB_REL; validModes &= force_zp ? AMM_ZP_REL : ( force_abs ? AMM_REL : ( AMM_ZP_REL | AMM_REL ) );
					}
				}
			}
			expression.trim_whitespace();
		} else if (c == '<' ) {	// force zero page not indirect
			++line; line.trim_whitespace();
			strref suffix = line.after(','); suffix.skip_whitespace();
			expression = line.before_or_full(','); expression.trim_whitespace();
			if( suffix ) {
				if( suffix.get_first() == 's' || suffix.get_first() == 'S' ) {
					addrMode = AMB_STK; validModes &= AMM_STK;	// not correct usage of < but I'll allow it.
				} else if( suffix.get_first() == 'x' || suffix.get_first() == 'X' ) {
					addrMode = AMB_ZP_X; validModes &= AMM_ZP_X;
				} else { return ERROR_BAD_ADDRESSING_MODE; }
			} else {
				addrMode = AMB_ZP; validModes &= AMM_ZP;
			}
		} else if (c == '<') {
			validModes &= AMM_ZP | AMM_ZP_X | AMM_ZP_REL_X | AMM_ZP_Y_REL |
				AMM_ZP_REL | AMM_ZP_ABS | AMM_ZP_REL_L | AMM_ZP_REL_Y_L | AMM_FLIPXY;
		} else if( cpu == CPU_65816 && ( c == '|' /*|| c == '!'*/ ) ) { // disabling ! for now since it conflicts with scope start
			++line; line.trim_whitespace();
			strref suffix = line.after( ',' ); suffix.skip_whitespace();
			expression = line.before_or_full( ',' ); expression.trim_whitespace();
			addrMode = AMB_NON;
			if( suffix ) {
				if( suffix.get_first() == 'x' || suffix.get_first() == 'X' ) {
					addrMode = AMB_ABS_X; validModes &= AMM_ABS_X;
				} else if( suffix.get_first() == 'y' || suffix.get_first() == 'Y' ) {
					addrMode = AMB_ABS_Y; validModes &= AMM_ABS_Y;
				}
			} else {
				addrMode = AMB_ABS; validModes &= AMM_ABS;
			}
		} else if( cpu == CPU_65816 && c == '>' ) {
			++line; line.trim_whitespace();
			strref suffix = line.after( ',' ); suffix.skip_whitespace();
			expression = line.before_or_full( ',' ); expression.trim_whitespace();
			if( suffix ) {
				if( suffix.get_first() == 'x' || suffix.get_first() == 'X' ) {
					addrMode = AMB_ABS_L_X; validModes &= AMM_ABS_L_X;
				}
				else { return ERROR_BAD_ADDRESSING_MODE; }
			}
			else {
				addrMode = AMB_ABS_L; validModes &= AMM_ABS_L;
			}
		} else if (c == '#') {
			++line;
			addrMode = AMB_IMM;
			validModes &= AMM_IMM | AMM_IMM_DBL_A | AMM_IMM_DBL_XY;
			expression = line;
		} else if (line) {
			if (line[0]=='.' && strref::is_ws(line[2])) {
				switch (strref::tolower(line[1])) {
					case 'z': force_zp = true; line += 3; need_more = true; len = 1; break;
					case 'b': line += 3; need_more = true; len = 1; validModes &= ~(AMM_IMM_DBL_A | AMM_IMM_DBL_XY); break;
					case 'w': line += 3; need_more = true; len = 2; break;
					case 'l': force_24 = true; line += 3; need_more = true; len = 3; break;
					case 'a': force_abs = true; line += 3; need_more = true; break;
				}
			}
			if (!need_more) {
				if( strref( "A" ).is_prefix_word( line ) ) {
					addrMode = AMB_ACC;
				} else {	// absolute (zp, offs x, offs y)
					addrMode = force_24 ? AMB_ABS_L : (force_zp ? AMB_ZP : AMB_ABS);
					expression = line.split_token_trim_track_parens(',');
					if( force_abs ) { validModes &= AMM_ABS | AMM_ABS_X | AMM_ABS_Y | AMM_REL | AMM_REL_X; }
					if( force_zp ) { validModes &= AMM_ZP | AMM_ZP_X | AMM_ZP_REL_X | AMM_ZP_Y_REL |
						AMM_ZP_REL | AMM_ZP_ABS | AMM_ZP_REL_L | AMM_ZP_REL_Y_L | AMM_FLIPXY; }
					if( line && (line[ 0 ] == 's' || line[ 0 ] == 'S') ) { addrMode = AMB_STK; }
					else {
						bool relX = line && (line[0]=='x' || line[0]=='X');
						bool relY = line && (line[0]=='y' || line[0]=='Y');
						if ((flipXY && relY) || (!flipXY && relX))
							addrMode = force_24 ? AMB_ABS_L_X : (force_zp ? AMB_ZP_X : AMB_ABS_X);
						else if ((flipXY && relX) || (!flipXY && relY)) {
							if (force_zp) { return ERROR_INSTRUCTION_NOT_ZP; }
							addrMode = AMB_ABS_Y;
						}
					}
				}
			}
		}
		first = false;
	}
	return STATUS_OK;
}

// Push an opcode to the output buffer in the current section
StatusCode Asm::AddOpcode(strref line, int index, strref source_file) {
	StatusCode error = STATUS_OK;
	strref expression;

	// allowed modes
	uint32_t validModes = opcode_table[index].modes;

	// instruction parameter length override
	int op_param = 0;

	// Get the addressing mode and the expression it refers to
	AddrMode addrMode;
	switch (validModes) {
		case AMC_BBR:
			addrMode = AMB_ZP_ABS;
			expression = line.split_token_trim_track_parens(',');
			if (!expression || !line)
				return ERROR_INVALID_ADDRESSING_MODE;
			break;
		case AMM_BRA:
			addrMode = AMB_ABS;
			expression = line;
			break;
		case AMM_ACC:
		case (AMM_ACC|AMM_NON):
		case AMM_NON:
			addrMode = AMB_NON;
			break;
		case AMM_BLK_MOV:
			addrMode = AMB_BLK_MOV;
			expression = line.before_or_full_track_parens(',');
			break;
		default:
			error = GetAddressMode(line, !!(validModes & AMM_FLIPXY), validModes, addrMode, op_param, expression);
			break;
	}

	int value = 0;
	int target_section = -1;
	int target_section_offs = -1;
	int8_t target_section_shift = 0;
	bool evalLater = false;
	if (expression) {
		struct EvalContext etx;
		SetEvalCtxDefaults(etx);
		if (validModes & (AMM_BRANCH | AMM_BRANCH_L))
			etx.relative_section = SectionId();
		error = EvalExpression(expression, etx, value);
		if (error == STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT) {
			evalLater = true;
			error = STATUS_OK;
		} else if (error == STATUS_RELATIVE_SECTION) {
			target_section = lastEvalSection;
			target_section_offs = lastEvalValue;
			target_section_shift = lastEvalShift;
		} else if (error != STATUS_OK)
			return error;
	}

	// check if address is in zero page range and should use a ZP mode instead of absolute
	if (!evalLater && value>=0 && value<0x100 && (error != STATUS_RELATIVE_SECTION ||
		(target_section>=0 && allSections[target_section].type==ST_ZEROPAGE))) {
		switch (addrMode) {
			case AMB_ABS:
				if (validModes & AMM_ZP)
					addrMode = AMB_ZP;
				else if (validModes & AMM_ABS_L)
					addrMode = AMB_ABS_L;
				break;
			case AMB_ABS_X:
				if (validModes & AMM_ZP_X)
					addrMode = AMB_ZP_X;
				else if (validModes & AMM_ABS_L_X)
					addrMode = AMB_ABS_L_X;
				break;
			default:
				break;
		}
	}

	// Check if an explicit 24 bit address
	if (expression[0] == '$' && (expression + 1).len_hex()>4) {
		if (addrMode==AMB_ABS&&(validModes & AMM_ABS_L)) {
			addrMode = AMB_ABS_L;
		} else if (addrMode==AMB_ABS_X&&(validModes & AMM_ABS_L_X)) {
			addrMode = AMB_ABS_L_X;
		}
	}

	if (!(validModes & (1 << addrMode))) {
		if (addrMode==AMB_ZP_REL_X&&(validModes & AMM_REL_X)) {
			addrMode = AMB_REL_X;
		} else if (addrMode==AMB_REL&&(validModes & AMM_ZP_REL)) {
			addrMode = AMB_ZP_REL;
		} else if (addrMode==AMB_ABS&&(validModes & AMM_ABS_L)) {
			addrMode = AMB_ABS_L;
		} else if (addrMode==AMB_ABS_X&&(validModes & AMM_ABS_L_X)) {
			addrMode = AMB_ABS_L_X;
		} else if (addrMode==AMB_REL_L&&(validModes & AMM_ZP_REL_L)) {
			addrMode = AMB_ZP_REL_L;
		} else if (Merlin()&&addrMode==AMB_IMM && validModes==AMM_ABS) {
			addrMode = AMB_ABS;	// Merlin seems to allow this
		} else if (Merlin()&&addrMode==AMB_ABS && validModes==AMM_ZP_REL) {
			addrMode = AMB_ZP_REL;	// Merlin seems to allow this
		} else { return ERROR_INVALID_ADDRESSING_MODE; }
	}

	// Add the instruction and argument to the code
	if (error == STATUS_OK || error == STATUS_RELATIVE_SECTION) {
		uint8_t opcode = opcode_table[index].aCodes[addrMode];
		StatusCode cap_status = CheckOutputCapacity(4);
		if (cap_status != STATUS_OK)
			return error;

		AddByte(opcode);

		CODE_ARG codeArg = CA_NONE;
		if (validModes & AMM_BRANCH_L)
			codeArg = CA_BRANCH_16;
		else if (validModes & AMM_BRANCH)
			codeArg = CA_BRANCH;
		else {
			switch (addrMode) {
				case AMB_ZP_REL_X:		// 0 ($12:x)
				case AMB_ZP:			// 1 $12
				case AMB_ZP_Y_REL:		// 4 ($12),y
				case AMB_ZP_X:			// 5 $12,x
				case AMB_ZP_REL:		// b ($12)
				case AMB_ZP_REL_L:		// e [$02]
				case AMB_ZP_REL_Y_L:	// f [$00],y
				case AMB_STK:			// 12 $12,s
				case AMB_STK_REL_Y:		// 13 ($12,s),y
					codeArg = CA_ONE_BYTE;
					break;

				case AMB_ABS_Y:			// 6 $1234,y
				case AMB_ABS_X:			// 7 $1234,x
				case AMB_ABS:			// 3 $1234
				case AMB_REL:			// 8 ($1234)
				case AMB_REL_X:			// c ($1234,x)
				case AMB_REL_L:			// 14 [$1234]
					codeArg = CA_TWO_BYTES;
					break;

				case AMB_ABS_L:			// 10 $e00001
				case AMB_ABS_L_X:		// 11 $123456,x
					codeArg = CA_THREE_BYTES;
					break;

				case AMB_ZP_ABS:			// d $12, label
					codeArg = CA_BYTE_BRANCH;
					break;

				case AMB_BLK_MOV:		// 15 $12,$34
					codeArg = CA_TWO_ARG_BYTES;
					break;

				case AMB_IMM:			// 2 #$12
					codeArg = CA_ONE_BYTE;
					// check for double immediate
					if( validModes & ( AMM_IMM_DBL_A | AMM_IMM_DBL_XY ) ) {
						if( op_param ) { codeArg = op_param == 2 ? CA_TWO_BYTES : CA_ONE_BYTE; }
						else if( ( ( validModes&AMM_IMM_DBL_A ) && accumulator_16bit ) ||
							( ( validModes&AMM_IMM_DBL_XY ) && index_reg_16bit ) ) { codeArg = CA_TWO_BYTES; }
						else { codeArg = CA_ONE_BYTE; }
					}
					break;
					
				case AMB_ACC:			// 9 A
				case AMB_NON:			// a
				default:
					break;
					
			}
		}
			
		switch (codeArg) {
			case CA_ONE_BYTE:
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BYTE);
				else if (error == STATUS_RELATIVE_SECTION)
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section, 1, target_section_shift);
				AddByte(value);
				break;

			case CA_TWO_BYTES:
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_ABS_REF);
				else if (error == STATUS_RELATIVE_SECTION) {
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section, 2, target_section_shift);
					value = 0;
				}
				AddWord(value);
				break;

			case CA_THREE_BYTES:
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_ABS_L_REF);
				else if (error == STATUS_RELATIVE_SECTION) {
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section, 3, target_section_shift);
					value = 0;
				}
				AddTriple(value);
				break;

			case CA_TWO_ARG_BYTES: {
					// second operand stored first.
					StatusCode error = STATUS_OK;
					int value = 0;

					struct EvalContext etx;
					SetEvalCtxDefaults(etx);
					etx.pc = CurrSection().GetPC()-1;
					line.split_token_trim_track_parens(',');
					error = EvalExpression(line, etx, value);
					if (error==STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT)
						AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC()-1, scope_address[scope_depth], line, source_file, LateEval::LET_BYTE);
					else if (error == STATUS_RELATIVE_SECTION) {
						CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), lastEvalSection, 1, lastEvalShift);
					}

					AddByte(value);
				}

				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC()-2, scope_address[scope_depth], expression, source_file, LateEval::LET_BYTE);
				else if (error == STATUS_RELATIVE_SECTION) {
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section, 1, target_section_shift);
				}
				AddByte(value);
				break;

			case CA_BRANCH:
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BRANCH);
				else if (((int)value - (int)CurrSection().GetPC()-1) < -128 || ((int)value - (int)CurrSection().GetPC()-1) > 127)
					error = ERROR_BRANCH_OUT_OF_RANGE;
				AddByte(evalLater ? 0 : (uint8_t)((int)value - (int)CurrSection().GetPC()) - 1);
				break;

			case CA_BRANCH_16:
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BRANCH_16);
				AddWord(evalLater ? 0 : (value-(CurrSection().GetPC()+2)));
				break;

			case CA_BYTE_BRANCH: {
				if (evalLater)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BYTE);
				else if (error == STATUS_RELATIVE_SECTION)
					CurrSection().AddReloc(target_section_offs, CurrSection().DataOffset(), target_section, 1, target_section_shift);
				AddByte(value);
				struct EvalContext etx;
				SetEvalCtxDefaults(etx);
				etx.pc = CurrSection().GetPC()-2;
				etx.relative_section = SectionId();
				error = EvalExpression(line, etx, value);
				if (error==STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT)
					AddLateEval(CurrSection().DataOffset(), CurrSection().GetPC(), scope_address[scope_depth], line, source_file, LateEval::LET_BRANCH);
				else if (((int)value - (int)CurrSection().GetPC() - 1) < -128 || ((int)value - (int)CurrSection().GetPC() - 1) > 127)
					error = ERROR_BRANCH_OUT_OF_RANGE;
				AddByte((error == STATUS_NOT_READY || error == STATUS_XREF_DEPENDENT) ?
						0 : (uint8_t)((int)value - (int)CurrSection().GetPC()) - 1);
				break;
			}
			case CA_NONE:
				break;
		}
	}
	return error;
}

// Build a line of code
void Asm::PrintError(strref line, StatusCode error, strref source) {
	strown<512> errorText;
	if (contextStack.has_work()) {
		errorText.sprintf("Error " STRREF_FMT "(%d): ", STRREF_ARG(contextStack.curr().source_name),
						  contextStack.curr().source_file.count_lines(line)+1);
	} else if (source) { errorText.sprintf_append("Error (%d): ", source.count_lines(line)); }
	else { errorText.append("Error: "); }
	errorText.append(aStatusStrings[error]);
	errorText.append(" \"");
	errorText.append(line.get_trimmed_ws());
	errorText.append("\"\n");
	errorText.c_str();
	fwrite(errorText.get(), errorText.get_len(), 1, stderr);
	error_encountered = true;
}

// Build a line of code
StatusCode Asm::BuildLine(strref line) {
	StatusCode error = STATUS_OK;

	// MERLIN: First char of line is * means comment
	if (Merlin()&&line[0]=='*') { return STATUS_OK; }

	// remember for listing
	int start_section = SectionId();
	int start_address = CurrSection().address;
	strref code_line = line;
	const char* data_line = line.get();	// code_line is current line, data_line is where data is generated
	list_flags = 0;

	while (line && error == STATUS_OK) {
		strref line_start = line;
		char char0 = line[0];				// first char including white space
		line.skip_whitespace();				// skip to first character
		line = line.before_or_full(';');	// clip any line comments
		line = line.before_or_full(c_comment);
		line.clip_trailing_whitespace();
		if (KickAsm()&&line.get_first()==':') { ++line; }	// Kick Assembler macro prefix (incompatible with merlin and sane syntax)
		strref line_nocom = line;
		strref operation = line.split_range(Merlin() ? label_end_char_range_merlin : label_end_char_range);
		if( operation.get_last() == ':' ) { operation.clip( 1 ); }
		char char1 = operation[0];			// first char of first word
		char charE = operation.get_last();	// end char of first word
		line.trim_whitespace();
		bool force_label = charE==':' || charE=='$';
		if (!force_label && Merlin()&&(line||operation)) { // MERLIN fixes and PoP does some naughty stuff like 'and = 0'
			force_label = (!strref::is_ws(char0)&&char0!='{' && char0!='}')||char1==']'||charE=='?';
		} /*else if (!Merlin()&&line[0]==':') { force_label = true; }*/
		if (!operation && !force_label) {
			if (ConditionalAsm()) {
				// scope open / close
				switch (line[0]) {
					case '{':
						error = EnterScope();
						brace_depth++;
						list_flags |= ListLine::CYCLES_START;
						if (error == STATUS_OK) {
							++line;
							line.skip_whitespace();
						}
						break;
					case '}':
						// check for late eval of anything with an end scope
						error = ExitScope();
						for (LabelPool *pool = labelPools.getValues(), *end = pool+labelPools.count(); pool!=end; pool++) {
							pool->ExitScope((uint16_t)brace_depth);
						}
						brace_depth--;
						list_flags |= ListLine::CYCLES_STOP;
						if (error == STATUS_OK) {
							++line;
							line.skip_whitespace();
						}
						break;
					case '*':
						// if first char is '*' this seems like a line comment on some assemblers
						line.clear();
						break;
					case 127:
						++line;	// bad character?
						break;
				}
			} else { line.clear(); }
		} else {
			// ignore leading period for instructions and directives - not for labels
			strref label = operation;
			if ((!Merlin()&&operation[0]==':')||operation[0]=='.') { ++operation; }
			operation = operation.before_or_full('.');

			int op_idx = LookupOpCodeIndex(operation.fnv1a_lower(), aInstructions, num_instructions);
			if (op_idx >= 0 && !force_label && (aInstructions[op_idx].type==OT_DIRECTIVE || line[0]!='=')) {
				if (line_nocom.is_substr(operation.get())) {
					line = line_nocom + strl_t(operation.get()+operation.get_len()-line_nocom.get());
					line.skip_whitespace();
				}
				if (aInstructions[op_idx].type==OT_DIRECTIVE) {
					data_line = operation.get();
					error = ApplyDirective((AssemblerDirective)aInstructions[op_idx].index, line, contextStack.curr().source_file);
					list_flags |= ListLine::KEYWORD;
				} else if (ConditionalAsm() && aInstructions[op_idx].type == OT_MNEMONIC) {
					data_line = operation.get();
					error = AddOpcode(line, aInstructions[op_idx].index, contextStack.curr().source_file);
					list_flags |= ListLine::MNEMONIC;
				}
				line.clear();
			} else if (!ConditionalAsm()) {
				line.clear(); // do nothing if conditional nesting so clear the current line
			} else if (line.get_first()=='=') {
				++line;
				error = AssignLabel(label, line);
				line.clear();
				list_flags |= ListLine::KEYWORD;
			} else if (keyword_equ.is_prefix_word(line)) {
				line += keyword_equ.get_len();
				line.skip_whitespace();
				error = AssignLabel(label, line);
				line.clear();
				list_flags |= ListLine::KEYWORD;
			} else {
				uint32_t nameHash = label.fnv1a();
				uint32_t macro = FindLabelIndex(nameHash, macros.getKeys(), macros.count());
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
					uint32_t labPool = FindLabelIndex(nameHash, labelPools.getKeys(), labelPools.count());
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
						if (StringSymbol *pStr = GetString(label)) {
							StringAction(pStr, line);
							line.clear();
						} else if (Merlin() && strref::is_ws(line_start[0])) {
							error = ERROR_UNDEFINED_CODE;
						} else if (label[0]=='$') {
							line.clear();
						} else {
							if (label.get_last()==':') { label.clip(1); }
							error = AddressLabel(label);
							line = line_start + int(label.get() + label.get_len() -line_start.get());
							if (line[0]==':'||line[0]=='?') { ++line; } // there may be codes after the label
							list_flags |= ListLine::KEYWORD;
						}
					}
				}
			}
		}
		if( error >= ERROR_STOP_PROCESSING_ON_HIGHER ) { return error; }
		// Check for unterminated condition in source
		if (!contextStack.curr().next_source &&
			(!ConditionalAsm() || ConditionalConsumed() || !conditional_depth)) {
			if (Merlin()) {	// this isn't a listed feature,
				conditional_nesting[0] = 0;	//	some files just seem to get away without closing
				conditional_consumed[0] = 0;
				conditional_depth = 0;
			} else {
				PrintError(conditional_source[conditional_depth], ERROR_UNTERMINATED_CONDITION);
				return ERROR_UNTERMINATED_CONDITION;
			}
		}
		if (line.same_str_case(line_start)) {
			error = ERROR_UNABLE_TO_PROCESS;
		} else if (CurrSection().type==ST_ZEROPAGE && CurrSection().address>0x100) {
			error = ERROR_ZEROPAGE_SECTION_OUT_OF_RANGE;
		}
		if (error>STATUS_XREF_DEPENDENT) {
			PrintError(line_start, error);
		}

		// dealt with error, continue with next instruction unless too broken
		if (error<ERROR_STOP_PROCESSING_ON_HIGHER) {
			error = STATUS_OK;
		}
	}
	// update listing
	if (error == STATUS_OK) {
		if (SectionId() == start_section) {
			Section &curr = CurrSection();
			if (list_assembly) {
				if (!curr.pListing) { curr.pListing = new Listing; }
				if (((list_flags & (ListLine::KEYWORD | ListLine::CYCLES_START | ListLine::CYCLES_STOP)) ||
					(curr.address != start_address && curr.size())) && !curr.IsDummySection()) {
					struct ListLine lst;
					lst.address = start_address - curr.start_address;
					lst.size = curr.address - start_address;
					lst.code = contextStack.curr().source_file;
					lst.column = (uint16_t)(data_line - code_line.get());
					lst.source_name = contextStack.curr().source_name;
					lst.line_offs = int(code_line.get() - lst.code.get());
					lst.flags = list_flags;
					curr.pListing->push_back(lst);
				}
			}
			if (src_debug) {
				if (curr.address != start_address && curr.size()) {
					AddSrcDbg(start_address, SourceDebugType::Code, data_line);
				}
			}
		}
	}
	return error;
}

void Asm::AddSrcDbg(int address, SourceDebugType type, const char* text)
{
	SourceDebugEntry entry;
	Section& curr = CurrSection();
	if (!curr.pSrcDbg) { curr.pSrcDbg = new SourceDebug; }

	entry.address = address - curr.start_address;
	entry.size = curr.address - address;
	entry.type = (int)type;

	size_t sf = 0, nsf = source_files.size();
	strref src = contextStack.curr().source_name;
	for (; sf < nsf; ++sf) {
		if (src.same_str_case(source_files[sf])) { break; }
	}
	if (sf == nsf) {
		source_files.push_back(StringCopy(src));
	}
	entry.source_file_index = (int)sf;
	entry.source_file_offset = (int)(text - contextStack.curr().source_file.get());
	curr.pSrcDbg->push_back(entry);
}

// Build a segment of code (file or macro)
StatusCode Asm::BuildSegment() {
	StatusCode error = STATUS_OK;
	while (contextStack.curr().read_source) {
		contextStack.curr().next_source = contextStack.curr().read_source;
		error = BuildLine(contextStack.curr().next_source.line());
		if (error>ERROR_STOP_PROCESSING_ON_HIGHER) { break; }
		contextStack.curr().read_source = contextStack.curr().next_source;
	}
	if (error == STATUS_OK) { error = CheckLateEval(strref(), CurrSection().GetPC()); }
	return error;
}

// Produce the assembler listing
#define MAX_DEPTH_CYCLE_COUNTER 64

struct cycleCnt {
	int base;
	int16_t plus, a16, x16, dp;
	void clr() { base = 0; plus = a16 = x16 = dp = 0; }
	void add(uint8_t c) {
		if (c != 0xff) {
			base += (c >> 1) & 7;
			plus += c & 1;
			if (c & 0xf0) {
				int i = c >> 4;
				if (i <= 8) {
					a16 += timing_65816_plus[i][0];
					x16 += timing_65816_plus[i][1];
					dp += timing_65816_plus[i][2];
				}
			}
		}
	}
	int plus_acc() { return plus + a16 + x16 + dp; }
	void combine(const struct cycleCnt &o) {
		base += o.base; plus += o.plus; a16 += o.a16; x16 += o.x16; dp += o.dp;
	}
	bool complex() const { return a16 != 0 || x16 != 0 || dp != 0; }
	static int get_base(uint8_t c) { return (c & 0xf) >> 1;	}
	static int sum_plus(uint8_t c) {
		if (c==0xff) { return 0; }
		int i = c >> 4;
		if (i) {
			return i<=8 ? (timing_65816_plus[i][0]+timing_65816_plus[i][1]+timing_65816_plus[i][2]) : 0;
		}
		return c & 1;
	}
};

bool Asm::ListTassStyle( strref filename ) {
	// NOTE: This file needs more information to be useful for things
	FILE *f = stdout;
	bool opened = false;
	if( filename ) {
		f = fopen( strown<512>( filename ).c_str(), "w" );
		if( !f ) { return false; }
		opened = true;
	}

	// ensure that the greatest instruction set referenced is used for listing
	if (list_cpu!=cpu) { SetCPU(list_cpu); }

	// Build a disassembly lookup table
	uint8_t mnemonic[256];
	uint8_t addrmode[256];
	memset(mnemonic, 255, sizeof(mnemonic));
	memset(addrmode, 255, sizeof(addrmode));
	for (int i = 0; i < opcode_count; i++) {
		for (int j = AMB_COUNT-1; j >= 0; j--) {
			if (opcode_table[i].modes & (1 << j)) {
				uint8_t op = opcode_table[i].aCodes[j];
				if (addrmode[op]==255) {
					mnemonic[op] = (uint8_t)i;
					addrmode[op] = (uint8_t)j;
				}
			}
		}
	}

	strref first_src;
	for( std::vector<Section>::iterator si = allSections.begin(); si != allSections.end() && !first_src; ++si ) {
		if( !si->pListing )
			continue;
		for(auto & li : *si->pListing) {
			if( li.source_name ) { first_src = li.source_name; break; }
		}
	}
	fprintf( f, ";6502/65C02/65816/CPU64/DTV Turbo Assembler V1.4x listing file of \"" STRREF_FMT "\"\n;done on WkDay Month Date Time\n\n", STRREF_ARG( first_src ) );

	fprintf( f, ";Offset\t;Hex\t\t;Monitor\t;Source\n\n" );

	for(auto &si : allSections) {
		if( !si.pListing || si.type == ST_REMOVED || !si.pListing ) { continue; }
		for(const struct ListLine &lst : *si.pListing) {
			/*if( lst.size && lst.wasMnemonic() )*/ {
				strown<256> out;
				/*if( lst.size )*/ { out.sprintf_append( ".%04x\t", lst.address + si.start_address ); }
				int s = lst.wasMnemonic() ? ( lst.size < 4 ? lst.size : 4 ) : ( lst.size < 8 ? lst.size : 8 );
				if( si.output && si.output_capacity >= size_t( lst.address + s ) ) {
					for( int b = 0; b < s; ++b ) {
						out.sprintf_append( "%02x ", si.output[ lst.address + b ] );
					}
				}
				out.append( "\t\t" );
				if (lst.size && lst.wasMnemonic()) {
					//out.pad_to(' ', 21);
					uint8_t *buf = si.output + lst.address;
					uint8_t op = mnemonic[*buf];
					uint8_t am = addrmode[*buf];
					if (op != 255 && am != 255 && am<(sizeof(aAddrModeFmt)/sizeof(aAddrModeFmt[0]))) {
						const char *fmt = aAddrModeFmt[am];
						if (opcode_table[op].modes & AMM_FLIPXY) {
							if (am == AMB_ZP_X)	fmt = "%s $%02x,y";
							else if (am == AMB_ABS_X) fmt = "%s $%04x,y";
						}
						if (opcode_table[op].modes & AMM_ZP_ABS) {
							out.sprintf_append(fmt, opcode_table[op].instr, buf[1], (char)buf[2]+lst.address+si.start_address+3);
						} else if (opcode_table[op].modes & AMM_BRANCH) {
							out.sprintf_append(fmt, opcode_table[op].instr, (char)buf[1]+lst.address+si.start_address+2);
						} else if (opcode_table[op].modes & AMM_BRANCH_L) {
							out.sprintf_append(fmt, opcode_table[op].instr, (int16_t)(buf[1]|(buf[2]<<8))+lst.address+si.start_address+3);
						} else if (am==AMB_NON||am==AMB_ACC) {
							out.sprintf_append(fmt, opcode_table[op].instr);
						} else if (am==AMB_ABS||am==AMB_ABS_X||am==AMB_ABS_Y||am==AMB_REL||am==AMB_REL_X||am==AMB_REL_L) {
							out.sprintf_append(fmt, opcode_table[op].instr, buf[1]|(buf[2]<<8));
						} else if (am==AMB_ABS_L||am==AMB_ABS_L_X) {
							out.sprintf_append(fmt, opcode_table[op].instr, buf[1]|(buf[2]<<8)|(buf[3]<<16));
						} else if (am==AMB_BLK_MOV) {
							out.sprintf_append(fmt, opcode_table[op].instr, buf[2], buf[1]);
						} else if (am==AMB_IMM && lst.size==3) {
							out.sprintf_append("%s #$%04x", opcode_table[op].instr, buf[1]|(buf[2]<<8));
						} else {
							out.sprintf_append(fmt, opcode_table[op].instr, buf[1]);
						}
					}
				} else { out.append('\t');}
//				out.pad_to( ' ', 36 );
				out.append( '\t' );
				strref line = lst.code.get_skipped( lst.line_offs ).get_line();
				line.clip_trailing_whitespace();
				strown<128> line_fix( line );
				for( strl_t pos = 0; pos < line_fix.len(); ++pos ) {
					if( line_fix[ pos ] == '\t' )
						line_fix.exchange( pos, 1, pos & 1 ? strref( " " ) : strref( "  " ) );
				}
				out.append( line_fix.get_strref() );

				fprintf( f, STRREF_FMT "\n", STRREF_ARG( out ) );
			}
		}
	}
	fprintf( f, "\n;******  end of code\n" );
	if( opened ) { fclose( f ); }
	return true;
}

bool Asm::SourceDebugExport(strref filename) {
	FILE* f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f) { return false; }
		opened = true;
	} else {
		return false;
	}

	std::vector<strovl> source_code;  source_code.reserve(source_files.size());

	fprintf(f, "<C64debugger version=\"1.0\">\n\t<Sources values=\"INDEX,FILE\">\n");
	for (size_t i = 0, n = source_files.size(); i < n; ++i) {
		fprintf(f, "\t\t%d,%s\n", (int)i+1, source_files[i]);

		size_t size = 0;
		char* src = LoadText(source_files[i], size);
		source_code.push_back(strovl(src, (strl_t)size, (strl_t)size));
	}
	fprintf(f, "\t</Sources>\n\n");

	for (size_t i = 0, n = allSections.size(); i < n; ++i) {
		Section& s = allSections[i];
		if (s.pSrcDbg && s.pSrcDbg->size()) {
			fprintf(f, "\t<Segment name=\"" STRREF_FMT "\" dest=\"\" values=\"START,END,FILE_IDX,LINE1,COL1,LINE2,COL2\">\n\t\t<Block name=\"Unnamed\">\n",
				STRREF_ARG(s.name));
			for (size_t d = 0, nd = s.pSrcDbg->size(); d < nd; ++d) {
				SourceDebugEntry& e = s.pSrcDbg->at(d);
				int line = 0, col0 = 0, col1 = 0;
				if ((e.source_file_index) < source_code.size()) {
					strref src = source_code[e.source_file_index].get_strref();
					if (src.get_len() > strl_t(e.source_file_offset)) {
						line = strref(src.get(), e.source_file_offset).count_lines();
						strl_t offs = e.source_file_offset;
						while (src.get_at(offs) != 0x0a && src.get_at(offs) != 0x0d && offs) { --offs; ++col0; }
						col1 = col0;
						offs = e.source_file_offset;
						while (src.get_at(offs) != 0x0a && src.get_at(offs) != 0x0d && offs<src.get_len()) { ++offs; ++col1; }
					}
				}
				fprintf(f, "\t\t\t$%04x,$%04x,%d,%d,%d,%d,%d\n",
					e.address+s.start_address, e.address+e.size+s.start_address-1, e.source_file_index+1,
					line+1, col0 ? (col0-1) : 0, line+1, col1 );
			}
			fprintf(f, "\t\t</Block>\n\t</Segment>\n\n");
		}
	}

	fprintf(f, "\t<Labels values=\"SEGMENT,ADDRESS,NAME,START,END,FILE_IDX,LINE1,COL1,LINE2,COL2\">\n");
	for (MapSymbolArray::iterator i = map.begin(); i != map.end(); ++i) {
		if (i->name.same_str("debugbreak")) { continue; }
		uint32_t value = (uint32_t)i->value;
		strref sectName;
		int16_t section = i->section;
		int16_t orig_section = i->orig_section;
		if (size_t(section) < allSections.size()) {
			value += allSections[section].start_address;
		}
		if (size_t(orig_section) < allSections.size()) {
			sectName = allSections[orig_section].name;
		}
		fprintf(f, "\t\t" STRREF_FMT ",$%04x," STRREF_FMT ",0,0,0,0,0\n", STRREF_ARG(sectName), value, STRREF_ARG(i->name));
	}
	fprintf(f, "\t</Labels>\n\n");

	fprintf(f, "\t<Breakpoints values=\"SEGMENT,ADDRESS,ARGUMENT\">\n");
	for (MapSymbolArray::iterator i = map.begin(); i != map.end(); ++i) {
		if (i->name.same_str("debugbreak")) {
			uint32_t value = (uint32_t)i->value;
			strref sectName;
			uint16_t section = i->section;
			if (size_t(section) < allSections.size()) {
				value += allSections[section].start_address;
				sectName = allSections[section].name;
			}
			fprintf(f, "\t\t" STRREF_FMT ",$%04x,\n", STRREF_ARG(sectName), value);
		}
	}
	fprintf(f, "\t</Breakpoints>\n\n");

	fprintf(f, "\t<Watchpoints values=\"SEGMENT,ADDRESS1,ADDRESS2,ARGUMENT\">\n");
	fprintf(f, "\t</Watchpoints>\n\n");

	fprintf(f, "</C64debugger>\n");
	fclose(f);

	for (size_t i = 0, n = source_code.size(); i < n; ++i) {
		if (source_code[i].get()) { free(source_code[i].charstr()); }
	}
	return true;
}

bool Asm::List(strref filename) {
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f) { return false; }
		opened = true;
	}

	// ensure that the greatest instruction set referenced is used for listing
	if (list_cpu!=cpu) { SetCPU(list_cpu); }

	// Build a disassembly lookup table
	uint8_t mnemonic[256];
	uint8_t addrmode[256];
	memset(mnemonic, 255, sizeof(mnemonic));
	memset(addrmode, 255, sizeof(addrmode));
	for (int i = 0; i < opcode_count; i++) {
		for (int j = AMB_COUNT-1; j >= 0; j--) {
			if (opcode_table[i].modes & (1 << j)) {
				uint8_t op = opcode_table[i].aCodes[j];
				if (addrmode[op]==255) {
					mnemonic[op] = (uint8_t)i;
					addrmode[op] = (uint8_t)j;
				}
			}
		}
	}

	struct cycleCnt cycles[MAX_DEPTH_CYCLE_COUNTER];
	int16_t cycles_depth = 0;
	memset(cycles, 0, sizeof(cycles));


	// show merged sections
	for (size_t i = 0, n = allSections.size(); i < n; ++i)
	{
		Section& s = allSections[i];
		if (s.type != ST_REMOVED) {
			if (s.include_from) {
				fprintf(f, "Section " STRREF_FMT " from " STRREF_FMT " $%04x - $%04x ($%04x)\n",
					STRREF_ARG(s.name), STRREF_ARG(s.include_from), s.start_address, s.address, s.address - s.start_address);
			} else {
				fprintf(f, "Section " STRREF_FMT " $%04x - $%04x ($%04x)\n",
					STRREF_ARG(s.name), s.start_address, s.address, s.address - s.start_address);
			}
			for (size_t j = 0; j < n; ++j)
			{
				Section& s2 = allSections[j];
				if (s2.type == ST_REMOVED && s2.merged_into >= 0)
				{
					int offset = s2.merged_at;
					int parent = s2.merged_into;
					while (parent != i && parent >= 0) {
						offset += allSections[parent].merged_at;
						parent = allSections[parent].merged_into;
					}
					if (parent == i) {
						if (s2.include_from) {
							fprintf(f, " + " STRREF_FMT " from " STRREF_FMT " $%04x - $%04x ($%04x) at offset 0x%04x\n",
								STRREF_ARG(s2.name), STRREF_ARG(s2.include_from), s2.merged_at + s.start_address,
								s2.merged_at + s.start_address + s2.merged_size, s2.merged_size, s2.merged_at);
						} else {
							fprintf(f, " + " STRREF_FMT " $%04x - $%04x ($%04x) at offset 0x%04x\n",
								STRREF_ARG(s2.name), s2.merged_at + s.start_address,
								s2.merged_at + s.start_address + s2.merged_size, s2.merged_size, s2.merged_at);
						}
					}
				}
			}
		}
	}

	strref prev_src;
	int prev_offs = 0;
	for (std::vector<Section>::iterator si = allSections.begin(); si != allSections.end(); ++si) {
		if (si->merged_into >= 0) {
			fprintf(f, STRREF_FMT " from " STRREF_FMT " merged into " STRREF_FMT " at offset 0x%04x\n",
			STRREF_ARG(si->name), STRREF_ARG(si->include_from), STRREF_ARG(allSections[si->merged_into].name), si->merged_at); }
		if (si->type==ST_REMOVED) { continue; }
		if (si->address_assigned)
			fprintf(f, "Section " STRREF_FMT " (%d, %s): $%04x-$%04x\n", STRREF_ARG(si->name),
					(int)(&*si - &allSections[0]), si->type>=0 && si->type<num_section_type_str ?
					str_section_type[si->type] : "???", si->start_address, si->address);
		else
			fprintf(f, "Section " STRREF_FMT " (%d, %s) (relocatable)\n", STRREF_ARG(si->name),
				(int)(&*si - &allSections[0]), str_section_type[si->type]);

		if (!si->pListing)
			continue;
		for (Listing::iterator li = si->pListing->begin(); li != si->pListing->end(); ++li) {
			strown<256> out;
			const struct ListLine &lst = *li;
			if (prev_src.fnv1a() != lst.source_name.fnv1a() || lst.line_offs < prev_offs) {
				fprintf(f, STRREF_FMT "(%d):\n", STRREF_ARG(lst.source_name), lst.code.count_lines(lst.line_offs));
				prev_src = lst.source_name;
			} else {
				strref prvline = lst.code.get_substr(prev_offs, lst.line_offs - prev_offs);
				prvline.next_line();
				if (prvline.count_lines() < 5) {
					while (strref space_line = prvline.line()) {
						space_line.clip_trailing_whitespace();
						strown<128> line_fix(space_line);
						for (strl_t pos = 0; pos < line_fix.len(); ++pos) {
							if (line_fix[pos]=='\t') {
								line_fix.exchange(pos, 1, pos&1 ? strref(" ") : strref("  "));
							}
						}
						out.pad_to(' ', aCPUs[cpu].timing ? 40 : 33);
						out.append(line_fix.get_strref());
						fprintf(f, STRREF_FMT "\n", STRREF_ARG(out));
						out.clear();
					}
				} else {
					fprintf(f, STRREF_FMT "(%d):\n", STRREF_ARG(lst.source_name), lst.code.count_lines(lst.line_offs));
				}
			}

			if (lst.size) { out.sprintf_append("$%04x ", lst.address+si->start_address); }

			int s = lst.wasMnemonic() ? (lst.size < 4 ? lst.size : 4) : (lst.size < 8 ? lst.size : 8);
			if (si->output && si->output_capacity >= size_t(lst.address + s)) {
				for (int b = 0; b<s; ++b) {
					out.sprintf_append("%02x ", si->output[lst.address+b]);
				}
			}
			if (lst.startClock() && cycles_depth<MAX_DEPTH_CYCLE_COUNTER) {
				cycles_depth++;	cycles[cycles_depth].clr();
				out.pad_to(' ', 6); out.sprintf_append("c>%d", cycles_depth);
			}
			if (lst.stopClock()) {
				out.pad_to(' ', 6);
				if (cycles[cycles_depth].complex()) {
					out.sprintf_append("c<%d = %d + m%d + i%d + d%d", cycles_depth,
									   cycles[cycles_depth].base, cycles[cycles_depth].a16,
									   cycles[cycles_depth].x16, cycles[cycles_depth].dp);
				} else {
					out.sprintf_append("c<%d = %d + %d", cycles_depth,
									   cycles[cycles_depth].base, cycles[cycles_depth].plus_acc());
				}
				if (cycles_depth) {
					cycles_depth--;
					cycles[cycles_depth].combine(cycles[cycles_depth + 1]);
				}
			}
			if (lst.size && lst.wasMnemonic()) {
				out.pad_to(' ', 18);
				uint8_t *buf = si->output + lst.address;
				uint8_t op = mnemonic[*buf];
				uint8_t am = addrmode[*buf];
				if (op != 255 && am != 255 && am<(sizeof(aAddrModeFmt)/sizeof(aAddrModeFmt[0]))) {
					const char *fmt = aAddrModeFmt[am];
					if (opcode_table[op].modes & AMM_FLIPXY) {
						if (am == AMB_ZP_X)	fmt = "%s $%02x,y";
						else if (am == AMB_ABS_X) fmt = "%s $%04x,y";
					}
					if (opcode_table[op].modes & AMM_ZP_ABS) {
						out.sprintf_append(fmt, opcode_table[op].instr, buf[1], (char)buf[2]+lst.address+si->start_address+3);
					} else if (opcode_table[op].modes & AMM_BRANCH) {
						out.sprintf_append(fmt, opcode_table[op].instr, (char)buf[1]+lst.address+si->start_address+2);
					} else if (opcode_table[op].modes & AMM_BRANCH_L) {
						out.sprintf_append(fmt, opcode_table[op].instr, (int16_t)(buf[1]|(buf[2]<<8))+lst.address+si->start_address+3);
					} else if (am==AMB_NON||am==AMB_ACC) {
						out.sprintf_append(fmt, opcode_table[op].instr);
					} else if (am==AMB_ABS||am==AMB_ABS_X||am==AMB_ABS_Y||am==AMB_REL||am==AMB_REL_X||am==AMB_REL_L) {
						out.sprintf_append(fmt, opcode_table[op].instr, buf[1]|(buf[2]<<8));
					} else if (am==AMB_ABS_L||am==AMB_ABS_L_X) {
						out.sprintf_append(fmt, opcode_table[op].instr, buf[1]|(buf[2]<<8)|(buf[3]<<16));
					} else if (am==AMB_BLK_MOV) {
						out.sprintf_append(fmt, opcode_table[op].instr, buf[2], buf[1]);
					} else if (am==AMB_IMM && lst.size==3) {
						out.sprintf_append("%s #$%04x", opcode_table[op].instr, buf[1]|(buf[2]<<8));
					} else {
						out.sprintf_append(fmt, opcode_table[op].instr, buf[1]);
					}
					if (aCPUs[cpu].timing) {
						cycles[cycles_depth].add(aCPUs[cpu].timing[*buf]);
						out.pad_to(' ', 33);
						if (cycleCnt::sum_plus(aCPUs[cpu].timing[*buf])==1) {
							out.sprintf_append("%d+", cycleCnt::get_base(aCPUs[cpu].timing[*buf]));
						} else if (cycleCnt::sum_plus(aCPUs[cpu].timing[*buf])) {
							out.sprintf_append("%d+%d", cycleCnt::get_base(aCPUs[cpu].timing[*buf]), cycleCnt::sum_plus(aCPUs[cpu].timing[*buf]));
						} else {
							out.sprintf_append("%d", cycleCnt::get_base(aCPUs[cpu].timing[*buf]));
						}
					}
				}
			}

			out.pad_to(' ', aCPUs[cpu].timing ? 40 : 33);
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
	if (opened) { fclose(f); }
	return true;
}

// Create a listing of all valid instructions and addressing modes
bool Asm::AllOpcodes(strref filename) {
	FILE *f = stdout;
	bool opened = false;
	if (filename) {
		f = fopen(strown<512>(filename).c_str(), "w");
		if (!f) { return false; }
		opened = true;
	}
	for (int i = 0; i < opcode_count; i++) {
		uint32_t modes = opcode_table[i].modes;
		for (int a = 0; a < AMB_COUNT; a++) {
			if (modes & (1<<a)) {
				const char *fmt = aAddrModeFmt[a];
				fputs("\t", f);
				if (opcode_table[i].modes & AMM_BRANCH) {
					fprintf(f, "%s *+%d", opcode_table[i].instr, 5);
				} else if (a==AMB_ZP_ABS) {
					fprintf(f, "%s $%02x,*+%d", opcode_table[i].instr, 0x23, 13);
				} else {
					if (opcode_table[i].modes & AMM_FLIPXY) {
						if (a == AMB_ZP_X)	fmt = "%s $%02x,y";
						else if (a == AMB_ABS_X) fmt = "%s $%04x,y";
					}
					if (a == AMB_ABS_L || a == AMB_ABS_L_X) {
						if ((modes & ~(AMM_ABS_L|AMM_ABS_L_X))) {
							fprintf(f, a==AMB_ABS_L ? "%s.l $%06x" : "%s.l $%06x,x", opcode_table[i].instr, 0x222120);
						} else {
							fprintf(f, fmt, opcode_table[i].instr, 0x222120);
						}
					} else if (a==AMB_ABS||a==AMB_ABS_X||a==AMB_ABS_Y||a==AMB_REL||a==AMB_REL_X||a==AMB_REL_L) {
						fprintf(f, fmt, opcode_table[i].instr, 0x2120);
					} else if (a==AMB_IMM&&(modes&(AMM_IMM_DBL_A|AMM_IMM_DBL_XY))) {
						fprintf(f, "%s.b #$%02x\n", opcode_table[i].instr, 0x21);
						fprintf(f, "\t%s.w #$%04x", opcode_table[i].instr, 0x2322);
					} else {
						fprintf(f, fmt, opcode_table[i].instr, 0x21, 0x20, 0x1f);
					}
				}
				fputs("\n", f);
			}
		}
	}
	if (opened) { fclose(f); }
	return true;
}

// create an instruction table (mnemonic hash lookup + directives)
void Asm::Assemble(strref source, strref filename, bool obj_target) {
	SetCPU(cpu);
	StatusCode error = STATUS_OK;
	PushContext(filename, source, source);
	scope_address[scope_depth] = CurrSection().GetPC();
	while (contextStack.has_work() && error == STATUS_OK) {
		error = BuildSegment();
		if( error >= ERROR_STOP_PROCESSING_ON_HIGHER ) { break; }
		if (contextStack.curr().complete()) {
			error = PopContext();
		} else {
			error = FlushLocalLabels(scope_depth);
			if (error>=FIRST_ERROR) { break; }
			contextStack.curr().restart();
			error = STATUS_OK;
		}
	}
	if (error==STATUS_OK) {
		if (!obj_target) { LinkZP(); }
		error = CheckLateEval();
		if (error>STATUS_XREF_DEPENDENT) {
			strown<512> errorText;
			errorText.copy("Error: ");
			errorText.append(aStatusStrings[error]);
			fwrite(errorText.get(), errorText.get_len(), 1, stderr);
		} else { CheckLateEval(strref(), -1, true); } // output any missing xref's

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
	} else {
		PrintError(contextStack.has_work() ?
				   contextStack.curr().read_source.get_line() : strref(), error);
	}
}

//
//
// OBJECT FILE HANDLING
//
//

struct ObjFileHeader {
	int16_t id;	// 'x6'
	int16_t sections;
	int16_t relocs;
	int16_t labels;
	int16_t late_evals;
	int16_t map_symbols;
	uint32_t stringdata;
	uint32_t srcdebug;
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
	int end_address;		// address size
	int output_size;		// assembled binary size
	int align_address;
	int srcdebug_count;		// how many addresses included in debugger listing
	int16_t next_group;		// next section of group
	int16_t first_group;	// first section of group
	int16_t relocs;
	SectionType type;
	int8_t flags;
};

struct ObjFileSource {
	struct ObjFileStr file;
};

struct ObjFileReloc {
	int base_value;
	int section_offset;
	int16_t target_section;
	int8_t bytes;
	int8_t shift;
};

struct ObjFileLabel {
	enum LabelFlags {
		OFL_EVAL = (1<<15),		// Evaluated (may still be relative)
		OFL_ADDR = (1<<14),		// Address or Assign
		OFL_CNST = (1<<13),		// Constant
		OFL_XDEF = OFL_CNST-1	// External (index into file array)
	};
	struct ObjFileStr name;
	int value;
	int flags;					// 1<<(LabelFlags)
	int16_t section;			// -1 if resolved, file section # if section rel
	int16_t mapIndex;			// -1 if resolved, index into map if relative
};

struct ObjFileLateEval {
	struct ObjFileStr label;
	struct ObjFileStr expression;
	int address;				// PC relative to section or fixed
	int target;					// offset into section memory
	int16_t section;			// section to target
	int16_t rept;				// value of rept for this late eval
	int16_t scope;				// PC start of scope
	int16_t type;				// label, byte, branch, word (LateEval::Type)
};

struct ObjFileMapSymbol {
	struct ObjFileStr name;		// symbol name
	int value;
	int16_t section;			// address relative to this section
	int16_t orig_section;		// seciton label originated from
	bool local;					// local labels are probably needed
};

// this struct is follwed by numSources x ObjFileStr
struct ObjFileSourceList {
	uint32_t numSources;
	ObjFileStr sourceFile[1];
};

// after that one long array of all sections worth of source references
struct ObjFileSrcDbg {
	uint16_t addr;		// relative to section
	uint16_t src_idx;
	uint16_t size;
	uint16_t _pad;
	uint32_t file_offs;	// byte offset into file to row/column where op starts1
};


// Simple string pool, converts strref strings to zero terminated strings and returns the offset to the string in the pool.
static int _AddStrPool(const strref str, pairArray<uint32_t, int> *pLookup, char **strPool, uint32_t &strPoolSize, uint32_t &strPoolCap) {
	if (!str.get()||!str.get_len()) { return -1; }	// empty string

	uint32_t hash = str.fnv1a();
	uint32_t index = FindLabelIndex(hash, pLookup->getKeys(), pLookup->count());
	if (index<pLookup->count()&&str.same_str_case(*strPool+pLookup->getValue(index))) {
		return pLookup->getValue(index);
	}
	int strOffs = strPoolSize;
	if ((strOffs + str.get_len() + 1) > strPoolCap) {
		strPoolCap = strOffs + (uint32_t)str.get_len() + 4096;
		char *strPoolGrow = (char*)malloc(strPoolCap);
		if (strPoolGrow) {
			if (*strPool) {
				memcpy(strPoolGrow, *strPool, strPoolSize);
				free(*strPool);
			}
			*strPool = strPoolGrow;
		} else { return -1; }
	}

	if (*strPool) {
		char *dest = *strPool + strPoolSize;
		memcpy(dest, str.get(), str.get_len());
		dest[str.get_len()] = 0;
		strPoolSize += (uint32_t)str.get_len()+1;
		pLookup->insert(index, hash);
		pLookup->getValues()[index] = strOffs;
	}
	return strOffs;
}

void Asm::ShowReferences()
{
	size_t num = labels.count();
	const Label* lbl = labels.getValues();
	printf("LABEL REFERENCE SUMMARY:\n");
	for (size_t l = 0; l < num; ++l) {
		if (lbl[l].external) {
			printf(" * >" STRREF_FMT " Sect: " STRREF_FMT " Offs: $%0x\n",
				   STRREF_ARG(lbl[l].label_name), STRREF_ARG(lbl[l].section >= 0 ? allSections[lbl[l].section].name : strref("fixed")), lbl[l].value);
		}
		if (lbl[l].reference) {
			printf(" * <" STRREF_FMT " Sect: " STRREF_FMT " Offs: $%0x\n",
				   STRREF_ARG(lbl[l].label_name), STRREF_ARG(lbl[l].section >= 0 ? allSections[lbl[l].section].name : strref("fixed")), lbl[l].value);
		}
	}
}

StatusCode Asm::WriteObjectFile(strref filename) {
	if (allSections.size()==0)
		return ERROR_NOT_A_SECTION;
	if (FILE *f = fopen(strown<512>(filename).c_str(), "wb")) {
		struct ObjFileHeader hdr = { 0 };
		hdr.id = 0x7836;
		hdr.sections = (int16_t)allSections.size();
		hdr.relocs = 0;
		hdr.bindata = 0;
		
		for (std::vector<Section>::iterator s = allSections.begin(); s!=allSections.end(); ++s) {
			if (s->type != ST_REMOVED) {
				if (s->pRelocs) { hdr.relocs += int16_t(s->pRelocs->size()); }
				hdr.bindata += s->size();
			}
		}
		hdr.late_evals = (int16_t)lateEval.size();
		hdr.map_symbols = (int16_t)map.size();
		hdr.stringdata = 0;
		hdr.srcdebug = 0;

		// labels don't include XREF labels
		hdr.labels = 0;
		for (uint32_t l = 0; l<labels.count(); l++) {
			const Label &lo = labels.getValue(l);
			if (!lo.reference && !lo.borrowed) { hdr.labels++; }
		}

		int16_t *aRemapSects = hdr.sections ? (int16_t*)malloc(sizeof(int16_t) * hdr.sections) : nullptr;
		if (!aRemapSects) {
			fclose(f);
			return ERROR_OUT_OF_MEMORY;
		}

		// include space for external protected labels
		for (std::vector<ExtLabels>::iterator el = externals.begin(); el!=externals.end(); ++el) {
			hdr.labels += (int16_t)el->labels.count();
		}
		char *stringPool = nullptr;
		uint32_t stringPoolCap = 0;
		pairArray<uint32_t, int> stringArray;
		stringArray.reserve(hdr.labels * 2 + hdr.sections + hdr.late_evals*2 + (uint32_t)source_files.size());

		struct ObjFileSection *aSects = hdr.sections ? (struct ObjFileSection*)calloc(hdr.sections, sizeof(struct ObjFileSection)) : nullptr;
		struct ObjFileReloc *aRelocs = hdr.relocs ? (struct ObjFileReloc*)calloc(hdr.relocs, sizeof(struct ObjFileReloc)) : nullptr;
		struct ObjFileLabel *aLabels = hdr.labels ? (struct ObjFileLabel*)calloc(hdr.labels, sizeof(struct ObjFileLabel)) : nullptr;
		struct ObjFileLateEval *aLateEvals = hdr.late_evals ? (struct ObjFileLateEval*)calloc(hdr.late_evals, sizeof(struct ObjFileLateEval)) : nullptr;
		struct ObjFileMapSymbol *aMapSyms = hdr.map_symbols ? (struct ObjFileMapSymbol*)calloc(hdr.map_symbols, sizeof(struct ObjFileMapSymbol)) : nullptr;
		int sect = 0, reloc = 0, labs = 0, late = 0, map_sym = 0;

		memset(aRemapSects, 0xff, sizeof(int16_t) * hdr.sections);

		// discard the removed sections by making a table of skipped indices
		for (std::vector<Section>::iterator si = allSections.begin(); si!=allSections.end(); ++si) {
			if (si->type != ST_REMOVED)
				aRemapSects[&*si-&allSections[0]] = (int16_t)sect++;
		}
		
		sect = 0;
		uint32_t srcDbgEntries = 0;
		// write out sections and relocs
		if (hdr.sections) {
			for (std::vector<Section>::iterator si = allSections.begin(); si!=allSections.end(); ++si) {
				if (si->type == ST_REMOVED) { continue; }
				struct ObjFileSection &s = aSects[sect++];
				s.name.offs = _AddStrPool(si->name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				s.exp_app.offs = _AddStrPool(si->export_append, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				s.output_size = si->size();
				s.align_address = si->align_address;
				s.next_group = si->next_group >= 0 ? aRemapSects[si->next_group] : -1;
				s.first_group = si->first_group >= 0 ? aRemapSects[si->first_group] : -1;
				s.relocs = si->pRelocs ? (int16_t)(si->pRelocs->size()) : 0;
				s.srcdebug_count = si->pSrcDbg ? (int)si->pSrcDbg->size() : 0;
				srcDbgEntries += s.srcdebug_count;
				s.start_address = si->start_address;
				s.end_address = si->address;
				s.type = si->type;
				s.flags =
					(si->IsDummySection() ? (1 << ObjFileSection::OFS_DUMMY) : 0) |
					(si->IsMergedSection() ? (1 << ObjFileSection::OFS_MERGED) : 0) |
					(si->address_assigned ? (1 << ObjFileSection::OFS_FIXED) : 0);
				if (si->pRelocs && si->pRelocs->size() && aRelocs) {
					for (relocList::iterator ri = si->pRelocs->begin(); ri!=si->pRelocs->end(); ++ri) {
						struct ObjFileReloc &r = aRelocs[reloc++];
						r.base_value = ri->base_value;
						r.section_offset = ri->section_offset;
						r.target_section = ri->target_section >= 0 ? aRemapSects[ri->target_section] : -1;
						r.bytes = ri->bytes;
						r.shift = ri->shift;
					}
				}
			}
		}
		hdr.sections = (int16_t)sect;

		struct ObjFileSrcDbg* srcLines = srcDbgEntries ? (ObjFileSrcDbg*)calloc(1, sizeof(ObjFileSrcDbg) * srcDbgEntries) : nullptr;
		if (srcLines) {
			size_t srcDbgIdx = 0;
			for (std::vector<Section>::iterator si = allSections.begin(); si != allSections.end(); ++si) {
				if (si->type == ST_REMOVED) { continue; }
				if (si->pSrcDbg && si->pSrcDbg->size()) {
					for (size_t src = 0, nsrc = si->pSrcDbg->size(); src < nsrc; ++src) {
						SourceDebugEntry& entry = si->pSrcDbg->at(src);
						ObjFileSrcDbg& ref = srcLines[srcDbgIdx++];
						ref.addr = entry.address;
						ref.src_idx = entry.source_file_index;
						ref.size = entry.size;
						ref.file_offs = entry.source_file_offset;
					}
				}
			}
			assert(srcDbgIdx == srcDbgEntries);
		}

		// write out labels
		if (hdr.labels) {
			for (uint32_t li = 0; li<labels.count(); li++) {
				Label &lo = labels.getValue(li);
				if (!lo.reference && !lo.borrowed) {
					struct ObjFileLabel &l = aLabels[labs++];
					l.name.offs = _AddStrPool(lo.label_name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
					l.value = lo.value;
					l.section = lo.section >=0 ? aRemapSects[lo.section] : -1;
					l.mapIndex = (int16_t)lo.mapIndex;
					l.flags =
						(lo.constant ? ObjFileLabel::OFL_CNST : 0) |
						(lo.pc_relative ? ObjFileLabel::OFL_ADDR : 0) |
						(lo.evaluated ? ObjFileLabel::OFL_EVAL : 0) |
						(lo.external ? ObjFileLabel::OFL_XDEF : 0);
				}
			}
		}

		// protected labels included from other object files
		if (hdr.labels) {
			int file_index = 1;
			for (std::vector<ExtLabels>::iterator el = externals.begin(); el != externals.end(); ++el) {
				for (uint32_t li = 0; li < el->labels.count(); ++li) {
					Label &lo = el->labels.getValue(li);
					if (!lo.borrowed) {
						struct ObjFileLabel& l = aLabels[labs++];
						l.name.offs = _AddStrPool(lo.label_name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
						l.value = lo.value;
						l.section = lo.section >= 0 ? aRemapSects[lo.section] : -1;
						l.mapIndex = (int16_t)lo.mapIndex;
						l.flags =
							(lo.constant ? ObjFileLabel::OFL_CNST : 0) |
							(lo.pc_relative ? ObjFileLabel::OFL_ADDR : 0) |
							(lo.evaluated ? ObjFileLabel::OFL_EVAL : 0) |
							file_index;
					}
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
				le.section = lei->section >= 0 ? aRemapSects[lei->section] : -1;
				le.rept = lei->rept;
				le.target = (int16_t)lei->target;
				le.address = lei->address;
				le.scope = (int16_t)lei->scope;
				le.type = (int16_t)lei->type;
			}
		}

		// write out map symbols
		if (aMapSyms) {
			for (MapSymbolArray::iterator mi = map.begin(); mi != map.end(); ++mi) {
				struct ObjFileMapSymbol &ms = aMapSyms[map_sym++];
				ms.name.offs = _AddStrPool(mi->name, &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
				ms.value = mi->value;
				ms.local = mi->local;
				ms.section = mi->section >= 0 ? aRemapSects[mi->section] : -1;
				ms.orig_section = mi->orig_section >= 0 ? aRemapSects[mi->orig_section] : -1;
			}
		}

		struct ObjFileSourceList* sourceList = nullptr;
		if (source_files.size() && srcLines) {
			sourceList = (struct ObjFileSourceList*)calloc(1, sizeof(ObjFileSourceList) + sizeof(ObjFileStr) * (source_files.size() - 1));
			sourceList->numSources = (uint32_t)source_files.size();
			for (size_t src = 0, nsrc = source_files.size(); src < nsrc; ++src) {
				sourceList->sourceFile[src].offs = _AddStrPool(strref(source_files[src]), &stringArray, &stringPool, hdr.stringdata, stringPoolCap);
			}
		}

		hdr.srcdebug = srcDbgEntries;

		// write out the file
		fwrite(&hdr, sizeof(hdr), 1, f);
		fwrite(aSects, sizeof(aSects[0]), sect, f);
		fwrite(aRelocs, sizeof(aRelocs[0]), reloc, f);
		fwrite(aLabels, sizeof(aLabels[0]), labs, f);
		fwrite(aLateEvals, sizeof(aLateEvals[0]), late, f);
		fwrite(aMapSyms, sizeof(aMapSyms[0]), map_sym, f);
		fwrite(stringPool, hdr.stringdata, 1, f);
		for (std::vector<Section>::iterator si = allSections.begin(); si!=allSections.end(); ++si) {
			if (!si->IsDummySection()&&!si->IsMergedSection()&&si->size()!=0&&si->type!=ST_REMOVED) {
				fwrite(si->output, si->size(), 1, f);
			}
		}
		if (sourceList) {
			fwrite(sourceList, sizeof(ObjFileSourceList) + sizeof(ObjFileStr) * (source_files.size() - 1), 1, f);
			fwrite(srcLines, sizeof(ObjFileSrcDbg) * srcDbgEntries, 1, f);
		}


		// done with I/O
		fclose(f);
		if (aRemapSects) { free(aRemapSects); }
		if (stringPool) { free(stringPool); }
		if (aMapSyms) { free(aMapSyms); }
		if (aLateEvals) { free(aLateEvals); }
		if (aLabels) { free(aLabels); }
		if (aRelocs) { free(aRelocs); }
		if (aSects) { free(aSects); }
		stringArray.clear();
	}
	return STATUS_OK;
}

StatusCode Asm::ReadObjectFile(strref filename, int link_to_section)
{
	size_t size;
	strown<512> file;
	file.copy(filename); // Merlin mostly uses extension-less files, append .x65 as a default
	if ((Merlin() && !file.has_suffix(".x65")) || filename.find('.')<0)
		file.append(".x65");
	int file_index = (int)externals.size();
	if (char *data = LoadBinary(file.get_strref(), size)) {
		struct ObjFileHeader &hdr = *(struct ObjFileHeader*)data;
		size_t sum = sizeof(hdr) + hdr.sections*sizeof(struct ObjFileSection) +
			hdr.relocs * sizeof(struct ObjFileReloc) + hdr.labels * sizeof(struct ObjFileLabel) +
			hdr.late_evals * sizeof(struct ObjFileLateEval) +
			hdr.map_symbols * sizeof(struct ObjFileMapSymbol) + hdr.stringdata + hdr.bindata;
		const struct ObjFileSourceList* pSrcDbgInfo = hdr.srcdebug ? (struct ObjFileSourceList*)(data + sum) : nullptr;
		const struct ObjFileSrcDbg* pSrcDbgEntry = pSrcDbgInfo ? (const struct ObjFileSrcDbg*)(pSrcDbgInfo->sourceFile + pSrcDbgInfo->numSources) : nullptr;

		if (pSrcDbgInfo) {
			sum += sizeof(ObjFileSourceList) + (pSrcDbgInfo->numSources - 1) * sizeof(ObjFileStr);
			sum += sizeof(ObjFileSrcDbg) * hdr.srcdebug;
		}

		if (hdr.id == 0x7836 && sum == size) {
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

			// source files
			std::vector<size_t> source_file_remap;
			if (pSrcDbgInfo) {
				source_file_remap.reserve(pSrcDbgInfo->numSources);
				for (size_t s = 0, n = pSrcDbgInfo->numSources; s < n; ++s) {
					strref source_file(str_pool + pSrcDbgInfo->sourceFile[s].offs);
					size_t i = 0, sz = source_files.size();
					for (; i < sz; ++i) {
						if (source_file.same_str(source_files[i])) { break; }
					}
					if (i == sz) {
						source_files.push_back(StringCopy(source_file));
					}
					source_file_remap.push_back(i);
				}
			}

			int prevSection = SectionId();
			int16_t *aSctRmp = (int16_t*)malloc(hdr.sections * sizeof(int16_t));
			int last_linked_section = link_to_section;
			while (last_linked_section>=0 && allSections[last_linked_section].next_group>=0) {
				last_linked_section = allSections[last_linked_section].next_group;
			}

			// sections
			for (int si = 0; si < hdr.sections; si++) {
				int16_t f = aSect[si].flags;
				if (f & (1 << ObjFileSection::OFS_MERGED))
					continue;
				if (f & (1 << ObjFileSection::OFS_DUMMY)) {
					if (f&(1 << ObjFileSection::OFS_FIXED)) {
						DummySection(aSect[si].start_address);
						CurrSection().AddBin(nullptr, aSect[si].end_address - aSect[si].start_address);
					} else {
						DummySection();
						CurrSection().AddBin(nullptr, aSect[si].end_address - aSect[si].start_address);
					}
				} else {
					if (f&(1<<ObjFileSection::OFS_FIXED)) {
						SetSection(aSect[si].name.offs>=0 ? strref(str_pool+aSect[si].name.offs) : strref(), aSect[si].start_address);
					} else {
						SetSection(aSect[si].name.offs>=0 ? strref(str_pool+aSect[si].name.offs) : strref());
					}
					Section &s = CurrSection();
					s.include_from = filename;
					s.export_append = aSect[si].exp_app.offs>=0 ? strref(str_pool + aSect[si].name.offs) : strref();
					s.align_address = aSect[si].align_address;
					s.address = aSect[si].end_address;
					s.type = aSect[si].type;
					if (aSect[si].output_size) {
						s.output = (uint8_t*)malloc(aSect[si].output_size);
						memcpy(s.output, bin_data, aSect[si].output_size);
						s.curr = s.output + aSect[si].output_size;
						s.output_capacity = aSect[si].output_size;
						bin_data += aSect[si].output_size;
					}
					if (last_linked_section>=0) {
						allSections[last_linked_section].next_group = SectionId();
						s.first_group = allSections[last_linked_section].first_group >=0 ? allSections[last_linked_section].first_group : last_linked_section;
						last_linked_section = SectionId();
					}
					// add source debug entries from object file
					if (aSect[si].srcdebug_count) {
						if (!s.pSrcDbg) { s.pSrcDbg = new SourceDebug; }
						for (int sd = 0, nsd = aSect[si].srcdebug_count; sd < nsd; ++sd) {
							SourceDebugEntry entry;
							entry.address = pSrcDbgEntry->addr;
							entry.size = pSrcDbgEntry->size;
							entry.source_file_index = (int)source_file_remap[pSrcDbgEntry->src_idx];
							entry.source_file_offset = pSrcDbgEntry->file_offs;
							s.pSrcDbg->push_back(entry);
							++pSrcDbgEntry;
						}
					}
				}
				aSctRmp[si] = (int16_t)allSections.size()-1;
			}

			// fix up groups and relocs
			int curr_reloc = 0;
			for (int si = 0; si < hdr.sections; si++) {
				Section &s = allSections[aSctRmp[si]];
				if (aSect[si].first_group >= 0)
					s.first_group = aSctRmp[aSect[si].first_group];
				if (aSect[si].next_group >= 0)
					s.first_group = aSctRmp[aSect[si].next_group];
				for (int ri = 0; ri < aSect[si].relocs; ri++) {
					int r = ri + curr_reloc;
					struct ObjFileReloc &rs = aReloc[r];
					allSections[aSctRmp[si]].AddReloc(rs.base_value, rs.section_offset, aSctRmp[rs.target_section], rs.bytes, rs.shift);
				}
				curr_reloc += aSect[si].relocs;
			}

			for (int mi = 0; mi < hdr.map_symbols; mi++) {
				struct ObjFileMapSymbol &m = aMapSyms[mi];
				if (map.size()==map.capacity()) {
					map.reserve(map.size()+256);
				}
				MapSymbol sym;
				sym.name = m.name.offs>=0 ? strref(str_pool + m.name.offs) : strref();
				sym.section = m.section >=0 ? aSctRmp[m.section] : m.section;
				sym.orig_section = m.orig_section >= 0 ? aSctRmp[m.orig_section] : m.orig_section;
				sym.value = m.value;
				sym.local = m.local;
				map.push_back(sym);
			}

			for (int li = 0; li < hdr.labels; li++) {
				struct ObjFileLabel &l = aLabels[li];
				strref name = l.name.offs >= 0 ? strref(str_pool + l.name.offs) : strref();
				Label *lbl = GetLabel(name);
				int16_t f = (int16_t)l.flags;
				int external = f & ObjFileLabel::OFL_XDEF;
				if (external == ObjFileLabel::OFL_XDEF) {
					if (!lbl) { lbl = AddLabel(name.fnv1a()); lbl->referenced = false; }	// insert shared label
					else if (!lbl->reference) { continue; }
				} else {								// insert protected label
					while ((file_index + external) >= (int)externals.size()) {
						if (externals.size()==externals.capacity()) {
							externals.reserve(externals.size()+32);
						}
						externals.push_back(ExtLabels());
					}
					uint32_t hash = name.fnv1a();
					uint32_t index = FindLabelIndex(hash, externals[file_index].labels.getKeys(), externals[file_index].labels.count());
					externals[file_index].labels.insert(index, hash);
					lbl = externals[file_index].labels.getValues() + index;
				}
				lbl->label_name = name;
				lbl->pool_name.clear();
				lbl->value = l.value;
				lbl->section = l.section >= 0 ? aSctRmp[l.section] : l.section;
				lbl->mapIndex = l.mapIndex >= 0 ? (l.mapIndex + (int)map.size()) : -1;
				lbl->evaluated = !!(f & ObjFileLabel::OFL_EVAL);
				lbl->pc_relative = !!(f & ObjFileLabel::OFL_ADDR);
				lbl->constant = !!(f & ObjFileLabel::OFL_CNST);
				lbl->external = external == ObjFileLabel::OFL_XDEF;
				lbl->reference = false;
			}
			// no protected labels => don't track as separate file
			if (file_index==(int)externals.size()) { file_index = -1; }

			for (int li = 0; li < hdr.late_evals; ++li) {
				struct ObjFileLateEval &le = aLateEval[li];
				strref name = le.label.offs >= 0 ? strref(str_pool + le.label.offs) : strref();
				Label *pLabel = GetLabel(name);
				if (pLabel) {
					if (pLabel->evaluated) {
						AddLateEval(name, le.address, le.scope, strref(str_pool + le.expression.offs), (LateEval::Type)le.type);
						LateEval &last = lateEval[lateEval.size()-1];
						last.section = le.section >= 0 ? aSctRmp[le.section] : le.section;
						last.rept = le.rept;
						last.source_file = strref();
						last.file_ref = file_index;
					}
				} else {
					AddLateEval(le.target, le.address, le.scope, strref(str_pool + le.expression.offs), strref(), (LateEval::Type)le.type);
					LateEval &last = lateEval[lateEval.size()-1];
					last.section = le.section >= 0 ? aSctRmp[le.section] : le.section;
					last.rept = le.rept;
					last.file_ref = file_index;
				}
			}
			free(aSctRmp);

			// restore previous section
			current_section = &allSections[prevSection];
		} else { return ERROR_NOT_AN_X65_OBJECT_FILE; }

	}
	return STATUS_OK;
}

// number of section types that can be merged
enum OMFRecCode {
	OMFR_END = 0,
	OMFR_RELOC = 0xe2,	// bytes.b, bitshift.b, offset.l, value.l
	OMFR_INTERSEG = 0xe3, // bytes.b, bitshift.b, offset.l, filenum.w, segnum.w, offsref.l
	OMFR_LCONST = 0xf2,	// bytes.b, b,b,b, data.b[bytes]
	OMFR_cRELOC = 0xf5,		// bytes.b, bitshift.b, offset.w, value.w
	OMFR_cINTERSEG = 0xf6,	// bytes.b, bitshift.b, offset.w, segnum.b, offsref.w
	OMFR_SUPER = 0xf7,
};

struct OMFSegHdr {
	uint8_t SegTotal[4];				// + Segment Segment Header size Body size
	uint8_t ResSpc[4];				// Number of 0x00 to add to the end of Body
	uint8_t Length[4];				// Memory Size Segment
	uint8_t pad1[1];
	uint8_t LabLen[1];				// Length Names: 10
	uint8_t NumLen[1];				// Size = 4 numbers uint8s
	uint8_t Version[1];				// OMF Version: 2
	uint8_t BankSize[4];				// Size of a Bank: 64 KB code if, between 0 and 64 KB for Data
	uint8_t Kind[2];					// Type Segment
	uint8_t pad2[2];
	uint8_t Org[4];
	uint8_t Align[4];				// Alignment: 0, 64 or 256 KB
	uint8_t NumSEx[1];				// Little Endian: 0 for IIgs
	uint8_t pad3[1];
	uint8_t SegNum[2];				// Segment Number: 1-> N
	uint8_t EntryPointOffset[4];		// Entry point in the Segment
	uint8_t DispNameOffset[2];		// Where the offset is located LoadName
	uint8_t DispDataOffset[2];		// Offset begins the Body Segment
	uint8_t tempOrg[4];				// 
};

// write a number of bytes of a value
static void _writeNBytes(uint8_t *dest, int bytes, int value) {
	while (bytes--) {
		*dest++ = (uint8_t)value;
		value >>= 8;
	}
}

// sort relocs before writing GS OS reloc instructions
static int sortRelocByOffs(const void *A, const void *B) {
	return ((const Reloc*)A)->section_offset - ((const Reloc*)B)->section_offset;
}

// Export an Apple II GS relocatable executable
StatusCode Asm::WriteA2GS_OMF(strref filename, bool full_collapse) {
	// determine the section with startup code - either first loaded object file or current file
	int first_section = 0;
	for (int s = 1; s<(int)allSections.size(); s++) {
		if (allSections[s].type==ST_CODE && (allSections[first_section].type != ST_CODE ||
			(!allSections[s].include_from && allSections[first_section].include_from)))
			first_section = s;
	}

	// collapse all section together that share the same name
	StatusCode status = MergeSectionsByName(first_section);
	if (status!=STATUS_OK) { return status; }

	// Zero page section for x65 implies addresses, OMF direct-page/stack seg implies size of direct page + stack
	// so resolve the zero page sections first
	status = LinkZP();
	if (status!=STATUS_OK) { return status; }

	// full collapse means that all sections gets merged into one
	// code+data section and one bss section which will be appended
	if (full_collapse) {
		status = MergeAllSections(first_section);
		if (status!=STATUS_OK) { return status; }
	}

	// determine if there is a direct page stack
	int DP_Stack_Size = 0;	// 0 => default size (don't include)
	for (std::vector<Section>::iterator s = allSections.begin(); s != allSections.end(); ++s) {
		if (s->type==ST_ZEROPAGE) { s->type = ST_REMOVED; }
		if (s->type == ST_BSS && s->name.same_str("directpage_stack")) {
			DP_Stack_Size += s->addr_size();
			s->type = ST_REMOVED;
		}
	}

	std::vector<int> SegNum;	// order of valid segments
	std::vector<int> SegLookup;	// inverse of SegNum
	SegNum.reserve(allSections.size());
	SegLookup.reserve(allSections.size());
	int reloc_max = 1;

	// OMF super instructions work by incremental addresses, sort relocs to simplify output
	for (std::vector<Section>::iterator s = allSections.begin(); s != allSections.end(); ++s) {
		if (first_section==SectionId(*s)) {
			SegNum.insert(SegNum.begin(), SectionId(*s));
		} else if (s->type!=ST_REMOVED) {
			SegNum.push_back(SectionId(*s));
		}
		SegLookup.push_back(-1);
		if ((s->type == ST_CODE || s->type == ST_DATA) && s->pRelocs && s->pRelocs->size() > 1) {
			qsort(&(*s->pRelocs)[0], s->pRelocs->size(), sizeof(Reloc), sortRelocByOffs);
			if ((int)s->pRelocs->size()>reloc_max) {
				reloc_max = (int)s->pRelocs->size();
			}
		}
	}

	// reloc_max needs to greater than zero
	if (reloc_max<1) { return ERROR_ABORTED; }

	for (std::vector<int>::iterator i = SegNum.begin(); i!=SegNum.end(); ++i) {
		SegLookup[*i] = (int)(&*i-&SegNum[0]);
	}
	uint8_t *instructions = (uint8_t*)malloc(reloc_max * 16);
	if (!instructions) { return ERROR_OUT_OF_MEMORY; }
	
	// open a file for writing
	FILE *f = fopen(strown<512>(filename).c_str(), "wb");
	if (!f) {
		free(instructions);
		return ERROR_CANT_WRITE_TO_FILE;
	}

	// consume all the relocs
	struct OMFSegHdr hdr = { 0 };	// initialize segment header
	hdr.NumLen[0] = 4;		// numbers are 4 bytes under GS OS
	hdr.Version[0] = 2;		// version is 2 for GS OS
	hdr.BankSize[2] = 1;	// 64k banks
	_writeNBytes(hdr.DispNameOffset, 2, sizeof(hdr));	// start of file name (10 chars)

	strref fileBase = export_base_name;
	char segfile[10];
	memset(segfile, ' ', 10);
	memcpy(segfile, fileBase.get(), fileBase.get_len() > 10 ? 10 : fileBase.get_len());

	for (std::vector<int>::iterator i = SegNum.begin(); i != SegNum.end(); ++i) {
		Section &s = allSections[*i];
		strref segName = s.name ? s.name : (s.type == ST_CODE ? strref("CODE") : strref("DATA"));

		// support zero bytes at end of block
		int num_zeroes_at_end = s.addr_size() - s.size();
		int num_bytes_file = s.size();
		while (num_bytes_file && s.output[num_bytes_file - 1] == 0) {
			num_zeroes_at_end++;
			num_bytes_file--;
		}

		_writeNBytes(hdr.SegNum, 2, SegLookup[*i] + 1);
		_writeNBytes(hdr.Kind, 2, s.type == ST_CODE ? 0x1000 : (s.type == ST_ZEROPAGE ? 0x12 : 0x1001));
		_writeNBytes(hdr.DispDataOffset, 2, sizeof(hdr) + 10 + 1 + (int)segName.get_len());
		_writeNBytes(hdr.Length, 4, num_bytes_file + num_zeroes_at_end);
		_writeNBytes(hdr.ResSpc, 4, num_zeroes_at_end);
		_writeNBytes(hdr.Align, 4, s.align_address > 1 ? 256 : 0);
		// instruction list starts with a LCONST + Length(4) + binary, the relocs begin after that
		int instruction_offs = 0;
		instructions[instruction_offs++] = OMFR_LCONST;
		_writeNBytes(instructions+instruction_offs, 4, num_bytes_file);
		instruction_offs += 4;
		if (s.pRelocs && s.pRelocs->size()) {
			// insert all SUPER_RELOC2 / SUPER_RELOC3
			for (int b = 0; b <= 1; b++) {
				int count_offs = -1;
				int prev_page = -1;
				int inst_curr = instruction_offs;
				instructions[inst_curr++] = OMFR_SUPER;
				int len_offs = inst_curr;
				inst_curr += 4;
				instructions[inst_curr++] = (uint8_t)b;	// SUPER_RELOC2 / SUPER_RELOC3
				// try all SUPER_RELOC2 (2 bytes self reference, no shift)
				relocList::iterator r = s.pRelocs->begin();
				while (r != s.pRelocs->end()) {
					if (r->shift == 0 && r->bytes == (b+2) && r->target_section == SectionId(s)) {
						if ((r->section_offset >> 8) != prev_page) {
							instructions[inst_curr++] = uint8_t(0x80 | ((r->section_offset >> 8) - prev_page - 1));
							count_offs = -1;
						}	// update patch counter for current page or add a new counter
						if (count_offs < 0) {
							count_offs = inst_curr;
							instructions[inst_curr++] = 0;
						} else
							instructions[count_offs]++;
						prev_page = r->section_offset>>8;
						instructions[inst_curr++] = (uint8_t)r->section_offset;	// write patch offset into binary
						_writeNBytes(s.output + r->section_offset, b + 2, r->base_value);	// patch binary with base value
						r = s.pRelocs->erase(r);
					} else
						++r;
				}
				if (inst_curr > (instruction_offs + 6)) {
					_writeNBytes(instructions + len_offs, 4, inst_curr - instruction_offs - 5);
					instruction_offs = inst_curr;
				}
			}
			// insert all other records as they are encountered
			relocList::iterator r = s.pRelocs->begin();
			while (r != s.pRelocs->end()) {
				if (r->target_section == SectionId(s)) {
					// this is a reloc, check if cRELOC is ok or if need RELOC
					bool cRELOC = r->section_offset < 0x10000 && r->base_value < 0x10000;
					instructions[instruction_offs++] = uint8_t(cRELOC ? OMFR_cRELOC : OMFR_RELOC);
					instructions[instruction_offs++] = r->bytes;
					instructions[instruction_offs++] = r->shift;
					_writeNBytes(instructions + instruction_offs, cRELOC ? 2 : 4, r->section_offset);
					instruction_offs += cRELOC ? 2 : 4;
					_writeNBytes(instructions + instruction_offs, cRELOC ? 2 : 4, r->base_value);
					instruction_offs += cRELOC ? 2 : 4;
				} else {
					// this is an interseg
					bool cINTERSEG = r->section_offset < 0x10000 && r->base_value < 0x10000;
					instructions[instruction_offs++] = uint8_t(cINTERSEG ? OMFR_cINTERSEG : OMFR_INTERSEG);
					instructions[instruction_offs++] = r->bytes;
					instructions[instruction_offs++] = r->shift;
					_writeNBytes(instructions + instruction_offs, cINTERSEG ? 2 : 4, r->section_offset);
					instruction_offs += cINTERSEG ? 2 : 4;
					_writeNBytes(instructions + instruction_offs, cINTERSEG ? 0: 2 , 1);	// file number = 1
					instruction_offs += cINTERSEG ? 0 : 2;
					_writeNBytes(instructions + instruction_offs, cINTERSEG ? 1 : 2, SegLookup[r->target_section] + 1);	// segment number starting from 1
					instruction_offs += cINTERSEG ? 1 : 2;
					_writeNBytes(instructions + instruction_offs, cINTERSEG ? 2 : 4, r->base_value);
					instruction_offs += cINTERSEG ? 2 : 4;
				}
				r = s.pRelocs->erase(r);
			}
		}
		instructions[instruction_offs++] = OMFR_END;

		// size of seg = file header + 10 bytes file name + 1 byte seg name length + seg nameh + seg.addr_size() + instruction_size 
		int segSize = sizeof(hdr) + 10 + 1 + (int)segName.get_len() + num_bytes_file + instruction_offs;
		if (num_bytes_file==0) { segSize -= 5; }
		_writeNBytes(hdr.SegTotal, 4, segSize);
		uint8_t lenSegName = (uint8_t)segName.get_len();
		fwrite(&hdr, sizeof(hdr), 1, f);
		fwrite(segfile, 10, 1, f);
		fwrite(&lenSegName, 1, 1, f);
		fwrite(segName.get(), segName.get_len(), 1, f);
		if (num_bytes_file) {
			fwrite(instructions, 5, 1, f);	// $f2 + 4 bytes data size
			fwrite(s.output, num_bytes_file, 1, f); // segment data
		}
		if (instruction_offs > 5)
			fwrite(instructions + 5, instruction_offs - 5, 1, f); // reloc instructions
	}
	// if there is a size of the direct page & stack, write it
	if (DP_Stack_Size) {
		strref segName("DPStack");
		char lenSegName = (char)segName.get_len();
		_writeNBytes(hdr.SegNum, 2, (int)SegNum.size()+1);
		_writeNBytes(hdr.Kind, 2, 0x12);
		_writeNBytes(hdr.DispDataOffset, 2, sizeof(hdr) + 10 + 1 + (int)segName.get_len());
		_writeNBytes(hdr.Length, 4, DP_Stack_Size);
		_writeNBytes(hdr.ResSpc, 4, DP_Stack_Size);
		_writeNBytes(hdr.Align, 4, 256);
		int segSize = sizeof(hdr) + 10 + 1 + (int)segName.get_len() + 1;
		_writeNBytes(hdr.SegTotal, 4, segSize);
		instructions[0] = 0;
		fwrite(&hdr, sizeof(hdr), 1, f);
		fwrite(segfile, 10, 1, f);
		fwrite(&lenSegName, 1, 1, f);
		fwrite(segName.get(), segName.get_len(), 1, f);
		fwrite(instructions, 1, 1, f); // end instruction
	}
	free(instructions);
	fclose(f);
	return STATUS_OK;
}

int main(int argc, char **argv) {

	int return_value = 0;
	bool load_header = true;
	bool size_header = false;
	bool info = false;
	bool gen_allinstr = false;
	bool gs_os_reloc = false;
	bool force_merge_sections = false;
	bool list_output = false;
	bool tass_list_output = false;
	bool show_refs_before_link = false;
	bool sym_full = false;

	Asm assembler;

	const char *source_filename = nullptr, *obj_out_file = nullptr;
	const char *binary_out_name = nullptr;
	const char *sym_file = nullptr, *vs_file = nullptr, *cmdarg_tass_labels_file = nullptr;
	strref list_file, allinstr_file;
	strref tass_list_file;
	strref srcdebug_file;
	for (int a = 1; a<argc; a++) {
		if (argv[a][0]=='-') {
			strref arg(argv[a]+1);
			if (arg.get_first()=='i') { assembler.AddIncludeFolder(arg+1); }
			else if (arg.same_str(cmdarg_kickasm) ) { assembler.syntax = SYNTAX_KICKASM; }
			else if (arg.same_str(cmdarg_merlin)) { assembler.syntax = SYNTAX_MERLIN; }
			else if (arg.get_first()=='D'||arg.get_first()=='d') {
				++arg;
				if (arg.find('=')>0) {
					assembler.AssignLabel(arg.before('='), arg.after('='));
				} else {
					assembler.AssignLabel(arg, "1");
				}
			} else if (arg.same_str(cmdarg_c64)) {
				load_header = true;
				size_header = false;
			} else if (arg.same_str(cmdarg_a2b)) {
				assembler.default_org = 0x0803;
				load_header = true;
				size_header = true;
			} else if (arg.same_str(cmdarg_bin)) {
				load_header = false;
				size_header = false;
			} else if (arg.same_str(cmdarg_a2p)) {
				assembler.default_org = 0x2000;
				load_header = false;
				size_header = false;
			} else if (arg.same_str(cmdarg_a2o)) {
				gs_os_reloc = true;
			} else if (arg.same_str(cmdarg_mrg)) {
				force_merge_sections = true;
			} else if (arg.same_str(cmdarg_sect)) {
				info = true;
			} else if (arg.same_str(cmdarg_endmacro)) {
				assembler.end_macro_directive = true;
			} else if (arg.same_str(cmdarg_xrefimp)) {
				assembler.import_means_xref = true;
			} else if (arg.same_str(cmdarg_references)) {
				show_refs_before_link = true;
			} else if (arg.has_prefix(cmdarg_listing) && (arg.get_len() == cmdarg_listing.get_len() || arg[cmdarg_listing.get_len()] == '=')) {
				assembler.list_assembly = true;
				list_output = true;
				list_file = arg.after('=');
			} else if (arg.has_prefix(cmdarg_srcdebug) && (arg.get_len() == cmdarg_srcdebug.get_len() || arg[cmdarg_srcdebug.get_len()] == '=')) {
				assembler.src_debug = true;
				srcdebug_file = arg.after('=');
			} else if (arg.has_prefix(cmdarg_tass_listing)&&(arg.get_len()== cmdarg_tass_listing.get_len()||arg[cmdarg_tass_listing.get_len()]=='=')) {
				assembler.list_assembly = true;
				tass_list_output = true;
				tass_list_file = arg.after( '=' );
			} else if (arg.has_prefix(cmdarg_allinstr)&&(arg.get_len()==cmdarg_allinstr.get_len()||arg[cmdarg_allinstr.get_len()]=='=')) {
				gen_allinstr = true;
				allinstr_file = arg.after('=');
			} else if (arg.has_prefix(cmdarg_org)) {
				arg = arg.after('=');
				if (arg && arg.get_first()=='$' && arg.get_len()>1) {
					assembler.default_org = (int)(arg+1).ahextoui();
				} else if (arg.is_number()) { assembler.default_org = (int)arg.atoi(); }
				// force the current section to be org'd
				assembler.AssignAddressToSection(assembler.SectionId(), assembler.default_org);
			} else if (arg.has_prefix(cmdarg_acc)&&arg[cmdarg_acc.get_len()]=='=') {
				assembler.accumulator_16bit = arg.after('=').atoi()==16;
			} else if (arg.has_prefix(cmdarg_xy)&&arg[cmdarg_xy.get_len()]=='=') {
				assembler.index_reg_16bit = arg.after('=').atoi()==16;
			} else if (arg.has_prefix(cmdarg_cpu)&&(arg.get_len()==cmdarg_cpu.get_len()||arg[cmdarg_cpu.get_len()]=='=')) {
				arg.split_token_trim('=');
				bool found = false;
				for (int c = 0; c<nCPUs; c++) {
					if (arg) {
						if (arg.same_str(aCPUs[c].name)) {
							assembler.SetCPU((CPUIndex)c);
							found = true;
							break;
						}
					} else { printf("%s\n", aCPUs[c].name); }
				}
				if (!found && arg) {
					printf("ERROR: UNKNOWN CPU " STRREF_FMT "\n", STRREF_ARG(arg));
					return 1;
				}
				if (!arg) { return 0; }
			} else if (arg.same_str(cmdarg_symfull)&&(a+1)<argc) {
				sym_file = argv[++a];
				assembler.full_map = true;
			} else if (arg.same_str(cmdarg_sym)&&(a+1)<argc) {
				sym_file = argv[++a];
			} else if (arg.same_str(cmdarg_obj)&&(a+1)<argc) {
				obj_out_file = argv[++a];
			} else if (arg.same_str(cmdarg_vice)&&(a+1)<argc) {
				vs_file = argv[++a];
			} else if (arg.same_str(cmdarg_tass_labels)&&(a+1)<argc) {
				cmdarg_tass_labels_file = argv[++a];
			} else { printf("Unexpected option " STRREF_FMT "\n", STRREF_ARG(arg)); }
		} else if (!source_filename) { source_filename = argv[a]; }
		else if (!binary_out_name) { binary_out_name = argv[a]; }
	}
	for (int a = 1; a < argc; a++) {
		strref arg(argv[a]);
		printf(STRREF_FMT "\n", STRREF_ARG(arg));
	}
	if (gen_allinstr) {
		assembler.AllOpcodes(allinstr_file);
	} else if (!source_filename) {
		puts("Usage:\n"
			 " x65 filename.s code.prg [options]\n"
			 "  * -i(path) : Add include path\n"
			 "  * -D(label)[=value] : Define a label with an optional value (otherwise defined as 1)\n"
			 "  * -cpu=6502/65c02/65c02wdc/65816: assemble with opcodes for a different cpu\n"
			 "  * -acc=8/16: set the accumulator mode for 65816 at start, default is 8 bits\n"
			 "  * -xy=8/16: set the index register mode for 65816 at start, default is 8 bits\n"
			 "  * -org = $2000 or - org = 4096: force fixed address code at address\n"
			 "  * -obj (file.x65) : generate object file for later linking\n"
			 "  * -bin : Raw binary\n"
			 "  * -c64 : Include load address(default)\n"
			 "  * -a2b : Apple II Dos 3.3 Binary\n"
			 "  * -a2p : Apple II ProDos Binary\n"
			 "  * -a2o : Apple II GS OS executable (relocatable)\n"
			 "  * -mrg : Force merge all sections (use with -a2o)\n"
			 "  * -sym (file.sym) : symbol file\n"
			 "  * -lst / -lst = (file.lst) : generate disassembly text from result(file or stdout)\n"
			 "  * -opcodes / -opcodes=(file.s) : dump all available opcodes(file or stdout)\n"
			 "  * -srcdbg / -srcdbg=(file.dbg) : generate a source level debugging file for object files or linked files"
			 "  * -sect: display sections loaded and built\n"
			 "  * -vice (file.vs) : export a vice symbol file\n"
			 "  * -merlin: use Merlin syntax\n"
			 "  * -endm : macros end with endm or endmacro instead of scoped('{' - '}')\n");
		return 0;
	}

	// Load source
	if (source_filename) {
		size_t size = 0;
		strref srcname(source_filename);
		assembler.export_base_name =
			strref(binary_out_name).after_last_or_full('/', '\\').before_or_full('.');

		if (char *buffer = assembler.LoadText(srcname, size)) {
			// if source_filename contains a path add that as a search path for include files
			assembler.AddIncludeFolder(srcname.before_last('/', '\\'));
			assembler.Assemble(strref(buffer, strl_t(size)), srcname, obj_out_file != nullptr);
			if (show_refs_before_link) {
				assembler.ShowReferences();
			}
			if (assembler.error_encountered) {
				return_value = 1;
			} else {
				// export object file (this can be done at the same time as building a binary)
				if (obj_out_file) { assembler.WriteObjectFile(obj_out_file); }

				// if exporting binary or relocatable executable, complete the build
				if (binary_out_name && !srcname.same_str(binary_out_name)) {
					if (gs_os_reloc)
						assembler.WriteA2GS_OMF(binary_out_name, force_merge_sections);
					else {
						strref binout(binary_out_name);
						strref ext = binout.after_last('.');
						if (ext) { binout.clip(ext.get_len()+1); }
						strref aAppendNames[MAX_EXPORT_FILES];
						StatusCode err = assembler.LinkZP();	// link zero page sections
						if (err > FIRST_ERROR) {
							assembler.PrintError(strref(), err);
							return_value = 1;
						}
						int numExportFiles = assembler.GetExportNames(aAppendNames, MAX_EXPORT_FILES);
						for (int e = 0; e < numExportFiles; e++) {
							strown<512> file(binout);
							file.append(aAppendNames[e]);
							file.append('.');
							file.append(ext);
							int size_export;
							int addr;
							if (uint8_t *buf = assembler.BuildExport(aAppendNames[e], size_export, addr)) {
								if (FILE *f = fopen(file.c_str(), "wb")) {
									if (load_header) {
										uint8_t load_addr[2] = { (uint8_t)addr, (uint8_t)(addr >> 8) };
										fwrite(load_addr, 2, 1, f);
									}
									if (size_header) {
										uint8_t byte_size[2] = { (uint8_t)size_export, (uint8_t)(size_export >> 8) };
										fwrite(byte_size, 2, 1, f);
									}
									fwrite(buf, size_export, 1, f);
									fclose(f);
								}
								free(buf);
							}
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
								for (relocList::iterator rel = s.pRelocs->begin(); rel != s.pRelocs->end(); ++rel)
									printf("\tReloc value $%x at offs $%x section %d\n", rel->base_value, rel->section_offset, rel->target_section);
							}
						}
					}
				}

				// listing after export since addresses are now resolved
				if (list_output) { assembler.List(list_file); }

				if (srcdebug_file) { assembler.SourceDebugExport(srcdebug_file); }

				if (tass_list_output) { assembler.ListTassStyle(tass_list_file); }

				// export .sym file
				if (sym_file && !srcname.same_str(sym_file) && !assembler.map.empty()) {
					if (FILE *f = fopen(sym_file, "w")) {
						bool wasLocal = false;
						for (MapSymbolArray::iterator i = assembler.map.begin(); i!=assembler.map.end(); ++i) {
							uint32_t value = (uint32_t)i->value;
							if (size_t(i->section) < assembler.allSections.size()) {
								value += assembler.allSections[i->section].start_address;
							}
							if (!i->borrowed || assembler.full_map) {
								fprintf(f, "%s.label " STRREF_FMT " = $%04x", wasLocal == i->local ? "\n" :
									(i->local ? " {\n" : "\n}\n"), STRREF_ARG(i->name), value);
								wasLocal = i->local;
							}
						}
						fputs(wasLocal ? "\n}\n" : "\n", f);
						fclose(f);
					}
				}

				// export vice monitor commands
				if (vs_file && !srcname.same_str(vs_file) && !assembler.map.empty()) {
					if (FILE *f = fopen(vs_file, "w")) {
						for (MapSymbolArray::iterator i = assembler.map.begin(); i!=assembler.map.end(); ++i) {
							uint32_t value = (uint32_t)i->value;
							if (size_t(i->section) < assembler.allSections.size()) { value += assembler.allSections[i->section].start_address; }
							if(i->name.same_str("debugbreak")) {
								fprintf(f, "break $%04x\n", value);
							} else {
								fprintf(f, "al $%04x %s" STRREF_FMT "\n", value, i->name[0]=='.' ? "" : ".",
										STRREF_ARG(i->name));
							}
						}
						fclose(f);
					}
				}
				// export tass labels
				if( cmdarg_tass_labels_file && !srcname.same_str( cmdarg_tass_labels_file ) && !assembler.map.empty() ) {
					if( FILE *f = fopen( cmdarg_tass_labels_file, "w" ) ) {
						for( MapSymbolArray::iterator i = assembler.map.begin(); i != assembler.map.end(); ++i ) {
							uint32_t value = ( uint32_t )i->value;
							if( size_t( i->section ) < assembler.allSections.size() ) { value += assembler.allSections[ i->section ].start_address; }
							if( i->name.same_str( "debugbreak" ) ) {}
							else {
								strown<256> line;
								line.append( i->name );
								line.sprintf_append( "\t= $%04x\n", value);
								fputs(line.c_str(), f);
							}
						}
						fclose( f );
					}
				}
			}
			// free some memory
			assembler.Cleanup();
		}
	}
	return return_value;
}

