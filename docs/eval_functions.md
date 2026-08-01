# Eval Functions

Eval functions are used like symbols in expressions, but they are always followed by parentheses with optional arguments. They are case-insensitive and can be called with or without a leading dot in many contexts, which makes them handy in macros and conditional assembly.

A simple conditional example:

```asm
if .defined(start)
    EVAL start is available
endif
```

### DEFINED, DEF

	.if .def(symbol)
	.endif

Evaluates to `1` if the symbol has been defined so far during the current assembly, and `0` otherwise.

### REFERENCED

	.if .referenced(symbol)
	.endif

Evaluates to `1` if the symbol has been referenced in the current assembly. The symbol should be defined at this point.

### BLANK

	.if .blank()
	.endif

Evaluates to `1` if the contents inside the parentheses are empty, which is primarily useful within macros.

### CONST

	if .const(symbol)
	.endif

Evaluates to `1` if the symbol has been declared `CONST`. The symbol should be defined at this point.

### SIZEOF

	STRUCT Module {
		word Init
		word Update
		word Shutdown
	}

	ds SIZEOF(Module)

Returns the byte size of a given struct.

### TRIGSIN

The parser recognizes `TRIGSIN` as an eval-function name. The current implementation does not provide a working trigonometric implementation yet and returns `0` for now.

