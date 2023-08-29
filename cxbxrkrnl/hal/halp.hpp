/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\ki.hpp"
#include "..\ki\hw_exp.hpp"


// This is used to track sw/hw interrupts currently pending. First four are for sw interrupts,
// while the remaining 16 are for the hw interrupts of the 8259 PIC
inline volatile DWORD HalpInterruptRequestRegister = 0;

inline volatile const DWORD HalpIrqlMasks[] = {
	0xFFFFFFFE, // IRQL 0 (PASSIVE_LEVEL)
	0xFFFFFFFC, // IRQL 1 (APC_LEVEL)
	0xFFFFFFF8, // IRQL 2 (DISPATCH_LEVEL)
	0xFFFFFFF0, // IRQL 3
	0x03FFFFF0, // IRQL 4
	0x01FFFFF0, // IRQL 5
	0x00FFFFF0, // IRQL 6
	0x007FFFF0, // IRQL 7
	0x003FFFF0, // IRQL 8
	0x001FFFF0, // IRQL 9
	0x000FFFF0, // IRQL 10
	0x0007FFF0, // IRQL 11
	0x0003FFF0, // IRQL 12
	0x0001FFF0, // IRQL 13
	0x0001FFF0, // IRQL 14
	0x00017FF0, // IRQL 15
	0x00013FF0, // IRQL 16
	0x00011FF0, // IRQL 17
	0x00011FF0, // IRQL 18
	0x000117F0, // IRQL 19
	0x000113F0, // IRQL 20
	0x000111F0, // IRQL 21
	0x000110F0, // IRQL 22
	0x00011070, // IRQL 23
	0x00011030, // IRQL 24
	0x00011010, // IRQL 25
	0x00010010, // IRQL 26 (PROFILE_LEVEL)
	0x00000010, // IRQL 27
	0x00000000, // IRQL 28 (SYNC_LEVEL)
	0x00000000, // IRQL 29
	0x00000000, // IRQL 30
	0x00000000, // IRQL 31 (HIGH_LEVEL)
};

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

inline VOID(XBOXAPI* const SwIntHandlers[])() = {
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
