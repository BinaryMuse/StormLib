/*****************************************************************************/
/* SCommon.cpp                            Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* Common functions for StormLib, used by all SFile*** modules               */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 24.03.03  1.00  Lad  The first version of SFileCommon.cpp                 */
/* 19.11.03  1.01  Dan  Big endian handling                                  */
/* 12.06.04  1.01  Lad  Renamed to SCommon.cpp                               */
/*****************************************************************************/

#define __STORMLIB_SELF__
#define __INCLUDE_CRYPTOGRAPHY__
#include "StormLib.h"
#include "SCommon.h"

char StormLibCopyright[] = "StormLib v " STORMLIB_VERSION_STRING " Copyright Ladislav Zezula 1998-2010";

//-----------------------------------------------------------------------------
// The buffer for decryption engine.

LCID    lcFileLocale = LANG_NEUTRAL;        // File locale
USHORT  wPlatform = 0;                      // File platform

//-----------------------------------------------------------------------------
// Storm buffer functions

#define MPQ_HASH_TABLE_OFFSET   0x000
#define MPQ_HASH_NAME_A         0x100
#define MPQ_HASH_NAME_B         0x200
#define MPQ_HASH_FILE_KEY       0x300

#define STORM_BUFFER_SIZE   0x500

static DWORD StormBuffer[STORM_BUFFER_SIZE];    // Buffer for the decryption engine
static bool  bMpqCryptographyInitialized = false;

static DWORD HashString(const char * szFileName, DWORD dwHashType)
{
    BYTE * pbKey   = (BYTE *)szFileName;
    DWORD  dwSeed1 = 0x7FED7FED;
    DWORD  dwSeed2 = 0xEEEEEEEE;
    DWORD  ch;

    while(*pbKey != 0)
    {
        ch = toupper(*pbKey++);

        dwSeed1 = StormBuffer[dwHashType + ch] ^ (dwSeed1 + dwSeed2);
        dwSeed2 = ch + dwSeed1 + dwSeed2 + (dwSeed2 << 5) + 3;
    }

    return dwSeed1;
}

void InitializeMpqCryptography()
{
    DWORD dwSeed = 0x00100001;
    DWORD index1 = 0;
    DWORD index2 = 0;
    int   i;

    // Initialize the decryption buffer.
    // Do nothing if already done.
    if(bMpqCryptographyInitialized == false)
    {
        for(index1 = 0; index1 < 0x100; index1++)
        {
            for(index2 = index1, i = 0; i < 5; i++, index2 += 0x100)
            {
                DWORD temp1, temp2;

                dwSeed = (dwSeed * 125 + 3) % 0x2AAAAB;
                temp1  = (dwSeed & 0xFFFF) << 0x10;

                dwSeed = (dwSeed * 125 + 3) % 0x2AAAAB;
                temp2  = (dwSeed & 0xFFFF);

                StormBuffer[index2] = (temp1 | temp2);
            }
        }

        // Also register both MD5 and SHA1 hash algorithms
        register_hash(&md5_desc);
        register_hash(&sha1_desc);

        // Use LibTomMath as support math library for LibTomCrypt
        ltc_mp = ltm_desc;    

        // Don't do that again
        bMpqCryptographyInitialized = true;
    }
}

//-----------------------------------------------------------------------------
// Functions decrypts the file key from the file name

DWORD DecryptFileKey(const char * szFileName)
{
    return HashString(szFileName, MPQ_HASH_FILE_KEY);
}

//-----------------------------------------------------------------------------
// Encrypting and decrypting MPQ file data

void EncryptMpqBlock(void * pvFileBlock, DWORD dwLength, DWORD dwSeed1)
{
    DWORD * block = (DWORD *)pvFileBlock;
    DWORD dwSeed2 = 0xEEEEEEEE;
    DWORD ch;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)
    {
        dwSeed2 += StormBuffer[0x400 + (dwSeed1 & 0xFF)];
        ch     = *block;
        *block++ = ch ^ (dwSeed1 + dwSeed2);

        dwSeed1  = ((~dwSeed1 << 0x15) + 0x11111111) | (dwSeed1 >> 0x0B);
        dwSeed2  = ch + dwSeed2 + (dwSeed2 << 5) + 3;
    }
}

void DecryptMpqBlock(void * pvFileBlock, DWORD dwLength, DWORD dwSeed1)
{
    DWORD * block = (DWORD *)pvFileBlock;
    DWORD dwSeed2 = 0xEEEEEEEE;
    DWORD ch;

    // Round to DWORDs
    dwLength >>= 2;

    while(dwLength-- > 0)
    {
        dwSeed2 += StormBuffer[0x400 + (dwSeed1 & 0xFF)];
        ch     = *block ^ (dwSeed1 + dwSeed2);

        dwSeed1  = ((~dwSeed1 << 0x15) + 0x11111111) | (dwSeed1 >> 0x0B);
        dwSeed2  = ch + dwSeed2 + (dwSeed2 << 5) + 3;
        *block++ = ch;
    }
}

void EncryptMpqTable(void * pvMpqTable, DWORD dwLength, const char * szKey)
{
    EncryptMpqBlock(pvMpqTable, dwLength, HashString(szKey, MPQ_HASH_FILE_KEY));
}

void DecryptMpqTable(void * pvMpqTable, DWORD dwLength, const char * szKey)
{
    DecryptMpqBlock(pvMpqTable, dwLength, HashString(szKey, MPQ_HASH_FILE_KEY));
}

//-----------------------------------------------------------------------------
// Functions tries to get file decryption key. The trick comes from sector
// positions which are stored at the begin of each compressed file. We know the
// file size, that means we know number of sectors that means we know the first
// DWORD value in sector position. And if we know encrypted and decrypted value,
// we can find the decryption key !!!
//
// hf            - MPQ file handle
// SectorOffsets - DWORD array of sector positions
// ch            - Decrypted value of the first sector pos

DWORD DetectFileKeyBySectorSize(DWORD * SectorOffsets, DWORD decrypted)
{
    DWORD saveKey1;
    DWORD temp = *SectorOffsets ^ decrypted;    // temp = seed1 + seed2
    temp -= 0xEEEEEEEE;                 // temp = seed1 + StormBuffer[0x400 + (seed1 & 0xFF)]

    for(int i = 0; i < 0x100; i++)      // Try all 255 possibilities
    {
        DWORD seed1;
        DWORD seed2 = 0xEEEEEEEE;
        DWORD ch;

        // Try the first DWORD (We exactly know the value)
        seed1  = temp - StormBuffer[0x400 + i];
        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = SectorOffsets[0] ^ (seed1 + seed2);

        if(ch != decrypted)
            continue;

        // Add 1 because we are decrypting sector positions
        saveKey1 = seed1 + 1;

        // If OK, continue and test the second value. We don't know exactly the value,
        // but we know that the second one has lower 16 bits set to zero
        // (no compressed sector is larger than 0xFFFF bytes)
        seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
        seed2  = ch + seed2 + (seed2 << 5) + 3;

        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = SectorOffsets[1] ^ (seed1 + seed2);

        if((ch & 0xFFFF0000) == 0)
            return saveKey1;
    }
    return 0;
}

// Function tries to detect file encryption key. It expectes at least two uncompressed bytes
DWORD DetectFileKeyByKnownContent(void * pvFileContent, DWORD nDwords, ...)
{
    DWORD * pdwContent = (DWORD *)pvFileContent;
    va_list argList;
    DWORD dwDecrypted[0x10];
    DWORD saveKey1;
    DWORD dwTemp;
    DWORD i, j;
    
    // We need at least two DWORDS to detect the file key
    if(nDwords < 0x02 || nDwords > 0x10)
        return 0;
    
    va_start(argList, nDwords);
    for(i = 0; i < nDwords; i++)
        dwDecrypted[i] = va_arg(argList, DWORD);
    va_end(argList);
    
    dwTemp = (*pdwContent ^ dwDecrypted[0]) - 0xEEEEEEEE;
    for(i = 0; i < 0x100; i++)      // Try all 256 possibilities
    {
        DWORD seed1;
        DWORD seed2 = 0xEEEEEEEE;
        DWORD ch;

        // Try the first DWORD
        seed1  = dwTemp - StormBuffer[0x400 + i];
        seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
        ch     = pdwContent[0] ^ (seed1 + seed2);

        if(ch != dwDecrypted[0])
            continue;

        saveKey1 = seed1;

        // If OK, continue and test all bytes.
        for(j = 1; j < nDwords; j++)
        {
            seed1  = ((~seed1 << 0x15) + 0x11111111) | (seed1 >> 0x0B);
            seed2  = ch + seed2 + (seed2 << 5) + 3;

            seed2 += StormBuffer[0x400 + (seed1 & 0xFF)];
            ch     = pdwContent[j] ^ (seed1 + seed2);

            if(ch == dwDecrypted[j] && j == nDwords - 1)
                return saveKey1;
        }
    }
    return 0;
}

DWORD DetectFileKeyByContent(void * pvFileContent, DWORD dwFileSize)
{
    DWORD dwFileKey;

    // Try to break the file encryption key as if it was a WAVE file
    if(dwFileSize >= 0x0C)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 3, 0x46464952, dwFileSize - 8, 0x45564157);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Try to break the encryption key as if it was an EXE file
    if(dwFileSize > 0x40)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 2, 0x00905A4D, 0x00000003);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Try to break the encryption key as if it was a XML file
    if(dwFileSize > 0x04)
    {
        dwFileKey = DetectFileKeyByKnownContent(pvFileContent, 2, 0x6D783F3C, 0x6576206C);
        if(dwFileKey != 0)
            return dwFileKey;
    }

    // Not detected, sorry
    return 0;
}

//-----------------------------------------------------------------------------
// Handle validation functions

bool IsValidMpqHandle(TMPQArchive * ha)
{
    if(ha == NULL)
        return false;
    if(ha->pHeader == NULL || ha->pHeader->dwID != ID_MPQ)
        return false;
    
    return (bool)(ha->pHeader->dwID == ID_MPQ);
}

bool IsValidFileHandle(TMPQFile * hf)
{
    if(hf == NULL)
        return false;

    if(hf->dwMagic != ID_MPQ_FILE)
        return false;

    if(hf->pStream != NULL)
        return true;

    return IsValidMpqHandle(hf->ha);
}

//-----------------------------------------------------------------------------
// Hash table and block table manipulation

#define GET_NEXT_HASH_ENTRY(pStartHash, pHash, pHashEnd)  \
        if(++pHash >= pHashEnd)                           \
            pHash = ha->pHashTable;                       \
        if(pHash == pStartHash)                           \
            break;

// Retrieves the first hash entry for the given file.
// Every locale version of a file has its own hash entry
TMPQHash * GetFirstHashEntry(TMPQArchive * ha, const char * szFileName)
{
    TMPQHash * pStartHash;                  // File hash entry (start)
    TMPQHash * pHashEnd = ha->pHashTable + ha->pHeader->dwHashTableSize;
    TMPQHash * pHash;                       // File hash entry (current)
    DWORD dwHashTableSizeMask;
    DWORD dwIndex = HashString(szFileName, MPQ_HASH_TABLE_OFFSET);
    DWORD dwName1 = HashString(szFileName, MPQ_HASH_NAME_A);
    DWORD dwName2 = HashString(szFileName, MPQ_HASH_NAME_B);

    // Get the first possible has entry that might be the one
    dwHashTableSizeMask = ha->pHeader->dwHashTableSize ? (ha->pHeader->dwHashTableSize - 1) : 0;
    pStartHash = pHash = ha->pHashTable + (dwIndex & dwHashTableSizeMask);

    // There might be deleted entries in the hash table prior to our desired entry.
    while(pHash->dwBlockIndex != HASH_ENTRY_FREE)
    {
        // If the entry agrees, we found it.
        if(pHash->dwName1 == dwName1 && pHash->dwName2 == dwName2 && pHash->dwBlockIndex < ha->pHeader->dwBlockTableSize)
            return pHash;

        // Move to the next hash entry. Stop searching
        // if we got reached the original hash entry
        GET_NEXT_HASH_ENTRY(pStartHash, pHash, pHashEnd)
    }

    // The apropriate hash entry was not found
    return NULL;
}

TMPQHash * GetNextHashEntry(TMPQArchive * ha, TMPQHash * pFirstHash, TMPQHash * pPrevHash)
{
    TMPQHash * pHashEnd = ha->pHashTable + ha->pHeader->dwHashTableSize;
    TMPQHash * pHash = pPrevHash;
    DWORD dwName1 = pPrevHash->dwName1;
    DWORD dwName2 = pPrevHash->dwName2;

    // Now go for any next entry that follows the pPrevHash,
    // until either free hash entry was found, or the start entry was reached
    for(;;)
    {
        // Move to the next hash entry. Stop searching
        // if we got reached the original hash entry
        GET_NEXT_HASH_ENTRY(pFirstHash, pHash, pHashEnd)

        // If the entry is a free entry, stop search
        if(pHash->dwBlockIndex == HASH_ENTRY_FREE)
            break;

        // If the entry is not free and the name agrees, we found it
        if(pHash->dwName1 == dwName1 && pHash->dwName2 == dwName2 && pHash->dwBlockIndex < ha->pHeader->dwBlockTableSize)
            return pHash;
    }

    // No next entry
    return NULL;
}

// Returns a hash table entry in the following order:
// 1) A hash table entry with the preferred locale
// 2) A hash table entry with the neutral locale
// 3) NULL
TMPQHash * GetHashEntryLocale(TMPQArchive * ha, const char * szFileName, LCID lcLocale)
{
    TMPQHash * pHashNeutral = NULL;
    TMPQHash * pFirstHash = GetFirstHashEntry(ha, szFileName);
    TMPQHash * pHash = pFirstHash;

    // Parse the found hashes
    while(pHash != NULL)
    {
        // If the locales match, return it
        if(pHash->lcLocale == lcLocale)
            return pHash;
        
        // If we found neutral hash, remember it
        if(pHash->lcLocale == 0)
            pHashNeutral = pHash;

        // Get the next hash entry for that file
        pHash = GetNextHashEntry(ha, pFirstHash, pHash); 
    }

    // At the end, return neutral hash (if found), otherwise NULL
    return pHashNeutral;
}

// Returns a hash table entry in the following order:
// 1) A hash table entry with the preferred locale
// 2) NULL
TMPQHash * GetHashEntryExact(TMPQArchive * ha, const char * szFileName, LCID lcLocale)
{
    TMPQHash * pFirstHash = GetFirstHashEntry(ha, szFileName);
    TMPQHash * pHash = pFirstHash;

    // Parse the found hashes
    while(pHash != NULL)
    {
        // If the locales match, return it
        if(pHash->lcLocale == lcLocale)
            return pHash;
        
        // Get the next hash entry for that file
        pHash = GetNextHashEntry(ha, pFirstHash, pHash); 
    }

    // Not found
    return NULL;
}

// Returns a hash table entry in the following order:
// 1) A hash table entry with the neutral locale
// 2) A hash table entry with any other locale
// 3) NULL
TMPQHash * GetHashEntryAny(TMPQArchive * ha, const char * szFileName)
{
    TMPQHash * pHashNeutral = NULL;
    TMPQHash * pFirstHash = GetFirstHashEntry(ha, szFileName);
    TMPQHash * pHashAny = NULL;
    TMPQHash * pHash = pFirstHash;

    // Parse the found hashes
    while(pHash != NULL)
    {
        // If we found neutral hash, remember it
        if(pHash->lcLocale == 0)
            pHashNeutral = pHash;
        if(pHashAny == NULL)
            pHashAny = pHash;

        // Get the next hash entry for that file
        pHash = GetNextHashEntry(ha, pFirstHash, pHash); 
    }

    // At the end, return neutral hash (if found), otherwise NULL
    return (pHashNeutral != NULL) ? pHashNeutral : pHashAny;
}

TMPQHash * GetHashEntryByIndex(TMPQArchive * ha, DWORD dwFileIndex)
{
    TMPQHash * pHashEnd = ha->pHashTable + ha->pHeader->dwHashTableSize;
    TMPQHash * pHash;

    for(pHash = ha->pHashTable; pHash != pHashEnd; pHash++)
    {
        // If the file index matches, return the hash
        if(pHash->dwBlockIndex == dwFileIndex)
            return pHash;
    }

    // Not found, sorry
    return NULL;
}

// Finds the nearest free hash entry for a file
TMPQHash * FindFreeHashEntry(TMPQHash * pHashTable, DWORD dwHashTableSize, const char * szFileName)
{
    TMPQHash * pStartHash;                  // Starting point of the search
    TMPQHash * pHashEnd = pHashTable + dwHashTableSize;
    TMPQHash * pHash;
    DWORD dwHashTableSizeMask;
    DWORD dwIndex = HashString(szFileName, MPQ_HASH_TABLE_OFFSET);
    DWORD dwName1 = HashString(szFileName, MPQ_HASH_NAME_A);
    DWORD dwName2 = HashString(szFileName, MPQ_HASH_NAME_B);

    // Save the starting hash position
    dwHashTableSizeMask = dwHashTableSize ? (dwHashTableSize - 1) : 0;
    pStartHash = pHash = pHashTable + (dwIndex & dwHashTableSizeMask);

    // Look for the first free or deleted hash entry.
    while(pHash->dwBlockIndex < HASH_ENTRY_DELETED)
    {
        if(++pHash >= pHashEnd)
            pHash = pHashTable;
        if(pHash == pStartHash)
            return NULL;
    }

    // Fill the hash entry with the informations about the file name
    pHash->dwName1   = dwName1;
    pHash->dwName2   = dwName2;
    pHash->lcLocale  = 0;
    pHash->wPlatform = wPlatform;
    pHash->dwBlockIndex = 0xFFFFFFFF;
    return pHash;
}

// Finds the nearest free hash entry for a file
TMPQHash * FindFreeHashEntry(TMPQArchive * ha, const char * szFileName)
{
    TMPQHash * pHash;
    DWORD dwBlockIndex = ha->pHeader->dwBlockTableSize;
    DWORD dwIndex;

    pHash = FindFreeHashEntry(ha->pHashTable, ha->pHeader->dwHashTableSize, szFileName);
    if(pHash != NULL)
    {
        // Now we have to find a free block entry
        for(dwIndex = 0; dwIndex < ha->pHeader->dwBlockTableSize; dwIndex++)
        {
            TMPQBlock * pBlock = ha->pBlockTable + dwIndex;

            if((pBlock->dwFlags & MPQ_FILE_EXISTS) == 0)
            {
                dwBlockIndex = dwIndex;
                break;
            }
        }

        // Put the block index to the hash table
        pHash->dwBlockIndex = dwBlockIndex;
    }

    return pHash;
}

// Finds a free space in the MPQ where to store next data
// The free space begins beyond the file that is stored at the fuhrtest
// position in the MPQ.
DWORD FindFreeMpqSpace(TMPQArchive * ha, PLARGE_INTEGER pMpqPos)
{
    LARGE_INTEGER TempPos;                  // For file position in MPQ
    LARGE_INTEGER MpqPos;                   // Free space position, relative to MPQ header
    TMPQBlockEx * pBlockEx = ha->pExtBlockTable;;
    TMPQBlock * pFreeBlock = NULL;
    TMPQBlock * pBlockEnd = ha->pBlockTable + ha->pHeader->dwBlockTableSize;
    TMPQBlock * pBlock = ha->pBlockTable;

    // The initial store position is after MPQ header
    MpqPos.QuadPart = ha->pHeader->dwHeaderSize;

    // Parse the entire block table
    while(pBlock < pBlockEnd)
    {
        // Only take existing files
        if(pBlock->dwFlags & MPQ_FILE_EXISTS)
        {
            // If the end of the file is bigger than current MPQ table pos, update it
            TempPos.HighPart = pBlockEx->wFilePosHigh;
            TempPos.LowPart = pBlock->dwFilePos;
            TempPos.QuadPart += pBlock->dwCSize;
            if(TempPos.QuadPart > MpqPos.QuadPart)
                MpqPos.QuadPart = TempPos.QuadPart;
        }
        else
        {
            if(pFreeBlock == NULL)
                pFreeBlock = pBlock;
        }

        // Move to the next block
        pBlockEx++;
        pBlock++;
    }

    // Give the free space position to the caller
    if(pMpqPos != NULL)
        *pMpqPos = MpqPos;

    // If we haven't found a free entry in the middle of the block table,
    // we have to increase the size of the block table. 
    if(pFreeBlock == NULL)
    {
        // Don't allow expanding block table if it's bigger than hash table size
        // This scenario will only happen on MPQs with some bogus entries
        // in the block table, which look like a file but don't have 
        // corresponding hash table entry.
        if(ha->pHeader->dwBlockTableSize >= ha->dwBlockTableMax)
            return 0xFFFFFFFF;

        // Take the entry past the current block table end.
        // DON'T increase size of the block table now, because
        // the caller might call this function without intend to add a file.
        pFreeBlock = pBlock;
    }

    // Return the index of the found free entry
    return (DWORD)(pFreeBlock - ha->pBlockTable);
}

//-----------------------------------------------------------------------------
// Common functions - MPQ File

// This function recalculates raw position of hash table,
// block table and extended block table.
static void CalculateTablePositions(TMPQArchive * ha)
{
    LARGE_INTEGER TablePos;                 // A table position, relative to the begin of the MPQ

    // Hash table position is calculated as position beyond
    // the latest stored file
    FindFreeMpqSpace(ha, &TablePos);
    ha->HashTablePos.QuadPart = ha->MpqPos.QuadPart + TablePos.QuadPart;

    // Update the hash table position in the MPQ header
    ha->pHeader->dwHashTablePos = TablePos.LowPart;
    ha->pHeader->wHashTablePosHigh = (USHORT)TablePos.HighPart;

    // Block table follows immediately after the hash table
    TablePos.QuadPart += ha->pHeader->dwHashTableSize * sizeof(TMPQHash);
    ha->BlockTablePos.QuadPart = ha->MpqPos.QuadPart + TablePos.QuadPart;

    // Update block table position in the MPQ header
    ha->pHeader->dwBlockTablePos = TablePos.LowPart;
    ha->pHeader->wBlockTablePosHigh = (USHORT)TablePos.HighPart;

    // Extended block table follows the old block table
    // Note that we will only use extended block table
    // if the current position is beyond 4 GB. 
    TablePos.QuadPart += ha->pHeader->dwBlockTableSize * sizeof(TMPQBlock);
    if(TablePos.HighPart != 0)
    {
        ha->ExtBlockTablePos.QuadPart = ha->MpqPos.QuadPart + TablePos.QuadPart;
        ha->pHeader->ExtBlockTablePos = TablePos;

        TablePos.QuadPart += ha->pHeader->dwBlockTableSize * sizeof(TMPQBlockEx);
    }
    else
    {
        ha->pHeader->ExtBlockTablePos.QuadPart = 0;
        ha->ExtBlockTablePos.QuadPart = 0;
    }

    // Update archive size in the header (only valid for MPQ v1)
    ha->pHeader->dwArchiveSize = (TablePos.HighPart == 0) ? TablePos.LowPart : 0;
}

TMPQFile * CreateMpqFile(TMPQArchive * ha, const char * szFileName)
{
    TMPQFile * hf;
    size_t nSize = sizeof(TMPQFile) + strlen(szFileName);

    // Allocate space for TMPQFile
    hf = (TMPQFile *)ALLOCMEM(BYTE, nSize);
    if(hf != NULL)
    {
        // Fill the file structure
        memset(hf, 0, nSize);
        hf->ha = ha;
        hf->pStream = NULL;
        hf->dwMagic = ID_MPQ_FILE;
        strcpy(hf->szFileName, szFileName);
    }

    return hf;
}

// Loads a table from MPQ.
// Can be used for hash table, block table, sector offset table or sector checksum table
int LoadMpqTable(
    TMPQArchive * ha,
    PLARGE_INTEGER pByteOffset,
    void * pvTable,
    DWORD dwCompressedSize,
    DWORD dwRealSize,
    const char * szKey)
{
    LPBYTE pbCompressed = NULL;
    LPBYTE pbToRead = (LPBYTE)pvTable;
    int nError = ERROR_SUCCESS;

    // Is the table compressed ?
    if(dwCompressedSize < dwRealSize)
    {
        // Allocate temporary buffer for holding compressed data
        pbCompressed = ALLOCMEM(BYTE, dwCompressedSize);
        if(pbCompressed == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Assign the temporary buffer as target for read operation
        pbToRead = pbCompressed;
    }

    // Read the table
    if(FileStream_Read(ha->pStream, pByteOffset, pbToRead, dwCompressedSize))
    {
        // First of all, decrypt the table
        if(szKey != NULL)
        {
            BSWAP_ARRAY32_UNSIGNED(pbToRead, dwCompressedSize);
            DecryptMpqTable(pbToRead, dwCompressedSize, szKey);
            BSWAP_ARRAY32_UNSIGNED(pbToRead, dwCompressedSize);
        }

        // If the table is compressed, decompress it
        if(dwCompressedSize < dwRealSize)
        {
            int cbOutBuffer = (int)dwRealSize;
            int cbInBuffer = (int)dwCompressedSize;

            if(!SCompDecompress((char *)pvTable, &cbOutBuffer, (char *)pbCompressed, cbInBuffer))
                nError = GetLastError();

            // Free the temporary buffer
            FREEMEM(pbCompressed);
        }
    }
    else
    {
        nError = GetLastError();
    }

    BSWAP_ARRAY32_UNSIGNED(pvTable, dwRealSize);
    return nError;
}


// Allocates sector buffer and sector offset table
int AllocateSectorBuffer(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;

    // Caller of AllocateSectorBuffer must ensure these
    assert(hf->pbFileSector == NULL);
    assert(hf->pBlock != NULL);
    assert(hf->ha != NULL);

    // Determine the file sector size and allocate buffer for it
    hf->dwSectorSize = (hf->pBlock->dwFlags & MPQ_FILE_SINGLE_UNIT) ? hf->pBlock->dwFSize : ha->dwSectorSize;
    hf->pbFileSector = ALLOCMEM(BYTE, hf->dwSectorSize);
    hf->dwSectorOffs = SFILE_INVALID_POS;

    // Return result
    return (hf->pbFileSector != NULL) ? (int)ERROR_SUCCESS : (int)ERROR_NOT_ENOUGH_MEMORY;
}

// Allocates sector offset table
int AllocateSectorOffsets(TMPQFile * hf, bool bLoadFromFile)
{
    TMPQArchive * ha = hf->ha;
    TMPQBlock * pBlock = hf->pBlock;
    DWORD dwArraySize;

    // Caller of AllocateSectorOffsets must ensure these
    assert(hf->SectorOffsets == NULL);
    assert(hf->pBlock != NULL);
    assert(hf->ha != NULL);

    // If the file is stored as single unit, just set number of sectors to 1
    if(pBlock->dwFlags & MPQ_FILE_SINGLE_UNIT)
    {
        hf->dwDataSectors = 1;
        hf->dwSectorCount = 1;
        return ERROR_SUCCESS;
    }

    // Calculate the number of data sectors
    hf->dwDataSectors = (pBlock->dwFSize / hf->dwSectorSize);
    if(pBlock->dwFSize % hf->dwSectorSize)
        hf->dwDataSectors++;

    // Calculate the number of file sectors
    // Note: I've seen corrupted MPQs that had MPQ_FILE_SECTOR_CRC flag absent,
    // but there was one extra sector offset, with the same value as the last one,
    // which corresponds to files with MPQ_FILE_SECTOR_CRC set, but with sector
    // checksums not present. StormLib doesn't handle such MPQs in any special way.
    // Compacting archive on such MPQs fails.
    hf->dwSectorCount = (pBlock->dwFSize / hf->dwSectorSize) + 1;
    if(pBlock->dwFSize % hf->dwSectorSize)
        hf->dwSectorCount++;
    if(pBlock->dwFlags & MPQ_FILE_SECTOR_CRC)
        hf->dwSectorCount++;

    // Only allocate and load the table if the file is compressed
    if(pBlock->dwFlags & MPQ_FILE_COMPRESSED)
    {
        // Allocate the sector offset table
        hf->SectorOffsets = ALLOCMEM(DWORD, hf->dwSectorCount);
        if(hf->SectorOffsets == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        // Calculate the size of the bytes to be read
        dwArraySize = hf->dwSectorCount * sizeof(DWORD);

        // Only read from the file if we are supposed to do so
        if(bLoadFromFile)
        {
            // Load the sector offsets from the file
            if(!FileStream_Read(ha->pStream, &hf->RawFilePos, hf->SectorOffsets, dwArraySize))
            {
                // Free the sector offsets
                FREEMEM(hf->SectorOffsets);
                hf->SectorOffsets = NULL;
                return GetLastError();
            }

            // Swap the sector positions
            BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwArraySize);

            // Decrypt loaded sector positions if necessary
            if(pBlock->dwFlags & MPQ_FILE_ENCRYPTED)
            {
                // If we don't know the file key, try to find it.
                if(hf->dwFileKey == 0)
                {
                    hf->dwFileKey = DetectFileKeyBySectorSize(hf->SectorOffsets, dwArraySize);
                    if(hf->dwFileKey == 0)
                    {
                        FREEMEM(hf->SectorOffsets);
                        hf->SectorOffsets = NULL;
                        return ERROR_UNKNOWN_FILE_KEY;
                    }
                }

                // Decrypt sector positions
                DecryptMpqBlock(hf->SectorOffsets, dwArraySize, hf->dwFileKey - 1);
            }

            //
            // Check if the sector positions are correct.
            // I saw a protector who puts negative offset into the sector offset table.
            // Because there are always at least 2 sector positions, we can check their difference
            //

            if((hf->SectorOffsets[1] - hf->SectorOffsets[0]) > ha->dwSectorSize)
            {
                FREEMEM(hf->SectorOffsets);
                hf->SectorOffsets = NULL;
                return ERROR_FILE_CORRUPT;
            }
        }
        else
        {
            memset(hf->SectorOffsets, 0, dwArraySize);
            hf->SectorOffsets[0] = dwArraySize;
        }
    }

    return ERROR_SUCCESS;
}

int AllocateSectorChecksums(TMPQFile * hf, bool bLoadFromFile)
{
    LARGE_INTEGER RawFilePos;
    TMPQArchive * ha = hf->ha;
    TMPQBlock * pBlock = hf->pBlock;
    DWORD dwCompressedSize;
    DWORD dwCrcOffset;                      // Offset of the CRC table, relative to file offset in the MPQ
    DWORD dwCrcSize;

    // Caller of AllocateSectorChecksums must ensure these
    assert(hf->SectorChksums == NULL);
    assert(hf->SectorOffsets != NULL);
    assert(hf->pBlock != NULL);
    assert(hf->ha != NULL);

    // Single unit files don't have sector checksums
    if(pBlock->dwFlags & MPQ_FILE_SINGLE_UNIT)
        return ERROR_SUCCESS;

    // Caller must ensure that we are only called when we have sector checksums
    assert(pBlock->dwFlags & MPQ_FILE_SECTOR_CRC);

    // If we only have to allocate the buffer, do it
    if(bLoadFromFile == false)
    {
        // Allocate buffer for sector checksums
        hf->SectorChksums = ALLOCMEM(DWORD, hf->dwDataSectors);
        if(hf->SectorChksums == NULL)
            return ERROR_NOT_ENOUGH_MEMORY;

        memset(hf->SectorChksums, 0, hf->dwDataSectors * sizeof(DWORD));
        return ERROR_SUCCESS;
    }

    // Check size of the checksums. If zero, there aren't any
    dwCompressedSize = hf->SectorOffsets[hf->dwDataSectors + 1] - hf->SectorOffsets[hf->dwDataSectors];
    if(dwCompressedSize == 0)
        return ERROR_SUCCESS;

    // Allocate buffer for sector CRCs
    hf->SectorChksums = ALLOCMEM(DWORD, hf->dwDataSectors);
    if(hf->SectorChksums == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Calculate offset of the CRC table
    dwCrcSize = hf->dwDataSectors * sizeof(DWORD);
    dwCrcOffset = hf->SectorOffsets[hf->dwDataSectors];
    CalculateRawSectorOffset(RawFilePos, hf, dwCrcOffset); 

    // Now read the table from the MPQ
    return LoadMpqTable(ha, &RawFilePos, hf->SectorChksums, dwCompressedSize, dwCrcSize, NULL);
}

void CalculateRawSectorOffset(
    LARGE_INTEGER & RawFilePos, 
    TMPQFile * hf,
    DWORD dwSectorOffset)
{
    //
    // Some MPQ protectors place the sector offset table after the actual file data.
    // Sector offsets in the sector offset table are negative. When added
    // to MPQ file offset from the block table entry, the result is a correct
    // position of the file data in the MPQ.
    //
    // The position of sector table must be always within the MPQ, however.
    // When a negative sector offset is found, we make sure that we make the addition
    // just in 32-bits, and then add the MPQ offset.
    //

    if(dwSectorOffset & 0x80000000)
    {
        dwSectorOffset += hf->pBlock->dwFilePos;
        RawFilePos.QuadPart = hf->ha->MpqPos.QuadPart + dwSectorOffset;
    }
    else
    {
        RawFilePos.QuadPart = hf->RawFilePos.QuadPart + dwSectorOffset;
    }
}

int WriteSectorOffsets(TMPQFile * hf)
{
    TMPQArchive * ha = hf->ha;
    TMPQBlock * pBlock = hf->pBlock;
    DWORD dwSectorPosLen = hf->dwSectorCount * sizeof(DWORD);
    bool bResult;

    // The caller must make sure that this function is only called
    // when the following is true.
    assert(hf->pBlock->dwFlags & MPQ_FILE_COMPRESSED);
    assert(hf->SectorOffsets != NULL);

    // If file is encrypted, sector positions are also encrypted
    if(pBlock->dwFlags & MPQ_FILE_ENCRYPTED)
        EncryptMpqBlock(hf->SectorOffsets, dwSectorPosLen, hf->dwFileKey - 1);
    
    // Write sector offsets to the archive
    BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwSectorPosLen);
    bResult = FileStream_Write(ha->pStream, &hf->RawFilePos, hf->SectorOffsets, dwSectorPosLen);
    
    // Not necessary, as the sector checksums
    // are going to be freed when this is done.
//  BSWAP_ARRAY32_UNSIGNED(hf->SectorOffsets, dwSectorPosLen);

    if(!bResult)
        return GetLastError();
    return ERROR_SUCCESS;
}


int WriteSectorChecksums(TMPQFile * hf)
{
    LARGE_INTEGER RawFilePos;
    TMPQArchive * ha = hf->ha;
    TMPQBlock * pBlock = hf->pBlock;
    LPBYTE pbCompressed;
    DWORD dwCompressedSize = 0;
    DWORD dwCrcSize;
    int nOutSize;
    int nError = ERROR_SUCCESS;

    // The caller must make sure that this function is only called
    // when the following is true.
    assert(hf->pBlock->dwFlags & MPQ_FILE_SECTOR_CRC);
    assert(hf->SectorOffsets != NULL);
    assert(hf->SectorChksums != NULL);

    // Calculate size of the checksum array
    dwCrcSize = hf->dwDataSectors * sizeof(DWORD);

    // Allocate buffer for compressed sector CRCs.
    pbCompressed = ALLOCMEM(BYTE, dwCrcSize);
    if(pbCompressed == NULL)
        return ERROR_NOT_ENOUGH_MEMORY;

    // Perform the compression
    BSWAP_ARRAY32_UNSIGNED(hf->SectorChksums, dwCrcSize);

    nOutSize = (int)dwCrcSize;
    SCompCompress((char *)pbCompressed, &nOutSize, (char *)hf->SectorChksums, (int)dwCrcSize, MPQ_COMPRESSION_ZLIB, 0, 0);
    dwCompressedSize = (DWORD)nOutSize;

    // Write the sector CRCs to the archive
    RawFilePos.QuadPart = hf->RawFilePos.QuadPart + hf->SectorOffsets[hf->dwSectorCount - 2];
    if(!FileStream_Write(ha->pStream, &RawFilePos, pbCompressed, dwCompressedSize))
        nError = GetLastError();

    // Not necessary, as the sector checksums
    // are going to be freed when this is done.
//  BSWAP_ARRAY32_UNSIGNED(hf->SectorChksums, dwCrcSize);

    // Store the sector CRCs 
    hf->SectorOffsets[hf->dwSectorCount - 1] = hf->SectorOffsets[hf->dwSectorCount - 2] + dwCompressedSize;
    pBlock->dwCSize += dwCompressedSize;
    FREEMEM(pbCompressed);
    return nError;
}

// Frees the structure for MPQ file
void FreeMPQFile(TMPQFile *& hf)
{
    if(hf != NULL)
    {
        if(hf->SectorOffsets != NULL)
            FREEMEM(hf->SectorOffsets);
        if(hf->SectorChksums != NULL)
            FREEMEM(hf->SectorChksums);
        if(hf->pbFileSector != NULL)
            FREEMEM(hf->pbFileSector);
        FileStream_Close(hf->pStream);
        FREEMEM(hf);
        hf = NULL;
    }
}

// Saves MPQ header, hash table, block table and extended block table.
int SaveMPQTables(TMPQArchive * ha)
{
    BYTE * pbBuffer = NULL;
    DWORD dwBytes;
    DWORD dwBuffSize = STORMLIB_MAX(ha->pHeader->dwHashTableSize, ha->pHeader->dwBlockTableSize);
    int   nError = ERROR_SUCCESS;

    // Calculate positions of the MPQ tables.
    // This sets HashTablePos, BlockTablePos and ExtBlockTablePos,
    // as well as the values in the MPQ header
    CalculateTablePositions(ha);

    // Allocate temporary buffer for tables encryption
    pbBuffer = ALLOCMEM(BYTE, sizeof(TMPQHash) * dwBuffSize);
    if(pbBuffer == NULL)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    // Write the MPQ Header
    if(nError == ERROR_SUCCESS)
    {
        // Remember the header size before swapping
        DWORD dwBytesToWrite = ha->pHeader->dwHeaderSize;

        // Write the MPQ header
        BSWAP_TMPQHEADER(ha->pHeader);
        if(FileStream_Write(ha->pStream, &ha->MpqPos, ha->pHeader, dwBytesToWrite))
            ha->dwFlags &= ~MPQ_FLAG_NO_HEADER;
        else
            nError = GetLastError();
        BSWAP_TMPQHEADER(ha->pHeader);
    }

    // Write the hash table
    if(nError == ERROR_SUCCESS)
    {
        // Copy the hash table to temporary buffer
        dwBytes = ha->pHeader->dwHashTableSize * sizeof(TMPQHash);
        memcpy(pbBuffer, ha->pHashTable, dwBytes);

        // Convert to little endian for file save
        EncryptMpqTable(pbBuffer, dwBytes, "(hash table)");

        // Set the file pointer to the offset of the hash table and write it
        BSWAP_ARRAY32_UNSIGNED(pbBuffer, dwBytes);
        if(!FileStream_Write(ha->pStream, &ha->HashTablePos, pbBuffer, dwBytes))
            nError = GetLastError();
    }

    // Write the block table
    if(nError == ERROR_SUCCESS)
    {
        // Copy the block table to temporary buffer
        dwBytes = ha->pHeader->dwBlockTableSize * sizeof(TMPQBlock);
        memcpy(pbBuffer, ha->pBlockTable, dwBytes);

        // Encrypt the block table and write it to the file
        EncryptMpqTable(pbBuffer, dwBytes, "(block table)");
        
        // Convert to little endian for file save
        BSWAP_ARRAY32_UNSIGNED(pbBuffer, dwBytes);
        if(!FileStream_Write(ha->pStream, &ha->BlockTablePos, pbBuffer, dwBytes))
            nError = GetLastError();
    }

    // Write the extended block table
    if(nError == ERROR_SUCCESS && ha->pHeader->ExtBlockTablePos.QuadPart != 0)
    {
        // We expect format V2 or newer in this case
        assert(ha->pHeader->wFormatVersion >= MPQ_FORMAT_VERSION_2);

        // Copy the block table to temporary buffer
        dwBytes = ha->pHeader->dwBlockTableSize * sizeof(TMPQBlockEx);
        memcpy(pbBuffer, ha->pExtBlockTable, dwBytes);

        // Convert to little endian for file save
        BSWAP_ARRAY16_UNSIGNED(pbBuffer, dwBytes);
        if(!FileStream_Write(ha->pStream, &ha->ExtBlockTablePos, pbBuffer, dwBytes))
            nError = GetLastError();
    }

    // Set end of file here
    if(nError == ERROR_SUCCESS)
    {
        LARGE_INTEGER NewFileSize;

        FileStream_GetPos(ha->pStream, &NewFileSize);
        FileStream_SetSize(ha->pStream, &NewFileSize);
        ha->dwFlags &= ~MPQ_FLAG_CHANGED;
    }

    // Cleanup and exit
    if(pbBuffer != NULL)
        FREEMEM(pbBuffer);
    return nError;
}

// Frees the MPQ archive
void FreeMPQArchive(TMPQArchive *& ha)
{
    if(ha != NULL)
    {
        if(ha->pExtBlockTable != NULL)
            FREEMEM(ha->pExtBlockTable);
        if(ha->pBlockTable != NULL)
            FREEMEM(ha->pBlockTable);
        if(ha->pHashTable != NULL)
            FREEMEM(ha->pHashTable);
        if(ha->pListFile != NULL)
            SListFileFreeListFile(ha);
        if(ha->pAttributes != NULL)
            FreeMPQAttributes(ha->pAttributes);
        FileStream_Close(ha->pStream);
        FREEMEM(ha);
        ha = NULL;
    }
}

const char * GetPlainLocalFileName(const char * szFileName)
{
#ifdef WIN32
    const char * szPlainName = strrchr(szFileName, '\\');
#else
    const char * szPlainName = strrchr(szFileName, '/');
#endif
    return (szPlainName != NULL) ? szPlainName + 1 : szFileName;
}

const char * GetPlainMpqFileName(const char * szFileName)
{
    const char * szPlainName = strrchr(szFileName, '\\');
    return (szPlainName != NULL) ? szPlainName + 1 : szFileName;
}

//-----------------------------------------------------------------------------
// Swapping functions

#ifndef PLATFORM_LITTLE_ENDIAN

//
// Note that those functions are implemented for Mac operating system,
// as this is the only supported platform that uses big endian.
//

// Swaps a signed 16-bit integer
int16_t SwapShort(uint16_t data)
{
	return (int16_t)CFSwapInt16(data);
}

// Swaps an unsigned 16-bit integer
uint16_t SwapUShort(uint16_t data)
{
	return CFSwapInt16(data);
}

// Swaps signed 32-bit integer
int32_t SwapLong(uint32_t data)
{
	return (int32_t)CFSwapInt32(data);
}

// Swaps an unsigned 32-bit integer
uint32_t SwapULong(uint32_t data)
{
	return CFSwapInt32(data);
}

// Swaps array of unsigned 16-bit integers
void ConvertUnsignedShortBuffer(void * ptr, size_t length)
{
    uint16_t * buffer = (uint16_t *)ptr;
    uint32_t nbShorts = (uint32_t)(length / sizeof(uint16_t));

    while (nbShorts-- > 0)
	{
		*buffer = SwapUShort(*buffer);
		buffer++;
	}
}

// Swaps array of unsigned 32-bit integers
void ConvertUnsignedLongBuffer(void * ptr, size_t length)
{
    uint32_t * buffer = (uint32_t *)ptr;
    uint32_t nbLongs = (uint32_t)(length / sizeof(uint32_t));

	while (nbLongs-- > 0)
	{
		*buffer = SwapULong(*buffer);
		buffer++;
	}
}

// Swaps the TMPQUserData structure
void ConvertTMPQUserData(void *userData)
{
	TMPQUserData * theData = (TMPQUserData *)userData;

	theData->dwID = SwapULong(theData->dwID);
	theData->cbUserDataSize = SwapULong(theData->cbUserDataSize);
	theData->dwHeaderOffs = SwapULong(theData->dwHeaderOffs);
	theData->cbUserDataHeader = SwapULong(theData->cbUserDataHeader);
}

// Swaps the TMPQHeader structure
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

#endif  // PLATFORM_LITTLE_ENDIAN
