* Memory Manager macros
*   by Dave Klimas
;
; Copyright Apple Computer, Inc. 1986, 1987
; and Roger Wagner Publishing, Inc. 1988
; All Rights Reserved
;
macro _MMBootInit
{
                     Tool  $102
}
;~MMStartUp           MAC
;                     PHA
macro 	_MMStartUp
{
                     Tool  $202
}
;~MMShutDown          MAC
;                     PHW   ]1
macro _MMShutDown
{
                     Tool  $302
}
;~MMVersion           MAC
;                     PHA
macro _MMVersion
{
                     Tool  $402
}
macro _MMReset
{
                     Tool  $502
}
;~MMStatus            MAC
;                     PHA
macro _MMStatus
{
                     Tool  $602
}
;~NewHandle           MAC
;                     P2SL  ]1
;                     PxW   ]2;]3
;                     PHL   ]4
macro _NewHandle
{
                     Tool  $902
}
;~ReallocHandle       MAC
;                     PHLW  ]1;]2
;                     PHWL  ]3;]4
;                     PHL   ]5
macro _ReallocHandle
{
                     Tool  $A02
}
;~RestoreHandle       MAC
;                     PHL   ]1
macro _RestoreHandle
{
                     Tool  $B02
}
;~AddToOOMQueue       MAC
;                     PHL   ]1
macro _AddToOOMQueue
{
                     Tool  $C02
}
;~DeleteFromOOMQueue  MAC
;                     PHL   ]1
macro _DeleteFromOOMQueue
{
                     Tool  $D02
}
;~DisposeHandle       MAC
;                     PHL   ]1
macro _DisposeHandle
{
                     Tool  $1002
}
;~DisposeAll          MAC
;                     PHW   ]1
macro _DisposeAll
{
                     Tool  $1102
}
;~PurgeHandle         MAC
;                     PHL   ]1
macro _PurgeHandle
{
                     Tool  $1202
}
;~PurgeAll            MAC
;                     PHW   ]1
macro _PurgeAll
{
                     Tool  $1302
}
;~GetHandleSize       MAC
;                     P2SL  ]1
macro _GetHandleSize
{
                     Tool  $1802
}
;~SetHandleSize       MAC
;                     PxL   ]1;]2
macro _SetHandleSize
{
                     Tool  $1902
}
;~FindHandle          MAC
;                     P2SL  ]1
macro _FindHandle
{
                     Tool  $1A02
}
;~FreeMem             MAC
;                     PHS   2
macro _FreeMem
{
                     Tool  $1B02
}
;~MaxBlock            MAC
;                     PHS   2
macro _MaxBlock
{
                     Tool  $1C02
}
;~TotalMem            MAC
;                     PHS   2
macro _TotalMem
{
                     Tool  $1D02
}
;~CheckHandle         MAC
;                     PHL   ]1
macro _CheckHandle
{
                     Tool  $1E02
}
macro _CompactMem
{
                     Tool  $1F02
}
;~HLock               MAC
;                     PHL   ]1
macro _HLock
{
                     Tool  $2002
}
;~HLockAll            MAC
;                     PHW   ]1
macro _HLockAll
{
                     Tool  $2102
}
;~HUnlock             MAC
;                     PHL   ]1
macro _HUnlock
{
                     Tool  $2202
}
;~HUnlockAll          MAC
;                     PHW   ]1
macro _HUnlockAll
{
                     Tool  $2302
}
;~SetPurge            MAC
;                     PHWL  ]1;]2
macro _SetPurge
{
                     Tool  $2402
}
;~SetPurgeAll         MAC
;                     PxW   ]1;]2
macro _SetPurgeAll
{
                     Tool  $2502
}
;~PtrToHand           MAC
;                     PxL   ]1;]2;]3
macro _PtrToHand
{
                     Tool  $2802
}
;~HandToPtr           MAC
;                     PxL   ]1;]2;]3
macro _HandToPtr
{
                     Tool  $2902
}
;~HandToHand          MAC
;                     PxL   ]1;]2;3
macro _HandToHand
{
                     Tool  $2A02
}
;~BlockMove           MAC
;                     PxL   ]1;]2;]3
macro _BlockMove
{
                     Tool  $2B02
}
;~RealFreeMem         MAC
;                     PHS   2
macro _RealFreeMem
{
                     Tool  $2F02
}
macro _SetHandleID
{
                     Tool  $3002
}

