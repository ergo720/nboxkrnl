/*
 * ergo720                Copyright (c) 2023
 */

#include "ke.hpp"
#include "..\ki\ki.hpp"
#include "..\hal\hal.hpp"
#include "..\hal\halp.hpp"


EXPORTNUM(103) KIRQL XBOXAPI KeGetCurrentIrql()
{
	__asm movzx eax, byte ptr [KiPcr].Irql
}

EXPORTNUM(129) KIRQL XBOXAPI KeRaiseIrqlToDpcLevel()
{
	// This function is called frequently, so avoid forwarding to KfRaiseIrql
	// This function must update the irql atomically, so we use inline assembly

	__asm {
		movzx eax, byte ptr [KiPcr].Irql // clear the high bits to avoid returning a bogus irql
		mov byte ptr [KiPcr].Irql, DISPATCH_LEVEL
#if _DEBUG
		// Only bug check in debug builds
		cmp al, DISPATCH_LEVEL
		jle ok
		push 0
		push 0
		push DISPATCH_LEVEL
		push eax
		push IRQL_NOT_GREATER_OR_EQUAL
		call KeBugCheckEx
	ok:
#endif
	}
}

EXPORTNUM(161) VOID FASTCALL KfLowerIrql
(
	KIRQL NewIrql
)
{
	// This function must update the irql atomically, so we use inline assembly

	__asm {
		and ecx, 0xFF // clear the high bits to avoid using a bogus irql
#if _DEBUG
		// Only bug check in debug builds
		cmp byte ptr [KiPcr].Irql, cl
		jge ok
		xor eax, eax
		mov al, byte ptr [KiPcr].Irql
		mov byte ptr [KiPcr].Irql, HIGH_LEVEL
		push 0
		push 0
		push ecx
		push eax
		push IRQL_NOT_LESS_OR_EQUAL
		call KeBugCheckEx
	ok:
#endif
		pushfd
		cli
		mov byte ptr [KiPcr].Irql, cl
		call HalpCheckUnmaskedInt
		popfd
	}
}

EXPORTNUM(163) __declspec(naked) VOID FASTCALL KiUnlockDispatcherDatabase
(
	KIRQL OldIrql
)
{
	__asm {
		mov eax, [KiPcr]KPCR.PrcbData.NextThread
		test eax, eax
		jz end_func // check to see if a new thread was selected by the scheduler
		cmp cl, DISPATCH_LEVEL
		jb thread_switch // check to see if we can context switch to the new thread
		mov eax, [KiPcr]KPCR.PrcbData.DpcRoutineActive
		jnz end_func // we can't, so request a dispatch interrupt if we are not running a DPC already
		push ecx
		mov cl, DISPATCH_LEVEL
		call HalRequestSoftwareInterrupt
		pop ecx
		jmp end_func
	thread_switch:
		// Save non-volatile registers
		push esi
		push edi
		push ebx
		push ebp
		mov edi, eax
		mov esi, [KiPcr]KPCR.PrcbData.CurrentThread
		mov byte ptr [esi]KTHREAD.WaitIrql, cl // we still need to lower the IRQL when this thread is switched back, so save it for later use
		mov ecx, esi
		call KeAddThreadToTailOfReadyList
		mov [KiPcr]KPCR.PrcbData.CurrentThread, edi
		mov [KiPcr]KPCR.PrcbData.NextThread, 0
		movzx ebx, byte ptr [esi]KTHREAD.WaitIrql
		call KiSwapThreadContext // when this returns, it means this thread was switched back again
		test eax, eax
		mov cl, byte ptr [edi]KTHREAD.WaitIrql
		jnz deliver_apc
	restore_regs:
		// Restore non-volatile registers
		pop ebp
		pop ebx
		pop edi
		pop esi
		jmp end_func
	deliver_apc:
		mov cl, APC_LEVEL
		call KfLowerIrql
		call HalpSwIntApc // NOTE: this might not be right, depending on how that function is going to be implemented
		xor ecx, ecx // if KiSwapThreadContext signals an APC, then WaitIrql of the previous thread must have been zero
		jmp restore_regs
	end_func:
		jmp KfLowerIrql
	}
}
