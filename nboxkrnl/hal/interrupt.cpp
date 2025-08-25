/*
 * ergo720                Copyright (c) 2022
 */

#include "hal.hpp"
#include "halp.hpp"
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

static BOOLEAN HalpCmosIsrFirstCall = TRUE;
static DWORD HalpCmosLoopStartAddress = 0;
static DWORD HalpCounterOverflow = 0;

VOID XBOXAPI HalpSwIntApc()
{
	// On entry, interrupts must be disabled

	KIRQL OldIrql = KiPcr.Irql;
	KiPcr.Irql = APC_LEVEL; // raise IRQL
	HalpPendingInt &= ~(1 << APC_LEVEL);
	enable();
	KiExecuteApcQueue();
	disable();
	KiPcr.Irql = OldIrql; // lower IRQL
	HalpCheckUnmaskedInt();
}

VOID __declspec(naked) XBOXAPI HalpSwIntDpc()
{
	// On entry, interrupts must be disabled

	ASM_BEGIN
		ASM(movzx eax, byte ptr [KiPcr]KPCR.Irql);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, DISPATCH_LEVEL); // raise IRQL
		ASM(and HalpPendingInt, ~(1 << DISPATCH_LEVEL));
		ASM(push eax);
		ASM(lea eax, dword ptr [KiPcr]KPCR.PrcbData.DpcListHead);
		ASM(cmp eax, dword ptr [eax]LIST_ENTRY.Flink);
		ASM(jz no_dpc);
		ASM(push dword ptr [KiPcr]KPCR.NtTib.ExceptionList);
		ASM(mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2);
		ASM(mov eax, esp);
		ASM(mov esp, dword ptr [KiPcr]KPCR.PrcbData.DpcStack); // switch to DPC stack
		ASM(push eax);
		ASM(call KiExecuteDpcQueue);
		ASM(pop esp);
		ASM(pop dword ptr [KiPcr]KPCR.NtTib.ExceptionList);
	no_dpc:
		ASM(sti);
		ASM(cmp dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 0);
		ASM(jnz quantum_end);
		ASM(mov eax, dword ptr [KiPcr]KPCR.PrcbData.NextThread);
		ASM(test eax, eax);
		ASM(jnz thread_switch);
		ASM(jmp end_func);
	thread_switch:
		ASM(push esi);
		ASM(push edi);
		ASM(push ebx);
		ASM(push ebp);
		ASM(mov edi, eax);
		ASM(mov esi, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(mov ecx, esi);
		ASM(call KeAddThreadToTailOfReadyList);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.CurrentThread, edi);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0);
		ASM(mov ebx, 1);
		ASM(call KiSwapThreadContext); // when this returns, it means this thread was switched back again
		ASM(pop ebp);
		ASM(pop ebx);
		ASM(pop edi);
		ASM(pop esi);
		ASM(jmp end_func);
	quantum_end:
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 0);
		ASM(call KiQuantumEnd);
		ASM(test eax, eax);
		ASM(jnz thread_switch);
	end_func:
		ASM(cli);
		ASM(pop eax);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, al); // lower IRQL
		ASM(call HalpCheckUnmaskedInt);
		ASM(ret);
	ASM_END
}

VOID XBOXAPI HalpHwInt0()
{
	ASM(int IDT_INT_VECTOR_BASE + 0);
}

VOID XBOXAPI HalpHwInt1()
{
	ASM(int IDT_INT_VECTOR_BASE + 1);
}

VOID XBOXAPI HalpHwInt2()
{
	ASM(int IDT_INT_VECTOR_BASE + 2);
}

VOID XBOXAPI HalpHwInt3()
{
	ASM(int IDT_INT_VECTOR_BASE + 3);
}

VOID XBOXAPI HalpHwInt4()
{
	ASM(int IDT_INT_VECTOR_BASE + 4);
}

VOID XBOXAPI HalpHwInt5()
{
	ASM(int IDT_INT_VECTOR_BASE + 5);
}

VOID XBOXAPI HalpHwInt6()
{
	ASM(int IDT_INT_VECTOR_BASE + 6);
}

VOID XBOXAPI HalpHwInt7()
{
	ASM(int IDT_INT_VECTOR_BASE + 7);
}

VOID XBOXAPI HalpHwInt8()
{
	ASM(int IDT_INT_VECTOR_BASE + 8);
}

VOID XBOXAPI HalpHwInt9()
{
	ASM(int IDT_INT_VECTOR_BASE + 9);
}

VOID XBOXAPI HalpHwInt10()
{
	ASM(int IDT_INT_VECTOR_BASE + 10);
}

VOID XBOXAPI HalpHwInt11()
{
	ASM(int IDT_INT_VECTOR_BASE + 11);
}

VOID XBOXAPI HalpHwInt12()
{
	ASM(int IDT_INT_VECTOR_BASE + 12);
}

VOID XBOXAPI HalpHwInt13()
{
	ASM(int IDT_INT_VECTOR_BASE + 13);
}

VOID XBOXAPI HalpHwInt14()
{
	ASM(int IDT_INT_VECTOR_BASE + 14);
}

VOID XBOXAPI HalpHwInt15()
{
	ASM(int IDT_INT_VECTOR_BASE + 15);
}

VOID HalpCheckUnmaskedInt()
{
	// On entry, interrupts must be disabled

	while (true) {
		DWORD CurrUnmaskedInt = HalpIrqlMasks[KiPcr.Irql] & HalpPendingInt;
		if (CurrUnmaskedInt) { // if not zero, then there are one or more pending interrupts that have become unmasked
			if (!(HalpIntInProgress & ACTIVE_IRQ_MASK)) { // make sure we complete the active IRQ first
				ULONG HighestPendingInt;
				bit_scan_reverse(&HighestPendingInt, CurrUnmaskedInt);
				if (HighestPendingInt > DISPATCH_LEVEL) { // hw int
					outb(PIC_MASTER_DATA, (BYTE)HalpIntDisabled);
					outb(PIC_SLAVE_DATA, (BYTE)(HalpIntDisabled >> 8));
					ULONG HighestPendingIntMask = (1 << HighestPendingInt);
					HalpIntInProgress |= HighestPendingIntMask;
					HalpPendingInt ^= HighestPendingIntMask;
					SwIntHandlers[HighestPendingInt]();
					HalpIntInProgress ^= HighestPendingIntMask;
				}
				else { // sw int
					SwIntHandlers[HighestPendingInt]();
				}
				continue;
			}
		}
		break;
	}
}

static ULONG FASTCALL HalpCheckMaskedIntAtIRQLLevelNonSpurious(ULONG BusInterruptLevel, ULONG Irql)
{
	// On entry, interrupts must be disabled

	if (BusInterruptLevel >= 8) {
		outb(PIC_MASTER_CMD, OCW2_EOI_IRQ | 2); // send eoi to master pic
	}

	if ((KIRQL)Irql <= KiPcr.Irql) {
		// Interrupt is masked at current IRQL, dismiss it
		HalpPendingInt |= (1 << (IRQL_OFFSET_FOR_IRQ + BusInterruptLevel));
		WORD IRQsMaskedAtIRQL = PicIRQMasksForIRQL[KiPcr.Irql];
		IRQsMaskedAtIRQL |= HalpIntDisabled;
		outb(PIC_MASTER_DATA, (BYTE)IRQsMaskedAtIRQL);
		IRQsMaskedAtIRQL >>= 8;
		outb(PIC_SLAVE_DATA, (BYTE)IRQsMaskedAtIRQL);
		enable();

		return 1;
	}

	KiPcr.Irql = (KIRQL)Irql; // raise IRQL
	enable();

	return 0;
}

static ULONG FASTCALL HalpCheckMaskedIntAtIRQLNonSpurious(ULONG BusInterruptLevel, ULONG Irql)
{
	// On entry, interrupts must be disabled

	if ((KIRQL)Irql <= KiPcr.Irql) {
		// Interrupt is masked at current IRQL, dismiss it
		HalpPendingInt |= (1 << (IRQL_OFFSET_FOR_IRQ + BusInterruptLevel));
		WORD IRQsMaskedAtIRQL = PicIRQMasksForIRQL[KiPcr.Irql];
		IRQsMaskedAtIRQL |= HalpIntDisabled;
		outb(PIC_MASTER_DATA, (BYTE)IRQsMaskedAtIRQL);
		IRQsMaskedAtIRQL >>= 8;
		outb(PIC_SLAVE_DATA, (BYTE)IRQsMaskedAtIRQL);
		enable();

		return 1;
	}

	KiPcr.Irql = (KIRQL)Irql; // raise IRQL
	if (BusInterruptLevel >= 8) {
		outb(PIC_SLAVE_CMD, OCW2_EOI_IRQ | (BusInterruptLevel - 8)); // send eoi to slave pic
		outb(PIC_MASTER_CMD, OCW2_EOI_IRQ | 2); // send eoi to master pic
	}
	else {
		outb(PIC_MASTER_CMD, OCW2_EOI_IRQ | BusInterruptLevel); // send eoi to master pic
	}
	enable();

	return 0;
}

template<KINTERRUPT_MODE InterruptMode>
static ULONG FASTCALL HalpCheckMaskedIntAtIRQLSpurious7(ULONG BusInterruptLevel, ULONG Irql)
{
	// On entry, interrupts must be disabled

	outb(PIC_MASTER_CMD, OCW3_READ_ISR); // read IS register of master pic
	if (inb(PIC_MASTER_CMD) & (1 << 7)) {
		if constexpr (InterruptMode == Edge) {
			return HalpCheckMaskedIntAtIRQLNonSpurious(BusInterruptLevel, Irql);
		}
		else {
			return HalpCheckMaskedIntAtIRQLLevelNonSpurious(BusInterruptLevel, Irql);
		}
	}
	enable();

	return 1;
}

template<KINTERRUPT_MODE InterruptMode>
static ULONG FASTCALL HalpCheckMaskedIntAtIRQLSpurious15(ULONG BusInterruptLevel, ULONG Irql)
{
	// On entry, interrupts must be disabled

	outb(PIC_SLAVE_CMD, OCW3_READ_ISR); // read IS register of slave pic
	if (inb(PIC_SLAVE_CMD) & (1 << 7)) {
		if constexpr (InterruptMode == Edge) {
			return HalpCheckMaskedIntAtIRQLNonSpurious(BusInterruptLevel, Irql);
		}
		else {
			return HalpCheckMaskedIntAtIRQLLevelNonSpurious(BusInterruptLevel, Irql);
		}
	}
	outb(PIC_MASTER_CMD, OCW2_EOI_IRQ | 2); // send eoi to master pic
	enable();

	return 1;
}

static constexpr ULONG(FASTCALL *const HalpCheckMaskedIntAtIRQL[])(ULONG BusInterruptLevel, ULONG Irql) = {
	// Check level-triggered interrupts
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLSpurious7<LevelSensitive>,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLLevelNonSpurious,
	HalpCheckMaskedIntAtIRQLSpurious15<LevelSensitive>,
	// Check edge-triggered interrupts
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLSpurious7<Edge>,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLNonSpurious,
	HalpCheckMaskedIntAtIRQLSpurious15<Edge>,
};

VOID __declspec(naked) XBOXAPI HalpInterruptCommon()
{
	// On entry, interrupts must be disabled
	// This function uses a custom calling convention, so never call it from C++ code
	// esi -> Interrupt

	ASM_BEGIN
		ASM(mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2);
		ASM(inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
		ASM(movzx ebx, byte ptr [KiPcr]KPCR.Irql);
		ASM(movzx eax, byte ptr [esi]KINTERRUPT.Mode);
		ASM(shl eax, 6);
		ASM(mov ecx, dword ptr [esi]KINTERRUPT.BusInterruptLevel);
		ASM(mov edx, dword ptr [esi]KINTERRUPT.Irql);
		ASM(call HalpCheckMaskedIntAtIRQL[eax + ecx * 4]);
		ASM(test eax, eax);
		ASM(jz valid_int);
		ASM(cli);
		ASM(jmp end_isr);
	valid_int:
		ASM(push dword ptr [esi]KINTERRUPT.ServiceContext);
		ASM(push esi);
		ASM(call dword ptr [esi]KINTERRUPT.ServiceRoutine); // call ISR of the interrupt
		ASM(cli);
		ASM(movzx eax, byte ptr [esi]KINTERRUPT.Mode);
		ASM(test eax, eax);
		ASM(jnz check_unmasked_int); // only send eoi if level-triggered
		ASM(mov eax, dword ptr [esi]KINTERRUPT.BusInterruptLevel);
		ASM(cmp eax, 8);
		ASM(jae eoi_to_slave);
		ASM(or eax, OCW2_EOI_IRQ);
		ASM(out PIC_MASTER_CMD, al);
		ASM(jmp check_unmasked_int);
	eoi_to_slave:
		ASM(add eax, OCW2_EOI_IRQ - 8);
		ASM(out PIC_SLAVE_CMD, al);
	check_unmasked_int:
		ASM(mov byte ptr [KiPcr]KPCR.Irql, bl); // lower IRQL
		ASM(call HalpCheckUnmaskedInt);
	end_isr:
		EXIT_INTERRUPT;
	ASM_END
}

VOID __declspec(naked) XBOXAPI HalpClockIsr()
{
	ASM_BEGIN
		CREATE_KTRAP_FRAME_FOR_INT;
		ASM(movzx eax, byte ptr [KiPcr]KPCR.Irql);
		ASM(cmp eax, CLOCK_LEVEL);
		ASM(jge masked_int);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, CLOCK_LEVEL); // raise IRQL
		ASM(push eax);
		ASM(mov al, OCW2_EOI_IRQ);
		ASM(out PIC_MASTER_CMD, al); // send eoi to master pic
		// Query the total execution time and clock increment. If we instead just increment the time with the xbox increment, if the host
		// doesn't manage to call this every ms, then the time will start to lag behind the system clock time read from the CMOS, which in turn is synchronized
		// with the current host time
		ASM(mov edx, KE_CLOCK_INCREMENT_LOW);
		ASM(in eax, dx);
		ASM(mov esi, eax);
		ASM(inc edx);
		ASM(in eax, dx);
		ASM(mov edi, eax);
		ASM(inc edx);
		ASM(in eax, dx);
		ASM(sti);
		ASM(mov ecx, dword ptr [KeInterruptTime]KSYSTEM_TIME.LowTime);
		ASM(mov edx, dword ptr [KeInterruptTime]KSYSTEM_TIME.HighTime);
		ASM(add ecx, esi);
		ASM(adc edx, edi);
		ASM(mov dword ptr [KeInterruptTime]KSYSTEM_TIME.High2Time, edx);
		ASM(mov dword ptr [KeInterruptTime]KSYSTEM_TIME.LowTime, ecx);
		ASM(mov dword ptr [KeInterruptTime]KSYSTEM_TIME.HighTime, edx);
		ASM(mov ecx, dword ptr [KeSystemTime]KSYSTEM_TIME.LowTime);
		ASM(mov edx, dword ptr [KeSystemTime]KSYSTEM_TIME.HighTime);
		ASM(add ecx, esi);
		ASM(adc edx, edi);
		ASM(mov dword ptr [KeSystemTime]KSYSTEM_TIME.High2Time, edx);
		ASM(mov dword ptr [KeSystemTime]KSYSTEM_TIME.LowTime, ecx);
		ASM(mov dword ptr [KeSystemTime]KSYSTEM_TIME.HighTime, edx);
		ASM(mov ebx, KeTickCount);
		ASM(mov KeTickCount, eax);
		ASM(mov edi, eax);
		ASM(mov ecx, ebx);
		ASM(call KiCheckExpiredTimers);
		ASM(sub edi, ebx); // ms elapsed since the last clock interrupt
		ASM(inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
		ASM(mov ecx, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(cmp byte ptr [esp], DISPATCH_LEVEL);
		ASM(jb kernel_time);
		ASM(ja interrupt_time);
		ASM(cmp dword ptr [KiPcr]KPCR.PrcbData.DpcRoutineActive, 0);
		ASM(jz kernel_time);
		ASM(add dword ptr [KiPcr]KPCR.PrcbData.DpcTime, edi); // DpcTime: time spent executing DPCs, in ms
		ASM(jmp quantum);
	interrupt_time:
		ASM(add dword ptr [KiPcr]KPCR.PrcbData.InterruptTime, edi); // InterruptTime: time spent executing code at IRQL > 2, in ms
		ASM(jmp quantum);
	kernel_time:
		ASM(add dword ptr [ecx]KTHREAD.KernelTime, edi); // KernelTime: per-thread time spent executing code at IRQL < 2, in ms
	quantum:
		ASM(mov eax, CLOCK_QUANTUM_DECREMENT);
		ASM(mul edi); // scale ms with the clock decrement
		ASM(sub dword ptr [ecx]KTHREAD.Quantum, eax);
		ASM(jg not_expired);
		ASM(cmp ecx, offset KiIdleThread); // if it's the idle thread, then don't switch
		ASM(jz not_expired);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 1);
		ASM(mov cl, DISPATCH_LEVEL);
		ASM(call HalRequestSoftwareInterrupt);
	not_expired:
		ASM(cli);
		ASM(pop ecx);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, cl); // lower IRQL
		ASM(call HalpCheckUnmaskedInt);
		ASM(jmp end_isr);
	masked_int:
		ASM(mov edx, 1);
		ASM(mov ecx, IRQL_OFFSET_FOR_IRQ); // clock IRQ is zero
		ASM(shl edx, cl);
		ASM(or HalpPendingInt, edx); // save masked int in sw IRR, so that we can deliver it later when the IRQL goes down
		ASM(mov ax, PicIRQMasksForIRQL[eax * 2]);
		ASM(or ax, HalpIntDisabled); // mask all IRQs on the PIC with IRQL <= than current IRQL
		ASM(out PIC_MASTER_DATA, al);
		ASM(shr ax, 8);
		ASM(out PIC_SLAVE_DATA, al);
	end_isr:
		EXIT_INTERRUPT;
	ASM_END
}

VOID __declspec(naked) XBOXAPI HalpSmbusIsr()
{
	ASM_BEGIN
		CREATE_KTRAP_FRAME_FOR_INT;
		ASM(mov al, OCW2_EOI_IRQ | 2);
		ASM(out PIC_MASTER_CMD, al); // send eoi to master pic
		ASM(movzx eax, byte ptr [KiPcr]KPCR.Irql);
		ASM(cmp eax, SMBUS_LEVEL);
		ASM(jge masked_int);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, SMBUS_LEVEL); // raise IRQL
		ASM(push eax);
		ASM(sti);
		ASM(inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
		ASM(xor eax, eax);
		ASM(mov edx, SMBUS_STATUS);
		ASM(in al, dx);
		ASM(out dx, al); // clear status bits on smbus to dismiss the interrupt
		ASM(push 0);
		ASM(push eax);
		ASM(push offset HalpSmbusDpcObject);
		ASM(call KeInsertQueueDpc);
		ASM(cli);
		ASM(mov eax, 11 + OCW2_EOI_IRQ - 8);
		ASM(out PIC_SLAVE_CMD, al); // send eoi to slave pic
		ASM(pop eax);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, al); // lower IRQL
		ASM(call HalpCheckUnmaskedInt);
		ASM(jmp end_isr);
	masked_int:
		ASM(mov edx, 1);
		ASM(mov ecx, 11 + IRQL_OFFSET_FOR_IRQ); // smbus IRQ is eleven
		ASM(shl edx, cl);
		ASM(or HalpPendingInt, edx); // save masked int in sw IRR, so that we can deliver it later when the IRQL goes down
		ASM(mov ax, PicIRQMasksForIRQL[eax * 2]);
		ASM(or ax, HalpIntDisabled); // mask all IRQs on the PIC with IRQL <= than current IRQL
		ASM(out PIC_MASTER_DATA, al);
		ASM(shr ax, 8);
		ASM(out PIC_SLAVE_DATA, al);
	end_isr:
		EXIT_INTERRUPT;
	ASM_END
}

VOID __declspec(naked) XBOXAPI HalpCmosIsr()
{
	// NOTE: this is only called during kernel initialization, so we ignore the IRQL stuff here and don't save a KTRAP_FRAME on the stack either

	ASM_BEGIN
		ASM(mov al, OCW2_EOI_IRQ | 2);
		ASM(out PIC_MASTER_CMD, al); // send eoi to master pic
		ASM(mov al, 0x0C);
		ASM(out CMOS_PORT_CMD, al);
		ASM(in al, CMOS_PORT_DATA); // clear interrupt flags
		ASM(cmp HalpCmosIsrFirstCall, TRUE);
		ASM(jz first_interrupt);
		ASM(pop eax); // discard previous ret eip pushed by cpu
		ASM(mov eax, HalpCmosLoopStartAddress);
		ASM(add eax, 6 + 1 + 3 + 2 + 10); // mov ret eip to skip (jmp init_failed) of HalpCalibrateStallExecution
		ASM(jmp second_interrupt);
	first_interrupt:
		ASM(mov HalpCmosIsrFirstCall, FALSE);
		ASM(mov ecx, 0); // discard the value and restart the counter
		ASM(pop eax); // discard previous ret eip pushed by the cpu
		ASM(mov eax, HalpCmosLoopStartAddress);
		ASM(add eax, 6 + 1); // mov ret eip to return to loop_start label of HalpCalibrateStallExecution
	second_interrupt:
		ASM(push eax);
		ASM(mov eax, 8 + OCW2_EOI_IRQ - 8);
		ASM(out PIC_SLAVE_CMD, al); // send eoi to slave pic
		ASM(iretd);
	ASM_END
}

BOOLEAN HalpCalibrateStallExecution()
{
	// NOTE: this is called after initializing the PIC, but before setting up the PIT and SMBUS, so we can only receive a CMOS interrupt here

	HalpIntDisabled &= ~(1 << 8); // CMOS IRQ is 8
	BYTE PicImr = (HalpIntDisabled >> 8) & 0xFF;
	BYTE CurrElcr = inb(PIC_SLAVE_ELCR);
	BYTE ElcrMask = 1 << (8 & 7);
	CurrElcr |= ElcrMask; // CMOS interrupt is level sensitive
	outb(PIC_SLAVE_ELCR, CurrElcr);
	outb(PIC_SLAVE_DATA, PicImr);
	KiIdt[IDT_INT_VECTOR_BASE + 8] = BUILD_IDT_ENTRY(HalpCmosIsr);

	BYTE OldRegisterA = HalpReadCmosRegister(0x0A);
	BYTE OldRegisterB = HalpReadCmosRegister(0x0B);
	HalpReadCmosRegister(0x0C); // clear existing interrupts
	BYTE NewRegisterA = 0b00101101; // select 32.768 KHz freq and 125 ms periodic interrupt rate
	BYTE NewRegisterB = (OldRegisterB | (1 << 6)); // enable periodic interrupt

	HalpWriteCmosRegister(0x0A, NewRegisterA);
	HalpWriteCmosRegister(0x0B, NewRegisterB);

	DWORD LoopCount = 0;

	ASM_BEGIN
		ASM(mov ecx, LoopCount);
		ASM(push next_instr);
	next_instr:
		ASM(pop HalpCmosLoopStartAddress);
		ASM(sti); // ready to receive CMOS interrupt now
	loop_start:
		ASM(sub ecx, 1);
		ASM(jnz loop_start);
		ASM(mov HalpCounterOverflow, 1);
		ASM(cli);
		ASM(mov LoopCount, ecx);
	ASM_END

	if (HalpCounterOverflow) {
		return FALSE;
	}

	HalpWriteCmosRegister(0x0B, OldRegisterB & 7); // disable all CMOS interrupts
	HalpWriteCmosRegister(0x0A, OldRegisterA);
	HalpIntDisabled |= (1 << 8);
	PicImr = (HalpIntDisabled >> 8) & 0xFF;
	outb(PIC_SLAVE_DATA, PicImr); // mask CMOS IRQ again
	KiIdt[IDT_INT_VECTOR_BASE + 8] = 0; // make CMOS IDT entry invalid again

	LoopCount = 0 - LoopCount;
	HalCounterPerMicroseconds = LoopCount / 125000;
	HalCounterPerMicroseconds += ((LoopCount % 125000) ? 1 : 0);

	return TRUE;
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

	CurrElcr = inb(ElcrPort);

	if (InterruptMode == Edge) {
		CurrElcr &= ~ElcrMask;
	}
	else {
		CurrElcr |= ElcrMask;
	}

	outb(ElcrPort, CurrElcr);
	outb(DataPort, PicImr);
	enable();
}

EXPORTNUM(44) ULONG XBOXAPI HalGetInterruptVector
(
	ULONG BusInterruptLevel,
	PKIRQL Irql
)
{
	ULONG IdtVector = BusInterruptLevel + IDT_INT_VECTOR_BASE;
	if ((IdtVector < IDT_INT_VECTOR_BASE) || (IdtVector > (IDT_INT_VECTOR_BASE + MAX_BUS_INTERRUPT_LEVEL))) {
		return 0;
	}

	*Irql = MAX_BUS_INTERRUPT_LEVEL - BusInterruptLevel;
	return IdtVector;
}

EXPORTNUM(48) VOID FASTCALL HalRequestSoftwareInterrupt
(
	KIRQL Request
)
{
	assert((Request == APC_LEVEL) || (Request == DISPATCH_LEVEL));

	DWORD OldEflags = save_int_state_and_disable();

	HalpPendingInt |= (1 << Request);
	if (HalpIrqlMasks[KiPcr.Irql] & (1 << Request)) { // is the requested IRQL unmasked at the current IRQL?
		SwIntHandlers[Request]();
	}

	restore_int_state(OldEflags);
}
