#include"totalrecall.h"

//this is where we store the 8MB of data
void* MEMSPACE;

//these are pointers to the beginning of the data
//the end of special space for the kernal
//and the end of all the data
char* beg;
char* kernalEnd;
char* schedulerEnd;
char* end;
char* thread_pages_count_end;
char* shallocEnd;
char* trueEnd;

//current position in free list
PageNode* free_list_position;

//this is the file pointer for the swapfile
int swapFP;

//this flag indicates whether or not threads have started to allocate data
short start = 0;

//this indicates how large pages are on the machine
int PAGE_SIZE;

//this global variable is ued to tell which thread is allocating
////memory at a given time
int currThread = 1;

//Total memory left is kept track of in the variable pagesLeft
int pagesLeft = 5500;

static int swapFileGrab(int offset_out, int offset_in) {
	printf("SWAPPING FROM SWAP FILE\n");
	//printf("offset_out: %d , offset_in: %d\n" , offset_out , offset_in);
	//offset_in is the offset in swapfile to read from
	char temp[PAGE_SIZE];
	int result = lseek(swapFP , offset_in * PAGE_SIZE , SEEK_SET);	//move to place where we want to read from in swap file

	//printf("*************************************************result: %d\n" , result);
	if(result == -1) {
		printf("ERROR WITH LSEEK\n");
		exit(1);
	}

	int read_ret_val = read(swapFP, temp, PAGE_SIZE);	//read data from swap file into buffer temp
	if(read_ret_val == -1) {
		printf("ERROR READING FROM SWAPFILE\n");
		exit(1);
	}

	//page that we are evicting out
	void *page_out = (schedulerEnd + (offset_out * PAGE_SIZE));

	if (mprotect(page_out, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
		//unprotects page that's being swapped out
		printf("ERROR mprotecting page at line %d\n", __LINE__);
		exit(1);
	}


	
	int result2 = lseek(swapFP , offset_in * PAGE_SIZE , SEEK_SET);	//move to place where we want to read from in swap file
	if(result2 == -1) {
		printf("ERROR WITH LSEEK\n");
		exit(1);
	}

	//printf("PAGE_OUT %c\n", *((char *)page_out + 35));
	int write_ret_val = write(swapFP, page_out, PAGE_SIZE);
	//printf("BYTES WRITTEN %d\n", write_ret_val);

	if(write_ret_val == -1) {
		printf("ERROR WRITING TO SWAPFILE\n");
		perror(strerror(errno));
		exit(1);
	}

	//printf("TEMP %c\n", *(temp + 35));
	//copy what we got from swapfile to take place of page that was evicted
	memcpy(page_out, temp, PAGE_SIZE);	

	//update the page nodes
	PageNode *ptr_swapped_in = ((PageNode *)MEMSPACE) + offset_out;	//pagenode for page that was swapped in
	PageNode *ptr_swapped_out = (((PageNode *)MEMSPACE) + 1600) + offset_in;	//pagenode for page that was swapped out

	ptr_swapped_out -> retAddr = ptr_swapped_in -> offset_p;

	int temp2 = ptr_swapped_out -> threadid;
	ptr_swapped_out -> threadid = ptr_swapped_in -> threadid;

	ptr_swapped_in -> retAddr = -1;		//one we put in is at its current place
	ptr_swapped_in -> threadid = temp2;

	return 1;
}

int swapPage(int offset_out, int offset_in, int type) {
	printf("SWAPPING PAGE HAS BEGUN\n");
	printf("out: %d , in: %d\n" , offset_out , offset_in);
	char temp[PAGE_SIZE];
	void *page_out = (schedulerEnd + (offset_out * PAGE_SIZE));

	if (mprotect(page_out, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
		//unprotects page that's being swapped out
		printf("ERROR mprotecting page at line %d\n", __LINE__);
		exit(1);
	}

	void *page_in = (schedulerEnd + (offset_in * PAGE_SIZE));

	if (mprotect(page_in, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
		//unprotects page that's being swapped in
		printf("ERROR mprotecting page at line %d\n", __LINE__);
		exit(1);
	}

	memcpy(temp, page_out, PAGE_SIZE);	//copy page that's being swapped out to temp variable
	memcpy(page_out, page_in, PAGE_SIZE);	//copy page that's being swapped in into right place
	memcpy(page_in, temp, PAGE_SIZE);	//copy page that was swapped out into where page_in was, SWAP COMPLETED

	PageNode *ptr_swapped_in = ((PageNode *)MEMSPACE) + offset_out;	//pagenode for page that was swapped in
	PageNode *ptr_swapped_out = ((PageNode *)MEMSPACE) + offset_in;	//pagenode for page that was swapped out

	int temp1 = ptr_swapped_out -> threadid;
	int temp2 = ptr_swapped_out -> reference_bit;
	int temp3 = ptr_swapped_out -> retAddr;

	ptr_swapped_out -> threadid = ptr_swapped_in -> threadid;
	ptr_swapped_out -> reference_bit = ptr_swapped_in -> reference_bit;
	ptr_swapped_out -> retAddr = ptr_swapped_in -> offset_p;

	if (type == 1) {
		ptr_swapped_in -> retAddr = temp3;		//one we put in is at its current place
	} else {
		ptr_swapped_in -> retAddr = -1;		//one we put in is at its current place
	}
	ptr_swapped_in -> threadid = temp1;
	ptr_swapped_in -> reference_bit = temp2;

	if (mprotect(page_in, PAGE_SIZE, PROT_NONE) == -1) {		//Protect page that was swapped out
		printf("ERROR mprotecting page at line %d\n", __LINE__);
		exit(1);
	}

	return 1;
}

int evictPage() {
	//evicts page using 2nd change algo
	printf("CHOOSING VICTIM TO EVICT\n");
	while (1) {
		if (free_list_position->threadid == currThread) {
			//skip over ones for current executing thread
			if (free_list_position->offset_p == 1599) {
				free_list_position = (PageNode *)MEMSPACE;
			} else {
				free_list_position++;
			}
		}
		if (free_list_position->reference_bit == 1) {
			//reset reference bit if reference bit is 1
			free_list_position->reference_bit = 0;
		} else {
			//if reference bit is 0, found victim
			PageNode* evict = free_list_position;	//one to evict

			if (free_list_position->offset_p == 1599) {
				free_list_position = (PageNode *)MEMSPACE;
			} else {
				free_list_position++;
			}

			return evict->offset_p;	//return offset of page to evict	
		}

		if (free_list_position->offset_p == 1599) {
			free_list_position = (PageNode *)MEMSPACE;
		} else {
			free_list_position++;
		}
	}
}

//this function handles when a thread accesses a page they were not supposed to
static void pageFaultHandler(int sig, siginfo_t *si, void *unused) {
	printf("IN PAGE FAULT HANDLER FOR THREAD: %d\n" , currThread);
	long seg_fault_addr = (long) si -> si_addr;	//address where seg fault occurred
	PageNode *ptr = (PageNode *)MEMSPACE;

	if ((seg_fault_addr < (long)schedulerEnd) || (seg_fault_addr > (long)end)) {
		printf("Segmentation fault you naughty boy\n");
		exit(1);
	}

	int i = 0;
	while (i <= 1600) {	//go through all pageNodes
		//long page_start_addr = (long)(schedulerEnd + (ptr->offset_p * PAGE_SIZE));
		long page_start_addr = (long)(schedulerEnd + (i*PAGE_SIZE));

		if (page_start_addr > seg_fault_addr) {
			//printf("FOUND PAGE CORRESPONDING TO SEGFAULT ADDRESS: %d, PAGEADDRESS: %d\n", seg_fault_addr, page_start_addr - PAGE_SIZE);
			ptr = ptr - 1;	//this is pageNode corresponding to page of segfault
			break;
		}
		ptr++;
		i++;
	}

	//seg fault happenning in page i - 1
	//check to see if memory access is happenning in page that thread owns
	int * thread_page_count = ((int *)end + currThread);

	if (*thread_page_count < i) {
		printf("Num pages for currThread %d is %d and i is %d\n", currThread, *thread_page_count, i);
		printf("Segmentation Fault BOO YA\n");
		exit(1);
	}

	//The thread was already trying to access its own memory
	if(ptr -> threadid == currThread && ptr -> retAddr == -1) {
		//printf("Found itself freeing up\n");
		ptr -> reference_bit = 1;	//indicate accessed page for free list

		void *page = (schedulerEnd + (ptr -> offset_p * PAGE_SIZE));
		if (mprotect(page, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
			printf("ERROR mprotecting page at line %d\n", __LINE__);
			exit(1);
		}
		return;
	}

	//Looking for one it already owns
	PageNode *ptr2 = (PageNode *)MEMSPACE;
	i = 0;
	while (i < 1600) {
		if ((ptr2 -> threadid == currThread) && (ptr2 -> retAddr == ptr -> offset_p)) {
			//found the page belonging to the current thread
			ptr2 -> reference_bit = 1;	//indicate accessed page for free list
			swapPage(ptr -> offset_p, ptr2 -> offset_p, 0);
			return;
		}
		i++;
		ptr2++;
	}

	//Look for one it already owns is in the swap space
	//NEEDS TESTING
	PageNode *ptr5 = (PageNode *)MEMSPACE+1600;	//start at swap nodes
	i = 0;

	while (i < 3900) {
		if ((ptr5 -> threadid == currThread) && (ptr5 -> retAddr == ptr -> offset_p)) {
			//evict Page
			ptr5 -> reference_bit = 1;
			printf("EVICT 1\n");
			int offsetToEvict = evictPage();
			printf("AFTER CHOOSING A VICTIM\n");

			if (ptr -> offset_p == offsetToEvict) {
				swapFileGrab(ptr -> offset_p, ptr5 -> offset_p);
			} else {
				swapPage(ptr -> offset_p, offsetToEvict, 1);	//put victim in place of ptr
				swapFileGrab(ptr -> offset_p, ptr5 -> offset_p);	//swap victim with one we need from swap file
			}
			return;
		}
		i++;
		ptr5++;
	}


	int found = 0;

	//the page it faulted on was already free
	if(ptr->threadid == 0) {
		//printf("Found free\n");

		void *page = (schedulerEnd + (ptr -> offset_p * PAGE_SIZE));
                if (mprotect(page, PAGE_SIZE, PROT_READ | PROT_WRITE) == -1) {
                        printf("ERROR mprotecting page at line %d\n", __LINE__);
                        exit(1);
                }
		ptr->threadid = currThread;
		ptr->reference_bit = 1;
		return;
	} else {
		PageNode *ptr3 = (PageNode *)MEMSPACE;
		i = 0;

		while (i < 1600) {
			if (ptr3 -> threadid == 0) {	//found an empty page
				ptr3 -> threadid = currThread;
				ptr3 -> reference_bit = 1;
				swapPage(ptr -> offset_p, ptr3 -> offset_p, 0);
				found = 1;
				break;
			}  
			i++;
			ptr3++;
		}
	}

	if (found == 0) {
		//offset of page to evict
		printf("EVICT 2\n");
		int offsetToEvict = evictPage();
		//grab page from swap space
		PageNode *ptr4 = (PageNode *)MEMSPACE+1600;	//start at swap nodes
		i = 0;
		printf("AFTER CHOOSING A VICTIM\n");

		while (i < 3900) {
			if (ptr4 -> threadid == 0) {
				ptr4 -> threadid = currThread;
				ptr4 -> reference_bit = 1;

				if (ptr -> offset_p == offsetToEvict) {
					swapFileGrab(ptr -> offset_p, ptr4 -> offset_p);
				} else {
					//put victim in place of ptr
					swapPage(ptr -> offset_p, offsetToEvict, 1);
					//swap victim with one we need from swap file
					swapFileGrab(ptr -> offset_p, ptr4 -> offset_p);
				}

				break;
			}
			i++;
			ptr4++;
		}
	}
}

//this function sets up the special book keeping space in the data
int buildKernalSpace() {
	//currThread = 1;	
	//the page size is set
	PAGE_SIZE = sysconf(_SC_PAGE_SIZE);

	//the data is allocated and aligned to the page size
	posix_memalign(&MEMSPACE , sysconf(_SC_PAGE_SIZE) , 8000000);
	//the special pointers are set
	beg = (char*)MEMSPACE;
	kernalEnd = (char*)MEMSPACE + 18*PAGE_SIZE;
	schedulerEnd = kernalEnd + 200*PAGE_SIZE;
	end = schedulerEnd + 1600*PAGE_SIZE;
	thread_pages_count_end = end + PAGE_SIZE;
	shallocEnd = thread_pages_count_end + 4 * PAGE_SIZE;

	trueEnd = beg + 8000000;

	//the nodes used to indicate the pages are initialized here
	//free_list_position is initiated to beginning of MEMSPACE
	free_list_position = (PageNode*)MEMSPACE;
       	PageNode *temp = (PageNode*)MEMSPACE;

	int i;
	//1600 page nodes for Thread use

	for(i = 0 ; i < 1600 ; i++) {
		temp -> threadid = 0;
		temp -> offset_p = i;
		temp -> retAddr = -1;
		temp -> reference_bit = 0;
		temp = temp + 1;
	}

	//swap space nodes
	for(i = 0 ; i < 3900 ; i++) {
		temp -> threadid = 0;
		temp -> offset_p = i;
		temp -> retAddr = -1;
		temp -> reference_bit = 0;
		temp = temp + 1;
	}

	//this sets up the function that can catch Page Faults
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sigemptyset(&sa.sa_mask);
        sa.sa_sigaction = pageFaultHandler;

	if(sigaction(SIGSEGV , &sa ,NULL) == -1) {
		printf("SIG HANDLER FAILURE TO SET UP\n");
		exit(1);
	}

	PageNode *temp1 = (PageNode *)MEMSPACE;
	i = 0;
	while(i < 1600) {		//Protect all thread pages in memory
		void* page = (schedulerEnd + (temp1->offset_p)*PAGE_SIZE); 
		if (mprotect(page , PAGE_SIZE , PROT_NONE) == -1) {
			perror(strerror(errno));
			printf("ERROR MPROTECTING IN: %s line %d\n", __FILE__, __LINE__ - 1);
		}

		temp1 = temp1 + 1;
		i++;
	}

	//create swap file
	swapFP = open("swap_space" , O_CREAT | O_RDWR , S_IRUSR | S_IWUSR);

	if(swapFP == -1) {
		printf("ERROR OPENING SWAPFILE\n");
		exit(1);
	}

	//make swap file 16MB in size
	int result = lseek(swapFP , 0 , SEEK_SET);

	if(result == -1) {
		printf("ERROR SETTINF SWAPFILE SIZE\n");
		exit(1);
	}

	write(swapFP , MEMSPACE , 8000000);
	write(swapFP , MEMSPACE , 8000000);

	char c = 0;
	int result1 = write(swapFP, (void *) &c, 1);

	return 1;
}

//this protects the pages of the given thread
void protectPages(int threadID, int exiting) {
	//if exiting is 1, protect blocks and reset their values so new thread can use them
	printf("IN PROTECT PAGES. Thread ID %d\n", threadID);

	PageNode* temp = (PageNode*)MEMSPACE;
	//printf("Attempting to protect pages of %d\n" , threadID);

	int i = 0;
	while(temp -> threadid != 0 && i < 1600) {
		if(temp -> threadid == threadID) {
			void* page = (schedulerEnd + (temp->offset_p)*PAGE_SIZE); 

			if (mprotect(page , PAGE_SIZE , PROT_WRITE | PROT_READ) == -1) {
				printf("ERROR MPROTECTING IN: %s line %d\n", __FILE__, __LINE__ - 1);
			}

			if (exiting == 1) {	//reset node if thread exiting
				memset(page, 0, PAGE_SIZE);	//zero out memory of page
				
				temp -> threadid = 0;
				temp -> retAddr = -1;
			}

			if (mprotect(page , PAGE_SIZE , PROT_NONE) == -1) {
				printf("ERROR MPROTECTING IN: %s line %d\n", __FILE__, __LINE__ - 1);
			}
		}
		temp = temp + 1;
		i++;
	}

	if (exiting == 1) {
		//Releases pages for exiting thread
		int * thread_page_count = ((int *)end + threadID);
		pagesLeft = pagesLeft + *thread_page_count;
		*thread_page_count = 0;
	}

	return;

}

void* schedulerallocate(size_t size) {

	AllocNode* head = (AllocNode*)kernalEnd;

	if(head -> signature != 3233) {
		head -> signature = 3233;
		head -> next = NULL;
		head -> prev = NULL;
		if((char*)head + size >= schedulerEnd) {
			return NULL;
		}
		head->inUse = 1;
		head->capacity=size;
		return head + 1;
	}

	AllocNode* temp = head;
	AllocNode* prevAddr;

	while(temp != NULL) {
		 if(temp -> inUse == 0 && temp -> capacity == size) {
                        temp -> inUse = 1;
                        return (char*)(temp + 1);
                 }

                prevAddr = temp;
                temp = temp -> next;
	}

	AllocNode* newNode = (AllocNode*)((char*)(prevAddr + 1) + (prevAddr->capacity));
        if((char*)newNode + size >= schedulerEnd) {
                return NULL;
        }


        newNode -> signature = 3233;
        newNode -> capacity = size;
        newNode -> inUse = 1;
        newNode -> prev = prevAddr;
        prevAddr -> next = newNode;
        newNode -> next = NULL;

        return (char*)(newNode+1);

}

void* myallocate(size_t size , char* file , int line , int reqType) {
	printf("Allocating in file %s on line %d\n" , file , line);
	
	//checks if allocations have started and if not sets up the 
	//special book keeping space
	if(start == 0) {
		start = 1;
		buildKernalSpace();
	}

	if (size <= 0) {
		printf("Invalid Size\n");
		return NULL;
	}

	//checks if the scheduler is calling malloc
	//if so a special id is given
	//-2 indicates the scheduler
	if(reqType == LIBRARYREQ) {
		protectPages(currThread, 0);	//protect pages for current thread before scheduler comes in
		return schedulerallocate(size);
	} 

	printf("for the current thread: %d\n" , currThread);

	int num_pages_allocating;
	int * thread_page_count = ((int *)end + currThread);

	//this is the start of allocated memory for each thread
	AllocNode* head = (AllocNode*)schedulerEnd;

	//increment page count based on size
	if (*thread_page_count == 0) {
		if ((char*)(head+1) + size >= end) {
			printf("RETURNS NULL HERE\n");
			return NULL;
		}

		for (num_pages_allocating = 1; (num_pages_allocating * PAGE_SIZE) <= size + sizeof(AllocNode); num_pages_allocating++);

		if(pagesLeft - num_pages_allocating < 0){
                        printf("You cannot allocate any more pages.\n");
                        return NULL;
                }
		printf("NUMBER PAGES ALLOCATING: %d\n", num_pages_allocating);
		*thread_page_count = *thread_page_count + num_pages_allocating;
		pagesLeft = pagesLeft - num_pages_allocating;	//decrement pages left
	}

	//we check if the Page had already been stroed in data
	if(head -> signature != 3233) {
		head -> signature = 3233;
		head -> next = NULL;
		head -> prev = NULL;
		head -> inUse = 1;
		head -> capacity = size;
		printf("Successful0 Allocation in file %s on line %d\n" , file , line);
		printf("ret: %d\n" , head+1);

		return head + 1;  
	}

	//if not we search for open space
	AllocNode* temp = head;
	AllocNode * prevAddr;

	while(temp != NULL) {
		if(temp -> inUse == 0 && temp -> capacity == size) {
			temp -> inUse = 1;
			printf("Successful1 Allocation in file %s on line %d\n" , file , line);
			printf("ret : %d\n" , (char*)(temp+1));
			return (char*)(temp + 1);
		}		

		prevAddr = temp;
		temp = temp -> next;
	}

	//once one is found we make sure it is not to big then
	//return the data after the node
	AllocNode* newNode = (AllocNode*)((char*)(prevAddr + 1) + (prevAddr->capacity));
	if((char*)(newNode+1) + size >= (end)) {
		printf("Unsuccessful0 Allocation in file %s on line %d\n" , file , line);
		return NULL;
	}

	//space left in current page allocNode is in
	int spaceLeftInPage = PAGE_SIZE - (((long)(newNode + 1)) % PAGE_SIZE);

	//increment page count based on size
	for (num_pages_allocating = 0; (num_pages_allocating * PAGE_SIZE) + spaceLeftInPage <= size; num_pages_allocating++);

	if(pagesLeft - num_pages_allocating < 0){
                        printf("You cannot allocate any more pages.\n");
                        return NULL;
        }

	*thread_page_count = *thread_page_count + num_pages_allocating;

	pagesLeft = pagesLeft - num_pages_allocating;	//decrement pages left
	
	newNode -> signature = 3233;
	newNode -> capacity = size;
	newNode -> inUse = 1;
	newNode -> prev = prevAddr;
	prevAddr -> next = newNode;
	newNode -> next = NULL;

	printf("Successful2 Allocation in file %s on line %d\n" , file , line);
	printf("ret: %d\n" , (char*)(newNode+1));
	return (char*)(newNode+1);

}

void mydeallocate(void* ptr , char* file , int line , int reqType) {
	printf("Freeing Memory in file %s on line %d\n" , file , line);

	//we must check if any allocations had even occured
	if(start == 0) {
		printf("Unsuccessful0 free in file %s on line %d\n" , file , line);
		return;
	}

	//if the scheduler is allocating
	if(reqType == LIBRARYREQ) {
		if((char*)ptr < kernalEnd || (char*)ptr >= schedulerEnd) {
			return;
		}
	} else {
		printf("for : %d\n" , currThread);
		if((char*)ptr < schedulerEnd || (char*)ptr >= end) {
			printf("Unsuccessful1 free in file %s on line %d\n" , file , line);
			return;
		}
	}

	//the node adjacent to the to be freed data is gotten
	//then we check if it is in fact a node
	//then we check if it has already been freed
	AllocNode* currNode = (AllocNode*)ptr - 1;

	if(currNode -> signature != 3233) {
		printf("Unsuccessful2 free in file %s on line %d\n" , file , line);
		return;
	}

	if(currNode -> inUse == 0) {
		printf("Unuccessful3 free in file %s on line %d\n" , file , line);
		return;
	}

	//the current data is set so it is out of use
	currNode -> inUse = 0;

	//we check all the previous nodes to see if they are in use
	//if not we delete the current node and increase the capacity of the previous
	while(currNode -> prev != NULL) {
		if(currNode -> prev -> inUse == 0) {
			currNode -> signature = 0;
			currNode -> prev -> next = currNode -> next;
			currNode -> prev -> capacity = currNode -> prev -> capacity + sizeof(AllocNode) + currNode -> capacity;
			currNode = currNode -> prev;
		} else {
			break;
		}
	}

	//we check all the next nodes to see if they are in use
	//if not we delete them and increase the capacity of the current node
	while(currNode -> next != NULL) {
		if(currNode -> next -> inUse == 0) {
			currNode -> next -> signature = 0;
			currNode -> capacity = currNode -> next -> capacity + sizeof(AllocNode) + currNode -> capacity;
			currNode -> next = currNode -> next -> next;
		} else {
			break;
		}
	}

	//if there is nothing after the freed data then the node is deleted
	if(currNode -> next == NULL) {
		currNode -> signature = 0;
		currNode -> capacity = 0;
		if(currNode -> prev != NULL) {
			currNode -> prev -> next = NULL;
		}
		currNode -> prev = NULL;
		currNode = NULL;
		printf("Successful0 free in file %s on line %d\n" , file , line);
		return;
	}

	printf("Successful1 free in file %s on line %d\n" , file , line);
	return;

}

void* shalloc(size_t size) {
	printf("Shallocing.\n");

	if(start == 0) {
		start = 1;
		buildKernalSpace();
	}

	AllocNode* head = (AllocNode*) thread_pages_count_end;

	if(head -> signature != 3233) {
		head -> signature = 3233;
		head -> next = NULL;
		head -> prev = NULL;
		if((char*)head + size >= shallocEnd) {
			return NULL;
		}
		head->inUse = 1;
		head->capacity=size;
		return head + 1;
	}

	AllocNode* temp = head;
	AllocNode* prevAddr;

	while(temp != NULL) {
		 if(temp -> inUse == 0 && temp -> capacity == size) {
                        temp -> inUse = 1;
                        return (char*)(temp + 1);
                 }

                prevAddr = temp;
                temp = temp -> next;
	}

	AllocNode* newNode = (AllocNode*)((char*)(prevAddr + 1) + (prevAddr->capacity));
        if((char*)newNode + size >= shallocEnd) {
                return NULL;
        }


        newNode -> signature = 3233;
        newNode -> capacity = size;
        newNode -> inUse = 1;
        newNode -> prev = prevAddr;
        prevAddr -> next = newNode;
        newNode -> next = NULL;

        return (char*)(newNode+1);
}



