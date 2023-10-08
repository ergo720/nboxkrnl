
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

// Masks out the RPL and GDT / LDT flag of a selector
#define SELECTOR_MASK 0xFFF8

 // This macro creates a KTRAP_FRAME on the stack of an exception handler. Note that the members Eip, SegCs and EFlags are already pushed by the cpu when it invokes
// the handler. ErrCode is only pushed by the cpu for exceptions that use it, for the others an additional push must be done separately

#define CREATE_KTRAP_FRAME \
	__asm { \
		__asm push ebp \
		__asm push ebx \
		__asm push esi \
		__asm push edi \
		__asm mov ebx, dword ptr fs:[0] \
		__asm push ebx \
		__asm push eax \
		__asm push ecx \
		__asm push edx \
		__asm sub esp, 24 \
		__asm mov ebp, esp \
		__asm cld \
		__asm mov ebx, [ebp]KTRAP_FRAME.Ebp \
		__asm mov edi, [ebp]KTRAP_FRAME.Eip \
		__asm mov [ebp]KTRAP_FRAME.DbgArgPointer, 0 \
		__asm mov [ebp]KTRAP_FRAME.DbgArgMark, 0xDEADBEEF \
		__asm mov [ebp]KTRAP_FRAME.DbgEip, edi \
		__asm mov [ebp]KTRAP_FRAME.DbgEbp, ebx \
	}

#define CREATE_KTRAP_FRAME_WITH_CODE \
	__asm { \
		__asm mov word ptr [esp + 2], 0 \
		CREATE_KTRAP_FRAME \
	}

#define CREATE_KTRAP_FRAME_NO_CODE \
	__asm { \
		__asm push 0 \
		CREATE_KTRAP_FRAME \
	}

#define CREATE_KTRAP_FRAME_FOR_INT \
	__asm { \
		CREATE_KTRAP_FRAME_NO_CODE \
		__asm mov [KiPcr]KPCR.NtTib.ExceptionList, 0xFFFFFFFF \
	}

#define CREATE_EXCEPTION_RECORD_ARG0 \
	__asm { \
		__asm sub esp, SIZE EXCEPTION_RECORD \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionCode, eax \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionFlags, 0 \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionRecord, 0 \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionAddress, ebx \
		__asm mov [esp]EXCEPTION_RECORD.NumberParameters, 0 \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionInformation[0], 0 \
		__asm mov [esp]EXCEPTION_RECORD.ExceptionInformation[1], 0 \
	}

#define HANDLE_EXCEPTION \
	__asm { \
		__asm mov ecx, esp \
		__asm mov edx, ebp \
		__asm push TRUE \
		__asm call offset KiDispatchException \
	}

#define EXIT_EXCEPTION \
	__asm { \
		__asm cli \
		__asm mov edx, [ebp]KTRAP_FRAME.ExceptionList \
		__asm mov dword ptr fs:[0], edx \
		__asm test [ebp]KTRAP_FRAME.SegCs, SELECTOR_MASK \
		__asm jz esp_changed \
		__asm mov eax, [ebp]KTRAP_FRAME.Eax \
		__asm mov edx, [ebp]KTRAP_FRAME.Edx \
		__asm mov ecx, [ebp]KTRAP_FRAME.Ecx \
		__asm lea esp, [ebp]KTRAP_FRAME.Edi \
		__asm pop edi \
		__asm pop esi \
		__asm pop ebx \
		__asm pop ebp \
		__asm add esp, 4 \
		__asm iretd \
		__asm esp_changed: \
		__asm mov ebx, [ebp]KTRAP_FRAME.TempSegCs \
		__asm mov [ebp]KTRAP_FRAME.SegCs, ebx \
		__asm mov ebx, [ebp]KTRAP_FRAME.TempEsp \
		__asm sub ebx, 12 \
		__asm mov [ebp]KTRAP_FRAME.ErrCode, ebx \
		__asm mov esi, [ebp]KTRAP_FRAME.EFlags \
		__asm mov [ebx + 8], esi \
		__asm mov esi, [ebp]KTRAP_FRAME.SegCs \
		__asm mov [ebx + 4], esi \
		__asm mov esi, [ebp]KTRAP_FRAME.Eip \
		__asm mov [ebx], esi \
		__asm mov eax, [ebp]KTRAP_FRAME.Eax \
		__asm mov edx, [ebp]KTRAP_FRAME.Edx \
		__asm mov ecx, [ebp]KTRAP_FRAME.Ecx \
		__asm lea esp, [ebp]KTRAP_FRAME.Edi \
		__asm pop edi \
		__asm pop esi \
		__asm pop ebx \
		__asm pop ebp \
		__asm mov esp, [esp] \
		__asm iretd \
	}

#define EXIT_INTERRUPT \
	__asm { \
		__asm mov edx, [ebp]KTRAP_FRAME.ExceptionList \
		__asm mov dword ptr fs:[0], edx \
		__asm mov eax, [ebp]KTRAP_FRAME.Eax \
		__asm mov edx, [ebp]KTRAP_FRAME.Edx \
		__asm mov ecx, [ebp]KTRAP_FRAME.Ecx \
		__asm lea esp, [ebp]KTRAP_FRAME.Edi \
		__asm pop edi \
		__asm pop esi \
		__asm pop ebx \
		__asm pop ebp \
		__asm add esp, 4 \
		__asm iretd \
	}

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
VOID KiContinueService(PCONTEXT ContextRecord, BOOLEAN TestAlert);
VOID KiRaiseExceptionService(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance);
