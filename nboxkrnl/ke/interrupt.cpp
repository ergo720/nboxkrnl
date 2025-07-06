/*
 * ergo720                Copyright (c) 2024
 */

#include "ki.hpp"
#include "halp.hpp"
#include "hal.hpp"
#include <string.h>
#include <assert.h>


// Same code as CREATE_KTRAP_FRAME_NO_CODE
static constexpr unsigned char KiInterruptEntryCode[] = {
	0x6a, 0x00,                                  // push 0x0
	0x55,                                        // push ebp
	0x53,                                        // push ebx
	0x56,                                        // push esi
	0x57,                                        // push edi
	0x64, 0x8b, 0x1d, 0x00, 0x00, 0x00, 0x00,    // mov ebx, DWORD PTR fs:0x0
	0x53,                                        // push ebx
	0x50,                                        // push eax
	0x51,                                        // push ecx
	0x52,                                        // push edx
	0x83, 0xec, 0x18,                            // sub esp, 0x18
	0x8b, 0xec,                                  // mov ebp, esp
	0xfc,                                        // cld
	0x8b, 0x5d, 0x34,                            // mov ebx, DWORD PTR [ebp+0x34]
	0x8b, 0x7d, 0x3c,                            // mov edi, DWORD PTR [ebp+0x3c]
	0xc7, 0x45, 0x0c, 0x00, 0x00, 0x00, 0x00,    // mov DWORD PTR [ebp+0xc], 0x0
	0xc7, 0x45, 0x08, 0xef, 0xbe, 0xad, 0xde,    // mov DWORD PTR [ebp+0x8], 0xDEADBEEF
	0x89, 0x7d, 0x04,                            // mov DWORD PTR [ebp+0x4], edi
	0x89, 0x5d, 0x00,                            // mov DWORD PTR [ebp+0x0], ebx
	0xbe, 0x00, 0x00, 0x00, 0x00,                // mov esi, 0x0 -> patched to mov esi, Interrupt
	0xb8, 0x00, 0x00, 0x00, 0x00,                // mov eax, 0x0 -> patched to mov eax, &HalpInterruptCommon
	0xff, 0xe0,                                  // jmp eax
};

static_assert(sizeof(KiInterruptEntryCode) <= sizeof(KINTERRUPT::DispatchCode));
static_assert(offsetof(KINTERRUPT, ServiceRoutine) == 0x00);
static_assert(offsetof(KINTERRUPT, ServiceContext) == 0x04);
static_assert(offsetof(KINTERRUPT, BusInterruptLevel) == 0x08);
static_assert(offsetof(KINTERRUPT, Irql) == 0x0C);
static_assert(offsetof(KINTERRUPT, Connected) == 0x10);
static_assert(offsetof(KINTERRUPT, ShareVector) == 0x11);
static_assert(offsetof(KINTERRUPT, Mode) == 0x12);
static_assert(offsetof(KINTERRUPT, ServiceCount) == 0x14);
static_assert(offsetof(KINTERRUPT, DispatchCode) == 0x18);


EXPORTNUM(98) BOOLEAN XBOXAPI KeConnectInterrupt
(
	PKINTERRUPT  InterruptObject
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
	BOOLEAN Connected = FALSE;

	if (InterruptObject->Connected == FALSE) {
		if (KiIdt[IDT_INT_VECTOR_BASE + InterruptObject->BusInterruptLevel] == 0) {
			KiIdt[IDT_INT_VECTOR_BASE + InterruptObject->BusInterruptLevel] = BUILD_IDT_ENTRY(InterruptObject->DispatchCode[0]);
			HalEnableSystemInterrupt(InterruptObject->BusInterruptLevel, (KINTERRUPT_MODE)InterruptObject->Mode);
			InterruptObject->Connected = Connected = TRUE;
		}
	}

	KiUnlockDispatcherDatabase(OldIrql);

	return Connected;
}

EXPORTNUM(109) VOID XBOXAPI KeInitializeInterrupt
(
	PKINTERRUPT Interrupt,
	PKSERVICE_ROUTINE ServiceRoutine,
	PVOID ServiceContext,
	ULONG Vector,
	KIRQL Irql,
	KINTERRUPT_MODE InterruptMode,
	BOOLEAN ShareVector
)
{
	Interrupt->ServiceRoutine = ServiceRoutine;
	Interrupt->ServiceContext = ServiceContext;
	Interrupt->BusInterruptLevel = Vector - IDT_INT_VECTOR_BASE;
	Interrupt->Irql = Irql;
	Interrupt->Mode = InterruptMode;
	Interrupt->Connected = FALSE;

	PCHAR InterruptPatchedCode = (PCHAR)&Interrupt->DispatchCode[0];
	memcpy(InterruptPatchedCode, KiInterruptEntryCode, sizeof(KiInterruptEntryCode));
	PULONG PatchedInterruptOffset = (PULONG)&InterruptPatchedCode[0x32];
	assert(*PatchedInterruptOffset == 0);
	*PatchedInterruptOffset = (ULONG)Interrupt;
	PULONG PatchedHalpInterruptCommonOffset = (PULONG)&InterruptPatchedCode[0x37];
	assert(*PatchedHalpInterruptCommonOffset == 0);
	*PatchedHalpInterruptCommonOffset = (ULONG)&HalpInterruptCommon;
}
