/*
 * ergo720                Copyright (c) 2022
 */

#include "..\kernel.hpp"
#include "ke.hpp"
#include "rtl.hpp"
#include "fsc.hpp"
#include "mm.hpp"
#include "mi.hpp"
#include "vad_tree.hpp"
#include <assert.h>


EXPORTNUM(102) MMGLOBALDATA MmGlobalData = {
	&MiRetailRegion,
	&MiSystemPteRegion,
	&MiTotalPagesAvailable,
	MiPagesByUsage,
	&MiVadLock,
	(PVOID *)&MiVadRoot,
	nullptr,
	(PVOID *)&MiLastFree
};

BOOLEAN MmInitSystem()
{
	ULONG RequiredPt = 2;

	XboxType = static_cast<SystemType>(inl(KE_SYSTEM_TYPE));
	MiLayoutRetail = (XboxType == SYSTEM_XBOX);
	MiLayoutChihiro = (XboxType == SYSTEM_CHIHIRO);
	MiLayoutDevkit = (XboxType == SYSTEM_DEVKIT);

	// Set up general memory variables according to the xbe type
	if (MiLayoutChihiro) {
		MiMaxContiguousPfn = CHIHIRO_CONTIGUOUS_MEMORY_LIMIT;
		MmSystemMaxMemory = CHIHIRO_MEMORY_SIZE;
		MiHighestPage = CHIHIRO_HIGHEST_PHYSICAL_PAGE;
		MiNV2AInstancePage = CHIHIRO_INSTANCE_PHYSICAL_PAGE;
		MiPfnAddress = CHIHIRO_PFN_ADDRESS;
	}
	else if (MiLayoutDevkit) {
		MmSystemMaxMemory = CHIHIRO_MEMORY_SIZE;
		MiHighestPage = CHIHIRO_HIGHEST_PHYSICAL_PAGE;
		RequiredPt += 2;
	}

	// Calculate how large is the kernel image, so that we can keep its allocation and unmap all the other large pages we were booted with
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(KERNEL_BASE);
	PIMAGE_NT_HEADERS32 pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(KERNEL_BASE + dosHeader->e_lfanew);
	DWORD KernelSize = ROUND_UP_4K(pNtHeader->OptionalHeader.SizeOfImage);

	// Sanity check: make sure our kernel size is below 4 MiB. A real uncompressed kernel is approximately 1.2 MiB large
	ULONG PdeNumber = PAGES_SPANNED_LARGE(KERNEL_BASE, KernelSize);
	PdeNumber = PAGES_SPANNED_LARGE(KERNEL_BASE, (PdeNumber + RequiredPt) * PAGE_SIZE + KernelSize); // also count the space needed for the pts
	if ((KERNEL_BASE + KernelSize + PdeNumber * PAGE_SIZE) >= 0x80400000) {
		return FALSE;
	}

	// Mark all the entries in the pfn database as free
	PFN DatabasePfn, DatabasePfnEnd;
	{
		if (MiLayoutRetail) {
			DatabasePfn = XBOX_PFN_DATABASE_PHYSICAL_PAGE;
			DatabasePfnEnd = XBOX_PFN_DATABASE_PHYSICAL_PAGE + 16 - 1;
		}
		else if (MiLayoutDevkit) {
			DatabasePfn = XBOX_PFN_DATABASE_PHYSICAL_PAGE;
			DatabasePfnEnd = XBOX_PFN_DATABASE_PHYSICAL_PAGE + 32 - 1;
		}
		else {
			DatabasePfn = CHIHIRO_PFN_DATABASE_PHYSICAL_PAGE;
			DatabasePfnEnd = CHIHIRO_PFN_DATABASE_PHYSICAL_PAGE + 32 - 1;
		}

		MiInsertPageRangeInFreeListNoBusy(0, MiHighestPage);
	}

	// Map the kernel image
	ULONG NextPageTableAddr = KERNEL_BASE + KernelSize;
	ULONG TempPte = ValidKernelPteBits | SetPfn(KERNEL_BASE);
	PMMPTE PteEnd = (PMMPTE)(NextPageTableAddr + GetPteOffset(KERNEL_BASE + KernelSize - 1) * 4);
	RtlFillMemoryUlong((PCHAR)NextPageTableAddr, PAGE_SIZE, 0); // zero out the page table used by the kernel
	{
		ULONG Addr = KERNEL_BASE;
		for (PMMPTE Pte = (PMMPTE)(NextPageTableAddr + GetPteOffset(KERNEL_BASE) * 4); Pte <= PteEnd; ++Pte) {
			WritePte(Pte, TempPte);

			// NOTE: this cannot use the overload MiRemovePageFromFreeList that updates PtesUsed of the pfn of the page table that maps this allocation. This, because
			// the PDE of the kernel is written below, and thus GetPfnOfPt will calculate a wrong physical address for the page table, causing it to corrupt
			// the PFN entry with index zero
			PXBOX_PFN Pf = MiRemovePageFromFreeList(GetPfnFromContiguous(Addr));
			Pf->Busy.Busy = 1;
			Pf->Busy.BusyType = Contiguous;
			Pf->Busy.LockCount = 0;
			Pf->Busy.PteIndex = GetPteOffset(GetVAddrMappedByPte(Pte));
			++MiPagesByUsage[Contiguous];

			TempPte += PAGE_SIZE;
			Addr += PAGE_SIZE;
		}
		PteEnd->Hw |= PTE_GUARD_END_MASK;
	}

	{
		// Map the pt of the kernel image. This is backed by the page immediately following it
		// This must happen after the ptes of the pt have been written, because writing the new pde invalidates the large page the kernel is currently using. Otherwise,
		// if some kernel code resides on a 4K page that was not yet cached in the TLB, it will cause a page fault
		WritePte(GetPdeAddress(KERNEL_BASE), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the kernel image and also of the pt
		PXBOX_PFN Pf = MiRemovePageFromFreeList(GetPfnFromContiguous(NextPageTableAddr));
		Pf->PtPageFrame.Busy = 1;
		Pf->PtPageFrame.BusyType = VirtualPageTable;
		Pf->PtPageFrame.LockCount = 0;
		Pf->PtPageFrame.PtesUsed = PAGES_SPANNED(KERNEL_BASE, KernelSize);
		++MiPagesByUsage[VirtualPageTable];
		NextPageTableAddr += PAGE_SIZE;
	}

	{
		// Map the first contiguous page for d3d
		// Skip the pde since that's already been written by the kernel image allocation of above
		PMMPTE Pte = GetPteAddress(CONTIGUOUS_MEMORY_BASE);
		TempPte = ValidKernelPteBits | PTE_PERSIST_MASK | PTE_GUARD_END_MASK | SetPfn(CONTIGUOUS_MEMORY_BASE);
		WritePte(Pte, TempPte);
		MiRemovePageFromFreeList(GetPfnFromContiguous(CONTIGUOUS_MEMORY_BASE), Contiguous, Pte);
	}

	{
		// Map the page directory
		// Skip the pde since that's already been written by the kernel image allocation of above
		PMMPTE Pte = GetPteAddress(CONTIGUOUS_MEMORY_BASE + PAGE_DIRECTORY_PHYSICAL_ADDRESS);
		TempPte = ValidKernelPteBits | PTE_PERSIST_MASK | PTE_GUARD_END_MASK | SetPfn(CONTIGUOUS_MEMORY_BASE + PAGE_DIRECTORY_PHYSICAL_ADDRESS);
		WritePte(Pte, TempPte);
		MiRemovePageFromFreeList(GetPfnFromContiguous(CONTIGUOUS_MEMORY_BASE + PAGE_DIRECTORY_PHYSICAL_ADDRESS), Contiguous, Pte);
	}

	// Unmap all the remaining contiguous memory using 4 MiB pages
	PteEnd = GetPdeAddress(CONTIGUOUS_MEMORY_BASE + MmSystemMaxMemory - 1);
	for (PMMPTE Pte = GetPdeAddress(ROUND_UP(NextPageTableAddr, PAGE_LARGE_SIZE)); Pte <= PteEnd; ++Pte) {
		WriteZeroPte(Pte);
	}

	{
		// Map the pfn database
		ULONG Addr = reinterpret_cast<ULONG>ConvertPfnToContiguous(DatabasePfn);
		WritePte(GetPdeAddress(Addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the pfn database 1
		MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, GetPdeAddress(Addr));
		NextPageTableAddr += PAGE_SIZE;
		if (MiLayoutDevkit) {
			// on devkits, the pfn database crosses a 4 MiB boundary, so it needs another pt
			WritePte(GetPdeAddress(Addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the pfn database 2
			MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, GetPdeAddress(Addr));
			NextPageTableAddr += PAGE_SIZE;
		}
		TempPte = ValidKernelPteBits | PTE_PERSIST_MASK | SetPfn(Addr);
		PteEnd = GetPteAddress(ConvertPfnToContiguous(DatabasePfnEnd));
		for (PMMPTE Pte = GetPteAddress(Addr); Pte <= PteEnd; ++Pte) { // write ptes for the pfn database
			WritePte(Pte, TempPte);
			TempPte += PAGE_SIZE;
		}

		// Only write the pfns after the database has all its ptes written, to avoid triggering page faults
		for (PMMPTE Pte = GetPteAddress(Addr); Pte <= PteEnd; ++Pte) {
			MiRemovePageFromFreeList(GetPfnFromContiguous(Addr), Unknown, Pte);
			Addr += PAGE_SIZE;
		}
		PteEnd->Hw |= PTE_GUARD_END_MASK;
	}

	{
		// Write the pdes of the WC (tiled) memory - no page tables
		TempPte = ValidKernelPteBits | PTE_PAGE_LARGE_MASK | SetPfn(WRITE_COMBINED_BASE);
		TempPte |= SetWriteCombine(TempPte);
		PdeNumber = PAGES_SPANNED_LARGE(WRITE_COMBINED_BASE, WRITE_COMBINED_SIZE);
		ULONG i = 0;
		for (PMMPTE Pte = GetPdeAddress(WRITE_COMBINED_BASE); i < PdeNumber; ++i) {
			WritePte(Pte, TempPte);
			TempPte += PAGE_LARGE_SIZE;
			++Pte;
		}

		// Write the pdes of the UC memory region - no page tables
		TempPte = ValidKernelPteBits | PTE_PAGE_LARGE_MASK | DisableCachingBits | SetPfn(UNCACHED_BASE);
		PdeNumber = PAGES_SPANNED_LARGE(UNCACHED_BASE, UNCACHED_SIZE);
		i = 0;
		for (PMMPTE Pte = GetPdeAddress(UNCACHED_BASE); i < PdeNumber; ++i) {
			WritePte(Pte, TempPte);
			TempPte += PAGE_LARGE_SIZE;
			++Pte;
		}
	}

	{
		// Map the nv2a instance memory
		PFN Pfn, PfnEnd;
		if (MiLayoutRetail || MiLayoutDevkit) {
			Pfn = XBOX_INSTANCE_PHYSICAL_PAGE;
			PfnEnd = XBOX_INSTANCE_PHYSICAL_PAGE + NV2A_INSTANCE_PAGE_COUNT - 1;
		}
		else {
			Pfn = CHIHIRO_INSTANCE_PHYSICAL_PAGE;
			PfnEnd = CHIHIRO_INSTANCE_PHYSICAL_PAGE + NV2A_INSTANCE_PAGE_COUNT - 1;
		}

		ULONG Addr = reinterpret_cast<ULONG>ConvertPfnToContiguous(Pfn);
		TempPte = ValidKernelPteBits | DisableCachingBits | SetPfn(Addr);
		PteEnd = GetPteAddress(ConvertPfnToContiguous(PfnEnd));
		for (PMMPTE Pte = GetPteAddress(Addr); Pte <= PteEnd; ++Pte) { // write ptes for the instance memory
			WritePte(Pte, TempPte);
			MiRemovePageFromFreeList(GetPfnFromContiguous(Addr), Contiguous, Pte);
			TempPte += PAGE_SIZE;
			Addr += PAGE_SIZE;
		}
		PteEnd->Hw |= PTE_GUARD_END_MASK;

		if (MiLayoutDevkit) {
			// Devkits have two nv2a instance memory, another one at the top of the second 64 MiB block

			Pfn += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
			PfnEnd += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
			Addr = reinterpret_cast<ULONG>ConvertPfnToContiguous(Pfn);
			WritePte(GetPdeAddress(Addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the second instance memory
			MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, GetPdeAddress(Addr));

			TempPte = ValidKernelPteBits | DisableCachingBits | SetPfn(Addr);
			PteEnd = GetPteAddress(ConvertPfnToContiguous(PfnEnd));
			for (PMMPTE Pte = GetPteAddress(Addr); Pte <= PteEnd; ++Pte) { // write ptes for the second instance memory
				WritePte(Pte, TempPte);
				MiRemovePageFromFreeList(GetPfnFromContiguous(Addr), Contiguous, Pte);
				TempPte += PAGE_SIZE;
				Addr += PAGE_SIZE;
			}
			PteEnd->Hw |= PTE_GUARD_END_MASK;
		}
	}

	// Unmap the ram window starting at address zero
	PteEnd = GetPdeAddress(MmSystemMaxMemory - 1);
	for (PMMPTE Pte = GetPdeAddress(0); Pte <= PteEnd; ++Pte) {
		WriteZeroPte(Pte);
	}

	// We have changed the memory mappings so flush the tlb now
	MiFlushEntireTlb();

	KiPcr.PrcbData.DpcStack = MmCreateKernelStack(KERNEL_STACK_SIZE, FALSE);
	if (KiPcr.PrcbData.DpcStack == nullptr) {
		return FALSE;
	}

	if (!NT_SUCCESS(FscSetCacheSize(16))) {
		return FALSE;
	}

	if (InsertVADNode(LOWEST_USER_ADDRESS, HIGHEST_USER_ADDRESS, Free, PAGE_NOACCESS) == FALSE) {
		return FALSE;
	}

	MiLastFree = GetVADNode(LOWEST_USER_ADDRESS);

	return TRUE;
}

EXPORTNUM(165) PVOID XBOXAPI MmAllocateContiguousMemory
(
	ULONG NumberOfBytes
)
{
	return MmAllocateContiguousMemoryEx(NumberOfBytes, 0, MAXULONG_PTR, 0, PAGE_READWRITE);
}

EXPORTNUM(166) PVOID XBOXAPI MmAllocateContiguousMemoryEx
(
	ULONG NumberOfBytes,
	PHYSICAL_ADDRESS LowestAcceptableAddress,
	PHYSICAL_ADDRESS HighestAcceptableAddress,
	ULONG Alignment,
	ULONG ProtectionType
)
{
	MMPTE TempPte;
	if (!NumberOfBytes || (MiConvertPageToSystemPtePermissions(ProtectionType, &TempPte) == FALSE)) {
		return nullptr;
	}

	ULONG NumberOfPages = ROUND_UP_4K(NumberOfBytes) >> PAGE_SHIFT;
	ULONG LowestPfn = LowestAcceptableAddress >> PAGE_SHIFT;
	ULONG HighestPfn = HighestAcceptableAddress >> PAGE_SHIFT;
	ULONG PfnAlignment = Alignment >> PAGE_SHIFT;

	if (HighestPfn > MiMaxContiguousPfn) { HighestPfn = MiMaxContiguousPfn; }
	if (LowestPfn > HighestPfn) { LowestPfn = HighestPfn; }
	if (!PfnAlignment) { PfnAlignment = 1; }

	KIRQL OldIrql = MiLock();

	if (NumberOfPages > MiRetailRegion.PagesAvailable) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	ULONG CurrentPfn = ROUND_DOWN(HighestPfn, PfnAlignment), FoundPages = 0;
	do {
		PXBOX_PFN Pf = GetPfnElement(CurrentPfn);
		if (Pf->Busy.Busy) {
			FoundPages = 0;
			CurrentPfn = ROUND_DOWN(CurrentPfn - 1, PfnAlignment);
			continue;
		}

		++FoundPages;
		if (FoundPages == NumberOfPages) {
			break;
		}

		--CurrentPfn;
	} while ((LONG)CurrentPfn >= (LONG)LowestPfn);

	if (FoundPages != NumberOfPages) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	PMMPTE Pte = GetPteAddress(CONTIGUOUS_MEMORY_BASE + (CurrentPfn << PAGE_SHIFT)), StartPte = Pte, PteEnd = Pte + NumberOfPages - 1;
	while (Pte <= PteEnd) {
		PMMPTE Pde = GetPteAddress(Pte);
		if ((Pde->Hw & PTE_VALID_MASK) == 0) {
			++NumberOfPages;
		}
		Pte = PMMPTE((PCHAR)Pte + PAGE_SIZE);
	}

	if (NumberOfPages > MiRetailRegion.PagesAvailable) {
		MiUnlock(OldIrql);
		return nullptr;
	}

	Pte = StartPte;
	while (Pte <= PteEnd) {
		if ((Pte == StartPte) || IsPteOnPdeBoundary(Pte)) {
			PMMPTE Pde = GetPteAddress(Pte);
			if ((Pde->Hw & PTE_VALID_MASK) == 0) {
				PFN_NUMBER PageTablePfn = MiRemoveRetailPageFromFreeList();
				WritePte(Pde, ValidKernelPdeBits | SetPfn(ConvertPfnToContiguous(PageTablePfn)));
				MiRemoveAndZeroPageTableFromFreeList(PageTablePfn, VirtualPageTable, Pde);
			}
		}

		MiRemovePageFromFreeList(CurrentPfn, Contiguous, Pte);
		WritePte(Pte, TempPte.Hw | ((CurrentPfn << PAGE_SHIFT) + CONTIGUOUS_MEMORY_BASE));
		++CurrentPfn;
		++Pte;
	}
	PteEnd->Hw |= PTE_GUARD_END_MASK;

	MiUnlock(OldIrql);

	return (PVOID)GetVAddrMappedByPte(StartPte);
}

EXPORTNUM(167) PVOID XBOXAPI MmAllocateSystemMemory
(
	ULONG NumberOfBytes,
	ULONG Protect
)
{
	return MiAllocateSystemMemory(NumberOfBytes, Protect, SystemMemory, FALSE);
}

// Source: Cxbx-Reloaded
EXPORTNUM(168) PVOID XBOXAPI MmClaimGpuInstanceMemory
(
	SIZE_T NumberOfBytes,
	SIZE_T *NumberOfPaddingBytes
)
{
	// Note that, even though devkits have 128 MiB, there's no need to have a different case for those, since the instance
	// memory is still located 0x10000 bytes from the top of memory just like retail consoles

	if (MiLayoutChihiro) {
		*NumberOfPaddingBytes = 0;
	}
	else {
		*NumberOfPaddingBytes = ConvertPfnToContiguous(X64M_PHYSICAL_PAGE) - ConvertPfnToContiguous(XBOX_INSTANCE_PHYSICAL_PAGE + NV2A_INSTANCE_PAGE_COUNT);
	}

	if (NumberOfBytes != MAXULONG_PTR) {
		KIRQL OldIrql = MiLock();

		NumberOfBytes = ROUND_UP_4K(NumberOfBytes);

		// This check is necessary because some games (e.g. Halo) call this twice but they provide the same size, meaning that we don't need
		// to change anything
		if (NumberOfBytes != MiNV2AInstanceMemoryBytes) {
			PFN Pfn = MiNV2AInstancePage + NV2A_INSTANCE_PAGE_COUNT - (ROUND_UP_4K(MiNV2AInstanceMemoryBytes) >> PAGE_SHIFT);
			PFN PfnEnd = MiNV2AInstancePage + NV2A_INSTANCE_PAGE_COUNT - (NumberOfBytes >> PAGE_SHIFT) - 1;
			PMMPTE Pte = GetPteAddress(ConvertPfnToContiguous(Pfn));
			PMMPTE PteEnd = GetPteAddress(ConvertPfnToContiguous(PfnEnd));

			// NOTE1: we use GetPfnFromContiguous for the found pfn because the instance memory uses physical addresses in the contiguous region
			// NOTE2: it's not necessary to check if PtesUsed drops to zero here because in all systems, the PFN database is either before or
			// after the instance memory, and it's never deallocated
			while (Pte <= PteEnd) {
				assert(Pte->Hw & PTE_VALID_MASK);
				PFN_NUMBER CurrentPfn = GetPfnFromContiguous(Pte->Hw);
				WriteZeroPte(Pte);
				MiFlushTlbForPage((PVOID)GetVAddrMappedByPte(Pte));
				MiInsertPageInFreeList(CurrentPfn);
				PXBOX_PFN Pf = GetPfnOfPt(Pte);
				--Pf->PtPageFrame.PtesUsed;
				++Pte;
			}

			if (MiLayoutDevkit) {
				// Devkits have also another nv2a instance memory at the top of memory, so free also that
				// 3fe0: nv2a; 3ff0: pfn; 4000 + 3fe0: nv2a; 4000 + 3fe0 + 10: free

				Pfn += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
				PfnEnd += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
				Pte = GetPteAddress(ConvertPfnToContiguous(Pfn));
				PteEnd = GetPteAddress(ConvertPfnToContiguous(PfnEnd));

				while (Pte <= PteEnd) {
					assert(Pte->Hw & PTE_VALID_MASK);
					PFN_NUMBER CurrentPfn = GetPfnFromContiguous(Pte->Hw);
					WriteZeroPte(Pte);
					MiFlushTlbForPage((PVOID)GetVAddrMappedByPte(Pte));
					MiInsertPageInFreeList(CurrentPfn);
					PXBOX_PFN Pf = GetPfnOfPt(Pte);
					--Pf->PtPageFrame.PtesUsed;
					if (Pf->PtPageFrame.PtesUsed == 0) {
						// If PtesUsed drops to zero, we can also free the page table and zero out the PDE for it
						PMMPTE Pde = GetPteAddress(Pte);
						PFN_NUMBER PtPfn = GetPfnFromContiguous(Pde->Hw);
						WriteZeroPte(Pde);
						MiFlushTlbForPage((PVOID)GetVAddrMappedByPte(Pde));
						MiInsertPageInFreeList(PtPfn);
					}
					++Pte;
				}
			}
			MiNV2AInstanceMemoryBytes = NumberOfBytes;
		}

		MiUnlock(OldIrql);
	}

	return ConvertPfnToContiguous(MiHighestPage + 1) - *NumberOfPaddingBytes;
}

EXPORTNUM(169) PVOID XBOXAPI MmCreateKernelStack
(
	ULONG NumberOfBytes,
	BOOLEAN DebuggerThread
)
{
	return MiAllocateSystemMemory(NumberOfBytes, PAGE_READWRITE, DebuggerThread ? Debugger : Stack, TRUE);
}

EXPORTNUM(170) VOID XBOXAPI MmDeleteKernelStack
(
	PVOID StackBase,
	PVOID StackLimit
)
{
	ULONG StackSize = (ULONG_PTR)StackBase - (ULONG_PTR)StackLimit + PAGE_SIZE;
	PVOID StackBottom = (PVOID)((ULONG_PTR)StackBase - StackSize);
	MiFreeSystemMemory(StackBottom, StackSize);
}

EXPORTNUM(172) ULONG XBOXAPI MmFreeSystemMemory
(
	PVOID BaseAddress,
	ULONG NumberOfBytes
)
{
	return MiFreeSystemMemory(BaseAddress, NumberOfBytes);
}

EXPORTNUM(180) SIZE_T XBOXAPI MmQueryAllocationSize
(
	PVOID BaseAddress
)
{
	assert(IS_PHYSICAL_ADDRESS(BaseAddress) || IS_SYSTEM_ADDRESS(BaseAddress) || IS_DEVKIT_ADDRESS(BaseAddress));

	KIRQL OldIrql = MiLock();

	ULONG Stop = FALSE, NumberOfPages = 0;
	PMMPTE Pte = GetPteAddress(BaseAddress);

	while (!Stop) {
		Stop = Pte->Hw & PTE_GUARD_END_MASK;
		++Pte;
		++NumberOfPages;
	}

	MiUnlock(OldIrql);

	return NumberOfPages << PAGE_SHIFT;
}

EXPORTNUM(181) NTSTATUS XBOXAPI MmQueryStatistics
(
	PMM_STATISTICS MemoryStatistics
)
{
	if (!MemoryStatistics || (MemoryStatistics->Length != sizeof(MM_STATISTICS))) {
		return STATUS_INVALID_PARAMETER;
	}

	KIRQL OldIrql = MiLock();

	MemoryStatistics->TotalPhysicalPages = MmSystemMaxMemory >> PAGE_SHIFT;
	MemoryStatistics->AvailablePages = MiRetailRegion.PagesAvailable + ((MiLayoutDevkit && MiAllowNonDebuggerOnTop64MiB) ? MiDevkitRegion.PagesAvailable : 0);
	MemoryStatistics->VirtualMemoryBytesCommitted = (MiPagesByUsage[VirtualMemory] + MiPagesByUsage[Image]) << PAGE_SHIFT;
	MemoryStatistics->VirtualMemoryBytesReserved = MiVirtualMemoryBytesReserved;
	MemoryStatistics->CachePagesCommitted = MiPagesByUsage[Cache];
	MemoryStatistics->PoolPagesCommitted = MiPagesByUsage[Pool];
	MemoryStatistics->StackPagesCommitted = MiPagesByUsage[Stack];
	MemoryStatistics->ImagePagesCommitted = MiPagesByUsage[Image];

	MiUnlock(OldIrql);

	return STATUS_SUCCESS;
}
