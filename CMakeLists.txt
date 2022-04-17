cmake_minimum_required(VERSION 3.4.3)
project(cxbxrkrnl)

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
 set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${cxbxrkrnl_BINARY_DIR}/bin")
endif()

set(CMAKE_CXX_STANDARD_LIBRARIES "")
STRING (REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

set(CXBXRKRNL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)

add_compile_options(
 /GS-
 /GR-
 /kernel
)

file (GLOB SOURCES
 "${CXBXRKRNL_ROOT_DIR}/cxbxrkrnl/Main.cpp"
)

add_executable(cxbxrkrnl ${SOURCES})

set_target_properties(cxbxrkrnl PROPERTIES
 LINK_FLAGS "
 /BASE:0x10000 \
 /DYNAMICBASE:NO \
 /FIXED \
 /LARGEADDRESSAWARE \
 /INCREMENTAL:NO \
 /OPT:REF \
 /OPT:ICF \
 /SUBSYSTEM:NATIVE \
 /ENTRY:KernelEntry \
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
