/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"

#define XBEIMAGE_MEDIA_TYPE_HARD_DISK            0x00000001
#define XBEIMAGE_MEDIA_TYPE_DVD_X2               0x00000002
#define XBEIMAGE_MEDIA_TYPE_DVD_CD               0x00000004
#define XBEIMAGE_MEDIA_TYPE_CD                   0x00000008
#define XBEIMAGE_MEDIA_TYPE_DVD_5_RO             0x00000010
#define XBEIMAGE_MEDIA_TYPE_DVD_9_RO             0x00000020
#define XBEIMAGE_MEDIA_TYPE_DVD_5_RW             0x00000040
#define XBEIMAGE_MEDIA_TYPE_DVD_9_RW             0x00000080
#define XBEIMAGE_MEDIA_TYPE_DONGLE               0x00000100
#define XBEIMAGE_MEDIA_TYPE_MEDIA_BOARD          0x00000200
#define XBEIMAGE_MEDIA_TYPE_NONSECURE_HARD_DISK  0x40000000
#define XBEIMAGE_MEDIA_TYPE_NONSECURE_MODE       0x80000000
#define XBEIMAGE_MEDIA_TYPE_MEDIA_MASK           0x00FFFFFF

#define XBEIMAGE_SECTION_WRITEABLE               0x00000001
#define XBEIMAGE_SECTION_PRELOAD                 0x00000002
#define XBEIMAGE_SECTION_EXECUTABLE              0x00000004
#define XBEIMAGE_SECTION_INSERTFILE              0x00000008
#define XBEIMAGE_SECTION_HEAD_PAGE_READONLY      0x00000010
#define XBEIMAGE_SECTION_TAIL_PAGE_READONLY      0x00000020


#pragma pack(1)
struct XBE_HEADER {
	ULONG dwMagic;                         // 0x0000 - magic number [should be "XBEH"]
	UCHAR pbDigitalSignature[256];         // 0x0004 - digital signature
	ULONG dwBaseAddr;                      // 0x0104 - base address
	ULONG dwSizeofHeaders;                 // 0x0108 - size of headers
	ULONG dwSizeofImage;                   // 0x010C - size of image
	ULONG dwSizeofImageHeader;             // 0x0110 - size of image header
	ULONG dwTimeDate;                      // 0x0114 - timedate stamp
	ULONG dwCertificateAddr;               // 0x0118 - certificate address
	ULONG dwSections;                      // 0x011C - number of sections
	ULONG dwSectionHeadersAddr;            // 0x0120 - section headers address

	struct INIT_FLAGS {
		ULONG bMountUtilityDrive : 1;    // mount utility drive flag
		ULONG bFormatUtilityDrive : 1;    // format utility drive flag
		ULONG bLimit64MB : 1;    // limit development kit run time memory to 64mb flag
		ULONG bDontSetupHarddisk : 1;    // don't setup hard disk flag
		ULONG Unused : 4;    // unused (or unknown)
		ULONG Unused_b1 : 8;    // unused (or unknown)
		ULONG Unused_b2 : 8;    // unused (or unknown)
		ULONG Unused_b3 : 8;    // unused (or unknown)
	};

	union {                                // 0x0124 - initialization flags
		INIT_FLAGS dwInitFlags;
		ULONG dwInitFlags_value;
	};

	ULONG dwEntryAddr;                     // 0x0128 - entry point address
	ULONG dwTLSAddr;                       // 0x012C - thread local storage directory address
	ULONG dwPeStackCommit;                 // 0x0130 - size of stack commit
	ULONG dwPeHeapReserve;                 // 0x0134 - size of heap reserve
	ULONG dwPeHeapCommit;                  // 0x0138 - size of heap commit
	ULONG dwPeBaseAddr;                    // 0x013C - original base address
	ULONG dwPeSizeofImage;                 // 0x0140 - size of original image
	ULONG dwPeChecksum;                    // 0x0144 - original checksum
	ULONG dwPeTimeDate;                    // 0x0148 - original timedate stamp
	ULONG dwDebugPathnameAddr;             // 0x014C - debug pathname address
	ULONG dwDebugFilenameAddr;             // 0x0150 - debug filename address
	ULONG dwDebugUnicodeFilenameAddr;      // 0x0154 - debug unicode filename address
	ULONG dwKernelImageThunkAddr;          // 0x0158 - kernel image thunk address
	ULONG dwNonKernelImportDirAddr;        // 0x015C - non kernel import directory address
	ULONG dwLibraryVersions;               // 0x0160 - number of library versions
	ULONG dwLibraryVersionsAddr;           // 0x0164 - library versions address
	ULONG dwKernelLibraryVersionAddr;      // 0x0168 - kernel library version address
	ULONG dwXAPILibraryVersionAddr;        // 0x016C - xapi library version address
	ULONG dwLogoBitmapAddr;                // 0x0170 - logo bitmap address
	ULONG dwSizeofLogoBitmap;              // 0x0174 - logo bitmap size
};
using PXBE_HEADER = XBE_HEADER *;

struct XBE_CERTIFICATE {
	uint32_t  dwSize;                               // 0x0000 - size of certificate
	uint32_t  dwTimeDate;                           // 0x0004 - timedate stamp
	uint32_t  dwTitleId;                            // 0x0008 - title id
	wchar_t wsTitleName[40];                        // 0x000C - title name (unicode)
	uint32_t  dwAlternateTitleId[0x10];             // 0x005C - alternate title ids
	uint32_t  dwAllowedMedia;                       // 0x009C - allowed media types
	uint32_t  dwGameRegion;                         // 0x00A0 - game region
	uint32_t  dwGameRatings;                        // 0x00A4 - game ratings
	uint32_t  dwDiscNumber;                         // 0x00A8 - disc number
	uint32_t  dwVersion;                            // 0x00AC - version
	uint8_t  bzLanKey[16];                          // 0x00B0 - lan key
	uint8_t  bzSignatureKey[16];                    // 0x00C0 - signature key
	// NOT ALL XBEs have these fields!
	uint8_t  bzTitleAlternateSignatureKey[16][16];  // 0x00D0 - alternate signature keys
	uint32_t  dwOriginalCertificateSize;            // 0x01D0 - Original Certificate Size?
	uint32_t  dwOnlineService;                      // 0x01D4 - Online Service ID
	uint32_t  dwSecurityFlags;                      // 0x01D8 - Extra Security Flags
	uint8_t  bzCodeEncKey[16];                      // 0x01DC - Code Encryption Key?
};

struct XBE_SECTION {
	ULONG Flags;
	PVOID VirtualAddress;
	ULONG VirtualSize;
	ULONG FileAddress;
	ULONG FileSize;
	PCSZ SectionName;
	ULONG SectionReferenceCount;
	PUSHORT HeadReferenceCount;
	PUSHORT TailReferenceCount;
	BYTE ShaHash[20];
};
using PXBE_SECTION = XBE_SECTION *;
#pragma pack()
