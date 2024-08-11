/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "ex.hpp"


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

EXPORTNUM(221) NTSTATUS XBOXAPI NtReleaseMutant
(
	HANDLE MutantHandle,
	PLONG PreviousCount
)
{
	PVOID MutantObject;
	NTSTATUS Status = ObReferenceObjectByHandle(MutantHandle, &ExMutantObjectType, &MutantObject);

	if (NT_SUCCESS(Status)) {
		// KeReleaseMutant will raise an exception if this thread doesn't own the mutex
		__try {
			LONG Count = KeReleaseMutant((PKMUTANT)MutantObject, PRIORITY_BOOST_MUTANT, FALSE, FALSE);
			if (PreviousCount) {
				*PreviousCount = Count;
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			Status = GetExceptionCode();
		}

		ObfDereferenceObject(MutantObject);
	}

	return Status;
}
