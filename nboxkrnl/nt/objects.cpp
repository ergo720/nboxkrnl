/*
 * ergo720                Copyright (c) 2023
 */

#include "nt.hpp"
#include "..\rtl\rtl.hpp"


EXPORTNUM(187) NTSTATUS XBOXAPI NtClose
(
	HANDLE Handle
)
{
	RIP_UNIMPLEMENTED();

	return STATUS_SUCCESS;
}
