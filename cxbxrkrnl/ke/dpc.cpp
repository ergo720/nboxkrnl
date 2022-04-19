/*
 * ergo720                Copyright (c) 2022
 * cxbxr devs
 */

#include "ke.h"


VOID XBOXAPI KeInitializeDpc
(
    PKDPC Dpc,
    PKDEFERRED_ROUTINE DeferredRoutine,
    PVOID DeferredContext
)
{
    Dpc->Type = to_underlying(KOBJECTS::DpcObject);
    Dpc->Inserted = FALSE;
    Dpc->DeferredRoutine = DeferredRoutine;
    Dpc->DeferredContext = DeferredContext;
}
