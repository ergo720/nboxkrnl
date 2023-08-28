/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#include "..\kernel.hpp"
#include "..\ke\ke.hpp"
#include "mm.hpp"
#include "mi.hpp"
#include <string.h>


VOID MmInitSystem()
{
	ULONG RequiredPt = 2;

	SystemType Type = static_cast<SystemType>(InputFromHost(KE_SYSTEM_TYPE));
	MiLayoutRetail = (Type == SYSTEM_XBOX);
	MiLayoutChihiro = (Type == SYSTEM_CHIHIRO);
	MiLayoutDevkit = (Type == SYSTEM_DEVKIT);

	// Set up general memory variables according to the xbe type
	if (MiLayoutChihiro) {
		MiMaxContiguousPfn = CHIHIRO_CONTIGUOUS_MEMORY_LIMIT;
		MmSystemMaxMemory = CHIHIRO_MEMORY_SIZE;
		MiPhysicalPagesAvailable = MmSystemMaxMemory >> PAGE_SHIFT;
		MiHighestPage = CHIHIRO_HIGHEST_PHYSICAL_PAGE;
		MiNV2AInstancePage = CHIHIRO_INSTANCE_PHYSICAL_PAGE;
	}
	else if (MiLayoutDevkit) {
		MmSystemMaxMemory = CHIHIRO_MEMORY_SIZE;
		MiDebuggerPagesAvailable = X64M_PHYSICAL_PAGE;
		MiHighestPage = CHIHIRO_HIGHEST_PHYSICAL_PAGE;
		++RequiredPt;

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
	PdeNumber = PAGES_SPANNED_LARGE(KERNEL_BASE, (PdeNumber + RequiredPt) * PAGE_SIZE + KernelSize); // also count the space needed for the pt's
	if ((KERNEL_BASE + KernelSize + PdeNumber * PAGE_SIZE) >= 0x80400000) {
		KeBugCheck(INIT_FAILURE);
	}

	// Map the pt of the kernel image. This is backed by the page immediately following it
	ULONG NextPageTableAddr = KERNEL_BASE + KernelSize;
	memset(ConvertContiguousToPhysical(NextPageTableAddr), 0, PAGE_SIZE);
	WritePte(GetPdeAddress(KERNEL_BASE), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the kernel image and also of the pt
	WritePte(GetPteAddress(NextPageTableAddr), ValidKernelPteBits | SetPfn(NextPageTableAddr)); // write pte of new pt for the kernel image
	NextPageTableAddr += PAGE_SIZE;

	// Map the kernel image
	MMPTE TempPte = ValidKernelPteBits | SetPfn(KERNEL_BASE);
	PMMPTE pPde_end = GetPteAddress(KERNEL_BASE + KernelSize - 1);
	for (PMMPTE pPde = GetPteAddress(KERNEL_BASE); pPde <= pPde_end; ++pPde) {
		WritePte(pPde, TempPte);
		TempPte += PAGE_SIZE;
	}
	--pPde_end;
	(*pPde_end) |= PTE_GUARD_END_MASK;

	{
		// Map the first contiguous page for d3d
		// Skip the pde since that's already been written by the kernel image allocation of above
		PMMPTE pPde = GetPteAddress(CONTIGUOUS_MEMORY_BASE);
		TempPte = ValidKernelPteBits | PTE_PERSIST_MASK | PTE_GUARD_END_MASK | SetPfn(CONTIGUOUS_MEMORY_BASE);
		WritePte(pPde, TempPte);
	}

	{
		// Map the page directory
		// Skip the pde since that's already been written by the kernel image allocation of above
		PMMPTE pPde = GetPteAddress(CONTIGUOUS_MEMORY_BASE + PAGE_DIRECTORY_PHYSICAL_ADDRESS);
		TempPte = ValidKernelPteBits | PTE_PERSIST_MASK | PTE_GUARD_END_MASK | SetPfn(CONTIGUOUS_MEMORY_BASE + PAGE_DIRECTORY_PHYSICAL_ADDRESS);
		WritePte(pPde, TempPte);
	}

	// Unmap all the remaining contiguous memory using 4 MiB pages (assumes that only xbox contiguous size was mapped)
	pPde_end = GetPdeAddress(CONTIGUOUS_MEMORY_BASE + XBOX_CONTIGUOUS_MEMORY_SIZE - 1);
	for (PMMPTE pPde = GetPdeAddress(ROUND_UP(NextPageTableAddr, PAGE_LARGE_SIZE)); pPde <= pPde_end; ++pPde) {
		WriteZeroPte(pPde);
	}

	// Write the pde's of the WC (tiled) memory - no page tables
	TempPte = ValidKernelPteBits | PTE_PAGE_LARGE_MASK | SetPfn(WRITE_COMBINED_BASE);
	TempPte |= SetWriteCombine(TempPte);
	PdeNumber = PAGES_SPANNED_LARGE(WRITE_COMBINED_BASE, WRITE_COMBINED_SIZE);
	ULONG i = 0;
	for (PMMPTE pPde = GetPdeAddress(WRITE_COMBINED_BASE); i < PdeNumber; ++i) {
		WritePte(pPde, TempPte);
		TempPte += PAGE_LARGE_SIZE;
		++pPde;
	}

	// Write the pde's of the UC memory region - no page tables
	TempPte = ValidKernelPteBits | PTE_PAGE_LARGE_MASK | DisableCachingBits | SetPfn(UNCACHED_BASE);
	PdeNumber = PAGES_SPANNED_LARGE(UNCACHED_BASE, UNCACHED_SIZE);
	i = 0;
	for (PMMPTE pPde = GetPdeAddress(UNCACHED_BASE); i < PdeNumber; ++i) {
		WritePte(pPde, TempPte);
		TempPte += PAGE_LARGE_SIZE;
		++pPde;
	}

	{
		// Map the nv2a instance memory
		PFN pfn, pfn_end;
		if (MiLayoutRetail || MiLayoutDevkit) {
			pfn = XBOX_INSTANCE_PHYSICAL_PAGE;
			pfn_end = XBOX_INSTANCE_PHYSICAL_PAGE + NV2A_INSTANCE_PAGE_COUNT - 1;
		}
		else {
			pfn = CHIHIRO_INSTANCE_PHYSICAL_PAGE;
			pfn_end = CHIHIRO_INSTANCE_PHYSICAL_PAGE + NV2A_INSTANCE_PAGE_COUNT - 1;
		}

		ULONG addr = reinterpret_cast<ULONG>ConvertPfnToContiguousPhysical(pfn);
		memset(ConvertContiguousToPhysical(NextPageTableAddr), 0, PAGE_SIZE);
		WritePte(GetPteAddress(NextPageTableAddr), ValidKernelPteBits | SetPfn(NextPageTableAddr)); // write pte of new pt for the instance memory
		WritePte(GetPdeAddress(addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the instance memory

		TempPte = ValidKernelPteBits | DisableCachingBits | SetPfn(addr);
		pPde_end = GetPteAddress(ConvertPfnToContiguousPhysical(pfn_end));
		for (PMMPTE pPde = GetPteAddress(addr); pPde <= pPde_end; ++pPde) { // write ptes for the instance memory
			WritePte(pPde, TempPte);
			TempPte += PAGE_SIZE;
		}
		--pPde_end;
		(*pPde_end) |= PTE_GUARD_END_MASK;
		NextPageTableAddr += PAGE_SIZE;

		if (MiLayoutDevkit) {
			// Devkits have two nv2a instance memory, another one at the top of the second 64 MiB block

			pfn += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
			pfn_end += DEBUGKIT_FIRST_UPPER_HALF_PAGE;
			addr = reinterpret_cast<ULONG>ConvertPfnToContiguousPhysical(pfn);
			memset(ConvertContiguousToPhysical(NextPageTableAddr), 0, PAGE_SIZE);
			WritePte(GetPteAddress(NextPageTableAddr), ValidKernelPteBits | SetPfn(NextPageTableAddr)); // write pte of new pt for the second instance memory
			WritePte(GetPdeAddress(addr), ValidKernelPdeBits | SetPfn(NextPageTableAddr)); // write pde for the second instance memory

			TempPte = ValidKernelPteBits | DisableCachingBits | SetPfn(addr);
			pPde_end = GetPteAddress(ConvertPfnToContiguousPhysical(pfn_end));
			for (PMMPTE pPde = GetPteAddress(addr); pPde <= pPde_end; ++pPde) { // write ptes for the second instance memory
				WritePte(pPde, TempPte);
				TempPte += PAGE_SIZE;
			}
			--pPde_end;
			(*pPde_end) |= PTE_GUARD_END_MASK;
			NextPageTableAddr += PAGE_SIZE;
		}
	}

	// We have changed the memory mappings so flush the tlb now
	MiFlushEntireTlb();

	// TODO: initialize the pool manager and the VAD tree, keep track of page type usage
}
