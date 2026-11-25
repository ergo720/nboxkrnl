cmake_minimum_required(VERSION 4.2.3)
project(nboxkrnl)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${nboxkrnl_BINARY_DIR}/bin")
 set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

set(CMAKE_CXX_STANDARD_LIBRARIES "")
set(CMAKE_MSVC_RUNTIME_CHECKS "")

set(NBOXKRNL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

find_package(Git REQUIRED)
find_package(Python REQUIRED COMPONENTS Interpreter)
add_custom_target(gen_git_hash ALL
 COMMAND ${Python_EXECUTABLE} ${NBOXKRNL_ROOT_DIR}/scripts/gen_git_hash.py
)

add_compile_definitions(
 "$<$<CONFIG:Debug>:ENABLE_POOL_TAG_TRACKING>"
 _PDCLIB_STATIC_DEFINE
 __STDC_NO_THREADS__
 _HAS_EXCEPTIONS=0
 NANOPRINTF_IMPLEMENTATION
 NANOPRINTF_VISIBILITY_STATIC
 NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS=1
 NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS=1
 NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS=1
 NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS=1
 NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS=0
 NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS=0
)

add_compile_options(
 /GS-
 /GR-
 /sdl-
 /X
 /Zc:preprocessor
 /wd4731
 /wd4733
 /wd4804
 /wd26812
 /arch:SSE
)

include_directories(
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/platform/xbox/include"
 "${NBOXKRNL_ROOT_DIR}/import/nanoprintf"
 "${NBOXKRNL_ROOT_DIR}/src/av"
 "${NBOXKRNL_ROOT_DIR}/src/dbg"
 "${NBOXKRNL_ROOT_DIR}/src/ex"
 "${NBOXKRNL_ROOT_DIR}/src/hal"
 "${NBOXKRNL_ROOT_DIR}/src/io"
 "${NBOXKRNL_ROOT_DIR}/src/ke"
 "${NBOXKRNL_ROOT_DIR}/src/ki"
 "${NBOXKRNL_ROOT_DIR}/src/mm"
 "${NBOXKRNL_ROOT_DIR}/src/nt"
 "${NBOXKRNL_ROOT_DIR}/src/ob"
 "${NBOXKRNL_ROOT_DIR}/src/ps"
 "${NBOXKRNL_ROOT_DIR}/src/rtl"
 "${NBOXKRNL_ROOT_DIR}/src/xc"
 "${NBOXKRNL_ROOT_DIR}/src/xe"
)

link_directories(
 "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
)

file (GLOB HEADERS
 "${NBOXKRNL_ROOT_DIR}/src/helpers.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/kernel.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/kernel_version.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ntstatus.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/types.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/winerror.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/av/av.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/dbg/dbg.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/ex.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/hal.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/halp.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/hw_io.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/fsc.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/io.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/iop.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/cdrom/cdrom.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/cdrom/xdvdfs.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/hdd/fatx.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/hdd/hdd.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/raw/raw.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/bug_codes.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/ke.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/hw_exp.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/ki.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/seh.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/mi.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/mm.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/vad_tree.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/nt.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/zw.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ob/ob.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ob/obp.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ps/ps.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/ps/psp.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/exp_sup.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/rtl.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/xc/xc.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/xe/xbe.hpp"
 "${NBOXKRNL_ROOT_DIR}/src/xe/xe.hpp"

 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/float.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/limits.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/stdarg.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/stddef.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/stdint.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/string.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/pdclib/_PDCLIB_internal.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/include/pdclib/_PDCLIB_lib_ext1.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/platform/xbox/include/pdclib/_PDCLIB_config.h"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/platform/xbox/include/assert.h"

 "${NBOXKRNL_ROOT_DIR}/import/nanoprintf/nanoprintf.h"
)

file (GLOB SOURCES
 "${NBOXKRNL_ROOT_DIR}/src/main.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/av/av.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/dbg/debug.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/event.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/ex.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/exp_sup.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/mutant.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/pool.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ex/rwlock.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/interrupt.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/hal.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/hal/halp.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/fsc.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/io.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/iop.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/cdrom/cdrom.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/cdrom/xdvdfs.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/hdd/fatx.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/hdd/hdd.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/io/raw/raw.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/div_exp.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/hw_exp.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/init.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/ki.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ki/seh.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/apc.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/bug.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/device_queue.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/dpc.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/event.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/interrupt.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/irql.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/kernel.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/mutant.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/semaphore.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/thread.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/thunk.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/timer.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ke/wait.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/mi.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/mm.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/mm/vad_tree.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/control.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/file.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/memory.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/mutant.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/objects.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/nt/services.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ob/ob.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ob/obp.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ps/ps.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/ps/psp.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/comparison.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/error.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/exp_sup.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/rtl.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/strings.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/string_append.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/string_conversion.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/string_lowercase.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/rtl/string_uppercase.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/xc/crypto.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/xc/sha.cpp"
 "${NBOXKRNL_ROOT_DIR}/src/xe/xe.cpp"

 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/memcmp.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/memcpy.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/memmove.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/memset.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/strcmp.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/strlen.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/strncmp.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/functions/string/strncpy.c"
 "${NBOXKRNL_ROOT_DIR}/import/nanopdclib/platform/xbox/functions/_PDCLIB/assert.c"
)

source_group(TREE ${NBOXKRNL_ROOT_DIR} PREFIX header FILES ${HEADERS})
source_group(TREE ${NBOXKRNL_ROOT_DIR} PREFIX source FILES ${SOURCES})

add_executable(nboxkrnl ${HEADERS} ${SOURCES})
add_dependencies(nboxkrnl gen_git_hash)

target_link_libraries(nboxkrnl PUBLIC
 debug libcmtd.lib
 optimized libcmt.lib
)

set_target_properties(nboxkrnl PROPERTIES
 LINK_FLAGS "
 /BASE:0x80010000 \
 /DYNAMICBASE:NO \
 /FIXED \
 /LARGEADDRESSAWARE \
 /NODEFAULTLIB \
 /DEBUG:FULL \
 /INCREMENTAL:NO \
 /OPT:REF \
 /OPT:ICF \
 /SUBSYSTEM:NATIVE \
 /ENTRY:KernelEntry \
 /IGNORE:4210 \
 /SAFESEH:NO \
 "
)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} \
 /MTd \
 "
)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} \
 /O2 \
 /MT \
 /Zi \
 "
)
