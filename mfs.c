#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>   
#include <errno.h>
#include <stdbool.h>
#define MAX_INPUT 255
uint32_t parentCluster = 0; // Initialize to 0
uint32_t curCluster = 0;    // Current directory cluster
uint32_t totalClusters;
int dir_entries_count = 0;


struct __attribute__((__packed__)) DirectoryEntry {
    char DIR_Name[11];             // File name
    uint8_t DIR_Attr;              // File attributes
    uint8_t Unused1[8];            // Unused bytes
    uint16_t DIR_FirstClusterHigh; // High word of first cluster
    uint8_t Unused2[4];            // More unused bytes
    uint16_t DIR_FirstClusterLow;  // Low word of first cluster
    uint32_t DIR_FileSize;         // File size in bytes
};

#define MAX_DIR_ENTRIES 512 // Adjust as needed
struct DirectoryEntry dir[MAX_DIR_ENTRIES];

uint16_t BPB_BytesPerSec;
uint8_t BPB_SecPerClus;
uint16_t BPB_RsvdSecCnt;
uint8_t BPB_NumFATs;
uint32_t BPB_FATSz32;
uint16_t BPB_ExtFlags;
uint32_t BPB_RootClus;
uint16_t BPB_FSInfo;

FILE *fp = NULL;

char open_filename[MAX_INPUT] = "";





int LBAToOffset(uint32_t sector) {
    return ((sector - 2) * BPB_BytesPerSec) +
           (BPB_BytesPerSec * BPB_RsvdSecCnt) +
           (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec);
}

uint32_t NextLb(uint32_t sector) {
    uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector * 4);
    uint32_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 4, 1, fp);
    return val;
}



void parse() {
    fseek(fp, 11, SEEK_SET);
    fread(&BPB_BytesPerSec, 2, 1, fp);
    BPB_BytesPerSec = le16toh(BPB_BytesPerSec);

    fseek(fp, 13, SEEK_SET);
    fread(&BPB_SecPerClus, 1, 1, fp);

    fseek(fp, 14, SEEK_SET);
    fread(&BPB_RsvdSecCnt, 2, 1, fp);
    BPB_RsvdSecCnt = le16toh(BPB_RsvdSecCnt);

    fseek(fp, 16, SEEK_SET);
    fread(&BPB_NumFATs, 1, 1, fp);

    fseek(fp, 36, SEEK_SET);
    fread(&BPB_FATSz32, 4, 1, fp);
    BPB_FATSz32 = le32toh(BPB_FATSz32);

    fseek(fp, 44, SEEK_SET);
    fread(&BPB_RootClus, 4, 1, fp);
    BPB_RootClus = le32toh(BPB_RootClus);

    curCluster = BPB_RootClus;
    parentCluster = BPB_RootClus;

    // Calculate totalClusters
    totalClusters = (BPB_FATSz32 * BPB_BytesPerSec) / 4;
}


int dir_entry_offsets[MAX_DIR_ENTRIES]; // Global array to store offsets

 

void readDirectory(uint32_t cluster) {
    int i = 0;
    int entries_per_cluster = (BPB_BytesPerSec * BPB_SecPerClus) / sizeof(struct DirectoryEntry);
    struct DirectoryEntry entry;
    dir_entries_count = 0; // Keep track of the total entries read

    do {
        int offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);

        for (int j = 0; j < entries_per_cluster; j++) {
            fread(&entry, sizeof(struct DirectoryEntry), 1, fp);

           
            if (entry.DIR_Name[0] == 0x00) {
                return;
            }

            
            dir[dir_entries_count++] = entry;
            if (dir_entries_count >= MAX_DIR_ENTRIES) {
                // Resize dir array or handle accordingly
                printf("Error: Directory entries exceed maximum limit.\n");
                return;
            }
        }

      
        cluster = NextLb(cluster);
    } while (cluster < 0x0FFFFFF8);
}

void formatFilename(const char *input, char *formattedName) {
    memset(formattedName, ' ', 11); 

    char name[9] = {0};  
    char ext[4] = {0};   

    
    char temp[256];
    strncpy(temp, input, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';

    
    char *token = strtok(temp, ".");
    if (token != NULL) {
        strncpy(name, token, 8);
        token = strtok(NULL, ".");
        if (token != NULL) {
            strncpy(ext, token, 3);
        }
    }

    
    for (int i = 0; i < 8; i++) {
        if (i < strlen(name)) {
            formattedName[i] = toupper(name[i]);
        } else {
            formattedName[i] = ' ';
        }
    }
    for (int i = 0; i < 3; i++) {
        if (i < strlen(ext)) {
            formattedName[8 + i] = toupper(ext[i]);
        } else {
            formattedName[8 + i] = ' ';
        }
    }
}


void parsePath(char *path, char **components, int *count) {
    char *token;
    *count = 0;
    token = strtok(path, "/");
    while (token != NULL) {
        components[(*count)++] = token;
        token = strtok(NULL, "/");
    }
}

bool findDirectoryEntry(uint32_t startingCluster, char *name, uint32_t *cluster) {
    readDirectory(startingCluster);
    char formattedName[12];
    formatFilename(name, formattedName);

    for (int i = 0; i < dir_entries_count; i++) {
        if (strncmp(dir[i].DIR_Name, formattedName, 11) == 0) {
            if (dir[i].DIR_Attr & 0x10) { // Check if it's a directory
                *cluster = (dir[i].DIR_FirstClusterHigh << 16) | dir[i].DIR_FirstClusterLow;
                if (*cluster == 0) {
                    *cluster = BPB_RootClus; // Handle special case for root directory
                }
                return true;
            }
        }
    }
    return false;
}
uint32_t getParentCluster(uint32_t currentCluster) {
    readDirectory(currentCluster);

    for (int i = 0; i < dir_entries_count; i++) {
        if (strncmp(dir[i].DIR_Name, "..         ", 11) == 0) {
            uint32_t parentCluster = (dir[i].DIR_FirstClusterHigh << 16) | dir[i].DIR_FirstClusterLow;
            if (parentCluster == 0) {
                parentCluster = BPB_RootClus; // Handle special case for root directory
            }
            return parentCluster;
        }
    }
    
    return BPB_RootClus;
}

int main(int argc, char *argv[]){
    char input[MAX_INPUT];
    char *list_of_arguments[MAX_INPUT];
    
    while(1){
        printf("mfs>");
        
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("Error reading input\n");
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        if (strlen(input) == 0) {
            continue;
        }

        char *token;
        int i = 0;
        token = strtok(input," ");
        while(token != NULL){
            list_of_arguments[i++] = token;
            token = strtok(NULL," ");
        }
        list_of_arguments[i] = NULL;

        if(strcmp(list_of_arguments[0], "quit") == 0 || strcmp(list_of_arguments[0], "exit") == 0) break;
        
        if(strcmp(list_of_arguments[0], "open") == 0){
            if (fp != NULL) {
                printf("Error: A file system is already open. Close it first before opening another.\n");
                continue;
            }

            if (list_of_arguments[1] == NULL) {
                printf("Error: No filename provided.\n");
                continue;
            }
            fp = fopen(list_of_arguments[1], "rb");
            if (fp == NULL) {
                printf("Error opening file\n");
            } else {
                strncpy(open_filename, list_of_arguments[1], MAX_INPUT);
                open_filename[MAX_INPUT - 1] = '\0';

                parse();
                curCluster = BPB_RootClus;
                readDirectory(curCluster);
                printf("File system image '%s' opened successfully.\n", list_of_arguments[1]);
            }
        }
        else if(strcmp(list_of_arguments[0], "close") == 0){
            if(fp == NULL) printf("Error: File system not open.\n");
            else{
                fclose(fp);
                fp = NULL;
                printf("File system closed successfully.\n");
            }
        }
        else if(strcmp(list_of_arguments[0], "info") == 0){
            if (fp == NULL) {
                printf("Error: File system not open.\n");
                continue;
            }
 
            printf("BPB_BytesPerSec: %u (0x%X)\n", BPB_BytesPerSec, BPB_BytesPerSec);
            printf("BPB_SecPerClus: %u (0x%X)\n", BPB_SecPerClus, BPB_SecPerClus);
            printf("BPB_RsvdSecCnt: %u (0x%X)\n", BPB_RsvdSecCnt, BPB_RsvdSecCnt);
            printf("BPB_NumFATs: %u (0x%X)\n", BPB_NumFATs, BPB_NumFATs);
            printf("BPB_FATSz32: %u (0x%X)\n", BPB_FATSz32, BPB_FATSz32);
            printf("BPB_RootClus: %u (0x%X)\n", BPB_RootClus, BPB_RootClus);
        }
        else if(strcmp(list_of_arguments[0], "stat") == 0){
            if (fp == NULL) {
                printf("Error: File system not open.\n");
                continue;
            }

            if(list_of_arguments[1] == NULL){
                printf("Error: No filename or directory name provided.\n");
                continue;
            }

            char formattedName[12];
            formatFilename(list_of_arguments[1], formattedName);

            bool found = false;
            readDirectory(curCluster);

            for(int i = 0; i < 16; i++){
                if(strncmp(dir[i].DIR_Name, formattedName, 11) == 0){
                    printf("File Attributes: ");
                    if(dir[i].DIR_Attr & 0x01) printf("Read Only ");
                    if(dir[i].DIR_Attr & 0x02) printf("Hidden ");
                    if(dir[i].DIR_Attr & 0x04) printf("System ");
                    if(dir[i].DIR_Attr & 0x08) printf("Volume ID ");
                    if(dir[i].DIR_Attr & 0x10) printf("Directory ");
                    if(dir[i].DIR_Attr & 0x20) printf("Archive ");
                    printf("\n");

                    uint32_t firstCluster = (dir[i].DIR_FirstClusterHigh << 16) | dir[i].DIR_FirstClusterLow;
                    printf("Starting Cluster Number: %u\n", firstCluster);
                    printf("File Size: %u bytes\n", dir[i].DIR_FileSize);
                    found = true;
                    break;
                }
            }

            if(!found){
                printf("Error: File not found\n");
            }
        }
        

    else if(strcmp(list_of_arguments[0], "ls") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    readDirectory(curCluster);  

    for (int i = 0; i < 16; i++) {
       
        if (dir[i].DIR_Name[0] == 0xE5 || dir[i].DIR_Name[0] == 0x00) {
            continue;
        }

        
        if (dir[i].DIR_Attr & 0x02 || dir[i].DIR_Attr & 0x04) {
            continue;
        }

        
        char name[12];
        memset(name, 0, 12);  // Initialize name with null characters
        memcpy(name, dir[i].DIR_Name, 11);  // Copy name (8 + 3 format)

        
        for (int j = 10; j >= 0 && name[j] == ' '; j--) {
            name[j] = '\0';
        }

        // Print directory and file names
        if (dir[i].DIR_Attr & 0x10) {  // Directory attribute
            printf("<DIR> %s\n", name);
        } else {
            printf("%s\n", name);
        }
    }
}



else if (strcmp(list_of_arguments[0], "get") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    if (list_of_arguments[1] == NULL) {
        printf("Error: no file specified\n");
        continue;
    }

   
    char formattedName[12];
    formatFilename(list_of_arguments[1], formattedName);

    const char *output_filename;
    if (list_of_arguments[2] != NULL) {
        output_filename = list_of_arguments[2];
    } else {
        output_filename = list_of_arguments[1];
    }

    readDirectory(curCluster);

    
    bool found = false;
    struct DirectoryEntry *entry = NULL;
    for (int i = 0; i < 16; i++) {
        if (strncmp(dir[i].DIR_Name, formattedName, 11) == 0) {
            found = true;
            entry = &dir[i];
            break;
        }
    }

    if (!found) {
        printf("Error: File not found\n");
        continue;
    }

   
    uint32_t firstCluster = (entry->DIR_FirstClusterHigh << 16) | entry->DIR_FirstClusterLow;
    uint32_t fileSize = entry->DIR_FileSize;

    
    FILE *dest_fp = fopen(output_filename, "wb");
    if (dest_fp == NULL) {
        printf("Error: Unable to open file '%s' for writing.\n", output_filename);
        continue;
    }

    
    uint32_t remaining_bytes = fileSize;
    uint32_t cluster = firstCluster;

    while (remaining_bytes > 0) {
        int offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);

        
        uint8_t buffer[BPB_BytesPerSec * BPB_SecPerClus];
        size_t bytes_to_read = (remaining_bytes < sizeof(buffer)) ? remaining_bytes : sizeof(buffer);
        fread(buffer, 1, bytes_to_read, fp);
        fwrite(buffer, 1, bytes_to_read, dest_fp);

       
        remaining_bytes -= bytes_to_read;
        cluster = NextLb(cluster);

       
        if (cluster >= 0x0FFFFFF8) {
            break;
        }
    }

    fclose(dest_fp);

    printf("File '%s' retrieved successfully.\n", output_filename);
}

else if (strcmp(list_of_arguments[0], "put") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    if (list_of_arguments[1] == NULL) {
        printf("Error: No filename specified.\n");
        continue;
    }

   
    char *source_filename = list_of_arguments[1];
    const char *new_filename_const = (list_of_arguments[2] != NULL) ? list_of_arguments[2] : list_of_arguments[1];

    
    char new_filename[256]; // Use a larger buffer to be safe
    strncpy(new_filename, new_filename_const, sizeof(new_filename) - 1);
    new_filename[sizeof(new_filename) - 1] = '\0';

    
    char formattedName[12];
    formatFilename(new_filename, formattedName);

    // Open the source file for reading
    FILE *src_fp = fopen(source_filename, "rb");
    if (src_fp == NULL) {
        printf("Error: File not found.\n");
        continue;
    }

    // Determine the file size
    fseek(src_fp, 0, SEEK_END);
    uint32_t fileSize = ftell(src_fp);
    fseek(src_fp, 0, SEEK_SET);

  // Find an empty directory entry
    readDirectory(curCluster);
    struct DirectoryEntry *entry = NULL;
    int entry_index = -1;
    for (int i = 0; i < dir_entries_count; i++) {
        if (dir[i].DIR_Name[0] == 0x00 || dir[i].DIR_Name[0] == 0xE5) {
            entry = &dir[i];
            entry_index = i;
            break;
        }
    }

    if (entry == NULL) {
        printf("Error: No empty directory entry found.\n");
        fclose(src_fp);
        continue;
    }


    // Find the total number of clusters
    uint32_t totalClusters = (BPB_FATSz32 * BPB_BytesPerSec) / 4;

    // Find a free cluster to start storing the file
    uint32_t firstCluster = 0;
    uint32_t EOC = 0x0FFFFFF8; // End of Cluster chain marker

    // Mark the clusters in the FAT
    uint32_t clusterChain[totalClusters]; // Over-allocate for simplicity
    memset(clusterChain, 0, sizeof(clusterChain));

    uint32_t neededClusters = (fileSize + (BPB_BytesPerSec * BPB_SecPerClus) - 1) / (BPB_BytesPerSec * BPB_SecPerClus);
    uint32_t clustersFound = 0;

    for (uint32_t i = 2; i < totalClusters && clustersFound < neededClusters; i++) {
        uint32_t FAT_address = BPB_BytesPerSec * BPB_RsvdSecCnt + (i * 4);
        fseek(fp, FAT_address, SEEK_SET);
        uint32_t FAT_entry;
        fread(&FAT_entry, 4, 1, fp);

        if (FAT_entry == 0x00000000) {
            clusterChain[clustersFound++] = i;
        }
    }

    if (clustersFound < neededClusters) {
        printf("Error: Not enough free clusters.\n");
        fclose(src_fp);
        continue;
    }

    firstCluster = clusterChain[0];

    
    for (uint32_t idx = 0; idx < clustersFound; idx++) {
        uint32_t cluster = clusterChain[idx];
        uint32_t nextCluster = (idx < clustersFound - 1) ? clusterChain[idx + 1] : EOC;

        for (uint32_t j = 0; j < BPB_NumFATs; j++) {
            uint32_t FAT_address = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (j * BPB_FATSz32 * BPB_BytesPerSec) + (cluster * 4);
            fseek(fp, FAT_address, SEEK_SET);
            fwrite(&nextCluster, 4, 1, fp);
        }
    }

   
    uint32_t remainingBytes = fileSize;
    for (uint32_t idx = 0; idx < clustersFound; idx++) {
        uint32_t cluster = clusterChain[idx];
        uint8_t buffer[BPB_BytesPerSec * BPB_SecPerClus];
        memset(buffer, 0, sizeof(buffer));

        size_t bytesToRead = (remainingBytes < sizeof(buffer)) ? remainingBytes : sizeof(buffer);
        size_t bytesRead = fread(buffer, 1, bytesToRead, src_fp);

        if (bytesRead == 0) {
            printf("Error reading source file.\n");
            fclose(src_fp);
            break;
        }

        int offset = LBAToOffset(cluster);
        fseek(fp, offset, SEEK_SET);
        fwrite(buffer, 1, bytesRead, fp);

        remainingBytes -= bytesRead;
    }

    fclose(src_fp);

    
    memset(entry, 0, sizeof(struct DirectoryEntry));
    memcpy(entry->DIR_Name, formattedName, 11);
    entry->DIR_Attr = 0x20;  // Archive attribute
    entry->DIR_FileSize = fileSize;
    entry->DIR_FirstClusterLow = firstCluster & 0xFFFF;
    entry->DIR_FirstClusterHigh = (firstCluster >> 16) & 0xFFFF;

    
    fseek(fp, dir_entry_offsets[entry_index], SEEK_SET);
    fwrite(entry, sizeof(struct DirectoryEntry), 1, fp);

    printf("File '%s' successfully placed in the FAT32 image as '%s'.\n", source_filename, new_filename);
}
else if (strcmp(list_of_arguments[0], "cd") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    if (list_of_arguments[1] == NULL) {
        printf("Error: No directory specified.\n");
        continue;
    }

    char path[MAX_INPUT];
    strncpy(path, list_of_arguments[1], MAX_INPUT - 1);
    path[MAX_INPUT - 1] = '\0';

    char *components[64]; // Adjust size as needed
    int componentCount = 0;

    parsePath(path, components, &componentCount);

    uint32_t startingCluster;

    
    if (list_of_arguments[1][0] == '/') {
        startingCluster = BPB_RootClus;
    } else {
        startingCluster = curCluster;
    }

    bool success = true;
    for (int i = 0; i < componentCount; i++) {
        char *component = components[i];

        if (strcmp(component, ".") == 0) {
           
        } else if (strcmp(component, "..") == 0) {
           
            if (startingCluster == BPB_RootClus) {
                // Already at root, cannot go up
                printf("Error: Already at root directory.\n");
                success = false;
                break;
            } else {
               
                uint32_t parentCluster = getParentCluster(startingCluster);
                if (parentCluster == 0) {
                    printf("Error: Cannot find parent directory.\n");
                    success = false;
                    break;
                }
                startingCluster = parentCluster;
            }
        } else {
           
            uint32_t nextCluster;
            if (findDirectoryEntry(startingCluster, component, &nextCluster)) {
                startingCluster = nextCluster;
            } else {
                printf("Error: Directory '%s' not found.\n", component);
                success = false;
                break;
            }
        }
    }

    if (success) {
        curCluster = startingCluster;
        printf("Changed directory to '%s'.\n", list_of_arguments[1]);
    }
}



else if (strcmp(list_of_arguments[0], "read") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    
    if (list_of_arguments[1] == NULL || list_of_arguments[2] == NULL || list_of_arguments[3] == NULL) {
        printf("Error: Missing arguments. Usage: read <filename> <position> <number of bytes> <OPTION>\n");
        continue;
    }

    char *filename = list_of_arguments[1];
    int position = atoi(list_of_arguments[2]);
    int num_bytes = atoi(list_of_arguments[3]);

    
    char *option = NULL;
    if (list_of_arguments[4] != NULL) {
        option = list_of_arguments[4];
    }

 
    char formattedName[12];
    formatFilename(filename, formattedName);

   
    readDirectory(curCluster);

    
    bool found = false;
    struct DirectoryEntry *entry = NULL;
    for (int i = 0; i < dir_entries_count; i++) {
        if (strncmp(dir[i].DIR_Name, formattedName, 11) == 0) {
            // Ensure it's a file (not a directory)
            if (!(dir[i].DIR_Attr & 0x10)) {
                found = true;
                entry = &dir[i];
                break;
            }
        }
    }

    if (!found) {
        printf("Error: File not found.\n");
        continue;
    }

    uint32_t fileSize = entry->DIR_FileSize;

    if (position < 0 || position >= fileSize) {
        printf("Error: Position out of bounds.\n");
        continue;
    }

    if (num_bytes <= 0) {
        printf("Error: Number of bytes must be positive.\n");
        continue;
    }

    // Adjust num_bytes if it exceeds the file size
    if (position + num_bytes > fileSize) {
        num_bytes = fileSize - position;
    }

    uint32_t firstCluster = (entry->DIR_FirstClusterHigh << 16) | entry->DIR_FirstClusterLow;
    if (firstCluster == 0) {
        firstCluster = BPB_RootClus; // Handle special case
    }

    uint8_t *buffer = malloc(num_bytes);
    if (buffer == NULL) {
        printf("Error: Memory allocation failed.\n");
        continue;
    }

    uint32_t clusterSize = BPB_BytesPerSec * BPB_SecPerClus;
    uint32_t cluster = firstCluster;
    uint32_t bytesRead = 0;
    uint32_t offset = position;

    
    uint32_t clusterSkip = offset / clusterSize;
    offset = offset % clusterSize;

   
    for (uint32_t i = 0; i < clusterSkip; i++) {
        cluster = NextLb(cluster);
        if (cluster >= 0x0FFFFFF8) {
            printf("Error: Reached end of cluster chain unexpectedly.\n");
            free(buffer);
            continue;
        }
    }

    while (bytesRead < num_bytes && cluster < 0x0FFFFFF8) {
        
        uint32_t clusterOffset = offset % clusterSize;

     
        int fileOffset = LBAToOffset(cluster);
        fseek(fp, fileOffset + clusterOffset, SEEK_SET);

        
        uint32_t bytesToRead = clusterSize - clusterOffset;
        if (bytesToRead > num_bytes - bytesRead) {
            bytesToRead = num_bytes - bytesRead;
        }

        fread(buffer + bytesRead, 1, bytesToRead, fp);
        bytesRead += bytesToRead;
        offset += bytesToRead;

        
        offset = 0;

        // Move to the next cluster if necessary
        if (bytesRead < num_bytes) {
            cluster = NextLb(cluster);
        }
    }

    // Output the data
    if (option == NULL || strcmp(option, "-hex") == 0) {
        // Default: Hexadecimal output
        for (int i = 0; i < bytesRead; i++) {
            printf("0x%02X ", buffer[i]);
        }
        printf("\n");
    } else if (strcmp(option, "-ascii") == 0) {
        // ASCII output
        for (int i = 0; i < bytesRead; i++) {
            if (isprint(buffer[i])) {
                printf("%c", buffer[i]);
            } else {
                printf(".");
            }
        }
        printf("\n");
    } else if (strcmp(option, "-dec") == 0) {
        
        for (int i = 0; i < bytesRead; i++) {
            printf("%d ", buffer[i]);
        }
        printf("\n");
    } else {
        printf("Error: Invalid option '%s'.\n", option);
    }

    free(buffer);
}

else if (strcmp(list_of_arguments[0], "del") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    if (list_of_arguments[1] == NULL) {
        printf("Error: No filename specified.\n");
        continue;
    }

    char *filename = list_of_arguments[1];

   
    char formattedName[12];
    formatFilename(filename, formattedName);

    
    readDirectory(curCluster);

    bool found = false;

   
    for (int i = 0; i < dir_entries_count; i++) {
        if (strncmp(dir[i].DIR_Name, formattedName, 11) == 0) {
            // Mark the directory entry as deleted
            dir[i].DIR_Name[0] = 0xE5;

           
            uint32_t cluster = curCluster;
            int entries_per_cluster = (BPB_BytesPerSec * BPB_SecPerClus) / sizeof(struct DirectoryEntry);
            int cluster_offset = (i / entries_per_cluster) * BPB_SecPerClus;
            int entry_offset = i % entries_per_cluster;

            // Move to the correct cluster in the chain
            for (int c = 0; c < cluster_offset; c++) {
                cluster = NextLb(cluster);
            }

            int offset = LBAToOffset(cluster) + (entry_offset * sizeof(struct DirectoryEntry));

           
            fseek(fp, offset, SEEK_SET);
            fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, fp);

            found = true;
            printf("File '%s' deleted successfully.\n", filename);
            break;
        }
    }

    if (!found) {
        printf("Error: File not found.\n");
    }
}

else if (strcmp(list_of_arguments[0], "undel") == 0) {
    if (fp == NULL) {
        printf("Error: File system not open.\n");
        continue;
    }

    if (list_of_arguments[1] == NULL) {
        printf("Error: No filename specified.\n");
        continue;
    }

    char *filename = list_of_arguments[1];

    // Format the filename, replacing the first character with '?'
    char formattedName[12];
    formatFilename(filename, formattedName);
    formattedName[0] = '?'; 

    // Read the current directory
    readDirectory(curCluster);

    // Search for deleted entries
    bool found = false;
    struct DirectoryEntry *entry = NULL;
    int entry_index = -1;
    for (int i = 0; i < dir_entries_count; i++) {
        if ((unsigned char)dir[i].DIR_Name[0] == 0xE5) {
            // Compare the rest of the filename
            if (strncmp(&dir[i].DIR_Name[1], &formattedName[1], 10) == 0) {
                found = true;
                entry = &dir[i];
                entry_index = i;
                break;
            }
        }
    }

    if (!found) {
        printf("Error: Deleted file not found.\n");
        continue;
    }

    
    printf("Enter the original first character of the filename: ");
    char originalChar;
    scanf(" %c", &originalChar);
    getchar(); // Consume the newline character


    entry->DIR_Name[0] = toupper(originalChar);

    
    uint32_t cluster = (entry->DIR_FirstClusterHigh << 16) | entry->DIR_FirstClusterLow;
    cluster = le32toh(cluster); // Handle endianness

    if (cluster == 0) {
        printf("Error: Cannot undelete file with starting cluster 0.\n");
        continue;
    }

    uint32_t fileSize = le32toh(entry->DIR_FileSize);
    uint32_t clusterSize = BPB_BytesPerSec * BPB_SecPerClus;
    uint32_t clustersNeeded = (fileSize + clusterSize - 1) / clusterSize;

    uint32_t currentCluster = cluster;
    for (uint32_t i = 0; i < clustersNeeded; i++) {
        uint32_t nextCluster;

        if (i == clustersNeeded - 1) {
        
            nextCluster = 0x0FFFFFF8;
        } else {
            
            nextCluster = currentCluster + 1;
        }

        // Update FAT entries in all FAT copies
        for (uint32_t j = 0; j < BPB_NumFATs; j++) {
            uint32_t FAT_address = (BPB_BytesPerSec * BPB_RsvdSecCnt) +
                                   (j * BPB_FATSz32 * BPB_BytesPerSec) +
                                   (currentCluster * 4);
            fseek(fp, FAT_address, SEEK_SET);
            nextCluster = htole32(nextCluster); // Handle endianness
            fwrite(&nextCluster, 4, 1, fp);
        }

        currentCluster = le32toh(nextCluster); // Convert back to host order

        // Check if we've reached the maximum cluster number
        if (currentCluster >= totalClusters || currentCluster >= 0x0FFFFFF8) {
            break;
        }
    }

    // Write the updated directory entry back to the image
    fseek(fp, dir_entry_offsets[entry_index], SEEK_SET);
    fwrite(entry, sizeof(struct DirectoryEntry), 1, fp);

    printf("File '%s' undeleted successfully.\n", filename);
    }


  }
    return 0;
}


