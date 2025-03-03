/*
* ergo720                Copyright (c) 2025
*/

#include "nt.hpp"


EXPORTNUM(196) NTSTATUS XBOXAPI NtDeviceIoControlFile
(
	HANDLE FileHandle,
	HANDLE Event,
	PIO_APC_ROUTINE ApcRoutine,
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG IoControlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength
)
{
	return IopControlFile(FileHandle, Event, ApcRoutine, ApcContext, IoStatusBlock, IoControlCode, InputBuffer, InputBufferLength, OutputBuffer,
		OutputBufferLength, IRP_MJ_DEVICE_CONTROL);
}
