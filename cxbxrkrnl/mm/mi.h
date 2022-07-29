/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#pragma once

#include "..\kernel.h"
#include "mm.h"


using PFN = ULONG;
using PFN_COUNT = ULONG;
using PFN_NUMBER = ULONG; 

// An entry of the list tracking the free pages on the system
struct FreeBlock {
	PFN start;             // starting page of the block
	PFN_COUNT size;        // number of pages in the block
	LIST_ENTRY ListEntry;
};
using PFreeBlock = FreeBlock *;


inline bool MiLayoutRetail;
inline bool MiLayoutChihiro;
inline bool MiLayoutDevkit;

// Highest pfn available for contiguous allocations
inline PFN MiMaxContiguousPfn = XBOX_CONTIGUOUS_MEMORY_LIMIT;
// Amount of free physical pages available for non-debugger usage
inline PFN_COUNT MiPhysicalPagesAvailable = X64M_PHYSICAL_PAGE;
// Highest page on the system
inline PFN MiHighestPage = XBOX_HIGHEST_PHYSICAL_PAGE;
// First page of the nv2a instance memory
inline PFN MiNV2AInstancePage = XBOX_INSTANCE_PHYSICAL_PAGE;
// Amount of free physical pages available for usage by the debugger (devkit only)
inline PFN_COUNT MiDebuggerPagesAvailable = 0;
// Doubly linked list tracking the free physical pages
inline LIST_ENTRY MiFreeList = { &MiFreeList, &MiFreeList };

VOID MiFlushEntireTlb();
