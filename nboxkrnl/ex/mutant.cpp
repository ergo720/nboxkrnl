/*
 * ergo720                Copyright (c) 2023
 */


#include "ex.hpp"


VOID XBOXAPI ExpDeleteMutant(PVOID Object)
{
	KeReleaseMutant((PKMUTANT)Object, PRIORITY_BOOST_MUTANT, TRUE, FALSE);
}
