/*****************************************************************************/
/* StormLibTest.cpp                       Copyright (c) Ladislav Zezula 2003 */
/*---------------------------------------------------------------------------*/
/* This module uses very brutal test methods for StormLib. It extracts all   */
/* files from the archive with Storm.dll and with stormlib and compares them,*/
/* then tries to build a copy of the entire archive, then removes a few files*/
/* from the archive and adds them back, then compares the two archives, ...  */
/*---------------------------------------------------------------------------*/
/*   Date    Ver   Who  Comment                                              */
/* --------  ----  ---  -------                                              */
/* 25.03.03  1.00  Lad  The first version of StormLibTest.cpp                */
/*****************************************************************************/

#define _CRT_SECURE_NO_DEPRECATE
#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>

#define __STORMLIB_SELF__               // Don't use StormLib.lib
#define __LINK_STORM_DLL__
#include "../src/StormLib.h"

#pragma warning(disable : 4505) 
#pragma comment(lib, "Winmm.lib")

//------------------------------------------------------------------------------
// Defines

#define MPQ_SECTOR_SIZE 0x1000

//-----------------------------------------------------------------------------
// Constants

static const char * szWorkDir = "E:\\Multimedia\\MPQs\\Work";

static DWORD AddFlags[] = 
{
//  Compression          Encryption             Fixed key           Single Unit            Sector CRC
    0                 |  0                   |  0                 | 0                    | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    0                 |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    0                 |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | 0                    | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_IMPLODE  |  0                   |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_IMPLODE  |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  0                   |  0                 | 0                    | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | 0,
    MPQ_FILE_COMPRESS |  0                   |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | MPQ_FILE_SINGLE_UNIT | 0,
    MPQ_FILE_COMPRESS |  0                   |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  0                 | 0                    | MPQ_FILE_SECTOR_CRC,
    MPQ_FILE_COMPRESS |  MPQ_FILE_ENCRYPTED  |  MPQ_FILE_FIX_KEY  | 0                    | MPQ_FILE_SECTOR_CRC,
    0xFFFFFFFF
};

//-----------------------------------------------------------------------------
// Local testing functions

static void clreol()
{
    CONSOLE_SCREEN_BUFFER_INFO ScreenInfo;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    LPTSTR szConsoleLine;
    int nConsoleChars;
    int i = 0;

    GetConsoleScreenBufferInfo(hConsole, &ScreenInfo);
    nConsoleChars = (ScreenInfo.srWindow.Right - ScreenInfo.srWindow.Left);
    if(nConsoleChars > 0)
    {
        szConsoleLine = new TCHAR[nConsoleChars + 3];
        if(szConsoleLine != NULL)
        {
            szConsoleLine[i++] = '\r';
            for(; i < nConsoleChars; i++)
                szConsoleLine[i] = ' ';
            szConsoleLine[i++] = '\r';
            szConsoleLine[i] = 0;

            printf(szConsoleLine);
            delete []  szConsoleLine;
        }
    }
}

static const char * GetPlainName(const char * szFileName)
{
    const char * szTemp;

    if((szTemp = strrchr(szFileName, '\\')) != NULL)
        szFileName = szTemp + 1;
    return szFileName;
}

int GetFirstDiffer(void * ptr1, void * ptr2, int nSize)
{
    char * buff1 = (char *)ptr1;
    char * buff2 = (char *)ptr2;
    int nDiffer;

    for(nDiffer = 0; nDiffer < nSize; nDiffer++)
    {
        if(*buff1++ != *buff2++)
            return nDiffer;
    }
    return -1;
}

static void ShowProcessedFile(const char * szFileName)
{
    char szLine[80];
    size_t nLength = strlen(szFileName);

    memset(szLine, 0x20, sizeof(szLine));
    szLine[sizeof(szLine)-1] = 0;

    if(nLength > sizeof(szLine)-1)
        nLength = sizeof(szLine)-1;
    memcpy(szLine, szFileName, nLength);
    printf("\r%s\r", szLine);
}


static void WINAPI CompactCB(void * /* lpParam */, DWORD dwWork, LARGE_INTEGER * pBytesDone, LARGE_INTEGER * pTotalBytes)
{
    clreol();

    printf("%u of %u ", pBytesDone->LowPart, pTotalBytes->LowPart);
    switch(dwWork)
    {
        case CCB_CHECKING_FILES:
            printf("Checking files in archive ...\r");
            break;

        case CCB_CHECKING_HASH_TABLE:
            printf("Checking hash table ...\r");
            break;

        case CCB_COPYING_NON_MPQ_DATA:
            printf("Copying non-MPQ data ...\r");
            break;

        case CCB_COMPACTING_FILES:
            printf("Compacting archive ...\r");
            break;

        case CCB_CLOSING_ARCHIVE:
            printf("Closing archive ...\r");
            break;
    }
}

static void GenerateRandomDataBlock(LPBYTE pbBuffer, DWORD cbBuffer)
{
    LPBYTE pbBufferEnd = pbBuffer + cbBuffer;
    LPBYTE pbPtr = pbBuffer;
    DWORD cbBytesToPut = 0;
    BYTE ByteToPut = 0;
    bool bRandomData = false;

    while(pbPtr < pbBufferEnd)
    {
        // If there are no bytes to put, we will generate new byte and length
        if(cbBytesToPut == 0)
        {
            bRandomData = false;
            switch(rand() % 10)
            {
                case 0:     // A short sequence of zeros
                    cbBytesToPut = rand() % 0x08;
                    ByteToPut = 0;
                    break;

                case 1:     // A long sequence of zeros
                    cbBytesToPut = rand() % 0x80;
                    ByteToPut = 0;
                    break;

                case 2:     // A short sequence of non-zeros
                    cbBytesToPut = rand() % 0x08;
                    ByteToPut = (BYTE)(rand() % 0x100);
                    break;

                case 3:     // A long sequence of non-zeros
                    cbBytesToPut = rand() % 0x80;
                    ByteToPut = (BYTE)(rand() % 0x100);
                    break;

                case 4:     // A short random data
                    cbBytesToPut = rand() % 0x08;
                    bRandomData = true;
                    break;

                case 5:     // A long random data
                    cbBytesToPut = rand() % 0x80;
                    bRandomData = true;
                    break;

                default:    // A single random byte
                    cbBytesToPut = 1;
                    ByteToPut = (BYTE)(rand() % 0x100);
                    break;
            }
        }

        // Generate random byte, if needed
        if(bRandomData)
            ByteToPut = (BYTE)(rand() % 0x100);

        // Put next byte to the output buffer
        *pbPtr++ = ByteToPut;
        cbBytesToPut--;
    }
}

static BOOL CompareArchivedFiles(const char * szFileName, HANDLE hFile1, HANDLE hFile2, DWORD dwBlockSize)
{
    LPBYTE pbBuffer1 = NULL;
    LPBYTE pbBuffer2 = NULL;
    DWORD dwRead1;                      // Number of bytes read (Storm.dll)
    DWORD dwRead2;                      // Number of bytes read (StormLib)
    BOOL bResult1 = FALSE;              // Result from Storm.dll
    BOOL bResult2 = FALSE;              // Result from StormLib
    BOOL bResult = TRUE;
    int nDiff;

    UNREFERENCED_PARAMETER(szFileName);

    // Allocate buffers
    pbBuffer1 = new BYTE[dwBlockSize];
    pbBuffer2 = new BYTE[dwBlockSize];

    for(;;)
    {
        // Read the file's content by both methods and compare the result
        memset(pbBuffer1, 0, dwBlockSize);
        memset(pbBuffer2, 0, dwBlockSize);
        bResult1 = SFileReadFile(hFile1, pbBuffer1, dwBlockSize, &dwRead1, NULL);
        bResult2 = SFileReadFile(hFile2, pbBuffer2, dwBlockSize, &dwRead2, NULL);
        if(bResult1 != bResult2)
        {
            printf("Different results from SFileReadFile, Mpq1 %u, Mpq2 %u\n", bResult1, bResult2);
            bResult = FALSE;
            break;
        }

        // Test the number of bytes read
        if(dwRead1 != dwRead2)
        {
            printf("Different bytes read from SFileReadFile, Mpq1 %u, Mpq2 %u\n", dwRead1, dwRead2);
            bResult = FALSE;
            break;
        }

        // No more bytes ==> OK
        if(dwRead1 == 0)
            break;
        
        // Test the content
        if((nDiff = GetFirstDiffer(pbBuffer1, pbBuffer2, dwRead1)) != -1)
        {
            bResult = FALSE;
            break;
        }
    }

    delete [] pbBuffer2;
    delete [] pbBuffer1;
    return bResult;
}

// Random read version
static BOOL CompareArchivedFilesRR(const char * /* szFileName */, HANDLE hFile1, HANDLE hFile2, DWORD dwBlockSize)
{
    LPCTSTR szPositions[3] = {"FILE_BEGIN  ", "FILE_CURRENT", "FILE_END    "};
    LPBYTE pbBuffer1 = NULL;
    LPBYTE pbBuffer2 = NULL;
    DWORD dwFileSize1;                  // File size (Storm.dll)
    DWORD dwFileSize2;                  // File size (StormLib)
    DWORD dwRead1;                      // Number of bytes read (Storm.dll)
    DWORD dwRead2;                      // Number of bytes read (StormLib)
    BOOL bResult1 = FALSE;              // Result from Storm.dll
    BOOL bResult2 = FALSE;              // Result from StormLib
    int nError = ERROR_SUCCESS;

    // Test the file size
    dwFileSize1 = SFileGetFileSize(hFile1, NULL);
    dwFileSize2 = SFileGetFileSize(hFile2, NULL);
    if(dwFileSize1 != dwFileSize2)
    {
        printf("Different size from SFileGetFileSize, Storm.dll: %u, StormLib: %u !!!!\n", dwFileSize1, dwFileSize2);
        return FALSE;
    }

    if(dwFileSize1 != 0)
    {
        for(int i = 0; i < 10000; i++)
        {
            DWORD dwRandom     = rand() * rand();
            DWORD dwMoveMethod = dwRandom % 3;
            DWORD dwPosition   = dwRandom % dwFileSize1;
            DWORD dwToRead     = dwRandom % dwBlockSize; 

            // Also test negative seek
            if(rand() & 1)
            {
                int nPosition = (int)dwPosition;
                dwPosition = (DWORD)(-nPosition);
            }

            // Allocate buffers
            pbBuffer1 = new BYTE[dwToRead];
            pbBuffer2 = new BYTE[dwToRead];

            // Set the file pointer
            printf("RndRead (%u): pos %8i from %s, size %u ...\r", i, dwPosition, szPositions[dwMoveMethod], dwToRead);
            dwRead1 = SFileSetFilePointer(hFile1, dwPosition, NULL, dwMoveMethod);
            dwRead2 = SFileSetFilePointer(hFile2, dwPosition, NULL, dwMoveMethod);
            if(dwRead1 != dwRead2)
            {
                printf("Difference returned by SFileSetFilePointer, Storm.dll: %u, StormLib: %u !!!!\n", dwRead1, dwRead2);
                nError = ERROR_READ_FAULT;
                break;
            }

            // Read the file's content by both methods and compare the result
            bResult1 = SFileReadFile(hFile1, pbBuffer1, dwToRead, &dwRead1, NULL);
            bResult2 = SFileReadFile(hFile2, pbBuffer2, dwToRead, &dwRead2, NULL);
            if(bResult1 != bResult2)
            {
                printf("Different results from SFileReadFile, Storm.dll: %u, StormLib: %u !!!!\n", bResult1, bResult2);
                nError = ERROR_READ_FAULT;
                break;
            }

            // Test the number of bytes read
            if(dwRead1 != dwRead2)
            {
                printf("Different bytes read from SFileReadFile, Storm.dll: %u, StormLib: %u !!!!\n", dwRead1, dwRead2);
                nError = ERROR_READ_FAULT;
                break;
            }
            
            // Test the content
            if(dwRead1 != 0 && memcmp(pbBuffer1, pbBuffer2, dwRead1))
            {
                printf("Different data content from SFileReadFile !!\n");
                nError = ERROR_READ_FAULT;
                break;
            }

            delete [] pbBuffer2;
            delete [] pbBuffer1;
        }
    }
    clreol();
    return (nError == ERROR_SUCCESS) ? TRUE : FALSE;
}

//-----------------------------------------------------------------------------
// Partial file reading

static int TestPartFileRead(const char * szFileName)
{
    LARGE_INTEGER ByteOffset;
    LARGE_INTEGER FileSize;
    TFileStream * pStream;
    BYTE BigBuffer[0x7000];
    BYTE Buffer[0x100];
    int nError = ERROR_SUCCESS;

    // Open the partial file
    pStream = FileStream_OpenFile(szFileName, false);
    if(pStream == NULL)
        nError = GetLastError();

    // Get the size of the stream
    if(nError == ERROR_SUCCESS)
    {
        if(!FileStream_GetSize(pStream, &FileSize))
            nError = GetLastError();
    }

    // Read the last 0x7000 bytes
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset.QuadPart = FileSize.QuadPart - sizeof(BigBuffer);
        if(!FileStream_Read(pStream, &ByteOffset, BigBuffer, sizeof(BigBuffer)))
            nError = GetLastError();
    }

    // Read the last 0x100 bytes
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset.QuadPart = FileSize.QuadPart - sizeof(Buffer);
        if(!FileStream_Read(pStream, &ByteOffset, Buffer, sizeof(Buffer)))
            nError = GetLastError();
    }

    // Read 0x100 bytes from position (FileSize - 0xFF)
    if(nError == ERROR_SUCCESS)
    {
        ByteOffset.QuadPart = FileSize.QuadPart - sizeof(Buffer) + 1;
        if(!FileStream_Read(pStream, &ByteOffset, Buffer, sizeof(Buffer)))
            nError = GetLastError();
    }

    FileStream_Close(pStream);
    return nError;
}

//-----------------------------------------------------------------------------
// Compare sparse compression

extern "C" void starcraft_compress_sparse(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer);
extern "C" int starcraft_decompress_sparse(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer);
void Compress_SPARSE(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer, int * /* pCmpType */, int /* nCmpLevel */);
int Decompress_SPARSE(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer);

static int CompareSparseCompressions(int nSectorSize)
{
    LPBYTE pbCompressed1 = NULL;            // Compressed by our code
    LPBYTE pbCompressed2 = NULL;            // Compressed by Blizzard's code
    LPBYTE pbDecompressed1 = NULL;          // Decompressed by our code
    LPBYTE pbDecompressed2 = NULL;          // Decompressed by Blizzard's code
    LPBYTE pbOriginalData = NULL;
    int nError = ERROR_SUCCESS;

    // Allocate buffers
    // Must allocate twice blocks due to probable bug in Storm.dll.
    // Storm.dll corrupts stack when uncompresses data with PKWARE DCL
    // and no compression occurs.
    pbDecompressed1 = new BYTE [nSectorSize];
    pbDecompressed2 = new BYTE [nSectorSize];
    pbCompressed1 = new BYTE [nSectorSize];
    pbCompressed2 = new BYTE [nSectorSize];
    pbOriginalData = new BYTE[nSectorSize];
    if(!pbDecompressed1 || !pbDecompressed2 || !pbCompressed1 || !pbCompressed2 || !pbOriginalData)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    if(nError == ERROR_SUCCESS)
    {
        for(int i = 0; i < 100000; i++)
        {
            int   nDcmpLength1;
            int   nDcmpLength2;
            int   nCmpLength1;
            int   nCmpLength2;
            int   nDiff;

            clreol();
            printf("Testing compression of sector %u\r", i + 1);

            // Generate random data sector
            GenerateRandomDataBlock(pbOriginalData, nSectorSize);

__TryToCompress:

            // Compress the sector by both methods
            nCmpLength1 = nCmpLength2 = nSectorSize;
            Compress_SPARSE((char *)pbCompressed1, &nCmpLength1, (char *)pbOriginalData, nSectorSize, 0, 0);
            starcraft_compress_sparse((char *)pbCompressed2, &nCmpLength2, (char *)pbOriginalData, nSectorSize);

            // Compare the length of the output data
            if(nCmpLength1 != nCmpLength2)
            {
                printf("Difference in compressed blocks lengths (%u vs %u)\n", nCmpLength1, nCmpLength2);
                goto __TryToCompress;
            }

            // Compare the output
            if((nDiff = GetFirstDiffer(pbCompressed1, pbCompressed2, nCmpLength1)) != -1)
            {
                printf("Difference in compressed blocks (offset 0x%08lX)\n", nDiff);
                goto __TryToCompress;
            }

            // Check for data overflow
            if(pbCompressed1[nSectorSize] != 0xFD || pbCompressed2[nSectorSize] != 0xFD)
            {
                printf("Damage after compressed sector !!!\n");
                goto __TryToCompress;
            }

__TryToDecompress:

            // Only test decompression when the compression actually succeeded
            if(nCmpLength1 < nSectorSize)
            {
                // Decompress both data
                nDcmpLength2 = nDcmpLength1 = nSectorSize;
                Decompress_SPARSE((char *)pbDecompressed1, &nDcmpLength1, (char *)pbCompressed1, nCmpLength1);
                starcraft_decompress_sparse((char *)pbDecompressed2, &nDcmpLength2, (char *)pbCompressed2, nCmpLength2);

                // Compare the length of the output data
                if(nDcmpLength1 != nDcmpLength2)
                {
                    printf("Difference in compressed blocks lengths (%u vs %u)\n", nDcmpLength1, nDcmpLength2);
                    goto __TryToDecompress;
                }

                // Compare the output
                if((nDiff = GetFirstDiffer(pbDecompressed1, pbDecompressed2, nDcmpLength1)) != -1)
                {
                    printf("Difference in decompressed blocks (offset 0x%08lX)\n", nDiff);
                    goto __TryToDecompress;
                }

                // Check for data overflow
                if(pbDecompressed1[nSectorSize] != 0xFD || pbDecompressed1[nSectorSize] != 0xFD)
                {
                    printf("Damage after decompressed sector !!!\n");
                    goto __TryToDecompress;
                }

                // Compare the decompressed data against original data
                if((nDiff = GetFirstDiffer(pbDecompressed1, pbOriginalData, nDcmpLength1)) != -1)
                {
                    printf("Difference between original data and decompressed data (offset 0x%08lX)\n", nDiff);
                    goto __TryToDecompress;
                }
            }
        }
    }

    // Cleanup
    if(pbOriginalData != NULL)
        delete [] pbOriginalData;
    if(pbCompressed2 != NULL)
        delete [] pbCompressed2;
    if(pbCompressed1 != NULL)
        delete [] pbCompressed1;
    if(pbDecompressed2 != NULL)
        delete [] pbDecompressed2;
    if(pbDecompressed1 != NULL)
        delete [] pbDecompressed1;
    clreol();
    return nError;
}

//-----------------------------------------------------------------------------
// Compare LZMA decompression

typedef void * (*ALLOC_MEMORY)(size_t);
typedef void (*FREE_MEMORY)(void *);
typedef int (GIVE_DATA)(void *);

extern "C" int starcraft_decompress_lzma(char * pbInBuffer, int cbInBuffer, char * pbOutBuffer, int cbOutBuffer, int * pcbOutBuffer, ALLOC_MEMORY pfnAllocMemory, FREE_MEMORY pfnFreeMemory);
extern "C" int starcraft_compress_lzma(char * pbInBuffer, int cbInBuffer, int dummy1, char * pbOutBuffer, int cbOutBuffer, int dummy2, int * pcbOutBuffer, ALLOC_MEMORY pfnAllocMemory, FREE_MEMORY pfnFreeMemory, GIVE_DATA pfnGiveData);
void Compress_LZMA(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer, int * /* pCmpType */, int /* nCmpLevel */);
int Decompress_LZMA(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer);

extern "C" void * operator_new(size_t sz)
{
    return malloc(sz);
}

void * Memory_Allocate(size_t byte_size)
{
    return malloc(byte_size);
}

void Memory_Free(void * address)
{
    if(address != NULL)
        free(address);
}

int GiveData(void *)
{
    return 0;
}

static int StarcraftCompress_LZMA(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer)
{
    return starcraft_compress_lzma(pbInBuffer,
                                   cbInBuffer,
                                   0,
                                   pbOutBuffer,
                                  *pcbOutBuffer,
                                   0,
                                   pcbOutBuffer,
                                   Memory_Allocate,
                                   Memory_Free,
                                   GiveData);
}

static int StarcraftDecompress_LZMA(char * pbOutBuffer, int * pcbOutBuffer, char * pbInBuffer, int cbInBuffer)
{
    return starcraft_decompress_lzma(pbInBuffer,
                                     cbInBuffer,
                                     pbOutBuffer,
                                     *pcbOutBuffer,
                                     pcbOutBuffer,
                                     Memory_Allocate,
                                     Memory_Free);
}

static int CompareLzmaCompressions(int nSectorSize)
{
    LPBYTE pbCompressed1 = NULL;            // Compressed by our code
    LPBYTE pbCompressed2 = NULL;            // Compressed by Blizzard's code
    LPBYTE pbDecompressed1 = NULL;          // Decompressed by our code
    LPBYTE pbDecompressed2 = NULL;          // Decompressed by Blizzard's code
    LPBYTE pbOriginalData = NULL;
    int nError = ERROR_SUCCESS;

    // Allocate buffers
    // Must allocate twice blocks due to probable bug in Storm.dll.
    // Storm.dll corrupts stack when uncompresses data with PKWARE DCL
    // and no compression occurs.
    pbDecompressed1 = new BYTE [nSectorSize];
    pbDecompressed2 = new BYTE [nSectorSize];
    pbCompressed1 = new BYTE [nSectorSize];
    pbCompressed2 = new BYTE [nSectorSize];
    pbOriginalData = new BYTE[nSectorSize];
    if(!pbDecompressed1 || !pbDecompressed2 || !pbCompressed1 || !pbCompressed2 || !pbOriginalData)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    if(nError == ERROR_SUCCESS)
    {
        for(int i = 0; i < 100000; i++)
        {
            int   nDcmpLength1;
            int   nDcmpLength2;
            int   nCmpLength1;
            int   nCmpLength2;
            int   nDiff;

            clreol();
            printf("Testing compression of sector %u\r", i + 1);

            // Generate random data sector
            GenerateRandomDataBlock(pbOriginalData, nSectorSize);

            // Compress the sector by both methods
            nCmpLength1 = nCmpLength2 = nSectorSize;
//          Compress_LZMA((char *)pbCompressed1, &nCmpLength1, (char *)pbOriginalData, nSectorSize, 0, 0);
            StarcraftCompress_LZMA((char *)pbCompressed1, &nCmpLength2, (char *)pbOriginalData, nSectorSize);

__TryToDecompress:

            // Only test decompression when the compression actually succeeded
            if(nCmpLength1 < nSectorSize)
            {
                // Decompress both data
                nDcmpLength2 = nDcmpLength1 = nSectorSize;
//              Decompress_LZMA((char *)pbDecompressed1, &nDcmpLength1, (char *)pbCompressed1, nCmpLength1);
                StarcraftDecompress_LZMA((char *)pbDecompressed2, &nDcmpLength2, (char *)pbCompressed1, nCmpLength1);

                // Compare the length of the output data
                if(nDcmpLength1 != nDcmpLength2)
                {
                    printf("Difference in compressed blocks lengths (%u vs %u)\n", nDcmpLength1, nDcmpLength2);
                    goto __TryToDecompress;             
                }

                // Compare the output
                if((nDiff = GetFirstDiffer(pbDecompressed1, pbDecompressed2, nDcmpLength1)) != -1)
                {
                    printf("Difference in decompressed blocks (offset 0x%08lX)\n", nDiff);
                    goto __TryToDecompress;
                }

                // Check for data overflow
                if(pbDecompressed1[nSectorSize] != 0xFD || pbDecompressed1[nSectorSize] != 0xFD)
                {
                    printf("Damage after decompressed sector !!!\n");
                    goto __TryToDecompress;
                }

                // Compare the decompressed data against original data
                if((nDiff = GetFirstDiffer(pbDecompressed1, pbOriginalData, nDcmpLength1)) != -1)
                {
                    printf("Difference between original data and decompressed data (offset 0x%08lX)\n", nDiff);
                    goto __TryToDecompress;
                }
            }
        }
    }

    // Cleanup
    if(pbOriginalData != NULL)
        delete [] pbOriginalData;
    if(pbCompressed2 != NULL)
        delete [] pbCompressed2;
    if(pbCompressed1 != NULL)
        delete [] pbCompressed1;
    if(pbDecompressed2 != NULL)
        delete [] pbDecompressed2;
    if(pbDecompressed1 != NULL)
        delete [] pbDecompressed1;
    clreol();
    return nError;
}

static int TestSectorCompress(int nSectorSize)
{
    LPBYTE pbDecompressed = NULL;
    LPBYTE pbCompressed = NULL;
    LPBYTE pbOriginal = NULL;
    int nError = ERROR_SUCCESS;

    // Allocate buffers
    // Must allocate twice blocks due to probable bug in Storm.dll.
    // Storm.dll corrupts stack when uncompresses data with PKWARE DCL
    // and no compression occurs.
    pbDecompressed = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, nSectorSize);
    pbCompressed = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, nSectorSize);
    pbOriginal = (LPBYTE)HeapAlloc(GetProcessHeap(), 0, nSectorSize);
    if(!pbDecompressed || !pbCompressed || !pbOriginal)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    if(nError == ERROR_SUCCESS)
    {
        for(int i = 0; i < 100000; i++)
        {
            int nOriginalLength = nSectorSize % (rand() + 1);
            int nCompressedLength;
            int nDecompressedLength;
            int nCmp = MPQ_COMPRESSION_SPARSE | MPQ_COMPRESSION_ZLIB | MPQ_COMPRESSION_BZIP2 | MPQ_COMPRESSION_PKWARE;
            int nDiff;

            clreol();
            printf("Testing compression of sector %u\r", i + 1);

            // Generate random data sector
            GenerateRandomDataBlock(pbOriginal, nOriginalLength);
            if(nOriginalLength == 0x123)
                nOriginalLength = 0;

__TryAgain:

            // Compress the sector
            nCompressedLength = nOriginalLength;
            SCompCompress((char *)pbCompressed, &nCompressedLength, (char *)pbOriginal, nOriginalLength, nCmp, 0, -1);
//          SCompImplode((char *)pbCompressed, &nCompressedLength, (char *)pbOriginal, nOriginalLength);

            // When the method was unable to compress data,
            // the compressed data must be identical to original data
            if(nCompressedLength == nOriginalLength)
            {
                if((nDiff = GetFirstDiffer(pbCompressed, pbOriginal, nOriginalLength)) != -1)
                {
                    printf("Compression error: Fail when unable to compress the data (Offset 0x%08lX).\n", nDiff);
                    goto __TryAgain;
                }
            }

            // Uncompress the sector
            nDecompressedLength = nOriginalLength;
            SCompDecompress((char *)pbDecompressed, &nDecompressedLength, (char *)pbCompressed, nCompressedLength);
//          SCompExplode((char *)pbDecompressed, &nDecompressedLength, (char *)pbCompressed, nCompressedLength);

            // Check the decompressed length against original length
            if(nDecompressedLength != nOriginalLength)
            {
                printf("Length of uncompressed data does not agree with original data length !!!\n");
                goto __TryAgain;
            }
            
            // Check decompressed block against original block
            if((nDiff = GetFirstDiffer(pbDecompressed, pbOriginal, nOriginalLength)) != -1)
            {
                printf("Decompressed sector does not agree with the original data !!! (Offset 0x%08lX)\n", nDiff);
                goto __TryAgain;
            }
        }
    }

    // Cleanup
    if(pbOriginal != NULL)
        HeapFree(GetProcessHeap(), 0, pbOriginal);
    if(pbCompressed != NULL)
        HeapFree(GetProcessHeap(), 0, pbCompressed);
    if(pbDecompressed != NULL)
        HeapFree(GetProcessHeap(), 0, pbDecompressed);
    clreol();
    return nError;
}

static int TestArchiveOpenAndClose(const char * szMpqName)
{
    const char * szFileName1 = LISTFILE_NAME;
    const char * szFileName2 = "E:\\(listfile)";
    HANDLE hFile1 = NULL;
    HANDLE hFile2 = NULL;
    HANDLE hMpq = NULL;
    int nError = ERROR_SUCCESS;

    if(nError == ERROR_SUCCESS)
    {
        printf("Opening archive %s ...\n", szMpqName);
        if(!SFileOpenArchive(szMpqName, 0, MPQ_OPEN_CHECK_SECTOR_CRC, &hMpq))
            nError = GetLastError();
    }

    // Open listfile from the MPQ
    if(nError == ERROR_SUCCESS)
    {
        if(!SFileOpenFileEx(hMpq, szFileName1, SFILE_OPEN_FROM_MPQ, &hFile1))
        {
            nError = GetLastError();
            printf("%s - file integrity error\n", szFileName1);
        }
    }

    // Open local listfile
    if(nError == ERROR_SUCCESS)
    {
        if(!SFileOpenFileEx(hMpq, szFileName2, SFILE_OPEN_LOCAL_FILE, &hFile2))
        {
            nError = GetLastError();
            printf("%s - file integrity error\n", szFileName2);
        }
    }

    // Verify the MPQ listfile
    if(nError == ERROR_SUCCESS)
    {
        SFileVerifyFile(hMpq, szFileName1, 0xFFFFFFFF); 
        if(!CompareArchivedFilesRR(szFileName1, hFile1, hFile2, 0x100000))
            nError = ERROR_READ_FAULT;
    }

    if(hFile2 != NULL)
        SFileCloseFile(hFile2);
    if(hFile1 != NULL)
        SFileCloseFile(hFile1);
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

static int TestFindFiles(const char * szMpqName)
{
    TMPQFile * hf;
    HANDLE hFile;
    HANDLE hMpq = NULL;
    BYTE Buffer[100];
    int nError = ERROR_SUCCESS;
    int nFiles = 0;
    int nFound = 0;

    // Open the archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Opening \"%s\" for finding files ...\n", szMpqName);
        if(!SFileOpenArchive(szMpqName, MPQ_OPEN_EXISTING, 0, &hMpq))
            nError = GetLastError();
    }

    // Compact the archive
    if(nError == ERROR_SUCCESS)
    {
        SFILE_FIND_DATA sf;
        HANDLE hFind;
        DWORD dwExtraDataSize;
        BOOL bFound = TRUE;

        hFind = SFileFindFirstFile(hMpq, "*", &sf, "c:\\Tools32\\ListFiles\\ListFile.txt");
        while(hFind != NULL && bFound != FALSE)
        {
            if(SFileOpenFileEx(hMpq, sf.cFileName, 0, &hFile))
            {
                hf = (TMPQFile *)hFile;
                SFileReadFile(hFile, Buffer, sizeof(Buffer));
                nFiles++;

                if(sf.dwFileFlags & MPQ_FILE_SECTOR_CRC)
                {
                    dwExtraDataSize = hf->SectorOffsets[hf->dwSectorCount - 1] - hf->SectorOffsets[hf->dwSectorCount - 2];
                    if(dwExtraDataSize != 0)
                        nFound++;
                }

                SFileCloseFile(hFile);
            }

            bFound = SFileFindNextFile(hFind, &sf);
        }
    }

    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    if(nError == ERROR_SUCCESS)
        printf("Search complete\n");
    return nError;
}

static int TestMpqCompacting(const char * szMpqName)
{
    HANDLE hMpq = NULL;
    int nError = ERROR_SUCCESS;

    // Open the archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Opening \"%s\" for compacting ...\n", szMpqName);
        if(!SFileOpenArchive(szMpqName, MPQ_OPEN_EXISTING, 0, &hMpq))
            nError = GetLastError();
    }
/*
    if(nError == ERROR_SUCCESS)
    {
        char * szFileName = "Campaign\\Human\\Human01.pud";

        printf("Deleting file %s ...\r", szFileName);
        if(!SFileRemoveFile(hMpq, szFileName))
            nError = GetLastError();
    }
*/
    // Compact the archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Compacting archive ...\r");
        SFileSetCompactCallback(hMpq, CompactCB, NULL);
        if(!SFileCompactArchive(hMpq, NULL))
            nError = GetLastError();
    }

    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    if(nError == ERROR_SUCCESS)
        printf("Compacting complete (No errors)\n");
    return nError;
}


static int TestCreateArchiveV2(const char * szMpqName)
{
    HANDLE hMpq = NULL;                 // Handle of created archive 
    HANDLE hFile = INVALID_HANDLE_VALUE;
    LPTSTR szFileName1 = "C:\\Tools32\\FileTest.exe";
    LPTSTR szFileName2 = "E:\\ZeroSize.txt";
    char szMpqFileName[MAX_PATH];
    int nError = ERROR_SUCCESS;

    // Create the new file
    hFile = CreateFile(szMpqName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
        nError = GetLastError();

    // Write some data
    if(nError == ERROR_SUCCESS)
    {
        SetFilePointer(hFile, 0x100000, NULL, FILE_BEGIN);
        if(!SetEndOfFile(hFile))
            nError = GetLastError();
        CloseHandle(hFile);
    }

    // Well, now create the MPQ archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Creating %s ...\n", szMpqName);
        SFileSetLocale(LANG_CZECH);
        if(!SFileCreateArchive(szMpqName,
                               MPQ_CREATE_ARCHIVE_V2 | MPQ_CREATE_ATTRIBUTES | MPQ_OPEN_ALWAYS,
                               0x40,
                              &hMpq))
        {
            nError = GetLastError();
        }
    }

    // Add the same file multiple times
    if(nError == ERROR_SUCCESS)
    {
        SFileCompactArchive(hMpq);
        SFileSetLocale(LANG_NEUTRAL);

        for(int i = 0; AddFlags[i] != 0xFFFFFFFF; i++)
        {
            sprintf(szMpqFileName, "FileTest_%02u.exe", i);
            if(!SFileAddFileEx(hMpq, szFileName1, szMpqFileName, AddFlags[i], MPQ_COMPRESSION_ZLIB))
                printf("Failed to add the file \"%s\".\n", szMpqFileName);

            if(SFileVerifyFile(hMpq, szMpqFileName, MPQ_ATTRIBUTE_CRC32 | MPQ_ATTRIBUTE_MD5))
                printf("CRC error on \"%s\"\n", szMpqFileName);
        }

        SFileSetLocale(0);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_1.txt", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

        SFileSetLocale(0x0405);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_1.txt", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

        SFileSetLocale(0x0407);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_1.txt", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

        SFileSetLocale(0x0408);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_1.txt", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

//      SFileSetLocale(0x0409);
//      if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_1.txt", MPQ_FILE_COMPRESS | MPQ_FILE_ENCRYPTED, MPQ_COMPRESSION_ZLIB))
//          printf("Cannot add the file\n");

        SFileSetLocale(0);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_2.txt", MPQ_FILE_SINGLE_UNIT | MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

        SFileSetLocale(0x0405);
        if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_2.txt", MPQ_FILE_SINGLE_UNIT | MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB))
            printf("Cannot add the file\n");

//      SFileSetLocale(0x0406);
//      if(!SFileAddFileEx(hMpq, szFileName2, "ZeroSize_2.txt", MPQ_FILE_SINGLE_UNIT | MPQ_FILE_COMPRESS, MPQ_COMPRESSION_ZLIB))
//          printf("Cannot add the file\n");
    }

    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

static int TestFileReadAndWrite(
    const char * szMpqName,
    const char * szFileName)
{
    PVOID pvFile = NULL;
    HANDLE hFile = NULL;
    HANDLE hMpq = NULL;
    DWORD dwBytesRead;
    DWORD dwFileSize;
    int nError = ERROR_SUCCESS;

    if(!SFileOpenArchive(szMpqName, MPQ_OPEN_EXISTING, 0, &hMpq))
    {
        nError = GetLastError();
        printf("Failed to open the archive %s (%u).\n", szMpqName, nError);
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileOpenFileEx(hMpq, szFileName, 0, &hFile))
        {
            nError = GetLastError();
            printf("Failed to open the file %s (%u).\n", szFileName, nError);
        }
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileGetFileInfo(hFile, SFILE_INFO_FILE_SIZE, &dwFileSize, sizeof(DWORD)))
        {
            nError = GetLastError();
            printf("Failed to get the file size (%u).\n", nError);
        }
    }

    if(nError == ERROR_SUCCESS)
    {
        pvFile = new BYTE[dwFileSize];
        if(pvFile == NULL)
        {
            nError = ERROR_NOT_ENOUGH_MEMORY;
            printf("Failed to allocate buffer for the file (%u).\n", nError);
        }
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileReadFile(hFile, pvFile, dwFileSize, &dwBytesRead))
        {
            nError = GetLastError();
            printf("Failed to read file (%u).\n", nError);
        }
    }

    if(hFile != NULL)
    {
        SFileCloseFile(hFile);
        hFile = NULL;
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileCreateFile(hMpq, szFileName, NULL, dwFileSize, 0, MPQ_FILE_REPLACEEXISTING, &hFile))
        {
            nError = GetLastError();
            printf("Failed to create %s in the archive (%u).\n", szFileName, nError);
        }
    }

    if(nError == ERROR_SUCCESS)
    {
        if(!SFileWriteFile(hFile, pvFile, dwFileSize, 0))
        {
            nError = GetLastError();
            printf("Failed to write the data to the MPQ (%u).\n", nError);
        }
    }

    if(hFile != NULL)
    {
        if(!SFileFinishFile(hFile))
        {
            nError = GetLastError();
            printf("Failed to finalize file creation (%u).\n", nError);
        }
    }

    if(pvFile != NULL)
        delete [] pvFile;
    if(hMpq != NULL)
        SFileCloseArchive(hMpq);
    return nError;
}

static int TestSignatureVerify(const char * szMpqName)
{
    HANDLE hMpq;

    if(SFileOpenArchive(szMpqName, 0, 0, &hMpq))
    {
        SFileVerifyArchive(hMpq);
        SFileCloseArchive(hMpq);
    }

    return 0;
}


static int TestCreateArchiveCopy(const char * szMpqName, const char * szMpqCopyName, const char * szListFile)
{
    char   szLocalFile[MAX_PATH] = "";
    HANDLE hMpq1 = NULL;                // Handle of existing archive
    HANDLE hMpq2 = NULL;                // Handle of created archive 
    HANDLE hFile = INVALID_HANDLE_VALUE;
    DWORD dwHashTableSize = 0;
    int nError = ERROR_SUCCESS;

    // If no listfile or an empty one, use NULL
    if(szListFile == NULL || *szListFile == 0)
        szListFile = NULL;

    // Create the new file
    hFile = CreateFile(szMpqCopyName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if(hFile == INVALID_HANDLE_VALUE)
        nError = GetLastError();

    // Write some data
    if(nError == ERROR_SUCCESS)
    {
        SetFilePointer(hFile, 0x100000, NULL, FILE_BEGIN);
        if(!SetEndOfFile(hFile))
            nError = GetLastError();
        CloseHandle(hFile);
    }

    // Open the existing MPQ archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Opening %s ...\n", szMpqName);
        if(!SFileOpenArchive(szMpqName, 0, 0, &hMpq1))
            nError = GetLastError();
    }

    // Well, now create the MPQ archive
    if(nError == ERROR_SUCCESS)
    {
        printf("Creating %s ...\n", szMpqCopyName);
        SFileGetFileInfo(hMpq1, SFILE_INFO_HASH_TABLE_SIZE, &dwHashTableSize, 4);
        if(!SFileCreateArchive(szMpqCopyName, 0, dwHashTableSize, &hMpq2))
            nError = GetLastError();
    }

    // Copy all files from one archive to another
    if(nError == ERROR_SUCCESS)
    {
        SFILE_FIND_DATA sf;
        HANDLE hFind = SFileFindFirstFile(hMpq1, "*", &sf, szListFile);
        BOOL bResult = TRUE;

        printf("Adding files\n");

        if(hFind != NULL)
        {
            while(bResult)
            {
                if(strcmp(sf.cFileName, LISTFILE_NAME) && strcmp(sf.cFileName, ATTRIBUTES_NAME))
                {
                    ShowProcessedFile(sf.cFileName);
                    SFileSetLocale(sf.lcLocale);

                    // Create the local file name
                    sprintf(szLocalFile, "%s\\%s", szWorkDir, GetPlainName(sf.cFileName));
                    if(SFileExtractFile(hMpq1, sf.cFileName, szLocalFile))
                    {
                        sf.dwFileFlags &= ~MPQ_FILE_SECTOR_CRC;
                        sf.dwFileFlags &= ~MPQ_FILE_EXISTS;

                        if(!SFileAddFile(hMpq2, szLocalFile, sf.cFileName, sf.dwFileFlags))
                        {
                            nError = GetLastError();
                            printf("Failed to add the file %s into archive\n", sf.cFileName);
                            DeleteFile(szLocalFile);
                            break;
                        }
                    }
                    else
                    {
                        nError = GetLastError();
                        printf("Failed to extract %s\n", sf.cFileName);
                    }

                    // Delete the added file
                    DeleteFile(szLocalFile);
                }

                // Find the next file
                bResult = SFileFindNextFile(hFind, &sf);
            }

            // Close the search handle
            SFileFindClose(hFind);
        }
    }

    // Test rename function
    if(nError == ERROR_SUCCESS)
    {
        SFileSetLocale(LANG_NEUTRAL);
        if(!SFileRenameFile(hMpq2, "FileTest_08.exe", "FileTest_08a.exe"))
        {
            nError = GetLastError();
            printf("Failed to rename the file\n");
        }

        if(!SFileRenameFile(hMpq2, "FileTest_08a.exe", "FileTest_08.exe"))
        {
            nError = GetLastError();
            printf("Failed to rename the file\n");
        }

        if(!SFileRenameFile(hMpq2, "FileTest_10.exe", "FileTest_10a.exe"))
        {
            nError = GetLastError();
            printf("Failed to rename the file\n");
        }

        if(!SFileRenameFile(hMpq2, "FileTest_10a.exe", "FileTest_10.exe"))
        {
            nError = GetLastError();
            printf("Failed to rename the file\n");
        }
    }

    // Close both archives
    if(hMpq2 != NULL)
        SFileCloseArchive(hMpq2);
    if(hMpq1 != NULL)
        SFileCloseArchive(hMpq1);
    if(nError == ERROR_SUCCESS)
        printf("MPQ creating complete (No errors)\n");
    return nError;
}

static int TestCompareTwoArchives(
    const char * szMpqName1,
    const char * szMpqName2,
    const char * szListFile,
    DWORD dwBlockSize)
{
    TMPQArchive * ha1 = NULL;
    TMPQArchive * ha2 = NULL;
    LPBYTE pbBuffer1 = NULL;
    LPBYTE pbBuffer2 = NULL;
    HANDLE hMpq1 = NULL;                // Handle of the first archive
    HANDLE hMpq2 = NULL;                // Handle of the second archive
    HANDLE hFile1 = NULL;
    HANDLE hFile2 = NULL;
    int nError = ERROR_SUCCESS;

    // If no listfile or an empty one, use NULL
    if(szListFile == NULL || *szListFile == 0)
        szListFile = NULL;

    // Allocate both buffers
    pbBuffer1 = (LPBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBlockSize);
    pbBuffer2 = (LPBYTE)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwBlockSize);
    if(pbBuffer1 == NULL || pbBuffer2 == NULL)
        nError = ERROR_NOT_ENOUGH_MEMORY;

    printf("=============== Comparing MPQ archives ===============\n");

    // Open the first MPQ archive
    if(nError == ERROR_SUCCESS && szMpqName1 != NULL)
    {
        printf("Opening %s ...\n", szMpqName1);
        if(!SFileOpenArchive(szMpqName1, 0, 0, &hMpq1))
            nError = GetLastError();
        ha1 = (TMPQArchive *)hMpq1;
    }

    // Open the second MPQ archive
    if(nError == ERROR_SUCCESS && szMpqName2 != NULL)
    {
        printf("Opening %s ...\n", szMpqName2);
        if(!SFileOpenArchive(szMpqName2, 0, 0, &hMpq2))
            nError = GetLastError();
        ha2 = (TMPQArchive *)hMpq2;
    }

    // Compare the header
    if(nError == ERROR_SUCCESS && (ha1 != NULL && ha2 != NULL))
    {
        if(ha1->pHeader->dwHeaderSize != ha2->pHeader->dwHeaderSize)
            printf(" - Header size is different\n");
        if(ha1->pHeader->wFormatVersion != ha2->pHeader->wFormatVersion)
            printf(" - Format version is different\n");
        if(ha1->pHeader->wSectorSize != ha2->pHeader->wSectorSize)
            printf(" - Block size Format version is different\n");
        if(ha1->pHeader->dwHashTableSize != ha2->pHeader->dwHashTableSize)
            printf(" - Hash table size is different\n");
        if(ha1->pHeader->dwBlockTableSize != ha2->pHeader->dwBlockTableSize)
            printf(" - Block table size is different\n");
    }

    // Find all files in the first archive and compare them
    if(nError == ERROR_SUCCESS)
    {
        SFILE_FIND_DATA sf;
        HANDLE hFind = SFileFindFirstFile(hMpq1, "*", &sf, szListFile);
        DWORD dwSearchScope1 = SFILE_OPEN_FROM_MPQ;
        DWORD dwSearchScope2 = SFILE_OPEN_FROM_MPQ;
        BOOL bResult = TRUE;

        if(hMpq1 == NULL)
            dwSearchScope1 = SFILE_OPEN_LOCAL_FILE;
        if(hMpq2 == NULL)
            dwSearchScope2 = SFILE_OPEN_LOCAL_FILE;

        while(hFind != NULL && bResult == TRUE)
        {
            ShowProcessedFile(sf.cFileName);
            SFileSetLocale(sf.lcLocale);

            // Open the first file
            if(!SFileOpenFileEx(hMpq1, sf.cFileName, dwSearchScope1, &hFile1))
            {
                printf("Failed to open the file %s in the first archive\n", sf.cFileName);
                continue;
            }

            if(!SFileOpenFileEx(hMpq2, sf.cFileName, dwSearchScope2, &hFile2))
            {
                printf("Failed to open the file %s in the second archive\n", sf.cFileName);
                continue;
            }

            if(dwSearchScope1 == SFILE_OPEN_FROM_MPQ && dwSearchScope2 == SFILE_OPEN_FROM_MPQ)
            {
                TMPQFile * hf1 = (TMPQFile *)hFile1;
                TMPQFile * hf2 = (TMPQFile *)hFile2;

                // Compare the file sizes
                if(hf1->pBlock->dwFSize != hf2->pBlock->dwFSize)
                    printf(" - %s different size (%u x %u)\n", sf.cFileName, hf1->pBlock->dwFSize, hf2->pBlock->dwFSize);

                if(hf1->pBlock->dwFlags != hf2->pBlock->dwFlags)
                    printf(" - %s different flags (%08lX x %08lX)\n", sf.cFileName, hf1->pBlock->dwFlags, hf2->pBlock->dwFlags);
            }

            if(!CompareArchivedFiles(sf.cFileName, hFile1, hFile2, 0x1001))
                printf(" - %s different content\n", sf.cFileName);

            if(!CompareArchivedFilesRR(sf.cFileName, hFile1, hFile2, 0x100000))
                printf(" - %s different content\n", sf.cFileName);

            // Close both files
            SFileCloseFile(hFile2);
            SFileCloseFile(hFile1);
            hFile2 = hFile1 = NULL;

            // Find the next file
            bResult = SFileFindNextFile(hFind, &sf);
        }

        // Close all handles
        if(hFile2 != NULL)
            SFileCloseFile(hFile2);
        if(hFile1 != NULL)
            SFileCloseFile(hFile1);
        if(hFind != NULL)
            SFileFindClose(hFind);
    }

    // Close both archives
    clreol();
    printf("================ MPQ compare complete ================\n");
    if(hMpq2 != NULL)
        SFileCloseArchive(hMpq2);
    if(hMpq1 != NULL)
        SFileCloseArchive(hMpq1);
    if(pbBuffer2 != NULL)
        HeapFree(GetProcessHeap(), 0, pbBuffer2);
    if(pbBuffer1 != NULL)
        HeapFree(GetProcessHeap(), 0, pbBuffer1);
    return nError;
}


//-----------------------------------------------------------------------------
// Main
// 
// The program must be run with two command line arguments
//
// Arg1 - The source MPQ name (for testing reading and file find)
// Arg2 - Listfile name
//

void main(void)
{
    DWORD dwHashTableSize = 0x8000;     // Size of the hash table.
    int nError = ERROR_SUCCESS;

    UNREFERENCED_PARAMETER(dwHashTableSize);

    // Mix the random number generator
    srand(GetTickCount());

    // Set the lowest priority to allow running in the background
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

    // Test structure sizes
//  if(nError == ERROR_SUCCESS)
//      nError = TestStructureSizes();

    // Test reading partial file
//  if(nError == ERROR_SUCCESS)
//      nError = TestPartFileRead("e:\\Multimedia\\MPQs\\PartialMPQs\\patch.MPQ.part");

    // Test "sparse" compression method against the code ripped from Starcraft II
//  if(nError == ERROR_SUCCESS)
//      nError = CompareLzmaCompressions(MPQ_SECTOR_SIZE);
//      nError = CompareSparseCompressions(MPQ_SECTOR_SIZE);

    // Test compression methods
//  if(nError == ERROR_SUCCESS)
//      nError = TestSectorCompress(MPQ_SECTOR_SIZE);

    // Test the archive open and close
//  if(nError == ERROR_SUCCESS)
//      nError = TestArchiveOpenAndClose("e:\\Multimedia\\MPQs\\Warcraft III\\War3x_orig.mpq");
                                                                             
//  if(nError == ERROR_SUCCESS)
//      nError = TestFindFiles("e:\\Hry\\Warcraft III\\Movies.renamed\\HumanEd.mpq");

    // Create a big MPQ archive
//  if(nError == ERROR_SUCCESS)
//      nError = TestCreateArchiveV2("E:\\Multimedia\\MPQs\\Test.mpq");

//  if(nError == ERROR_SUCCESS)
//      nError = TestFileReadAndWrite("e:\\Multimedia\\MPQs\\Warcraft III\\(10)DustwallowKeys.w3m", "war3map.j");

    // Verify the archive signature
//  if(nError == ERROR_SUCCESS)
//      nError = TestSignatureVerify("e:\\Hry\\World of Warcraft\\WoW-3.2.0.10192-to-3.3.0.10958-enGB-patch.exe");
//      nError = TestSignatureVerify("e:\\Multimedia\\MPQs\\Warcraft III\\(10)DustwallowKeys.w3m");
//      nError = TestSignatureVerify("e:\\Multimedia\\MPQs\\Warcraft III\\War3TFT_121b_English.exe");
//      nError = TestSignatureVerify("e:\\Multimedia\\MPQs\\World of Warcraft\\standalone.MPQ");

    // Compact the archive        
//  if(nError == ERROR_SUCCESS)
//      nError = TestMpqCompacting("e:\\Multimedia\\MPQs\\Test.mpq");

    // Create copy of the archive, appending some bytes before the MPQ header
    if(nError == ERROR_SUCCESS)
        nError = TestCreateArchiveCopy("e:\\Multimedia\\MPQs\\PartialMPQs\\interface.MPQ.part", "e:\\Multimedia\\MPQs\\PartialMPQs\\interface-copy.MPQ.part", NULL);

//  if(nError == ERROR_SUCCESS)
//  {
//      nError = TestCompareTwoArchives("e:\\Multimedia\\MPQs\\Warcraft III\\War3x.mpq",
//                                      NULL,
//                                      NULL,
//                                      0x1001);
//  }

    // Remove the working directory
    clreol();
    if(nError != ERROR_SUCCESS)
        printf("One or more errors occurred when testing StormLib\n");
    printf("Work complete. Press any key to exit ...\n");
    _getch();
}
