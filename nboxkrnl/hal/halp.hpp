/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"
#include "ki.hpp"
#include "hw_exp.hpp"

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

// SMBUS ports
#define SMBUS_STATUS 0xC000
#define SMBUS_CONTROL 0xC002
#define SMBUS_ADDRESS 0xC004
#define SMBUS_DATA 0xC006
#define SMBUS_COMMAND 0xC008
#define SMBUS_FIFO 0xC009

// SMBUS constants
#define GS_PRERR_STS (1 << 2) // cycle failed
#define GS_HCYC_STS (1 << 4) // cycle succeeded
#define GE_RW_BYTE 2
#define GE_RW_WORD 3
#define GE_RW_BLOCK 5
#define GE_HOST_STC (1 << 3) // set to start a cycle
#define GE_HCYC_EN (1 << 4) // set to enable interrupts
#define HA_RC (1 << 0) // set to specify a read/receive cycle

// SMBUS sw addresses
#define EEPROM_WRITE_ADDR 0xA8
#define EEPROM_READ_ADDR 0xA9
#define SMC_WRITE_ADDR 0x20
#define SMC_READ_ADDR 0x21
#define CONEXANT_WRITE_ADDR 0x8A
#define CONEXANT_READ_ADDR 0x8B

// SMC video mode values
#define SMC_VIDEO_MODE_COMMAND   0x04
#define SMC_VIDEO_MODE_SCART     0x00
#define SMC_VIDEO_MODE_HDTV      0x01
#define SMC_VIDEO_MODE_VGA       0x02
#define SMC_VIDEO_MODE_RFU       0x03
#define SMC_VIDEO_MODE_SVIDEO    0x04
#define SMC_VIDEO_MODE_STANDARD  0x06
#define SMC_VIDEO_MODE_NONE      0x07


inline KDPC HalpSmbusDpcObject;
inline struct SMBUS_CYCLE_INFO {
	NTSTATUS Status;
	BYTE Data[32];
	UCHAR BlockAmount;
	BOOLEAN IsWrite;
	KEVENT EventLock;
	KEVENT EventComplete;
} HalpSmbusCycleInfo;

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
VOID XBOXAPI HalpSmbusIsr();

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
VOID HalpInitSMCstate();
VOID HalpReadCmosTime(PTIME_FIELDS TimeFields);
VOID HalpCheckUnmaskedInt();
VOID XBOXAPI HalpSmbusDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
VOID HalpExecuteReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN ReadWordValue);
VOID HalpExecuteWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN WriteWordValue, ULONG DataValue);
VOID HalpExecuteBlockReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode);
VOID HalpExecuteBlockWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, PBYTE Data);
NTSTATUS HalpReadSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR ReadAmount, BYTE *Buffer);
NTSTATUS HalpWriteSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR WriteAmount, BYTE *Buffer);
