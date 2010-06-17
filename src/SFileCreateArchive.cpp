/*****************************************************************************/
/* SFileCreateArchive.cpp                 Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* MPQ Editing functions                                                     */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 24.03.03  1.00  Lad  Splitted from SFileOpenArchive.cpp                   */
/* 08.06.10  1.00  Lad  Renamed to SFileCreateArchive.cpp                    */
/*****************************************************************************/

#define __STORMLIB_SELF__
#include "StormLib.h"
#include "SCommon.h"

//-----------------------------------------------------------------------------
// Defines

#define DEFAULT_SECTOR_SIZE  3       // Default size of a file sector

//-----------------------------------------------------------------------------
// Creates a new MPQ archive.

bool WINAPI SFileCreateArchive(const char * szMpqName, DWORD dwFlags, DWORD dwHashTableSize, HANDLE * phMpq)
{
    LARGE_INTEGER MpqPos = {0};             // Position of MPQ header in the file
    TFileStream * pStream = NULL;           // File stream
    TMPQArchive * ha = NULL;                // MPQ archive handle
    USHORT wFormatVersion = MPQ_FORMAT_VERSION_1;
    DWORD dwBlockTableSize = 0;             // Initial block table size
    DWORD dwPowerOfTwo;
    int nError = ERROR_SUCCESS;

    // Check the parameters, if they are valid
    if(szMpqName == NULL || *szMpqName == 0 || phMpq == NULL)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return false;
    }

    // One time initialization of MPQ cryptography
    InitializeMpqCryptography();

    // We verify if the file already exists and if it's a MPQ archive.
    // If yes, we won't allow to overwrite it.
    if(!(dwFlags & MPQ_CREATE_NO_MPQ_CHECK))
    {
        HANDLE hMpq = NULL;

        if(SFileOpenArchive(szMpqName, 0, dwFlags, &hMpq))
        {
            SFileCloseArchive(hMpq);
            SetLastError(ERROR_ALREADY_EXISTS);
            return false;
        }
    }

    //
    // At this point, we have to create the archive.
    // - If the file exists, convert it to MPQ archive.
    // - If the file doesn't exist, create new empty file
    //

    pStream = FileStream_OpenFile(szMpqName, true);
    if(pStream == NULL)
    {
        pStream = FileStream_CreateFile(szMpqName);
        if(pStream == NULL)
            return false;
    }

    // Decide what format to use
    if(dwFlags & MPQ_CREATE_ARCHIVE_V2)
        wFormatVersion = MPQ_FORMAT_VERSION_2;

    // Retrieve the file size and round it up to 0x200 bytes
    FileStream_GetSize(pStream, &MpqPos);
    MpqPos.QuadPart += 0x1FF;
    MpqPos.LowPart &= 0xFFFFFE00;
    if(!FileStream_SetSize(pStream, &MpqPos))
        nError = GetLastError();

    // Create the archive handle
    if(nError == ERROR_SUCCESS)
    {
        if((ha = ALLOCMEM(TMPQArchive, 1)) == NULL)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Fill the MPQ archive handle structure and create the header,
    // hash table and block table
    if(nError == ERROR_SUCCESS)
    {
        // Round the hash table size up to the nearest power of two
        for(dwPowerOfTwo = HASH_TABLE_SIZE_MIN; dwPowerOfTwo < HASH_TABLE_SIZE_MAX; dwPowerOfTwo <<= 1)
        {
            if(dwPowerOfTwo >= dwHashTableSize)
            {
                dwHashTableSize = dwPowerOfTwo;
                break;
            }
        }

        // Don't allow the hash table size go over allowed maximum
        dwHashTableSize = STORMLIB_MIN(dwHashTableSize, HASH_TABLE_SIZE_MAX);

#ifdef _DEBUG    
        // Debug code, used for testing StormLib
//      dwBlockTableSize = dwHashTableSize * 2;
#endif

        memset(ha, 0, sizeof(TMPQArchive));
        ha->pStream         = pStream;
        ha->dwSectorSize    = 0x200 << DEFAULT_SECTOR_SIZE;
        ha->UserDataPos     = MpqPos;
        ha->MpqPos          = MpqPos;
        ha->pHeader         = &ha->Header;
        ha->dwBlockTableMax = STORMLIB_MAX(dwHashTableSize, dwBlockTableSize);
        ha->pHashTable      = ALLOCMEM(TMPQHash, dwHashTableSize);
        ha->pBlockTable     = ALLOCMEM(TMPQBlock, ha->dwBlockTableMax);
        ha->pExtBlockTable  = ALLOCMEM(TMPQBlockEx, ha->dwBlockTableMax);
        ha->pListFile       = NULL;
        ha->dwFlags         = 0;
        pStream = NULL;
        if(!ha->pHashTable || !ha->pBlockTable || !ha->pExtBlockTable)
            nError = ERROR_NOT_ENOUGH_MEMORY;
    }

    // Fill the MPQ header and all buffers
    if(nError == ERROR_SUCCESS)
    {
        TMPQHeader2 * pHeader = ha->pHeader;
        DWORD dwHeaderSize = (wFormatVersion == MPQ_FORMAT_VERSION_2) ? MPQ_HEADER_SIZE_V2 : MPQ_HEADER_SIZE_V1;

        memset(pHeader, 0, sizeof(TMPQHeader2));
        pHeader->dwID             = ID_MPQ;
        pHeader->dwHeaderSize     = dwHeaderSize;
        pHeader->dwArchiveSize    = pHeader->dwHeaderSize + dwHashTableSize * sizeof(TMPQHash);
        pHeader->wFormatVersion   = wFormatVersion;
        pHeader->wSectorSize      = DEFAULT_SECTOR_SIZE; // 0x1000 bytes per sector
        pHeader->dwHashTableSize  = dwHashTableSize;
        pHeader->dwBlockTableSize = dwBlockTableSize;

        // Clear all tables
        memset(ha->pHashTable, 0xFF, sizeof(TMPQHash) * dwHashTableSize);
        memset(ha->pBlockTable, 0, sizeof(TMPQBlock) * ha->dwBlockTableMax);
        memset(ha->pExtBlockTable, 0, sizeof(TMPQBlockEx) * ha->dwBlockTableMax);

        // Remember if we shall check sector CRCs when reading file
        if(dwFlags & MPQ_OPEN_CHECK_SECTOR_CRC)
            ha->dwFlags |= MPQ_FLAG_CHECK_SECTOR_CRC;

        //
        // Note: Don't write the MPQ header at this point. If any operation fails later,
        // the unfinished MPQ would stay on the disk, being 0x20 (or 0x2C) bytes long,
        // containing naked MPQ header.
        //

        ha->dwFlags |= MPQ_FLAG_NO_HEADER;

        //
        // Note: Don't recalculate position of MPQ tables at this point.
        // We merely set a flag that indicates that the MPQ tables
        // have been changed, and SaveMpqTables will do the work when closing the archive.
        //

        ha->dwFlags |= MPQ_FLAG_CHANGED;
    }

    // Create the internal listfile
    if(nError == ERROR_SUCCESS)
        nError = SListFileCreateListFile(ha);

    // Create the file attributes
    if(nError == ERROR_SUCCESS && (dwFlags & MPQ_CREATE_ATTRIBUTES))
        nError = SAttrCreateAttributes(ha, MPQ_ATTRIBUTE_ALL);

    // Cleanup : If an error, delete all buffers and return
    if(nError != ERROR_SUCCESS)
    {
        FileStream_Close(pStream);
        FreeMPQArchive(ha);
        SetLastError(nError);
        ha = NULL;
    }
    
    // Return the values
    *phMpq = (HANDLE)ha;
    return (nError == ERROR_SUCCESS);
}

//-----------------------------------------------------------------------------
// Opens or creates a (new) MPQ archive.
//
//  szMpqName - Name of the archive to be created.
//
//  dwCreationDisposition:
//
//   Value              Doesn't exist      Exists, not a MPQ  Exists, it's a MPQ
//   ----------         -------------      -----------------  ---------------------
//   MPQ_CREATE_NEW     Creates empty MPQ  Converts to MPQ    Fails
//   MPQ_CREATE_ALWAYS  Creates empty MPQ  Converts to MPQ    Overwrites the MPQ
//   MPQ_OPEN_EXISTING  Fails              Fails              Opens the MPQ
//   MPQ_OPEN_ALWAYS    Creates empty MPQ  Converts to MPQ    Opens the MPQ
//
//   The above mentioned values can be combined with the following flags:
//
//   MPQ_CREATE_ARCHIVE_V1 - Creates MPQ archive version 1
//   MPQ_CREATE_ARCHIVE_V2 - Creates MPQ archive version 2
//   MPQ_CREATE_ATTRIBUTES - Will also add (attributes) file with the CRCs
//   
// dwHashTableSize - Size of the hash table (only if creating a new archive).
//        Must be between 2^4 (= 16) and 2^18 (= 262 144)
//
// phMpq - Receives handle to the archive
//

bool WINAPI SFileCreateArchiveEx(const char * szMpqName, DWORD dwFlags, DWORD dwHashTableSize, HANDLE * phMpq)
{
    TMPQArchive * ha;
    HANDLE hMpq;

    switch(dwFlags & 0x0F)
    {
        case MPQ_CREATE_NEW:

            // Create empty MPQ or convert a file to MPQ. Fail if the file already is a MPQ.
            return SFileCreateArchive(szMpqName, dwFlags, dwHashTableSize, phMpq);

        case MPQ_CREATE_ALWAYS:

            // If the file already exists and it is a MPQ,
            // we crop the MPQ part from the file and create new MPQ
            if(SFileOpenArchive(szMpqName, 0, 0, &hMpq))
            {
                ha = (TMPQArchive *)hMpq;
                FileStream_SetSize(ha->pStream, &ha->MpqPos);
                SFileCloseArchive(hMpq);
            }

            // Now create new one
            return SFileCreateArchive(szMpqName, dwFlags | MPQ_CREATE_NO_MPQ_CHECK, dwHashTableSize, phMpq);

        case MPQ_OPEN_EXISTING:

            // Open existing MPQ. Fail if it doesn't exist or if it's not a MPQ.
            return SFileOpenArchive(szMpqName, 0, dwFlags, phMpq);

        case MPQ_OPEN_ALWAYS:

            // If the file exists and it's an MPQ, open it.
            // Otherwise, create empty MPQ or create MPQ over a file
            if(SFileOpenArchive(szMpqName, 0, dwFlags, phMpq))
                return true;

            // Create new MPQ
            return SFileCreateArchive(szMpqName, dwFlags, dwHashTableSize, phMpq);
    }

    SetLastError(ERROR_INVALID_PARAMETER);
    return false;
}
