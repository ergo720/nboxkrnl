/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.hpp"
#include "hw_exp.hpp"
#include "..\hal\halp.hpp"
#include "..\rtl\exp_sup.hpp"
#include <string.h>


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
		__asm mov esp, ebp \
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


// These handlers will first attempt to fix the problem in the kernel, and if that fails, they will deliver the exception to the xbe since it might
// have installed an SEH handler for it. If both fail, the execution will be terminated

// Divide Error
void __declspec(naked) XBOXAPI KiTrap0()
{
	// NOTE: the cpu raises this exception also for division overflows, but we don't check for it and always report a divide by zero code

	__asm {
		CREATE_KTRAP_FRAME_NO_CODE;
		sti
		mov eax, 0xC0000094 // STATUS_INTEGER_DIVIDE_BY_ZERO
		mov ebx, [ebp]KTRAP_FRAME.Eip
		CREATE_EXCEPTION_RECORD_ARG0;
		HANDLE_EXCEPTION;
		EXIT_EXCEPTION;
	}
}

// Debug breakpoint
void __declspec(naked) XBOXAPI KiTrap1()
{
	HalpShutdownSystem();
}

// NMI interrupt
void __declspec(naked) XBOXAPI KiTrap2()
{
	HalpShutdownSystem();
}

// Breakpoint (int 3)
void __declspec(naked) XBOXAPI KiTrap3()
{
	HalpShutdownSystem();
}

// Overflow (int O)
void __declspec(naked) XBOXAPI KiTrap4()
{
	HalpShutdownSystem();
}

// Bound range exceeded
void __declspec(naked) XBOXAPI KiTrap5()
{
	HalpShutdownSystem();
}

// Invalid opcode
void __declspec(naked) XBOXAPI KiTrap6()
{
	HalpShutdownSystem();
}

// No math coprocessor
void __declspec(naked) XBOXAPI KiTrap7()
{
	HalpShutdownSystem();
}

// Double fault
void __declspec(naked) XBOXAPI KiTrap8()
{
	HalpShutdownSystem();
}

// Invalid tss
void __declspec(naked) XBOXAPI KiTrap10()
{
	HalpShutdownSystem();
}

// Segment not present
void __declspec(naked) XBOXAPI KiTrap11()
{
	HalpShutdownSystem();
}

// Stack-segment fault
void __declspec(naked) XBOXAPI KiTrap12()
{
	HalpShutdownSystem();
}

// General protection
void __declspec(naked) XBOXAPI KiTrap13()
{
	HalpShutdownSystem();
}

// Page fault
void __declspec(naked) XBOXAPI KiTrap14()
{
	HalpShutdownSystem();
}

// Math fault
void __declspec(naked) XBOXAPI KiTrap16()
{
	HalpShutdownSystem();
}

// Alignment check
void __declspec(naked) XBOXAPI KiTrap17()
{
	HalpShutdownSystem();
}

// Machine check
void __declspec(naked) XBOXAPI KiTrap18()
{
	HalpShutdownSystem();
}

// SIMD floating-point exception
void __declspec(naked) XBOXAPI KiTrap19()
{
	HalpShutdownSystem();
}

// Used to catch any intel-reserved exception
void __declspec(naked) XBOXAPI KiUnexpectedInterrupt()
{
	HalpShutdownSystem();
}

VOID FASTCALL KiRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance, PKTRAP_FRAME TrapFrame);

VOID __declspec(naked) KiRaiseExceptionService(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
	// ecx -> ExceptionRecord
	// edx -> ContextRecord
	// eax -> FirstChance

	__asm {
		CREATE_KTRAP_FRAME_NO_CODE;
		sti
		push ebp
		push eax
		call KiRaiseException
		EXIT_EXCEPTION;
	}
}

 // XXX This should probably be moved in a file specific for float support
static VOID KiFlushNPXState()
{
	__asm {
		pushfd
		cli
		mov edx, [KiPcr]KPCR.PrcbData.CurrentThread
		cmp byte ptr [edx]KTHREAD.NpxState, NPX_STATE_LOADED
		jne not_loaded
		mov eax, cr0
		test eax, (CR0_MP | CR0_EM | CR0_TS)
		jz no_fpu_exp
		and eax, ~(CR0_MP | CR0_EM | CR0_TS)
		mov cr0, eax
	no_fpu_exp:
		mov ecx, [KiPcr]KPCR.NtTib.StackBase
		fxsave [ecx]
		mov byte ptr [edx]KTHREAD.NpxState, NPX_STATE_NOT_LOADED
		mov [KiPcr]KPCR.PrcbData.NpxThread, 0
		or eax, NPX_STATE_NOT_LOADED
		or eax, [ecx]
		mov cr0, eax
	not_loaded:
		popfd
	}
}

static VOID KiCopyKframeToContext(PKTRAP_FRAME TrapFrame, PCONTEXT ContextRecord)
{
	if (ContextRecord->ContextFlags & CONTEXT_CONTROL) {
		ContextRecord->Ebp = TrapFrame->Ebp;
		ContextRecord->Eip = TrapFrame->Eip;
		ContextRecord->SegCs = TrapFrame->SegCs & 0xFFFF;
		ContextRecord->EFlags = TrapFrame->EFlags;
		ContextRecord->SegSs = 0x10; // same selector set in KernelEntry, never changes after that
		ContextRecord->Esp = reinterpret_cast<ULONG>((&TrapFrame->EFlags) + 1); // cpu is always in ring 0, so a stack switch never occurs
	}

	if (ContextRecord->ContextFlags & CONTEXT_INTEGER) {
		ContextRecord->Edi = TrapFrame->Edi;
		ContextRecord->Esi = TrapFrame->Esi;
		ContextRecord->Ebx = TrapFrame->Ebx;
		ContextRecord->Ecx = TrapFrame->Ecx;
		ContextRecord->Edx = TrapFrame->Edx;
		ContextRecord->Eax = TrapFrame->Eax;
	}

	if ((ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT) || (ContextRecord->ContextFlags & CONTEXT_EXTENDED_REGISTERS)) {
		KiFlushNPXState();
		PFX_SAVE_AREA NpxFrame;
		KeGetStackBase(NpxFrame);
		memcpy(&ContextRecord->FloatSave, &NpxFrame->FloatSave, sizeof(FLOATING_SAVE_AREA));
	}
}

static VOID KiCopyContextToKframe(PKTRAP_FRAME TrapFrame, PCONTEXT ContextRecord)
{
	if (ContextRecord->ContextFlags & CONTEXT_CONTROL) {
		TrapFrame->Ebp = ContextRecord->Ebp;
		TrapFrame->Eip = ContextRecord->Eip;
		TrapFrame->EFlags = ContextRecord->EFlags & 0x003E0FD7; // blocks modifying iopl, nt, rf and reserved bits
		if (reinterpret_cast<ULONG>((&TrapFrame->EFlags) + 1) != ContextRecord->Esp) {
			// The macro EXIT_EXCEPTION currently assumes that esp was not touched in the TRAP_FRAME, so bug check for now if the handler modified it
			KeBugCheck(INVALID_CONTEXT);
		}
	}

	if (ContextRecord->ContextFlags & CONTEXT_INTEGER) {
		TrapFrame->Edi = ContextRecord->Edi;
		TrapFrame->Esi = ContextRecord->Esi;
		TrapFrame->Ebx = ContextRecord->Ebx;
		TrapFrame->Ecx = ContextRecord->Ecx;
		TrapFrame->Edx = ContextRecord->Edx;
		TrapFrame->Eax = ContextRecord->Eax;
	}

	if ((ContextRecord->ContextFlags & CONTEXT_FLOATING_POINT) || (ContextRecord->ContextFlags & CONTEXT_EXTENDED_REGISTERS)) {
		KiFlushNPXState();
		PFX_SAVE_AREA NpxFrame;
		KeGetStackBase(NpxFrame);
		memcpy(&NpxFrame->FloatSave, &ContextRecord->FloatSave, sizeof(FLOATING_SAVE_AREA));
		NpxFrame->FloatSave.Cr0NpxState &= ~(CR0_EM | CR0_MP | CR0_TS);
		NpxFrame->FloatSave.MXCsr = NpxFrame->FloatSave.MXCsr & 0xFFBF;
	}
}

static VOID FASTCALL KiRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance, PKTRAP_FRAME TrapFrame)
{
	KiCopyContextToKframe(TrapFrame, ContextRecord);
	KiDispatchException(ExceptionRecord, TrapFrame, FirstChance);
}

VOID FASTCALL KiDispatchException(PEXCEPTION_RECORD ExceptionRecord, PKTRAP_FRAME TrapFrame, BOOLEAN FirstChance)
{
	CONTEXT ContextRecord;
	ContextRecord.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_EXTENDED_REGISTERS;
	KiCopyKframeToContext(TrapFrame, &ContextRecord);

	if (ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) {
		ContextRecord.Eip--;
	}

	if (FirstChance) {
		if (RtlDispatchException(ExceptionRecord, &ContextRecord)) {
			KiCopyContextToKframe(TrapFrame, &ContextRecord);
			return;
		}
	}

	KeBugCheckEx(KERNEL_UNHANDLED_EXCEPTION, ExceptionRecord->ExceptionCode, reinterpret_cast<ULONG>(ExceptionRecord->ExceptionAddress),
		ExceptionRecord->ExceptionInformation[0], ExceptionRecord->ExceptionInformation[1]);
}
