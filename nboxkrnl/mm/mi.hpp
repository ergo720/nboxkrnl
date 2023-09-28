/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\kernel.hpp"
#include "..\ke\ke.hpp"
#include "mm.hpp"

 // Pte protection masks
#define PTE_VALID_MASK              0x00000001
#define PTE_WRITE_MASK              0x00000002
#define PTE_OWNER_MASK              0x00000004
#define PTE_WRITE_THROUGH_MASK      0x00000008
#define PTE_CACHE_DISABLE_MASK      0x00000010
#define PTE_ACCESS_MASK             0x00000020
#define PTE_DIRTY_MASK              0x00000040
#define PTE_PAGE_LARGE_MASK         0x00000080
#define PTE_GLOBAL_MASK             0x00000100
#define PTE_GUARD_END_MASK          0x00000200
#define PTE_PERSIST_MASK            0x00000400
#define PTE_NOACCESS                0x00000000
#define PTE_READONLY                PTE_VALID_MASK
#define PTE_READWRITE               PTE_WRITE_MASK
#define PTE_NOCACHE                 PTE_CACHE_DISABLE_MASK
#define PTE_GUARD                   PTE_GUARD_END_MASK
#define PTE_VALID_PROTECTION_MASK   0x0000021B // valid, write, write-through, no cache, guard/end
#define PTE_SYSTEM_PROTECTION_MASK  0x0000001B // valid, write, write-through, no cache

// Indicates that the system pte entry is not linked to other entries
#define PTE_LIST_END (ULONG)0x3FFFFFFF

// Indicates that the pfn entry is not linked to other entries
#define PFN_LIST_END (USHORT)0x7FFF
// Define how many free lists a pfn region has
#define PFN_LIST_SHIFT 1
#define PFN_NUM_LISTS (1 << PFN_LIST_SHIFT)
#define PFN_LIST_MASK (PFN_NUM_LISTS - 1)

using PFN = ULONG;
using PFN_COUNT = ULONG;
using PFN_NUMBER = ULONG;

// NOTE: the size of a free pte block is stored in the free pte immediately following Free. If the size is only one, then this is signalled by OnePte instead
union MMPTE {
	ULONG Hw;              // Used when the pte is valid
	struct {
		ULONG Present : 1; // must be clear or else the cpu will interpret the pte as valid
		ULONG OnePte : 1;  // when set, the size of the current block is one pte
		ULONG Flink : 30;  // virtual address to the next free pte block
	} Free;
};
using PMMPTE = MMPTE *;

struct PTEREGION {
	MMPTE Head;
	ULONG Next4MiBlock;
	ULONG EndAddr;
};
using PPTEREGION = PTEREGION *;

// This represents a free pfn entry, possibly linked to other free entries
/*
15th bit: must be clear or else the Busy member of the pfn will be set for a free entry
14th bit: if zero, then it's a valid link, otherwise it's the list end marker
low bits: encoded pfn index number to the next free pfn entry
00/1NN NNNN NNNN NNNN
*/
struct PFNFREE {
	USHORT Flink;
	USHORT Blink;
};

// PFN entry used by the memory manager
union XBOX_PFN {
	PFNFREE Free;              // Used when pfn is free
	struct {
		ULONG LockCount : 16;  // Set to prevent page relocation
		ULONG PteIndex : 10;   // Offset in the PT that maps the pte
		ULONG Unused : 1;
		ULONG BusyType : 4;    // What the page is used for
		ULONG Busy : 1;        // If set, PFN is in use
	} Busy;
	struct {
		ULONG LockCount : 16;  // Set to prevent page relocation
		ULONG PtesUsed : 11;   // Number of used ptes in the pt pointed by the pde
		ULONG BusyType : 4;    // What the page is used for (must be VirtualPageTableType or SystemPageTableType)
		ULONG Busy : 1;        // If set, PFN is in use
	} PtPageFrame;
};
using PXBOX_PFN = XBOX_PFN *;

struct PFNREGION {
	PFNFREE FreeListHead[PFN_NUM_LISTS];
	PFN_COUNT PagesAvailable;
};
using PPFNREGION = PFNREGION *;

enum PageType {
	Unknown,           // Used by the PFN database
	Stack,             // Used by MmCreateKernelStack
	VirtualPageTable,  // Used by the pages holding the pts that map any but system / devkit memory
	SystemPageTable,   // Used by the pages holding the pts that map the system / devkit memory
	Pool,              // Used by ExAllocatePoolWithTag
	VirtualMemory,     // Used by NtAllocateVirtualMemory
	SystemMemory,      // Used by MmAllocateSystemMemory
	Image,             // Used to mark executable memory
	Cache,             // Used by the file cache functions
	Contiguous,        // Used by MmAllocateContiguousMemoryEx and others
	Debugger,          // Used by the debugger
	Max                // The size of the array containing the page usage per type
};

// Various macros to manipulate PDE/PTE/PFN
#define GetPteBlockSize(Pte) (*((PULONG)(Pte) + 1))
#define GetPdeAddress(Va) ((PMMPTE)(((((ULONG)(Va)) >> 22) << 2) + PAGE_DIRECTORY_BASE)) // (Va/4M) * 4 + PDE_BASE
#define GetPteAddress(Va) ((PMMPTE)(((((ULONG)(Va)) >> 12) << 2) + PAGE_TABLES_BASE))    // (Va/4K) * 4 + PTE_BASE
#define GetVAddrMappedByPte(Pte) ((ULONG)((ULONG_PTR)(Pte) << 10))
#define GetPteOffset(Va) ((((ULONG)(Va)) << 10) >> 22)
#define IsPteOnPdeBoundary(Pte) (((ULONG_PTR)(Pte) & (PAGE_SIZE - 1)) == 0)
#define WriteZeroPte(pPte) (*(PULONG)(pPte) = 0)
#define WritePte(pPte, Pte) (*(PULONG)(pPte) = Pte)
#define ValidKernelPteBits (PTE_VALID_MASK | PTE_WRITE_MASK | PTE_DIRTY_MASK | PTE_ACCESS_MASK) // 0x63
#define ValidKernelPdeBits (PTE_VALID_MASK | PTE_WRITE_MASK | PTE_OWNER_MASK | PTE_DIRTY_MASK | PTE_ACCESS_MASK) // 0x67
#define DisableCachingBits (PTE_WRITE_THROUGH_MASK | PTE_CACHE_DISABLE_MASK)
#define SetWriteCombine(Pte) (((Pte) & ~PTE_CACHE_DISABLE_MASK) | PTE_WRITE_THROUGH_MASK)
#define ConvertPfnToContiguous(Pfn) ((PCHAR)PHYSICAL_MAP_BASE + ((Pfn) << PAGE_SHIFT))
#define ConvertContiguousToPhysical(Va) ((PCHAR)((ULONG)(Va) & (PHYSICAL_MAP_SIZE - 1)))
#define SetPfn(Addr) (ROUND_DOWN_4K((ULONG)(Addr)))
#define GetPfnFromContiguous(Addr) ((ULONG)ConvertContiguousToPhysical(Addr) >> PAGE_SHIFT)
#define GetPfnElement(Pfn) (&((PXBOX_PFN)MiPfnAddress)[Pfn])
#define GetPfnOfPt(Pte) (GetPfnElement(GetPteAddress(Pte)->Hw >> PAGE_SHIFT))
#define EncodeFreePfn(PfnIdx) ((USHORT)((USHORT)(PfnIdx) >> (USHORT)PFN_LIST_SHIFT))
#define DecodeFreePfn(PfnIdx, Idx) ((USHORT)(((USHORT)(PfnIdx) << (USHORT)PFN_LIST_SHIFT) + (USHORT)Idx))
#define GetPfnListIdx(Pfn) ((Pfn) & PFN_LIST_MASK)

// Macros to ensure thread safety
#define MiLock() KeRaiseIrqlToDpcLevel()
#define MiUnlock(Irql) KfLowerIrql(Irql)

inline bool MiLayoutRetail;
inline bool MiLayoutChihiro;
inline bool MiLayoutDevkit;

// Highest pfn available for contiguous allocations
inline PFN MiMaxContiguousPfn = XBOX_CONTIGUOUS_MEMORY_LIMIT;
// Highest page on the system
inline PFN MiHighestPage = XBOX_HIGHEST_PHYSICAL_PAGE;
// First page of the nv2a instance memory
inline PFN MiNV2AInstancePage = XBOX_INSTANCE_PHYSICAL_PAGE;
// Array containing the number of pages in use per type
inline PFN_COUNT MiPagesByUsage[Max] = { 0 };
// Total physical free pages currently available (retail + devkit)
inline PFN_COUNT MiTotalPagesAvailable = 0;
// Tracks free pfns for retail / chihiro
inline PFNREGION MiRetailRegion = { {{ PFN_LIST_END, PFN_LIST_END }, { PFN_LIST_END, PFN_LIST_END }}, 0 };
// Tracks free pfns for the upper 64 MiB of a devkit
inline PFNREGION MiDevkitRegion = { {{ PFN_LIST_END, PFN_LIST_END }, { PFN_LIST_END, PFN_LIST_END }}, 0 };
// Tracks free pte blocks in the system region
inline PTEREGION MiSystemPteRegion = { PTE_LIST_END << 2, SYSTEM_MEMORY_BASE, SYSTEM_MEMORY_END };
// Tracks free pte blocks in the devkit region
inline PTEREGION MiDevkitPteRegion = { PTE_LIST_END << 2, DEVKIT_MEMORY_BASE, DEVKIT_MEMORY_END };
// Start address of the pfn database
inline PCHAR MiPfnAddress = XBOX_PFN_ADDRESS;

VOID MiFlushEntireTlb();
VOID MiFlushTlbForPage(PVOID Addr);
VOID MiInsertPageInFreeList(PFN_NUMBER Pfn);
VOID MiInsertPageRangeInFreeList(PFN_NUMBER Pfn, PFN_NUMBER PfnEnd);
PXBOX_PFN MiRemovePageFromFreeList(PFN_NUMBER Pfn);
VOID MiRemovePageFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pte);
VOID MiRemoveAndZeroPageTableFromFreeList(PFN_NUMBER Pfn, PageType BusyType, BOOLEAN Unused);
VOID MiRemoveAndZeroPageTableFromFreeList(PFN_NUMBER Pfn, PageType BusyType);
VOID MiRemoveAndZeroPageFromFreeList(PFN_NUMBER Pfn, PageType BusyType, PMMPTE Pte);
PFN_NUMBER MiRemoveAnyPageFromFreeList(PageType BusyType, PMMPTE Pte);
PFN_NUMBER MiRemoveAnyPageFromFreeList();
PVOID MiAllocateSystemMemory(ULONG NumberOfBytes, ULONG Protect, PageType BusyType, BOOLEAN AddGuardPage);
ULONG MiFreeSystemMemory(PVOID BaseAddress, ULONG NumberOfBytes);
