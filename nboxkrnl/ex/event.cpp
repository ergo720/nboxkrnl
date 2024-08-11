/*
 * ergo720                Copyright (c) 2024
 */

#include "ex.hpp"


EXPORTNUM(16) OBJECT_TYPE ExEventObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	nullptr,
	nullptr,
	(PVOID)offsetof(KEVENT, Header),
	'vevE'
};
