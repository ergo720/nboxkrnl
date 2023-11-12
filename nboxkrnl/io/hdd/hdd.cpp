/*
 * ergo720                Copyright (c) 2023
 */

#include "hdd.hpp"
#include "..\..\ob\obp.hpp"
#include "..\..\ex\ex.hpp"
#include "..\..\nt\nt.hpp"
#include "..\..\rtl\rtl.hpp"


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

	return TRUE;
}

NTSTATUS XBOXAPI HddParseDirectory(PVOID ParseObject, POBJECT_TYPE ObjectType, ULONG Attributes, POBJECT_STRING Name, POBJECT_STRING RemainderName,
	PVOID Context, PVOID *Object)
{
	// This is the parse routine that is invoked by OB when it needs to reference path/files on the HDD, in particular, those that start with "\\Device\\Harddisk0"
	
	RIP_UNIMPLEMENTED();
}
