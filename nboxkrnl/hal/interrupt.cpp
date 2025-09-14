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

	// clang-format off
	__asm {
		movzx eax, byte ptr [KiPcr]KPCR.Irql
		mov byte ptr [KiPcr]KPCR.Irql, DISPATCH_LEVEL // raise IRQL
		and HalpPendingInt, ~(1 << DISPATCH_LEVEL)
		push eax
		lea eax, dword ptr [KiPcr]KPCR.PrcbData.DpcListHead
		cmp eax, dword ptr [eax]LIST_ENTRY.Flink
		jz no_dpc
		push dword ptr [KiPcr]KPCR.NtTib.ExceptionList
		mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2
		mov eax, esp
		mov esp, dword ptr [KiPcr]KPCR.PrcbData.DpcStack // switch to DPC stack
		push eax
		call KiExecuteDpcQueue
		pop esp
		pop dword ptr [KiPcr]KPCR.NtTib.ExceptionList
	no_dpc:
		sti
		cmp dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 0
		jnz quantum_end
		mov eax, dword ptr [KiPcr]KPCR.PrcbData.NextThread
		test eax, eax
		jnz thread_switch
		jmp end_func
	thread_switch:
		push esi
		push edi
		push ebx
		push ebp
		mov edi, eax
		mov esi, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread
		mov ecx, esi
		call KeAddThreadToTailOfReadyList
		mov dword ptr [KiPcr]KPCR.PrcbData.CurrentThread, edi
		mov dword ptr [KiPcr]KPCR.PrcbData.NextThread, 0
		mov ebx, 1
		call KiSwapThreadContext // when this returns, it means this thread was switched back again
		pop ebp
		pop ebx
		pop edi
		pop esi
		jmp end_func
	quantum_end:
		mov dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 0
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
	// clang-format on
}

// clang-format off
VOID XBOXAPI HalpHwInt0()
{
	__asm int IDT_INT_VECTOR_BASE + 0
}

VOID XBOXAPI HalpHwInt1()
{
	__asm int IDT_INT_VECTOR_BASE + 1
}

VOID XBOXAPI HalpHwInt2()
{
	__asm int IDT_INT_VECTOR_BASE + 2
}

VOID XBOXAPI HalpHwInt3()
{
	__asm int IDT_INT_VECTOR_BASE + 3
}

VOID XBOXAPI HalpHwInt4()
{
	__asm int IDT_INT_VECTOR_BASE + 4
}

VOID XBOXAPI HalpHwInt5()
{
	__asm int IDT_INT_VECTOR_BASE + 5
}

VOID XBOXAPI HalpHwInt6()
{
	__asm int IDT_INT_VECTOR_BASE + 6
}

VOID XBOXAPI HalpHwInt7()
{
	__asm int IDT_INT_VECTOR_BASE + 7
}

VOID XBOXAPI HalpHwInt8()
{
	__asm int IDT_INT_VECTOR_BASE + 8
}

VOID XBOXAPI HalpHwInt9()
{
	__asm int IDT_INT_VECTOR_BASE + 9
}

VOID XBOXAPI HalpHwInt10()
{
	__asm int IDT_INT_VECTOR_BASE + 10
}

VOID XBOXAPI HalpHwInt11()
{
	__asm int IDT_INT_VECTOR_BASE + 11
}

VOID XBOXAPI HalpHwInt12()
{
	__asm int IDT_INT_VECTOR_BASE + 12
}

VOID XBOXAPI HalpHwInt13()
{
	__asm int IDT_INT_VECTOR_BASE + 13
}

VOID XBOXAPI HalpHwInt14()
{
	__asm int IDT_INT_VECTOR_BASE + 14
}

VOID XBOXAPI HalpHwInt15()
{
	__asm int IDT_INT_VECTOR_BASE + 15
}
// clang-format on

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

	// clang-format off
	__asm {
		mov dword ptr [KiPcr]KPCR.NtTib.ExceptionList, EXCEPTION_CHAIN_END2
		inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount // InterruptCount: number of interrupts that have occurred
		movzx ebx, byte ptr [KiPcr]KPCR.Irql
		movzx eax, byte ptr [esi]KINTERRUPT.Mode
		shl eax, 6
		mov ecx, dword ptr [esi]KINTERRUPT.BusInterruptLevel
		mov edx, dword ptr [esi]KINTERRUPT.Irql
		call HalpCheckMaskedIntAtIRQL[eax + ecx * 4]
		test eax, eax
		jz valid_int
		cli
		jmp end_isr
	valid_int:
		push dword ptr [esi]KINTERRUPT.ServiceContext
		push esi
		call dword ptr [esi]KINTERRUPT.ServiceRoutine // call ISR of the interrupt
		cli
		movzx eax, byte ptr [esi]KINTERRUPT.Mode
		test eax, eax
		jnz check_unmasked_int // only send eoi if level-triggered
		mov eax, dword ptr [esi]KINTERRUPT.BusInterruptLevel
		cmp eax, 8
		jae eoi_to_slave
		or eax, OCW2_EOI_IRQ
		out PIC_MASTER_CMD, al
		jmp check_unmasked_int
	eoi_to_slave:
		add eax, OCW2_EOI_IRQ - 8
		out PIC_SLAVE_CMD, al
	check_unmasked_int:
		mov byte ptr [KiPcr]KPCR.Irql, bl // lower IRQL
		call HalpCheckUnmaskedInt
	end_isr:
		EXIT_INTERRUPT
	}
	// clang-format on
}

VOID __declspec(naked) XBOXAPI HalpClockIsr()
{
	// clang-format off
	__asm {
		CREATE_KTRAP_FRAME_FOR_INT
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
		mov ecx, dword ptr [KeInterruptTime]KSYSTEM_TIME.LowTime
		mov edx, dword ptr [KeInterruptTime]KSYSTEM_TIME.HighTime
		add ecx, esi
		adc edx, edi
		mov dword ptr [KeInterruptTime]KSYSTEM_TIME.High2Time, edx
		mov dword ptr [KeInterruptTime]KSYSTEM_TIME.LowTime, ecx
		mov dword ptr [KeInterruptTime]KSYSTEM_TIME.HighTime, edx
		mov ecx, dword ptr [KeSystemTime]KSYSTEM_TIME.LowTime
		mov edx, dword ptr [KeSystemTime]KSYSTEM_TIME.HighTime
		add ecx, esi
		adc edx, edi
		mov dword ptr [KeSystemTime]KSYSTEM_TIME.High2Time, edx
		mov dword ptr [KeSystemTime]KSYSTEM_TIME.LowTime, ecx
		mov dword ptr [KeSystemTime]KSYSTEM_TIME.HighTime, edx
		mov ebx, KeTickCount
		mov KeTickCount, eax
		mov edi, eax
		mov ecx, ebx
		call KiCheckExpiredTimers
		sub edi, ebx // ms elapsed since the last clock interrupt
		inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount // InterruptCount: number of interrupts that have occurred
		mov ecx, dword ptr [KiPcr]KPCR.PrcbData.CurrentThread
		cmp byte ptr [esp], DISPATCH_LEVEL
		jb kernel_time
		ja interrupt_time
		cmp dword ptr [KiPcr]KPCR.PrcbData.DpcRoutineActive, 0
		jz kernel_time
		add dword ptr [KiPcr]KPCR.PrcbData.DpcTime, edi // DpcTime: time spent executing DPCs, in ms
		jmp quantum
	interrupt_time:
		add dword ptr [KiPcr]KPCR.PrcbData.InterruptTime, edi // InterruptTime: time spent executing code at IRQL > 2, in ms
		jmp quantum
	kernel_time:
		add dword ptr [ecx]KTHREAD.KernelTime, edi // KernelTime: per-thread time spent executing code at IRQL < 2, in ms
	quantum:
		mov eax, CLOCK_QUANTUM_DECREMENT
		mul edi // scale ms with the clock decrement
		sub dword ptr [ecx]KTHREAD.Quantum, eax
		jg not_expired
		cmp ecx, offset KiIdleThread // if it's the idle thread, then don't switch
		jz not_expired
		mov dword ptr [KiPcr]KPCR.PrcbData.QuantumEnd, 1
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
		EXIT_INTERRUPT
	}
	// clang-format on
}

VOID __declspec(naked) XBOXAPI HalpSmbusIsr()
{
	// clang-format off
	__asm {
		CREATE_KTRAP_FRAME_FOR_INT
		mov al, OCW2_EOI_IRQ | 2
		out PIC_MASTER_CMD, al // send eoi to master pic
		movzx eax, byte ptr [KiPcr]KPCR.Irql
		cmp eax, SMBUS_LEVEL
		jge masked_int
		mov byte ptr [KiPcr]KPCR.Irql, SMBUS_LEVEL // raise IRQL
		push eax
		sti
		inc dword ptr [KiPcr]KPCR.PrcbData.InterruptCount // InterruptCount: number of interrupts that have occurred
		xor eax, eax
		mov edx, SMBUS_STATUS
		in al, dx
		out dx, al // clear status bits on smbus to dismiss the interrupt
		push 0
		push eax
		push offset HalpSmbusDpcObject
		call KeInsertQueueDpc
		cli
		mov eax, 11 + OCW2_EOI_IRQ - 8
		out PIC_SLAVE_CMD, al // send eoi to slave pic
		pop eax
		mov byte ptr [KiPcr]KPCR.Irql, al // lower IRQL
		call HalpCheckUnmaskedInt
		jmp end_isr
	masked_int:
		mov edx, 1
		mov ecx, 11 + IRQL_OFFSET_FOR_IRQ // smbus IRQ is eleven
		shl edx, cl
		or HalpPendingInt, edx // save masked int in sw IRR, so that we can deliver it later when the IRQL goes down
		mov ax, PicIRQMasksForIRQL[eax * 2]
		or ax, HalpIntDisabled // mask all IRQs on the PIC with IRQL <= than current IRQL
		out PIC_MASTER_DATA, al
		shr ax, 8
		out PIC_SLAVE_DATA, al
	end_isr:
		EXIT_INTERRUPT
	}
	// clang-format on
}

VOID __declspec(naked) XBOXAPI HalpCmosIsr()
{
	// NOTE: this is only called during kernel initialization, so we ignore the IRQL stuff here and don't save a KTRAP_FRAME on the stack either

	// clang-format off
	__asm {
		mov al, OCW2_EOI_IRQ | 2
		out PIC_MASTER_CMD, al // send eoi to master pic
		mov al, 0x0C
		out CMOS_PORT_CMD, al
		in al, CMOS_PORT_DATA // clear interrupt flags
		cmp HalpCmosIsrFirstCall, TRUE
		jz first_interrupt
		pop eax // discard previous ret eip pushed by cpu
		mov eax, HalpCmosLoopStartAddress
		add eax, 6 + 1 + 3 + 2 + 10 // mov ret eip to skip (jmp init_failed) of HalpCalibrateStallExecution
		jmp second_interrupt
	first_interrupt:
		mov HalpCmosIsrFirstCall, FALSE
		mov ecx, 0 // discard the value and restart the counter
		pop eax // discard previous ret eip pushed by the cpu
		mov eax, HalpCmosLoopStartAddress
		add eax, 6 + 1 // mov ret eip to return to loop_start label of HalpCalibrateStallExecution
	second_interrupt:
		push eax
		mov eax, 8 + OCW2_EOI_IRQ - 8
		out PIC_SLAVE_CMD, al // send eoi to slave pic
		iretd
	}
	// clang-format on
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

	// clang-format off
	__asm {
		mov ecx, LoopCount
		push next_instr
	next_instr:
		pop HalpCmosLoopStartAddress
		sti // ready to receive CMOS interrupt now
	loop_start:
		sub ecx, 1
		jnz loop_start
		mov HalpCounterOverflow, 1
		cli
		mov LoopCount, ecx
	}
	// clang-format on

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
