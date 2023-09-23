/*
 * ergo720                Copyright (c) 2022
 */

#include "mi.hpp"
#include "..\rtl\rtl.hpp"
#include <assert.h>


VOID MiFlushEntireTlb()
{
	__asm {
		mov eax, cr3
		mov cr3, eax
	}
}

VOID MiFlushTlbForPage(PVOID Addr)
{
	__asm invlpg Addr
}

VOID MiInsertPageInFreeList(PFN_NUMBER Pfn)
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
	--MiPagesByUsage[BusyType];
}

VOID MiInsertPageRangeInFreeList(PFN_NUMBER Pfn, PFN_NUMBER PfnEnd)
{
	assert(Pfn <= PfnEnd);

	for (PFN_NUMBER Pfn1 = Pfn; Pfn1 <= PfnEnd; ++Pfn1) {
		MiInsertPageInFreeList(Pfn1);
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
	if (Pte->Hw == 0) {
		// Pte could already be valid because NtAllocateVirtualMemory supports committing over an already committed memory range
		++Pf->PtPageFrame.PtesUsed;
	}

	++MiPagesByUsage[BusyType];
}

VOID MiRemoveAndZeroPageFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pte)
{
	assert((BusyType != SystemPageTable) && (BusyType != VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->Busy.Busy = 1;
	Pf->Busy.BusyType = BusyType;
	Pf->Busy.LockCount = 0;
	Pf->Busy.PteIndex = GetPteOffset(GetVAddrMappedByPte(Pte));

	// Also update PtesUsed of the pfn that maps the pt that holds the pte
	Pf = GetPfnOfPt(Pte);
	if (Pte->Hw == 0) {
		// Pte could already be valid because NtAllocateVirtualMemory supports committing over an already committed memory range
		++Pf->PtPageFrame.PtesUsed;
	}

	// Temporarily identity map the page we are going to zero
	PCHAR PageAddr = ConvertPfnToContiguous(Pfn);
	assert(GetPdeAddress(PageAddr)->Hw & PTE_VALID_MASK);

	WritePte(GetPteAddress(PageAddr), ValidKernelPteBits | SetPfn(PageAddr));
	RtlFillMemoryUlong(PageAddr, PAGE_SIZE, 0);
	WriteZeroPte(GetPteAddress(PageAddr));
	MiFlushTlbForPage(PageAddr);

	++MiPagesByUsage[BusyType];
}

VOID MiRemoveAndZeroPageTableFromFreeList(PFN_NUMBER Pfn, PageType BusyType, BOOLEAN Unused)
{
	// NOTE: this overload doesn't flush the identity mapping done below from the tlb, and should only be used by MmInitSystem
	assert((BusyType == SystemPageTable) || (BusyType == VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->PtPageFrame.Busy = 1;
	Pf->PtPageFrame.BusyType = BusyType;
	Pf->PtPageFrame.LockCount = 0;
	Pf->PtPageFrame.PtesUsed = 0;

	// Temporarily identity map the page we are going to zero
	PCHAR PageAddr = ConvertPfnToContiguous(Pfn);
	WritePte(GetPteAddress(PageAddr), ValidKernelPteBits | SetPfn(PageAddr));
	RtlFillMemoryUlong(PageAddr, PAGE_SIZE, 0);

	++MiPagesByUsage[BusyType];
}

VOID MiRemoveAndZeroPageTableFromFreeList(PFN_NUMBER Pfn, PageType BusyType)
{
	assert((BusyType == SystemPageTable) || (BusyType == VirtualPageTable));

	PXBOX_PFN Pf = MiRemovePageFromFreeList(Pfn);
	Pf->PtPageFrame.Busy = 1;
	Pf->PtPageFrame.BusyType = BusyType;
	Pf->PtPageFrame.LockCount = 0;
	Pf->PtPageFrame.PtesUsed = 0;

	// Temporarily identity map the page we are going to zero
	PCHAR PageAddr = ConvertPfnToContiguous(Pfn);
	assert(GetPdeAddress(PageAddr)->Hw & PTE_VALID_MASK);

	WritePte(GetPteAddress(PageAddr), ValidKernelPteBits | SetPfn(PageAddr));
	RtlFillMemoryUlong(PageAddr, PAGE_SIZE, 0);
	MiFlushTlbForPage(PageAddr);

	++MiPagesByUsage[BusyType];
}

PFN_NUMBER MiRemoveAnyPageFromFreeList(PageType BusyType, PMMPTE Pte)
{
	PFN_NUMBER Pfn = MiRemoveAnyPageFromFreeList();
	MiRemovePageFromFreeList(Pfn, BusyType, Pte);
	return Pfn;
}

PFN_NUMBER MiRemoveAnyPageFromFreeList()
{
	// The caller should have already checked that we have enough free pages available
	assert(MiTotalPagesAvailable);

	for (PFN_COUNT ListIdx = 0; ListIdx < PFN_NUM_LISTS; ++ListIdx) {
		if (MiRetailRegion.FreeListHead[ListIdx].Blink != PFN_LIST_END) {
			return DecodeFreePfn(MiRetailRegion.FreeListHead[ListIdx].Blink, ListIdx);
		}
	}

	for (PFN_COUNT ListIdx = 0; ListIdx < PFN_NUM_LISTS; ++ListIdx) {
		if (MiDevkitRegion.FreeListHead[ListIdx].Blink != PFN_LIST_END) {
			return DecodeFreePfn(MiDevkitRegion.FreeListHead[ListIdx].Blink, ListIdx);
		}
	}

	RIP_API_MSG("should always find a free page.");
}

static BOOLEAN ConvertPageToSystemPtePermissions(ULONG Protect, PMMPTE Pte)
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
		StartPte->Free.OnePte = 0;
		StartPte[1].Free.Flink = (Pte->Free.OnePte ? 1 : Pte[1].Free.Flink) + NumberOfPtes;
		StartPte->Free.Flink = Pte->Free.Flink;
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
				LastPte->Free.Flink = ULONG(Pte + NumberOfPtes) >> 2;
			}
			else {
				Pte[NumberOfPtes].Free.OnePte = 0;
				Pte[NumberOfPtes + 1].Free.Flink = NumberOfPtesInBlock;
				LastPte->Free.Flink = ULONG(Pte + NumberOfPtes) >> 2;
			}

			return Pte;
		}

		LastPte = Pte;
	}

	// If we reach here, it means that no pte block is large enough to satisfy the request, so we need to allocate new pts
	ULONG NumberOfPts = (ROUND_UP(NumberOfPtes, PTE_PER_PAGE)) / PTE_PER_PAGE;
	if (NumberOfPts > MiTotalPagesAvailable) {
		return nullptr;
	}

	// If we don't have enough virtual memory, then fail the request
	if ((PteRegion->Next4MiBlock + MiB(4) * NumberOfPts - 1) > PteRegion->EndAddr) {
		return nullptr;
	}

	for (ULONG PtsCommitted = 0; PtsCommitted < NumberOfPts; ++PtsCommitted) {
		PFN_NUMBER PtPfn = MiRemoveAnyPageFromFreeList();
		ULONG PtAddr = PtPfn << PAGE_SHIFT;
		PMMPTE PtPde = GetPdeAddress(PteRegion->Next4MiBlock);

		assert(PtPde->Hw == 0);

		WritePte(PtPde, ValidKernelPdeBits | PtAddr); // write pde for the new pt
		MiRemoveAndZeroPageTableFromFreeList(PtPfn, SystemPageTable);
		PteRegion->Next4MiBlock += MiB(4);
	}

	// Update the pte list to include the new free blocks created above
	MiReleasePtes(PteRegion, GetPteAddress(PteRegion->Next4MiBlock), ROUND_UP(NumberOfPtes, PTE_PER_PAGE));
	// It can't fail now
	return MiReservePtes(PteRegion, NumberOfPtes);
}

PVOID MiAllocateSystemMemory(ULONG NumberOfBytes, ULONG Protect, PageType BusyType, BOOLEAN AddGuardPage)
{
	assert(AddGuardPage == FALSE); // TODO

	if (NumberOfBytes == 0) {
		return nullptr;
	}

	MMPTE TempPte;
	if (ConvertPageToSystemPtePermissions(Protect, &TempPte) == FALSE) {
		return nullptr;
	}

	KIRQL OldIrql = MiLock();

	ULONG NumberOfPages = ROUND_UP_4K(NumberOfBytes) >> PAGE_SHIFT;
	if (NumberOfPages > MiTotalPagesAvailable) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	PPTEREGION PteRegion;
	if (BusyType != Debugger) {
		PteRegion = &MiSystemPteRegion;
	}
	else {
		PteRegion = &MiDevkitPteRegion;
	}

	PMMPTE Pte = MiReservePtes(PteRegion, NumberOfPages);
	if (Pte == nullptr) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	// We have to check again because MiReservePtes might have consumed some pages to commit the pts
	if (NumberOfPages > MiTotalPagesAvailable) {
		MiReleasePtes(PteRegion, Pte, NumberOfPages);
		MiUnlock(OldIrql);
		return nullptr;
	}

	PMMPTE StartPte = Pte, PteEnd = Pte + NumberOfPages - 1;
	while (Pte <= PteEnd) {
		WritePte(Pte, TempPte.Hw | (MiRemoveAnyPageFromFreeList(BusyType, Pte) << PAGE_SHIFT));
		++Pte;
	}
	PteEnd->Hw |= PTE_GUARD_END_MASK;

	MiUnlock(OldIrql);

	return (PVOID)GetVAddrMappedByPte(StartPte);
}

ULONG MiFreeSystemMemory(PVOID BaseAddress, ULONG NumberOfBytes)
{
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
		BOOLEAN Stop = FALSE;
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
