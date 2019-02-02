/*
 * Authors: Lalit D. Chandwani, Aastha Shrivastava, Rakshit Sundriyal and Ujwal Singhania
 * Description: Unzip library
 * Last Modified Date: 14 March 2018
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "fileunzip.h"

unsigned char zipBuffer[BUFFER_SIZE]; //Used to limit max zip descriptor size

int readEndCentralDirectoryRecord(zipFile *zip, endCentralDirectoryRecord *endRecord){
  long fileSize, readBytes, index;
  endCentralDirectoryRecord *er;

  if(zip->seek(zip, 0, SEEK_END)){
    fprintf(stderr, "Could not go to the end of zip file.");
    return Z_ERRNO;
  }

  if((fileSize = zip->tell(zip)) <= sizeof(endCentralDirectoryRecord)){
    fprintf(stderr, "This zip is empty, nothing to unzip!");
    return Z_ERRNO;
  }
  printf("%li\n", sizeof(endCentralDirectoryRecord));
  readBytes = (fileSize < sizeof(zipBuffer)) ? fileSize : sizeof(zipBuffer);

  if(zip->seek(zip, fileSize - readBytes, SEEK_SET)){
    fprintf(stderr, "Cannot seek to location in zip file.");
    return Z_ERRNO;
  }

  if(zip->read(zip, zipBuffer, readBytes) < readBytes){
    fprintf(stderr, "Could not read till end of zip file.");
    return Z_ERRNO;
  }

  //Assume that signature can only be found at one address = 0x06054B50
  for(index = readBytes - sizeof(endCentralDirectoryRecord); index >= 0; index--){
    er = (endCentralDirectoryRecord *)(zipBuffer + index);
    //printf("%x\n", er->signature);
    if(er->signature == 0x06054B50){
      break;
    }
  }

  if(index < 0){
    fprintf(stderr, "End record signature not found in the zip.\n");
    return Z_ERRNO;
  }

  memcpy(endRecord, er, sizeof(endCentralDirectoryRecord));

  if(endRecord->diskNumber || endRecord->centralDirectoryDiskNumber ||
      endRecord->numEntries != endRecord->numEntriesThisDisk) {
      fprintf(stderr, "Multifile zips not supported. We will be creating this functionality soon.");
      return Z_ERRNO;
  }
  return Z_OK;
}

int readCentralDirectory(zipFile *zip, endCentralDirectoryRecord *endRecord, zipRecordCallback callback, void *userData){
  centralDirectoryFileHeader centralFileHeader;
  fileHeader header;
  int index;

  if(zip->seek(zip, endRecord->centralDirectoryOffset, SEEK_SET)){
    fprintf(stderr, "Cannot seek to destination in the zip file.\n");
    return Z_ERRNO;
  }

  for(index = 0; index < endRecord->numEntries; index = index + 1){
    if(zip->read(zip, &centralFileHeader, sizeof(centralDirectoryFileHeader)) < sizeof(centralDirectoryFileHeader)){
      fprintf(stderr, "Could not read file header %d.", index);
      return Z_ERRNO;
    }

    if(centralFileHeader.signature != 0x02014B50){
      fprintf(stderr, "File header signature is invalid, %d.", index);
      return Z_ERRNO;
    }

    if(centralFileHeader.fileNameLength + 1 >= BUFFER_SIZE){
      fprintf(stderr, "File name is too long, %d.", index);
      return Z_ERRNO;
    }

    if(zip->read(zip, zipBuffer, centralFileHeader.fileNameLength) < centralFileHeader.fileNameLength){
      fprintf(stderr, "Could not read file name, %d.", index);
      return Z_ERRNO;
    }

    zipBuffer[centralFileHeader.fileNameLength] = '\0'; //Terminate string with NULL character.

    if(zip->seek(zip, centralFileHeader.extraFieldLength, SEEK_CUR) || zip->seek(zip, centralFileHeader.fileCommentLength, SEEK_CUR)){
      fprintf(stderr, "Could not skip the extra field or file comment, %d.", index);
      return Z_ERRNO;
    }

    //Create fileHeader from centralDirectoryFileHeader
    memcpy(&header, &centralFileHeader.compressionMethod, sizeof(header));
    header.offset = centralFileHeader.relativeOffsetOflocalHeader;

    if(!callback(zip, index, &header, (char *)zipBuffer, userData)){
      break;
    }
  }
  return Z_OK;
}

int readLocalFileHeader(zipFile *zip, fileHeader *header, char *fileName, int length){
  localFileHeader localHeader;

  if(zip->read(zip, &localHeader, sizeof(localFileHeader)) < sizeof(localFileHeader)){
    return Z_ERRNO;
  }

  if(length){
    if(localHeader.fileNameLength >= length){
      return Z_ERRNO;
    }

    if(zip->read(zip, fileName, localHeader.fileNameLength) < localHeader.fileNameLength){
      return Z_ERRNO;
    }

    fileName[localHeader.fileNameLength] = '\0'; //Terminating with NULL
  }
  else{
    if(zip->seek(zip, localHeader.fileNameLength, SEEK_CUR)){
      return Z_ERRNO;
    }
  }

  if(localHeader.extraFieldLength){
    if(zip->seek(zip, localHeader.extraFieldLength, SEEK_CUR)){
      return Z_ERRNO;
    }
  }

  //CHECKING if compression method is 0 AND compressed size and uncompressed size
  if(localHeader.compressionMethod == 0 && (localHeader.compressedSize != localHeader.uncompressedSize)){
    return Z_ERRNO;
  }

  memcpy(header, &localHeader.compressionMethod, sizeof(fileHeader));
  header->offset = 0; //Pointing to actual place, therefore we are not setting value locally.

  return Z_OK;
}

//Read data from zip, exactly the header to send to buffer
int readData(zipFile *zip, fileHeader *header, void *buffer){
  unsigned char *bytes = (unsigned char *)buffer;
  long compressedLeft, uncompressedLeft;
  z_stream strm;
  int result;

  if(header->compressionMethod == 0){
    //This method is store, no compression, so we just read it right away.
    if(zip->read(zip, buffer, header->uncompressedSize) < header->uncompressedSize || zip->error(zip)){
      return Z_ERRNO;
    }
  }
  else if(header->compressionMethod == 8){
    //This method is the deflate method, here we use zlib.
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    //Using inflateInit2 with negative window bits to indicate the raw data
    if((result = inflateInit2(&strm, -MAX_WBITS)) != Z_OK){
        return result; // Zlib errors are negative
    }

    //Inflating compressed data.
    for(compressedLeft = header->compressedSize, uncompressedLeft = header->uncompressedSize; compressedLeft && uncompressedLeft && result != Z_STREAM_END; compressedLeft = compressedLeft - strm.avail_in){
      strm.avail_in = zip->read(zip, zipBuffer, (sizeof(zipBuffer) < compressedLeft) ? sizeof(zipBuffer) : compressedLeft);

      if(strm.avail_in == 0 || zip->error(zip)){
        inflateEnd(&strm);
        return Z_ERRNO;
      }

      strm.next_in = zipBuffer;
      strm.avail_out = uncompressedLeft;
      strm.next_out = bytes;

      compressedLeft = compressedLeft - strm.avail_in;
      result = inflate(&strm, Z_NO_FLUSH);
      if(result == Z_STREAM_ERROR){
        return result;
      }

      switch(result){
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
          (void) inflateEnd(&strm);
          return result;
        case Z_NEED_DICT:
          result = Z_DATA_ERROR;
      }

      //Calculate bytes umcompressed.
      bytes = bytes + uncompressedLeft - strm.avail_out;
      uncompressedLeft = strm.avail_out;
    }
    inflateEnd(&strm);
  }
  else{
    return Z_ERRNO;
  }
  return Z_OK;
}

typedef struct{
  zipFile handle;
  FILE *fp;
}STDIOZIPFILE;

static size_t stdio_file_handle_read(zipFile *file, void *ptr, size_t size){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)file;
  return fread(ptr, 1, size, handle->fp);
}

static size_t stdio_file_handle_tell(zipFile *file){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)file;
  return ftell(handle->fp);
}

static int stdio_file_handle_seek(zipFile *file, size_t offset, int whence){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)file;
  return fseek(handle->fp, offset, whence);
}

static int stdio_file_handle_error(zipFile *file){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)file;
  return ferror(handle->fp);
}

static void stdio_file_handle_close(zipFile *file){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)file;
  fclose(handle->fp);
  free(file);
}

zipFile * zipFileFromStdioFile(FILE *fp){
  STDIOZIPFILE *handle = (STDIOZIPFILE *)malloc(sizeof(STDIOZIPFILE));

  handle->handle.read = stdio_file_handle_read;
  handle->handle.tell = stdio_file_handle_tell;
  handle->handle.seek = stdio_file_handle_seek;
  handle->handle.error = stdio_file_handle_error;
  handle->handle.close = stdio_file_handle_close;
  handle->fp = fp;

  return &(handle->handle);
}
