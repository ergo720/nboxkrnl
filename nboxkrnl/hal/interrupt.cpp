/*
 * ergo720                Copyright (c) 2022
 */

#include "hal.hpp"
#include "halp.hpp"
#include "rtl.hpp"
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

	ASM_BEGIN
		ASM(movzx eax, byte ptr[KiPcr]KPCR.Irql);
		ASM(mov byte ptr[KiPcr]KPCR.Irql, APC_LEVEL); // raise IRQL
		ASM(and HalpPendingInt, ~(1 << APC_LEVEL));
		ASM(push eax);
		ASM(sti);
		ASM(call KiExecuteApcQueue);
		ASM(cli);
		ASM(pop eax);
		ASM(mov byte ptr[KiPcr]KPCR.Irql, al); // lower IRQL
		ASM(call HalpCheckUnmaskedInt);
	ASM_END
}

VOID __declspec(naked) XBOXAPI HalpSwIntDpc()
{
	// On entry, interrupts must be disabled

	ASM_BEGIN
		ASM(movzx eax, byte ptr [KiPcr]KPCR.Irql);
		ASM(mov byte ptr [KiPcr]KPCR.Irql, DISPATCH_LEVEL); // raise IRQL
		ASM(and HalpPendingInt, ~(1 << DISPATCH_LEVEL));
		ASM(push eax);
		ASM(lea eax, [KiPcr]KPCR.PrcbData.DpcListHead);
		ASM(cmp eax, [eax]LIST_ENTRY.Flink);
		ASM(jz no_dpc);
		ASM(push [KiPcr]KPCR.NtTib.ExceptionList);
		ASM(mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2); // dword ptr required or else MSVC will aceess ExceptionList as a byte
		ASM(mov eax, esp);
		ASM(mov esp, [KiPcr]KPCR.PrcbData.DpcStack); // switch to DPC stack
		ASM(push eax);
		ASM(call KiExecuteDpcQueue);
		ASM(pop esp);
		ASM(pop dword ptr [KiPcr]KPCR.NtTib.ExceptionList); // dword ptr required or else MSVC will aceess ExceptionList as a byte
	no_dpc:
		ASM(sti);
		ASM(cmp [KiPcr]KPCR.PrcbData.QuantumEnd, 0);
		ASM(jnz quantum_end);
		ASM(mov eax, [KiPcr]KPCR.PrcbData.NextThread);
		ASM(test eax, eax);
		ASM(jnz thread_switch);
		ASM(jmp end_func);
	thread_switch:
		ASM(push esi);
		ASM(push edi);
		ASM(push ebx);
		ASM(push ebp);
		ASM(mov edi, eax);
		ASM(mov esi, [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(mov ecx, esi);
		ASM(call KeAddThreadToTailOfReadyList);
		ASM(mov [KiPcr]KPCR.PrcbData.CurrentThread, edi);
		ASM(mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0); // dword ptr required or else MSVC will aceess NextThread as a byte
		ASM(mov ebx, 1);
		ASM(call KiSwapThreadContext); // when this returns, it means this thread was switched back again
		ASM(pop ebp);
		ASM(pop ebx);
		ASM(pop edi);
		ASM(pop esi);
		ASM(jmp end_func);
	quantum_end:
		ASM(mov [KiPcr]KPCR.PrcbData.QuantumEnd, 0);
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

VOID __declspec(naked) HalpCheckUnmaskedInt()
{
	// On entry, interrupts must be disabled

	ASM_BEGIN
	check_int:
		ASM(movzx ecx, byte ptr [KiPcr]KPCR.Irql);
		ASM(mov edx, HalpPendingInt);
		ASM(and edx, HalpIrqlMasks[ecx * 4]); // if not zero, then there are one or more pending interrupts that have become unmasked
		ASM(jnz unmasked_int);
		ASM(jmp exit_func);
	unmasked_int:
		ASM(test HalpIntInProgress, ACTIVE_IRQ_MASK); // make sure we complete the active IRQ first
		ASM(jnz exit_func);
		ASM(bsr ecx, edx);
		ASM(cmp ecx, DISPATCH_LEVEL);
		ASM(jg hw_int);
		ASM(call SwIntHandlers[ecx * 4]);
		ASM(jmp check_int);
	hw_int:
		ASM(mov ax, HalpIntDisabled);
		ASM(out PIC_MASTER_DATA, al);
		ASM(shr ax, 8);
		ASM(out PIC_SLAVE_DATA, al);
		ASM(mov edx, 1);
		ASM(shl edx, cl);
		ASM(test HalpIntInProgress, edx); // check again HalpIntInProgress because if a sw/hw int comes, it re-enables interrupts, and a hw int could come again
		ASM(jnz exit_func);
		ASM(or HalpIntInProgress, edx);
		ASM(xor HalpPendingInt, edx);
		ASM(call SwIntHandlers[ecx * 4]);
		ASM(xor HalpIntInProgress, edx);
		ASM(jmp check_int);
	exit_func:
		ASM(ret);
	ASM_END
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
		ASM(inc [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
		ASM(movzx ebx, byte ptr [KiPcr]KPCR.Irql);
		ASM(movzx eax, byte ptr [esi]KINTERRUPT.Mode);
		ASM(shl eax, 6);
		ASM(mov ecx, [esi]KINTERRUPT.BusInterruptLevel);
		ASM(mov edx, [esi]KINTERRUPT.Irql);
		ASM(call HalpCheckMaskedIntAtIRQL[eax + ecx * 4]);
		ASM(test eax, eax);
		ASM(jz valid_int);
		ASM(cli);
		ASM(jmp end_isr);
	valid_int:
		ASM(push [esi]KINTERRUPT.ServiceContext);
		ASM(push esi);
		ASM(call [esi]KINTERRUPT.ServiceRoutine); // call ISR of the interrupt
		ASM(cli);
		ASM(movzx eax, byte ptr [esi]KINTERRUPT.Mode);
		ASM(test eax, eax);
		ASM(jnz check_unmasked_int); // only send eoi if level-triggered
		ASM(mov eax, [esi]KINTERRUPT.BusInterruptLevel);
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
		ASM(mov ecx, [KeInterruptTime]KSYSTEM_TIME.LowTime);
		ASM(mov edx, [KeInterruptTime]KSYSTEM_TIME.HighTime);
		ASM(add ecx, esi);
		ASM(adc edx, edi);
		ASM(mov [KeInterruptTime]KSYSTEM_TIME.High2Time, edx);
		ASM(mov [KeInterruptTime]KSYSTEM_TIME.LowTime, ecx);
		ASM(mov [KeInterruptTime]KSYSTEM_TIME.HighTime, edx);
		ASM(mov ecx, [KeSystemTime]KSYSTEM_TIME.LowTime);
		ASM(mov edx, [KeSystemTime]KSYSTEM_TIME.HighTime);
		ASM(add ecx, esi);
		ASM(adc edx, edi);
		ASM(mov [KeSystemTime]KSYSTEM_TIME.High2Time, edx);
		ASM(mov [KeSystemTime]KSYSTEM_TIME.LowTime, ecx);
		ASM(mov [KeSystemTime]KSYSTEM_TIME.HighTime, edx);
		ASM(mov ebx, KeTickCount);
		ASM(mov KeTickCount, eax);
		ASM(mov edi, eax);
		ASM(mov ecx, ebx);
		ASM(call KiCheckExpiredTimers);
		ASM(sub edi, ebx); // ms elapsed since the last clock interrupt
		ASM(inc [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
		ASM(mov ecx, [KiPcr]KPCR.PrcbData.CurrentThread);
		ASM(cmp byte ptr [esp], DISPATCH_LEVEL);
		ASM(jb kernel_time);
		ASM(ja interrupt_time);
		ASM(cmp [KiPcr]KPCR.PrcbData.DpcRoutineActive, 0);
		ASM(jz kernel_time);
		ASM(add [KiPcr]KPCR.PrcbData.DpcTime, edi); // DpcTime: time spent executing DPCs, in ms
		ASM(jmp quantum);
	interrupt_time:
		ASM(add [KiPcr]KPCR.PrcbData.InterruptTime, edi); // InterruptTime: time spent executing code at IRQL > 2, in ms
		ASM(jmp quantum);
	kernel_time:
		ASM(add [ecx]KTHREAD.KernelTime, edi); // KernelTime: per-thread time spent executing code at IRQL < 2, in ms
	quantum:
		ASM(mov eax, CLOCK_QUANTUM_DECREMENT);
		ASM(mul edi); // scale ms with the clock decrement
		ASM(sub [ecx]KTHREAD.Quantum, eax);
		ASM(jg not_expired);
		ASM(cmp ecx, offset KiIdleThread); // if it's the idle thread, then don't switch
		ASM(jz not_expired);
		ASM(mov [KiPcr]KPCR.PrcbData.QuantumEnd, 1);
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
		ASM(inc [KiPcr]KPCR.PrcbData.InterruptCount); // InterruptCount: number of interrupts that have occurred
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

	ASM_BEGIN
		ASM(mov edx, ElcrPort);
		ASM(in al, dx);
		ASM(mov CurrElcr, al);
	ASM_END

	if (InterruptMode == Edge) {
		CurrElcr &= ~ElcrMask;
	}
	else {
		CurrElcr |= ElcrMask;
	}

	ASM_BEGIN
		ASM(mov edx, ElcrPort);
		ASM(mov al, CurrElcr);
		ASM(out dx, al);
		ASM(mov al, PicImr);
		ASM(mov edx, DataPort);
		ASM(out dx, al);
		ASM(sti);
	ASM_END
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

	ASM_BEGIN
		ASM(pushfd);
		ASM(cli);
	ASM_END

	HalpPendingInt |= (1 << Request);
	if (HalpIrqlMasks[KiPcr.Irql] & (1 << Request)) { // is the requested IRQL unmasked at the current IRQL?
		SwIntHandlers[Request]();
	}

	ASM(popfd);
}
