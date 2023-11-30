/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "..\..\ob\obp.hpp"
#include "..\..\ex\ex.hpp"
#include "..\..\nt\nt.hpp"
#include "..\..\rtl\rtl.hpp"
#include <assert.h>


/*
Drive Letter  Description  Offset (bytes)  Size (bytes)  Filesystem       Device Object
N/A           Config Area  0x00000000      0x00080000    Fixed Structure  \Device\Harddisk0\Partition0
X             Game Cache   0x00080000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition3
Y             Game Cache   0x2ee80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition4
Z             Game Cache   0x5dc80000      0x2ee00000    FATX 	          \Device\Harddisk0\Partition5
C             System       0x8ca80000      0x1f400000    FATX 	          \Device\Harddisk0\Partition2
E             Data         0xabe80000      0x131f00000   FATX 	          \Device\Harddisk0\Partition1
*/
// Note that this table ignores the non-standard partitions with drive letters F: and G:
static constexpr XBOX_PARTITION_TABLE HddPartitionTable = {
	{ '*', '*', '*', '*', 'P', 'A', 'R', 'T', 'I', 'N', 'F', 'O', '*', '*', '*', '*' },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{
		{ { 'X', 'B', 'O', 'X', ' ', 'S', 'H', 'E', 'L', 'L', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_MUSICPART_LBA_START, XBOX_MUSICPART_LBA_SIZE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'D', 'A', 'T', 'A', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, PE_PARTFLAGS_IN_USE, XBOX_SYSPART_LBA_START, XBOX_SYSPART_LBA_SIZE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '1' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART1_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '2' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART2_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
		{ { 'X', 'B', 'O', 'X', ' ', 'G', 'A', 'M', 'E', ' ', 'S', 'W', 'A', 'P', ' ', '3' }, PE_PARTFLAGS_IN_USE, XBOX_SWAPPART3_LBA_START, XBOX_SWAPPART_LBA_SIZE, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
		{ { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' }, 0, 0, 0, 0 },
	}
};

NTSTATUS XBOXAPI HddParseDirectory(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainderName,
	PVOID Context, PVOID *Object);

OBJECT_TYPE HddDirectoryObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	HddParseDirectory,
	&ObpDefaultObject,
	'0ddH'
};

static DRIVER_OBJECT HddDriverObject = {
	nullptr,                            // DriverStartIo
	nullptr,                            // DriverDeleteDevice
	nullptr,                            // DriverDismountVolume
	{
		IoInvalidDeviceRequest,         // IRP_MJ_CREATE
		IoInvalidDeviceRequest,         // IRP_MJ_CLOSE
		IoInvalidDeviceRequest,         // IRP_MJ_READ
		IoInvalidDeviceRequest,         // IRP_MJ_WRITE
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_SET_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_FLUSH_BUFFERS
		IoInvalidDeviceRequest,         // IRP_MJ_QUERY_VOLUME_INFORMATION
		IoInvalidDeviceRequest,         // IRP_MJ_DIRECTORY_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_FILE_SYSTEM_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_INTERNAL_DEVICE_CONTROL
		IoInvalidDeviceRequest,         // IRP_MJ_SHUTDOWN
		IoInvalidDeviceRequest,         // IRP_MJ_CLEANUP
	}
};

static PDEVICE_OBJECT HddPartitionObjectsArray[XBOX_MAX_NUM_OF_PARTITIONS];
INITIALIZE_GLOBAL_OBJECT_STRING(HddPartitionString, "Partition");


BOOLEAN HddInitDriver()
{
	// Note that, for ObCreateObject to work, an object with a name "\\Device" must have already been created previously. This is done with the object ObpIoDevicesDirectoryObject
	// with a name ObpIoDevicesString, which is created by ObpCreatePermanentObjects, called from ObInitSystem before this routine gains control

	OBJECT_ATTRIBUTES ObjectAttributes;
	INITIALIZE_GLOBAL_OBJECT_STRING(HddDirectoryName, "\\Device\\Harddisk0");
	InitializeObjectAttributes(&ObjectAttributes, &HddDirectoryName, OBJ_PERMANENT | OBJ_CASE_INSENSITIVE, nullptr);

	PVOID DiskDirectoryObject;
	NTSTATUS Status = ObCreateObject(&HddDirectoryObjectType, &ObjectAttributes, 0, &DiskDirectoryObject);

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	HANDLE Handle;
	Status = ObInsertObject(DiskDirectoryObject, &ObjectAttributes, 0, &Handle);

	if (!NT_SUCCESS(Status)) {
		return FALSE;
	}

	NtClose(Handle);

	// Create all device objects required by the HDD, one for each partition and the whole disk
	for (unsigned i = 0; i < XBOX_MAX_NUM_OF_PARTITIONS; ++i) {
		PDEVICE_OBJECT HddDeviceObject;
		Status = IoCreateDevice(&HddDriverObject, sizeof(IDE_DISK_EXTENSION), nullptr, FILE_DEVICE_DISK, FALSE, &HddDeviceObject);
		if (!NT_SUCCESS(Status)) {
			return FALSE;
		}

		PIDE_DISK_EXTENSION HddExtension = (PIDE_DISK_EXTENSION)HddDeviceObject->DeviceExtension;
		if (i == 0) {
			// Whole disk
			HddDeviceObject->Flags |= (DO_DIRECT_IO | DO_SCATTER_GATHER_IO | DO_RAW_MOUNT_ONLY);
			HddExtension->PartitionInformation.StartingOffset.QuadPart = (ULONGLONG)XBOX_CONFIG_AREA_LBA_START * HDD_SECTOR_SIZE;
			HddExtension->PartitionInformation.PartitionLength.QuadPart = (ULONGLONG)HDD_TOTAL_NUM_OF_SECTORS * HDD_SECTOR_SIZE;
		}
		else {
			// Data partition, system partition or cache partitions
			HddDeviceObject->Flags |= (DO_DIRECT_IO | DO_SCATTER_GATHER_IO);
			HddExtension->PartitionInformation.StartingOffset.QuadPart = (ULONGLONG)HddPartitionTable.TableEntries[i].LBAStart * HDD_SECTOR_SIZE;
			HddExtension->PartitionInformation.PartitionLength.QuadPart = (ULONGLONG)HddPartitionTable.TableEntries[i].LBASize * HDD_SECTOR_SIZE;
		}

		HddDeviceObject->AlignmentRequirement = HDD_ALIGNMENT_REQUIREMENT;
		HddDeviceObject->SectorSize = HDD_SECTOR_SIZE;
		HddExtension->DeviceObject = HddDeviceObject;
		HddExtension->PartitionInformation.RecognizedPartition = TRUE;
		HddExtension->PartitionInformation.PartitionNumber = i;
		HddDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

		HddPartitionObjectsArray[i] = HddDeviceObject;
	}

	return TRUE;
}

NTSTATUS XBOXAPI HddParseDirectory(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainderName,
	PVOID Context, PVOID *Object)
{
	// This is the parse routine that is invoked by OB when it needs to reference path/files on the HDD, in particular, those that start with "\\Device\\Harddisk0"
	
	*Object = NULL_HANDLE;

	if (RemainderName->Length == 0) {
		return STATUS_ACCESS_DENIED;
	}

	BOOLEAN HasBackslashAtEnd = FALSE;
	if (RemainderName->Buffer[RemainderName->Length - 1] == OB_PATH_DELIMITER) {
		HasBackslashAtEnd = TRUE; // creating or opening a directory
	}

	// Extract the partition name
	OBJECT_STRING FirstName, LocalRemainderName, OriName = *RemainderName;
	ObpParseName(&OriName, &FirstName, &LocalRemainderName);

	if (LocalRemainderName.Length && (LocalRemainderName.Buffer[0] == OB_PATH_DELIMITER)) {
		// Another delimiter in the name is invalid
		return STATUS_OBJECT_NAME_INVALID;
	}

	// FirstName must be "Partition" since we are here when we access the HDD
	if (FirstName.Length >= HddPartitionString.Length) {
		ULONG OriFirstNameLength = FirstName.Length;
		FirstName.Length = HddPartitionString.Length;
		if (RtlEqualString(&FirstName, &HddPartitionString, TRUE)) {
			// Sanity check: ensure that the partition number is valid

			FirstName.Length = OriFirstNameLength;
			PCHAR PartitionNumberStart = FirstName.Buffer + HddPartitionString.Length;
			PCHAR PartitionNumberEnd = FirstName.Buffer + FirstName.Length;
			ULONG PartitionNumber = 0;

			while (PartitionNumberStart < PartitionNumberEnd) {
				CHAR CurrChar = *PartitionNumberStart;
				if ((CurrChar >= '0') && (CurrChar <= '9')) {
					PartitionNumber = PartitionNumber * 10 + (CurrChar - '0');
					++PartitionNumberStart;
					continue;
				}
				break;
			}

			if ((PartitionNumberStart == PartitionNumberEnd) && (PartitionNumber < XBOX_MAX_NUM_OF_PARTITIONS)) {
				if (LocalRemainderName.Length || HasBackslashAtEnd) {
					// Always pass an absolute path to the fatx driver so that it can open directories
					LocalRemainderName.Buffer -= 1;
					LocalRemainderName.Length += 1;
					LocalRemainderName.MaximumLength = LocalRemainderName.Length;
					assert(*LocalRemainderName.Buffer == OB_PATH_DELIMITER);
				}

				return IoParseDevice(HddPartitionObjectsArray[PartitionNumber], ObjectType, Attributes, Name, &LocalRemainderName, Context, Object);
			}
		}
		FirstName.Length = OriFirstNameLength;
	}

	return LocalRemainderName.Length ? STATUS_OBJECT_PATH_NOT_FOUND : STATUS_OBJECT_NAME_NOT_FOUND;
}
