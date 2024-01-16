/*
 * ergo720                Copyright (c) 2022
 */

#include "hal.hpp"
#include "halp.hpp"
#include "..\rtl\rtl.hpp"
#include <assert.h>


// Mask out sw interrupts in HalpIntInProgress
#define ACTIVE_IRQ_MASK 0xFFFFFFF0

// Mask of hw interrupts currently being serviced
static DWORD HalpIntInProgress = 0;

// This is used to track sw/hw interrupts currently pending. First four are for sw interrupts,
// while the remaining 16 are for the hw interrupts of the 8259 PIC
static DWORD HalpPendingInt = 0;

// Mask of interrupts currently disabled on the pic. 1st byte: master, 2nd byte: slave. Note that IRQ2 of the master connects to the slave, so it's never disabled
static WORD HalpIntDisabled = 0xFFFB;

// Mask of all sw/hw interrupts that are unmasked at every IRQL. This table is the opposite of PicIRQMasksForIRQL
static constexpr DWORD HalpIrqlMasks[] = {
	0b11111111111111111111111111111110,  // IRQL 0  (PASSIVE)
	0b11111111111111111111111111111100,  // IRQL 1  (APC)
	0b11111111111111111111111111111000,  // IRQL 2  (DPC)
	0b11111111111111111111111111110000,  // IRQL 3
	0b00000011111111111111111111110000,  // IRQL 4
	0b00000001111111111111111111110000,  // IRQL 5
	0b00000000111111111111111111110000,  // IRQL 6
	0b00000000011111111111111111110000,  // IRQL 7
	0b00000000001111111111111111110000,  // IRQL 8
	0b00000000000111111111111111110000,  // IRQL 9
	0b00000000000011111111111111110000,  // IRQL 10
	0b00000000000001111111111111110000,  // IRQL 11
	0b00000000000000111111111111110000,  // IRQL 12 (IDE)
	0b00000000000000011111111111110000,  // IRQL 13
	0b00000000000000011111111111110000,  // IRQL 14
	0b00000000000000010111111111110000,  // IRQL 15 (SMBUS)
	0b00000000000000010011111111110000,  // IRQL 16
	0b00000000000000010001111111110000,  // IRQL 17 (USB1)
	0b00000000000000010001111111110000,  // IRQL 18
	0b00000000000000010001011111110000,  // IRQL 19
	0b00000000000000010001001111110000,  // IRQL 20 (ACI)
	0b00000000000000010001000111110000,  // IRQL 21 (APU)
	0b00000000000000010001000011110000,  // IRQL 22 (NIC)
	0b00000000000000010001000001110000,  // IRQL 23 (GPU)
	0b00000000000000010001000000110000,  // IRQL 24
	0b00000000000000010001000000010000,  // IRQL 25 (USB0)
	0b00000000000000010000000000010000,  // IRQL 26 (PROFILE)
	0b00000000000000000000000000010000,  // IRQL 27 (SCI)
	0b00000000000000000000000000000000,  // IRQL 28 (CLOCK)
	0b00000000000000000000000000000000,  // IRQL 29 (IPI)
	0b00000000000000000000000000000000,  // IRQL 30 (POWER)
	0b00000000000000000000000000000000,  // IRQL 31 (HIGH)
};

// Mask of IRQs disabled on the PIC for each IRQL. Every row of the table shows all IRQs disabled at that IRQL because, the corresponding IRQL of that IRQ is less or
// equal to the IRQL of that level. Example: IRQL12 (IDE) -> only IRQ14 is masked because all other devices have an IRQL greater than 12
// IRQL = 26 - IRQ, except for PIT (CLOCK), PROFILE and SCI, which use arbitrarily chosen IRQLs
// PIT:     IRQ0
// USB0:    IRQ1 -> IRQL25
// GPU:     IRQ3 -> IRQL23
// NIC:     IRQ4 -> IRQL22
// APU:     IRQ5 -> IRQL21
// ACI:     IRQ6 -> IRQL20
// PROFILE: IRQ8
// USB1:    IRQ9 -> IRQL17
// SMBUS:   IRQ11 -> IRQL15
// SCI:     IRQ12
// IDE:     IRQ14 -> IRQL12
static constexpr WORD PicIRQMasksForIRQL[] = {
	0b0000000000000000,  // IRQL 0  (PASSIVE)
	0b0000000000000000,  // IRQL 1  (APC)
	0b0000000000000000,  // IRQL 2  (DPC)
	0b0000000000000000,  // IRQL 3
	0b0000000000000000,  // IRQL 4
	0b0000000000000000,  // IRQL 5
	0b0000000000000000,  // IRQL 6
	0b0000000000000000,  // IRQL 7
	0b0000000000000000,  // IRQL 8
	0b0000000000000000,  // IRQL 9
	0b0000000000000000,  // IRQL 10
	0b1000000000000000,  // IRQL 11
	0b1100000000000000,  // IRQL 12 (IDE)
	0b1110000000000000,  // IRQL 13
	0b1110000000000000,  // IRQL 14
	0b1110100000000000,  // IRQL 15 (SMBUS)
	0b1110110000000000,  // IRQL 16
	0b1110111000000000,  // IRQL 17 (USB1)
	0b1110111000000000,  // IRQL 18
	0b1110111010000000,  // IRQL 19
	0b1110111011000000,  // IRQL 20 (ACI)
	0b1110111011100000,  // IRQL 21 (APU)
	0b1110111011110000,  // IRQL 22 (NIC)
	0b1110111011111000,  // IRQL 23 (GPU)
	0b1110111011111000,  // IRQL 24
	0b1110111011111010,  // IRQL 25 (USB0)
	0b1110111111111010,  // IRQL 26 (PROFILE)
	0b1111111111111010,  // IRQL 27 (SCI)
	0b1111111111111011,  // IRQL 28 (CLOCK)
	0b1111111111111011,  // IRQL 29 (IPI)
	0b1111111111111011,  // IRQL 30 (POWER)
	0b1111111111111011,  // IRQL 31 (HIGH)
};

VOID XBOXAPI HalpSwIntApc()
{
	// On entry, interrupts must be disabled

	__asm {
		movzx eax, byte ptr[KiPcr]KPCR.Irql
		mov byte ptr[KiPcr]KPCR.Irql, APC_LEVEL // raise IRQL
		and HalpPendingInt, ~(1 << APC_LEVEL)
		push eax
		sti
		call KiExecuteApcQueue
		cli
		pop eax
		mov byte ptr[KiPcr]KPCR.Irql, al // lower IRQL
		call HalpCheckUnmaskedInt
		ret
	}
}

VOID __declspec(naked) XBOXAPI HalpSwIntDpc()
{
	// On entry, interrupts must be disabled

	__asm {
		movzx eax, byte ptr [KiPcr]KPCR.Irql
		mov byte ptr [KiPcr]KPCR.Irql, DISPATCH_LEVEL // raise IRQL
		and HalpPendingInt, ~(1 << DISPATCH_LEVEL)
		push eax
		lea eax, [KiPcr]KPCR.PrcbData.DpcListHead
		cmp eax, [eax]LIST_ENTRY.Flink
		jz no_dpc
		push [KiPcr]KPCR.NtTib.ExceptionList
		mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2 // dword ptr required or else MSVC will aceess ExceptionList as a byte
		mov eax, esp
		mov esp, [KiPcr]KPCR.PrcbData.DpcStack // switch to DPC stack
		push eax
		call KiExecuteDpcQueue
		pop esp
		pop dword ptr [KiPcr]KPCR.NtTib.ExceptionList // dword ptr required or else MSVC will aceess ExceptionList as a byte
	no_dpc:
		sti
		cmp [KiPcr]KPCR.PrcbData.QuantumEnd, 0
		jnz quantum_end
		mov eax, [KiPcr]KPCR.PrcbData.NextThread
		test eax, eax
		jnz thread_switch
		jmp end_func
	thread_switch:
		push esi
		push edi
		push ebx
		push ebp
		mov edi, eax
		mov esi, [KiPcr]KPCR.PrcbData.CurrentThread
		mov ecx, esi
		call KeAddThreadToTailOfReadyList
		mov [KiPcr]KPCR.PrcbData.CurrentThread, edi
		mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0 // dword ptr required or else MSVC will aceess NextThread as a byte
		mov ebx, 1
		call KiSwapThreadContext // when this returns, it means this thread was switched back again
		pop ebp
		pop ebx
		pop edi
		pop esi
		jmp end_func
	quantum_end:
		mov [KiPcr]KPCR.PrcbData.QuantumEnd, 0
		call KiQuantumEnd
		test eax, eax
		jnz thread_switch
	end_func:
		cli
		pop eax
		mov byte ptr [KiPcr]KPCR.Irql, al // lower IRQL
		call HalpCheckUnmaskedInt
		ret
	}
}

VOID XBOXAPI HalpHwInt0()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 0
	}
}

VOID XBOXAPI HalpHwInt1()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 1
	}
}

VOID XBOXAPI HalpHwInt2()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 2
	}
}

VOID XBOXAPI HalpHwInt3()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 3
	}
}

VOID XBOXAPI HalpHwInt4()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 4
	}
}

VOID XBOXAPI HalpHwInt5()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 5
	}
}

VOID XBOXAPI HalpHwInt6()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 6
	}
}

VOID XBOXAPI HalpHwInt7()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 7
	}
}

VOID XBOXAPI HalpHwInt8()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 8
	}
}

VOID XBOXAPI HalpHwInt9()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 9
	}
}

VOID XBOXAPI HalpHwInt10()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 10
	}
}

VOID XBOXAPI HalpHwInt11()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 11
	}
}

VOID XBOXAPI HalpHwInt12()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 12
	}
}

VOID XBOXAPI HalpHwInt13()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 13
	}
}

VOID XBOXAPI HalpHwInt14()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 14
	}
}

VOID XBOXAPI HalpHwInt15()
{
	__asm {
		int IDT_INT_VECTOR_BASE + 15
	}
}

VOID __declspec(naked) HalpCheckUnmaskedInt()
{
	// On entry, interrupts must be disabled

	__asm {
	check_int:
		movzx ecx, byte ptr [KiPcr]KPCR.Irql
		mov edx, HalpPendingInt
		and edx, HalpIrqlMasks[ecx * 4] // if not zero, then there are one or more pending interrupts that have become unmasked
		jnz unmasked_int
		jmp exit_func
	unmasked_int:
		test HalpIntInProgress, ACTIVE_IRQ_MASK // make sure we complete the active IRQ first
		jnz exit_func
		bsr ecx, edx
		cmp ecx, DISPATCH_LEVEL
		jg hw_int
		call SwIntHandlers[ecx * 4]
		jmp check_int
	hw_int:
		mov ax, HalpIntDisabled
		out PIC_MASTER_DATA, al
		shr ax, 8
		out PIC_SLAVE_DATA, al
		mov edx, 1
		shl edx, cl
		test HalpIntInProgress, edx // check again HalpIntInProgress because if a sw/hw int comes, it re-enables interrupts, and a hw int could come again
		jnz exit_func
		or HalpIntInProgress, edx
		xor HalpPendingInt, edx
		call SwIntHandlers[ecx * 4]
		xor HalpIntInProgress, edx
		jmp check_int
	exit_func:
		ret
	}
}

VOID __declspec(naked) XBOXAPI HalpClockIsr()
{
	__asm {
		CREATE_KTRAP_FRAME_FOR_INT;
		movzx eax, byte ptr [KiPcr]KPCR.Irql
		cmp eax, CLOCK_LEVEL
		jge masked_int
		mov byte ptr [KiPcr]KPCR.Irql, CLOCK_LEVEL // raise IRQL
		push eax
		mov al, OCW2_EOI_IRQ
		out PIC_MASTER_CMD, al // send eoi to master pic
		// Query the total execution time and clock increment. If we instead just increment the time with the xbox increment, if the host
		// doesn't manage to call this every ms, then the time will start to lag behind the system clock time read from the CMOS, which in turn is synchronized
		// with the current host time
		mov edx, KE_CLOCK_INCREMENT_LOW
		in eax, dx
		mov esi, eax
		inc edx
		in eax, dx
		mov edi, eax
		inc edx
		in eax, dx
		sti
		mov ecx, [KeInterruptTime]KSYSTEM_TIME.LowTime
		mov edx, [KeInterruptTime]KSYSTEM_TIME.HighTime
		add ecx, esi
		adc edx, edi
		mov [KeInterruptTime]KSYSTEM_TIME.High2Time, edx
		mov [KeInterruptTime]KSYSTEM_TIME.LowTime, ecx
		mov [KeInterruptTime]KSYSTEM_TIME.HighTime, edx
		mov ecx, [KeSystemTime]KSYSTEM_TIME.LowTime
		mov edx, [KeSystemTime]KSYSTEM_TIME.HighTime
		add ecx, esi
		adc edx, edi
		mov [KeSystemTime]KSYSTEM_TIME.High2Time, edx
		mov [KeSystemTime]KSYSTEM_TIME.LowTime, ecx
		mov [KeSystemTime]KSYSTEM_TIME.HighTime, edx
		mov ebx, KeTickCount
		mov KeTickCount, eax
		mov edi, eax
		mov ecx, ebx
		call KiCheckExpiredTimers
		sub edi, ebx
		inc [KiPcr]KPCR.PrcbData.InterruptCount // InterruptCount: number of interrupts that have occurred
		mov ecx, [KiPcr]KPCR.PrcbData.CurrentThread
		cmp byte ptr [esp], DISPATCH_LEVEL
		jb kernel_time
		ja interrupt_time
		cmp [KiPcr]KPCR.PrcbData.DpcRoutineActive, 0
		jz kernel_time
		add [KiPcr]KPCR.PrcbData.DpcTime, edi // DpcTime: time spent executing DPCs, in ms
		jmp quantum
	interrupt_time:
		add [KiPcr]KPCR.PrcbData.InterruptTime, edi // InterruptTime: time spent executing code at IRQL > 2, in ms
		jmp quantum
	kernel_time:
		add [ecx]KTHREAD.KernelTime, edi // KernelTime: per-thread time spent executing code at IRQL < 2, in ms
	quantum:
		sub [ecx]KTHREAD.Quantum, edi
		jg not_expired
		cmp ecx, offset KiIdleThread // if it's the idle thread, then don't switch
		jz not_expired
		mov [KiPcr]KPCR.PrcbData.QuantumEnd, 1
		mov cl, DISPATCH_LEVEL
		call HalRequestSoftwareInterrupt
	not_expired:
		cli
		pop ecx
		mov byte ptr [KiPcr]KPCR.Irql, cl // lower IRQL
		call HalpCheckUnmaskedInt
		jmp end_isr
	masked_int:
		mov edx, 1
		mov ecx, IRQL_OFFSET_FOR_IRQ // clock IRQ is zero
		shl edx, cl
		or HalpPendingInt, edx // save masked int in sw IRR, so that we can deliver it later when the IRQL goes down
		mov ax, PicIRQMasksForIRQL[eax * 2]
		or ax, HalpIntDisabled // mask all IRQs on the PIC with IRQL <= than current IRQL
		out PIC_MASTER_DATA, al
		shr ax, 8
		out PIC_SLAVE_DATA, al
	end_isr:
		EXIT_INTERRUPT;
	}
}

EXPORTNUM(43) VOID XBOXAPI HalEnableSystemInterrupt
(
	ULONG BusInterruptLevel,
	KINTERRUPT_MODE InterruptMode
)
{
	disable();

	// NOTE: the bit in KINTERRUPT_MODE is the opposite of what needs to be set in elcr
	ULONG ElcrPort, DataPort;
	BYTE PicImr, CurrElcr, ElcrMask = 1 << (BusInterruptLevel & 7);
	HalpIntDisabled &= ~(1 << BusInterruptLevel);

	if (BusInterruptLevel > 7) {
		ElcrPort = PIC_SLAVE_ELCR;
		DataPort = PIC_SLAVE_DATA;
		PicImr = (HalpIntDisabled >> 8) & 0xFF;
	}
	else {
		ElcrPort = PIC_MASTER_ELCR;
		DataPort = PIC_MASTER_DATA;
		PicImr = HalpIntDisabled & 0xFF;
	}

	__asm {
		mov edx, ElcrPort
		in al, dx
		mov CurrElcr, al
	}

	if (InterruptMode == Edge) {
		CurrElcr &= ~ElcrMask;
	}
	else {
		CurrElcr |= ElcrMask;
	}

	__asm {
		mov edx, ElcrPort
		mov al, CurrElcr
		out dx, al
		mov al, PicImr
		mov edx, DataPort
		out dx, al
		sti
	}
}

EXPORTNUM(48) VOID FASTCALL HalRequestSoftwareInterrupt
(
	KIRQL Request
)
{
	assert((Request == APC_LEVEL) || (Request == DISPATCH_LEVEL));

	__asm {
		pushfd
		cli
	}

	HalpPendingInt |= (1 << Request);
	if (HalpIrqlMasks[KiPcr.Irql] & (1 << Request)) { // is the requested IRQL unmasked at the current IRQL?
		SwIntHandlers[Request]();
	}

	__asm popfd
}
