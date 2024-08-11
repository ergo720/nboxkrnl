/*
 * ergo720                Copyright (c) 2023
 */

#include "ki.hpp"
#include "ex.hpp"


EXPORTNUM(110) VOID XBOXAPI KeInitializeMutant
(
	PKMUTANT Mutant,
	BOOLEAN InitialOwner
)
{
	Mutant->Header.Type = MutantObject;
	Mutant->Header.Size = sizeof(KMUTANT) / sizeof(LONG);
	InitializeListHead(&Mutant->Header.WaitListHead);
	Mutant->Abandoned = FALSE;

	if (InitialOwner == TRUE) {
		PKTHREAD Thread = KeGetCurrentThread();
		Mutant->Header.SignalState = 0;
		Mutant->OwnerThread = Thread;

		KIRQL OldIrql = KeRaiseIrqlToDpcLevel();
		PLIST_ENTRY MutantListEntry = Thread->MutantListHead.Blink;
		InsertHeadList(MutantListEntry, &Mutant->MutantListEntry);
		KiUnlockDispatcherDatabase(OldIrql);
	}
	else {
		Mutant->Header.SignalState = 1;
		Mutant->OwnerThread = nullptr;
	}
}

EXPORTNUM(131) LONG XBOXAPI KeReleaseMutant
(
	PKMUTANT Mutant,
	KPRIORITY Increment,
	BOOLEAN Abandoned,
	BOOLEAN Wait
)
{
	KIRQL OldIrql = KeRaiseIrqlToDpcLevel();

	PKTHREAD Thread = KeGetCurrentThread();
	LONG OldState = Mutant->Header.SignalState;

	if (Abandoned) {
		Mutant->Header.SignalState = 1;
		Mutant->Abandoned = TRUE;
	}
	else {
		if (Mutant->OwnerThread != Thread) {
			KiUnlockDispatcherDatabase(OldIrql);
			ExRaiseStatus(Mutant->Abandoned ? STATUS_ABANDONED : STATUS_MUTANT_NOT_OWNED); // won't return
		}

		Mutant->Header.SignalState += 1;
	}

	if (Mutant->Header.SignalState == 1) {
		if (OldState <= 0) {
			RemoveEntryList(&Mutant->MutantListEntry);
		}

		Mutant->OwnerThread = nullptr;
		if (IsListEmpty(&Mutant->Header.WaitListHead) == FALSE) {
			KiWaitTest(Mutant, Increment);
		}
	}

	if (Wait) {
		Thread->WaitNext = TRUE;
		Thread->WaitIrql = OldIrql;
	}
	else {
		KiUnlockDispatcherDatabase(OldIrql);
	}

	return OldState;
}
