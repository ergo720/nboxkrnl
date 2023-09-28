/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "ob.hpp"

#define IsHandleOnSegmentBoundary(Handle)  (((ULONG_PTR)(Handle) & (OB_HANDLES_PER_SEGMENT * sizeof(HANDLE) - 1)) == 0)
#define HandleToUlong(Handle)              ((ULONG)(ULONG_PTR)(Handle))
#define UlongToHandle(Ulong)               ((HANDLE)(ULONG_PTR)(Ulong))
#define EncodeFreeHandle(Handle)           (HandleToUlong(Handle) | 1)
#define DecodeFreeHandle(Handle)           (HandleToUlong(Handle) & ~1)


BOOLEAN ObpAllocateNewHandleTable();
