/*
 * ergo720                Copyright (c) 2022
 */

#include "mi.hpp"
#include "rtl.hpp"
#include "dbg.hpp"
#include <assert.h>


VOID MiFlushEntireTlb()
{
	ASM_BEGIN
		ASM(mov eax, cr3);
		ASM(mov cr3, eax);
	ASM_END
}

VOID MiFlushTlbForPage(PVOID Addr)
{
	ASM(invlpg Addr);
}

PageType MiInsertPageInFreeListNoBusy(PFN_NUMBER Pfn)
{
	assert(Pfn <= MiHighestPage);

	PFN_COUNT ListIdx = GetPfnListIdx(Pfn);
	USHORT EncodedPfn = EncodeFreePfn(Pfn);
	PXBOX_PFN Pf = GetPfnElement(Pfn);
	PageType BusyType = (PageType)Pf->Busy.BusyType;
	PPFNREGION Region;
	if (MiLayoutDevkit && (Pfn >= DEBUGKIT_FIRST_UPPER_HALF_PAGE)) {
		Region = &MiDevkitRegion;
	}
	else {
		Region = &MiRetailRegion;
	}

	/*
	Performs a tail insertion from the list head
	Before:
	1 - 51
	head f 1, b 51
	After:
	1 - 51 - 3
	3 f null, b 51
	51 f 3, b unchanged
	head f unchanged, b 3
	*/

	if (Region->FreeListHead[ListIdx].Flink != PFN_LIST_END) {
		Pf->Free.Flink = PFN_LIST_END;
		Pf->Free.Blink = Region->FreeListHead[ListIdx].Blink;
		USHORT PrevPfnIdx = DecodeFreePfn(Region->FreeListHead[ListIdx].Blink, ListIdx);
		PXBOX_PFN PrevPf = GetPfnElement(PrevPfnIdx);
		PrevPf->Free.Flink = EncodedPfn;
		Region->FreeListHead[ListIdx].Blink = EncodedPfn;

		assert(GetPfnListIdx(PrevPfnIdx) == ListIdx);
	}
	else {
		Region->FreeListHead[ListIdx].Flink = EncodedPfn;
		Region->FreeListHead[ListIdx].Blink = EncodedPfn;
		Pf->Free.Flink = PFN_LIST_END;
		Pf->Free.Blink = PFN_LIST_END;
	}

	++Region->PagesAvailable;
	++MiTotalPagesAvailable;

	return BusyType;
}

VOID MiInsertPageInFreeList(PFN_NUMBER Pfn)
{
	--MiPagesByUsage[MiInsertPageInFreeListNoBusy(Pfn)];
}

VOID MiInsertPageRangeInFreeListNoBusy(PFN_NUMBER Pfn, PFN_NUMBER PfnEnd)
{
	assert(Pfn <= PfnEnd);

	for (PFN_NUMBER Pfn1 = Pfn; Pfn1 <= PfnEnd; ++Pfn1) {
		MiInsertPageInFreeListNoBusy(Pfn1);
	}
}

PXBOX_PFN MiRemovePageFromFreeList(PFN_NUMBER Pfn)
{
	assert(Pfn <= MiHighestPage);

	PFN_COUNT ListIdx = GetPfnListIdx(Pfn);
	PXBOX_PFN Pf = GetPfnElement(Pfn);
	PPFNREGION Region;
	if (MiLayoutDevkit && (Pfn >= DEBUGKIT_FIRST_UPPER_HALF_PAGE)) {
		Region = &MiDevkitRegion;
	}
	else {
		Region = &MiRetailRegion;
	}

	assert(Pf->Busy.Busy == 0);

	if (Pf->Free.Flink == PFN_LIST_END) {
		// This is the last element, so update Blink of head

		Region->FreeListHead[ListIdx].Blink = Pf->Free.Blink;
		USHORT Blink = Pf->Free.Blink;
		USHORT PrevPfnIdx = DecodeFreePfn(Blink, ListIdx);
		PXBOX_PFN PrevPf = GetPfnElement(PrevPfnIdx);
		PrevPf->Free.Flink = PFN_LIST_END;

		assert(GetPfnListIdx(PrevPfnIdx) == ListIdx);
	}
	else if (Pf->Free.Blink == PFN_LIST_END) {
		// This is the first element, so update Flink of head

		Region->FreeListHead[ListIdx].Flink = Pf->Free.Flink;
		USHORT Flink = Pf->Free.Flink;
		USHORT NextPfnIdx = DecodeFreePfn(Flink, ListIdx);
		PXBOX_PFN NextPf = GetPfnElement(NextPfnIdx);
		NextPf->Free.Blink = PFN_LIST_END;

		assert(GetPfnListIdx(NextPfnIdx) == ListIdx);
	}
	else {
		// The element is in the middle of the list

		USHORT Blink = Pf->Free.Blink;
		USHORT Flink = Pf->Free.Flink;
		USHORT PrevPfnIdx = DecodeFreePfn(Blink, ListIdx);
		PXBOX_PFN PrevPf = GetPfnElement(PrevPfnIdx);
		USHORT NextPfnIdx = DecodeFreePfn(Flink, ListIdx);
		PXBOX_PFN NextPf = GetPfnElement(NextPfnIdx);
		PrevPf->Free.Flink = Pf->Free.Flink;
		NextPf->Free.Blink = Pf->Free.Blink;

		assert(GetPfnListIdx(PrevPfnIdx) == ListIdx);
		assert(GetPfnListIdx(NextPfnIdx) == ListIdx);
	}

	--Region->PagesAvailable;
	--MiTotalPagesAvailable;

	return Pf;
}

VOID MiRemovePageFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pte)
{
	assert((BusyType != SystemPageTable) && (BusyType != VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->Busy.Busy = 1;
	Pf->Busy.BusyType = BusyType;
	Pf->Busy.LockCount = 0;
	Pf->Busy.PteIndex = GetPteOffset(GetVAddrMappedByPte(Pte));

	// Also update PtesUsed of the pfn that maps the pt that holds the pte
	Pf = GetPfnOfPt(Pte);
	++Pf->PtPageFrame.PtesUsed;

	++MiPagesByUsage[BusyType];
}

VOID MiRemoveAndZeroPageFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pte)
{
	// NOTE: "Pte" is the PTE that maps the page with a physical address mapped by "Pfn"

	assert((BusyType != SystemPageTable) && (BusyType != VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->Busy.Busy = 1;
	Pf->Busy.BusyType = BusyType;
	Pf->Busy.LockCount = 0;
	Pf->Busy.PteIndex = GetPteOffset(GetVAddrMappedByPte(Pte));

	// Also update PtesUsed of the pfn that maps the pt that holds the pte
	Pf = GetPfnOfPt(Pte);
	++Pf->PtPageFrame.PtesUsed;

	// Zero out the page
	RtlFillMemoryUlong((PCHAR)GetVAddrMappedByPte(Pte), PAGE_SIZE, 0);

	++MiPagesByUsage[BusyType];
}

VOID MiRemoveAndZeroPageTableFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pde)
{
	// NOTE: "Pde" is the PDE that maps the page table with a physical address mapped by "Pfn"

	assert((BusyType == SystemPageTable) || (BusyType == VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->PtPageFrame.Busy = 1;
	Pf->PtPageFrame.BusyType = BusyType;
	Pf->PtPageFrame.LockCount = 0;
	Pf->PtPageFrame.PtesUsed = 0;

	// Zero out the page table
	PCHAR PageTableAddr = (PCHAR)(PAGE_TABLES_BASE + ((((ULONG)Pde & PAGE_MASK) >> 2) << PAGE_SHIFT));
	RtlFillMemoryUlong(PageTableAddr, PAGE_SIZE, 0);

	++MiPagesByUsage[BusyType];
}

PFN_NUMBER MiRemovePageFromFreeList(PageType BusyType, PMMPTE Pte, PFN_COUNT(*AllocationRoutine)())
{
	PFN_NUMBER Pfn = AllocationRoutine();
	MiRemovePageFromFreeList(Pfn, BusyType, Pte);
	return Pfn;
}

PFN_NUMBER MiRemoveRetailPageFromFreeList()
{
	for (PFN_COUNT ListIdx = 0; ListIdx < PFN_NUM_LISTS; ++ListIdx) {
		if (MiRetailRegion.FreeListHead[ListIdx].Blink != PFN_LIST_END) {
			return DecodeFreePfn(MiRetailRegion.FreeListHead[ListIdx].Blink, ListIdx);
		}
	}

	RIP_API_MSG("should always find a free page.");
}

PFN_NUMBER MiRemoveDevkitPageFromFreeList()
{
	for (PFN_COUNT ListIdx = 0; ListIdx < PFN_NUM_LISTS; ++ListIdx) {
		if (MiDevkitRegion.FreeListHead[ListIdx].Blink != PFN_LIST_END) {
			return DecodeFreePfn(MiDevkitRegion.FreeListHead[ListIdx].Blink, ListIdx);
		}
	}

	RIP_API_MSG("should always find a free page.");
}

PFN_NUMBER MiRemoveAnyPageFromFreeList()
{
	// The caller should have already checked that we have enough free pages available
	assert(MiTotalPagesAvailable);

	MiRemoveRetailPageFromFreeList();
	MiRemoveDevkitPageFromFreeList();

	RIP_API_MSG("should always find a free page.");
}

BOOLEAN MiConvertPageToPtePermissions(ULONG Protect, PMMPTE Pte)
{
	if (Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE | 0xFF)) {
		return FALSE; // unknown or not allowed flag specified
	}

	if (Protect & PAGE_NOACCESS) {
		if (Protect & (PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE)) {
			return FALSE; // PAGE_NOACCESS cannot be specified together with the other access modifiers
		}
	}

	if ((Protect & PAGE_NOCACHE) && (Protect & PAGE_WRITECOMBINE)) {
		return FALSE; // PAGE_NOCACHE and PAGE_WRITECOMBINE cannot be specified together
	}

	ULONG Mask = 0;
	ULONG LowNibble = Protect & 0xF;
	ULONG HighNibble = (Protect >> 4) & 0xF;

	if ((LowNibble == 0 && HighNibble == 0) || (LowNibble != 0 && HighNibble != 0)) {
		return FALSE; // high and low permission flags cannot be mixed together
	}

	if ((LowNibble | HighNibble) == 1 || (LowNibble | HighNibble) == 2) {
		Mask = PTE_READONLY;
	}
	else if ((LowNibble | HighNibble) == 4) {
		Mask = PTE_READWRITE;
	}
	else {
		return FALSE; // all the other combinations are invalid
	}

	// Apply the rest of the access modifiers to the pte mask
	if ((Protect & (PAGE_NOACCESS | PAGE_GUARD)) == 0) {
		Mask |= PTE_VALID_MASK;
	}
	else if (Protect & PAGE_GUARD) {
		Mask |= PTE_GUARD;
	}

	if (Protect & PAGE_NOCACHE) {
		Mask |= PTE_CACHE_DISABLE_MASK;
	}
	else if (Protect & PAGE_WRITECOMBINE) {
		Mask |= PTE_WRITE_THROUGH_MASK;
	}

	assert((Mask & ~(PTE_VALID_PROTECTION_MASK)) == 0); // ensure that we've created a valid permission mask

	Pte->Hw = Mask;

	return TRUE;
}

BOOLEAN MiConvertPageToSystemPtePermissions(ULONG Protect, PMMPTE Pte)
{
	ULONG Mask = 0;

	if (Protect & ~(PAGE_NOCACHE | PAGE_WRITECOMBINE | PAGE_READWRITE | PAGE_READONLY)) {
		return FALSE; // unknown or not allowed flag specified
	}

	switch (Protect & (PAGE_READONLY | PAGE_READWRITE))
	{
	case PAGE_READONLY:
		Mask = (PTE_VALID_MASK | PTE_DIRTY_MASK | PTE_ACCESS_MASK);
		break;

	case PAGE_READWRITE:
		Mask = ValidKernelPteBits;
		break;

	default:
		return FALSE; // both are specified, wrong
	}

	switch (Protect & (PAGE_NOCACHE | PAGE_WRITECOMBINE))
	{
	case 0:
		break; // none is specified, ok

	case PAGE_NOCACHE:
		Mask |= PTE_CACHE_DISABLE_MASK;
		break;

	case PAGE_WRITECOMBINE:
		Mask |= PTE_WRITE_THROUGH_MASK;
		break;

	default:
		return FALSE; // both are specified, wrong
	}

	Pte->Hw = Mask;

	return TRUE;
}

static VOID MiReleasePtes(PPTEREGION PteRegion, PMMPTE StartPte, ULONG NumberOfPtes)
{
	RtlFillMemoryUlong(StartPte, NumberOfPtes * sizeof(MMPTE), 0); // caller should flush the TLB if necessary

	PMMPTE Pte = nullptr, LastPte = &PteRegion->Head;
	while (LastPte->Free.Flink != PTE_LIST_END) {
		Pte = PMMPTE(LastPte->Free.Flink << 2);
		if (Pte > StartPte) {
			break;
		}

		LastPte = Pte;
	}

	// Update Flink and size of StartPte assuming no merging with adjacent blocks
	if (NumberOfPtes == 1) {
		StartPte->Free.OnePte = 1;
	}
	else {
		StartPte->Free.OnePte = 0;
		StartPte[1].Free.Flink = NumberOfPtes;
	}

	StartPte->Free.Flink = LastPte->Free.Flink;
	LastPte->Free.Flink = (ULONG)StartPte >> 2;

	if ((StartPte + NumberOfPtes) == Pte) {
		// Following block is contiguous with the one we are freeing, merge the two
		// NOTE: StartPte[1] must be updated after StartPte. Otherwise, if NumberOfPtes=1, then StartPte[1] == Pte and writing to StartPte[1] first will corrupt Pte!
		StartPte->Free.OnePte = 0;
		StartPte->Free.Flink = Pte->Free.Flink;
		StartPte[1].Free.Flink = (Pte->Free.OnePte ? 1 : Pte[1].Free.Flink) + NumberOfPtes;
		NumberOfPtes = StartPte[1].Free.Flink;
	}

	if (LastPte != &PteRegion->Head) {
		ULONG NumberOfPtesInBlock = LastPte->Free.OnePte ? 1 : LastPte[1].Free.Flink;

		if (LastPte + NumberOfPtesInBlock == StartPte) {
			// Previous block is contiguous with the one we are freeing, merge the two
			LastPte->Free.OnePte = 0;
			LastPte->Free.Flink = StartPte->Free.Flink;
			LastPte[1].Free.Flink = NumberOfPtes + NumberOfPtesInBlock;
		}
	}
}

static PMMPTE MiReservePtes(PPTEREGION PteRegion, ULONG NumberOfPtes)
{
	PMMPTE Pte = nullptr, LastPte = &PteRegion->Head;
	while (LastPte->Free.Flink != PTE_LIST_END) {
		Pte = PMMPTE(LastPte->Free.Flink << 2);
		ULONG NumberOfPtesInBlock = Pte->Free.OnePte ? 1 : Pte[1].Free.Flink;
		if (NumberOfPtesInBlock >= NumberOfPtes) {
			// This block has enough ptes to satisfy the request
			NumberOfPtesInBlock -= NumberOfPtes;
			if (NumberOfPtesInBlock == 0) {
				LastPte->Free.Flink = Pte->Free.Flink;
			}
			else if (NumberOfPtesInBlock == 1) {
				Pte[NumberOfPtes].Free.OnePte = 1;
				Pte[NumberOfPtes].Free.Flink = Pte->Free.Flink;
				LastPte->Free.Flink = ULONG(Pte + NumberOfPtes) >> 2;
			}
			else {
				Pte[NumberOfPtes].Free.OnePte = 0;
				Pte[NumberOfPtes].Free.Flink = Pte->Free.Flink;
				Pte[NumberOfPtes + 1].Free.Flink = NumberOfPtesInBlock;
				LastPte->Free.Flink = ULONG(Pte + NumberOfPtes) >> 2;
			}

			return Pte;
		}

		LastPte = Pte;
	}

	// If we reach here, it means that no pte block is large enough to satisfy the request, so we need to allocate new pts
	ULONG NumberOfPts = (ROUND_UP(NumberOfPtes, PTE_PER_PAGE)) / PTE_PER_PAGE;
	if (NumberOfPts > *PteRegion->PagesAvailable) {
		return nullptr;
	}

	// If we don't have enough virtual memory, then fail the request
	if ((PteRegion->Next4MiBlock + MiB(4) * NumberOfPts - 1) > PteRegion->EndAddr) {
		return nullptr;
	}

	ULONG Start4MiBlock = PteRegion->Next4MiBlock;
	for (ULONG PtsCommitted = 0; PtsCommitted < NumberOfPts; ++PtsCommitted) {
		PFN_NUMBER PtPfn = PteRegion->AllocationRoutine();
		ULONG PtAddr = PtPfn << PAGE_SHIFT;
		PMMPTE PtPde = GetPdeAddress(PteRegion->Next4MiBlock);

		assert(PtPde->Hw == 0);

		WritePte(PtPde, ValidKernelPdeBits | PtAddr); // write pde for the new pt
		MiRemoveAndZeroPageTableFromFreeList(PtPfn, SystemPageTable, PtPde);
		PteRegion->Next4MiBlock += MiB(4);
	}

	// Update the pte list to include the new free blocks created above
	MiReleasePtes(PteRegion, GetPteAddress(Start4MiBlock), ROUND_UP(NumberOfPtes, PTE_PER_PAGE));
	// It can't fail now
	return MiReservePtes(PteRegion, NumberOfPtes);
}

PVOID MiAllocateSystemMemory(ULONG NumberOfBytes, ULONG Protect, PageType BusyType, BOOLEAN AddGuardPage)
{
	if (NumberOfBytes == 0) {
		return nullptr;
	}

	MMPTE TempPte;
	if (MiConvertPageToSystemPtePermissions(Protect, &TempPte) == FALSE) {
		return nullptr;
	}

	KIRQL OldIrql = MiLock();

	ULONG NumberOfPages = ROUND_UP_4K(NumberOfBytes) >> PAGE_SHIFT;
	PPTEREGION PteRegion;
	if (BusyType != Debugger) {
		PteRegion = &MiSystemPteRegion;
	}
	else {
		PteRegion = &MiDevkitPteRegion;
	}

	if (NumberOfPages > *PteRegion->PagesAvailable) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	ULONG NumberOfVirtualPages = AddGuardPage ? NumberOfPages + 1 : NumberOfPages;
	PMMPTE Pte = MiReservePtes(PteRegion, NumberOfVirtualPages);
	if (Pte == nullptr) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	// We have to check again because MiReservePtes might have consumed some pages to commit the pts
	if (NumberOfPages > *PteRegion->PagesAvailable) {
		MiReleasePtes(PteRegion, Pte, NumberOfVirtualPages);
		MiUnlock(OldIrql);
		return nullptr;
	}

	PMMPTE StartPte = Pte, PteEnd = Pte + NumberOfVirtualPages - 1;
	if (AddGuardPage) {
		WriteZeroPte(Pte); // the guard page of the stack is not backed by a physical page
		StartPte = PteEnd + 1; // returns the top of the allocation
		++Pte;
	}

	while (Pte <= PteEnd) {
		WritePte(Pte, TempPte.Hw | (MiRemovePageFromFreeList(BusyType, Pte, PteRegion->AllocationRoutine) << PAGE_SHIFT));
		++Pte;
	}
	PteEnd->Hw |= PTE_GUARD_END_MASK;

	MiUnlock(OldIrql);

	return (PVOID)GetVAddrMappedByPte(StartPte);
}

ULONG MiFreeSystemMemory(PVOID BaseAddress, ULONG NumberOfBytes)
{
	if (BaseAddress == nullptr) {
		// This happens in ObInitSystem because RootTable is initially set to nullptr
		return 0;
	}

	assert(CHECK_ALIGNMENT((ULONG)BaseAddress, PAGE_SIZE)); // all starting addresses in the system region are page aligned
	assert(IS_SYSTEM_ADDRESS(BaseAddress) || IS_DEVKIT_ADDRESS(BaseAddress));

	KIRQL OldIrql = MiLock();

	ULONG NumberOfPages;
	PMMPTE Pte = GetPteAddress(BaseAddress), StartPte = Pte;
	if (NumberOfBytes) {
		PMMPTE PteEnd = GetPteAddress((ULONG)BaseAddress + NumberOfBytes - 1);
		NumberOfPages = PteEnd - Pte + 1;

		while (Pte <= PteEnd) {
			if (Pte->Hw & PTE_VALID_MASK) {
				PFN_NUMBER Pfn = Pte->Hw >> PAGE_SHIFT;
				WriteZeroPte(Pte);
				MiFlushTlbForPage((PVOID)GetVAddrMappedByPte(Pte));
				MiInsertPageInFreeList(Pfn);
				PXBOX_PFN Pf = GetPfnOfPt(Pte);
				--Pf->PtPageFrame.PtesUsed;
			}
			++Pte;
		}
	}
	else {
		ULONG Stop = FALSE;
		NumberOfPages = 0;

		while (!Stop) {
			Stop = Pte->Hw & PTE_GUARD_END_MASK;
			if (Pte->Hw & PTE_VALID_MASK) {
				PFN_NUMBER Pfn = Pte->Hw >> PAGE_SHIFT;
				WriteZeroPte(Pte);
				MiFlushTlbForPage((PVOID)GetVAddrMappedByPte(Pte));
				MiInsertPageInFreeList(Pfn);
				PXBOX_PFN Pf = GetPfnOfPt(Pte);
				--Pf->PtPageFrame.PtesUsed;
			}
			++Pte;
			++NumberOfPages;
		}
	}

	MiReleasePtes(IS_SYSTEM_ADDRESS(BaseAddress) ? &MiSystemPteRegion : &MiDevkitPteRegion, StartPte, NumberOfPages);

	MiUnlock(OldIrql);

	return NumberOfPages;
}

VOID XBOXAPI MiPageFaultHandler(ULONG Cr2, ULONG Eip)
{
	// For now, this just logs the faulting access and returns

	DbgPrint("Page fault at 0x%X while touching address 0x%X", Eip, Cr2);
}
