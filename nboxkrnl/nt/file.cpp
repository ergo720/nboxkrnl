/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"


EXPORTNUM(202) NTSTATUS XBOXAPI NtOpenFile
(
	PHANDLE FileHandle,
	ACCESS_MASK DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG ShareAccess,
	ULONG OpenOptions
)
{
	return IoCreateFile(FileHandle, DesiredAccess, ObjectAttributes, IoStatusBlock, nullptr, 0, ShareAccess, FILE_OPEN, OpenOptions, 0);
}
