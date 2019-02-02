/*
 * Authors: Lalit D. Chandwani, Aastha Shrivastava, Rakshit Sundriyal and Ujwal Singhania
 * Description: Unzip library
 * Last Modified Date: 14 March 2018
*/
#ifndef FILEUNZIP_H
#define FILEUNZIP_H

#include <stdint.h>
typedef struct zipFile zipFile;

struct zipFile{
    size_t (*read)(zipFile *file, void *buf, size_t size);
    size_t (*tell)(zipFile *file);
    int (*seek)(zipFile *file, size_t offset, int whence);
    int (*error)(zipFile *file);
    void (*close)(zipFile *file);
};

typedef struct __attribute__ ((__packed__)){
    uint32_t signature;
    uint16_t versionNeededToExtract; //Not supported
    uint16_t generalPurposeBitFlag; //Not supported
    uint16_t compressionMethod;
    uint16_t lastModFileTime;
    uint16_t lastModFileDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength; //Not supported
} localFileHeader;

typedef struct __attribute__ ((__packed__)){
    uint32_t signature;
    uint16_t versionMadeBy; //Not supported
    uint16_t versionNeededToExtract; //Not supported
    uint16_t generalPurposeBitFlag; //Not supported
    uint16_t compressionMethod;
    uint16_t lastModFileTime;
    uint16_t lastModFileDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength; //Not supported
    uint16_t fileCommentLength; //Not supported
    uint16_t diskNumberStart; //Not supported
    uint16_t internalFileAttributes; //Not supported
    uint32_t externalFileAttributes; //Not supported
    uint32_t relativeOffsetOflocalHeader;
} centralDirectoryFileHeader;

typedef struct __attribute__ ((__packed__)){
    uint32_t signature; //0x0605504b
    uint16_t diskNumber; //Not supported
    uint16_t centralDirectoryDiskNumber; //Not supported
    uint16_t numEntriesThisDisk; //Not supported
    uint16_t numEntries;
    uint32_t centralDirectorySize;
    uint32_t centralDirectoryOffset;
    uint16_t zipCommentLength;
    //Followed by .ZIP file comment (variable size)
} endCentralDirectoryRecord;

typedef struct __attribute__ ((__packed__)){
    uint16_t compressionMethod;
    uint16_t lastModFileTime;
    uint16_t lastModFileDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint32_t offset;
} fileHeader;

#define BUFFER_SIZE 65536

zipFile * zipFileFromStdioFile(FILE *fp);

typedef int (*zipRecordCallback)(zipFile *zip, int index, fileHeader *header, char* fileName, void *userData);

//Read End of Central Directory Record.
int readEndCentralDirectoryRecord(zipFile *zip, endCentralDirectoryRecord *endRecord);
//Read Central Directory.
int readCentralDirectory(zipFile *zip, endCentralDirectoryRecord *endRecord, zipRecordCallback callback, void *userData);
//Read Local File Header(s).
int readLocalFileHeader(zipFile *zip, fileHeader *header, char *fileName, int len);
//Read data from the file(s), which are described in the header to send to buffer.
int readData(zipFile *zip, fileHeader *header, void *buffer);

#endif
