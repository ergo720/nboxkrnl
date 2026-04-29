/*
 * ergo720                Copyright (c) 2018
 */

#include "xc.hpp"


 // The following are the default implementations of the crypto functions

VOID XBOXAPI JumpedSHAInit
(
	PUCHAR pbSHAContext
)
{
	A_SHAInit((PSHA_CTX)pbSHAContext);
}

VOID XBOXAPI JumpedSHAUpdate
(
	PUCHAR pbSHAContext,
	PUCHAR pbInput,
	ULONG dwInputLength
)
{
	A_SHAUpdate((PSHA_CTX)pbSHAContext, pbInput, dwInputLength);
}

VOID XBOXAPI JumpedSHAFinal
(
	PUCHAR pbSHAContext,
	PUCHAR pbDigest
)
{
	A_SHAFinal((PSHA_CTX)pbSHAContext, (PULONG)pbDigest);
}


 /* This struct contains the original crypto functions exposed by the kernel */
const CRYPTO_VECTOR DefaultCryptoStruct = {
	JumpedSHAInit,
	JumpedSHAUpdate,
	JumpedSHAFinal,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr
};

/* This struct contains the updated crypto functions which can be changed by the title with XcUpdateCrypto */
CRYPTO_VECTOR UpdatedCryptoStruct = DefaultCryptoStruct;

EXPORTNUM(335) VOID XBOXAPI XcSHAInit
(
	PUCHAR pbSHAContext
)
{
	UpdatedCryptoStruct.pXcSHAInit(pbSHAContext);
}

EXPORTNUM(336) VOID XBOXAPI XcSHAUpdate
(
	PUCHAR pbSHAContext,
	PUCHAR pbInput,
	ULONG dwInputLength
)
{
	UpdatedCryptoStruct.pXcSHAUpdate(pbSHAContext, pbInput, dwInputLength);
}

EXPORTNUM(337) VOID XBOXAPI XcSHAFinal
(
	PUCHAR pbSHAContext,
	PUCHAR pbDigest
)
{
	UpdatedCryptoStruct.pXcSHAFinal(pbSHAContext, pbDigest);
}

// Source: Cxbx-Reloaded
EXPORTNUM(351) VOID XBOXAPI XcUpdateCrypto
(
	PCRYPTO_VECTOR pNewVector,
	PCRYPTO_VECTOR pROMVector
)
{
	// This function changes the default crypto function implementations with those supplied by the title (if not NULL)

	if (pNewVector->pXcSHAInit)
	{
		UpdatedCryptoStruct.pXcSHAInit = pNewVector->pXcSHAInit;
	}
	if (pNewVector->pXcSHAUpdate)
	{
		UpdatedCryptoStruct.pXcSHAUpdate = pNewVector->pXcSHAUpdate;
	}
	if (pNewVector->pXcSHAFinal)
	{
		UpdatedCryptoStruct.pXcSHAFinal = pNewVector->pXcSHAFinal;
	}
	if (pNewVector->pXcRC4Key)
	{
		UpdatedCryptoStruct.pXcRC4Key = pNewVector->pXcRC4Key;
	}
	if (pNewVector->pXcRC4Crypt)
	{
		UpdatedCryptoStruct.pXcRC4Crypt = pNewVector->pXcRC4Crypt;
	}
	if (pNewVector->pXcHMAC)
	{
		UpdatedCryptoStruct.pXcHMAC = pNewVector->pXcHMAC;
	}
	if (pNewVector->pXcPKEncPublic)
	{
		UpdatedCryptoStruct.pXcPKEncPublic = pNewVector->pXcPKEncPublic;
	}
	if (pNewVector->pXcPKDecPrivate)
	{
		UpdatedCryptoStruct.pXcPKDecPrivate = pNewVector->pXcPKDecPrivate;
	}
	if (pNewVector->pXcPKGetKeyLen)
	{
		UpdatedCryptoStruct.pXcPKGetKeyLen = pNewVector->pXcPKGetKeyLen;
	}
	if (pNewVector->pXcVerifyPKCS1Signature)
	{
		UpdatedCryptoStruct.pXcVerifyPKCS1Signature = pNewVector->pXcVerifyPKCS1Signature;
	}
	if (pNewVector->pXcModExp)
	{
		UpdatedCryptoStruct.pXcModExp = pNewVector->pXcModExp;
	}
	if (pNewVector->pXcDESKeyParity)
	{
		UpdatedCryptoStruct.pXcDESKeyParity = pNewVector->pXcDESKeyParity;
	}
	if (pNewVector->pXcKeyTable)
	{
		UpdatedCryptoStruct.pXcKeyTable = pNewVector->pXcKeyTable;
	}
	if (pNewVector->pXcBlockCrypt)
	{
		UpdatedCryptoStruct.pXcBlockCrypt = pNewVector->pXcBlockCrypt;
	}
	if (pNewVector->pXcBlockCryptCBC)
	{
		UpdatedCryptoStruct.pXcBlockCryptCBC = pNewVector->pXcBlockCryptCBC;
	}
	if (pNewVector->pXcCryptService)
	{
		UpdatedCryptoStruct.pXcCryptService = pNewVector->pXcCryptService;
	}

	// Return to the title the original implementations if it supplied an out buffer

	if (pROMVector)
	{
		*pROMVector = DefaultCryptoStruct;
	}
}
