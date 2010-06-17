/********************************************************************
*
* Description:	implementation for StormLib - Macintosh port
*	
*	these are function wraps to execute Windows API calls
*	as native Macintosh file calls (open/close/read/write/...)
*	requires Mac OS X
*
* Derived from Marko Friedemann <marko.friedemann@bmx-chemnitz.de>
* StormPort.cpp for Linux
*
* Author: Daniel Chiaramello <daniel@chiaramello.net>
*
* Carbonized by: Sam Wilkins <swilkins1337@gmail.com>
*
********************************************************************/

#if (!defined(_WIN32) && !defined(_WIN64))
#include "StormPort.h"
#include "StormLib.h"

/********************************************************************
*	 SwapLong
********************************************************************/

uint32_t SwapULong(uint32_t data)
{
	return CFSwapInt32(data);
}

int32_t SwapLong(uint32_t data)
{
	return (int32_t)CFSwapInt32(data);
}

/********************************************************************
*	 SwapShort
********************************************************************/
uint16_t SwapUShort(uint16_t data)
{
	return CFSwapInt16(data);
}

int16_t SwapShort(uint16_t data)
{
	return (int16_t)CFSwapInt16(data);
}

/********************************************************************
*	 ConvertUnsignedLongBuffer
********************************************************************/
void ConvertUnsignedLongBuffer(void * ptr, size_t length)
{
    uint32_t * buffer = (uint32_t *)ptr;
    uint32_t nbLongs = (uint32_t)(length / sizeof(uint32_t));

	while (nbLongs-- > 0)
	{
		*buffer = SwapLong(*buffer);
		buffer++;
	}
}

/********************************************************************
*	 ConvertUnsignedShortBuffer
********************************************************************/
void ConvertUnsignedShortBuffer(void * ptr, size_t length)
{
    uint16_t * buffer = (uint16_t *)ptr;
    uint32_t nbShorts = (uint32_t)(length / sizeof(uint16_t));

    while (nbShorts-- > 0)
	{
		*buffer = SwapShort(*buffer);
		buffer++;
	}
}

/********************************************************************
*	 ConvertTMPQUserData
********************************************************************/
void ConvertTMPQUserData(void *userData)
{
	TMPQUserData * theData = (TMPQUserData *)userData;

	theData->dwID = SwapULong(theData->dwID);
	theData->cbUserDataSize = SwapULong(theData->cbUserDataSize);
	theData->dwHeaderOffs = SwapULong(theData->dwHeaderOffs);
	theData->cbUserDataHeader = SwapULong(theData->cbUserDataHeader);
}

/********************************************************************
*	 ConvertTMPQHeader
********************************************************************/
void ConvertTMPQHeader(void *header)
{
	TMPQHeader2 * theHeader = (TMPQHeader2 *)header;
	
	theHeader->dwID = SwapULong(theHeader->dwID);
	theHeader->dwHeaderSize = SwapULong(theHeader->dwHeaderSize);
	theHeader->dwArchiveSize = SwapULong(theHeader->dwArchiveSize);
	theHeader->wFormatVersion = SwapUShort(theHeader->wFormatVersion);
	theHeader->wSectorSize = SwapUShort(theHeader->wSectorSize);
	theHeader->dwHashTablePos = SwapULong(theHeader->dwHashTablePos);
	theHeader->dwBlockTablePos = SwapULong(theHeader->dwBlockTablePos);
	theHeader->dwHashTableSize = SwapULong(theHeader->dwHashTableSize);
	theHeader->dwBlockTableSize = SwapULong(theHeader->dwBlockTableSize);

	if(theHeader->wFormatVersion >= MPQ_FORMAT_VERSION_2)
	{
		DWORD dwTemp = theHeader->ExtBlockTablePos.LowPart;
		theHeader->ExtBlockTablePos.LowPart = theHeader->ExtBlockTablePos.HighPart;
		theHeader->ExtBlockTablePos.HighPart = dwTemp;
		theHeader->ExtBlockTablePos.LowPart = SwapULong(theHeader->ExtBlockTablePos.LowPart);
		theHeader->ExtBlockTablePos.HighPart = SwapULong(theHeader->ExtBlockTablePos.HighPart);
		theHeader->wHashTablePosHigh = SwapUShort(theHeader->wHashTablePosHigh);
		theHeader->wBlockTablePosHigh = SwapUShort(theHeader->wBlockTablePosHigh);
	}
}

#pragma mark -

#endif
