// // The MIT License (MIT)
// //
// // Copyright (c) 2024 Trevor Bakker
// //
// // Permission is hereby granted, free of charge, to any person obtaining a copy
// // of this software and associated documentation files (the "Software"), to deal
// // in the Software without restriction, including without limitation the rights
// // to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// // copies of the Software, and to permit persons to whom the Software is
// // furnished to do so, subject to the following conditions:
// //
// // The above copyright notice and this permission notice shall be included in
// // all copies or substantial portions of the Software.
// //
// //
// // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// // THE SOFTWARE.

// /*
//  * Modified By Angelina Abuhilal (ID:1002108627)
//  * FAT32 Assignment CSE3320
// */

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>

#define WHITESPACE " \t\n" // We want to split our command line up into tokens
                           // so we need to define what delimits our tokens.
                           // In this case  white space
                           // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255 // The maximum command-line size

#define MAX_NUM_ARGUMENTS 32

#define CHONK 512 // Block size

struct __attribute__((__packed__)) DirectoryEntry {
    char DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4];
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};
struct DirectoryEntry dir[16];

FILE *fp = NULL;  // File pointer
int isOpen = 0;  // Flag to check if a file is open
char currentImage[100];  // Name of the current FAT32 image

// File info storage
uint16_t BPB_BytsPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATs;
uint32_t BPB_FATSz32;
uint32_t BPB_ExtFlags;
uint32_t BPB_RootClus;
uint32_t BPB_FSInfo;

void errMess()
{
  char error_message[30] = "An error has occurred\n";
  write(STDERR_FILENO, error_message, strlen(error_message));
}

void openImage(char *filename)
{
  if (isOpen == 0)
  {
    fp = fopen(filename, "r+");
    if (fp == NULL)
    {
      errMess();
    }
    else
    {
      isOpen = 1;
      strcpy(currentImage, filename);
    }
  }
  else
  {
    printf("FAT32 image already open.\n");
  }
}

void saveImage(char *filename)
{
  if (isOpen == 1)
  {
    FILE *new_fp = fopen(filename, "w");
    if (new_fp == NULL)
    {
      errMess();
    }
    else
    {
      char buffer[CHONK];
      fseek(fp, 0, SEEK_SET);
      while (fread(buffer, 1, CHONK, fp) == CHONK)
      {
        fwrite(buffer, 1, CHONK, new_fp);
      }
      fclose(new_fp);
    }
  }
  else
  {
    printf("No image is open.\n");
  }
}

void fileNameFormat(char * file, char fileName[])
{
  memset( fileName, ' ', 12 );
  char fileCopy[12];
  strncpy(fileCopy, file, 12);
  fileCopy[11] = '\0';  // Ensure null termination

  char *token = strtok(fileCopy, ".");
  strncpy( fileName, token, strlen( token ) );

  token = strtok( NULL, "." );

  if( token )
  {
    strncpy( (char*)(fileName+8), token, strlen(token ) );
  }

  fileName[11] = '\0';

  int i;
  for( i = 0; i < 11; i++ )
  {
    fileName[i] = toupper( fileName[i] );
  }

  // printing file name for debugging
  //   for( i = 0; i < 11; i++ )
  // {
  //   printf("%c", fileName[i]);
  // }
}

void populateFileDirectory (int offset)
{
  fseek(fp, offset, SEEK_SET);
  for (int i = 0; i < 16; i++) 
  {
    // directory name (11 bytes at offset 0x00)
    fseek(fp, offset + i * 32 + 0x00, SEEK_SET);
    fread(dir[i].DIR_Name, sizeof(char), 11, fp);

    // attribute (1 byte at offset 0x0B)
    fseek(fp, offset + i * 32 + 0x0B, SEEK_SET);
    fread(&dir[i].DIR_Attr, sizeof(uint8_t), 1, fp);

    // First Cluster High (2 bytes at offset 0x14)
    fseek(fp, offset + i * 32 + 0x14, SEEK_SET);
    fread(&dir[i].DIR_FirstClusterHigh, sizeof(uint16_t), 1, fp);

    // First Cluster Low (2 bytes at offset 0x1A)
    fseek(fp, offset + i * 32 + 0x1A, SEEK_SET);
    fread(&dir[i].DIR_FirstClusterLow, sizeof(uint16_t), 1, fp);

    // file size (4 bytes at offset 0x1C)
    fseek(fp, offset + i * 32 + 0x1C, SEEK_SET);
    fread(&dir[i].DIR_FileSize, sizeof(uint32_t), 1, fp);
  }
}

void changeDir(char *directory)
{
  if (strcmp(directory, "..") == 0)
  {
    int cluster = BPB_RootClus;
    int offset = LBAToOffset(cluster);
    populateFileDirectory(offset);
  }
  else if (strcmp(directory, ".") == 0)
  {
    exit;
  }
  else
  {
    char dirName[12];
    fileNameFormat(directory, dirName);
    int found = 0;
    for (int i = 0; i < 16; i++)
    {
      if (strncmp(dirName, dir[i].DIR_Name, 11) == 0)
      {
        found = 1;
        if (dir[i].DIR_Attr == 0x10)
        {
          int cluster = dir[i].DIR_FirstClusterLow;
          int offset = LBAToOffset(cluster);
          populateFileDirectory(offset);
        }
        else
        {
          errMess();
        }
      }
    }
  }
}

/*
 * Function    : LBAToOffset
 * Parameters  : The current sector number that points to a block of data
 * Returns     : The value of the address for that block of data
 * Description : Finds the starting address of a block of data given the sector number
 *               corresponding to that data block.
 */
int LBAToOffset(int32_t sector) {
    return ((sector - 2) * BPB_BytsPerSec) + (BPB_BytsPerSec * BPB_RsvdSecCnt) + (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec);
}

/*
 * Name    : NextLB
 * Purpose : Given a logical block address, look up into the first FAT and
 *           return the logical block address of the block in the file.
 *           If there is no further blocks then return -1
 */
int16_t NextLB(uint32_t sector) {
    uint32_t FATAddress = (BPB_BytsPerSec * BPB_RsvdSecCnt) + (sector * 4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2, 1, fp);
    return val;
}

int main(int argc, char *argv[])
{
  FILE *input = stdin;        //stdin unless told otherwise (batch mode)

  if (argc == 2)              // there is a file given while calling the program
  {
    input = fopen(argv[1], "r");
    if (input == NULL)
    {
      errMess();
      exit(1);
    }
  }
  else if (argc > 2)
  {
    errMess();
    exit(1);
  }

  char *command_string = (char *)malloc(MAX_COMMAND_SIZE);

  while (1)
  {
    // Print prompt only in interactive mode
    if (input == stdin)
    {
      printf("mfs> ");
    }

    // Read the command (from stdin or batch file)
    if (fgets(command_string, MAX_COMMAND_SIZE, input) == NULL)
    {
      // Exit if end-of-file is reached in batch mode
      if (input != stdin)
      {
        fclose(input);
      }
      exit(0);
    }

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;

    // Pointer to point to the token
    // parsed by strsep
    char *argument_pointer;

    char *working_string = strdup(command_string);

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end

    char *head_ptr = working_string;

    // Tokenize the input with whitespace used as the delimiter
    while (((argument_pointer = strsep(&working_string, WHITESPACE)) != NULL) &&
           (token_count < MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup(argument_pointer, MAX_COMMAND_SIZE);
      if (strlen(token[token_count]) == 0)
      {
        token[token_count] = NULL;
      }
      token_count++;
    }

    if (token[0] == NULL)     //print again when we enter blank stuff
    {
      free(head_ptr);
      continue;
    }
    else if (strcmp(token[0], "quit") == 0 || strcmp(token[0], "exit") == 0)
    {
      if (token[1] == NULL)
      {
        exit(0);
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "open") == 0)
    {
      if (token[1] != NULL && token[2] == NULL)
      {
        char *filename = token[1];
        openImage(filename);

        // once file is open, get the FAT32 info
        fseek(fp, 11, SEEK_SET);
        fread(&BPB_BytsPerSec, 1, 2, fp);

        fseek(fp, 13, SEEK_SET);
        fread(&BPB_SecPerClus, 1, 1, fp);

        fseek(fp, 14, SEEK_SET);
        fread(&BPB_RsvdSecCnt, 1, 2, fp);

        fseek(fp, 16, SEEK_SET);
        fread(&BPB_NumFATs, 1, 1, fp);

        fseek(fp, 36, SEEK_SET);
        fread(&BPB_FATSz32, 1, 4, fp);

        fseek(fp, 40, SEEK_SET);
        fread(&BPB_ExtFlags, 1, 2, fp);

        fseek(fp, 44, SEEK_SET);
        fread(&BPB_RootClus, 1, 4, fp);

        fseek(fp, 48, SEEK_SET);
        fread(&BPB_FSInfo, 1, 2, fp);

        //root directory address
        int root = (BPB_NumFATs * BPB_FATSz32 * BPB_BytsPerSec) + (BPB_RsvdSecCnt * BPB_BytsPerSec);

        populateFileDirectory(root);
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "close") == 0)
    {
      if (token[1] == NULL)
      {
        if (isOpen == 1)
        {
          fclose(fp);
          isOpen = 0;
        }
        else
        {
          printf("No image is open\n");
        }
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "save") == 0)
    {
      if (token[1] != NULL && token[2] == NULL) 
      {
        saveImage(token[1]);
      } 
      else if (token[1] == NULL)
      {
        saveImage(currentImage);
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "info") == 0)
    {
      if (isOpen == 0)
      {
        printf("No image is open\n");
      }
      else
      {
        printf("BPB_BytsPerSec:   %d    0x%x\n", BPB_BytsPerSec, BPB_BytsPerSec);
        printf("BPB_SecPerClus:   %d    0x%x\n", BPB_SecPerClus, BPB_SecPerClus);
        printf("BPB_RsvdSecCnt:   %d    0x%x\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
        printf("BPB_NumFATs:      %d    0x%x\n", BPB_NumFATs, BPB_NumFATs);
        printf("BPB_FATSz32:      %d    0x%x\n", BPB_FATSz32, BPB_FATSz32);
        printf("BPB_ExtFlags:     %d    0x%x\n", BPB_ExtFlags, BPB_ExtFlags);
        printf("BPB_RootClus:     %d    0x%x\n", BPB_RootClus, BPB_RootClus);
        printf("BPB_FSInfo:       %d    0x%x\n", BPB_FSInfo, BPB_FSInfo);
      }
    }
    else if (strcmp(token[0], "stat") == 0)
    {
      if (token[1] != NULL && token[2] == NULL)
      {
        char fileName[12];
        fileNameFormat(token[1], fileName);
        int found = 0;
        for (int i = 0; i < 16; i++)
        {
          if (strncmp(fileName, dir[i].DIR_Name, 11) == 0)
          {
            found = 1;
            printf("Attribute: %d\n", dir[i].DIR_Attr);
            printf("Starting Cluster Number: %d\n", dir[i].DIR_FirstClusterLow);
            printf("Size: %d\n", dir[i].DIR_FileSize);
            break;
          }
        }
        if (found == 0)
        {
          printf("Error: File not found\n");
        } 
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "get") == 0)
    {
      if (token[1] != NULL)
      {
        char fileName[12];
        fileNameFormat(token[1], fileName);
        int found = 0;
        for (int i = 0; i < 16; i++)
        {
          if (strncmp(fileName, dir[i].DIR_Name, 11) == 0)
          {
            found = 1;
            int cluster = dir[i].DIR_FirstClusterLow;
            int offset = LBAToOffset(cluster);
            int size = dir[i].DIR_FileSize;
            char buffer[CHONK];
            char *outputFileName;

            if (token[2] != NULL)
            {
              outputFileName = token[2];
            }
            else
            {
              outputFileName = token[1];
            }

            FILE *new_fp = fopen(outputFileName, "w");
            
            while (size > 0)
            {
              fseek(fp, offset, SEEK_SET);
              if (size > CHONK)
              {
                fread(buffer, 1, CHONK, fp);
                fwrite(buffer, 1, CHONK, new_fp);
                size -= CHONK;
                cluster = NextLB(cluster);
                offset = LBAToOffset(cluster);
              }
              else
              {
                fread(buffer, 1, size, fp);
                fwrite(buffer, 1, size, new_fp);
                size = 0;
              }
            }
            fclose(new_fp);
            break;
          }
        }
        if (found == 0)
        {
          errMess();
        }
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "put") == 0)
    {
      if (token[1] != NULL)
      {
        char fileName[12];
        char changeName[12];
        fileNameFormat(token[1], fileName);

        if (token[2] == NULL)
        {
          fileNameFormat(token[1], changeName);
        }
        else
        {
          fileNameFormat(token[2], changeName);
        }

        FILE *src_fp = fopen(token[1], "rb");
        if (src_fp == NULL)
        {
          printf("Error: File not found\n");
        }

        // Find empty directory entry
        int pos = -1;
        for(int i = 0; i < 16; i++)
        {
          if (dir[i].DIR_Name[0] == 0xE5 || dir[i].DIR_Name[0] == 0x00)
          {
            pos = i;
            break;
          }
        }
        if (pos == -1)
        {
          printf("No space ig??????\n");
          fclose(src_fp);
          continue;
        }
        // Change Directory Entry info
        for (int i = 0; i < 12; i++)
        {
          dir[pos].DIR_Name[i] = changeName[i];
        }
        dir[pos].DIR_Attr = 0x20;
        dir[pos].DIR_FileSize = 0;

        // Copy blocks from file to image starting at cluster low
        int cluster = dir[pos].DIR_FirstClusterLow;
        int offset = LBAToOffset(cluster);
        char buffer[CHONK];
        int size = 0;
        while (1)
        {
          size = fread(buffer, 1, CHONK, src_fp);
          fseek(fp, offset, SEEK_SET);
          fwrite(buffer, 1, size, fp);
          if (size > CHONK)
          {
            cluster = NextLB(cluster);
            offset = LBAToOffset(cluster);
          }
          else
          {
            break;
          }
        }
        // Update directory entry
        dir[pos].DIR_FileSize = ftell(src_fp);
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "cd") == 0)
    {
      if (token[1] != NULL && token[2] == NULL)
      {
        char pathCopy[MAX_INPUT];
        strncpy(pathCopy, token[1], sizeof(pathCopy));
        pathCopy[sizeof(pathCopy) - 1] = '\0'; 

        // Tokenize the path using "/" as the delimiter
        char *component = strtok(pathCopy, "/");
        
        while (component != NULL) 
        {
          //printf("Tokenized component: %s\n", component); tbh tokenization doesnt work, just kinda works for ../name
          changeDir(component); // Process each directory component
          component = strtok(NULL, "/"); // Get the next component
        }
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "ls") == 0)
    {
      if (strcmp(token[1], "..") == 0)
      {
        char currDir[12] = dir[0].DIR_Name;
        changeDir("..");
        for (int i = 0; i < 16; i++)
        {
          if ((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && dir[i].DIR_Name[0] != 0xE5)
          {
            printf("%s\n", dir[i].DIR_Name);
          }
        }
        changeDir(currDir);
      }
      else
      {
      for (int i = 0; i < 16; i++)
      {
        if ((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && dir[i].DIR_Name[0] != 0xE5)
        {
          printf("%s\n", dir[i].DIR_Name);
        }
      }
      }


    }
    else if (strcmp(token[0], "read") == 0)
    {
      if (token[1] == NULL || token[2] == NULL || token[3] == NULL)
      {
        errMess();
      }
      else
      {
        char fileName[12];
        fileNameFormat(token[1], fileName);
        int position = atoi(token[2]);
        int numOfBytes = atoi(token[3]);
        char buffer[numOfBytes];
        
        for (int i = 0; i < 16; i++)
        {
          if (strncmp(fileName, dir[i].DIR_Name, 11) == 0)
          {
            int cluster = dir[i].DIR_FirstClusterLow;
            int offset = LBAToOffset(cluster);
            int size = dir[i].DIR_FileSize;

            if (position > size)
            {
              errMess();
            }
            else
            {
              fseek(fp, offset + position, SEEK_SET);
              fread(&buffer, 1, numOfBytes, fp);
            }
            for (int j = 0; j < numOfBytes; j++)
            {
              if (token[4] == NULL)
              {
                printf("%x ", buffer[j]);
              }
              else if (strcmp(token[4], "-dec") == 0)
              {
                printf("%d ", buffer[j]);
              }
              else if (strcmp(token[4], "-ascii") == 0)
              {
                printf("%c ", buffer[j]);
              }
              else
              {
                printf("%x ", buffer[j]);
              }
            }
            printf("\n");
          }
        }
      }
    }
    else if (strcmp(token[0], "del") == 0)
    {
      if (token[1] != NULL && token[2] == NULL)
      {
        char fileName[12];
        fileNameFormat(token[1], fileName);
        int found = 0;
        for (int i = 0; i < 16; i++)
        {
          if (strncmp(fileName, dir[i].DIR_Name, 11) == 0)
          {
            found = 1;
            char temp[12];
            for (int k = 0; k < 12; k++)
            {
              temp[k] = fileName[k]; // Copy the file name to temp
            }

            for (int j = 0; j < 12; j++)
            {
              if(fileName[j] != ' ')
              {
                fileName[j+1] = temp[j];
              }
              else
              {
                break;
              }
            }
            fileName[0] = 0xE5;
            //printf("%s\n", fileName);
            for (int m = 0; m < 11; m++)
            {
              dir[i].DIR_Name[m] = fileName[m];
            }
            break;
          }
        }
        if (found == 0)
        {
          errMess();
        }
      }
      else
      {
        errMess();
      }
    }
    else if (strcmp(token[0], "undel") == 0)
    {
      if (token[1] != NULL)
      {
        char fileName[12];
        fileNameFormat(token[1], fileName);
        int found = 0;
        for (int i = 0; i < 16; i++)
        {
          // Checking all deleted files
          if (dir[i].DIR_Name[0] == 0xE5)
          {
            for (int k = 0; k < 12; k++)
            {
              if (k < 3)
              {
                if (fileName[k] != dir[i].DIR_Name[k+1])
                {
                  printf("Error: File not found\n");
                  break;
                }
              }
              if (k == 11)
              {
                found = 1;
                for(int j = 0; j < 11; j++)
                {
                  dir[i].DIR_Name[j] = fileName[j];
                }
              }
            }
          }
        }
        if (found == 0)
        {
          printf("Error: File not found\n");
          errMess();
        }
      }
      else
      {
        errMess();
      }
    }
    else
    {
      errMess();
    }

    free(head_ptr);
  }
  free(command_string);
  return 0;
}
