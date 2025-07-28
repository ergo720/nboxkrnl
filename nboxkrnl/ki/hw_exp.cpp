/*
 * ergo720                Copyright (c) 2022
 */

#include "ki.hpp"
#include "hw_exp.hpp"
#include "rtl.hpp"
#include "exp_sup.hpp"
#include "mi.hpp"
#include "dbg.hpp"
#include <string.h>


// These handlers will first attempt to fix the problem in the kernel, and if that fails, they will deliver the exception to the xbe since it might
// have installed an SEH handler for it. If both fail, the execution will be terminated

void __declspec(naked) XBOXAPI KiTrapDE()
{
	// NOTE: the cpu raises this exception also for division overflows, but we don't check for it and always report a divide by zero code

	ASM_BEGIN
		CREATE_KTRAP_FRAME_NO_CODE;
		ASM(sti);
		ASM(mov eax, 0xC0000094); // STATUS_INTEGER_DIVIDE_BY_ZERO
		ASM(mov ebx, dword ptr [ebp]KTRAP_FRAME.Eip);
		CREATE_EXCEPTION_RECORD_ARG0;
		HANDLE_EXCEPTION;
		EXIT_EXCEPTION;
	ASM_END
}

void __declspec(naked) XBOXAPI KiTrapDB()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapNMI()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapBP()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapOF()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapBR()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapUD()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapNM()
{
	// If CurrentThread->NpxState == NPX_STATE_NOT_LOADED, then we are here to load the floating state for this thread. Otherwise, bug check in all other cases

	ASM_BEGIN
		CREATE_KTRAP_FRAME_NO_CODE;
		ASM(mov edx, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(cmp byte ptr [edx]KTHREAD.NpxState, NPX_STATE_LOADED);
		ASM(je unexpected_exp);
		ASM(mov eax, cr0);
		ASM(and eax, ~(CR0_MP | CR0_EM | CR0_TS)); // allow executing fpu instructions
		ASM(mov cr0, eax);
		ASM(mov eax, dword ptr [KiPcr]KPCR.PrcbData.NpxThread);
		ASM(test eax, eax);
		ASM(jz no_npx_thread); // NpxThread can be nullptr when no thread has loaded the floating state
		ASM(mov ecx, dword ptr [eax]KTHREAD.StackBase);
		ASM(sub ecx, SIZE FX_SAVE_AREA); // points to FLOATING_SAVE_AREA of NpxThread
		ASM(fxsave [ecx]);
		ASM(mov dword ptr [eax]KTHREAD.NpxState, NPX_STATE_NOT_LOADED);
	no_npx_thread:
		ASM(mov ecx, dword ptr [edx]KTHREAD.StackBase);
		ASM(sub ecx, SIZE FX_SAVE_AREA); // points to FLOATING_SAVE_AREA of CurrentThread
		ASM(fxrstor [ecx]);
		ASM(and dword ptr [edx]FLOATING_SAVE_AREA.Cr0NpxState, ~(CR0_MP | CR0_EM | CR0_TS)); // allow executing fpu instructions for this thread only
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.NpxThread, edx);
		ASM(mov byte ptr [edx]KTHREAD.NpxState, NPX_STATE_LOADED);
		ASM(jmp exit_exp);
	unexpected_exp:
		ASM(sti);
		ASM(push KERNEL_UNHANDLED_EXCEPTION);
		ASM(call KeBugCheckLogEip); // won't return
	exit_exp:
		EXIT_EXCEPTION;
	ASM_END
}

void __declspec(naked) XBOXAPI KiTrapDF()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapTS()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapNP()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapSS()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapGP()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapPF()
{
	ASM_BEGIN
		CREATE_KTRAP_FRAME_WITH_CODE;
		ASM(push dword ptr [ebp]KTRAP_FRAME.Eip);
		ASM(mov edx, cr2);
		ASM(push edx);
		ASM(call MiPageFaultHandler);
		ASM(sti);
		ASM(mov eax, 0xC0000005); // STATUS_ACCESS_VIOLATION
		ASM(mov ebx, dword ptr [ebp]KTRAP_FRAME.Eip);
		CREATE_EXCEPTION_RECORD_ARG0;
		HANDLE_EXCEPTION;
		EXIT_EXCEPTION;
	ASM_END
}

void __declspec(naked) XBOXAPI KiTrapMF()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapAC()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapMC()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiTrapXM()
{
	RIP_UNIMPLEMENTED();
}

void __declspec(naked) XBOXAPI KiUnexpectedInterrupt()
{
	// Used to catch any intel-reserved exception
	RIP_UNIMPLEMENTED();
}

VOID FASTCALL KiContinue(PCONTEXT ContextRecord, BOOLEAN TestAlert, PKTRAP_FRAME TrapFrame);

VOID __declspec(naked) KiContinueService(PCONTEXT ContextRecord, BOOLEAN TestAlert)
{
  // ecx -> ContextRecord
  // edx -> TestAlert

	ASM_BEGIN
		CREATE_KTRAP_FRAME_NO_CODE;
		ASM(sti);
		ASM(push ebp);
		ASM(call KiContinue);
		EXIT_EXCEPTION;
	ASM_END
}

VOID FASTCALL KiRaiseException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance, PKTRAP_FRAME TrapFrame);

VOID __declspec(naked) KiRaiseExceptionService(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord, BOOLEAN FirstChance)
{
	// ecx -> ExceptionRecord
	// edx -> ContextRecord
	// eax -> FirstChance

	ASM_BEGIN
		CREATE_KTRAP_FRAME_NO_CODE;
		ASM(sti);
		ASM(push ebp);
		ASM(push eax);
		ASM(call KiRaiseException);
		EXIT_EXCEPTION;
	ASM_END
}

 // XXX This should probably be moved in a file specific for float support
static VOID KiFlushNPXState()
{
	ASM_BEGIN
		ASM(pushfd);
		ASM(cli);
		ASM(mov edx, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(cmp byte ptr [edx]KTHREAD.NpxState, NPX_STATE_LOADED);
		ASM(jne not_loaded);
		ASM(mov eax, cr0);
		ASM(test eax, (CR0_MP | CR0_EM | CR0_TS));
		ASM(jz no_fpu_exp);
		ASM(and eax, ~(CR0_MP | CR0_EM | CR0_TS));
		ASM(mov cr0, eax);
	no_fpu_exp:
		ASM(mov ecx, dword ptr [KiPcr]KPCR.NtTib.StackBase);
		ASM(fxsave [ecx]);
		ASM(mov byte ptr [edx]KTHREAD.NpxState, NPX_STATE_NOT_LOADED);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.NpxThread, 0);
		ASM(or eax, NPX_STATE_NOT_LOADED);
		ASM(or dword ptr [ecx]FLOATING_SAVE_AREA.Cr0NpxState, NPX_STATE_NOT_LOADED); // block executing fpu instructions for this thread only
		ASM(mov cr0, eax);
	not_loaded:
		ASM(popfd);
	ASM_END
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
