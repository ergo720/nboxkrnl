/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include <cstdint>
#include <cstddef>

#define TRUE 1
#define FALSE 0

#define XBOXAPI __stdcall


using VOID = void;
using PVOID = void *;
using BYTE = uint8_t;
using UCHAR = uint8_t;
using CHAR = int8_t;
using SCHAR = CHAR;
using CCHAR = CHAR;
using BOOLEAN = uint8_t;
using USHORT = uint16_t;
using CSHORT = int16_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using ULONG = uint32_t;
using LONG = int32_t;
using ULONG_PTR = uint32_t;
using LONG_PTR = int32_t;
using PULONG_PTR = ULONG_PTR *;
using DWORDLONG = uint64_t;

struct LIST_ENTRY {
	LIST_ENTRY *Flink;
	LIST_ENTRY *Blink;
};
using PLIST_ENTRY = LIST_ENTRY *;

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
