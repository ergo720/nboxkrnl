
/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "ki.hpp"
#include "seh.hpp"


#pragma pack(1)
struct KTRAP_FRAME {
	ULONG DbgEbp;
	ULONG DbgEip;
	ULONG DbgArgMark;
	ULONG DbgArgPointer;
	ULONG TempSegCs;
	ULONG TempEsp;
	ULONG Edx;
	ULONG Ecx;
	ULONG Eax;
	PEXCEPTION_REGISTRATION_RECORD ExceptionList;
	ULONG Edi;
	ULONG Esi;
	ULONG Ebx;
	ULONG Ebp;
	ULONG ErrCode;
	ULONG Eip;
	ULONG SegCs;
	ULONG EFlags;
};
#pragma pack()
using PKTRAP_FRAME = KTRAP_FRAME *;


VOID XBOXAPI KiTrap0();
VOID XBOXAPI KiTrap1();
VOID XBOXAPI KiTrap2();
VOID XBOXAPI KiTrap3();
VOID XBOXAPI KiTrap4();
VOID XBOXAPI KiTrap5();
VOID XBOXAPI KiTrap6();
VOID XBOXAPI KiTrap7();
VOID XBOXAPI KiTrap8();
VOID XBOXAPI KiTrap10();
VOID XBOXAPI KiTrap11();
VOID XBOXAPI KiTrap12();
VOID XBOXAPI KiTrap13();
VOID XBOXAPI KiTrap14();
VOID XBOXAPI KiTrap16();
VOID XBOXAPI KiTrap17();
VOID XBOXAPI KiTrap18();
VOID XBOXAPI KiTrap19();
VOID XBOXAPI KiUnexpectedInterrupt();

VOID FASTCALL KiDispatchException(PEXCEPTION_RECORD ExceptionRecord, PKTRAP_FRAME TrapFrame, BOOLEAN FirstChance);
VOID KiRaiseExceptionService(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance);
