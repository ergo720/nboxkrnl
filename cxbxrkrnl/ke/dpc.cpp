/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2016
 */

#include "ke.hpp"


EXPORTNUM(107) VOID XBOXAPI KeInitializeDpc
(
	PKDPC Dpc,
	PKDEFERRED_ROUTINE DeferredRoutine,
	PVOID DeferredContext
)
{
	Dpc->Type = DpcObject;
	Dpc->Inserted = FALSE;
	Dpc->DeferredRoutine = DeferredRoutine;
	Dpc->DeferredContext = DeferredContext;
}
