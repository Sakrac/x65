# Eval Functions

Eval Functions are used like symbols in expressions but are always followed by parenthesis with optional arguments.

### DEFINED, DEF

	.if .def(symbol)
	.endif

Evaluates to 1 if the symbol has been defined or 0 if it has not been encountered to this point in the current assembly.

### REFERENCED

	.if .referenced(symbol)
	.endif

Evaluates to 1 if the symbol has been referenced in the current assembly, the symbol should be defined at this point.

### BLANK

	.if .blank()
	.endif

Evaluates to 1 if the contents within the parenthesis is empty, primarily for use within macros.

### CONST

	if .const(symbol)
	.endif

Evaluates to 1 if the symbol has been declared CONST, the symbol should be defined at this point.

### SIZEOF

	STRUCT Module {
		word Init
		word Update
		word Shutdown
	}

	ds SIZEOF( Module )

Returns the byte size of a given struct.

### TRIGSIN

Not implemented, experimental math feature, currently returns 0
