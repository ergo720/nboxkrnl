
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
	ASM_BEGIN \
		ASM(push ebp); \
		ASM(push ebx); \
		ASM(push esi); \
		ASM(push edi); \
		ASM(mov ebx, dword ptr fs:[0]); \
		ASM(push ebx); \
		ASM(push eax); \
		ASM(push ecx); \
		ASM(push edx); \
		ASM(sub esp, 24); \
		ASM(mov ebp, esp); \
		ASM(cld); \
		ASM(mov ebx, [ebp]KTRAP_FRAME.Ebp); \
		ASM(mov edi, [ebp]KTRAP_FRAME.Eip); \
		ASM(mov [ebp]KTRAP_FRAME.DbgArgPointer, 0); \
		ASM(mov [ebp]KTRAP_FRAME.DbgArgMark, 0xDEADBEEF); \
		ASM(mov [ebp]KTRAP_FRAME.DbgEip, edi); \
		ASM(mov [ebp]KTRAP_FRAME.DbgEbp, ebx); \
	ASM_END

#define CREATE_KTRAP_FRAME_WITH_CODE \
	ASM_BEGIN \
		ASM(mov word ptr [esp + 2], 0); \
		CREATE_KTRAP_FRAME \
	ASM_END

#define CREATE_KTRAP_FRAME_NO_CODE \
	ASM_BEGIN \
		ASM(push 0); \
		CREATE_KTRAP_FRAME \
	ASM_END

// dword ptr required or else MSVC will access ExceptionList as a byte
#define CREATE_KTRAP_FRAME_FOR_INT \
	ASM_BEGIN \
		CREATE_KTRAP_FRAME_NO_CODE \
		ASM(mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2); \
	ASM_END

#define CREATE_EXCEPTION_RECORD_ARG0 \
	ASM_BEGIN \
		ASM(sub esp, SIZE EXCEPTION_RECORD); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionCode, eax); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionFlags, 0); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionRecord, 0); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionAddress, ebx); \
		ASM(mov [esp]EXCEPTION_RECORD.NumberParameters, 0); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionInformation[0], 0); \
		ASM(mov [esp]EXCEPTION_RECORD.ExceptionInformation[1], 0); \
	ASM_END

#define HANDLE_EXCEPTION \
	ASM_BEGIN \
		ASM(mov ecx, esp); \
		ASM(mov edx, ebp); \
		ASM(push TRUE); \
		ASM(call offset KiDispatchException); \
	ASM_END

#define EXIT_EXCEPTION \
	ASM_BEGIN \
		ASM(cli); \
		ASM(mov edx, [ebp]KTRAP_FRAME.ExceptionList); \
		ASM(mov dword ptr fs:[0], edx); \
		ASM(test [ebp]KTRAP_FRAME.SegCs, SELECTOR_MASK); \
		ASM(jz esp_changed); \
		ASM(mov eax, [ebp]KTRAP_FRAME.Eax); \
		ASM(mov edx, [ebp]KTRAP_FRAME.Edx); \
		ASM(mov ecx, [ebp]KTRAP_FRAME.Ecx); \
		ASM(lea esp, [ebp]KTRAP_FRAME.Edi); \
		ASM(pop edi); \
		ASM(pop esi); \
		ASM(pop ebx); \
		ASM(pop ebp); \
		ASM(add esp, 4); \
		ASM(iretd); \
		ASM(esp_changed:); \
		ASM(mov ebx, [ebp]KTRAP_FRAME.TempSegCs); \
		ASM(mov [ebp]KTRAP_FRAME.SegCs, ebx); \
		ASM(mov ebx, [ebp]KTRAP_FRAME.TempEsp); \
		ASM(sub ebx, 12); \
		ASM(mov [ebp]KTRAP_FRAME.ErrCode, ebx); \
		ASM(mov esi, [ebp]KTRAP_FRAME.EFlags); \
		ASM(mov [ebx + 8], esi); \
		ASM(mov esi, [ebp]KTRAP_FRAME.SegCs); \
		ASM(mov [ebx + 4], esi); \
		ASM(mov esi, [ebp]KTRAP_FRAME.Eip); \
		ASM(mov [ebx], esi); \
		ASM(mov eax, [ebp]KTRAP_FRAME.Eax); \
		ASM(mov edx, [ebp]KTRAP_FRAME.Edx); \
		ASM(mov ecx, [ebp]KTRAP_FRAME.Ecx); \
		ASM(lea esp, [ebp]KTRAP_FRAME.Edi); \
		ASM(pop edi); \
		ASM(pop esi); \
		ASM(pop ebx); \
		ASM(pop ebp); \
		ASM(mov esp, [esp]); \
		ASM(iretd); \
	ASM_END

#define EXIT_INTERRUPT \
	ASM_BEGIN \
		ASM(mov edx, [ebp]KTRAP_FRAME.ExceptionList); \
		ASM(mov dword ptr fs:[0], edx); \
		ASM(mov eax, [ebp]KTRAP_FRAME.Eax); \
		ASM(mov edx, [ebp]KTRAP_FRAME.Edx); \
		ASM(mov ecx, [ebp]KTRAP_FRAME.Ecx); \
		ASM(lea esp, [ebp]KTRAP_FRAME.Edi); \
		ASM(pop edi); \
		ASM(pop esi); \
		ASM(pop ebx); \
		ASM(pop ebp); \
		ASM(add esp, 4); \
		ASM(iretd); \
	ASM_END

VOID XBOXAPI KiTrapDE();
VOID XBOXAPI KiTrapDB();
VOID XBOXAPI KiTrapNMI();
VOID XBOXAPI KiTrapBP();
VOID XBOXAPI KiTrapOF();
VOID XBOXAPI KiTrapBR();
VOID XBOXAPI KiTrapUD();
VOID XBOXAPI KiTrapNM();
VOID XBOXAPI KiTrapDF();
VOID XBOXAPI KiTrapTS();
VOID XBOXAPI KiTrapNP();
VOID XBOXAPI KiTrapSS();
VOID XBOXAPI KiTrapGP();
VOID XBOXAPI KiTrapPF();
VOID XBOXAPI KiTrapMF();
VOID XBOXAPI KiTrapAC();
VOID XBOXAPI KiTrapMC();
VOID XBOXAPI KiTrapXM();
VOID XBOXAPI KiUnexpectedInterrupt();

VOID FASTCALL KiDispatchException(PEXCEPTION_RECORD ExceptionRecord, PKTRAP_FRAME TrapFrame, BOOLEAN FirstChance);
VOID KiContinueService(PCONTEXT ContextRecord, BOOLEAN TestAlert);
VOID KiRaiseExceptionService(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance);
