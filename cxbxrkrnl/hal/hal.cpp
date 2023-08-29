/*
 * ergo720                Copyright (c) 2022
 */

#include "halp.hpp"


VOID HalpShutdownSystem()
{
	OutputToHost(0, KE_ABORT);
	while (true) {
		__asm {
			cli;
			hlt;
		}
	}
}
