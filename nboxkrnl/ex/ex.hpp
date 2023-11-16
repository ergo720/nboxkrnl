/*
 * ergo720                Copyright (c) 2022
 */

#pragma once

#include "..\nt\zw.hpp"
#include "..\ob\ob.hpp"

#define TIME_ZONE_NAME_LENGTH 4

 // No defined value type.
#define REG_NONE 0
// A null - terminated string. This will be either a Unicode or an ANSI string, depending on whether you use the Unicode or ANSI functions.
#define REG_SZ 1
// A null - terminated string that contains unexpanded references to environment variables (for example, "%PATH%").
#define REG_EXPAND_SZ 2
// Binary data in any form.
#define REG_BINARY 3
// A 32 - bit number.
#define REG_DWORD 4
// A 32 - bit number in little - endian format. Windows is designed to run on little - endian computer architectures.
#define REG_DWORD_LITTLE_ENDIAN 4
// A 32 - bit number in big - endian format. Some UNIX systems support big - endian architectures.
#define REG_DWORD_BIG_ENDIAN 5
// A null - terminated Unicode string that contains the target path of a symbolic link that was created by calling the RegCreateKeyEx function with REG_OPTION_CREATE_LINK.
#define REG_LINK 6
// A sequence of null - terminated strings, terminated by an empty string(\0). String1\0String2\0String3\0LastString\0\0.
#define REG_MULTI_SZ 7
// Resource list in the resource map
#define REG_RESOURCE_LIST 8
// Resource list in the hardware description
#define REG_FULL_RESOURCE_DESCRIPTOR 9
#define REG_RESOURCE_REQUIREMENTS_LIST 10


enum XC_VALUE_INDEX {
	XC_TIMEZONE_BIAS = 0,
	XC_TZ_STD_NAME = 1,
	XC_TZ_STD_DATE = 2,
	XC_TZ_STD_BIAS = 3,
	XC_TZ_DLT_NAME = 4,
	XC_TZ_DLT_DATE = 5,
	XC_TZ_DLT_BIAS = 6,
	XC_LANGUAGE = 7,
	XC_VIDEO = 8,
	XC_AUDIO = 9,
	XC_P_CONTROL_GAMES = 0xA,
	XC_P_CONTROL_PASSWORD = 0xB,
	XC_P_CONTROL_MOVIES = 0xC,
	XC_ONLINE_IP_ADDRESS = 0xD,
	XC_ONLINE_DNS_ADDRESS = 0xE,
	XC_ONLINE_DEFAULT_GATEWAY_ADDRESS = 0xF,
	XC_ONLINE_SUBNET_ADDRESS = 0x10,
	XC_MISC = 0x11,
	XC_DVD_REGION = 0x12,
	XC_MAX_OS = 0xFF,
	// Break
	XC_FACTORY_START_INDEX = 0x100,
	XC_FACTORY_SERIAL_NUMBER = XC_FACTORY_START_INDEX,
	XC_FACTORY_ETHERNET_ADDR = 0x101,
	XC_FACTORY_ONLINE_KEY = 0x102,
	XC_FACTORY_AV_REGION = 0x103,
	XC_FACTORY_GAME_REGION = 0x104,
	XC_MAX_FACTORY = 0x1FF,
	// Break
	XC_ENCRYPTED_SECTION = 0xFFFE,
	XC_MAX_ALL = 0xFFFF
};
using PXC_VALUE_INDEX = XC_VALUE_INDEX *;

struct XBOX_ENCRYPTED_SETTINGS {
	UCHAR Checksum[20];
	UCHAR Confounder[8];
	UCHAR HDKey[XBOX_KEY_LENGTH];
	ULONG GameRegion;
};

struct XBOX_FACTORY_SETTINGS {
	ULONG Checksum;
	UCHAR SerialNumber[12];
	UCHAR EthernetAddr[6];
	UCHAR Reserved1[2];
	UCHAR OnlineKey[16];
	ULONG AVRegion;
	ULONG Reserved2;
};

struct XBOX_TIMEZONE_DATE {
	UCHAR Month;
	UCHAR Day;
	UCHAR DayOfWeek;
	UCHAR Hour;
};

struct XBOX_USER_SETTINGS {
	ULONG Checksum;
	LONG TimeZoneBias;
	CHAR TimeZoneStdName[TIME_ZONE_NAME_LENGTH];
	CHAR TimeZoneDltName[TIME_ZONE_NAME_LENGTH];
	ULONG Reserved1[2];
	XBOX_TIMEZONE_DATE TimeZoneStdDate;
	XBOX_TIMEZONE_DATE TimeZoneDltDate;
	ULONG Reserved2[2];
	LONG TimeZoneStdBias;
	LONG TimeZoneDltBias;
	ULONG Language;
	ULONG VideoFlags;
	ULONG AudioFlags;
	ULONG ParentalControlGames;
	ULONG ParentalControlPassword;
	ULONG ParentalControlMovies;
	ULONG OnlineIpAddress;
	ULONG OnlineDnsAddress;
	ULONG OnlineDefaultGatewayAddress;
	ULONG OnlineSubnetMask;
	ULONG MiscFlags;
	ULONG DvdRegion;
};

struct XBOX_EEPROM {
	XBOX_ENCRYPTED_SETTINGS EncryptedSettings;
	XBOX_FACTORY_SETTINGS FactorySettings;
	XBOX_USER_SETTINGS UserSettings;
	UCHAR Unused[58];
	UCHAR UEMInfo[4];
	UCHAR Reserved1[2];
};

struct ERWLOCK {
	LONG LockCount;
	ULONG WritersWaitingCount;
	ULONG ReadersWaitingCount;
	ULONG ReadersEntryCount;
	KEVENT WriterEvent;
	KSEMAPHORE ReaderSemaphore;
};
using PERWLOCK = ERWLOCK *;

static_assert(sizeof(XBOX_EEPROM) == 256);

inline XBOX_EEPROM CachedEeprom;
inline ULONG XboxFactoryGameRegion;


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(14) DLLEXPORT PVOID XBOXAPI ExAllocatePool
(
	SIZE_T NumberOfBytes
);

EXPORTNUM(15) DLLEXPORT PVOID XBOXAPI ExAllocatePoolWithTag
(
	SIZE_T NumberOfBytes,
	ULONG Tag
);

EXPORTNUM(17) DLLEXPORT VOID XBOXAPI ExFreePool
(
	PVOID P
);

EXPORTNUM(22) DLLEXPORT extern OBJECT_TYPE ExMutantObjectType;

EXPORTNUM(23) DLLEXPORT ULONG XBOXAPI ExQueryPoolBlockSize
(
	PVOID PoolBlock
);

EXPORTNUM(24) DLLEXPORT NTSTATUS XBOXAPI ExQueryNonVolatileSetting
(
	DWORD ValueIndex,
	DWORD *Type,
	PVOID Value,
	SIZE_T ValueLength,
	PSIZE_T ResultLength
);

EXPORTNUM(26) DLLEXPORT VOID XBOXAPI ExRaiseException
(
	PEXCEPTION_RECORD ExceptionRecord
);

EXPORTNUM(27) DLLEXPORT VOID XBOXAPI ExRaiseStatus
(
	NTSTATUS Status
);

EXPORTNUM(51) DLLEXPORT LONG FASTCALL InterlockedCompareExchange
(
	volatile PLONG Destination,
	LONG  Exchange,
	LONG  Comparand
);

EXPORTNUM(52) DLLEXPORT LONG FASTCALL InterlockedDecrement
(
	volatile PLONG Addend
);

EXPORTNUM(53) DLLEXPORT LONG FASTCALL InterlockedIncrement
(
	volatile PLONG Addend
);

EXPORTNUM(54) DLLEXPORT LONG FASTCALL InterlockedExchange
(
	volatile PLONG Destination,
	LONG Value
);

#ifdef __cplusplus
}
#endif


VOID XBOXAPI ExpDeleteMutant(PVOID Object);
