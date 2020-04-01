* Misc Tool macros
*   by Dave Klimas
;
; Copyright Apple Computer, Inc. 1986, 1987
; and Roger Wagner Publishing, Inc. 1988
; All Rights Reserved
;
macro _MTBootInit
{
                      Tool  $103
}
macro _MTStartUp
{
                      Tool  $203
}
macro _MTShutDown
{
                      Tool  $303
}
;~MTVersion            MAC
;                      PHA
macro _MTVersion
{
                      Tool  $403
}
macro _MTReset
{
                      Tool  $503
}
;~MTStatus             MAC
;                      PHA
macro _MTStatus
{
                      Tool  $603
}
;~WriteBRam            MAC
;                      PHL   ]1
macro _WriteBRam
{
                      Tool  $903
}
;~ReadBRam             MAC
;                      PHL   ]1
macro _ReadBRam
{
                      Tool  $A03
}
;~WriteBParam          MAC
;                      PxW   ]1;]2
macro _WriteBParam
{
                      Tool  $B03
}
;~ReadBParam           MAC
;                      P1SW  ]1
macro _ReadBParam
{
                      Tool  $C03
}
;~ReadTimeHex          MAC
;                      PHS   4
macro _ReadTimeHex
{
                      Tool  $D03
}
;~WriteTimeHex         MAC
;                      PxW   ]1;]2;]3
macro _WriteTimeHex
{
                      Tool  $E03
}
;~ReadAsciiTime        MAC
;                      PHL   ]1
macro _ReadAsciiTime
{
                      Tool  $F03
}
;~SetVector            MAC
;                      PHWL  ]1;]2
macro _SetVector
{
                      Tool  $1003
}
;~GetVector            MAC
;                      P2SW  ]1
macro _GetVector
{
                      Tool  $1103
}
;~SetHeartBeat         MAC
;                      PHL   ]1
macro _SetHeartBeat
{
                      Tool  $1203
}
;~DelHeartBeat         MAC
;                      PHL   ]1
macro _DelHeartBeat
{
                      Tool  $1303
}
macro _ClrHeartBeat
{
                      Tool  $1403
}
;~SysFailMgr           MAC
;                      PHWL  ]1;]2
macro _SysFailMgr
{
                      Tool  $1503
}
;~GetAddr              MAC
;                      P2SW  ]1
macro _GetAddr
{
                      Tool  $1603
}
;~ReadMouse            MAC
;                      PHS   3
macro _ReadMouse
{
                      Tool  $1703
}
;~InitMouse            MAC
;                      PHW   ]1
macro _InitMouse
{
                      Tool  $1803
}
;~SetMouse             MAC
;                      PHW   ]1
macro _SetMouse
{
                      Tool  $1903
}
macro _HomeMouse
{
                      Tool  $1A03
}
macro _ClearMouse
{
                      Tool  $1B03
}
;~ClampMouse           MAC
;                      PxW   ]1;]2;]3;]4
macro _ClampMouse
{
                      Tool  $1C03
}
;~GetMouseClamp        MAC
;                      PHS   4
macro _GetMouseClamp
{
                      Tool  $1D03
}
;~PosMouse             MAC
;                      PxW   ]1;]2
macro _PosMouse
{
                      Tool  $1E03
}
;~ServeMouse           MAC
;                      PHA
macro _ServeMouse
{
                      Tool  $1F03
}
;~GetNewID             MAC
;                      P1SW  ]1
macro _GetNewID
{
                      Tool  $2003
}
;~DeleteID             MAC
;                      PHW   ]1
macro _DeleteID
{
                      Tool  $2103
}
;~StatusID             MAC
;                      PHW   ]1
macro _StatusID
{
                      Tool  $2203
}
;~IntSource            MAC
;                      PHW   ]1
macro _IntSource
{
                      Tool  $2303
}
;~FWEntry              MAC
;                      PHS   4
;                      PxW   ]1;]2;]3;]4
macro _FWEntry
{
                      Tool  $2403
}
;~GetTick              MAC
;                      PHS   2
macro _GetTick
{
                      Tool  $2503
}
;~PackBytes            MAC
;                      P1SL  ]1
;                      PxL   ]2;]3
;                      PHW   ]4
macro _PackBytes
{
                      Tool  $2603
}
;~UnPackBytes          MAC
;                      P1SL  ]1
;                      PHW   ]2
;                      PxL   ]3;]4
macro _UnPackBytes
{
                      Tool  $2703
}
;~Munger               MAC
;                      P1SL  ]1
;                      PxL   ]2;]3
;                      PHWL  ]4;]5
;                      PHWL  ]6;]7
macro _Munger
{
                      Tool  $2803
}
;~GetIRQEnable         MAC
;                      PHA
macro _GetIRQEnable
{
                      Tool  $2903
}
;~SetAbsClamp          MAC
;                      PxW   ]1;]2;]3;]4
macro _SetAbsClamp
{
                      Tool  $2A03
}
;~GetAbsClamp          MAC
;                      PHS   4
macro _GetAbsClamp
{
                      Tool  $2B03
}
macro _SysBeep
{
                      Tool  $2C03
}
;~AddToQueue           MAC
;                      PxL   ]1;]2
macro _AddToQueue
{
                      Tool  $2E03
}
;~DeleteFromQueue      MAC
;                      PxL   ]1;]2
macro _DeleteFromQueue
{
                      Tool  $2F03
}
;~SetInterruptState    MAC
;                      PHLW  ]1;]2
macro _SetInterruptState
{
                      Tool  $3003
}
;~GetInterruptState    MAC
;                      PHLW  ]1;]2
macro _GetInterruptState
{
                      Tool  $3103
}
;~GetIntStateRecSize   MAC
;                      PHA
macro _GetIntStateRecSize
{
                      Tool  $3203
}
;~ReadMouse2           MAC
;                      PHS   3
macro _ReadMouse2
{
                      Tool  $3303
}
;~GetCodeResConverter  MAC
;                      PHS   2
macro _GetCodeResConverter
{
                      Tool  $3403
}
macro _GetROMResource
{
                      Tool  $3503
}
macro _ReleaseROMResource
{
                      Tool  $3603
}
macro _ConvSeconds
{
                      Tool  $3703
}
macro _SysBeep2
{
                      Tool  $3803
}
macro _VersionString
{
                      Tool  $3903
}
macro _WaitUntil
{
                      Tool  $3A03
}
macro _StringToText
{
                      Tool  $3B03
}
macro _ShowBootInfo
{
                      Tool  $3C03
}
macro _ScanDevices
{
                      Tool  $3D03
}
macro _AlertMessage
{
                      Tool  $3E03
}
macro _DoSysPrefs
{
                      Tool  $3F03
}

