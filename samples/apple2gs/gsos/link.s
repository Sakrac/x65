//
// fun2gs linker file
//
	; Stack, and Direct page space
	; in Bank0	
	section DirectPage_Stack,BSS
	ds.b 1024

	incobj "start.x65"
	incobj "dbgfnt.x65"
	incobj "lz4.x65"
	incobj "blit.x65"
	incobj "background.x65"
	
	; Get these things in the same bank
	;Merge start,dbgfont

