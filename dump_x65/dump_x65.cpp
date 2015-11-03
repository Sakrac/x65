//
//  dump_x65.cpp
//  dump_x65
//
//  Created by Carl-Henrik Skårstedt on 10/26/15.
//  Copyright © 2015 Carl-Henrik Skårstedt. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include "struse.h"

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
	int address;			// PC relative to section or fixed
	int target;				// offset into section memory
	short section;			// section to target
	short rept;				// value of rept for this late eval
	short scope;			// PC start of scope
	short type;				// label, byte, branch, word (LateEval::Type)
};

struct ObjFileMapSymbol {
	struct ObjFileStr name;	// symbol name
	int value;
	short section;
	bool local;				// local labels are probably needed
};

enum ShowFlags {
	SHOW_SECTIONS = 1,
	SHOW_RELOCS = 2,
	SHOW_LABELS = 4,
	SHOW_MAP_SYMBOLS = 8,
	SHOW_LATE_EVAL = 16,

	SHOW_DEFAULT = SHOW_SECTIONS
};

char* LoadBinary(const char* filename, size_t &size)
{
	if (FILE *f = fopen(filename, "rb")) {
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
	size = 0;
	return nullptr;
}

strref PoolStr(struct ObjFileStr off, const char *str_pool)
{
	return off.offs>=0 ? strref(str_pool + off.offs) : strref();
}

enum LEType {			// When an expression is evaluated late, determine how to encode the result
	LET_LABEL,			// this evaluation applies to a label and not memory
	LET_ABS_REF,		// calculate an absolute address and store at 0, +1
	LET_ABS_L_REF,		// calculate a bank + absolute address and store at 0, +1, +2
	LET_ABS_4_REF,		// calculate a 32 bit number
	LET_BRANCH,			// calculate a branch offset and store at this address
	LET_BRANCH_16,		// calculate a branch offset of 16 bits and store at this address
	LET_BYTE,			// calculate a byte and store at this address
};

static const char *reloc_type[] = {
	"NONE", "WORD", "LO_BYTE", "HI_BYTE" };
static const char *late_type[] = {
	"LABEL", "ABS_REF", "ABS_L_REF", "ABS_4_REF",
	"BRANCH", "BRANCH_16", "BYTE" };


void ReadObjectFile(const char *file, unsigned int show = SHOW_DEFAULT)
{
	size_t size;
	if (char *data = LoadBinary(file, size)) {
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

			// sections
			if (show & SHOW_SECTIONS) {
				int reloc_idx = 0;
				for (int si = 0; si < hdr.sections; si++) {
					struct ObjFileSection &s = aSect[si];
					short f = s.flags;
					if (f & (1 << ObjFileSection::OFS_MERGED)) {
						printf("Section %d: " STRREF_FMT "(Merged)\n",
							   reloc_idx, STRREF_ARG(PoolStr(s.name, str_orig)));
					} else if (f & (1 << ObjFileSection::OFS_DUMMY)) {
						if (f&(1 << ObjFileSection::OFS_FIXED)) {
							printf("Section %d: " STRREF_FMT "(Dummy, fixed at $%04x)\n",
								   reloc_idx, STRREF_ARG(PoolStr(s.name, str_orig)), s.start_address);
						} else {
							printf("Section %d: " STRREF_FMT "(Dummy, relative)\n",
								   reloc_idx, STRREF_ARG(PoolStr(s.name, str_orig)));
						}
					} else {
						if (f&(1 << ObjFileSection::OFS_FIXED)) {
							printf("Section %d: " STRREF_FMT "(Fixed $%04x, $%x bytes)\n",
								   reloc_idx, STRREF_ARG(PoolStr(s.name, str_orig)), s.start_address, s.output_size);
						} else {
							printf("Section %d: " STRREF_FMT "(Relative, $%x bytes, align to $%x)\n",
								   reloc_idx, STRREF_ARG(PoolStr(s.name, str_orig)), s.output_size, s.align_address);
						}
						strref export_append = PoolStr(s.exp_app, str_orig);
						if (export_append)
							printf("  Export as: " STRREF_FMT "\n", STRREF_ARG(export_append));
					}
					reloc_idx++;
				}
			}
			
			if (show & SHOW_RELOCS) {
				for (int si = 0; si < hdr.sections; si++) {
					printf("section %d relocs: %d\n", si, aSect[si].relocs);
					for (int r = 0; r < aSect[si].relocs; r++) {
						struct ObjFileReloc &rs = aReloc[r];
						printf("Reloc: section %d offset $%x base $%x type %s\n",
							   rs.target_section, rs.section_offset, rs.target_section, reloc_type[rs.value_type]);
					}
				}
			}

			if (show & SHOW_MAP_SYMBOLS) {
				for (int mi = 0; mi < hdr.map_symbols; mi++) {
					struct ObjFileMapSymbol &m = aMapSyms[mi];
					printf("Symbol: \"" STRREF_FMT "\" section: %d value: $%04x%s\n",
						   STRREF_ARG(PoolStr(m.name, str_orig)), m.section, m.value, m.local ? " (local)" : "");
				}
			}

			if (show & SHOW_LABELS) {
				for (int li = 0; li < hdr.labels; li++) {
					struct ObjFileLabel &l = aLabels[li];
					strref name = PoolStr(l.name, str_orig);
					short f = l.flags;
					int external = f & ObjFileLabel::OFL_XDEF;
					printf("Label: " STRREF_FMT " %s", STRREF_ARG(name), external==ObjFileLabel::OFL_XDEF ? "external" : "protected");
					if (external!=ObjFileLabel::OFL_XDEF)
						printf(" file %d", external);
					if (f & ObjFileLabel::OFL_CNST)
						printf(" constant");
					if (f & ObjFileLabel::OFL_ADDR)
						printf(" PC rel");
					if (f & ObjFileLabel::OFL_EVAL)
						printf(" evaluated");

					printf(" map index: %d\n", l.mapIndex);
				}
			}

			if (show & SHOW_LATE_EVAL) {
				for (int li = 0; li < hdr.late_evals; ++li) {
					struct ObjFileLateEval &le = aLateEval[li];
					strref name = PoolStr(le.label, str_orig);
					if (le.type == LEType::LET_LABEL) {
						printf("Late eval label: " STRREF_FMT " expression: " STRREF_FMT "\n",
							   STRREF_ARG(name), STRREF_ARG(PoolStr(le.expression, str_orig)));
					} else {
						printf("Late eval section: %d addr: $%04x scope: $%04x target: $%04x expression: " STRREF_FMT " %s\n",
							   le.section, le.address, le.scope, le.target, STRREF_ARG(PoolStr(le.expression, str_orig)),
							   late_type[le.type]);
					}
				}
			}

			// restore previous section
		} else
			printf("Not a valid x65 file\n");
	} else
		printf("Could not open %s\n", file);
}

int main(int argc, char **argv)
{
	const char *file = nullptr;
	bool def = true;
	unsigned int show = SHOW_DEFAULT, prv_show = 0;
	for (int a = 1; a<argc; a++) {
		if (argv[a][0]=='-') {
			strref arg = argv[a]+1;
			if (arg.same_str("sections")) {
				show = prv_show | SHOW_SECTIONS;
				prv_show = show;
			} else if (arg.same_str("relocs")) {
				show = prv_show | SHOW_RELOCS;
				prv_show = show;
			} else if (arg.same_str("labels")) {
				show = prv_show | SHOW_LABELS;
				prv_show = show;
			} else if (arg.same_str("map")) {
				show = prv_show | SHOW_MAP_SYMBOLS;
				prv_show = show;
			} else if (arg.same_str("late_eval")) {
				show = prv_show | SHOW_LATE_EVAL;
				prv_show = show;
			} else if (arg.same_str("all")) {
				show = prv_show | SHOW_SECTIONS | SHOW_RELOCS | SHOW_LABELS | SHOW_MAP_SYMBOLS | SHOW_LATE_EVAL;
				prv_show = show;
			}
		} else if (!file)
			file = argv[a];
	}

	if (!file) {
		printf("Usage:\ndump_x65 filename [-sections] [-relocs] [-labels] [-map] [-late_eval[\n");
		return 0;
	}

	if (argc>1) {
		ReadObjectFile(file, show);
	}
}
