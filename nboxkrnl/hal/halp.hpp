/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2022
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#pragma once

#include "..\types.hpp"
#include "ki.hpp"
#include "hw_exp.hpp"


inline ULONG HalpTrayState = MAXULONG;
inline ULONG HalpTrayStateChangeCount = 0;
inline KDPC HalpSciDpcObject;
inline KDPC HalpSmbusDpcObject;

inline struct SMBUS_CYCLE_INFO {
	NTSTATUS Status;
	BYTE Data[32];
	UCHAR BlockAmount;
	BOOLEAN IsWrite;
	KEVENT EventLock; // used by both SMBUS and SCI
	KEVENT EventComplete; // only used by SMBUS
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
VOID XBOXAPI HalpSciIsr();

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
VOID XBOXAPI HalpSciDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
VOID XBOXAPI HalpSmbusDpcRoutine(PKDPC Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2);
VOID HalpExecuteReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN ReadWordValue);
VOID HalpExecuteWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, BOOLEAN WriteWordValue, ULONG DataValue);
VOID HalpExecuteBlockReadSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode);
VOID HalpExecuteBlockWriteSmbusCycle(UCHAR SlaveAddress, UCHAR CommandCode, PBYTE Data);
NTSTATUS HalpReadSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR ReadAmount, BYTE *Buffer);
NTSTATUS HalpWriteSMBusBlock(UCHAR SlaveAddress, UCHAR CommandCode, UCHAR WriteAmount, BYTE *Buffer);
VOID HalpServiceNextSMBusInterrupt();
BOOLEAN HalpCalibrateStallExecution();
BYTE HalpReadCmosRegister(BYTE Register);
VOID HalpWriteCmosRegister(BYTE Register, BYTE Data);
