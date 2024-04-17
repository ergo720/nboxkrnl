/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\ki.hpp"
#include "..\ki\hw_exp.hpp"

 // PIC i/o ports
#define PIC_MASTER_CMD          0x20
#define PIC_MASTER_DATA         0x21
#define PIC_MASTER_ELCR         0x4D0
#define PIC_SLAVE_CMD           0xA0
#define PIC_SLAVE_DATA          0xA1
#define PIC_SLAVE_ELCR          0x4D1

// PIC icw1 command bytes
#define ICW1_ICW4_NEEDED        0x01
#define ICW1_CASCADE            0x00
#define ICW1_INTERVAL8          0x00
#define ICW1_EDGE               0x00
#define ICW1_INIT               0x10

// PIC icw4 command bytes
#define ICW4_8086               0x01
#define ICW4_NORNAL_EOI         0x00
#define ICW4_NON_BUFFERED       0x00
#define ICW4_NOT_FULLY_NESTED   0x00

// PIC ocw2 command bytes
#define OCW2_EOI_IRQ            0x60

// PIC ocw3 command bytes
#define OCW3_READ_ISR           0x0B

// PIC irq base (icw2)
#define PIC_MASTER_VECTOR_BASE  IDT_INT_VECTOR_BASE
#define PIC_SLAVE_VECTOR_BASE   (IDT_INT_VECTOR_BASE + 8)

// PCI ports
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC


VOID XBOXAPI HalpSwIntApc();
VOID XBOXAPI HalpSwIntDpc();
VOID XBOXAPI HalpHwInt0();
VOID XBOXAPI HalpHwInt1();
VOID XBOXAPI HalpHwInt2();
VOID XBOXAPI HalpHwInt3();
VOID XBOXAPI HalpHwInt4();
VOID XBOXAPI HalpHwInt5();
VOID XBOXAPI HalpHwInt6();
VOID XBOXAPI HalpHwInt7();
VOID XBOXAPI HalpHwInt8();
VOID XBOXAPI HalpHwInt9();
VOID XBOXAPI HalpHwInt10();
VOID XBOXAPI HalpHwInt11();
VOID XBOXAPI HalpHwInt12();
VOID XBOXAPI HalpHwInt13();
VOID XBOXAPI HalpHwInt14();
VOID XBOXAPI HalpHwInt15();

VOID XBOXAPI HalpInterruptCommon();
VOID XBOXAPI HalpClockIsr();

inline constexpr VOID(XBOXAPI *const SwIntHandlers[])() = {
	&KiUnexpectedInterrupt,
	&HalpSwIntApc,
	&HalpSwIntDpc,
	&KiUnexpectedInterrupt,
	&HalpHwInt0,
	&HalpHwInt1,
	&HalpHwInt2,
	&HalpHwInt3,
	&HalpHwInt4,
	&HalpHwInt5,
	&HalpHwInt6,
	&HalpHwInt7,
	&HalpHwInt8,
	&HalpHwInt9,
	&HalpHwInt10,
	&HalpHwInt11,
	&HalpHwInt12,
	&HalpHwInt13,
	&HalpHwInt14,
	&HalpHwInt15
};

[[noreturn]] VOID HalpShutdownSystem();
VOID HalpInitPIC();
VOID HalpInitPIT();
VOID HalpReadCmosTime(PTIME_FIELDS TimeFields);
VOID HalpCheckUnmaskedInt();
