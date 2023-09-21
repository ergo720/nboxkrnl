/*
 * ergo720                Copyright (c) 2022
 */

#include "mi.hpp"
#include <string.h>
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
	if (*Pte == 0) {
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
	if (*Pte == 0) {
		// Pte could already be valid because NtAllocateVirtualMemory supports committing over an already committed memory range
		++Pf->PtPageFrame.PtesUsed;
	}

	// Temporarily identity map the page we are going to zero
	PCHAR PageAddr = ConvertPfnToContiguous(Pfn);
	assert(*GetPdeAddress(PageAddr) & PTE_VALID_MASK);

	WritePte(GetPteAddress(PageAddr), ValidKernelPteBits | SetPfn(PageAddr));
	memset(PageAddr, 0, PAGE_SIZE);
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
	memset(PageAddr, 0, PAGE_SIZE);

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
	assert(*GetPdeAddress(PageAddr) & PTE_VALID_MASK);

	WritePte(GetPteAddress(PageAddr), ValidKernelPteBits | SetPfn(PageAddr));
	memset(PageAddr, 0, PAGE_SIZE);
	MiFlushTlbForPage(PageAddr);

	++MiPagesByUsage[BusyType];
}
