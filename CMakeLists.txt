cmake_minimum_required(VERSION 3.4.3)
project(cxbxrkrnl)

set(CMAKE_SUPPRESS_REGENERATION true)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${cxbxrkrnl_BINARY_DIR}/bin")
 set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

set(CMAKE_CXX_STANDARD_LIBRARIES "")
foreach(flag_var
    CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  string(REGEX REPLACE "/RTC(su|[1su])" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)

set(CXBXRKRNL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory("${CXBXRKRNL_ROOT_DIR}/import/pdclib")

add_compile_definitions(
 _PDCLIB_STATIC_DEFINE
 __STDC_NO_THREADS__
)

add_compile_options(
 /GS-
 /GR-
 /sdl-
 /X
 /Zc:preprocessor
 /kernel
 /wd4731
 /wd26812
 /arch:IA32
)

include_directories(
 "${CXBXRKRNL_ROOT_DIR}/import/pdclib/include"
 "${CXBXRKRNL_ROOT_DIR}/import/pdclib/platform/xbox/include"
)

link_directories(
 "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
)

file (GLOB HEADERS
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/kernel.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/types.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/dbg/dbg.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ex/ex.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/hal/halp.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/bug_codes.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/ke.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ki/ki.h"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/mm/mm.h"
)

file (GLOB SOURCES
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/main.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/dbg/debug.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ex/ex.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/hal/interrupt.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/hal/hal.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ki/exception.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ki/init.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ki/ki.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/apc.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/bug.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/dpc.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/semaphore.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/thread.cpp"
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/ke/timer.cpp"
)

source_group(TREE ${CXBXRKRNL_ROOT_DIR} PREFIX header FILES ${HEADERS})
source_group(TREE ${CXBXRKRNL_ROOT_DIR} PREFIX source FILES ${SOURCES})

add_executable(cxbxrkrnl ${HEADERS} ${SOURCES})

add_dependencies(cxbxrkrnl pdclib)

target_link_libraries(cxbxrkrnl PUBLIC
 pdclib.lib
 debug libcmtd.lib
 optimized libcmt.lib
)

set_target_properties(cxbxrkrnl PROPERTIES
 LINK_FLAGS "
 /BASE:0x80010000 \
 /DYNAMICBASE:NO \
 /FIXED \
 /LARGEADDRESSAWARE \
 /NODEFAULTLIB \
 /INCREMENTAL:NO \
 /OPT:REF \
 /OPT:ICF \
 /LTCG \
 /SUBSYSTEM:NATIVE \
 /ENTRY:KernelEntry \
 /IGNORE:4210 \
 "
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} \
 /MTd \
 "
)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} \
 /O2 \
 /GL \
 /MT \
 "
)
