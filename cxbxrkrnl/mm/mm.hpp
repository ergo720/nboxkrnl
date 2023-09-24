/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include "..\types.hpp"

#define KiB(x) ((x) *    1024 ) // = 0x00000400
#define MiB(x) ((x) * KiB(1024)) // = 0x00100000

#define PAGE_SHIFT                          12 // 2^12 = 4 KiB
#define PAGE_SIZE                           (1 << PAGE_SHIFT) // = 0x00001000 = KiB(4)
#define PAGE_MASK                           (PAGE_SIZE - 1)
#define PAGE_LARGE_SHIFT                    22 // 2^22 = 4 MiB
#define PAGE_LARGE_SIZE                     (1 << PAGE_LARGE_SHIFT) // = 0x00400000 = 4 MiB
#define PAGE_LARGE_MASK                     (PAGE_LARGE_SIZE - 1)
#define PTE_PER_PAGE                        1024

// Page masks
#define PAGE_NOACCESS          0x01
#define PAGE_READONLY          0x02
#define PAGE_READWRITE         0x04
#define PAGE_EXECUTE           0x10
#define PAGE_EXECUTE_READ      0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_GUARD             0x100
#define PAGE_NOCACHE           0x200
#define PAGE_WRITECOMBINE      0x400

#define NV2A_INSTANCE_PAGE_COUNT            16
#define PAGE_DIRECTORY_PHYSICAL_ADDRESS     0x0F000

 // Common page calculations
#define ROUND_UP_4K(size)                   (((size) + PAGE_MASK) & (~PAGE_MASK))
#define ROUND_UP(size, alignment)           (((size) + (alignment - 1)) & (~(alignment - 1)))
#define ROUND_DOWN_4K(size)                 ((size) & (~PAGE_MASK))
#define ROUND_DOWN(size, alignment)         ((size) & (~(alignment - 1)))
#define PAGES_SPANNED(Va, Size)             ((ULONG)((((ULONG_PTR)(Va) & (PAGE_SIZE - 1)) + (Size) + (PAGE_SIZE - 1)) >> PAGE_SHIFT))
#define PAGES_SPANNED_LARGE(Va, Size)       ((ULONG)((((ULONG_PTR)(Va) & (PAGE_LARGE_SIZE - 1)) + (Size) + (PAGE_LARGE_SIZE - 1)) >> PAGE_LARGE_SHIFT))
#define CHECK_ALIGNMENT(size, alignment)    (((size) % (alignment)) == 0)

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

// Memory ranges
#define LOWEST_USER_ADDRESS                 0x00010000
#define HIGHEST_USER_ADDRESS                0x7FFEFFFF
#define HIGHEST_VAD_ADDRESS                 (HIGHEST_USER_ADDRESS - KiB(64)) // for NtAllocateVirtualMemory
#define USER_MEMORY_SIZE                    (HIGHEST_USER_ADDRESS - LOWEST_USER_ADDRESS + 1) // 0x7FFE0000 = 2 GiB - 128 KiB

#define PHYSICAL_MAP_BASE                   0x80000000
#define PHYSICAL_MAP_SIZE                   (MiB(256)) // = 0x10000000
#define PHYSICAL_MAP_END                    (PHYSICAL_MAP_BASE + PHYSICAL_MAP_SIZE - 1) // 0x8FFFFFFF

#define CONTIGUOUS_MEMORY_BASE              PHYSICAL_MAP_BASE
#define XBOX_CONTIGUOUS_MEMORY_SIZE         XBOX_MEMORY_SIZE
#define CHIHIRO_CONTIGUOUS_MEMORY_SIZE      CHIHIRO_MEMORY_SIZE

#define DEVKIT_MEMORY_BASE                  0xB0000000
#define DEVKIT_MEMORY_SIZE                  (MiB(256)) // = 0x10000000
#define DEVKIT_MEMORY_END                   (DEVKIT_MEMORY_BASE + DEVKIT_MEMORY_SIZE - 1) // 0xBFFFFFFF

#define PAGE_TABLES_BASE                    0xC0000000
#define PAGE_TABLES_SIZE                    (MiB(4)) // = 0x00400000
#define PAGE_TABLES_END                     (PAGE_TABLES_BASE + PAGE_TABLES_SIZE - 1) // 0xC03FFFFF

#define PAGE_DIRECTORY_BASE                 0xC0300000

#define SYSTEM_MEMORY_BASE                  0xD0000000
#define SYSTEM_MEMORY_SIZE                  (MiB(512)) // = 0x20000000
#define SYSTEM_MEMORY_END                   (SYSTEM_MEMORY_BASE + SYSTEM_MEMORY_SIZE - 1) // 0xEFFFFFFF

#define WRITE_COMBINED_BASE                 0xF0000000 // WC (The WC memory is another name of the tiled memory)
#define WRITE_COMBINED_SIZE                 (MiB(128)) // = 0x08000000
#define WRITE_COMBINED_END                  (WRITE_COMBINED_BASE + WRITE_COMBINED_SIZE - 1) // 0xF7FFFFFF

#define UNCACHED_BASE                       0xF8000000 // UC
#define UNCACHED_SIZE                       (MiB(128)) // = 0x08000000
#define UNCACHED_END                        (UNCACHED_BASE + UNCACHED_SIZE - 1) // 0xFFFFFFFF

#define XBOX_PFN_ADDRESS                    ((XBOX_PFN_DATABASE_PHYSICAL_PAGE << PAGE_SHIFT) + (PCHAR)PHYSICAL_MAP_BASE)
#define CHIHIRO_PFN_ADDRESS                 ((CHIHIRO_PFN_DATABASE_PHYSICAL_PAGE << PAGE_SHIFT) + (PCHAR)PHYSICAL_MAP_BASE)

// These macros check if the supplied address is inside a known range
#define IS_PHYSICAL_ADDRESS(Va) (((ULONG)(Va) - PHYSICAL_MAP_BASE) <= (PHYSICAL_MAP_END - PHYSICAL_MAP_BASE))
#define IS_SYSTEM_ADDRESS(Va) (((ULONG)(Va) - SYSTEM_MEMORY_BASE) <= (SYSTEM_MEMORY_END - SYSTEM_MEMORY_BASE))
#define IS_DEVKIT_ADDRESS(Va) (((ULONG)(Va) - DEVKIT_MEMORY_BASE) <= (DEVKIT_MEMORY_END - DEVKIT_MEMORY_BASE))
#define IS_USER_ADDRESS(Va) (((ULONG)(Va) - LOWEST_USER_ADDRESS) <= (HIGHEST_USER_ADDRESS - LOWEST_USER_ADDRESS))


struct MMGLOBALDATA {
	PVOID RetailPfnRegion;
	PVOID SystemPteRange;
	PVOID AvailablePages;
	PVOID AllocatedPagesByUsage;
	PVOID AddressSpaceLock;
	PVOID *VadRoot;
	PVOID *VadHint;
	PVOID *VadFreeHint;
};

#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(102) DLLEXPORT extern MMGLOBALDATA MmGlobalData;

EXPORTNUM(167) DLLEXPORT PVOID XBOXAPI MmAllocateSystemMemory
(
	ULONG NumberOfBytes,
	ULONG Protect
);

EXPORTNUM(172) DLLEXPORT ULONG XBOXAPI MmFreeSystemMemory
(
	PVOID BaseAddress,
	ULONG NumberOfBytes
);

#ifdef __cplusplus
}
#endif

inline ULONG MmSystemMaxMemory = XBOX_MEMORY_SIZE;

VOID MmInitSystem();
