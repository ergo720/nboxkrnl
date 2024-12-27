/*
 * ergo720                Copyright (c) 2022
 * LukeUsher              Copyright (c) 2017
 */

#include "ke.hpp"
#include "xe.hpp"
#include "ex.hpp"
#include "nt.hpp"
#include "mi.hpp"
#include "obp.hpp"
#include <string.h>

#define XBE_BASE_ADDRESS 0x10000
#define GetXbeAddress() ((XBE_HEADER *)XBE_BASE_ADDRESS)

// XOR keys for Chihiro/Devkit/Retail XBEs
#define XOR_EP_RETAIL 0xA8FC57AB
#define XOR_EP_CHIHIRO 0x40B5C16E
#define XOR_EP_DEBUG 0x94859D4B
#define XOR_KT_RETAIL 0x5B6D40B6
#define XOR_KT_CHIHIRO 0x2290059D
#define XOR_KT_DEBUG 0xEFB1F152

// Required to make sure that XBE_CERTIFICATE has the correct size
static_assert(sizeof(wchar_t) == 2);


EXPORTNUM(164) PLAUNCH_DATA_PAGE LaunchDataPage = nullptr;

EXPORTNUM(326) OBJECT_STRING XeImageFileName = { 0, 0, nullptr };

static INITIALIZE_GLOBAL_CRITICAL_SECTION(XepXbeLoaderLock);


// Source: partially from Cxbx-Reloaded
static NTSTATUS XeLoadXbe()
{
	if (LaunchDataPage) {
		// TODO: XBE reboot support, by reading the path of the new XBE from the LaunchDataPage
		RIP_API_MSG("XBE reboot not supported");
	}
	else {
		// NOTE: we cannot just assume that the XBE name from the DVD drive is called "default.xbe", because the user might have renamed it
		ULONG PathSize;
		ASM_BEGIN
			ASM(mov edx, XE_XBE_PATH_LENGTH);
			ASM(in eax, dx);
			ASM(mov PathSize, eax);
		ASM_END

		PCHAR PathBuffer = (PCHAR)ExAllocatePoolWithTag(PathSize, 'PebX');
		if (!PathBuffer) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		ASM_BEGIN
			ASM(mov edx, XE_XBE_PATH_ADDR);
			ASM(mov eax, PathBuffer);
			ASM(out dx, eax);
		ASM_END

		XeImageFileName.Buffer = PathBuffer;
		XeImageFileName.Length = (USHORT)PathSize;
		XeImageFileName.MaximumLength = (USHORT)PathSize;
		memcpy(XeImageFileName.Buffer, PathBuffer, PathSize); // NOTE: doesn't copy the terminating NULL character

		OBJECT_ATTRIBUTES ObjectAttributes;
		InitializeObjectAttributes(&ObjectAttributes, &XeImageFileName, OBJ_CASE_INSENSITIVE, nullptr);
		IO_STATUS_BLOCK IoStatusBlock;
		HANDLE XbeHandle;
		if (NTSTATUS Status = NtOpenFile(&XbeHandle, GENERIC_READ, &ObjectAttributes, &IoStatusBlock, FILE_SHARE_READ,
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE); !NT_SUCCESS(Status)) {
			return Status;
		}

		PXBE_HEADER XbeHeader = (PXBE_HEADER)ExAllocatePoolWithTag(PAGE_SIZE, 'hIeX');
		if (!XbeHeader) {
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		LARGE_INTEGER XbeOffset{ .QuadPart = 0 };
		if (NTSTATUS Status = NtReadFile(XbeHandle, nullptr, nullptr, nullptr, &IoStatusBlock,
			XbeHeader, PAGE_SIZE, &XbeOffset); !NT_SUCCESS(Status)) {
			ExFreePool(XbeHeader);
			NtClose(XbeHandle);
			return Status;
		}

		// Sanity checks: make sure that the file looks like an XBE
		if ((IoStatusBlock.Information < sizeof(XBE_HEADER)) ||
			(XbeHeader->dwMagic != *(PULONG)"XBEH") ||
			(XbeHeader->dwSizeofHeaders > XbeHeader->dwSizeofImage) ||
			(XbeHeader->dwBaseAddr != XBE_BASE_ADDRESS)) {
			ExFreePool(XbeHeader);
			NtClose(XbeHandle);
			return STATUS_INVALID_IMAGE_FORMAT;
		}

		PVOID Address = GetXbeAddress();
		ULONG Size = XbeHeader->dwSizeofImage;
		NTSTATUS Status = NtAllocateVirtualMemory(&Address, 0, &Size, MEM_RESERVE, PAGE_READWRITE);
		if (!NT_SUCCESS(Status)) {
			ExFreePool(XbeHeader);
			NtClose(XbeHandle);
			return Status;
		}

		Address = GetXbeAddress();
		Size = XbeHeader->dwSizeofHeaders;
		Status = NtAllocateVirtualMemory(&Address, 0, &Size, MEM_COMMIT, PAGE_READWRITE);
		if (!NT_SUCCESS(Status)) {
			Address = GetXbeAddress();
			Size = 0;
			NtFreeVirtualMemory(&Address, &Size, MEM_RELEASE);
			ExFreePool(XbeHeader);
			NtClose(XbeHandle);
			return Status;
		}

		memcpy(GetXbeAddress(), XbeHeader, PAGE_SIZE);
		ExFreePool(XbeHeader);
		XbeHeader = nullptr;

		if (GetXbeAddress()->dwSizeofHeaders > PAGE_SIZE) {
			LARGE_INTEGER XbeOffset{ .QuadPart = PAGE_SIZE };
			if (NTSTATUS Status = NtReadFile(XbeHandle, nullptr, nullptr, nullptr, &IoStatusBlock,
				(PCHAR)XbeHeader + PAGE_SIZE, GetXbeAddress()->dwSizeofHeaders - PAGE_SIZE, &XbeOffset); !NT_SUCCESS(Status)) {
				Address = GetXbeAddress();
				Size = 0;
				NtFreeVirtualMemory(&Address, &Size, MEM_RELEASE);
				NtClose(XbeHandle);
				return Status;
			}
		}

		// Unscramble XBE entry point and kernel thunk addresses
		static constexpr ULONG SEGABOOT_EP_XOR = 0x40000000;
		if ((GetXbeAddress()->dwEntryAddr & WRITE_COMBINED_BASE) == SEGABOOT_EP_XOR) {
			GetXbeAddress()->dwEntryAddr ^= XOR_EP_CHIHIRO;
			GetXbeAddress()->dwKernelImageThunkAddr ^= XOR_KT_CHIHIRO;
		}
		else if ((GetXbeAddress()->dwKernelImageThunkAddr & PHYSICAL_MAP_BASE) > 0) {
			GetXbeAddress()->dwEntryAddr ^= XOR_EP_DEBUG;
			GetXbeAddress()->dwKernelImageThunkAddr ^= XOR_KT_DEBUG;
		}
		else {
			GetXbeAddress()->dwEntryAddr ^= XOR_EP_RETAIL;
			GetXbeAddress()->dwKernelImageThunkAddr ^= XOR_KT_RETAIL;
		}

		// Disable security checks. TODO: is this necessary?
		XBE_CERTIFICATE *XbeCertificate = (XBE_CERTIFICATE *)(GetXbeAddress()->dwCertificateAddr);
		XbeCertificate->dwAllowedMedia |= (
			XBEIMAGE_MEDIA_TYPE_HARD_DISK |
			XBEIMAGE_MEDIA_TYPE_DVD_X2 |
			XBEIMAGE_MEDIA_TYPE_DVD_CD |
			XBEIMAGE_MEDIA_TYPE_CD |
			XBEIMAGE_MEDIA_TYPE_DVD_5_RO |
			XBEIMAGE_MEDIA_TYPE_DVD_9_RO |
			XBEIMAGE_MEDIA_TYPE_DVD_5_RW |
			XBEIMAGE_MEDIA_TYPE_DVD_9_RW);
		if (XbeCertificate->dwSize >= offsetof(XBE_CERTIFICATE, bzCodeEncKey)) {
			XbeCertificate->dwSecurityFlags &= ~1;
		}

		// Load all sections marked as "preload"
		PXBE_SECTION SectionHeaders = (PXBE_SECTION)GetXbeAddress()->dwSectionHeadersAddr;
		for (unsigned i = 0; i < GetXbeAddress()->dwSections; ++i) {
			if (SectionHeaders[i].Flags & XBEIMAGE_SECTION_PRELOAD) {
				Status = XeLoadSection(&SectionHeaders[i]);
				if (!NT_SUCCESS(Status)) {
					Address = GetXbeAddress();
					Size = 0;
					NtFreeVirtualMemory(&Address, &Size, MEM_RELEASE);
					NtClose(XbeHandle);
					return Status;
				}
			}
		}

		// Map the kernel thunk table to the XBE kernel imports
		PULONG XbeKrnlThunk = (PULONG)GetXbeAddress()->dwKernelImageThunkAddr;
		unsigned i = 0;
		while (XbeKrnlThunk[i]) {
			ULONG t = XbeKrnlThunk[i] & 0x7FFFFFFF;
			XbeKrnlThunk[i] = KernelThunkTable[t];
			++i;
		}

		if ((XboxType == SYSTEM_DEVKIT) && !(GetXbeAddress()->dwInitFlags.bLimit64MB)) {
			MiAllowNonDebuggerOnTop64MiB = TRUE;
		}

		if (XboxType == SYSTEM_CHIHIRO) {
			GetXbeAddress()->dwInitFlags.bDontSetupHarddisk = 1;
		}

		// TODO: extract the keys from the certificate and store them in XboxLANKey, XboxSignatureKey and XboxAlternateSignatureKeys

		NtClose(XbeHandle);

		return STATUS_SUCCESS;
	}
}

VOID XBOXAPI XbeStartupThread(PVOID Opaque)
{
	NTSTATUS Status = XeLoadXbe();
	if (!NT_SUCCESS(Status)) {
		// TODO: this should probably either reboot to the dashboard with an error setting, or display the fatal error message to the screen
		KeBugCheck(XBE_LAUNCH_FAILED);
	}

	// Call the entry point of the XBE!
	using PXBE_ENTRY_POINT = VOID(__cdecl *)();
	PXBE_ENTRY_POINT XbeEntryPoint = (PXBE_ENTRY_POINT)(GetXbeAddress()->dwEntryAddr);
	XbeEntryPoint();
}

// Source: Cxbx-Reloaded
EXPORTNUM(327) NTSTATUS XBOXAPI XeLoadSection
(
	PXBE_SECTION Section
)
{
	RtlEnterCriticalSectionAndRegion(&XepXbeLoaderLock);

	// If the reference count was zero, load the section
	if (Section->SectionReferenceCount == 0) {
		// REMARK: Some titles have sections less than PAGE_SIZE, which will cause an overlap with the next section
		// since both will have the same aligned starting address.
		// Test case: Dead or Alive 3, section XGRPH has a size of 764 bytes
		// XGRPH										DSOUND
		// 1F18A0 + 2FC -> aligned_start = 1F1000		1F1BA0 -> aligned_start = 1F1000 <- collision

		OBJECT_ATTRIBUTES ObjectAttributes;
		InitializeObjectAttributes(&ObjectAttributes, &XeImageFileName, OBJ_CASE_INSENSITIVE, nullptr);
		IO_STATUS_BLOCK IoStatusBlock;
		HANDLE XbeHandle;
		if (NTSTATUS Status = NtOpenFile(&XbeHandle, GENERIC_READ, &ObjectAttributes, &IoStatusBlock, FILE_SHARE_READ,
			FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE); !NT_SUCCESS(Status)) {
			RtlLeaveCriticalSectionAndRegion(&XepXbeLoaderLock);
			return Status;
		}

		PVOID BaseAddress = Section->VirtualAddress;
		ULONG SectionSize = Section->VirtualSize;
		NTSTATUS Status = NtAllocateVirtualMemory(&BaseAddress, 0, &SectionSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		if (!NT_SUCCESS(Status)) {
			NtClose(XbeHandle);
			RtlLeaveCriticalSectionAndRegion(&XepXbeLoaderLock);
			return Status;
		}

		// Clear the memory the section requires
		memset(Section->VirtualAddress, 0, Section->VirtualSize);

		// Copy the section data
		LARGE_INTEGER XbeOffset{ .QuadPart = Section->FileAddress };
		if (NTSTATUS Status = NtReadFile(XbeHandle, nullptr, nullptr, nullptr, &IoStatusBlock,
			Section->VirtualAddress, Section->FileSize, &XbeOffset); !NT_SUCCESS(Status) || (IoStatusBlock.Information != Section->FileSize)) {
			if (IoStatusBlock.Information != Section->FileSize) {
				Status = STATUS_FILE_CORRUPT_ERROR;
			}
			BaseAddress = Section->VirtualAddress;
			SectionSize = Section->VirtualSize;
			NtFreeVirtualMemory(&BaseAddress, &SectionSize, MEM_DECOMMIT);
			NtClose(XbeHandle);
			RtlLeaveCriticalSectionAndRegion(&XepXbeLoaderLock);
			return Status;
		}

		NtClose(XbeHandle);

		// Increment the head/tail page reference counters
		(*Section->HeadReferenceCount)++;
		(*Section->TailReferenceCount)++;
	}

	// Increment the reference count
	Section->SectionReferenceCount++;

	RtlLeaveCriticalSectionAndRegion(&XepXbeLoaderLock);

	return STATUS_SUCCESS;
}

// Source: Cxbx-Reloaded
EXPORTNUM(328) NTSTATUS XBOXAPI XeUnloadSection
(
	PXBE_SECTION Section
)
{
	NTSTATUS Status = STATUS_INVALID_PARAMETER;
	RtlEnterCriticalSectionAndRegion(&XepXbeLoaderLock);

	// If the section was loaded, process it
	if (Section->SectionReferenceCount > 0) {
		// Decrement the reference count
		Section->SectionReferenceCount -= 1;

		// Free the section and the physical memory in use if necessary
		if (Section->SectionReferenceCount == 0) {
			// REMARK: the following can be tested with Broken Sword - The Sleeping Dragon, RalliSport Challenge, ...
			// Test-case: Apex/Racing Evoluzione requires the memory NOT to be zeroed

			ULONG BaseAddress = (ULONG)Section->VirtualAddress;
			ULONG EndingAddress = (ULONG)Section->VirtualAddress + Section->VirtualSize;

			// Decrement the head/tail page reference counters
			(*Section->HeadReferenceCount)--;
			(*Section->TailReferenceCount)--;

			if ((*Section->TailReferenceCount) != 0) {
				EndingAddress = ROUND_DOWN_4K(EndingAddress);
			}

			if ((*Section->HeadReferenceCount) != 0) {
				BaseAddress = ROUND_UP_4K(BaseAddress);
			}

			if (EndingAddress > BaseAddress) {
				PVOID BaseAddress2 = (PVOID)BaseAddress;
				ULONG RegionSize = EndingAddress - BaseAddress;
				NtFreeVirtualMemory(&BaseAddress2, &RegionSize, MEM_DECOMMIT);
			}
		}

		Status = STATUS_SUCCESS;
	}

	RtlLeaveCriticalSectionAndRegion(&XepXbeLoaderLock);

	return Status;
}
