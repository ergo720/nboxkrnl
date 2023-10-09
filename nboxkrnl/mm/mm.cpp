/*
 * ergo720                Copyright (c) 2022
 */

#include "..\kernel.hpp"
#include "..\ke\ke.hpp"
#include "..\rtl\rtl.hpp"
#include "mm.hpp"
#include "mi.hpp"
#include <assert.h>


EXPORTNUM(102) MMGLOBALDATA MmGlobalData = {
	&MiRetailRegion,
	&MiSystemPteRegion,
	&MiTotalPagesAvailable,
	MiPagesByUsage,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};

BOOLEAN MmInitSystem()
{
	ULONG RequiredPt = 2;

	XboxType = static_cast<SystemType>(InputFromHost(KE_SYSTEM_TYPE));
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

		// Note that even if this is true, only the heap/Nt functions of the title are affected, the Mm functions
		// will still use only the lower 64 MiB and the same is true for the debugger pages, meaning they will only
		// use the upper extra 64 MiB regardless of this flag
		//if (CxbxKrnl_Xbe->m_Header.dwInitFlags.bLimit64MB) { m_bAllowNonDebuggerOnTop64MiB = false; }
	}

	// Calculate how large is the kernel image, so that we can keep its allocation and unmap all the other large pages we were booted with
	PIMAGE_DOS_HEADER dosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(KERNEL_BASE);
	PIMAGE_NT_HEADERS32 pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS32>(KERNEL_BASE + dosHeader->e_lfanew);
	DWORD KernelSize = ROUND_UP_4K(pNtHeader->OptionalHeader.SizeOfImage);

	// Sanity check: make sure our kernel size is below 4 MiB. A real uncompressed kernel is approximately 1.2 MiB large
	ULONG PdeNumber = PAGES_SPANNED_LARGE(KERNEL_BASE, KernelSize);
	PdeNumber = PAGES_SPANNED_LARGE(KERNEL_BASE, (PdeNumber + RequiredPt) * PAGE_SIZE + KernelSize); // also count the space needed for the pts
	if ((KERNEL_BASE + KernelSize + PdeNumber * PAGE_SIZE) >= 0x80400000) {
		KeBugCheck(INIT_FAILURE);
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

		MiInsertPageRangeInFreeList(0, MiHighestPage);
	}

	// Map the kernel image
	ULONG NextPageTableAddr = KERNEL_BASE + KernelSize;
	ULONG TempPte = ValidKernelPteBits | SetPfn(KERNEL_BASE);
	PMMPTE PteEnd = (PMMPTE)(NextPageTableAddr + GetPteOffset(KERNEL_BASE + KernelSize - 1) * 4);
	{
		ULONG Addr = KERNEL_BASE;
		for (PMMPTE Pte = (PMMPTE)(NextPageTableAddr + GetPteOffset(KERNEL_BASE) * 4); Pte <= PteEnd; ++Pte) {
			WritePte(Pte, TempPte);
			MiRemovePageFromFreeList(GetPfnFromContiguous(Addr), Contiguous, Pte);
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
		MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, 0);
		NextPageTableAddr += PAGE_SIZE;
		if (MiLayoutDevkit) {
			// on devkits, the pfn database crosses a 4 MiB boundary, so it needs another pt
			WritePte(GetPdeAddress(Addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the pfn database 2
			MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, 0);
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
			MiRemoveAndZeroPageTableFromFreeList(GetPfnFromContiguous(NextPageTableAddr), VirtualPageTable, 0);

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

	// TODO: initialize the VAD tree

	return TRUE;
}

EXPORTNUM(167) PVOID XBOXAPI MmAllocateSystemMemory
(
	ULONG NumberOfBytes,
	ULONG Protect
)
{
	return MiAllocateSystemMemory(NumberOfBytes, Protect, SystemMemory, FALSE);
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
