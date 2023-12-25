/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\mm\mi.hpp"
#include "..\io\hdd\fatx.hpp"


struct FSCACHE_ELEMENT {
	ULONG AlignedByteOffset;
	PFSCACHE_EXTENSION CacheExtension;
	union {
		struct {
			ULONG NumOfUsers : 8;
			ULONG MarkForDeletion : 1;
			ULONG WriteInProgress : 1;
			ULONG Unused : 2;
			ULONG CachePageAlignedAddr : 20;
		};
		PCHAR CacheBuffer;
	};
	LIST_ENTRY ListEntry;
};
using PFSCACHE_ELEMENT = FSCACHE_ELEMENT *;


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(37) DLLEXPORT NTSTATUS XBOXAPI FscSetCacheSize
(
	ULONG NumberOfCachePages
);

#ifdef __cplusplus
}
#endif


inline ULONG FscCurrNumberOfCachePages = 0;

NTSTATUS FscMapElementPage(PFSCACHE_EXTENSION CacheExtension, ULONGLONG ByteOffset, PVOID *ReturnedBuffer, BOOLEAN IsWrite);
VOID FscUnmapElementPage(PVOID Buffer);
