/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "mm.hpp"


enum VAD_TYPE {
	Free,
	Reserved
};

struct VAD {
	ULONG m_Start = 0;
	ULONG m_Size = 0;
	VAD_TYPE m_Type = Free;
	// Initial permissions of the block, only used by NtQueryVirtualMemory
	ULONG m_Protect = PAGE_NOACCESS;
	// Tests if this block can be merged to the right with 'Next'
	BOOLEAN CanBeMergedWith(const VAD &Next) const;
};

struct VAD_NODE {
	ULONG m_Start;
	VAD m_Vad;
	int m_Height;
	VAD_NODE *m_Left, *m_Right;
	VOID Init(ULONG Start, ULONG Size, VAD_TYPE Type, ULONG Protect);
	int Compare(ULONG Start);
};

BOOLEAN InsertVADNode(ULONG Start, ULONG Size, VAD_TYPE Type, ULONG Protect, VAD_NODE **InsertedNode = nullptr);
VOID EraseVADNode(ULONG Start);
VAD_NODE *GetNextVAD(VAD_NODE *Node);
VAD_NODE *GetPrevVAD(VAD_NODE *Node);
VAD_NODE *GetVADNode(ULONG Start);
VAD_NODE *CheckConflictingVAD(ULONG Addr, ULONG Size, BOOLEAN *Overflow);
BOOLEAN ConstructVAD(ULONG Start, ULONG Size, ULONG Protect);
BOOLEAN DestructVAD(ULONG Addr, ULONG Size);

inline VAD_NODE *MiVadRoot = nullptr;
inline VAD_NODE *MiLastFree = nullptr;
