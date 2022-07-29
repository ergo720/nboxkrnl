/*
 * ergo720                Copyright (c) 2022
 */

#include "mi.h"


VOID MiFlushEntireTlb()
{
	__asm {
		mov eax, cr3
		mov cr3, eax
	}
}
