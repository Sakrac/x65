*=================================================
* Utility Macros - from Merlin disk
*   by Dave Klimas, et al
*
* Copyright Apple Computer, Inc. 1986, 1987
* and Roger Wagner Publishing, Inc. 1988
* All Rights Reserved
*-------------------------------------------------
	if 0
;PHWL        MAC
;            PHW       ]1
;            PHL       ]2
;            <<<
;PHLW        MAC
;            PHL       ]1
;            PHW       ]2
;            <<<
;PxW         MAC
;            DO        ]0/1
;            PHW       ]1
;            DO        ]0/2
;            PHW       ]2
;            DO        ]0/3
;            PHW       ]3
;            DO        ]0/4
;            PHW       ]4
;            FIN
;            FIN
;            FIN
;            FIN
;            <<<
;PxL         MAC
;            DO        ]0/1
;            PHL       ]1
;            DO        ]0/2
;            PHL       ]2
;            DO        ]0/3
;            PHL       ]3
;            DO        ]0/4
;            PHL       ]4
;            FIN
;            FIN
;            FIN
;            FIN
;            <<<
;P2SL        MAC
;            PHA
;P1SL        MAC
;            PHA
;PHL         MAC
;            IF        #=]1
;            PEA       ^]1
;            ELSE
;            PHW       ]1+2
;            FIN
;            PHW       ]1
;            <<<
;P2SW        MAC
;            PHA
;P1SW        MAC
;            PHA
;PHW         MAC
;            IF        #=]1
;            PEA       ]1
;            ELSE
;            IF        MX/2
;            LDA       ]1+1
;            PHA
;            FIN
;            LDA       ]1
;            PHA
;            FIN
;            <<<
;PushSpace   MAC
;PHS         MAC
;            DO        ]0
;            LUP       ]1
;            PHA
;            --^
;            ELSE
;            PHA
;            FIN
;            <<<
;
;********************************
;
;Push4       MAC
;            PushLong  #0
;            PushLong  #0
;            <<<
;
;PushPtr     MAC
;            PEA       ^]1
;            PEA       ]1
;            EOM
	endif
macro PushLong value
{
	pea $value>>16
	pea $value
}

macro PushWord value
{
	lda	value
	pha
}
	if 0
;PullLong    MAC
;            DO        ]0
;            PullWord  ]1
;            PullWord  ]1+2
;            ELSE
;            PullWord
;            PullWord
;            FIN
;            <<<
;
;PullWord    MAC
;            PLA
;            DO        ]0
;            STA       ]1
;            FIN
;            IF        MX/2
;            PLA
;            DO        ]0
;            STA       ]1+1
;            FIN
;            FIN
;            <<<
;
;MoveLong    MAC
;            MoveWord  ]1;]2
;            MoveWord  ]1+2;]2+2
;            <<<
;
;MoveWord    MAC
;            LDA       ]1
;            STA       ]2
;            IF        MX/2
;            LDA       ]1+1
;            STA       ]2+1
;            FIN
;            <<<
;
;MoveBlock   MAC                      ;1st_byte;last_byte;dest
;            DO        ]2/]1
;            DO        ]3/]1
;            LDX       #]2
;            LDY       #]3+]2-]1
;            LDA       #]2-]1
;            MVP       ]1,]3
;            ELSE
;            LDX       #]1
;            LDY       #]3
;            LDA       #]2-]1
;            MVN       ]1,]3
;            FIN
;            ELSE
;            ERR       1              ;Last adrs < first adrs
;            FIN
;            <<<
;
;CmpLong     MAC
;            LDA       ]1
;            CMP       ]2
;            IF        #=]1
;            LDA       ^]1
;            ELSE
;            LDA       ]1+2
;            FIN
;            IF        #=]2
;            SBC       ^]2
;            ELSE
;            SBC       ]2+2
;            FIN
;            <<<
;
;LONGM       MAC
;LONGACC     MAC                      ;Assumes native mode
;            IF        MX&2           ;If A is now short
;            REP       %00100000
;            FIN
;            <<<
;
;LONGX       MAC
;LONGXY      MAC                      ;Assumes native mode
;            IF        MX&1           ;If X is now short
;            REP       %00010000
;            FIN
;            <<<
;
;LONG        MAC
;LONGAX      MAC                      ;Assumes native mode
;            IF        MX             ;If not now in full 16
;            REP       %00110000
;            FIN
;            <<<
;
;SHORTM      MAC
;SHORTACC    MAC                      ;Assumes native mode
;            IF        MX&2           ;If A is now short,
;            ELSE                     ; ignore
;            SEP       %00100000
;            FIN
;            <<<
;
;SHORTX      MAC
;SHORTXY     MAC                      ;Assumes native mode
;            IF        MX&1           ;If X is now short,
;            ELSE                     ; ignore
;            SEP       %00010000
;            FIN
;            <<<
;
;SHORT       MAC
;SHORTAX     MAC                      ;Assumes native mode
;            IF        MX!%11         ;If not now in full 8
;            SEP       %00110000
;            FIN
;            <<<
;
;LONGI       MAC                      ; Duplicates APW function
;            LST       OFF
;            DO        ]1             ; If arg = 1 = "on" = make long
;
;            IF        MX-3/-1        ; If M is short and X is long
;; Leave alone
;            FIN                      ; End of this test
;
;            IF        MX/3           ; If M is short and X is short
;            MX        %10            ; Make X long, leave M short
;            FIN                      ; End of this test
;
;            IF        MX!3/3         ; If M is long and X is long
;            FIN                      ; Leave alone
;
;            IF        MX-2/-1        ; If M is long and X is short
;            MX        %00            ; Make X long, leave M long
;            FIN                      ; End of this test
;
;            ELSE                     ; If arg = 0 = "off" = make short
;
;            IF        MX/3           ; If M is short and X is short
;                                     ; Leave alone
;            FIN                      ; End of this test
;
;            IF        MX-3/-1        ; If M is short and X is long
;            MX        %11            ; Make X short, leave M short
;            FIN                      ; End of this test
;
;            IF        MX-2/-1        ; If M is long and X is short
;                                     ; Leave alone
;            FIN                      ; End of this test
;
;
;            IF        MX!3/3         ; If M is long and X is long
;            MX        %01            ; Make X short, leave M long
;            FIN                      ; Leave alone
;
;            FIN                      ; End of macro tests
;
;            LST       RTN
;            <<<
;
;LONGA       MAC                      ; Duplicates APW function
;            LST       OFF
;            DO        ]1             ; If arg = 1 = "on" = make long
;
;            IF        MX-3/-1        ; If M is short and X is long
;            MX        %00            ; Make M long, leave X long
;            FIN                      ; End of this test
;
;            IF        MX/3           ; If M is short and X is short
;            MX        %01            ; Make M long, leave X short
;            FIN                      ; End of this test
;
;            IF        MX!3/3         ; If M is long and X is long
;            FIN                      ; Leave alone
;
;            IF        MX-2/-1        ; If M is long and X is short
;                                     ; Leave alone
;            FIN                      ; End of this test
;
;            ELSE                     ; If arg = 0 = "off" = make short
;
;            IF        MX/3           ; If M is short and X is short
;                                     ; Leave alone
;            FIN                      ; End of this test
;
;            IF        MX-3/-1        ; If M is short and X is long
;                                     ; Leave alone
;            FIN                      ; End of this test
;
;            IF        MX-2/-1        ; If M is long and X is short
;            MX        %11            ; Make M short, leave X short
;            FIN                      ; End of this test
;
;
;            IF        MX!3/3         ; If M is long and X is long
;            MX        %10            ; Make M short, leave X long
;            FIN                      ; Leave alone
;
;            FIN                      ; End of macro tests
;
;            LST       RTN
;            <<<
;
;M65816      MAC
;            DO        ]1
;            XC
;            XC                       ; Full 65816 mode for assembler
;            MX        %00
;            ELSE
;            MX        %11            ; 8 bit mode for assembler
;            FIN
;            <<<
;
;Expmac      MAC                      ; Replace APW GEN function
;            DO        ]1
;            EXP       ONLY           ; Expand macros
;            ELSE
;            EXP       OFF
;            FIN
;            <<<
	endif
macro Tool _toolNum
{
            LDX       #_toolNum      ; load tool call #
            JSL       $E10000        ; go to dispatcher
}
	if 0
;**************************************************
;* Auto-menu item macros                          *
;* This is one alternative for defining a menu    *
;* item.  It has the advantage of letting you     *
;* include specifiers for Bold, Italic, etc.      *
;**************************************************
;
;*-------------------------------------------------
;* Syntax:
;*        ]mnum = 0           ; initialize menu # at startvalue-1
;*        Menu ' Menu 1 '
;*
;* (See Menu macro, defined later....)
;*
;*        ]inum = 255                 ; Menu item starts with #256
;*        Item ' Choice 1 ';Kybd;'Bb';Check
;*        Ch1 = ]inum                 ; Set label Ch1 if somewhere else
;*                                      needs to use this item #.
;*        Item ' Choice 2 ';Disable;'';Kybd;'Cc'
;*        Item ' Choice 3 ';Divide;''
;*
;*        Menu ' Menu 2 '
;*
;*        Item ' Choice 4 ';Bold;'';Check
;*        Item ' Choice 5 ';Italic;'';Blank
;*        Item ' Choice 6 ';Underline';Kybd;'Dd'
;*
;* IMPORTANT: ALL items, except for Check and Blank, are followed by a second
;* value.  For the Kybd item, the ASCII characters follow in single quotes, Ex:
;* Kybd;'Cc'  (specifies Apple-C as an equivalent).
;* All other items use a null 2nd value, as in:
;* Italic;''   or Divide;''   etc.
;*
;* The variable ]inum MUST be initialized for the value of your first
;* menu item MINUS 1 before using the first Item macro.
;*
;* Check or Blank, if used, MUST be the last item in the macro line.
;*
;* There can be up to three parameter pairs after the item name.
;*-------------------------------------------------
;* The point of all this is that rather than hard-
;* code menu items values and subsequent references
;* to that number when disabling menus, etc., this
;* lets you add and delete menu items at will,
;* and have labels like Ch1, etc. above, auto-
;* matically set for the correct value during the
;* assembly.
;*-------------------------------------------------
;
;* Equates for Item macro:
;
;Bold        =         'B'            ; bold menu item
;Disable     =         'D'            ; disabled menu item
;Italic      =         'I'            ; italic menu item
;Underline   =         'U'            ; underlined menu item
;Divide      =         'V'            ; menu dividing line
;ColorHi     =         'X'            ; color hilite menu item
;Kybd        =         '*'            ; keyboard menu equivalent
;Check       =         $1243          ; menu item with checkmark
;Blank       =         $2043          ; menu item with blank
;
;*-------------------------------------------------
;
;Item        MAC                      ; Macro for creating a menu item
;
;            ASC       '--'
;            ASC       ]1             ; Text of menu item
;            ASC       '\H'
;            DA        ]inum          ; Menu item #
;
;            DO        ]0/2           ; Only if more items to do... (>2)
;
;            DO        ]2-Check-1/-1  ; Only if Check item
;            DA        ]2             ; ]2 = Check
;            ELSE                     ; otherwise kybd char or null
;            DO        ]2-Blank-1/-1  ; Only if Blank check item
;            DA        ]2             ; ]2 = Blank
;            ELSE
;            DB        ]2             ; Function char value
;            ASC       ]3             ; ASCII argument, if any for Kybd
;            FIN
;            FIN
;
;            FIN
;
;            DO        ]0/4           ; Only if more items to do... (>3)
;
;            DO        ]4-Check-1/-1  ; Only if Check item
;            DA        ]4             ; ]4 = Check
;            ELSE                     ; otherwise kybd char or null
;            DO        ]4-Blank-1/-1  ; Only if Blank check item
;            DA        ]4             ; ]4 = Blank
;            ELSE
;            DB        ]4             ; Function char value
;            ASC       ]5             ; ASCII argument, if any for Kybd
;            FIN
;            FIN
;
;            FIN
;
;            DO        ]0/6           ; Only if more items to do... (>5)
;
;            DO        ]6-Check-1/-1  ; Only if Check item
;            DA        ]6             ; ]6 = Check
;            ELSE                     ; otherwise kybd char or null
;            DO        ]6-Blank-1/-1  ; Only if Blank check item
;            DA        ]6             ; ]6 = Blank
;            ELSE
;            DB        ]6             ; Function char value
;            ASC       ]7             ; ASCII argument, if any for Kybd
;            FIN
;            FIN
;
;            FIN
;
;            DB        $00            ; End of menu item
;
;]inum       =         ]inum+1
;            <<<
;
;**************************************************
;* This is another alternative macro for both     *
;* menus and menu items.  It is simpler, and      *
;* more compact, but not as versatile.            *
;**************************************************
;
;*===============================================
;* Variables ]mnum,]inum should be defined
;* prior to using these MenuMaker macros.
;*
;* They both should be starting value-1
;*
;* Syntax:
;*
;* ]mnum = 0                      ; 1st menu number will be 1
;* ]inum = 255                    ; 1st menu item number will be 256
;*
;*        Menu ' @';X             ; Apple menu, color highlighting.
;*
;*        MItem ' About... '      ; "About" menu item
;*
;*        Menu ' Menu Title 1'         ; (this will be menu number 2)
;*
;*        MItem ' Choice 1 '
;*        MItem ' Choice 2 ';'D*Cc'    ; Disabled, kybd char: Cc
;*                                     ; Above will be menu item #'s 2&3
;*
;*
;
;
;Menu        MAC
;            ASC       '>>'
;            ASC       ]1
;            ASC       '\H'
;            DA        ]mnum
;            DO        ]0>1
;            ASC       ]2
;            FIN
;            DB        0
;]mnum       =         ]mnum+1
;            <<<
;
;MItem       MAC
;            ASC       '--'
;            ASC       ]1
;            ASC       '\H'
;            DA        ]inum
;            DO        ]0>1
;            ASC       ]2
;            FIN
;            DB        0
;]inum       =         ]inum+1
;            <<<
;
;*-----------------------------------------------------
;*
;*  Native        --    Processor is in LONG "native" mode.
;*  Native Long   --    Processor is in LONG "native" mode.
;*  Native Short  --    Processor is in SHORT "native" mode.
;*
;
;Native      MAC
;            CLC
;            XCE
;            IF        0=]0           ;If Native (Long)
;            LONGAX
;            FIN
;
;            DO        ]0
;            IF        L=]1           ;If Native Long
;            LONGAX
;
;            FIN                      ;If Native Short only
;            FIN                      ;  do CLC, XCE.
;            EOM
;
;*--------------------------------------------------------
;*
;*  Emulation    --    Set Processor into "emulation" mode.
;*
;
;Emulation   MAC
;            SEC
;            XCE
;            EOM
;
;*-----------------------------------------------------
;*
;*  WriteCh      --    Print Character From Accumulator
;*  WriteCh ADDR --    Print Character At Label
;*  WriteCh ADDR,X --  Print Character At Label,X
;*
;
;WriteCh     MAC
;            DO        ]0
;            LDA       ]1
;            FIN
;            PHA
;            LDX       #$180C
;            JSL       $E10000
;            EOM
;
;*-----------------------------------------------------
;*
;*  ReadCh      --    Get Keypress in Accumulator
;*  ReadCh ADDR --    Get Keypress in Label
;*
;
;ReadCh      MAC
;            PEA       0
;            PEA       1
;            LDX       #$220C
;            JSL       $E10000
;            PLA
;            DO        ]0
;            STA       ]1
;            FIN
;            EOM
;
;*-----------------------------------------------------
;*
;*  WriteLn "STRING"  --   Print Literal String with CR.
;*  WriteLn ADDR      --   Print String At Address with CR.
;*  WriteLn           --   Print CR.
;*
;
;WriteLn     MAC
;            DO        ]0
;            WriteStr  ]1
;            FIN
;            WriteCh   #$8D
;            EOM
;
;*-----------------------------------------------------
;*
;*  WriteStr "STRING"  --   Print Literal String.
;*  WriteStr ADDR      --   Print String At Address.
;*  WriteStr           --   Print String At A (Lo),Y (Hi).
;*
;
;WriteStr    MAC
;            IF        0=]0           ;If No Label
;            PHY
;            PHA
;
;            ELSE
;
;            IF        "=]1
;            PEA       ^]String
;            PEA       ]String
;            BRL       ]Skip
;]String     STR       ]1
;]Skip
;            ELSE
;
;            IF        '=]1
;            PEA       ^]String
;            PEA       ]String
;            BRL       ]Skip
;]String     STR       ]1
;]Skip
;            ELSE
;
;            PEA       ^]1
;            PEA       ]1
;
;            FIN
;            FIN
;            FIN
;            LDX       #$1C0C
;            JSL       $E10000
;            EOM
;
;*-----------------------------------------------------
;*
;*  DrawStrHV 8;12;"STRING"     Print Literal String on
;*  DrawStr 8;12;ADDR           Super Hi-Res Screen.
;*
;
;DrawStrHV   MAC
;            HtabVtab  ]1;]2
;            DrawStr   ]3
;            <<<
;
;*-----------------------------------------------------
;*
;*  DrawStr "STRING"  --   Print Literal String.
;*  DrawStr ADDR      --   Print String At Address.
;*
;
;DrawStr     MAC
;            IF        "=]1 
;            PEA       ^]String
;            PEA       ]String
;            BRL       ]Skip
;]String     STR       ]1
;]Skip
;            ELSE
;            IF        '=]1 
;            PEA       ^]String
;            PEA       ]String
;            BRL       ]Skip
;]String     STR       ]1
;]Skip
;            ELSE
;            PEA       ^]1
;            PEA       ]1
;            FIN
;            FIN
;            LDX       #$A504         ;DrawString
;            JSL       $E10000
;            <<<
;
;*-----------------------------------------------------
;*
;*  HtabVtab #8;#12     --   Position at Htab 8, Vtab 12.
;*  HtabVtab H;V             on super hires screens.
;*
;
;HtabVtab    MAC
;            IF        #=]1
;            LDA       ]1*8
;            ELSE
;            LDA       ]1
;            ASL
;            ASL
;            ASL
;            FIN
;            PHA
;            IF        #=]1
;            LDA       ]2*8
;            ELSE
;            LDA       ]2
;            ASL
;            ASL
;            ASL
;            FIN
;            PHA
;            LDX       #$3A04         ;MoveTo
;            JSL       $E10000
;            <<<
;
;*-----------------------------------------------------
;*
;*  Deref MyHandle;MyPtr  --  Uses zero page 0-3 to
;*                            de-reference a handle.
;*
;
;Deref       MAC
;            LDA       ]1
;            LDX       ]1+2
;            STA       0
;            STX       2
;            LDA       [0]
;            STA       ]2
;            LDY       #2
;            LDA       [0],Y
;            STA       ]2+2
;            <<<
;
;*==================================================
;* The MLI16 macro assumes the CALLDOS file from the
;* SUBROUT.LIB is linked in.  It provides an easy
;* way to make MLI calls.  Example syntax:
;*
;*        MLI16 close;CLSPARMS
;*--------------------------------------------------
;
;MLI16       MAC                      ;Uses CALLDOS file in the
;            IF        MX             ; subroutine library
;            REP       %00110000      ;Force full 16-bit mode, if
;            FIN                      ; not already there.
;            LDX       #]1            ;Call code (use MLI.CODES)
;            LDA       #]2            ;Low word of PARMS tbl adr
;            JSR       CALLDOS        ;Returns CS if an error
;            <<<
;
;*=================================================
;*  The following macros are APW-equivalents for
;*  compatibility with APW style listings.
;*
;*  One difference between Merlin and APW here:  Instead of
;*  using the syntax PULL1 ADDRESS;X, Merlin can take
;*  the raw statement PULL1 ADDRESS,X.
;*
;
;PULL1       MAC
;            SEP       #%00100000
;            PLA
;            REP       #%00100000
;            DO        ]0/1           ;If 1 parm
;            IF        MX>0
;            STA       ]1
;            FIN
;            IF        MX=0
;            STAL      ]1
;            FIN
;            FIN
;            <<<
;
;PULL3       MAC
;            SEP       #%00100000
;            PLA
;            STA       ]1
;            REP       #%00100000
;            PLA
;            STA       ]1+1
;            FIN
;            <<<
;
;PUSH1       MAC
;            SEP       #%00100000
;            IF        ]0/1           ;if one parm
;            LDA       ]1
;            FIN
;            PHA
;            REP       #%00100000
;            <<<
;
;PUSH3       MAC
;            IF        #=]1
;            LDA       #^]1           ;get two hi order bytes
;            PHA
;            PHB
;            LDA       #<]1
;            STA       1,S
;            ELSE
;            LDA       ]1+1
;            PHA
;            PHB
;            LDA       ]1
;            STA       1,S
;            FIN
;            <<<
	endif

