#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "task1.h"
#include "task2.h"
#include "task3.h"
#include "task4.h"
#include "task5.h"

extern File *openFile(Volume *volume, Directory * directoryArray) {
    File *file = (File *)malloc(sizeof(File));
    file->volume = volume;
    file->directoryArray = directoryArray;
    return file;
}

extern off_t seekFile (File * file, off_t offset, int whence, int startCluster){
    offset += (file -> volume -> bootSector->BPB_NumFATs * file -> volume -> bootSector->BPB_FATSz16);
    offset *= file -> volume -> bootSector->BPB_BytsPerSec;
    offset += (file -> volume -> bootSector->BPB_RootEntCnt * sizeof(Directory));
    offset += (startCluster * file -> volume -> bootSector->BPB_SecPerClus * file -> volume -> bootSector->BPB_BytsPerSec);
    return offset;
}

extern size_t readFile(File *file, void *buffer, size_t length) {
    size_t bytesRead = 0;
    if (file == NULL || buffer == NULL || length == 0) {
        return bytesRead;
    }

    int fd = file->fd;
    
    bytesRead = read(fd, buffer, length);

    if (bytesRead == -1) {
        perror("Error reading file");
        bytesRead = 0; 
    }
    return bytesRead;
}

extern void closeFile(File *file) {
    free(file); 
}


off_t calculateSubdirectoryOffset(File * file, int clusterNumber) {
    int FirstDataSector = file -> volume -> bootSector->BPB_RsvdSecCnt + (file -> volume -> bootSector->BPB_NumFATs *  file -> volume -> bootSector->BPB_FATSz16) +  file -> volume -> bootSector->BPB_RootEntCnt / ( file -> volume -> bootSector->BPB_BytsPerSec / sizeof(Directory));
    int startingSector = FirstDataSector + (clusterNumber - 2) *  file -> volume -> bootSector->BPB_SecPerClus;
    return startingSector *  file -> volume -> bootSector->BPB_BytsPerSec;
}



char * getClusterBytes(File * file, int startCluster, size_t length, bool isSubDirectory){
    off_t data_offset;
    int clusterSize = file -> volume -> bootSector->BPB_SecPerClus * file -> volume -> bootSector->BPB_BytsPerSec;
    if (length > clusterSize){
        return NULL;
    }
    char* data_buffer = (char*) malloc(length + 1);

    if (isSubDirectory){
        data_offset = calculateSubdirectoryOffset(file, startCluster);
    }else{
        data_offset = seekFile(file, file->volume->bootSector->BPB_RsvdSecCnt, SEEK_SET, startCluster);
    }
    lseek(file->fd, data_offset, SEEK_SET);
    readFile(file, data_buffer, length);
    data_buffer[length] = '\0';
    return data_buffer;
}




int choiceRootDirectory(){
    int choice = -2;
    while (choice == -2){
        printf("-------------------------------------------------------------------------------------------------------------------\n");
        printf("Select a file\n");
        printf("Enter -1 to exit\n");
        printf("FOR EXAMPLE, IF YOU WANT TO READ THE SESSIONS.TXT FILE, enter the directory number 18\n");
        printf("-------------------------------------------------------------------------------------------------------------------\n");
        printf("Your choice: ");
        scanf("%d", &choice);
    }
    return choice;
}
int choiceSubDirectory(){
    int choice = -2;
    while (choice == -2){
        printf("-------------------------------------------------------------------------------------------------------------------\n");
        printf("YOU HAVE OPEN A FOLDER!!!");
        printf("Select a file\n");
        printf("Select -1 to go back to ROOT DIRECTORY\n");
        printf("FOR EXAMPLE, IF YOU WANT TO READ THE MIDDLEMATCH.TXT, ENTER THE DIRECTORY NUMBER 7\n");
        printf("-------------------------------------------------------------------------------------------------------------------\n");
        printf("Your choice: ");
        scanf("%d", &choice);
    }
    return choice;
}

void displayFileContent(File * file, size_t length, int clusterSize, int startCluster, bool isSubDirectory){
    size_t bytesRemaining = length; 
    while (startCluster < 0xfff8 && bytesRemaining > 0){
        char *file_contents;

        if (bytesRemaining >= clusterSize) {
            file_contents = getClusterBytes(file, startCluster, clusterSize, isSubDirectory);
            bytesRemaining -= clusterSize;

        } else {
            file_contents = getClusterBytes(file, startCluster, bytesRemaining, isSubDirectory);
            bytesRemaining = 0; 
        }
        printf("%s", file_contents); 
        free(file_contents);
        startCluster = fat_array[startCluster];
    }
}

void displaySubDirectoryFileContent(File * file, Directory * parentDirectoryArray, int directoryIndex, int choice, bool isSubDirectory){
    int startingCluster = getStartingCluster(parentDirectoryArray[directoryIndex].DIR_FstClusLO, parentDirectoryArray[directoryIndex].DIR_FstClusHI);
    Directory *subdirectoryArray = loadSubdirectory(file->fd, file->volume->bootSector, startingCluster);
    Directory sub = subdirectoryArray[choice];

    int fileSize = sub.DIR_FileSize;
    int clusterSize = file->volume->bootSector->BPB_SecPerClus * file->volume->bootSector->BPB_BytsPerSec;
    int startCluster = getStartingCluster(sub.DIR_FstClusLO, sub.DIR_FstClusHI);   
    printf("\n\n\n\n\n");

    if (startCluster > 0 && startCluster <= 0xfff8){
        displayFileContent(file, fileSize, clusterSize, startCluster, isSubDirectory);
        printf("\n");
    }
    
    if (sub.DIR_Attr == 0x10){
        Directory *subdirectoryArray = loadSubdirectory(file->fd, file->volume->bootSector, startingCluster);
        displaySubDirectory(file->volume->bootSector, subdirectoryArray, choice);
        int choiceSub = choiceSubDirectory();
        if (choiceSub == -1){
            return;
        }
        displaySubDirectoryFileContent(file, subdirectoryArray, choice, choiceSub, isSubDirectory);
    }
}


void displayRootDirectoryContent(File * file, int choice) {
    bool isSubDirectory = false; 
    Directory dir = file -> directoryArray[choice];
    if (dir.DIR_Attr == 0x10){
        displaySubDirectory(file -> volume -> bootSector, file -> directoryArray, choice);

        isSubDirectory = true; 
        int choiceSub = choiceSubDirectory();
        if (choiceSub == -1){
            return;
        }
        displaySubDirectoryFileContent(file, file -> directoryArray, choice, choiceSub, isSubDirectory);
    }
    size_t fileSize = dir.DIR_FileSize;
    int clusterSize = file -> volume -> bootSector->BPB_SecPerClus * file -> volume -> bootSector->BPB_BytsPerSec;
    int startCluster = getStartingCluster(dir.DIR_FstClusLO, dir.DIR_FstClusHI) - 2;
    if (startCluster > 0 && startCluster <= 0xfff8){
        displayFileContent(file, fileSize, clusterSize, startCluster, isSubDirectory);
        printf("\n");
    }
}


