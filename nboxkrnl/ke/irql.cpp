/*
 * ergo720                Copyright (c) 2023
 */

#include "ke.hpp"
#include "ki.hpp"
#include "hal.hpp"
#include "halp.hpp"


EXPORTNUM(103) KIRQL XBOXAPI KeGetCurrentIrql()
{
	ASM(movzx eax, byte ptr [KiPcr].Irql);
}

EXPORTNUM(129) KIRQL XBOXAPI KeRaiseIrqlToDpcLevel()
{
	// This function is called frequently, so avoid forwarding to KfRaiseIrql
	// This function must update the irql atomically, so we use inline assembly

	ASM_BEGIN
		ASM(movzx eax, byte ptr [KiPcr].Irql); // clear the high bits to avoid returning a bogus irql
		ASM(mov byte ptr [KiPcr].Irql, DISPATCH_LEVEL);
#if _DEBUG
		// Only bug check in debug builds
		ASM(cmp al, DISPATCH_LEVEL);
		ASM(jle ok);
		ASM(push 0);
		ASM(push 0);
		ASM(push DISPATCH_LEVEL);
		ASM(push eax);
		ASM(push IRQL_NOT_GREATER_OR_EQUAL);
		ASM(call KeBugCheckEx);
	ok:
#endif
	ASM_END
}

EXPORTNUM(130) KIRQL XBOXAPI KeRaiseIrqlToSynchLevel()
{
	return KfRaiseIrql(SYNC_LEVEL);
}

EXPORTNUM(160) KIRQL FASTCALL KfRaiseIrql
(
	KIRQL NewIrql
)
{
	// This function must update the irql atomically, so we use inline assembly

	ASM_BEGIN
		ASM(movzx eax, byte ptr [KiPcr].Irql); // clear the high bits to avoid returning a bogus irql
		ASM(mov byte ptr [KiPcr].Irql, cl);
#if _DEBUG
		// Only bug check in debug builds
		ASM(cmp al, cl);
		ASM(jle ok);
		ASM(movzx ecx, cl);
		ASM(push 0);
		ASM(push 0);
		ASM(push ecx);
		ASM(push eax);
		ASM(push IRQL_NOT_GREATER_OR_EQUAL);
		ASM(call KeBugCheckEx);
	ok:
#endif
	ASM_END
}

EXPORTNUM(161) VOID FASTCALL KfLowerIrql
(
	KIRQL NewIrql
)
{
	// This function must update the irql atomically, so we use inline assembly

	ASM_BEGIN
		ASM(and ecx, 0xFF); // clear the high bits to avoid using a bogus irql
#if _DEBUG
		// Only bug check in debug builds
		ASM(cmp byte ptr [KiPcr].Irql, cl);
		ASM(jge ok);
		ASM(xor eax, eax);
		ASM(mov al, byte ptr [KiPcr].Irql);
		ASM(mov byte ptr [KiPcr].Irql, HIGH_LEVEL);
		ASM(push 0);
		ASM(push 0);
		ASM(push ecx);
		ASM(push eax);
		ASM(push IRQL_NOT_LESS_OR_EQUAL);
		ASM(call KeBugCheckEx);
	ok:
#endif
		ASM(pushfd);
		ASM(cli);
		ASM(mov byte ptr [KiPcr].Irql, cl);
		ASM(call HalpCheckUnmaskedInt);
		ASM(popfd);
	ASM_END
}

EXPORTNUM(163) __declspec(naked) VOID FASTCALL KiUnlockDispatcherDatabase
(
	KIRQL OldIrql
)
{
	ASM_BEGIN
		ASM(mov eax, [KiPcr]KPCR.PrcbData.NextThread);
		ASM(test eax, eax);
		ASM(jz end_func); // check to see if a new thread was selected by the scheduler
		ASM(cmp cl, DISPATCH_LEVEL);
		ASM(jb thread_switch); // check to see if we can context switch to the new thread
		ASM(mov eax, [KiPcr]KPCR.PrcbData.DpcRoutineActive);
		ASM(jnz end_func); // we can't, so request a dispatch interrupt if we are not running a DPC already
		ASM(push ecx);
		ASM(mov cl, DISPATCH_LEVEL);
		ASM(call HalRequestSoftwareInterrupt);
		ASM(pop ecx);
		ASM(jmp end_func);
	thread_switch:
		// Save non-volatile registers
		ASM(push esi);
		ASM(push edi);
		ASM(push ebx);
		ASM(push ebp);
		ASM(mov edi, eax);
		ASM(mov esi, [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(mov byte ptr [esi]KTHREAD.WaitIrql, cl); // we still need to lower the IRQL when this thread is switched back, so save it for later use
		ASM(mov ecx, esi);
		ASM(call KeAddThreadToTailOfReadyList);
		ASM(mov [KiPcr]KPCR.PrcbData.CurrentThread, edi);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0); // dword ptr required or else MSVC will aceess NextThread as a byte
		ASM(movzx ebx, byte ptr [esi]KTHREAD.WaitIrql);
		ASM(call KiSwapThreadContext); // when this returns, it means this thread was switched back again
		ASM(test eax, eax);
		ASM(mov cl, byte ptr [edi]KTHREAD.WaitIrql);
		ASM(jnz deliver_apc);
	restore_regs:
		// Restore non-volatile registers
		ASM(pop ebp);
		ASM(pop ebx);
		ASM(pop edi);
		ASM(pop esi);
		ASM(jmp end_func);
	deliver_apc:
		ASM(mov cl, APC_LEVEL);
		ASM(call KfLowerIrql);
		ASM(call KiExecuteApcQueue);
		ASM(xor ecx, ecx); // if KiSwapThreadContext signals an APC, then WaitIrql of the previous thread must have been zero
		ASM(jmp restore_regs);
	end_func:
		ASM(jmp KfLowerIrql);
	ASM_END
}
