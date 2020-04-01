*-----   Merlin 16+ Directives

	rel
	dsk start.l
 
	include common.i
;	SECTION code
   

dbg	XREF dbgprint_char
	XREF background.c1
	
STRING FontOrder = " !""#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
	
start	ent
            mx        %00                  ; assemble in 16 bit

            rel                            ; Build a relocated S16 file
            include Locator.Macs.s       ; Macro Definition Files
            include Mem.Macs.s
            include Misc.Macs.s
            include Util.Macs.s
            include Sound.Macs.s

*-----   Begin Of Program   ----------

            PHK                            ; Data Bank Register = Program Bank Register
            PLB
			
			; A has UserID
			sta myID
			
			bra .next
	  
; work around OMF export bug
			dc.t start	

.next
			; D = direct page address
			; S = Stack Pointer
			tsx
			stx |mySP
			tdc
			sta |myDP
			sec
			tsc
			sbc |myDP
			inc 				; Count byte # 0
			sta |myBank0Size 

            JSR       ToolInit             ; Init Tools + Compact Memory + Ask Shadowing
            JSR       BackupEnv            ; Backup environment (colors...)

*-----   Your Code Starts Here   ----------
			phb
			lda #$7FFF       ; Length - 1
			ldx #<background.c1 ; source address
			ldy #$2000       ; dest address
			mvn ^$e12000,^background.c1  ; dst/src addresses
			plb
{
			ldy #$2000+(32*160)
			jsr PrintE1
			text "Stack ADDR:"
			dc.b 0
			
			ldy #$2000+(40*160)
			jsr PrintE1
			text "   DP ADDR:"
			dc.b 0
			
			ldy #$2000+(48*160)
			jsr PrintE1
			text "Bank0 Size:"
			dc.b 0
			
			ldy #$2000+(80*160)+(4*0)
			jsr PrintE1
			text "[Press any key to Quit]"
			dc.b 0
			
			ldy #$2000+(16*160)
			jsr PrintE1
			text "x65 OMF Test App"
			dc.b 0
}
{
	pool zpWork $e0-$100
	zpWork zpCharNum.w
	
			phb
			* get us into bank e1
			pea $e1e1
			plb
			plb
			
			lda #32
			ldy #$2000
			clc
.loop
			sta <zpCharNum
			jsr dbgprint_char
			tya
			adc #4
			tay
			lda <zpCharNum
			inc
			cmp #64
			bcc .loop

			plb
}
{
			pea $e1e1
			plb
			plb
			
			ldy #$2000+(32*160)+(4*12)
			lda >mySP
			jsr PrintHEX
			
			ldy #$2000+(40*160)+(4*12)
			lda >myDP
			jsr PrintHEX
			
			ldy #$2000+(48*160)+(4*12)
			lda >myBank0Size
			jsr PrintHEX
			
			phk
			plb
}

            JSR       WaitForKey           ; Wait until a Key is pressed

*-----   End Of Program   ---------

End         JSR       RestoreEnv           ; Restore environment (colors...)

            JSR       ToolTerm             ; End up Tools
            JMP       Exit                 ; Quit to the Launcher

************************************************************
*******         INIT TOOL SET/ FREE TOOL CODE        *******
************************************************************

ToolInit	
;			_TLStartUp                     ; Start Tools
;            PHA
;            _MMStartUp                     ; Start Memory Manager Tool Set
;            PLA
;            STA       myID                 ; Get current ID
*--
            _MTStartUp                     ; Start Miscellaneous Tool Set
*--
*			clc 						   ; grab the memory next to my DP
*			lda |myDP
*			adc #$100
*			pha
*            _SoundStartUp                  ; Start Sound Tool Set
*--
            PushLong  #0                   ; Compact Memory
            PushLong  #$8fffff
            PushWord  myID
            PushWord  #%11000000_00000000
            PushLong  #0
            _NewHandle
            _DisposeHandle
            _CompactMem
*--
	// Allocate Bank 01 memory  + 4K before and after (25 lines pre flow)
	// $012000-$019BFF pixel data
	// $019D00-$019DC7 SCB data
	// $019E00-$019FFF Clut data
	// $900 bytes afer, (14 lines buffer on the bottom, which will wreck SCB+CLUT

            PushLong  #0                   ; Ask Shadowing Screen ($8000 bytes from $01/2000)
            PushLong  #$9600
            PushWord  myID
            PushWord  #%11000000_00000011
            PushLong  #$011000
            _NewHandle
            PLA
            PLA
			BCC :NoError
*--
:NoError
            RTS

*-------

ToolTerm:
*           _SoundShutDown                 ; Stop Tools
            _MTShutDown
            PushWord  myID
            _DisposeAll
;            PushWord  myID
;            _MMShutDown
;            _TLShutDown
            RTS

myID        ds      2                    ; ID of this Program in memory
mySP		ds		2
myDP		ds      2
myBank0Size ds		2
*---------------------------------------

BackupEnv   SEP       #$30                 ; Backup Environment values (color, border...)
            LDAL      $00C022
            STA       BE_C022
            LDAL      $00C029
            STA       BE_C029
            LDAL      $00C034
            STA       BE_C034
            LDAL      $00C035
            STA       BE_C035
            REP       #$30
            RTS

*-----

RestoreEnv  SEP       #$30                 ; Restore Environment values (color, border...)
            LDA       BE_C035
            STAL      $00C035
            LDA       BE_C034
            STAL      $00C034
            LDA       BE_C029
            STAL      $00C029
            LDA       BE_C022
            STAL      $00C022
            REP       #$30
            RTS

BE_C022     byte       00                   ; Background Color
BE_C029     byte       00                   ; Linearization of the Graphic Page
BE_C034     byte       00                   ; Border Color
BE_C035     byte       00                   ; Shadowing

************************************************************
*******                   GS/OS CODE                 *******
************************************************************

GSOS        =         $E100A8

*-------

Exit        JSL       GSOS                 ; Quit Program
            dc.w      $2029
            dc.l      gsosQUIT

*-------

gsosQUIT    ds.w        2                    ; pCount
            ds.b        4                    ; pathname
            ds.b        2                    ; flags


************************************************************
*******            EVENT HANDLER CODE                *******
************************************************************

WaitForKey  SEP       #$30                 ; Wait for a Key Press
WFK_1       LDAL      $00c000
            BPL       WFK_1
            STAL      $00c010
            REP       #$30
            RTS

************************************************************
*
* Print out a TEXT String, at memory location Y
* in Bank E1
*
PrintE1:
		mx %00
{
	pool zpWork $e0-$100
	zpWork pString.l
	
*  Setup pString as pointer to the string

			plx
			inx
			stx <pString
			phk
			phk
			pla
			sta <pString+2
			
*  Font render bank			
			
			pea $e1e1
			plb
			plb
	 
			clc	

.loop
			lda [pString]
			and #$00FF
			beq .done

			sbc #31
			jsr dbgprint_char
			
			tya
			adc #4
			tay
			inc <pString
			bra .loop
.done
			pei <pString
			rts
}


************************************************************
*
* Print out a 16 bit hex number, at memory location Y
* in the current bank
*
PrintHEX:
		mx %00
{		
		pha
		xba
		lsr
		lsr
		lsr
		lsr
		and #$000F
		
		tax
		lda >chartable,x
		and #$00FF
		jsr dbgprint_char
		
		tya
		adc #4
		tay
		lda 2,s
		and #$000F
		tax
		lda >chartable,x
		and #$00FF
		jsr dbgprint_char
		
		tya
		adc #4
		tay
		
		lda 1,s
		lsr
		lsr
		lsr
		lsr
		and #$000F
		tax
		lda >chartable,x
		and #$00FF
		jsr dbgprint_char
		
		tya
		adc #4
		tay
		pla
		and #$000F
		tax
		lda >chartable,x
		and #$00FF
		jsr dbgprint_char		

		rts

chartable dc.b 16,17,18,19,20,21,22,23,24,25,33,34,35,36,37,38

}
		
************************************************************

