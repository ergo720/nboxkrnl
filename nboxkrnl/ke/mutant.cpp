/*
 * ergo720                Copyright (c) 2023
 */

#include "..\ki\ki.hpp"


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
