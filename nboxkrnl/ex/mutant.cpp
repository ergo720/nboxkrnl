/*
 * ergo720                Copyright (c) 2023
 */


#include "ex.hpp"


EXPORTNUM(22) OBJECT_TYPE ExMutantObjectType = {
	ExAllocatePoolWithTag,
	ExFreePool,
	nullptr,
	ExpDeleteMutant,
	nullptr,
	(PVOID)offsetof(KMUTANT, Header),
	'atuM'
};

VOID XBOXAPI ExpDeleteMutant(PVOID Object)
{
	KeReleaseMutant((PKMUTANT)Object, PRIORITY_BOOST_MUTANT, TRUE, FALSE);
}
