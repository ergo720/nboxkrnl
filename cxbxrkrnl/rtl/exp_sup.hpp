/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "..\ki\ki.hpp"
#include "..\ki\seh.hpp"


BOOLEAN RtlDispatchException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
