/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#include "..\ki\ki.h"


EXPORTNUM(156) volatile DWORD KeTickCount = 0;

VOID XBOXAPI KiTimerExpiration
(
	PKDPC Dpc,
	PVOID DeferredContext,
	PVOID SystemArgument1,
	PVOID SystemArgument2
)
{
	// TODO
}
