cmake_minimum_required(VERSION 3.4.3)
project(nboxkrnl)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${nboxkrnl_BINARY_DIR}/bin")
 set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endif()

set(CMAKE_CXX_STANDARD_LIBRARIES "")
foreach(flag_var
    CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  string(REGEX REPLACE "/RTC(su|[1su])" "" ${flag_var} "${${flag_var}}")
endforeach(flag_var)

set(NBOXKRNL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory("${NBOXKRNL_ROOT_DIR}/import/pdclib")

add_compile_definitions(
 _PDCLIB_STATIC_DEFINE
 __STDC_NO_THREADS__
 _HAS_EXCEPTIONS=0
)

add_compile_options(
 /GS-
 /GR-
 /sdl-
 /X
 /Zc:preprocessor
 /wd4731
 /wd4733
 /wd26812
 /arch:SSE
)

include_directories(
 "${NBOXKRNL_ROOT_DIR}/import/pdclib/include"
 "${NBOXKRNL_ROOT_DIR}/import/pdclib/platform/xbox/include"
)

link_directories(
 "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}"
)

file (GLOB HEADERS
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/kernel.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ntstatus.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/types.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/winerror.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/dbg/dbg.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ex/ex.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/hal/hal.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/hal/halp.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/io/io.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/io/iop.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/bug_codes.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/ke.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/hw_exp.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/ki.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/seh.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/mi.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/mm.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/vad_tree.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/nt.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/zw.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ob/ob.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ob/obp.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ps/ps.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ps/psp.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/exp_sup.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/rtl.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/xe/xbe.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/xe/xe.hpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/io/hdd/hdd.hpp"
)

file (GLOB SOURCES
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/main.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/dbg/debug.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ex/ex.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ex/exp_sup.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ex/pool.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/hal/interrupt.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/hal/hal.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/hal/halp.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/io/io.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/hw_exp.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/init.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/ki.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ki/seh.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/apc.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/bug.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/device_queue.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/dpc.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/event.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/irql.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/kernel.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/semaphore.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/thread.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/thunk.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/timer.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ke/wait.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/mi.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/mm.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/mm/vad_tree.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/file.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/memory.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/objects.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/nt/services.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ob/ob.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ob/obp.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ps/ps.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/ps/psp.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/error.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/exp_sup.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/rtl.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/rtl/strings.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/xe/xe.cpp"
 "${NBOXKRNL_ROOT_DIR}/nboxkrnl/io/hdd/hdd.cpp"
)

source_group(TREE ${NBOXKRNL_ROOT_DIR} PREFIX header FILES ${HEADERS})
source_group(TREE ${NBOXKRNL_ROOT_DIR} PREFIX source FILES ${SOURCES})

add_executable(nboxkrnl ${HEADERS} ${SOURCES})

add_dependencies(nboxkrnl pdclib)

target_link_libraries(nboxkrnl PUBLIC
 pdclib.lib
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
 "
)
