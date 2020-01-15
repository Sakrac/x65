# List of x65 error messages
	
## Undefined code

Could not recognize code at this point in the file

## Unexpected character in expression

A character in an expression has confused the assembler evaluator

## Too many values in expression

There is a limit to the number of values encountered in a single expression, feel free to change that number if you must.

## Too many operators in expression

There is a limit to the number of operators encountered in a single expression, this can also be modified as needed.

## Unbalanced right parenthesis in expression

A right parenthesis without a corresponding left parenthesis was encountered in the expression

## Expression operation

The expression evaluator has confused itself with an unrecognized operator.

## Expression missing values

Not enough values to complete an expression operator

## Instruction can not be zero page

An attempt to force a zero page command that does not support it was foiled by the assembler.

## Invalid addressing mode for instruction

Indeed!

## Internal label organization mishap

Internal error

## Bad addressing mode

Don't be bad

## Unexpected character in addressing mode

What gives?

## Unexpected label assignment format

Equal sign or EQU is desired for assigning const or mutable (LABEL) labels.

## Changing value of label that is constant

You declared that you would not change your mind but you did. I can't deal with it.

## Out of labels in pool

A label pool was declared at a certain size that has now been exceeded.

## Internal label pool release confusion

Internal error

## Label pool range evaluation failed

Could not determine the range for the label pool at the current line of assembly

## Label pool was redeclared within its scope

No recursive pool dipping please.

## Pool label already defined

Once is enough

## Struct already defined

Don't repeat yourself

## Referenced struct not found

But specify it at least once.

## Declare constant type not recognized (dc.?)

Specify word size using something that can be understood

## rept count expression could not be evaluated

The count needs to be evaluated at the current line of assembly

## hex must be followed by an even number of hex numbers

## DS directive failed to evaluate immediately

The count needs to be evaluated at the current line of assembly

## File is not a valid x65 object file

A file that was referenced with an INCOBJ directive was not recognized as a valid x65 object file

## Failed to read include file
## Using symbol PULL without first using a PUSH

Withdrawing beyond your deposits amounts to robbery.

## User invoked error

An ABORT or ERR directive was assembled

## Errors after this point will stop execution

This is a placeholder error message

## Branch is out of range

Max branch distance was exceeded at this point in assembly

## Function declaration is missing name or expression

A FUNCTION directive requires a name, open/close parenthesis and an expression. Parameters within the parenthesis is optional.

## Function could not resolve the expression

The expression could not be evaluated at this point in assembly

## Expression evaluateion recursion too deep
## Target address must evaluate immediately for this operation
## Scoping is too deep
## Unbalanced scope closure
## Unexpected macro formatting
## Align must evaluate immediately
## Out of memory for macro expansion

Your memory is not enough

## Problem with macro argument
## Conditional could not be resolved
## #endif encountered outside conditional block

ENDIF directive without an IF or equivalent

## #else or #elif outside conditional block

ELSE or ELIF directive without an IF or equivalent

## Struct can not be assembled as is
## Enum can not be assembled as is
## Conditional assembly (#if/#ifdef) was not terminated in file or macro

an IF or equivalent does not have a matching ENDIF directive

## rept is missing a scope ('{ ... }')

You want me to repeat what exactly?

## Link can only be used in a fixed address section
## Link can not be used in dummy sections
## Can not process this line
## Unexpected target offset for reloc or late evaluation
## CPU is not supported
## Can't append sections
## Zero page / Direct page section out of range
## Attempting to assign an address to a non-existent section
## Attempting to assign an address to a fixed address section
## Can not link a zero page section with a non-zp section
## Out of memory while building
## Can not write to file
## Assembly aborted
## Condition too deeply nested

There is a limit to the number of IFs within IFs.