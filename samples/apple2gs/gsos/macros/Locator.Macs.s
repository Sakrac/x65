* Tool Locator macros
*   by Dave Klimas
;
; Copyright Apple Computer, Inc. 1986, 1987
; and Roger Wagner Publishing, Inc. 1988
; All Rights Reserved
;
macro _TLBootInit
{
                    Tool  $101
}
macro _TLStartUp
{
                    Tool  $201
}
macro _TLShutDown
{
                    Tool  $301
}
;~TLVersion          MAC
;                    PHA
macro _TLVersion
{
                    Tool  $401
}
macro _TLReset
{
                    Tool  $501
}
;~TLStatus           MAC
;                    PHA
macro _TLStatus
{
                    Tool  $601
}
;~GetTSPtr           MAC
;                    PHS   2
;                    PxW   ]1;]2
macro _GetTSPtr
{
                    Tool  $901
}
;~SetTSPtr           MAC
;                    PxW   ]1;]2
;                    PHL   ]3
macro _SetTSPtr
{
                    Tool  $A01
}
;~GetFuncPtr         MAC
;                    PHS   2
;                    PxW   ]1;]2
macro _GetFuncPtr
{
                    Tool  $B01
}
;~GetWAP             MAC
;                    PHS   2
;                    PxW   ]1;]2
macro _GetWAP
{
                    Tool  $C01
}
;~SetWAP             MAC
;                    PxW   ]1;]2
;                    PHL   ]3
macro _SetWAP
{
                    Tool  $D01
}
;~LoadTools          MAC
;                    PHL   ]1
macro _LoadTools
{
                    Tool  $E01
}
;~LoadOneTool        MAC
;                    PxW   ]1;]2
macro _LoadOneTool
{
                    Tool  $F01
}
;~UnloadOneTool      MAC
;                    PHW   ]1
macro _UnloadOneTool
{
                    Tool  $1001
}
;~TLMountVolume      MAC
;                    PHA
;                    PxW   ]1;]2
;                    PxL   ]3;]4;]5;]6
macro _TLMountVolume
{
                    Tool  $1101
}
;~TLTextMountVolume  MAC
;                    PHA
;                    PxL   ]1;]2;]3;]4
macro _TLTextMountVolume
{
                    Tool  $1201
}
;~SaveTextState      MAC
;                    PHS   2
macro _SaveTextState
{
                    Tool  $1301
}
;~RestoreTextState   MAC
;                    PHL   ]1
macro _RestoreTextState
{
                    Tool  $1401
}
;~MessageCenter      MAC
;                    PxW   ]1;]2
;                    PHL   ]3
macro _MessageCenter
{
                    Tool  $1501
}
macro _SetDefaultTPT
{
                    Tool  $1601
}
;~MessageByName      MAC
;                    PHS   2
;                    PHWL  ]1;]2
;                    PxW   ]3;]4
macro _MessageByName
{
                    Tool  $1701
}
;~StartUpTools       MAC
;                    PHA
;                    PxW   ]1;]2
;                    PxL   ]3;]4
macro _StartUpTools
{
                    Tool  $1801
}
;~ShutDownTools      MAC
;                    PHWL  ]1;]2
macro _ShutDownTools
{
                    Tool  $1901
}
macro _GetMsgHandle
{
                    Tool  $1A01
}
macro _AcceptRequests
{
                    Tool  $1B01
}
macro _SendRequest
{
                    Tool  $1C01
}

