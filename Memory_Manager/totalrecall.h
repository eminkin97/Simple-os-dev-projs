#ifndef TOTALRECALL_H_
#define TOTALRECALL_H_

#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include<stddef.h>
#include<sys/mman.h>
#include<unistd.h>
#include<signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define malloc(x) myallocate(x,__FILE__,__LINE__,THREADREQ)
#define free(x) mydeallocate(x,__FILE__,__LINE__,THREADREQ)

#define THREADREQ 8888
#define LIBRARYREQ 9999

//these nodes are used for the meta-data in a given page
struct AllocNodeN {

	short inUse;
	int capacity;
	struct AllocNodeN* prev;
	struct AllocNodeN* next;
	short signature;

};

typedef struct AllocNodeN AllocNode;

//these nodes hold the meta-data for pages in memory
//they hold a threadid
//their offset from the kernal space
//and a return address to indicate where it should be swapped back into
//in case it is ever swapped out
struct PageNodeN {

	int threadid;
	int offset_p;
	int retAddr;
	char reference_bit;

};

typedef struct PageNodeN PageNode;

//this global variable is ued to tell which thread is allocating
////memory at a given time
int currThread;

void* myallocate(size_t size , char* file , int line , int reqType);
void mydeallocate(void* ptr , char* file , int line , int reqType);
void protectPages(int, int);
void* shalloc(size_t size);

#endif //TOTALRECALL_H_




