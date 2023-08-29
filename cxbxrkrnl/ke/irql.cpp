/*
 * ergo720                Copyright (c) 2023
 * cxbxr devs
 */

#include "ke.hpp"
#include "..\ki\ki.hpp"
#include "..\hal\halp.hpp"


EXPORTNUM(103) KIRQL XBOXAPI KeGetCurrentIrql()
{
	return KeGetPcr()->Irql;
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
		mov edx, HalpInterruptRequestRegister
		and edx, HalpIrqlMasks[ecx * 4]
		jz no_int
		bsr ecx, edx
		cmp ecx, DISPATCH_LEVEL
		jbe sw_int
		// FIXME: this should mask the interrupts of the 8259 with the IMR register. However, lib86cpu doesn't implement interrupts
		// at the moment, so we will ignore this for now
		mov edx, 1
		shl edx, cl
		xor HalpInterruptRequestRegister, edx // this clears the interrupt in the IRR
	sw_int:
		call SwIntHandlers[ecx * 4]
	no_int:
		popfd
	}
}
