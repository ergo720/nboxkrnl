/*
 * ergo720                Copyright (c) 2023
 */

#pragma once

#include "..\types.hpp"
#include "ki.hpp"
#include "seh.hpp"


BOOLEAN XBOXAPI RtlDispatchException(PEXCEPTION_RECORD ExceptionRecord, PCONTEXT ContextRecord);
