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

// FAT32 Structures and Global Variables
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

void errMess()
{
  char error_message[30] = "An error has occurred\n";
  write(STDERR_FILENO, error_message, strlen(error_message));
}

void changeDir(char *token[])
{
  if (token[1] != NULL && token[2] == NULL)       // must have 1 argument after cd and 1 only
  {
    chdir(token[1]);
    if (chdir(token[1]) != 0)
    {
      errMess();
    }
  }
  else
  {
    errMess();
  }
}

void openImage(char *filename)
{
  if (isOpen == 0)
  {
    fp = fopen(filename, "r");
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
      char buffer[512];
      fseek(fp, 0, SEEK_SET);
      while (fread(buffer, 1, 512, fp) == 512)
      {
        fwrite(buffer, 1, 512, new_fp);
      }
      fclose(new_fp);
    }
  }
  else
  {
    printf("No image is open.\n");
  }
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

    // DEBUG: making sure tokenization is working
    // int token_index = 0;
    // for (token_index = 0; token_index < token_count; token_index++)
    // {
    //   printf("token[%d] = %s\n", token_index, token[token_index]);
    // }

    if (token[0] == NULL)     //print again when we enter blank stuff
    {
      free(head_ptr);
      continue;
    }
    else if (strcmp(token[0], "cd") == 0)
    {
      changeDir(token);
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
          printf("No image is open.\n");
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

    }
    else if (strcmp(token[0], "stat") == 0)
    {

    }
    else if (strcmp(token[0], "get") == 0)
    {

    }
    else if (strcmp(token[0], "put") == 0)
    {

    }
    else if (strcmp(token[0], "ls") == 0)
    {

    }
    else if (strcmp(token[0], "read") == 0)
    {

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