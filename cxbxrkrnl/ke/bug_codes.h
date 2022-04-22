/*
 * ergo720                Copyright (c) 2022
 */

#pragma once


// Bug codes format: 1st byte -> kernel system, 2nd byte -> kernel sub-system

#define SYSTEM_KI                0x00
#define SYSTEM_KE                0x01
#define SYSTEM_PDCLIB            0xFF

#define SUBSYSTEM_INIT           0x00
#define SUBSYSTEM_PDCLIB         0xFF

#define BUG_KI_INIT (SYSTEM_KI | (SUBSYSTEM_INIT << 8))
