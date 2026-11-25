/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2023
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
