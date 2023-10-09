/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ke\ke.hpp"

#define PSP_MAX_CREATE_THREAD_NOTIFY 8


using PCREATE_THREAD_NOTIFY_ROUTINE = VOID(XBOXAPI *)(
	PETHREAD eThread,
	HANDLE ThreadId,
	BOOLEAN Create
	);

VOID XBOXAPI PspSystemThreadStartup(PKSTART_ROUTINE StartRoutine, PVOID StartContext);
VOID PspCallThreadNotificationRoutines(PETHREAD eThread, BOOLEAN Create);

inline PCREATE_THREAD_NOTIFY_ROUTINE PspNotifyRoutines[PSP_MAX_CREATE_THREAD_NOTIFY] = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};
