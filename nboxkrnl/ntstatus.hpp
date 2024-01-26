/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L)
#define STATUS_ABANDONED                        ((NTSTATUS)0x00000080L)
#define STATUS_USER_APC                         ((NTSTATUS)0x000000C0L)
#define STATUS_KERNEL_APC                       ((NTSTATUS)0x00000100L)
#define STATUS_TIMEOUT                          ((NTSTATUS)0x00000102L)
#define STATUS_PENDING                          ((NTSTATUS)0x00000103L)
#define STATUS_OBJECT_NAME_EXISTS               ((NTSTATUS)0x40000000L)
#define STATUS_GUARD_PAGE_VIOLATION             ((NTSTATUS)0x80000001L)
#define STATUS_BREAKPOINT                       ((NTSTATUS)0x80000003L)
#define STATUS_SINGLE_STEP                      ((NTSTATUS)0x80000004L)
#define STATUS_BUFFER_OVERFLOW                  ((NTSTATUS)0x80000005L)
#define STATUS_NOT_IMPLEMENTED                  ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_INFO_CLASS               ((NTSTATUS)0xC0000003L)
#define STATUS_INFO_LENGTH_MISMATCH             ((NTSTATUS)0xC0000004L)
#define STATUS_ACCESS_VIOLATION                 ((NTSTATUS)0xC0000005L)
#define STATUS_INVALID_HANDLE                   ((NTSTATUS)0xC0000008L)
#define STATUS_INVALID_PARAMETER                ((NTSTATUS)0xC000000DL)
#define STATUS_NO_SUCH_DEVICE                   ((NTSTATUS)0xC000000EL)
#define STATUS_INVALID_DEVICE_REQUEST           ((NTSTATUS)0xC0000010L)
#define STATUS_END_OF_FILE                      ((NTSTATUS)0xC0000011L)
#define STATUS_MORE_PROCESSING_REQUIRED         ((NTSTATUS)0xC0000016L)
#define STATUS_NO_MEMORY                        ((NTSTATUS)0xC0000017L)
#define STATUS_CONFLICTING_ADDRESSES            ((NTSTATUS)0xC0000018L)
#define STATUS_ACCESS_DENIED                    ((NTSTATUS)0xC0000022L)
#define STATUS_BUFFER_TOO_SMALL                 ((NTSTATUS)0xC0000023L)
#define STATUS_OBJECT_TYPE_MISMATCH             ((NTSTATUS)0xC0000024L)
#define STATUS_NONCONTINUABLE_EXCEPTION         ((NTSTATUS)0xC0000025L)
#define STATUS_INVALID_DISPOSITION              ((NTSTATUS)0xC0000026L)
#define STATUS_UNWIND                           ((NTSTATUS)0xC0000027L)
#define STATUS_BAD_STACK                        ((NTSTATUS)0xC0000028L)
#define STATUS_INVALID_UNWIND_TARGET            ((NTSTATUS)0xC0000029L)
#define STATUS_PARITY_ERROR                     ((NTSTATUS)0xC000002BL)
#define STATUS_OBJECT_NAME_INVALID              ((NTSTATUS)0xC0000033L)
#define STATUS_OBJECT_NAME_NOT_FOUND            ((NTSTATUS)0xC0000034L)
#define STATUS_OBJECT_NAME_COLLISION            ((NTSTATUS)0xC0000035L)
#define STATUS_OBJECT_PATH_NOT_FOUND            ((NTSTATUS)0xC000003AL)
#define STATUS_SHARING_VIOLATION                ((NTSTATUS)0xC0000043L)
#define STATUS_INVALID_PAGE_PROTECTION          ((NTSTATUS)0xC0000045L)
#define STATUS_MUTANT_NOT_OWNED                 ((NTSTATUS)0xC0000046L)
#define STATUS_SEMAPHORE_LIMIT_EXCEEDED         ((NTSTATUS)0xC0000047L)
#define STATUS_DELETE_PENDING                   ((NTSTATUS)0xC0000056L)
#define STATUS_INVALID_IMAGE_FORMAT             ((NTSTATUS)0xC000007BL)
#define STATUS_DISK_FULL                        ((NTSTATUS)0xC000007FL)
#define STATUS_ARRAY_BOUNDS_EXCEEDED            ((NTSTATUS)0xC000008CL)
#define STATUS_FLOAT_DENORMAL_OPERAND           ((NTSTATUS)0xC000008DL)
#define STATUS_FLOAT_DIVIDE_BY_ZERO             ((NTSTATUS)0xC000008EL)
#define STATUS_FLOAT_INEXACT_RESULT             ((NTSTATUS)0xC000008FL)
#define STATUS_FLOAT_INVALID_OPERATION          ((NTSTATUS)0xC0000090L)
#define STATUS_FLOAT_OVERFLOW                   ((NTSTATUS)0xC0000091L)
#define STATUS_FLOAT_STACK_CHECK                ((NTSTATUS)0xC0000092L)
#define STATUS_FLOAT_UNDERFLOW                  ((NTSTATUS)0xC0000093L)
#define STATUS_INTEGER_DIVIDE_BY_ZERO           ((NTSTATUS)0xC0000094L)
#define STATUS_PRIVILEGED_INSTRUCTION           ((NTSTATUS)0xC0000096L)
#define STATUS_INSUFFICIENT_RESOURCES           ((NTSTATUS)0xC000009AL)
#define STATUS_FILE_IS_A_DIRECTORY              ((NTSTATUS)0xC00000BAL)
#define STATUS_NOT_A_DIRECTORY                  ((NTSTATUS)0xC0000103L)
#define STATUS_CANNOT_DELETE                    ((NTSTATUS)0xC0000121L)
#define STATUS_FILE_CLOSED                      ((NTSTATUS)0xC0000128L)
#define STATUS_UNRECOGNIZED_VOLUME              ((NTSTATUS)0xC000014FL)
#define STATUS_IO_DEVICE_ERROR                  ((NTSTATUS)0xC0000185L)
#define STATUS_MUTANT_LIMIT_EXCEEDED            ((NTSTATUS)0xC0000191L)
#define STATUS_VOLUME_DISMOUNTED                ((NTSTATUS)0xC000026EL)

#define NT_SUCCESS(Status)                      ((NTSTATUS)(Status) >= 0)
#define NT_WARNING(Status)                      ((ULONG)(Status) >> 30 == 2)
#define NT_ERROR(Status)                        ((ULONG)(Status) >> 30 == 3)
