/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

// Workaround fix for using clang-format with WhitespaceSensitiveMacros option
// However, clang-format will not format inside the parentheses bracket.
#define ASM(...) __asm __VA_ARGS__
#define ASM_BEGIN __asm {
#define ASM_END }

#include <stdint.h>
#include <stddef.h>
#include "bug_codes.hpp"

#define TRUE 1
#define FALSE 0

#define MAXLONG     0x7fffffff

#define XBOXAPI  __stdcall
#define FASTCALL __fastcall
#define CDECL    __cdecl

#define EXPORTNUM(n)
#define DLLEXPORT __declspec(dllexport)

#define ANSI_NULL ((CHAR)0)
#define UNICODE_NULL ((WCHAR)0)


using VOID = void;
using PVOID = void *;
using BYTE = uint8_t;
using UCHAR = unsigned char;
using CHAR = char;
using WCHAR = wchar_t;
using BOOL = bool;
using SCHAR = CHAR;
using CCHAR = CHAR;
using BOOLEAN = uint8_t;
using USHORT = uint16_t;
using CSHORT = int16_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using UINT = uint32_t;
using LONG = int32_t;
using INT = int32_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using QUAD = ULONGLONG;
using PCHAR = CHAR *;
using PBYTE = BYTE *;
using PWCHAR = WCHAR *;
using PWSTR = PWCHAR;
using PUSHORT = USHORT *;
using PULONG = ULONG *;
using PULONGLONG = ULONGLONG *;
using LPDWORD = DWORD *;
using PUCHAR = UCHAR *;
using PLONG = LONG *;
using PCSZ = const CHAR *;
using ULONG_PTR = uintptr_t;
using DWORD_PTR = uintptr_t;
using LONG_PTR = intptr_t;
using PULONG_PTR = ULONG_PTR *;
using SIZE_T = ULONG_PTR;
using PSIZE_T = SIZE_T *;
using DWORDLONG = uint64_t;
using NTSTATUS = LONG;
using KPRIORITY = LONG;
using HANDLE = PVOID;
using PHANDLE = HANDLE *;
using HRESULT = LONG;
using ACCESS_MASK = ULONG;
using PACCESS_MASK = ACCESS_MASK *;

#include "helpers.hpp"
#include "ntstatus.hpp"
#include <stddef.h>

#define CONTAINING_RECORD(address, type, field) ((type *)((PCHAR)(address) - (ULONG_PTR)offsetof(type, field)))

#define LOWORD(l) ((WORD)((DWORD_PTR)(l)))
#define HIWORD(l) ((WORD)(((DWORD_PTR)(l) >> 16) & 0xFFFF))

#define MAXULONG_PTR (~((ULONG_PTR)0))
#define MAXLONG_PTR  ((LONG_PTR)(MAXULONG_PTR >> 1))
#define MINLONG_PTR  (~MAXLONG_PTR)

struct LIST_ENTRY {
	LIST_ENTRY *Flink;
	LIST_ENTRY *Blink;
};
using PLIST_ENTRY = LIST_ENTRY *;

struct SINGLE_LIST_ENTRY {
	struct _SINGLE_LIST_ENTRY *Next;
};
using SLIST_ENTRY = SINGLE_LIST_ENTRY;
using PSINGLE_LIST_ENTRY = SINGLE_LIST_ENTRY *;
using PSLIST_ENTRY = SLIST_ENTRY *;

union SLIST_HEADER {
	ULONGLONG Alignment;
	struct {
		SINGLE_LIST_ENTRY Next;
		USHORT Depth;
		USHORT Sequence;
	};
};
using PSLIST_HEADER = SLIST_HEADER *;

struct STRING {
	USHORT Length;
	USHORT MaximumLength;
	PCHAR Buffer;
};
using ANSI_STRING = STRING;
using PSTRING = STRING *;
using PANSI_STRING = PSTRING;

struct UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWCHAR Buffer;
};
using PUNICODE_STRING = UNICODE_STRING *;

struct DISPATCHER_HEADER {
	UCHAR Type;
	UCHAR Absolute;
	UCHAR Size;
	UCHAR Inserted;
	LONG SignalState;
	LIST_ENTRY WaitListHead;
};

union ULARGE_INTEGER {
	struct {
		DWORD LowPart;
		DWORD HighPart;
	};

	struct {
		DWORD LowPart;
		DWORD HighPart;
	} u;

	DWORDLONG QuadPart;
};
using PULARGE_INTEGER = ULARGE_INTEGER *;

union LARGE_INTEGER {
	struct {
		DWORD LowPart;
		LONG HighPart;
	};

	struct {
		DWORD LowPart;
		LONG HighPart;
	} u;

	LONGLONG QuadPart;
};
using PLARGE_INTEGER = LARGE_INTEGER *;

struct TIME_FIELDS {
	USHORT Year;
	USHORT Month;
	USHORT Day;
	USHORT Hour;
	USHORT Minute;
	USHORT Second;
	USHORT Millisecond;
	USHORT Weekday;
};
using PTIME_FIELDS = TIME_FIELDS *;

#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16
#define IMAGE_SIZEOF_SHORT_NAME              8

struct IMAGE_DOS_HEADER {
	WORD   e_magic;
	WORD   e_cblp;
	WORD   e_cp;
	WORD   e_crlc;
	WORD   e_cparhdr;
	WORD   e_minalloc;
	WORD   e_maxalloc;
	WORD   e_ss;
	WORD   e_sp;
	WORD   e_csum;
	WORD   e_ip;
	WORD   e_cs;
	WORD   e_lfarlc;
	WORD   e_ovno;
	WORD   e_res[4];
	WORD   e_oemid;
	WORD   e_oeminfo;
	WORD   e_res2[10];
	LONG   e_lfanew;
};
using PIMAGE_DOS_HEADER = IMAGE_DOS_HEADER *;

struct IMAGE_FILE_HEADER {
	WORD    Machine;
	WORD    NumberOfSections;
	DWORD   TimeDateStamp;
	DWORD   PointerToSymbolTable;
	DWORD   NumberOfSymbols;
	WORD    SizeOfOptionalHeader;
	WORD    Characteristics;
};
using PIMAGE_FILE_HEADER = IMAGE_FILE_HEADER *;

struct IMAGE_DATA_DIRECTORY {
	DWORD   VirtualAddress;
	DWORD   Size;
};
using PIMAGE_DATA_DIRECTORY = IMAGE_DATA_DIRECTORY *;

struct IMAGE_OPTIONAL_HEADER32 {
	WORD                 Magic;
	BYTE                 MajorLinkerVersion;
	BYTE                 MinorLinkerVersion;
	DWORD                SizeOfCode;
	DWORD                SizeOfInitializedData;
	DWORD                SizeOfUninitializedData;
	DWORD                AddressOfEntryPoint;
	DWORD                BaseOfCode;
	DWORD                BaseOfData;
	DWORD                ImageBase;
	DWORD                SectionAlignment;
	DWORD                FileAlignment;
	WORD                 MajorOperatingSystemVersion;
	WORD                 MinorOperatingSystemVersion;
	WORD                 MajorImageVersion;
	WORD                 MinorImageVersion;
	WORD                 MajorSubsystemVersion;
	WORD                 MinorSubsystemVersion;
	DWORD                Win32VersionValue;
	DWORD                SizeOfImage;
	DWORD                SizeOfHeaders;
	DWORD                CheckSum;
	WORD                 Subsystem;
	WORD                 DllCharacteristics;
	DWORD                SizeOfStackReserve;
	DWORD                SizeOfStackCommit;
	DWORD                SizeOfHeapReserve;
	DWORD                SizeOfHeapCommit;
	DWORD                LoaderFlags;
	DWORD                NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};
using PIMAGE_OPTIONAL_HEADER32 = IMAGE_OPTIONAL_HEADER32 *;

struct IMAGE_NT_HEADERS32 {
	DWORD Signature;
	IMAGE_FILE_HEADER FileHeader;
	IMAGE_OPTIONAL_HEADER32 OptionalHeader;
};
using PIMAGE_NT_HEADERS32 = IMAGE_NT_HEADERS32 *;

struct IMAGE_SECTION_HEADER {
	BYTE    Name[IMAGE_SIZEOF_SHORT_NAME];
	union {
		DWORD   PhysicalAddress;
		DWORD   VirtualSize;
	} Misc;
	DWORD   VirtualAddress;
	DWORD   SizeOfRawData;
	DWORD   PointerToRawData;
	DWORD   PointerToRelocations;
	DWORD   PointerToLinenumbers;
	WORD    NumberOfRelocations;
	WORD    NumberOfLinenumbers;
	DWORD   Characteristics;
};
using PIMAGE_SECTION_HEADER = IMAGE_SECTION_HEADER *;
