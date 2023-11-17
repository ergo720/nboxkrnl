/*
 * ergo720                Copyright (c) 2018
 */

#pragma once

#include "..\types.hpp"


// Source: Cxbx-Reloaded
/* Function pointers which point to all the kernel crypto functions. Used by PCRYPTO_VECTOR. */
using pfXcSHAInit = VOID(XBOXAPI *)(PUCHAR pbSHAContext);
using pfXcSHAUpdate = VOID(XBOXAPI *)(PUCHAR pbSHAContext, PUCHAR pbInput, ULONG dwInputLength);
using pfXcSHAFinal = VOID(XBOXAPI *)(PUCHAR pbSHAContext, PUCHAR pbDigest);
using pfXcRC4Key = VOID(XBOXAPI *)(PUCHAR pbKeyStruct, ULONG dwKeyLength, PUCHAR pbKey);
using pfXcRC4Crypt = VOID(XBOXAPI *)(PUCHAR pbKeyStruct, ULONG dwInputLength, PUCHAR pbInput);
using pfXcHMAC = VOID(XBOXAPI *)(PBYTE pbKeyMaterial, ULONG cbKeyMaterial, PBYTE pbData, ULONG cbData, PBYTE pbData2, ULONG cbData2, PBYTE HmacData);
using pfXcPKEncPublic = ULONG(XBOXAPI *)(PUCHAR pbPubKey, PUCHAR pbInput, PUCHAR pbOutput);
using pfXcPKDecPrivate = ULONG(XBOXAPI *)(PUCHAR pbPrvKey, PUCHAR pbInput, PUCHAR pbOutput);
using pfXcPKGetKeyLen = ULONG(XBOXAPI *)(PUCHAR pbPubKey);
using pfXcVerifyPKCS1Signature = BOOLEAN(XBOXAPI *)(PUCHAR pbSig, PUCHAR pbPubKey, PUCHAR pbDigest);
using pfXcModExp = ULONG(XBOXAPI *)(LPDWORD pA, LPDWORD pB, LPDWORD pC, LPDWORD pD, ULONG dwN);
using pfXcDESKeyParity = VOID(XBOXAPI *)(PUCHAR pbKey, ULONG dwKeyLength);
using pfXcKeyTable = VOID(XBOXAPI *)(ULONG dwCipher, PUCHAR pbKeyTable, PUCHAR pbKey);
using pfXcBlockCrypt = VOID(XBOXAPI *)(ULONG dwCipher, PUCHAR pbOutput, PUCHAR pbInput, PUCHAR pbKeyTable, ULONG dwOp);
using pfXcBlockCryptCBC = VOID(XBOXAPI *)(ULONG dwCipher, ULONG dwInputLength, PUCHAR pbOutput, PUCHAR pbInput, PUCHAR pbKeyTable, ULONG dwOp, PUCHAR pbFeedback);
using pfXcCryptService = ULONG(XBOXAPI *)(ULONG dwOp, PVOID pArgs);

struct CRYPTO_VECTOR {
	pfXcSHAInit pXcSHAInit;
	pfXcSHAUpdate pXcSHAUpdate;
	pfXcSHAFinal pXcSHAFinal;
	pfXcRC4Key pXcRC4Key;
	pfXcRC4Crypt pXcRC4Crypt;
	pfXcHMAC pXcHMAC;
	pfXcPKEncPublic pXcPKEncPublic;
	pfXcPKDecPrivate pXcPKDecPrivate;
	pfXcPKGetKeyLen pXcPKGetKeyLen;
	pfXcVerifyPKCS1Signature pXcVerifyPKCS1Signature;
	pfXcModExp pXcModExp;
	pfXcDESKeyParity pXcDESKeyParity;
	pfXcKeyTable pXcKeyTable;
	pfXcBlockCrypt pXcBlockCrypt;
	pfXcBlockCryptCBC pXcBlockCryptCBC;
	pfXcCryptService pXcCryptService;
};
using PCRYPTO_VECTOR = CRYPTO_VECTOR *;

typedef struct {
	ULONG Unknown[6];
	ULONG State[5];
	ULONG Count[2];
	UCHAR Buffer[64];
} SHA_CTX, *PSHA_CTX;


#ifdef __cplusplus
extern "C" {
#endif

EXPORTNUM(335) DLLEXPORT VOID XBOXAPI XcSHAInit
(
	PUCHAR pbSHAContext
);

EXPORTNUM(336) DLLEXPORT VOID XBOXAPI XcSHAUpdate
(
	PUCHAR pbSHAContext,
	PUCHAR pbInput,
	ULONG dwInputLength
);

EXPORTNUM(337) DLLEXPORT VOID XBOXAPI XcSHAFinal
(
	PUCHAR pbSHAContext,
	PUCHAR pbDigest
);

EXPORTNUM(351) DLLEXPORT VOID XBOXAPI XcUpdateCrypto
(
	PCRYPTO_VECTOR pNewVector,
	PCRYPTO_VECTOR pROMVector
);

#ifdef __cplusplus
}
#endif

void XBOXAPI A_SHAInit(PSHA_CTX Context);
void XBOXAPI A_SHAUpdate(PSHA_CTX Context, const unsigned char *Buffer, UINT BufferSize);
void XBOXAPI A_SHAFinal(PSHA_CTX Context, PULONG Result);
