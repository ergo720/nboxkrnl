/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.hpp"
#include "hw_exp.hpp"
#include "..\rtl\rtl.hpp"
#include "..\rtl\exp_sup.hpp"
#include "..\mm\mi.hpp"
#include "..\dbg\dbg.hpp"
#include <string.h>


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
	RIP_UNIMPLEMENTED();
}

// NMI interrupt
void __declspec(naked) XBOXAPI KiTrap2()
{
	RIP_UNIMPLEMENTED();
}

// Breakpoint (int 3)
void __declspec(naked) XBOXAPI KiTrap3()
{
	RIP_UNIMPLEMENTED();
}

// Overflow (int O)
void __declspec(naked) XBOXAPI KiTrap4()
{
	RIP_UNIMPLEMENTED();
}

// Bound range exceeded
void __declspec(naked) XBOXAPI KiTrap5()
{
	RIP_UNIMPLEMENTED();
}

// Invalid opcode
void __declspec(naked) XBOXAPI KiTrap6()
{
	RIP_UNIMPLEMENTED();
}

// No math coprocessor
void __declspec(naked) XBOXAPI KiTrap7()
{
	RIP_UNIMPLEMENTED();
}

// Double fault
void __declspec(naked) XBOXAPI KiTrap8()
{
	RIP_UNIMPLEMENTED();
}

// Invalid tss
void __declspec(naked) XBOXAPI KiTrap10()
{
	RIP_UNIMPLEMENTED();
}

// Segment not present
void __declspec(naked) XBOXAPI KiTrap11()
{
	RIP_UNIMPLEMENTED();
}

// Stack-segment fault
void __declspec(naked) XBOXAPI KiTrap12()
{
	RIP_UNIMPLEMENTED();
}

// General protection
void __declspec(naked) XBOXAPI KiTrap13()
{
	RIP_UNIMPLEMENTED();
}

// Page fault
void __declspec(naked) XBOXAPI KiTrap14()
{
	__asm {
		CREATE_KTRAP_FRAME_WITH_CODE;
		push [ebp]KTRAP_FRAME.Eip
		mov edx, cr2
		push edx
		call MiPageFaultHandler
		sti
		mov eax, 0xC0000005 // STATUS_ACCESS_VIOLATION
		mov ebx, [ebp]KTRAP_FRAME.Eip
		CREATE_EXCEPTION_RECORD_ARG0;
		HANDLE_EXCEPTION;
		EXIT_EXCEPTION;
	}
}

// Math fault
void __declspec(naked) XBOXAPI KiTrap16()
{
	RIP_UNIMPLEMENTED();
}

// Alignment check
void __declspec(naked) XBOXAPI KiTrap17()
{
	RIP_UNIMPLEMENTED();
}

// Machine check
void __declspec(naked) XBOXAPI KiTrap18()
{
	RIP_UNIMPLEMENTED();
}

// SIMD floating-point exception
void __declspec(naked) XBOXAPI KiTrap19()
{
	RIP_UNIMPLEMENTED();
}

// Used to catch any intel-reserved exception
void __declspec(naked) XBOXAPI KiUnexpectedInterrupt()
{
	RIP_UNIMPLEMENTED();
}

VOID FASTCALL KiContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert, PKTRAP_FRAME TrapFrame);

VOID __declspec(naked) KiContinueService(PCONTEXT ContextRecord, BOOLEAN TestAlert)
{
  // ecx -> ContextRecord
  // edx -> TestAlert

	__asm {
		CREATE_KTRAP_FRAME_NO_CODE;
		sti
		push ebp
		call KiContinue
		EXIT_EXCEPTION;
	}
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

		ULONG OldEsp = reinterpret_cast<ULONG>((&TrapFrame->EFlags) + 1);
		if (OldEsp != ContextRecord->Esp) {
			if (ContextRecord->Esp < OldEsp) {
				// The new esp must be higher in the stack than the one that was saved in the trap frame
				KeBugCheck(INVALID_CONTEXT);
			}

			TrapFrame->TempSegCs = TrapFrame->SegCs;
			TrapFrame->SegCs = TrapFrame->SegCs & ~SELECTOR_MASK;
			TrapFrame->TempEsp = ContextRecord->Esp;
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

static VOID FASTCALL KiContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert, PKTRAP_FRAME TrapFrame)
{
	KiCopyContextToKframe(TrapFrame, ContextRecord);
	if (TestAlert) {
		KeTestAlertThread(KernelMode);
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

	// Capture a stack trace to help debugging this issue
	PVOID BackTrace[128] = { 0 };
	ULONG TraceHash;
	ULONG NumOfFrames = RtlCaptureStackBackTrace(0, 126, BackTrace, &TraceHash);
	DbgPrint("The kernel has encountered an unhandled exception at Eip 0x%X. A stack trace was created with %u frames captured (Hash = 0x%X)", TrapFrame->Eip, NumOfFrames, TraceHash);
	DbgPrint("The trap frame on the stack is the following:\nDbgEbp: 0x%X\nDbgEip: 0x%X\nDbgArgMark: 0x%X\nDbgArgPointer: 0x%X\nTempSegCs: 0x%X\nTempEsp: 0x%X\nEdx: 0x%X\nEcx: 0x%X\n\
Eax: 0x%X\nExceptionList: 0x%X\nEdi: 0x%X\nEsi: 0x%X\nEbx: 0x%X\nEbp: 0x%X\nErrCode: 0x%X\nEip: 0x%X\nSegCs: 0x%X\nEFlags: 0x%X",
TrapFrame->DbgEbp, TrapFrame->DbgEip, TrapFrame->DbgArgMark, TrapFrame->DbgArgPointer, TrapFrame->TempSegCs, TrapFrame->TempEsp, TrapFrame->Edx, TrapFrame->Ecx, TrapFrame->Eax,
TrapFrame->ExceptionList, TrapFrame->Edi, TrapFrame->Esi, TrapFrame->Ebx, TrapFrame->Ebp, TrapFrame->ErrCode, TrapFrame->Eip, TrapFrame->SegCs, TrapFrame->EFlags);
	for (unsigned i = 0; (i < 128) && BackTrace[i]; ++i) {
		DbgPrint("Traced Eip at level %u is 0x%X", i, BackTrace[i]);
	}

	KeBugCheckEx(KERNEL_UNHANDLED_EXCEPTION, ExceptionRecord->ExceptionCode, reinterpret_cast<ULONG>(ExceptionRecord->ExceptionAddress),
		ExceptionRecord->ExceptionInformation[0], ExceptionRecord->ExceptionInformation[1]);
}
