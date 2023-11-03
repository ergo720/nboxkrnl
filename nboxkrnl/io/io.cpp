/*
 * ergo720                Copyright (c) 2022
 */

#include "io.hpp"
#include "..\ex\ex.hpp"
#include "..\ob\ob.hpp"


BOOLEAN IoInitSystem()
{
	IoInfoBlock InfoBlock;
	IoRequest Packet;
	Packet.Id = InterlockedIncrement(&IoRequestId);
	Packet.Type = IoRequestType::Read;
	Packet.HandleOrPath = EEPROM_HANDLE;
	Packet.Offset = 0;
	Packet.Size = sizeof(XBOX_EEPROM);
	Packet.HandleOrAddress = (ULONG_PTR)&CachedEeprom;
	SubmitIoRequestToHost(&Packet);
	RetrieveIoRequestFromHost(&InfoBlock, Packet.Id);

	if (InfoBlock.Status != Success) {
		return FALSE;
	}

	XboxFactoryGameRegion = CachedEeprom.EncryptedSettings.GameRegion;

	return TRUE;
}
