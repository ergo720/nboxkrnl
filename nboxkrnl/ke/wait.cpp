/*
 * ergo720                Copyright (c) 2023
 */

#include "..\ke\ke.hpp"
#include "..\rtl\rtl.hpp"


EXPORTNUM(159) DLLEXPORT NTSTATUS XBOXAPI KeWaitForSingleObject
(
	PVOID Object,
	KWAIT_REASON WaitReason,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout
)
{
	RIP_UNIMPLEMENTED();
}
