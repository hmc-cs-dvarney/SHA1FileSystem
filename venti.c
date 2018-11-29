/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

	venti.c edited by Dalton Varney and Daniel Diemer Spring 2018 File Systems
	ASSIGNMENT 3: VENTI FILE SYSTEM complete

	sources:
	stackoverflow occasionally (just for c questions... haven't used it since 105)

  gcc -Wall `pkg-config fuse --cflags --libs` venti.c -o venti -lcrypto
*/


#define FUSE_USE_VERSION 26
#define VENTI_DISK_SIZE 100000
#define BLOCK_SIZE 4096
#define DIRECTORY_SIZE 44
#define FILE_NAME_SIZE 32
#define PATH_NAME_SIZE 1024
#define HASH_SIZE 20


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <openssl/sha.h>



struct venti_entry {
	//four fields
	char name[FILE_NAME_SIZE];
	size_t fileType;
	size_t fileSize;
	time_t aTime;
	int firstBlock;
};

struct venti_superblock {
	//6 fields
	//version number
	size_t system_size;
	size_t system_block_size;
	struct venti_entry rootDirectory;
	//pointer to the free list

} venti_superblock;


struct indexEntry {

	char fingerprint[HASH_SIZE*4];
	size_t finalGrub;

} indexEntry;

union Superblock {
    struct venti_superblock s;
    char		pad[BLOCK_SIZE];
} superblock;

union Superblock theBlock[1];
int theVENTI[VENTI_DISK_SIZE/BLOCK_SIZE];

struct indexEntry theINDEX[VENTI_DISK_SIZE/BLOCK_SIZE];


FILE *fp;

char* tempHash(const char* path){
	//call sha1
	int i;
	unsigned char temp[SHA_DIGEST_LENGTH];
	char buf[SHA_DIGEST_LENGTH*2];

	memset(buf, 0x0, SHA_DIGEST_LENGTH*2);
	memset(temp, 0x0, SHA_DIGEST_LENGTH);

	SHA1((unsigned char *)path, strlen(path), temp);

	for (i=0; i < HASH_SIZE; i++) {
		sprintf((char*)&(buf[i*2]), "%02x", temp[i]);
	}

	printf("\n\nUnique Fingerprint for %s: %s\n", path, buf);

	static char hash[SHA_DIGEST_LENGTH*2];
	strcpy(hash, buf);
	return hash;

}

void* venti_init(struct fuse_conn_info *conn)
{
	//Check for Superblock??
	//If no superblock, make a new one, and new table and new
	//Get file length

	char curr_dir[PATH_NAME_SIZE];
	getcwd(curr_dir, sizeof(curr_dir));
	size_t ventiExists;
	char* diskName;
	diskName = strcat(curr_dir, "/venti_disk");
	if (access(diskName, F_OK) != -1){
		ventiExists = 1;
		fp = fopen(diskName, "r+");
	}
	else{
		fp = fopen(diskName, "w+");
		ventiExists = 0;
	}


	size_t superblock_write_successes = fread(theBlock, BLOCK_SIZE, 1, fp);
	//printf("superblock_write_successes: %lu\n", superblock_write_successes);

	if (!ventiExists){
		printf("INITIALIZE DISK\n");
		//Write this to file
		//make root directory
		theVENTI[0] = 0;
		theVENTI[1] = 0;
		theVENTI[2] = 0;
		theVENTI[3] = 0;
		size_t j;
		for(j = 3; j < VENTI_DISK_SIZE/BLOCK_SIZE; theVENTI[++j] = -1);
		struct venti_entry newDirectory;
		strcpy(newDirectory.name, "/");
		newDirectory.fileSize = 1;
		newDirectory.firstBlock = 3;
		// 0 denotes the file is a directory
		newDirectory.fileType = 0;


		(theBlock->s).system_block_size = BLOCK_SIZE;
		(theBlock->s).rootDirectory = newDirectory;
		(theBlock->s).system_size = 3;

		char* fingerprint;
		fingerprint = tempHash("/");
		struct indexEntry rootIndexEntry;
		struct indexEntry finalEntry;
		strcpy(rootIndexEntry.fingerprint, fingerprint);
		//free(fingerprint);
		strcpy(finalEntry.fingerprint, "FinalEntryInTheIndex");

		struct indexEntry systemEntry;
		strcpy(systemEntry.fingerprint, "DONOTUSE!!!!!!!!!!!!");
		systemEntry.finalGrub = 0;
		finalEntry.finalGrub = 0;
		//initialize the hash index
		theINDEX[0] = systemEntry;
		theINDEX[1] = systemEntry;
		theINDEX[2] = systemEntry;
		theINDEX[3] = rootIndexEntry;
		theINDEX[4] = finalEntry;

		fseek(fp, 0, SEEK_SET);
		//Write the superblock at block 0
		size_t successSuper = fwrite(theBlock, BLOCK_SIZE, 1, fp);
		if (successSuper == 0){
			return (void *)-ENOENT;
		}
		//write the VENTI at block 1
		size_t successVENTI = fwrite(theVENTI, BLOCK_SIZE, 1, fp);
		if (successVENTI == 0){
			return (void *)-ENOENT;
		}

		//write the INDEX at block 2
		size_t successINDEX = fwrite(theINDEX, BLOCK_SIZE, 1, fp);
		if (successINDEX == 0){
			return (void *)-ENOENT;
		}

		struct venti_entry directoryList[BLOCK_SIZE /DIRECTORY_SIZE];
		struct venti_entry tempEntry;
		strcpy(tempEntry.name, "~!");
		directoryList[0] = tempEntry;
		fwrite(directoryList, BLOCK_SIZE, 1, fp);


		fseek(fp, VENTI_DISK_SIZE - ((theBlock->s).system_size * BLOCK_SIZE), SEEK_SET);
		fputc('\0', fp);
		fflush(fp);
	}
	else {
		printf("IMPORTING EXISTING DISK");
		fseek(fp, 0, SEEK_SET);
		fseek(fp, BLOCK_SIZE, SEEK_SET);
		fread(theVENTI, BLOCK_SIZE, 1, fp);
		fread(theINDEX, BLOCK_SIZE, 1, fp);
	}


	return NULL;
}

char* getCleanName(const char* path){
	char* splitPath;
	splitPath = (char*) malloc(sizeof(char) * PATH_NAME_SIZE);
	strcpy(splitPath, path);
	char* tempName;

	char* newDirName;
	newDirName = (char*) malloc(sizeof(char) * PATH_NAME_SIZE);
	//gets the name of the new directory from the input path
	while((tempName = strsep(&splitPath, "/")) != NULL){
		strcpy(newDirName, tempName);
	}

	free(splitPath);
	return newDirName;

}

int findDir(const char *dirName)
{
	char* fingerprint;
	fingerprint = tempHash(dirName);
	size_t i;
	size_t currentBlock;
	currentBlock = 0;

	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE*2, SEEK_SET);
	fread(theINDEX, BLOCK_SIZE, 1, fp);
	printf("\nSearching for matching fingerprint...\n");
	for(i = 0; i < VENTI_DISK_SIZE/BLOCK_SIZE; ++i){

		if (strcmp(theINDEX[i].fingerprint, "FinalEntryInTheIndex") == 0){

			currentBlock = 0;
			break;
		}
		if(strcmp(theINDEX[i].fingerprint, "DONOTUSE!!!!!!!!!!!!") != 0){
			printf("\nPossible Match: %s\n", theINDEX[i].fingerprint);
		}
		if (strcmp(theINDEX[i].fingerprint, fingerprint) == 0){
			currentBlock = i;
			printf("\nFingerprint found...\n\n");
			break;
		}
	}
	int res;
	res = currentBlock;
	//free(fingerprint);
	return res;
}

time_t findTime(const char* path){
	char* fileName;
	fileName = getCleanName(path);
	size_t inputPath_length = (strlen(path)-strlen(fileName));
	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';

	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(inputPath);
	if (entryStorage == 0){
		free(inputPath);
		free(fileName);
		return 2;
	}
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];

	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	size_t k;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		// printf("fileNAME IS: %s\n", fileName);
		// printf("Current Entry: %s\n", directoryList[k].name);
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		if (strcmp(directoryList[k].name, fileName) == 0) {
			free(inputPath);
			free(fileName);
			return directoryList[k].aTime;
		}
	}
	free(inputPath);
	free(fileName);
	return time(NULL);

}

int isFile(const char* path)
{
	char* fileName;
	fileName = getCleanName(path);
	size_t inputPath_length = (strlen(path)-strlen(fileName));
	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';

	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(inputPath);
	if (entryStorage == 0){
		free(inputPath);
		free(fileName);
		return 2;
	}
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];

	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	size_t k;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		// printf("fileNAME IS: %s\n", fileName);
		// printf("Current Entry: %s\n", directoryList[k].name);
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		if (strcmp(directoryList[k].name, fileName) == 0) {
			free(inputPath);
			free(fileName);
			return directoryList[k].fileType;
		}
	}
	free(inputPath);
	free(fileName);
	return 2;

}

int findFileSize(const char* path)
{
	char* fileName;
	fileName = getCleanName(path);
	size_t inputPath_length = (strlen(path)-strlen(fileName));
	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';

	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(inputPath);
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];

	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	size_t k;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		if (strcmp(directoryList[k].name, fileName) == 0) {
			free(inputPath);
			free(fileName);
			return directoryList[k].fileSize;
		}
	}
	free(inputPath);
	free(fileName);
	return 1;

}

void setFileSize(const char* path, int size)
{
	char* fileName;
	fileName = getCleanName(path);
	size_t inputPath_length = (strlen(path)-strlen(fileName));
	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';

	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(inputPath);
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];

	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	size_t k;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		if (strcmp(directoryList[k].name, fileName) == 0) {
			directoryList[k].fileSize = size;
		}
	}
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	size_t successDirList = fwrite(directoryList, BLOCK_SIZE, 1, fp);
	if (!successDirList){
		printf("WARNING WARNING WARNING SIGN");
	}
	fflush(fp);
	free(inputPath);
	free(fileName);
}

static int venti_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	if (strcmp(path,"/") == 0){
		stbuf->st_mode = S_IFDIR | 0777;
		stbuf->st_nlink = 1;
		return res;
	}

	size_t fileType;
	fileType = isFile(path);
	stbuf->st_atime = findTime(path);
	stbuf->st_mtime = findTime(path);

	if(fileType == 0){
		stbuf->st_mode  = S_IFDIR | 0777;
		stbuf->st_nlink = 1;
	}
	else if (fileType == 1){
		stbuf->st_mode  = S_IFREG | 0777;
		stbuf->st_nlink = 1;
		stbuf->st_size  = findFileSize(path);
	}


	else {
		res = -ENOENT;
	}

	return res;
}

static int venti_fgetattr(const char *path, struct stat *stbuf)
{
	int attrReturn;
	attrReturn = venti_getattr(path, stbuf);
	return attrReturn;
}

int findFree(const char* path) {
	//iterate through the venti looking for empty blocks
	size_t i;
	size_t freeBlock;
	for(i = 0; i < sizeof(theVENTI); ++i) {
		if(theVENTI[i] == -1) {
			//if we find one, set it to singluar block size and rewrite
			theVENTI[i] = 0;

			fseek(fp, 0, SEEK_SET);
			fseek(fp, BLOCK_SIZE, SEEK_SET);
			size_t successVENTI = fwrite(theVENTI, BLOCK_SIZE, 1, fp);
			if (!successVENTI){
				printf("WARNING WARNING WARNING SIGN");
			}
			fflush(fp);
			freeBlock = i;
			break;
		}
	}
	char* fingerprint;
	fingerprint = tempHash(path);

	struct indexEntry tempIndexEntry;
	strcpy(tempIndexEntry.fingerprint, fingerprint);
	tempIndexEntry.finalGrub = 0;

	struct indexEntry finalEntry;
	strcpy(finalEntry.fingerprint, "FinalEntryInTheIndex");
	finalEntry.finalGrub = 0;

	theINDEX[freeBlock] = tempIndexEntry;
	size_t finalMarker;
	finalMarker = freeBlock + 1;
	theINDEX[finalMarker] = finalEntry;
	// printf("FIND FREE FINAL MARKER: %lu", finalMarker);

	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE*2, SEEK_SET);
	size_t successINDEX = fwrite(theINDEX, BLOCK_SIZE, 1, fp);
	if (!successINDEX){
		printf("WARNING WARNING WARNING SIGN");
	}
	fflush(fp);

	//free(fingerprint);

	return freeBlock;
}

static int venti_mkdir(const char *path, mode_t mode)
{
	//find the new directory name and separate name and path
	char* newDirName;
	newDirName = getCleanName(path);

	size_t inputPath_length = (strlen(path)-strlen(newDirName));
	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	// subtract new dir name from input path for finddir
	//char inputPath[PATH_NAME_SIZE];
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';

	//make our new directory entry
	struct venti_entry newDirectory;
	strcpy(newDirectory.name, newDirName);
	newDirectory.fileSize = 1;
	newDirectory.aTime = time(NULL);
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(inputPath);
	// printf("ENTRY STORAGE FOR PARENT: %lu\n", entryStorage);

	size_t newBlock = findFree(path);
	// printf("STORAGE FOR NEW DIRECTORY IS: %lu\n", newBlock);

	newDirectory.firstBlock = newBlock;
	newDirectory.fileType = 0;

	//seek to proper data block then write new directory entry
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];
	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	// find the size of the directoryList and put the entry after that
	struct venti_entry tempEntry;
	size_t k;
	size_t index;
	index = 0;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		++index;
	}


	// We need an ending entry so that we know that we are at the end of the list
	strcpy(tempEntry.name,"~!");
	directoryList[index] = newDirectory;
	directoryList[index+1] = tempEntry;
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	size_t successMkdir = fwrite(directoryList, BLOCK_SIZE, 1, fp);
	if (!successMkdir){
		printf("WARNING WARNING WARNING SIGN");
	}
	fflush(fp);
	((theBlock->s).system_size) += 1;

	// make our . and .. entries for each new directory
	struct venti_entry dot;
	struct venti_entry dotdot;
	struct venti_entry endDirectoryList;
	strcpy(dot.name, ".");
	strcpy(dotdot.name, "..");
	strcpy(endDirectoryList.name, "~!");
	dot.fileSize = 1;
	dotdot.fileSize = 1;
	dot.firstBlock = newBlock;
	dotdot.firstBlock = entryStorage;
	dot.fileType = 0;
	dotdot.fileType = 0;

  // put . and .. into directoryList for new directory
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * newBlock, SEEK_SET);
	struct venti_entry newDirectoryList[BLOCK_SIZE / DIRECTORY_SIZE];
	newDirectoryList[0] = dot;
	newDirectoryList[1] = dotdot;
	newDirectoryList[2] = endDirectoryList;
	size_t successMkdot = fwrite(newDirectoryList, BLOCK_SIZE, 1, fp);
	if (!successMkdot){
		printf("WARNING WARNING WARNING SIGN");
	}

	fflush(fp);

	free(inputPath);
	free(newDirName);

	return 0;
}

static int venti_access(const char *path, int mask)
{
	//split name from PATH
	//input path into find dir
	// check for name in block returned by dir

	char* newDirName;
	newDirName = getCleanName(path);
	//printf("NEWDIRNAME PRIOR IS: %p\n", newDirName);

	//find the size of the input path so we know how much to allocate
	// We had trouble with statically allocating c arrays, so we just malloc'd
	size_t inputPath_length = (strlen(path)-strlen(newDirName));

	if (inputPath_length > 1){
		inputPath_length -= 1;
	}
	// subtract new dir name from input path for finddir
	//char inputPath[PATH_NAME_SIZE];
	char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);

	// printf("INPUTPATH POST malloc IS: %p\n", inputPath);
	// printf("NEWDIRNAME IS: %p\n", newDirName);
	// subtract new dir name from input path for finddir
	size_t t;
	for (t=0; t < inputPath_length; ++t){
		inputPath[t] = path[t];
	}
	inputPath[t] = '\0';


	size_t findDirSuccess;
	// we use findDir to find the data block for the given path
	findDirSuccess = findDir(inputPath);
	free(newDirName);
	free(inputPath);
	if (findDirSuccess){
		return 0;
	}

	return -ENOENT;
}

static int venti_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	(void) offset;
	(void) fi;
	//iterate through directories, put them into filler.
	// iteration starts where offset is given
	// check if offset is greater than directory List size, and return 0 if so.
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);
	// printf("READDIR WHERE ARE WE READING FROM: %lu\n", entryStorage);
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];
	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	//we need to stop the loop based on the size of directoryList
	// everytime you call mkdir, you overwrite the last one
	// how do we show files with an ls along with directories
	size_t t;
	size_t index;
	index = 0;
	for(t = 0; t < BLOCK_SIZE / DIRECTORY_SIZE; t++) {
		if (strcmp(directoryList[t].name,"~!") == 0){
			break;
		}
		++index;
	}
	// if offset is greater than the amount of elements don't do anything else!
	if (offset > (index-2)){
		return 0;
	}
	//put our entries into filler if they are greater than offset
	size_t i;
	for(i = 0; i < BLOCK_SIZE / DIRECTORY_SIZE; i++) {

		if (i >= offset){
			if (strcmp(directoryList[i].name,"~!") == 0){
				return 0;
			}
			// struct FUSE_STAT fuse_stat;
			// memset(&fuse_stat, 0, sizeof(fuse_stat));
			// fuse_stat.st_mode = S_IFREG;
			// printf("TIME IS: %f\n", time(NULL));
			// fuse_stat.st_atim.tv_sec = time(NULL);
			// fuse_stat.st_mtim.tv_sec = time(NULL);
			// fuse_stat.st_ctim.tv_sec = time(NULL);
			if(!directoryList[i].fileType) {
				filler(buf, directoryList[i].name, NULL, i);
			}
			else{
				filler(buf, directoryList[i].name, NULL, i);
			}
		}
	}

	return 0;
}


// get an I/O error when we mknod
// still creates it b/c if we try to mknod again it says file exists
static int venti_mknod(const char* path, mode_t mode, dev_t rdev)
{
	if (!S_ISREG(mode)){
		struct venti_entry newFile;
		newFile.fileType = 1;

		char* newFileName;
	 	newFileName = getCleanName(path);
		strcpy(newFile.name, newFileName);
		size_t inputPath_length = (strlen(path)-strlen(newFile.name));
		if (inputPath_length > 1){
			inputPath_length -= 1;
		}
		char* inputPath = (char*) malloc(sizeof(char) * inputPath_length);
		// subtract new dir name from input path for finddir
		size_t t;
		for (t=0; t < inputPath_length; ++t){
			inputPath[t] = path[t];
		}
		inputPath[t] = '\0';

		size_t entryStorage;
		//find where to put it
		entryStorage = findDir(inputPath);
		//read in block with directoryList
		fseek(fp, 0, SEEK_SET);
		fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
		struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];
		size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
		if (!block_read_success){
			printf("WARNING WARNING WARNING SIGN");
		}

		size_t newBlock = findFree(path);

		newFile.firstBlock = newBlock;
		newFile.fileSize   = 0;
		newFile.aTime = time(NULL);

		// find the size of the directoryList and put the entry after that
		struct venti_entry tempEntry;
		size_t k;
		size_t index;
		index = 0;
		for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
			if (strcmp(directoryList[k].name,"~!") == 0){
				break;
			}
			++index;
		}


		// We need an ending entry so that we know that we are at the end of the list
		strcpy(tempEntry.name,"~!");
		directoryList[index] = newFile;
		directoryList[index+1] = tempEntry;
		fseek(fp, 0, SEEK_SET);
		fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
		size_t successMkdir = fwrite(directoryList, BLOCK_SIZE, 1, fp);
		if (!successMkdir){
			printf("WARNING WARNING WARNING SIGN");
		}
		fflush(fp);
		((theBlock->s).system_size) += 1;

		free(inputPath);
		free(newFileName);
		return 0;


	}
	return -EACCES;



}

// warning for incompatible pointer type
static int venti_create(const char* path, mode_t mode){
	int birdUp;
	birdUp = venti_mknod(path, S_IFREG + mode, 'p');
	return birdUp;
}

// inputting into buffer and can print in gdb but nothing shows up in console
static int venti_read(const char* path, char *buf, size_t size, off_t offset,
	 struct fuse_file_info* fi)
{
	size = findFileSize(path);
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);
	//read in block with directoryList

	if (offset > BLOCK_SIZE){
		if (theVENTI[entryStorage] == 0){
			return 0;
		}
		else{
			entryStorage = theVENTI[entryStorage];
			offset -= BLOCK_SIZE;
		}

	}
	fseek(fp, 0, SEEK_SET);
	fseek(fp, (BLOCK_SIZE * entryStorage) + offset, SEEK_SET);
	size_t block_read_success = fread(buf, 1, size, fp);
	return block_read_success;

}

static int venti_statfs(const char* path, struct statvfs* stbuf)
{
	stbuf->f_bsize   = BLOCK_SIZE;
	stbuf->f_blocks  = VENTI_DISK_SIZE / BLOCK_SIZE;
	stbuf->f_bfree   = (VENTI_DISK_SIZE / BLOCK_SIZE) - ((theBlock->s).system_size);
	stbuf->f_namemax = FILE_NAME_SIZE;
	stbuf->f_bavail  = (VENTI_DISK_SIZE / BLOCK_SIZE) - ((theBlock->s).system_size);

	return 0;
}

static int venti_release(const char* path, struct fuse_file_info* fi)
{
	//ALL OF OUR DATA STRUCTURES ARE FREED IN THEIR RESPECTIVE FUNCTIONS
	return 0;
}

static int venti_write(const char* path, const char *buf, size_t size, off_t offset,
	 struct fuse_file_info* fi)
{
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);
	//read in block with directoryList


	if(findFileSize(path) < (offset + size)) {
		setFileSize(path, (offset + size));
	}
	fseek(fp, 0, SEEK_SET);
	fseek(fp, (BLOCK_SIZE * entryStorage) + offset, SEEK_SET);
	size_t block_write_success = fwrite(buf, 1, size, fp);
	size_t block_write_null = fwrite("\0", 1, 1, fp);
	fseek(fp, 0, SEEK_SET);
	fseek(fp, (BLOCK_SIZE * entryStorage) + offset, SEEK_SET);
	char text[10];
	size_t block_read_success = fread(text, 1, size, fp);
	if(!block_write_success || !block_write_null) {
		printf("PROBLEEEEMMMMM???\n");
	}

	fflush(fp);
	return block_write_success;
}

static int venti_truncate(const char* path, off_t size){
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage + size, SEEK_SET);
	size_t block_read_success = fwrite("\0", 1, 1, fp);

	if(!block_read_success) {
		printf("PROBLEEEEMMMMM???\n");
	}
	fflush(fp);
	return 0;
}

static int venti_open(const char* path, struct fuse_file_info* fi)
{
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);

	if(entryStorage) {
		return 0;
	}
	return -ENOENT;
}


static int venti_rmdir(const char* path){
	//findDir, if empty, remove it?
	size_t entryStorage;
	//find where to put it
	entryStorage = findDir(path);
	//read in block with directoryList
	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];
	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	// find the size of the directoryList and put the entry after that
	size_t k;
	size_t index;
	index = 0;
	for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
		if (strcmp(directoryList[k].name,"~!") == 0){
			break;
		}
		++index;
	}

	if (index < 3){
		// We need an ending entry so that we know that we are at the end of the list
		size_t parentStorage;
		parentStorage = directoryList[1].firstBlock;
		fseek(fp, 0, SEEK_SET);
		fseek(fp, BLOCK_SIZE * parentStorage, SEEK_SET);
		size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);
		if (!block_read_success){
			printf("WARNING WARNING WARNING SIGN");
		}

		char* cleanName;
		cleanName = getCleanName(path);
		size_t k;
		size_t downsize = 0;
		for(k = 0; k < BLOCK_SIZE / DIRECTORY_SIZE; k++) {
			if (downsize){
				directoryList[k-1] = directoryList[k];
			}
			if (strcmp(directoryList[k].name, cleanName) == 0){
				downsize = 1;
			}
		}

		fseek(fp, 0, SEEK_SET);
		fseek(fp, BLOCK_SIZE * parentStorage, SEEK_SET);
		size_t successMkdir = fwrite(directoryList, BLOCK_SIZE, 1, fp);
		if (!successMkdir){
			printf("WARNING WARNING WARNING SIGN");
		}
		fflush(fp);
		((theBlock->s).system_size) -= 1;
		free(cleanName);
		return 0;
	}
	return -EACCES;


}

// to is real file
// from is symlink
static int venti_symlink(const char* to, const char* from)
{
	size_t entryStorage;
	entryStorage = findDir(to);

	mode_t mode;
	mode = 0777;

	venti_mknod(from, mode, 'p');

	size_t symStorage;
	symStorage = findDir(from);

	struct venti_entry pointerToTo;
	pointerToTo.firstBlock = entryStorage;
	pointerToTo.fileSize = 1;
	pointerToTo.fileType = 1;
	strcpy(pointerToTo.name,"linkHHHH");

	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * symStorage, SEEK_SET);
	struct venti_entry newDirectoryList[BLOCK_SIZE / DIRECTORY_SIZE];

	newDirectoryList[0] = pointerToTo;
	size_t successLink = fwrite(newDirectoryList, BLOCK_SIZE, 1, fp);

	if(!successLink) {
		printf("PROBLEM\n");
		return -ENOENT;
	}

	fflush(fp);
	return 0;
}

static int venti_readlink(const char* path, char* buf, size_t size)
{
	size_t entryStorage;
	entryStorage = findDir(path);

	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * entryStorage, SEEK_SET);
	struct venti_entry directoryList[BLOCK_SIZE / DIRECTORY_SIZE];
	size_t block_read_success = fread(directoryList, BLOCK_SIZE, 1, fp);

	if (!block_read_success){
		printf("WARNING WARNING WARNING SIGN");
	}

	size_t linkStorage;
	linkStorage = directoryList[0].firstBlock;

	fseek(fp, 0, SEEK_SET);
	fseek(fp, BLOCK_SIZE * linkStorage, SEEK_SET);

	int linkSuccess = fread(buf, 1, size, fp);

	if(linkSuccess != size) {
		return -EACCES;
	}

	return linkSuccess;

}

static int venti_unlink(const char* path)
{
	venti_rmdir(path);
	return 0;
}




static struct fuse_operations venti_oper = {
	.init     = venti_init,
	.getattr	= venti_getattr,
	.mkdir    = venti_mkdir,
	.access   = venti_access,
	.write    = venti_write,
	.readdir	= venti_readdir,
	.mknod    = venti_mknod,
	.rmdir    = venti_rmdir,
	.read     = venti_read,
	.open     = venti_open,
	.truncate = venti_truncate,
	.release  = venti_release,
	.statfs   = venti_statfs,
	.symlink  = venti_symlink,
	.readlink = venti_readlink,
	.unlink   = venti_unlink,
	.fgetattr = venti_fgetattr,
	.create   = venti_create,
};

int main(int argc, char *argv[])
{
	//venti_init();
	return fuse_main(argc, argv, &venti_oper, NULL);
}
