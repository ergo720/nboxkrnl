/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "..\ex\ex.hpp"


EXPORTNUM(192) DLLEXPORT NTSTATUS XBOXAPI NtCreateMutant
(
	PHANDLE MutantHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	BOOLEAN InitialOwner
)
{
	PVOID MutantObject;
	NTSTATUS Status = ObCreateObject(&ExMutantObjectType, ObjectAttributes, sizeof(KMUTANT), &MutantObject);

	if (NT_SUCCESS(Status)) {
		KeInitializeMutant((PKMUTANT)MutantObject, InitialOwner);
		Status = ObInsertObject(MutantObject, ObjectAttributes, 0, MutantHandle);
	}

	return Status;
}
