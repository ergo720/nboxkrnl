/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include "..\types.h"

#define KiB(x) ((x) *    1024 ) // = 0x00000400
#define MiB(x) ((x) * KiB(1024)) // = 0x00100000

#define PAGE_SHIFT                          12 // 2^12 = 4 KiB
#define PAGE_SIZE                           (1 << PAGE_SHIFT) // = 0x00001000 = KiB(4)
#define PAGE_MASK                           (PAGE_SIZE - 1)
#define PAGE_LARGE_SHIFT                    22 // 2^22 = 4 MiB
#define PAGE_LARGE_SIZE                     (1 << PAGE_LARGE_SHIFT) // = 0x00400000 = 4 MiB
#define PAGE_LARGE_MASK                     (PAGE_LARGE_SIZE - 1)

#define NV2A_INSTANCE_PAGE_COUNT            16
#define PAGE_DIRECTORY_PHYSICAL_ADDRESS     0x0F000

 // Common page calculations
#define ROUND_UP_4K(size)                   (((size) + PAGE_MASK) & (~PAGE_MASK))
#define ROUND_UP(size, alignment)           (((size) + (alignment - 1)) & (~(alignment - 1)))
#define ROUND_DOWN_4K(size)                 ((size) & (~PAGE_MASK))
#define ROUND_DOWN(size, alignment)         ((size) & (~(alignment - 1)))
#define PAGES_SPANNED(Va, Size)             ((ULONG)((((ULONG_PTR)(Va) & (PAGE_SIZE - 1)) + (Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))
#define PAGES_SPANNED_LARGE(Va, Size)       ((ULONG)((((ULONG_PTR)(Va) & (PAGE_LARGE_SIZE - 1)) + (Size) + (PAGE_LARGE_SIZE - 1)) >> PAGE_LARGE_SHIFT))

// Memory size per system
#define XBOX_MEMORY_SIZE                    (MiB(64))
#define CHIHIRO_MEMORY_SIZE                 (MiB(128))

// Define page frame numbers used by the system
#define XBOX_CONTIGUOUS_MEMORY_LIMIT        0x03FDF
#define XBOX_INSTANCE_PHYSICAL_PAGE         0x03FE0
#define XBOX_PFN_DATABASE_PHYSICAL_PAGE     0x03FF0
#define XBOX_HIGHEST_PHYSICAL_PAGE          0x03FFF
#define X64M_PHYSICAL_PAGE                  0x04000
#define DEBUGKIT_FIRST_UPPER_HALF_PAGE      X64M_PHYSICAL_PAGE // = 0x4000

#define CHIHIRO_CONTIGUOUS_MEMORY_LIMIT     0x07FCF
#define CHIHIRO_PFN_DATABASE_PHYSICAL_PAGE  0x07FD0
#define CHIHIRO_INSTANCE_PHYSICAL_PAGE      0x07FF0
#define CHIHIRO_HIGHEST_PHYSICAL_PAGE       0x07FFF

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

// Memory ranges
#define PHYSICAL_MAP_BASE                   0x80000000
#define PHYSICAL_MAP_SIZE                   (MiB(256)) // = 0x10000000
#define PHYSICAL_MAP_END                    (PHYSICAL_MAP_BASE + PHYSICAL_MAP_SIZE - 1) // 0x8FFFFFFF

#define CONTIGUOUS_MEMORY_BASE              PHYSICAL_MAP_BASE
#define XBOX_CONTIGUOUS_MEMORY_SIZE         (MiB(64))
#define CHIHIRO_CONTIGUOUS_MEMORY_SIZE      (MiB(128))

#define PAGE_TABLES_BASE                    0xC0000000
#define PAGE_TABLES_SIZE                    (MiB(4)) // = 0x00400000
#define PAGE_TABLES_END                     (PAGE_TABLES_BASE + PAGE_TABLES_SIZE - 1) // 0xC03FFFFF

#define PAGE_DIRECTORY_BASE                 0xC0300000

#define WRITE_COMBINED_BASE                 0xF0000000 // WC (The WC memory is another name of the tiled memory)
#define WRITE_COMBINED_SIZE                 (MiB(128)) // = 0x08000000
#define WRITE_COMBINED_END                  (WRITE_COMBINED_BASE + WRITE_COMBINED_SIZE - 1) // 0xF7FFFFFF

#define UNCACHED_BASE                       0xF8000000 // UC
#define UNCACHED_SIZE                       (MiB(128)) // = 0x08000000
#define UNCACHED_END                        (UNCACHED_BASE + UNCACHED_SIZE - 1) // 0xFFFFFFFF

// Various macros to manipulate PDE/PTE/PFN
#define GetPdeAddress(Va) ((PMMPTE)(((((ULONG)(Va)) >> 22) << 2) + PAGE_DIRECTORY_BASE)) // (Va/4M) * 4 + PDE_BASE
#define GetPteAddress(Va) ((PMMPTE)(((((ULONG)(Va)) >> 12) << 2) + PAGE_TABLES_BASE))    // (Va/4K) * 4 + PTE_BASE
#define GetVAddrMappedByPte(Pte) ((ULONG)((ULONG_PTR)(Pte) << 10))
#define GetPteOffset(Va) ((((ULONG)(Va)) << 10) >> 22)
#define IsPteOnPdeBoundary(Pte) (((ULONG_PTR)(Pte) & (PAGE_SIZE - 1)) == 0)
#define WriteZeroPte(pPte) (*(pPte) = 0)
#define WritePte(pPte, Pte) (*(pPte) = Pte)
#define ValidKernelPteBits (PTE_VALID_MASK | PTE_WRITE_MASK | PTE_DIRTY_MASK | PTE_ACCESS_MASK) // 0x63
#define ValidKernelPdeBits (PTE_VALID_MASK | PTE_WRITE_MASK | PTE_OWNER_MASK | PTE_DIRTY_MASK | PTE_ACCESS_MASK) // 0x67
#define DisableCachingBits (PTE_WRITE_THROUGH_MASK | PTE_CACHE_DISABLE_MASK)
#define SetWriteCombine(Pte) (((Pte) & ~PTE_CACHE_DISABLE_MASK) | PTE_WRITE_THROUGH_MASK)
#define SetPfn(Addr) (ROUND_DOWN_4K((ULONG)(Addr)))
#define ConvertPfnToContiguousPhysical(Pfn) ((PCHAR)PHYSICAL_MAP_BASE + ((Pfn) << PAGE_SHIFT))
#define ConvertContiguousToPhysical(Va) ((PCHAR)((ULONG)(Va) & (PHYSICAL_MAP_SIZE - 1)))


using MMPTE = ULONG;
using PMMPTE = MMPTE *;

inline ULONG MmSystemMaxMemory = XBOX_MEMORY_SIZE;

VOID MmInitSystem();
