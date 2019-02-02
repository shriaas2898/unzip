/*
 * Authors: Lalit D. Chandwani, Aastha Shrivastava, Rakshit Sundriyal and Ujwal Singhania
 * Description: Unzip library
 * Last Modified Date: 14 March 2018
 * to compile run
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "fileunzip.h"

//TODO: Make sure that directory can be made.
int makeDirectory(char *dir){
  char command[50];

  strcpy(command, "mkdir " );
printf("done1\n" );
  strcat(command,dir);
printf("done2\n" );
  system(command);
printf("done3\n" );
  strcpy(command,"cd ");
printf("done4\n" );
  strcat(command,dir);
printf("done5\n" );
  system(command);
printf("done6\n" );
  return EXIT_SUCCESS;
}

void writeFile(char *fileName, void *data, long bytes){
  FILE *outFile;
  int index;

  for(index = 0; fileName[index]; index = index + 1){
    if(fileName[index] != '/'){
      continue;
    }

    fileName[index] = '\0'; //Terminate string at current position for the file name.
    if(makeDirectory(fileName)){
      fprintf(stderr, "Could not create subdirectory %s\n", fileName);
    }

    fileName[index] = '/'; //Restore the character.
  }

  if(!index || fileName[index-1] == '/'){
    //Do nothing, this is just to find the empty file or directory
  }

  outFile = fopen(fileName, "w");

  if(outFile != NULL){
    fwrite(data, 1, bytes, outFile);
    fclose(outFile);
  }
  else{
    fprintf(stderr, "Could not open %s for writing.\n", fileName);
  }
}

int processFile(zipFile *zip){
  fileHeader header;
  char fileName[1024];
  unsigned char *data;

  if(readLocalFileHeader(zip, &header, fileName, sizeof(fileName))){
    printf("Could not read local file header.\n");
    return -1;
  }

  if((data = (unsigned char*) malloc(header.uncompressedSize)) == NULL){
    printf("Could not allocate requested memory.\n");
    return -1;
  }

  printf("%s, %d / %d bytes at offset %08X\n", fileName, header.compressedSize, header.uncompressedSize, header.offset);

  if(readData(zip, &header, data) != Z_OK){
    printf("Could not read the file data.\n");
    free(data);
    return -1;
  }

  writeFile(fileName, data, header.uncompressedSize);
  free(data);

  return 0;
}

int recordCallback(zipFile *zip, int index, fileHeader *header, char *fileName, void *userData){
  long offset;

  offset = zip->tell(zip); //Keep old offset safe.

  if(zip->seek(zip, header->offset, SEEK_SET)){
    printf("Cannot seek in the given zip file.\n");
    return 0; //This will help exit loop + if block, where it is being used.
  }

  processFile(zip); //This will change the file offset.
  zip->seek(zip, offset, SEEK_SET); //Return to old offset.

  return 1; //Allow to continue to next records.
}




int main(int argc, char const *argv[]) {
  FILE *fp;
  endCentralDirectoryRecord endRecord;
  zipFile *zip;

  if(argc < 2){
    printf("Invalid arguments!\n");
    return EXIT_FAILURE;
  }

  if(!(fp = fopen(argv[1], "r"))){
    printf("Could not open \"%s\".\n", argv[1]);
    return EXIT_FAILURE;
  }

  zip = zipFileFromStdioFile(fp);

  if(readEndCentralDirectoryRecord(zip, &endRecord)){
    printf("Could not read ZIP file end central record.\n");
  }

  if(readCentralDirectory(zip, &endRecord, recordCallback, NULL)) {
    printf("Could not read ZIP file central directory record.\n");
  }
  return EXIT_SUCCESS;
}
