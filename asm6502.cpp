//
//  asm6502.cpp
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
// Details, source and documentation at https://github.com/Sakrac/Asm6502.
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
	ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH,
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
	"Invalid addressing mode for branch instruction",
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
};

// Assembler directives
enum AssemblerDirective {
	AD_ORG,			// ORG: Assemble as if loaded at this address
	AD_LOAD,		// LOAD: If applicable, instruct to load at this address
	AD_SECTION,		// SECTION: Enable code that will be assigned a start address during a link step
	AD_LINK,		// LINK: Put sections with this name at this address (must be ORG / fixed address section)
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

// Opcode encoding
typedef struct {
	unsigned int op_hash;
	unsigned char group;	// group #
	unsigned char index;	// ground index
	unsigned char type;		// mnemonic or
} OP_ID;

//
// 6502 instruction encoding according to this page
// http://www.llx.com/~nparker/a2/opcodes.html
// decoded instruction:
// XXY10000 for branches
// AAABBBCC for CC=00, 01, 10
// and some custom ops
//

enum AddressingMode {
	AM_REL_ZP_X,		// 0 (zp,x)
	AM_ZP,				// 1 zp
	AM_IMMEDIATE,		// 2 #$hh
	AM_ABSOLUTE,		// 3 $hhhh
	AM_REL_ZP_Y,		// 4 (zp),y
	AM_ZP_X,			// 5 zp,x
	AM_ABSOLUTE_Y,		// 6 $hhhh,y
	AM_ABSOLUTE_X,		// 7 $hhhh,x
	AM_RELATIVE,		// 8 ($xxxx)
	AM_ACCUMULATOR,		// 9 A
	AM_NONE,			// 10 <empty>
	AM_INVALID,			// 11
};

enum EvalOperator;

// How instruction argument is encoded
enum CODE_ARG {
	CA_NONE,			// single byte instruction
	CA_ONE_BYTE,		// instruction carries one byte
	CA_TWO_BYTES,		// instruction carries two bytes
	CA_BRANCH			// instruction carries a relative address
};

// opcode groups
enum OP_GROUP {
	OPG_SUBROUT,
	OPG_CC01,
	OPG_CC10,
	OPG_STACK,
	OPG_BRANCH,
	OPG_FLAG,
	OPG_CC00,
	OPG_TRANS
};

// opcode exception indices
enum OP_INDICES {
	OPI_JSR = 1,
	OPI_LDX = 5,
	OPI_STX = 4,
	OPI_STA = 4,
	OPI_JMP = 1,
};

#define RELATIVE_JMP_DELTA 0x20

// opcode names in groups (prefix by group size)
const char aInstr[] = {
	"BRK,JSR,RTI,RTS\n"
	"ORA,AND,EOR,ADC,STA,LDA,CMP,SBC\n"
	"ASL,ROL,LSR,ROR,STX,LDX,DEC,INC\n"
	"PHP,PLP,PHA,PLA,DEY,TAY,INY,INX\n"
	"BPL,BMI,BVC,BVS,BCC,BCS,BNE,BEQ\n"
	"CLC,SEC,CLI,SEI,TYA,CLV,CLD,SED\n"
	"BIT,JMP,,STY,LDY,CPY,CPX\n"
	"TXA,TXS,TAX,TSX,DEX,,NOP"
};

// group # + index => base opcode
const unsigned char aMulAddGroup[][2] = {
	{ 0x20,0x00 },
	{ 0x20,0x01 },
	{ 0x20,0x02 },
	{ 0x20,0x08 },
	{ 0x20,0x10 },
	{ 0x20,0x18 },
	{ 0x20,0x20 },
	{ 0x10,0x8a }
};

char aCC00Modes[] = { AM_IMMEDIATE, AM_ZP, AM_INVALID, AM_ABSOLUTE, AM_INVALID, AM_ZP_X, AM_INVALID, AM_ABSOLUTE_X };
char aCC01Modes[] = { AM_REL_ZP_X, AM_ZP, AM_IMMEDIATE, AM_ABSOLUTE, AM_REL_ZP_Y, AM_ZP_X, AM_ABSOLUTE_X, AM_ABSOLUTE_Y };
char aCC10Modes[] = { AM_IMMEDIATE, AM_ZP, AM_NONE, AM_ABSOLUTE, AM_INVALID, AM_ZP_X, AM_INVALID, AM_ABSOLUTE_X };

unsigned char CC00ModeAdd[] = { 0xff, 4, 0, 12, 0xff, 20, 0xff, 28 };
unsigned char CC00Mask[] = { 0x0a, 0x08, 0x08, 0x2a, 0xae, 0x0e, 0x0e };
unsigned char CC10ModeAdd[] = { 0xff, 4, 0, 12, 0xff, 20, 0xff, 28 };
unsigned char CC10Mask[] = { 0xaa, 0xaa, 0xaa, 0xaa, 0x2a, 0xae, 0xaa, 0xaa };

// hardtexted strings
static const strref c_comment("//");
static const strref word_char_range("!0-9a-zA-Z_@$!#");
static const strref label_end_char_range("!0-9a-zA-Z_@$!.:");
static const strref label_end_char_range_merlin("!0-9a-zA-Z_@$!.]:?");
static const strref filename_end_char_range("!0-9a-zA-Z_!@#$%&()\\-");
static const strref keyword_equ("equ");
static const strref str_label("label");
static const strref str_const("const");
static const strref struct_byte("byte");
static const strref struct_word("word");

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
		if (insert(pos)) {
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
	~pairArray() { clear(); }
};

// relocs are cheaper than full expressions and work with
// local labels for relative sections which would otherwise
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

// start of data section support
// Default is a fixed address section at $1000
// Whenever org or dum with address is encountered => new section
// If org is fixed and < $200 then it is a dummy section Otherwise clear dummy section
typedef struct Section {
	// section name, same named section => append
	strref name;			// name of section for comparison

	// generated address status
	int load_address;		// if assigned a load address
	int start_address;
	int address;			// relative or absolute PC
	bool address_assigned;	// address is absolute if assigned

	// merged sections
	int merged_offset;		// -1 if not merged
	int merged_section;		// which section merged with

	// data output
	unsigned char *output;	// memory for this section
	unsigned char *curr;	// current pointer for this section
	size_t output_capacity;	// current output capacity

	// reloc data
	relocList *pRelocs;		// link time resolve (not all sections need this)
	bool dummySection;		// true if section does not generate data, only labels

	void reset() {
		name.clear(); start_address = address = load_address = 0x0;
		address_assigned = false; output = nullptr; curr = nullptr;
		dummySection = false;  output_capacity = 0;
		merged_offset = -1;
		if (pRelocs) delete pRelocs; pRelocs = nullptr;
	}

	void Cleanup() { if (output) free(output); reset(); }
	bool empty() const { return merged_offset<0 && curr==output; }

	size_t DataOffset() const { return curr - output; }
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

	Section() : pRelocs(nullptr) { reset(); }
	Section(strref _name, int _address) : pRelocs(nullptr) { reset(); name = _name;
		start_address = load_address = address = _address; address_assigned = true; }
	Section(strref _name) : pRelocs(nullptr) { reset(); name = _name;
		start_address = load_address = address = 0; address_assigned = false; }
	~Section() { reset(); }

	// Appending data to a section
	void CheckOutputCapacity(unsigned int addSize);
	void AddByte(int b);
	void AddWord(int w);
	void AddBin(unsigned const char *p, int size);
	void SetByte(size_t offs, int b) { output[offs] = b; }
	void SetWord(size_t offs, int w) { output[offs] = w; output[offs+1] = w>>8; }
} Section;

// Data related to a label
typedef struct {
public:
	strref label_name;		// the name of this label
	strref expression;		// the expression of this label (optional, if not possible to evaluate yet)
	int value;
	int section;			// rel section address labels belong to a section, -1 if fixed address or assigned
	bool evaluated;			// a value may not yet be evaluated
	bool zero_page;			// addresses known to be zero page
	bool pc_relative;		// this is an inline label describing a point in the code
	bool constant;			// the value of this label can not change
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
	size_t target;	// offset into output buffer
	int address;			// current pc
	int scope;				// scope pc
	int section;			// which section to apply to.
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

	std::vector<LateEval> lateEval;
	std::vector<LocalLabelRecord> localLabels;
	std::vector<char*> loadedData;	// free when assembler is completed
	std::vector<MemberOffset> structMembers; // labelStructs refer to sets of structMembers
	std::vector<strref> includePaths;
	std::vector<Section> allSections;
	strovl symbols; // for building a symbol output file
	
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
	char conditional_nesting[MAX_CONDITIONAL_DEPTH];
	bool conditional_consumed[MAX_CONDITIONAL_DEPTH];

	// Scope info
	int scope_address[MAX_SCOPE_DEPTH];
	int scope_depth;

	// Eval result
	int lastEvalSection;

	bool symbol_export, last_label_local;
	bool errorEncountered;

	// Convert source to binary
	void Assemble(strref source, strref filename);

	// Clean up memory allocations, reset assembler state
	void Cleanup();
	
	// Make sure there is room to write more code
	void CheckOutputCapacity(unsigned int addSize);

	// Operations on current section
	void SetSection(strref name, int address);	// fixed address section
	void SetSection(strref name); // relative address section
	StatusCode LinkSections(strref name); // link relative address sections with this name here
	void DummySection(int address);
	void DummySection();
	void EndSection();
	Section& CurrSection() { return *current_section; }
	int SectionId() { return int(current_section - &allSections[0]); }
	void AddByte(int b) { CurrSection().AddByte(b); }
	void AddWord(int w) { CurrSection().AddWord(w); }
	void AddBin(unsigned const char *p, int size) { CurrSection().AddBin(p, size); }

	// Macro management
	StatusCode AddMacro(strref macro, strref source_name, strref source_file, strref &left);
	StatusCode BuildMacro(Macro &m, strref arg_list);

	// Structs
	StatusCode BuildStruct(strref name, strref declaration);
	StatusCode EvalStruct(strref name, int &value);
	StatusCode BuildEnum(strref name, strref declaration);

	// Calculate a value based on an expression.
	EvalOperator RPNToken_Merlin(strref &expression, int pc, int scope_pc,
		int scope_end_pc, EvalOperator prev_op, short &section, int &value);
	EvalOperator RPNToken(strref &expression, int pc, int scope_pc,
		int scope_end_pc, EvalOperator prev_op, short &section, int &value);
	StatusCode EvalExpression(strref expression, int pc, int scope_pc,
							  int scope_end_pc, int relative_section, int &result);

	// Access labels
	Label* GetLabel(strref label);
	Label* AddLabel(unsigned int hash);
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
	void AddLateEval(int pc, int scope_pc, strref expression,
					 strref source_file, LateEval::Type type);
	void AddLateEval(strref label, int pc, int scope_pc,
					 strref expression, LateEval::Type type);
	StatusCode CheckLateEval(strref added_label=strref(), int scope_end = -1);

	// Assembler steps
	StatusCode ApplyDirective(AssemblerDirective dir, strref line, strref source_file);
	AddressingMode GetAddressMode(strref line, bool flipXY,
								  StatusCode &error, strref &expression);
	StatusCode AddOpcode(strref line, int group, int index, strref source_file);
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
	for (std::vector<char*>::iterator i = loadedData.begin(); i!=loadedData.end(); ++i) {
		char *data = *i;
		free(data);
	}
	if (symbols.get()) {
		free(symbols.charstr());
		symbols.set_overlay(nullptr,0);
	}
	labelPools.clear();
	loadedData.clear();
	labels.clear();
	macros.clear();
	allSections.clear();
	SetSection(strref("default"), 0x1000);
	current_section = &allSections[0];
	syntax = SYNTAX_SANE;
	scope_depth = 0;
	conditional_depth = 0;
	conditional_nesting[0] = 0;
	conditional_consumed[0] = false;
	symbol_export = false;
	last_label_local = false;
	errorEncountered = false;
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
	Section newSection(name);
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

StatusCode Asm::LinkSections(strref name) {
	if (CurrSection().IsRelativeSection())
		return ERROR_LINKER_MUST_BE_IN_FIXED_ADDRESS_SECTION;
	if (CurrSection().IsDummySection())
		return ERROR_LINKER_CANT_LINK_TO_DUMMY_SECTION;

	int section_id = 0;
	for (std::vector<Section>::iterator i = allSections.begin(); i != allSections.end(); ++i) {
		if (i->name.same_str_case(name) && i->IsRelativeSection() && !i->IsMergedSection()) {
			// found a section to link!
			Section &s = *i;
			// Get base addresses
			CheckOutputCapacity((int)s.size());

			int section_address = CurrSection().GetPC();
			unsigned char *section_out = CurrSection().curr;
			memcpy(section_out, s.output, s.size());
			CurrSection().address += (int)s.size();
			CurrSection().curr += s.size();
			free(s.output);
			s.output = 0;
			s.curr = 0;
			s.output_capacity = 0;

			// Update address range and mark section as merged
			s.start_address = section_address;
			s.address += section_address;
			s.address_assigned = true;
			s.merged_section = SectionId();
			s.merged_offset = (int)(section_out - CurrSection().output);

			// All labels in this section can now be assigned
			Label *pLabels = labels.getValues();
			int numLabels = labels.count();
			for (int l = 0; l < numLabels; l++) {
				if (pLabels->section == section_id) {
					pLabels->value += section_address;
					pLabels->section = -1;
					CheckLateEval(pLabels->label_name);
				}
				++pLabels;
			}

			// go through relocs in all sections to see if any targets this section
			// relocate section to address!
			for (std::vector<Section>::iterator j = allSections.begin(); j != allSections.end(); ++j) {
				Section &s2 = *j;
				if (s2.pRelocs) {
					relocList *pList = s2.pRelocs;
					relocList::iterator i = pList->end();
					while (i != pList->begin()) {
						--i;
						if (i->target_section == section_id) {
							unsigned char *trg = s2.merged_offset < 0 ? s2.output :
								(allSections[s2.merged_section].output + s2.merged_offset);
							trg += i->section_offset;
							int value = i->base_value + section_address;
							if (i->value_type == Reloc::WORD) {
								*trg++ = (unsigned char)value;
								*trg = (unsigned char)(value >> 8);
							} else if (i->value_type == Reloc::LO_BYTE)
								*trg = (unsigned char)value;
							else if (i->value_type == Reloc::HI_BYTE)
								*trg = (unsigned char)(value >> 8);
							i = pList->erase(i);
							if (i != pList->end())
								++i;
						}
					}
				}
			}
		}
		++section_id;
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

void Section::AddByte(int b) {
	if (!dummySection) {
		CheckOutputCapacity(1);
		*curr++ = (unsigned char)b;
	}
	address++;
}

void Section::AddWord(int w) {
	if (!dummySection) {
		CheckOutputCapacity(2);
		*curr++ = (unsigned char)(w&0xff);
		*curr++ = (unsigned char)(w>>8);
	}
	address += 2;
}

void Section::AddBin(unsigned const char *p, int size) {
	if (!dummySection) {
		CheckOutputCapacity(size);
		memcpy(curr, p, size);
		curr += size;
	}
	address += size;
}

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

	while (strref line = declaration.line()) {
		line = line.before_or_full(',');
		line.trim_whitespace();
		strref name = line.split_token_trim('=');
		if (line) {
			StatusCode error = EvalExpression(line, CurrSection().GetPC(),
											  scope_address[scope_depth], -1, -1, value);
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


// Minimal value lookup in short array
static int _oneOf(short n, short *opt, short numOpt) {
	for (int s = 0; s<numOpt; s++) {
		if (opt[s] == n)
			return s;
	}
	return -1;
}

// These are expression tokens in order of precedence (last is highest precedence)
enum EvalOperator {
	EVOP_NONE,
	EVOP_VAL,	// value => read from value queue
	EVOP_LPR,	// left parenthesis
	EVOP_RPR,	// right parenthesis
	EVOP_ADD,	// +
	EVOP_SUB,	// -
	EVOP_MUL,	// * (note: if not preceded by value or right paren this is current PC)
	EVOP_DIV,	// /
	EVOP_AND,	// &
	EVOP_OR,	// |
	EVOP_EOR,	// ^
	EVOP_SHL,	// <<
	EVOP_SHR,	// >>
	EVOP_LOB,	// low byte of 16 bit value
	EVOP_HIB,	// high byte of 16 bit value
	EVOP_STP,	// Unexpected input, should stop and evaluate what we have
	EVOP_ERR,	// Error
	EVOP_NRY,	// Not ready yet
};

// Get a single token from a merlin expression
EvalOperator Asm::RPNToken_Merlin(strref &expression, int pc, int scope_pc, int scope_end_pc, EvalOperator prev_op, short &section, int &value)
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
			value = pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '/': ++expression; return EVOP_DIV;
		case '>': if (expression.get_len() >= 2 && expression[1] == '>') { expression += 2; return EVOP_SHR; }
			++expression; return EVOP_HIB;
		case '<': if (expression.get_len() >= 2 && expression[1] == '<') { expression += 2; return EVOP_SHL; }
			++expression; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (expression[1]=='0' || expression[1]=='1') {
				++expression; value = expression.abinarytoui_skip(); return EVOP_VAL; }
			if (scope_end_pc<0) return EVOP_NRY;
			++expression; value = scope_end_pc; return EVOP_VAL;
		case '|':
		case '.': ++expression; return EVOP_OR;	// MERLIN: . is or, | is not used
		case '^': ++expression; return EVOP_EOR;
		case '&': ++expression; return EVOP_AND;
		case '(': if (prev_op!=EVOP_VAL) { ++expression; return EVOP_LPR; } return EVOP_STP;
		case ')': ++expression; return EVOP_RPR;
		case ',':
		case '?':
		case '\'': return EVOP_STP;
		default: {	// MERLIN: ! is eor
			if (c == '!' && (prev_op == EVOP_VAL || prev_op == EVOP_RPR)) { ++expression; return EVOP_EOR; }
			else if (c == '!' && !(expression + 1).len_label()) {
				if (scope_pc < 0) return EVOP_NRY;	// ! by itself is current scope, !+label char is a local label
				++expression; value = scope_pc;  return EVOP_VAL;
			} else if (strref::is_number(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP;	// value followed by value doesn't make sense, stop
				value = expression.atoi_skip(); return EVOP_VAL;
			} else if (c == '!' || c == ']' || c==':' || strref::is_valid_label(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
				char e0 = expression[0];
				int start_pos = (e0==']' || e0==':' || e0=='!' || e0=='.') ? 1 : 0;
				strref label = expression.split_range_trim(label_end_char_range_merlin, start_pos);
				Label *pLabel = GetLabel(label);
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
EvalOperator Asm::RPNToken(strref &expression, int pc, int scope_pc, int scope_end_pc, EvalOperator prev_op, short &section, int &value)
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
			value = pc; section = CurrSection().IsRelativeSection() ? SectionId() : -1; return EVOP_VAL;
		case '/': ++expression; return EVOP_DIV;
		case '>': if (expression.get_len() >= 2 && expression[1] == '>') { expression += 2; return EVOP_SHR; }
				  ++expression; return EVOP_HIB;
		case '<': if (expression.get_len() >= 2 && expression[1] == '<') { expression += 2; return EVOP_SHL; }
				  ++expression; return EVOP_LOB;
		case '%': // % means both binary and scope closure, disambiguate!
			if (expression[1] == '0' || expression[1] == '1') { ++expression; value = expression.abinarytoui_skip(); return EVOP_VAL; }
			if (scope_end_pc<0) return EVOP_NRY;
			++expression; value = scope_end_pc; return EVOP_VAL;
		case '|': ++expression; return EVOP_OR;
		case '^': ++expression; return EVOP_EOR;
		case '&': ++expression; return EVOP_AND;
		case '(': if (prev_op != EVOP_VAL) { ++expression; return EVOP_LPR; } return EVOP_STP;
		case ')': ++expression; return EVOP_RPR;
		case '.': return EVOP_ERR; // Merlin uses . instead of | for or, but . is allowed for labels in other assemblers
		case ',':
		case '?':
		case '\'': return EVOP_STP;
		default: {	// ! by itself is current scope, !+label char is a local label
			if (c == '!' && !(expression + 1).len_label()) {
				if (scope_pc < 0) return EVOP_NRY;
				++expression; value = scope_pc; return EVOP_VAL;
			} else if (strref::is_number(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP;	// value followed by value doesn't make sense, stop
				value = expression.atoi_skip(); return EVOP_VAL;
			} else if (c == '!' || c == ':' || strref::is_valid_label(c)) {
				if (prev_op == EVOP_VAL) return EVOP_STP; // a value followed by a value does not make sense, probably start of a comment (ORCA/LISA?)
				char e0 = expression[0];
				int start_pos = (e0 == ':' || e0 == '!' || e0 == '.') ? 1 : 0;
				strref label = expression.split_range_trim(label_end_char_range, start_pos);
				Label *pLabel = GetLabel(label);
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

StatusCode Asm::EvalExpression(strref expression, int pc, int scope_pc, int scope_end_pc, int relative_section, int &result)
{
	int numValues = 0;
	int numOps = 0;

	char ops[MAX_EVAL_OPER];		// RPN expression
	int values[MAX_EVAL_VALUES];	// RPN values (in order of RPN EVOP_VAL operations)
	short section_ids[MAX_EVAL_SECTIONS];	// local index of each referenced section
	short section_val[MAX_EVAL_VALUES];		// each value can be assigned to one section, or -1 if fixed
	short num_sections = 0;			// number of sections in section_ids (normally 0 or 1, can be up to MAX_EVAL_SECTIONS)
	values[0] = 0;					// Initialize RPN if no expression
	StatusCode status = STATUS_OK;
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
				op = RPNToken_Merlin(expression, pc, scope_pc, scope_end_pc, prev_op, section, value);
			else
				op = RPNToken(expression, pc, scope_pc, scope_end_pc, prev_op, section, value);
			if (op == EVOP_ERR)
				return ERROR_UNEXPECTED_CHARACTER_IN_EXPRESSION;
			else if (op == EVOP_NRY)
				return STATUS_NOT_READY;
			if (section >= 0) {
				if ((index_section = _oneOf(section, section_ids, num_sections)) < 0) {
					if (num_sections == MAX_EVAL_SECTIONS) return STATUS_NOT_READY;
					section_ids[index_section = num_sections++] = section;
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
		short section_counts[MAX_EVAL_SECTIONS][MAX_EVAL_VALUES];
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
					values[ri-1] &= 0xff; break;
				case EVOP_HIB:
					values[ri-1] = (values[ri-1]>>8)&0xff; break;
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
				else if (relative_section==section_ids[i])
					curr_relative = true;
				else if (relative_section>=0)
					return STATUS_NOT_READY;
				section_index = i;
			}
		}
		result = values[0];
		if (section_index>=0 && !curr_relative) {
			lastEvalSection = section_ids[section_index];
			return STATUS_RELATIVE_SECTION;
		}
	}
			
	return STATUS_OK;
}


// if an expression could not be evaluated, add it along with
// the action to perform if it can be evaluated later.
void Asm::AddLateEval(int pc, int scope_pc, strref expression, strref source_file, LateEval::Type type)
{
	LateEval le;
	le.address = pc;
	le.scope = scope_pc;
	le.target = CurrSection().DataOffset();
	le.section = (int)(&CurrSection() - &allSections[0]);
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
	le.expression = expression;
	le.source_file.clear();
	le.type = type;
	
	lateEval.push_back(le);
}

// When a label is defined or a scope ends check if there are
// any related late label evaluators that can now be evaluated.
StatusCode Asm::CheckLateEval(strref added_label, int scope_end)
{
	std::vector<LateEval>::iterator i = lateEval.begin();
	bool evaluated_label = true;
	strref new_labels[MAX_LABELS_EVAL_ALL];
	int num_new_labels = 0;
	if (added_label)
		new_labels[num_new_labels++] = added_label;
	
	while (evaluated_label) {
		evaluated_label = false;
		while (i != lateEval.end()) {
			int value = 0;
			// check if this expression is related to the late change (new label or end of scope)
			bool check = num_new_labels==MAX_LABELS_EVAL_ALL;
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
				int ret = EvalExpression(i->expression, i->address, i->scope, scope_end,
										 i->type==LateEval::LET_BRANCH ? SectionId() : -1, value);
				if (ret == STATUS_OK) {
					// Check if target section merged with another section
					size_t trg = i->target;
					int sec = i->section;
					if (i->type != LateEval::LET_LABEL) {
						if (allSections[sec].IsMergedSection()) {
							trg += allSections[sec].merged_offset;
							sec = allSections[sec].merged_section;
						}
					}
					switch (i->type) {
						case LateEval::LET_BRANCH:
							value -= i->address+1;
							if (value<-128 || value>127)
								return ERROR_BRANCH_OUT_OF_RANGE;
							allSections[sec].SetByte(trg, value);
							break;
						case LateEval::LET_BYTE:
							allSections[sec].SetByte(trg, value);
							break;
						case LateEval::LET_ABS_REF:

							allSections[sec].SetWord(trg, value);
							break;
						case LateEval::LET_LABEL: {
							Label *label = GetLabel(i->label);
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
					i = lateEval.erase(i);
				} else
					++i;
			} else
				++i;
		}
		added_label.clear();
	}
	return STATUS_OK;
}



//
//
// LABELS
//
//



// Get a labelc record if it exists
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

// If exporting labels, append this label to the list
void Asm::LabelAdded(Label *pLabel, bool local)
{
	if (pLabel && pLabel->evaluated && symbol_export) {
		int space = 1 + str_label.get_len() + 1 + pLabel->label_name.get_len() + 1 + 9 + 2;
		if ((symbols.get_len()+space) > symbols.cap()) {
			strl_t new_size = ((symbols.get_len()+space)+8*1024);
			if (char *new_charstr = (char*)malloc(new_size)) {
				if (symbols.charstr()) {
					memcpy(new_charstr, symbols.charstr(), symbols.get_len());
					free(symbols.charstr());
				}
				symbols.set_overlay(new_charstr, new_size, symbols.get_len());
			}
		}
		if (local && !last_label_local)
			symbols.append("{\n");
		else if (!local && last_label_local)
			symbols.append("}\n");
		symbols.append(local ? " ." : ".");
		symbols.append(pLabel->constant ? str_const : str_label);
		symbols.append(' ');
		symbols.append(pLabel->label_name);
		symbols.sprintf_append("=$%04x\n", pLabel->value);
		last_label_local = local;
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
						if (LabelPool *pool = GetLabelPool(labels.getValue(index).expression)) {
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
	while (strref arg = args.split_token_trim(',')) {
		strref start = arg[0]=='(' ? arg.scoped_block_skip() : arg.split_token_trim('-');
		int addr0 = 0, addr1 = 0;
		if (STATUS_OK != EvalExpression(start, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, addr0))
			return ERROR_POOL_RANGE_EXPRESSION_EVAL;
		if (STATUS_OK != EvalExpression(arg, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, addr1))
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
	pLabel->expression = pool.pool_name;
	pLabel->evaluated = true;
	pLabel->section = -1;	// pool labels are section-less
	pLabel->value = addr;
	pLabel->zero_page = addr<0x100;
	pLabel->pc_relative = true;
	pLabel->constant = true;

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

// assignment of label (<label> = <expression>)
StatusCode Asm::AssignLabel(strref label, strref line, bool make_constant)
{
	line.trim_whitespace();
	int val = 0;
	StatusCode status = EvalExpression(line, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, val);
	if (status != STATUS_NOT_READY && status != STATUS_OK)
		return status;
	
	Label *pLabel = GetLabel(label);
	if (pLabel) {
		if (pLabel->constant && pLabel->evaluated && val != pLabel->value)
			return (status == STATUS_NOT_READY) ? STATUS_OK : ERROR_MODIFYING_CONST_LABEL;
	} else
		pLabel = AddLabel(label.fnv1a());
	
	pLabel->label_name = label;
	pLabel->expression = line;
	pLabel->evaluated = status==STATUS_OK;
	pLabel->section = -1;	// assigned labels are section-less
	pLabel->value = val;
	pLabel->zero_page = pLabel->evaluated && val<0x100;
	pLabel->pc_relative = false;
	pLabel->constant = make_constant;
	
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
	pLabel->expression.clear();
	pLabel->section = CurrSection().IsRelativeSection() ? SectionId() : -1;	// address labels are based on section
	pLabel->value = CurrSection().GetPC();
	pLabel->evaluated = true;
	pLabel->pc_relative = true;
	pLabel->constant = constLabel;
	pLabel->zero_page = false;
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
		conditional_consumed[conditional_depth] = false;
		conditional_nesting[conditional_depth] = 0;
	}
}

// This conditional block is going to be assembled, mark it as consumed
void Asm::ConsumeConditional()
{
	conditional_consumed[conditional_depth] = true;
}

// This conditional block is not going to be assembled so mark that it is nesting
void Asm::SetConditional()
{
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
	if (equ >=0) {
		// (EXP) == (EXP)
		strref left = line.get_clipped(equ);
		bool equal = left.get_last()!='!';
		left.trim_whitespace();
		strref right = line + equ + 1;
		if (right.get_first()=='=')
			++right;
		right.trim_whitespace();
		int value_left, value_right;
		if (STATUS_OK != EvalExpression(left, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value_left))
			return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
		if (STATUS_OK != EvalExpression(right, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value_right))
			return ERROR_CONDITION_COULD_NOT_BE_RESOLVED;
		result = (value_left==value_right && equal) || (value_left!=value_right && !equal);
	} else {
		bool invert = line.get_first()=='!';
		if (invert)
			++line;
		int value;
		if (STATUS_OK != EvalExpression(line, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value))
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
	{ "PC", AD_ORG },
	{ "ORG", AD_ORG },
	{ "LOAD", AD_LOAD },
	{ "SECTION", AD_SECTION },
	{ "LINK", AD_LINK },
	{ "ALIGN", AD_ALIGN },
	{ "MACRO", AD_MACRO },
	{ "EVAL", AD_EVAL },
	{ "PRINT", AD_EVAL },
	{ "BYTE", AD_BYTES },
	{ "BYTES", AD_BYTES },
	{ "DB", AD_BYTES },
	{ "DFB", AD_BYTES },
	{ "WORD", AD_WORDS },
	{ "DA", AD_WORDS },
	{ "DW", AD_WORDS },
	{ "DDB", AD_WORDS },
	{ "WORDS", AD_WORDS },
	{ "DC", AD_DC },
	{ "TEXT", AD_TEXT },
	{ "ASC", AD_TEXT },
	{ "INCLUDE", AD_INCLUDE },
	{ "PUT", AD_INCLUDE },
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
	switch (dir) {
		case AD_ORG: {		// org / pc: current address of code
			int addr;
			if (line[0]=='=')
				++line;
			else if(keyword_equ.is_prefix_word(line))	// optional '=' or equ
				line.next_word_ws();
			line.skip_whitespace();
			if ((error = EvalExpression(line, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, addr))) {
				error = error == STATUS_NOT_READY ? ERROR_TARGET_ADDRESS_MUST_EVALUATE_IMMEDIATELY : error;
				break;
			}
			SetSection(strref(), addr);
			break;
		}
		case AD_LOAD: {		// load: address for target to load code at
			int addr;
			if (line[0]=='=' || keyword_equ.is_prefix_word(line))
				line.next_word_ws();
			if ((error = EvalExpression(line, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, addr))) {
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

		case AD_ALIGN:		// align: align address to multiple of value, fill space with 0
			if (line) {
				if (line[0]=='=' || keyword_equ.is_prefix_word(line))
					line.next_word_ws();
				int value;
				int status = EvalExpression(line, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value);
				if (status == STATUS_NOT_READY)
					error = ERROR_ALIGN_MUST_EVALUATE_IMMEDIATELY;
				else if (status == STATUS_OK && value>0) {
					int add = (CurrSection().GetPC() + value-1) % value;
					for (int a = 0; a < add; a++)
						AddByte(0);
				}
			}
			break;
		case AD_EVAL: {		// eval: display the result of an expression in stdout
			int value = 0;
			strref description = line.find(':')>=0 ? line.split_token_trim(':') : strref();
			line.trim_whitespace();
			if (line && EvalExpression(line, CurrSection().GetPC(),
				scope_address[scope_depth], -1, -1, value) == STATUS_OK) {
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
			while (strref exp = line.split_token_trim(',')) {
				int value;
				error = EvalExpression(exp, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value);
				if (error>STATUS_NOT_READY)
					break;
				else if (error==STATUS_NOT_READY)
					AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], exp, source_file, LateEval::LET_BYTE);
				else
					AddByte(value);
			}
			break;
		case AD_WORDS:		// words: add words (16 bit values) by comma separated values
			while (strref exp = line.split_token_trim(',')) {
				int value = 0;
					if (!CurrSection().IsDummySection()) {
						error = EvalExpression(exp, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value);
					if (error>STATUS_NOT_READY)
						break;
					else if (error==STATUS_NOT_READY)
						AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], exp, source_file, LateEval::LET_ABS_REF);
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
			while (strref exp = line.split_token_trim(',')) {
				int value = 0;
				if (!CurrSection().IsDummySection()) {
					error = EvalExpression(exp, CurrSection().GetPC(), scope_address[scope_depth], -1, -1, value);
					if (error > STATUS_NOT_READY)
						break;
					else if (error == STATUS_NOT_READY)
						AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], exp, source_file, LateEval::LET_BYTE);
					else if (error == STATUS_RELATIVE_SECTION)
						CurrSection().AddReloc(value, CurrSection().GetPC(), lastEvalSection, Reloc::WORD);
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
				if (STATUS_OK != EvalExpression(expression, CurrSection().GetPC(),
					scope_address[scope_depth], -1, -1, count)) {
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
//			dummySection = true;
			line.trim_whitespace();
			if (line) {
				int reorg;
				if (STATUS_OK == EvalExpression(line, CurrSection().GetPC(),
					scope_address[scope_depth], -1, -1, reorg)) {
					DummySection(reorg);
					break;
				}
			}
			DummySection();
			break;
		case AD_DUMMY_END:
			if (CurrSection().IsDummySection())
				EndSection();
//			dummySection = false;
			break;
		case AD_DS: {
			int value;
			if (STATUS_OK != EvalExpression(line, CurrSection().GetPC(),
				scope_address[scope_depth], -1, -1, value))
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

int BuildInstructionTable(OP_ID *pInstr, strref instr_text, int maxInstructions)
{
	// create an instruction table (mnemonic hash lookup)
	int numInstructions = 0;
	char group_num = 0;
	while (strref line = instr_text.next_line()) {
		int index_num = 0;
		while (line) {
			strref mnemonic = line.split_token_trim(',');
			if (mnemonic) {
				OP_ID &op_hash = pInstr[numInstructions++];
				op_hash.op_hash = mnemonic.fnv1a_lower();
				op_hash.group = group_num;
				op_hash.index = index_num;
				op_hash.type = OT_MNEMONIC;
			}
			index_num++;
		}
		group_num++;
	}
	
	// add assembler directives
	for (int d=0; d<nDirectiveNames; d++) {
		OP_ID &op_hash = pInstr[numInstructions++];
		op_hash.op_hash = strref(aDirectiveNames[d].name).fnv1a_lower();
		op_hash.group = 0xff;
		op_hash.index = (unsigned char)aDirectiveNames[d].directive;
		op_hash.type = OT_DIRECTIVE;
	}
	
	// sort table by hash for binary search lookup
	qsort(pInstr, numInstructions, sizeof(OP_ID), sortHashLookup);
	return numInstructions;
}

AddressingMode Asm::GetAddressMode(strref line, bool flipXY, StatusCode &error, strref &expression)
{
	bool force_zp = false;
	bool need_more = true;
	strref arg, deco;
	AddressingMode addrMode = AM_INVALID;
	while (need_more) {
		need_more = false;
		switch (line.get_first()) {
			case 0:		// empty line, empty addressing mode
				addrMode = AM_NONE;
				break;
			case '(':	// relative (jmp (addr), (zp,x), (zp),y)
				deco = line.scoped_block_skip();
				line.skip_whitespace();
				expression = deco.split_token_trim(',');
				addrMode = AM_RELATIVE;
				if (deco[0]=='x' || deco[0]=='X')
					addrMode = AM_REL_ZP_X;
				else if (line[0]==',') {
					++line;
					line.skip_whitespace();
					if (line[0]=='y' || line[0]=='Y') {
						addrMode = AM_REL_ZP_Y;
						++line;
					}
				}
				break;
			case '#':	// immediate, determine if value is ok
				++line;
				addrMode = AM_IMMEDIATE;
				expression = line;
				break;
			case '.': {	// .z => force zp (needs further parsing for address mode)
				++line;
				char c = line.get_first();
				if ((c=='z' || c=='Z') && strref::is_sep_ws(line[1])) {
					force_zp = true;
					++line;
					need_more = true;
				} else
					error = ERROR_UNEXPECTED_CHARACTER_IN_ADDRESSING_MODE;
				break;
			}
			default: {	// accumulator or absolute
				if (line) {
					if (line.get_label().same_str("A")) {
						addrMode = AM_ACCUMULATOR;
					} else {	// absolute (zp, offs x, offs y)
						addrMode = force_zp ? AM_ZP : AM_ABSOLUTE;
						expression = line.split_token_trim(',');
						bool relX = line && (line[0]=='x' || line[0]=='X');
						bool relY = line && (line[0]=='y' || line[0]=='Y');
						if ((flipXY && relY) || (!flipXY && relX))
							addrMode = addrMode==AM_ZP ? AM_ZP_X : AM_ABSOLUTE_X;
						else if ((flipXY && relX) || (!flipXY && relY)) {
							if (force_zp) {
								error = ERROR_INSTRUCTION_NOT_ZP;
								break;
							}
							addrMode = AM_ABSOLUTE_Y;
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
StatusCode Asm::AddOpcode(strref line, int group, int index, strref source_file)
{
	StatusCode error = STATUS_OK;
	int base_opcode = aMulAddGroup[group][1] + index * aMulAddGroup[group][0];
	strref expression;
	
	// Get the addressing mode and the expression it refers to
	AddressingMode addrMode = GetAddressMode(line,
		group==OPG_CC10&&index>=OPI_STX&&index<=OPI_LDX, error, expression);
	
	int value = 0;
	int target_section = -1;
	bool evalLater = false;
	if (expression) {
		error = EvalExpression(expression, CurrSection().GetPC(),
							   scope_address[scope_depth], -1,
							   (group==OPG_BRANCH ? SectionId() : -1), value);
		if (error == STATUS_NOT_READY) {
			evalLater = true;
			error = STATUS_OK;
		} else if (error == STATUS_RELATIVE_SECTION)
			target_section = lastEvalSection;
		else if (error != STATUS_OK)
			return error;
	}
	
	// check if address is in zero page range and should use a ZP mode instead of absolute
	if (!evalLater && value>=0 && value<0x100 && group!=OPG_BRANCH && error != STATUS_RELATIVE_SECTION) {
		switch (addrMode) {
			case AM_ABSOLUTE:
				addrMode = AM_ZP;
				break;
			case AM_ABSOLUTE_X:
				addrMode = AM_ZP_X;
				break;
			default:
				break;
		}
	}
	
	CODE_ARG codeArg = CA_NONE;
	unsigned char opcode = base_opcode;
	
	// analyze addressing mode per mnemonic group
	switch (group) {
		case OPG_BRANCH:
			if (addrMode != AM_ABSOLUTE) {
				error = ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH;
				break;
			}
			codeArg = CA_BRANCH;
			break;
			
		case OPG_SUBROUT:
			if (index==OPI_JSR) {	// jsr
				if (addrMode != AM_ABSOLUTE)
					error = ERROR_INVALID_ADDRESSING_MODE_FOR_BRANCH;
				else
					codeArg = CA_TWO_BYTES;
			}
			break;
		case OPG_STACK:
		case OPG_FLAG:
		case OPG_TRANS:
			codeArg = CA_NONE;
			break;
		case OPG_CC00:
			// jump relative exception
			if (addrMode==AM_RELATIVE && index==OPI_JMP) {
				base_opcode += RELATIVE_JMP_DELTA;
				addrMode = AM_ABSOLUTE; // the relative address is in an absolute location ;)
			}
			if (addrMode>7 || (CC00Mask[index]&(1<<addrMode))==0)
				error = ERROR_BAD_ADDRESSING_MODE;
			else {
				opcode = base_opcode + CC00ModeAdd[addrMode];
				switch (addrMode) {
					case AM_ABSOLUTE:
					case AM_ABSOLUTE_Y:
					case AM_ABSOLUTE_X:
						codeArg = CA_TWO_BYTES;
						break;
					default:
						codeArg = CA_ONE_BYTE;
						break;
				}
			}
			break;
		case OPG_CC01:
			if (addrMode>7 || (addrMode==AM_IMMEDIATE && index==OPI_STA))
				error = ERROR_BAD_ADDRESSING_MODE;
			else {
				opcode = base_opcode + addrMode*4;
				switch (addrMode) {
					case AM_ABSOLUTE:
					case AM_ABSOLUTE_Y:
					case AM_ABSOLUTE_X:
						codeArg = CA_TWO_BYTES;
						break;
					default:
						codeArg = CA_ONE_BYTE;
						break;
				}
			}
			break;
		case OPG_CC10: {
			if (addrMode == AM_NONE || addrMode == AM_ACCUMULATOR) {
				if (index>=4)
					error = ERROR_BAD_ADDRESSING_MODE;
				else {
					opcode = base_opcode + 8;
					codeArg = CA_NONE;
				}
			} else {
				if (addrMode>7 || (CC10Mask[index]&(1<<addrMode))==0)
					error = ERROR_BAD_ADDRESSING_MODE;
				else {
					opcode = base_opcode + CC10ModeAdd[addrMode];
					switch (addrMode) {
						case AM_IMMEDIATE:
						case AM_ZP:
						case AM_ZP_X:
							codeArg = CA_ONE_BYTE;
							break;
						default:
							codeArg = CA_TWO_BYTES;
							break;
					}
				}
			}
			break;
		}
	}
	// Add the instruction and argument to the code
	if (error == STATUS_OK || error == STATUS_RELATIVE_SECTION) {
		CheckOutputCapacity(4);
		switch (codeArg) {
			case CA_BRANCH:
				AddByte(opcode);
				if (evalLater)
					AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BRANCH);
				else if (((int)value - (int)CurrSection().GetPC()-1) < -128 || ((int)value - (int)CurrSection().GetPC()-1) > 127) {
					error = ERROR_BRANCH_OUT_OF_RANGE;
					break;
				}
				AddByte(evalLater ? 0 : (unsigned char)((int)value - (int)CurrSection().GetPC()) - 1);
				break;
			case CA_ONE_BYTE:
				AddByte(opcode);
				if (evalLater || error == STATUS_RELATIVE_SECTION)
					AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_BYTE);
				AddByte(value);
				break;
			case CA_TWO_BYTES:
				AddByte(opcode);
				if (evalLater)
					AddLateEval(CurrSection().GetPC(), scope_address[scope_depth], expression, source_file, LateEval::LET_ABS_REF);
				else
					CurrSection().AddReloc(value, CurrSection().GetPC(), target_section, Reloc::WORD);
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

	while (line && error == STATUS_OK) {
		strref line_start = line;
		char char0 = line[0]; // first char including white space
		line.skip_whitespace();	// skip to first character
		line = line.before_or_full(';'); // clip any line comments
		line = line.before_or_full(c_comment);
		line.clip_trailing_whitespace();
		strref line_nocom = line;
		strref operation = line.split_range(syntax==SYNTAX_MERLIN ? label_end_char_range_merlin : label_end_char_range);
		char char1 = operation[0];			// first char of first word
		char charE = operation.get_last();	// end char of first word

		line.trim_whitespace();
		bool force_label = charE==':' || charE=='$';
		if (!force_label && syntax==SYNTAX_MERLIN && line) // MERLIN fixes and PoP does some naughty stuff like 'and = 0'
			force_label = !strref::is_ws(char0) || char1==']' || charE=='?';
		
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
			if (operation[0]==':' || operation[0]=='.')
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
				} else if (ConditionalAsm() && pInstr[op_idx].type==OT_MNEMONIC) {
					OP_ID &id = pInstr[op_idx];
					int group = id.group;
					int index = id.index;
					error = AddOpcode(line, group, index, contextStack.curr().source_file);
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
			error = ERROR_UNTERMINATED_CONDITION;
		}
		
		if (error > STATUS_NOT_READY)
			PrintError(line_start, error);
		
		// dealt with error, continue with next instruction unless too broken
		if (error < ERROR_STOP_PROCESSING_ON_HIGHER)
			error = STATUS_OK;
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

// create an instruction table (mnemonic hash lookup + directives)
void Asm::Assemble(strref source, strref filename)
{
	OP_ID *pInstr = new OP_ID[256];
	int numInstructions = BuildInstructionTable(pInstr, strref(aInstr, strl_t(sizeof(aInstr)-1)), 256);

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

		// close last local label of symbol file
		if (symbol_export && last_label_local)
			symbols.append("}\n");

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


int main(int argc, char **argv)
{
	int return_value = 0;
	bool load_header = true;
	bool size_header = false;
	Asm assembler;

	const char *source_filename=nullptr, *binary_out_name=nullptr;
	const char *sym_file=nullptr, *vs_file=nullptr;
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
			} else if (arg.same_str("sym") && (a + 1) < argc)
				sym_file = argv[++a];
			else if (arg.same_str("vice") && (a + 1) < argc)
				vs_file = argv[++a];
		}
		else if (!source_filename)
			source_filename = arg.get();
		else if (!binary_out_name)
			binary_out_name = arg.get();
	}

	if (!source_filename) {
		puts("Usage:\nAsm6502 [options] filename.s code.prg\n"
			 " * -i<path>: Add include path\n * -D<label>[=<value>]: Define a label with an optional value (otherwise 1)\n"
			 " * -bin: Raw binary\n * -c64: Include load address (default)\n * -a2b: Apple II Dos 3.3 Binary\n"
			 " * -sym <file.sym>: vice/kick asm symbol file\n"
			 " * -vice <file.vs>: export a vice symbol file\nhttps://github.com/sakrac/Asm6502\n");
		return 0;
	}
	

	// Load source
	if (source_filename) {
		size_t size = 0;
		strref srcname(source_filename);
		if (char *buffer = assembler.LoadText(srcname, size)) {
			// if source_filename contains a path add that as a search path for include files
			assembler.AddIncludeFolder(srcname.before_last('/', '\\'));

			assembler.symbol_export = sym_file!=nullptr;
			assembler.Assemble(strref(buffer, strl_t(size)), strref(argv[1]));
			
			/*if (assembler.errorEncountered)
				return_value = 1;
			else*/ {

				printf("SECTIONS SUMMARY\n===============Y\n");
				for (size_t i = 0; i<assembler.allSections.size(); ++i) {
					Section &s = assembler.allSections[i];
					printf("Section %d: \"" STRREF_FMT "\" Dummy: %s Relative: %s Start: 0x%04x End: 0x%04x\n",
						   (int)i, STRREF_ARG(s.name), s.dummySection ? "yes" : "no",
						   s.IsRelativeSection() ? "yes" : "no", s.start_address, s.address);
					if (s.pRelocs) {
						for (relocList::iterator i = s.pRelocs->begin(); i != s.pRelocs->end(); ++i)
							printf("\tReloc value $%x at offs $%x section %d\n", i->base_value, i->section_offset, i->target_section);
					}
				}

				if (binary_out_name && !assembler.CurrSection().empty()) {
					if (FILE *f = fopen(binary_out_name, "wb")) {
						if (load_header) {
							char addr[2] = {
								(char)assembler.CurrSection().GetLoadAddress(),
								(char)(assembler.CurrSection().GetLoadAddress() >> 8) };
							fwrite(addr, 2, 1, f);
						}
						if (size_header) {
							size_t int_size = assembler.CurrSection().size();
							char byte_size[2] = { (char)int_size, (char)(int_size >> 8) };
							fwrite(byte_size, 2, 1, f);
						}
						fwrite(assembler.CurrSection().get(), assembler.CurrSection().size(), 1, f);
						fclose(f);
					}
				}
				if (sym_file && assembler.symbols.get_len()) {
					if (FILE *f = fopen(sym_file, "w")) {
						fwrite(assembler.symbols.get(), assembler.symbols.get_len(), 1, f);
						fclose(f);
					}
				}
				if (vs_file && assembler.symbols.get_len()) {
					if (FILE *f = fopen(vs_file, "w")) {
						strref syms = assembler.symbols.get_strref();
						while (strref label = syms.line()) {
							if (label.has_prefix(".label")) {
								label.next_word_ws();
								strref name = label.split_token_trim('=');
								if (label[0] == '$')
									++label;
								fprintf(f, "al " STRREF_FMT " %s" STRREF_FMT "\n", STRREF_ARG(label), name[0]=='.' ? "" : ".", STRREF_ARG(name));
							}
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
