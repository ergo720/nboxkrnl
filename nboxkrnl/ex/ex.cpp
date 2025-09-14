/*
 * ergo720                Copyright (c) 2022
 * PatrickvL              Copyright (c) 2017
 */

#include "ex.hpp"
#include "ke.hpp"
#include "ob.hpp"
#include "rtl.hpp"
#include <string.h>

#define XC_END_MARKER (XC_VALUE_INDEX)-1

#define EEPROM_INFO_ENTRY(XC, Member, REG_Type) \
	{ XC, offsetof(XBOX_EEPROM, Member), REG_Type, sizeof(((XBOX_EEPROM *)0)->Member) }


struct EepromInfo {
	XC_VALUE_INDEX Index;
	INT ValueOffset;
	INT ValueType;
	INT ValueLength;
};

// Source: Cxbx-Reloaded
static const EepromInfo EepromInfos[] = {
	EEPROM_INFO_ENTRY(XC_TIMEZONE_BIAS,         UserSettings.TimeZoneBias,                REG_DWORD),
	EEPROM_INFO_ENTRY(XC_TZ_STD_NAME,           UserSettings.TimeZoneStdName,             REG_BINARY),
	EEPROM_INFO_ENTRY(XC_TZ_STD_DATE,           UserSettings.TimeZoneStdDate,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_TZ_STD_BIAS,           UserSettings.TimeZoneStdBias,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_TZ_DLT_NAME,           UserSettings.TimeZoneDltName,             REG_BINARY),
	EEPROM_INFO_ENTRY(XC_TZ_DLT_DATE,           UserSettings.TimeZoneDltDate,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_TZ_DLT_BIAS,           UserSettings.TimeZoneDltBias,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_LANGUAGE,              UserSettings.Language,                    REG_DWORD),
	EEPROM_INFO_ENTRY(XC_VIDEO,                 UserSettings.VideoFlags,                  REG_DWORD),
	EEPROM_INFO_ENTRY(XC_AUDIO,                 UserSettings.AudioFlags,                  REG_DWORD),
	EEPROM_INFO_ENTRY(XC_P_CONTROL_GAMES,       UserSettings.ParentalControlGames,        REG_DWORD),
	EEPROM_INFO_ENTRY(XC_P_CONTROL_PASSWORD,    UserSettings.ParentalControlPassword,     REG_DWORD),
	EEPROM_INFO_ENTRY(XC_P_CONTROL_MOVIES,      UserSettings.ParentalControlMovies,       REG_DWORD),
	EEPROM_INFO_ENTRY(XC_ONLINE_IP_ADDRESS,     UserSettings.OnlineIpAddress,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_ONLINE_DNS_ADDRESS,    UserSettings.OnlineDnsAddress,            REG_DWORD),
	EEPROM_INFO_ENTRY(XC_ONLINE_DEFAULT_GATEWAY_ADDRESS, UserSettings.OnlineDefaultGatewayAddress, REG_DWORD),
	EEPROM_INFO_ENTRY(XC_ONLINE_SUBNET_ADDRESS, UserSettings.OnlineSubnetMask,            REG_DWORD),
	EEPROM_INFO_ENTRY(XC_MISC,                  UserSettings.MiscFlags,                   REG_DWORD),
	EEPROM_INFO_ENTRY(XC_DVD_REGION,            UserSettings.DvdRegion,                   REG_DWORD),
	// XC_MAX_OS is called to return a complete XBOX_USER_SETTINGS structure
	EEPROM_INFO_ENTRY(XC_MAX_OS,                UserSettings,                             REG_BINARY),
	EEPROM_INFO_ENTRY(XC_FACTORY_SERIAL_NUMBER, FactorySettings.SerialNumber,             REG_BINARY),
	EEPROM_INFO_ENTRY(XC_FACTORY_ETHERNET_ADDR, FactorySettings.EthernetAddr,             REG_BINARY),
	EEPROM_INFO_ENTRY(XC_FACTORY_ONLINE_KEY,    FactorySettings.OnlineKey,                REG_BINARY),
	EEPROM_INFO_ENTRY(XC_FACTORY_AV_REGION,     FactorySettings.AVRegion,                 REG_DWORD),
	// Note : XC_FACTORY_GAME_REGION is linked to a separate ULONG XboxFactoryGameRegion (of type REG_DWORD)
	EEPROM_INFO_ENTRY(XC_FACTORY_GAME_REGION,   EncryptedSettings.GameRegion,             REG_DWORD),
	EEPROM_INFO_ENTRY(XC_ENCRYPTED_SECTION,     EncryptedSettings,                        REG_BINARY),
	{ XC_MAX_ALL, 0, REG_BINARY, sizeof(XBOX_EEPROM) },
	{ XC_END_MARKER }
};

static INITIALIZE_GLOBAL_CRITICAL_SECTION(ExpEepromLock);

// Source: Cxbx-Reloaded
static const EepromInfo *ExpFindEepromInfo(XC_VALUE_INDEX Index)
{
	for (int i = 0; EepromInfos[i].Index != XC_END_MARKER; ++i) {
		if (EepromInfos[i].Index == Index) {
			return &EepromInfos[i];
		}
	}

	return nullptr;
}

// Source: Cxbx-Reloaded
EXPORTNUM(24) NTSTATUS XBOXAPI ExQueryNonVolatileSetting
(
	DWORD ValueIndex,
	DWORD *Type,
	PVOID Value,
	SIZE_T ValueLength,
	PSIZE_T ResultLength
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PVOID ValueAddr = nullptr;
	INT ValueType;
	SIZE_T ActualLength;

	if (ValueIndex == XC_FACTORY_GAME_REGION) {
		ValueAddr = &XboxFactoryGameRegion;
		ValueType = REG_DWORD;
		ActualLength = sizeof(ULONG);
	}
	else {
		const EepromInfo *Info = ExpFindEepromInfo((XC_VALUE_INDEX)ValueIndex);
		if (Info) {
			ValueAddr = (PVOID)((PUCHAR)&CachedEeprom + Info->ValueOffset);
			ValueType = Info->ValueType;
			ActualLength = Info->ValueLength;
		}
	}

	if (ValueAddr) {
		if (ResultLength) {
			*ResultLength = ActualLength;
		}

		if (ValueLength >= ActualLength) {
			RtlEnterCriticalSectionAndRegion(&ExpEepromLock);

			*Type = ValueType;
			memset(Value, 0, ValueLength);
			memcpy(Value, ValueAddr, ActualLength);

			RtlLeaveCriticalSectionAndRegion(&ExpEepromLock);
		}
		else {
			Status = STATUS_BUFFER_TOO_SMALL;
		}
	}
	else {
		Status = STATUS_OBJECT_NAME_NOT_FOUND;
	}

	return Status;
}

EXPORTNUM(51) LONG FASTCALL InterlockedCompareExchange
(
	volatile PLONG Destination,
	LONG  Exchange,
	LONG  Comparand
)
{
	// clang-format off
	__asm {
		mov eax, Comparand
		cmpxchg [ecx], edx
	}
	// clang-format on
}

EXPORTNUM(52) LONG FASTCALL InterlockedDecrement
(
	volatile PLONG Addend
)
{
	// clang-format off
	__asm {
		or eax, 0xFFFFFFFF
		xadd [ecx], eax
		dec eax
	}
	// clang-format on
}

EXPORTNUM(53) LONG FASTCALL InterlockedIncrement
(
	volatile PLONG Addend
)
{
	// clang-format off
	__asm {
		mov eax, 1
		xadd [ecx], eax
		inc eax
	}
	// clang-format on
}

EXPORTNUM(54) LONG FASTCALL InterlockedExchange
(
	volatile PLONG Destination,
	LONG Value
)
{
	// clang-format off
	__asm {
		mov eax, Value
		xchg [ecx], eax
	}
	// clang-format on
}
