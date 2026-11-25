/*
* SPDX-License-Identifier: GPL-2.0+
*
* ergo720                Copyright (c) 2022
*
* This program is free software; you can redistribute it
* and/or modify it under the terms of the GNU General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#pragma once


// First bug code used in KeBugCheck(Ex)
#define INIT_FAILURE                      0
#define PDCLIB_ERROR                      1
#define IRQL_NOT_GREATER_OR_EQUAL         2
#define IRQL_NOT_LESS_OR_EQUAL            3
#define KERNEL_UNHANDLED_EXCEPTION        4
#define INVALID_CONTEXT                   5
#define UNHANDLED_NESTED_EXCEPTION        6
#define UNHANDLED_UNWIND_EXCEPTION        7
#define NORETURN_FUNCTION_RETURNED        8
#define XBE_LAUNCH_FAILED                 9
#define UNREACHABLE_CODE_REACHED          10
#define BAD_POOL_HEADER                   25
#define KERNEL_APC_PENDING_DURING_EXIT    32
#define MULTIPLE_IRP_COMPLETE_REQUESTS    68
#define BAD_POOL_CALLER                   194

// Optional bug codes used in following arguments in KeBugCheckEx
#define MM_FAILURE 0
#define OB_FAILURE 1
#define IO_FAILURE 2
#define PS_FAILURE 3
#define HAL_FAILURE 4

// BAD_POOL_CALLER codes during pool bugcheck
#define POOL_ENTRY_CORRUPTED 1
#define POOL_ENTRY_ALREADY_FREE 6
#define POOL_ENTRY_NOT_ALLOCATED 7
#define POOL_ALLOC_IRQL_INVALID 8
#define POOL_FREE_IRQL_INVALID 9
#define POOL_BILLED_PROCESS_INVALID 13
#define POOL_HEADER_SIZE_INVALID 32
