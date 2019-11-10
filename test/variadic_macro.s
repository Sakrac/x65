; Macros ending with ... as a parameter will be treated as a variadic macro

macro VariadicTest( lbl, ... ) {
lbl:
	rept va_cnt { dc.b rept }	; idea is to use var_arg(rept) eventually
}

VariadicTest Hello, 1, 2, 3, 4, 5

VariadicTest Countdown, 3, 2, 1, 0

