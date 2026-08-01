# List of x65 error messages

The assembler reports these messages through the status and error table in x65.cpp. The list below reflects the current implementation closely and is most useful when read alongside the source line that triggered it. In practice, many of these point to the same underlying issues: a missing brace or scope, an undefined label, a bad expression, or a section or link problem.

## Syntax and expression errors

- Undefined code — could not recognize code at this point in the file.
- Unexpected character in expression — a character in an expression confused the assembler evaluator.
- Too many values in expression — the expression parser exceeded the maximum number of values allowed.
- Too many operators in expression — the expression parser exceeded the maximum number of operators allowed.
- Unbalanced right parenthesis in expression — a closing parenthesis was found without a matching opening parenthesis.
- Expression operation — the expression evaluator encountered an unrecognized operator.
- Expression missing values — not enough values were present to complete an expression operation.
- Expression evaluation recursion too deep — the expression evaluator exceeded the recursion limit.
- Unexpected label assignment format — an equals sign or `EQU`-style assignment was expected for a label.
- Function declaration is missing name or expression — a `FUNCTION` directive did not include a valid name and expression.
- Function could not resolve the expression — a function body could not be evaluated at the point of use.

## Addressing, CPU, and instruction errors

- Instruction can not be zero page — an attempt was made to use a zero-page addressing mode that is not supported by the instruction.
- Invalid addressing mode for instruction — the addressing mode was not valid for the instruction.
- Bad addressing mode — the addressing mode supplied to the assembler was invalid.
- Unexpected character in addressing mode — the parser encountered an unexpected character when parsing an addressing mode.
- Branch is out of range — a branch instruction exceeded the supported target range.
- Target address must evaluate immediately for this operation — a target address was required to resolve immediately.
- Unexpected target offset for reloc or late evaluation — the relocation target address was out of range.
- CPU is not supported — the selected CPU is not supported by the current implementation.

## Labels, scopes, and structures

- Internal label organization mishap — internal label bookkeeping failed.
- Changing value of label that is constant — a constant label was assigned a new value.
- Out of labels in pool — a label pool was declared at a size that has now been exceeded.
- Internal label pool release confusion — internal label-pool bookkeeping failed.
- Label pool range evaluation failed — the label-pool range could not be resolved at the current assembly point.
- Label pool was redeclared within its scope — a pool was declared recursively within its own scope.
- Pool label already defined — the requested pool label had already been defined.
- Struct already defined — the struct was declared more than once.
- Referenced struct not found — a struct reference could not be resolved.
- Scoping is too deep — the nesting depth exceeded the supported scope depth.
- Unbalanced scope closure — the assembler found an unbalanced scope block.
- Struct can not be assembled as is — the struct could not be assembled in the current form.
- Enum can not be assembled as is — the enum could not be assembled in the current form.
- Condition too deeply nested — the conditional nesting depth exceeded the supported limit.

## Directives, macros, and conditionals

- Declare constant type not recognized (dc.?) — the size suffix for `DC` was not recognized.
- rept count expression could not be evaluated — the repeat count could not be resolved at the current line.
- hex must be followed by an even number of hex numbers — the `HEX` directive was given an odd number of hexadecimal digits.
- DS directive failed to evaluate immediately — the size requested by `DS` needed to be known at the current line.
- Using symbol PULL without first using a PUSH — `PULL` was used without a matching earlier `PUSH`.
- Align must evaluate immediately — the alignment value had to be known at the current line.
- Unexpected macro formatting — the macro syntax did not match the expected format.
- Out of memory for macro expansion — the assembler could not allocate enough memory for the macro expansion.
- Problem with macro argument — a macro argument was malformed or could not be resolved.
- Conditional could not be resolved — a conditional assembly expression could not be evaluated.
- `#endif` encountered outside conditional block — an `ENDIF` directive was found without a corresponding `IF` block.
- `#else` or `#elif` outside conditional block — an `ELSE` or `ELIF` directive was found without a corresponding `IF` block.
- Conditional assembly was not terminated in file or macro — an `IF`-style block was left unterminated.
- `rept` is missing a scope (`{ ... }`) — a repeat block was missing its required scope delimiters.

## Files, linking, and output

- File is not a valid x65 object file — the file referenced by `INCOBJ` was not a valid x65 object file.
- Failed to read include file — the assembler could not read the included source file.
- User invoked error — an `ABORT` or `ERR` directive was assembled.
- Errors after this point will stop execution — execution stops after the error threshold is reached.
- Link can only be used in a fixed address section — `LINK` was used in a relocatable section.
- Link can not be used in dummy sections — `LINK` was used in a dummy section.
- Can not process this line — the line could not be processed by the assembler.
- Can't append sections — the assembler could not append sections as requested.
- Zero page / direct page section out of range — a zero-page section address was outside the allowable range.
- Attempting to assign an address to a non-existent section — an address was assigned to a section that does not exist.
- Attempting to assign an address to a fixed address section — a fixed-address section was reassigned.
- Can not link a zero page section with a non-zp section — a zero-page section was linked with a non-zero-page section.
- Out of memory while building — the assembler ran out of memory during assembly or linking.
- Can not write to file — the assembler could not write the requested output file.
- Assembly aborted — assembly was stopped by the assembler.
