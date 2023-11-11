/*
 * ergo720                Copyright (c) 2023
 */

#include "io.hpp"
#include "iop.hpp"
#include "..\ex\ex.hpp"
#include "..\rtl\rtl.hpp"


BOOLEAN IoInitSystem()
{
	IoInfoBlock InfoBlock;
	IoRequest Packet;
	Packet.Id = InterlockedIncrement(&IoRequestId);
	Packet.Type = IoRequestType::Read;
	Packet.HandleOrPath = EEPROM_HANDLE;
	Packet.Offset = 0;
	Packet.Size = sizeof(XBOX_EEPROM);
	Packet.HandleOrAddress = (ULONG_PTR)&CachedEeprom;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlock, Packet.Id);

	if (InfoBlock.Status != Success) {
		return FALSE;
	}

	XboxFactoryGameRegion = CachedEeprom.EncryptedSettings.GameRegion;

	return TRUE;
}

EXPORTNUM(66) NTSTATUS XBOXAPI IoCreateFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	PLARGE_INTEGER AllocationSize,
	ULONG FileAttributes,
	ULONG ShareAccess,
	ULONG Disposition,
	ULONG CreateOptions,
	ULONG Options
)
{
	if (Options & IO_CHECK_CREATE_PARAMETERS) {
		/* Validate parameters */

		if (FileAttributes & ~FILE_ATTRIBUTE_VALID_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if (ShareAccess & ~FILE_SHARE_VALID_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if (Disposition > FILE_MAXIMUM_DISPOSITION) {
			return STATUS_INVALID_PARAMETER;
		}

		if (CreateOptions & ~FILE_VALID_OPTION_FLAGS) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) && (!(DesiredAccess & SYNCHRONIZE))) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_DELETE_ON_CLOSE) && (!(DesiredAccess & DELETE))) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & (FILE_SYNCHRONOUS_IO_NONALERT | FILE_SYNCHRONOUS_IO_ALERT)) == (FILE_SYNCHRONOUS_IO_NONALERT | FILE_SYNCHRONOUS_IO_ALERT)){
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_DIRECTORY_FILE) && !(CreateOptions & FILE_NON_DIRECTORY_FILE) && (CreateOptions & ~(FILE_DIRECTORY_FILE |
				FILE_SYNCHRONOUS_IO_ALERT |
				FILE_SYNCHRONOUS_IO_NONALERT |
				FILE_WRITE_THROUGH |
				FILE_COMPLETE_IF_OPLOCKED |
				FILE_OPEN_FOR_BACKUP_INTENT |
				FILE_DELETE_ON_CLOSE |
				FILE_OPEN_FOR_FREE_SPACE_QUERY |
				FILE_OPEN_BY_FILE_ID |
				FILE_OPEN_REPARSE_POINT))
			|| ((Disposition != FILE_CREATE)
				&& (Disposition != FILE_OPEN)
				&& (Disposition != FILE_OPEN_IF))
			) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_COMPLETE_IF_OPLOCKED) && (CreateOptions & FILE_RESERVE_OPFILTER)) {
			return STATUS_INVALID_PARAMETER;
		}

		if ((CreateOptions & FILE_NO_INTERMEDIATE_BUFFERING) && (DesiredAccess & FILE_APPEND_DATA)) {
			return STATUS_INVALID_PARAMETER;
		}
	}

	LARGE_INTEGER CapturedAllocationSize;
	CapturedAllocationSize.QuadPart = AllocationSize ? AllocationSize->QuadPart : 0;

	OPEN_PACKET OpenPacket;
	OpenPacket.Type = IO_TYPE_OPEN_PACKET;
	OpenPacket.Size = sizeof(OPEN_PACKET);
	OpenPacket.ParseCheck = 0;
	OpenPacket.AllocationSize = CapturedAllocationSize;
	OpenPacket.CreateOptions = CreateOptions;
	OpenPacket.FileAttributes = (USHORT)FileAttributes;
	OpenPacket.ShareAccess = (USHORT)ShareAccess;
	OpenPacket.Disposition = Disposition;
	OpenPacket.QueryOnly = FALSE;
	OpenPacket.DeleteOnly = FALSE;
	OpenPacket.Options = Options;
	OpenPacket.RelatedFileObject = nullptr;
	OpenPacket.DesiredAccess = DesiredAccess;
	OpenPacket.FinalStatus = STATUS_SUCCESS;
	OpenPacket.FileObject = nullptr;

	HANDLE Handle;
	NTSTATUS Status = ObOpenObjectByName(ObjectAttributes, nullptr, &OpenPacket, &Handle);

	RIP_API_MSG("incomplete");
}
