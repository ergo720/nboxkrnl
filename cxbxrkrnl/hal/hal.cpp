/*
 * ergo720                Copyright (c) 2022
 */

#include "halp.h"


VOID HalpShutdownSystem()
{
	while (true) {
		__asm {
			cli
			hlt
		}
	}
}
