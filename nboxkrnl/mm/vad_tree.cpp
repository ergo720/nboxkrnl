/*
 * ergo720                Copyright (c) 2023
 */

#include "vad_tree.hpp"
#include "ex.hpp"
#include <assert.h>


BOOLEAN VAD::CanBeMergedWith(const VAD &Next) const
{
	assert(m_Start + m_Size == Next.m_Start);

	if ((m_Type == Free) && (Next.m_Type == Free)) {
		return TRUE;
	}

	return FALSE;
}

VOID VAD_NODE::Init(ULONG Start, ULONG Size, VAD_TYPE Type, ULONG Protect)
{
	m_Start = m_Vad.m_Start = Start;
	m_Vad.m_Size = Size;
	m_Vad.m_Type = Type;
	m_Vad.m_Protect = Protect;
	m_Height = 0;
	m_Left = m_Right = nullptr;
}

int VAD_NODE::Compare(ULONG Start)
{
	if (m_Start < Start) {
		return -1;
	}
	else if (m_Start == Start) {
		return 0;
	}
	else {
		return 1;
	}
}

static int CalcHeight(VAD_NODE *Node)
{
	int HeightL = Node->m_Left ? Node->m_Left->m_Height : -1;
	int HeightR = Node->m_Right ? Node->m_Right->m_Height : -1;

	return (HeightL > HeightR ? HeightL : HeightR) + 1;
}

static int CalcBalance(VAD_NODE *Node)
{
	int HeightL = Node->m_Left ? Node->m_Left->m_Height : -1;
	int HeightR = Node->m_Right ? Node->m_Right->m_Height : -1;

	return HeightL - HeightR;
}

static VAD_NODE *RotateL(VAD_NODE *Node)
{
	VAD_NODE *NodeR = Node->m_Right;
	VAD_NODE *NodeRL = NodeR->m_Left;
	NodeR->m_Left = Node;
	Node->m_Right = NodeRL;

	Node->m_Height = CalcHeight(Node);
	NodeR->m_Height = CalcHeight(NodeR);

	return NodeR;
}

static VAD_NODE *RotateR(VAD_NODE *Node)
{
	VAD_NODE *NodeL = Node->m_Left;
	VAD_NODE *NodeLR = NodeL->m_Right;
	NodeL->m_Right = Node;
	Node->m_Left = NodeLR;

	Node->m_Height = CalcHeight(Node);
	NodeL->m_Height = CalcHeight(NodeL);

	return NodeL;
}

static VAD_NODE *RotateLR(VAD_NODE *Node)
{
	Node->m_Left = RotateL(Node->m_Left);
	return RotateR(Node);
}

static VAD_NODE *RotateRL(VAD_NODE *Node)
{
	Node->m_Right = RotateR(Node->m_Right);
	return RotateL(Node);
}

static VOID ReplaceParent(VAD_NODE *Parent, VAD_NODE *Child)
{
	if (Parent->m_Left == Child) {
		Parent->m_Start = Child->m_Start;
		Parent->m_Vad = Child->m_Vad;
		ExFreePool(Child);
		Parent->m_Left = nullptr;
	}
	else if (Parent->m_Right == Child) {
		Parent->m_Start = Child->m_Start;
		Parent->m_Vad = Child->m_Vad;
		ExFreePool(Child);
		Parent->m_Right = nullptr;
	}
}

static VAD_NODE *InsertVADNode(VAD_NODE *Node, ULONG Start, ULONG Size, VAD_TYPE Type, ULONG Protect, VAD_NODE **InsertedNode)
{
	if (Node == nullptr) {
		Node = (VAD_NODE *)ExAllocatePool(sizeof(VAD_NODE));
		if (Node == nullptr) {
			ExRaiseStatus(STATUS_NO_MEMORY);
		}
		Node->Init(Start, Size, Type, Protect);
		if (InsertedNode) {
			*InsertedNode = Node;
		}
		return Node;
	}

	int Ret = Node->Compare(Start);
	if (Ret < 0) {
		Node->m_Right = InsertVADNode(Node->m_Right, Start, Size, Type, Protect, InsertedNode);
	}
	else if (Ret == 0) {
		// Don't allow duplicate nodes
		Node->m_Vad.m_Start = Start;
		Node->m_Vad.m_Size = Size;
		Node->m_Vad.m_Type = Type;
		Node->m_Vad.m_Protect = Protect;
		if (InsertedNode) {
			*InsertedNode = Node;
		}
		return Node;
	}
	else {
		Node->m_Left = InsertVADNode(Node->m_Left, Start, Size, Type, Protect, InsertedNode);
	}

	Node->m_Height = CalcHeight(Node);

	int Balance = CalcBalance(Node);

	if (Balance > 1 && CalcBalance(Node->m_Left) >= 0) {
		return RotateR(Node);
	}

	if (Balance > 1 && CalcBalance(Node->m_Left) < 0) {
		return RotateLR(Node);
	}

	if (Balance < -1 && CalcBalance(Node->m_Right) <= 0) {
		return RotateL(Node);
	}

	if (Balance < -1 && CalcBalance(Node->m_Right) > 0) {
		return RotateRL(Node);
	}

	return Node;
}

static VAD_NODE *EraseVADNode(VAD_NODE *Node, ULONG Start)
{
	if (Node == nullptr) {
		return nullptr;
	}

	int ret = Node->Compare(Start);
	if (ret < 0) {
		Node->m_Right = EraseVADNode(Node->m_Right, Start);
	}
	else if (ret > 0) {
		Node->m_Left = EraseVADNode(Node->m_Left, Start);
	}
	else {
		if (Node->m_Left && Node->m_Right) {
			VAD_NODE *Node1 = Node->m_Right;
			while (Node1->m_Left) {
				Node1 = Node1->m_Left;
			}
			Node->m_Start = Node1->m_Start;
			Node->m_Right = EraseVADNode(Node->m_Right, Node1->m_Start);
		}
		else if (Node->m_Left) {
			ReplaceParent(Node, Node->m_Left);
		}
		else if (Node->m_Right) {
			ReplaceParent(Node, Node->m_Right);
		}
		else {
			ExFreePool(Node);
			Node = nullptr;
		}
	}

	if (Node != nullptr) {
		Node->m_Height = CalcHeight(Node);

		int balance = CalcBalance(Node);

		if (balance > 1 && CalcBalance(Node->m_Left) >= 0) {
			return RotateR(Node);
		}

		if (balance > 1 && CalcBalance(Node->m_Left) < 0) {
			return RotateLR(Node);
		}

		if (balance < -1 && CalcBalance(Node->m_Right) <= 0) {
			return RotateL(Node);
		}

		if (balance < -1 && CalcBalance(Node->m_Right) > 0) {
			return RotateRL(Node);
		}
	}

	return Node;
}

VAD_NODE *GetNextVAD(VAD_NODE *Node)
{
	if (Node == nullptr) {
		return nullptr;
	}

	if (Node->m_Right) {
		Node = Node->m_Right;
		while (Node->m_Left) {
			Node = Node->m_Left;
		}

		return Node;
	}
	else {
		VAD_NODE *Successor = nullptr;
		VAD_NODE *Node1 = MiVadRoot;
		while (Node1) {
			if (Node->m_Start < Node1->m_Start) {
				Successor = Node1;
				Node1 = Node1->m_Left;
			}
			else if (Node->m_Start > Node1->m_Start) {
				Node1 = Node1->m_Right;
			}
			else {
				break;
			}
		}

		return Successor;
	}
}

VAD_NODE *GetPrevVAD(VAD_NODE *Node)
{
	if (Node == nullptr) {
		Node = MiVadRoot;
		while (Node->m_Right) {
			Node = Node->m_Right;
		}

		return Node;
	}

	VAD_NODE *Predecessor = nullptr;
	VAD_NODE *Node1 = MiVadRoot;
	while (Node1) {
		if (Node->m_Start < Node1->m_Start) {
			Node1 = Node1->m_Left;
		}
		else if (Node->m_Start > Node1->m_Start) {
			Predecessor = Node1;
			Node1 = Node1->m_Right;
		}
		else {
			if (Node1->m_Left) {
				Node1 = Node1->m_Left;
				while (Node1->m_Right) {
					Node1 = Node1->m_Right;
				}

				return Node1;
			}
			break;
		}
	}

	return Predecessor;
}

static VAD_NODE *SplitVAD(VAD_NODE *Node, ULONG OffsetInVad)
{
	VAD &Old_Vad = Node->m_Vad;
	VAD NewVad = Old_Vad; // make a copy of the vma

	// Don't allow splitting at a boundary (CarveVAD ensures this)
	assert(OffsetInVad < Old_Vad.m_Size);
	assert(OffsetInVad > 0);

	Old_Vad.m_Size = OffsetInVad;
	NewVad.m_Start += OffsetInVad;
	NewVad.m_Size -= OffsetInVad;

	// Add the new split vad to the tree
	VAD_NODE *InsertedVad;
	if (InsertVADNode(NewVad.m_Start, NewVad.m_Size, NewVad.m_Type, NewVad.m_Protect, &InsertedVad)) {
		return InsertedVad;
	}

	return nullptr;
}

static VAD_NODE *CarveVAD(ULONG Start, ULONG Size)
{
	VAD_NODE *Node = GetVADNode(Start);

	// Start address is outside the user region
	assert(Node != nullptr);

	VAD &Vad = Node->m_Vad;
	ULONG OldSize = Vad.m_Size;

	// Vad is already reserved
	assert(Vad.m_Type == Free);

	ULONG StartInVad = Start - Vad.m_Start; // start addr of the new vad after the split (existing one must be free)
	ULONG EndInVad = StartInVad + Size; // end addr of new vad after the split

	// Requested allocation doesn't fit inside the vad
	assert(EndInVad <= Vad.m_Size);

	BOOLEAN SplitAtEnd = FALSE;
	if (EndInVad != Vad.m_Size) {
		if (SplitVAD(Node, EndInVad) == nullptr) {
			// Undo the split at the end that was done above
			Vad.m_Size = OldSize;
			return nullptr;
		}
		SplitAtEnd = TRUE;
	}

	if (StartInVad) {
		Node = SplitVAD(Node, StartInVad);
		if (Node == nullptr) {
			if (SplitAtEnd) {
				// Undo the split at the end that was done above and erase the newly added vad
				Vad.m_Size = OldSize;
				EraseVADNode(Vad.m_Start + EndInVad);
			}

			return nullptr;
		}
	}

	return Node;
}

static VAD_NODE *CarveVADRange(ULONG Start, ULONG Size)
{
	ULONG TargetEnd = Start + Size;
	assert(TargetEnd > Start);

	VAD_NODE *BeginVad = GetVADNode(Start);
	VAD &Vad = BeginVad->m_Vad;
	ULONG OldSize = Vad.m_Size;
	ULONG StartInVad = Start - Vad.m_Start;

	BOOLEAN SplitAtBeginning = FALSE;
	if (Start != BeginVad->m_Vad.m_Start) {
		BeginVad = SplitVAD(BeginVad, StartInVad);
		if (BeginVad == nullptr) {
			// Undo the split at the beginning that was done above
			Vad.m_Size = OldSize;
			return nullptr;
		}
		SplitAtBeginning = TRUE;
	}

	VAD_NODE *EndVad = GetVADNode(TargetEnd);
	if (TargetEnd != (EndVad->m_Vad.m_Start + EndVad->m_Vad.m_Size)) {
		EndVad = SplitVAD(EndVad, TargetEnd - EndVad->m_Vad.m_Start);
		if (EndVad == nullptr) {
			if (SplitAtBeginning) {
				// Undo the split at the beginning that was done above and erase the newly added vad
				Vad.m_Size = OldSize;
				EraseVADNode(Vad.m_Start + StartInVad);
			}

			return nullptr;
		}
	}

	return BeginVad;
}

static VAD_NODE *MergeAdjacentVAD(VAD_NODE *Node)
{
	VAD_NODE *NextNode = GetNextVAD(Node);
	if (NextNode && Node->m_Vad.CanBeMergedWith(NextNode->m_Vad)) {
		Node->m_Vad.m_Size += NextNode->m_Vad.m_Size;
		ULONG Start = Node->m_Start;
		EraseVADNode(NextNode->m_Start);
		Node = GetVADNode(Start);
	}

	if (Node != GetVADNode(LOWEST_USER_ADDRESS)) {
		VAD_NODE *PrevNode = GetPrevVAD(Node);
		if (PrevNode->m_Vad.CanBeMergedWith(Node->m_Vad)) {
			PrevNode->m_Vad.m_Size += Node->m_Vad.m_Size;
			ULONG PrevStart = PrevNode->m_Start;
			EraseVADNode(Node->m_Start);
			Node = GetVADNode(PrevStart);
		}
	}

	return Node;
}

static VAD_NODE *UnmapVAD(VAD_NODE *Node)
{
	VAD &Vad = Node->m_Vad;
	Vad.m_Type = Free;
	Vad.m_Protect = PAGE_NOACCESS;

	return MergeAdjacentVAD(Node);
}

VAD_NODE *GetVADNode(ULONG Start)
{
	VAD_NODE *Node1 = nullptr, *Parent = nullptr;
	VAD_NODE *Node2 = MiVadRoot;
	while (Node2) {
		Parent = Node2;
		if (Start < Node2->m_Start) {
			Node1 = Node2, Node2 = Node2->m_Left;
		}
		else {
			Node2 = Node2->m_Right;
		}
	}

	if (Node1 == nullptr) {
		return Parent;
	}
	else {
		return GetPrevVAD(Node1);
	}
}

BOOLEAN InsertVADNode(ULONG Start, ULONG Size, VAD_TYPE Type, ULONG Protect, VAD_NODE **InsertedNode)
{
	__try {
		// If InsertVADNode fails to allocate memory for the new node, it raises a STATUS_NO_MEMORY exception, so guard this call
		MiVadRoot = InsertVADNode(MiVadRoot, Start, Size, Type, Protect, InsertedNode);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return FALSE;
	}

	return TRUE;
}

VOID EraseVADNode(ULONG Start)
{
	MiVadRoot = EraseVADNode(MiVadRoot, Start);
}

VAD_NODE *CheckConflictingVAD(ULONG Addr, ULONG Size, BOOLEAN *Overflow)
{
	*Overflow = FALSE;
	VAD_NODE *Node = GetVADNode(Addr);

	if (Node == nullptr) {
		// Pretend we are overflowing since an overflow is always an error. Otherwise, nullptr will be interpreted
		// as a found free VAD.

		*Overflow = TRUE;
		return Node;
	}

	if (Size && ((Node->m_Start + Node->m_Vad.m_Size - 1) < (Addr + Size - 1))) {
		*Overflow = TRUE;
	}

	if (Node->m_Vad.m_Type != Free) {
		return Node; // conflict
	}

	return nullptr; // no conflict
}

BOOLEAN ConstructVAD(ULONG Start, ULONG Size, ULONG Protect)
{
	VAD_NODE *CarvedNode = CarveVAD(Start, Size);
	if (CarvedNode == nullptr) {
		return FALSE;
	}

	VAD &Vad = CarvedNode->m_Vad;
	Vad.m_Type = Reserved;
	Vad.m_Protect = Protect;

	// Depending on the splitting done by CarveVMA and the type of the adjacent VADs, there are no guarantees that the next
	// or previous VADs are free. We are just going to iterate forward and backward until we find one.

	VAD_NODE *Node = GetNextVAD(CarvedNode);
	VAD_NODE *NodeBegin = GetVADNode(LOWEST_USER_ADDRESS);
	VAD_NODE *NodeEnd = nullptr;

	while (Node != NodeEnd) {
		if (Node->m_Vad.m_Type == Free) {
			MiLastFree = Node;
			return TRUE;
		}

		Node = GetNextVAD(Node);
	}

	if (CarvedNode == NodeBegin) {
		MiLastFree = nullptr;
		return TRUE;
	}

	Node = GetPrevVAD(CarvedNode);

	while (true) {
		if (Node->m_Vad.m_Type == Free) {
			MiLastFree = Node;
			return TRUE;
		}

		if (Node == NodeBegin) {
			break;
		}

		Node = GetPrevVAD(Node);
	}

	MiLastFree = nullptr;

	return TRUE;
}

BOOLEAN DestructVAD(ULONG Addr, ULONG Size)
{
	VAD_NODE *CarvedNode = CarveVADRange(Addr, Size);
	if (CarvedNode == nullptr) {
		return FALSE;
	}

	ULONG TargetEnd = Addr + Size;
	while (CarvedNode && (CarvedNode->m_Vad.m_Start < TargetEnd)) {
		CarvedNode = GetNextVAD(UnmapVAD(CarvedNode));
	}

	// If we free an entire VAD, GetPrevVAD(CarvedNode) will be the freed VAD. If it is not, we'll do a standard search
	VAD_NODE *NodeBegin = GetVADNode(LOWEST_USER_ADDRESS);
	VAD_NODE *PrevNode = GetPrevVAD(CarvedNode);
	if ((CarvedNode != NodeBegin) && (PrevNode->m_Vad.m_Type == Free)) {
		MiLastFree = PrevNode;
		return TRUE;
	}
	else {
		VAD_NODE *Node = CarvedNode;
		while (Node) {
			if (Node->m_Vad.m_Type == Free) {
				MiLastFree = Node;
				return TRUE;
			}

			Node = GetNextVAD(Node);
		}

		if (CarvedNode == NodeBegin) {
			MiLastFree = nullptr;
			return TRUE;
		}

		Node = PrevNode;

		while (true) {
			if (Node->m_Vad.m_Type == Free) {
				MiLastFree = Node;
				return TRUE;
			}

			if (Node == NodeBegin) {
				break;
			}

			Node = GetPrevVAD(Node);
		}

		MiLastFree = nullptr;

		return TRUE;
	}
}
