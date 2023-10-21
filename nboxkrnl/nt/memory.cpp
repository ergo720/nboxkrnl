/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "..\ntstatus.hpp"
#include "..\mm\mi.hpp"
#include "..\mm\vad_tree.hpp"
#include <assert.h>


static ULONG VirtualMemoryBytesReserved = 0;

static ULONG MapMemoryBlock(ULONG Size, ULONG HighestAddress)
{
	VAD_NODE *Node = MiLastFree;
	VAD_NODE *EndNode = GetNextVAD(GetVADNode(HighestAddress));

	while (Node != EndNode) {
		if (Node->m_Vad.m_Type != Free) { // already reserved
			Node = GetNextVAD(Node);
			continue;
		}

		ULONG Addr = Node->m_Start;
		if (!CHECK_ALIGNMENT(Addr, X64K)) {
			// Addr is not aligned with the xbox granularity, jump to the next granularity boundary
			Addr = ROUND_UP(Addr, X64K);
		}

		// Make sure that the end address of this allocation won't exceed HighestAddress
		ULONG VadEnd = Node->m_Start + Node->m_Vad.m_Size;
		if (VadEnd > (HighestAddress + 1)) {
			VadEnd = HighestAddress + 1;
		}

		if ((Addr + Size - 1) < VadEnd) {
			return Addr;
		}

		Node = GetNextVAD(Node);
	}

	// If we are here, it means we reached the end of the user region. In desperation, we also try to map it from
	// MiLastFree and going backwards, since there could be holes created by deallocation operations...

	VAD_NODE *BeginNode = GetVADNode(LOWEST_USER_ADDRESS);
	if (MiLastFree == BeginNode) {
		return 0;
	}

	Node = GetPrevVAD(MiLastFree);
	while (true) {
		if (Node->m_Vad.m_Type == Free) {
			ULONG Addr = Node->m_Start;
			if (!CHECK_ALIGNMENT(Addr, X64K)) {
				// Addr is not aligned with the xbox granularity, jump to the next granularity boundary
				Addr = ROUND_UP(Addr, X64K);
			}

			ULONG VadEnd = Node->m_Start + Node->m_Vad.m_Size;
			if ((Addr + Size - 1) < VadEnd) {
				return Addr;
			}
		}

		if (Node == BeginNode) {
			break;
		}

		Node = GetPrevVAD(Node);
	}

	return 0;
}

EXPORTNUM(184) NTSTATUS XBOXAPI NtAllocateVirtualMemory
(
	PVOID *BaseAddress,
	ULONG ZeroBits,
	PULONG AllocationSize,
	DWORD AllocationType,
	DWORD Protect
)
{
	ULONG CapturedBase = *(ULONG *)BaseAddress;
	ULONG CapturedSize = *AllocationSize;

	// Invalid base address
	if (CapturedBase > HIGHEST_VAD_ADDRESS) {
		return STATUS_INVALID_PARAMETER;
	}

	// Invalid region size
	if (((HIGHEST_VAD_ADDRESS + 1) - CapturedBase) < CapturedSize) {
		return STATUS_INVALID_PARAMETER;
	}

	// Size cannot be zero
	if (CapturedSize == 0) {
		return STATUS_INVALID_PARAMETER;
	}

	// Limit number of zero bits upto 21
	if (ZeroBits > MAXIMUM_ZERO_BITS) {
		return STATUS_INVALID_PARAMETER;
	}

	// Check for unknown MEM flags
	if (AllocationType & ~(MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN | MEM_RESET | MEM_NOZERO)) {
		return STATUS_INVALID_PARAMETER;
	}

	// No other flags allowed in combination with MEM_RESET
	if ((AllocationType & MEM_RESET) && (AllocationType != MEM_RESET)) {
		return STATUS_INVALID_PARAMETER;
	}

	// At least MEM_RESET, MEM_COMMIT or MEM_RESERVE must be set
	if ((AllocationType & (MEM_COMMIT | MEM_RESERVE | MEM_RESET)) == 0) {
		return STATUS_INVALID_PARAMETER;
	}

	MMPTE TempPte;
	if (!MiConvertPageToPtePermissions(Protect, &TempPte)) {
		return STATUS_INVALID_PAGE_PROTECTION;
	}

	VadLock();

	BOOLEAN DestructVadOnFailure = FALSE;
	ULONG AlignedCapturedBase, AlignedCapturedSize;
	if (AllocationType & MEM_RESERVE || CapturedBase == 0) {
		if (CapturedBase == 0) {
			// We are free to decide where to put this block

			ULONG MaxAllowedAddress;
			AlignedCapturedSize = ROUND_UP_4K(CapturedSize);
			if (ZeroBits) {
				MaxAllowedAddress = MAX_VIRTUAL_ADDRESS >> ZeroBits;
				if (MaxAllowedAddress > HIGHEST_USER_ADDRESS) {
					VadUnlock();
					return STATUS_INVALID_PARAMETER;
				}
			}
			else {
				MaxAllowedAddress = HIGHEST_USER_ADDRESS;
			}

			if (AllocationType & MEM_TOP_DOWN) {
				MiLastFree = GetVADNode(MaxAllowedAddress);
			}

			AlignedCapturedBase = MapMemoryBlock(AlignedCapturedSize, MaxAllowedAddress);
			if (!AlignedCapturedBase) {
				VadUnlock();
				return STATUS_NO_MEMORY;
			}
		}
		else {
			BOOLEAN Overflow;
			AlignedCapturedBase = ROUND_DOWN(CapturedBase, X64K);
			AlignedCapturedSize = ROUND_UP_4K(CapturedSize);
			VAD_NODE *Node = CheckConflictingVAD(AlignedCapturedBase, AlignedCapturedSize, &Overflow);

			if (Node != nullptr || Overflow) {
				// Reserved VAD or we are overflowing a free VAD, report an error
				VadUnlock();
				return STATUS_CONFLICTING_ADDRESSES;
			}
		}

		if (ConstructVAD(AlignedCapturedBase, AlignedCapturedSize, Protect) == FALSE) {
			VadUnlock();
			return STATUS_NO_MEMORY;
		}

		VirtualMemoryBytesReserved += AlignedCapturedSize;

		if ((AllocationType & MEM_COMMIT) == 0) {
			// MEM_COMMIT was not specified, so we are done with the allocation

			*BaseAddress = (PULONG)AlignedCapturedBase;
			*AllocationSize = AlignedCapturedSize;

			VadUnlock();
			return STATUS_SUCCESS;
		}
		
		DestructVadOnFailure = TRUE;
		CapturedBase = AlignedCapturedBase;
		CapturedSize = AlignedCapturedSize;
	}

	// If we reach here then MEM_COMMIT was specified, so we will also have to allocate physical memory for the allocation and
	// write the PTEs/PFNs

	AlignedCapturedBase = ROUND_DOWN_4K(CapturedBase);
	AlignedCapturedSize = (PAGES_SPANNED(CapturedBase, CapturedSize)) << PAGE_SHIFT;

	BOOLEAN Overflow;
	VAD_NODE *Node = CheckConflictingVAD(AlignedCapturedBase, AlignedCapturedSize, &Overflow);
	if ((Node == nullptr) || Overflow) {
		// The specified region is not completely inside a reserved VAD or it's free

		if (DestructVadOnFailure) {
			VirtualMemoryBytesReserved -= AlignedCapturedSize;
			BOOLEAN Deleted = DestructVAD(AlignedCapturedBase, AlignedCapturedSize); // can't fail
			assert(Deleted);
		}

		VadUnlock();
		return STATUS_CONFLICTING_ADDRESSES;
	}

	if (AllocationType == MEM_RESET) {
		// MEM_RESET is a no-operation since it implies having page file support, which the Xbox doesn't have

		*BaseAddress = (PULONG)AlignedCapturedBase;
		*AllocationSize = AlignedCapturedSize;

		VadUnlock();
		return STATUS_SUCCESS;
	}

	KIRQL OldIrql = MiLock();

	// Figure out the number of physical pages we need to allocate. Note that NtAllocateVirtualMemory can do overlapped allocations so we
	// cannot just page-shift the size to know this number, since a part of those pages could already be allocated
	PMMPTE PointerPte = GetPteAddress(AlignedCapturedBase);
	PMMPTE EndingPte = GetPteAddress(AlignedCapturedBase + AlignedCapturedSize - 1);
	PMMPTE StartingPte = PointerPte;
	BOOLEAN UpdatePteProtections = FALSE;
	PFN_COUNT PteNumber = 0;

	while (PointerPte <= EndingPte) {
		if ((PointerPte == StartingPte) || IsPteOnPdeBoundary(PointerPte)) {
			PMMPTE PointerPde = GetPteAddress(PointerPte);
			if ((PointerPde->Hw & PTE_VALID_MASK) == 0) {
				// PDE is invalid, so we need to commit an extra page for the page table
				++PteNumber;
				// Also count the pages to commit for the PDE
				PMMPTE NextPointerPte = (PMMPTE)GetVAddrMappedByPte(PointerPde + 1);
				if (NextPointerPte > EndingPte) {
					PteNumber += (EndingPte - PointerPte + 1);
				}
				else {
					PteNumber += (NextPointerPte - PointerPte);
				}

				PointerPte = NextPointerPte;
				continue;
			}
		}

		if (PointerPte->Hw == 0) {
			++PteNumber;
		}
		else if ((PointerPte->Hw & PTE_VALID_PROTECTION_MASK) != TempPte.Hw) {
			UpdatePteProtections = TRUE;
		}

		++PointerPte;
	}

	if (PteNumber < MiRetailRegion.PagesAvailable) {
		if (MiAllowNonDebuggerOnTop64MiB) {
			if (PteNumber >= MiDevkitRegion.PagesAvailable) {
				goto EnoughPages;
			}
		}

		MiUnlock(OldIrql);

		if (DestructVadOnFailure) {
			VirtualMemoryBytesReserved -= AlignedCapturedSize;
			BOOLEAN Deleted = DestructVAD(AlignedCapturedBase, AlignedCapturedSize); // can't fail
			assert(Deleted);
		}

		VadUnlock();
		return STATUS_NO_MEMORY;
	}
	EnoughPages:

	PFN_COUNT(*PfnAllocationRoutine)() = MiAllowNonDebuggerOnTop64MiB ? MiRemoveAnyPageFromFreeList : MiRemoveRetailPageFromFreeList;
	VOID(*PageAllocationRoutine)(PFN_NUMBER, PageType, PMMPTE) = AllocationType & MEM_NOZERO ?
		(VOID(*)(PFN_NUMBER, PageType, PMMPTE))MiRemovePageFromFreeList : MiRemoveAndZeroPageFromFreeList;
	PageType BusyType = (Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
		| PAGE_EXECUTE_WRITECOPY)) ? Image : VirtualMemory;
	PointerPte = StartingPte;

	while (PointerPte <= EndingPte) {
		if ((PointerPte == StartingPte) || IsPteOnPdeBoundary(PointerPte)) {
			PMMPTE PointerPde = GetPteAddress(PointerPte);
			if ((PointerPde->Hw & PTE_VALID_MASK) == 0) {
				MiRemoveAndZeroPageTableFromFreeList(PfnAllocationRoutine(), VirtualPageTable);
			}
		}

		if (PointerPte->Hw == 0) {
			PageAllocationRoutine(PfnAllocationRoutine(), BusyType, PointerPte);
		}

		++PointerPte;
	}

	*BaseAddress = (PULONG)AlignedCapturedBase;
	*AllocationSize = AlignedCapturedSize;

	MiUnlock(OldIrql);
	VadUnlock();

	// If some PTEs were detected to have different permissions in the above check, we need to update them as well
	if (UpdatePteProtections) {
		ULONG TempProtect;
		NTSTATUS Status = NtProtectVirtualMemory((PVOID *)&AlignedCapturedBase, &AlignedCapturedSize, Protect, &TempProtect); // can't fail
		assert(NT_SUCCESS(Status));
	}

	return STATUS_SUCCESS;
}

EXPORTNUM(199) NTSTATUS XBOXAPI NtFreeVirtualMemory
(
	PVOID *BaseAddress,
	PULONG FreeSize,
	ULONG FreeType
)
{
	RIP_UNIMPLEMENTED();

	return STATUS_SUCCESS;
}

EXPORTNUM(204) NTSTATUS XBOXAPI NtProtectVirtualMemory
(
	PVOID *BaseAddress,
	PSIZE_T RegionSize,
	ULONG NewProtect,
	PULONG OldProtect
)
{
	RIP_UNIMPLEMENTED();

	return STATUS_SUCCESS;
}
