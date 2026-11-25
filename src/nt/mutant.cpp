/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2023
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
