// File:	my_pthread.c
// Author:	Yujie REN
// Date:	09/23/2017

// name: Thomas Daub tjd147, Marshal Nink mrn61, Eli Minkin ebm74
// iLab Server: design.cs.rutgers.edu

#include "totalrecall.h"
#include "my_pthread_t.h"

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	printf("CREATING PTHREAD\n");

        //Thread Creation Started
	// create priority queue if it doesn't exist
	if (priority_Queue == NULL ) {
		getcontext(&return_link);
		
	    return_link.uc_link = 0;		//create the uc_link context for all threads
	    return_link.uc_stack.ss_sp = myallocate(FIBER_STACK, __FILE__, __LINE__, LIBRARYREQ);
	    
	    if (return_link.uc_stack.ss_sp == NULL) {
	    	printf("Error in Malloc");
	    	exit(1);
	    }
	    
	    return_link.uc_stack.ss_size = FIBER_STACK;
	    return_link.uc_stack.ss_flags = 0; 
	   
		void (*exitFunc)(void*) = &my_pthread_exit;
	    
	    makecontext(&return_link, (void*)exitFunc, 1, NULL);
	    
		priority_Queue = (mpq *) myallocate(sizeof(mpq), __FILE__, __LINE__, LIBRARYREQ);		//allocate priority queue

		if (priority_Queue == NULL) {
    			printf("Error in Malloc");
			errno = EAGAIN;
    			return errno;
    		}
		//initialize thread count mutex
		thread_count_mtx = (my_pthread_mutex_t *)myallocate(sizeof(my_pthread_mutex_t), __FILE__, __LINE__, LIBRARYREQ);
		thread_count_mtx -> flag = 0;
		
		thread_count1 = 2;				//initialize thread count
	}
	// signal(SI_TIMER, sig_handler);
	
	ucontext_t *newthread = (ucontext_t *)myallocate(sizeof(ucontext_t), __FILE__, __LINE__, LIBRARYREQ);

	if (newthread == NULL) {
    		printf("Error in Malloc");
		errno = EAGAIN;
    		return errno;
    	}

	 // Get the current execution context
    	getcontext(newthread);
    	
	// Modify the context to a new stack
    	newthread->uc_link = &return_link;			//TODO: uc_link should point to context that calls pthread_exit()
    	newthread->uc_stack.ss_sp = myallocate(FIBER_STACK, __FILE__, __LINE__, LIBRARYREQ);
    
    	if (newthread->uc_stack.ss_sp == NULL) {
    		printf("Error in Malloc");
		errno = EAGAIN;
    		return errno;
    	}
    
    	newthread->uc_stack.ss_size = FIBER_STACK;
    	newthread->uc_stack.ss_flags = 0;        

    	// Create the new context
    	void *func = (void *) function;

    	if (arg != NULL) {
   		 //int * a = (int *)arg;
   		 makecontext(newthread, func, 1, arg);
    	} else {
    		makecontext(newthread, func, 0);
    	}

    	//create new Thread context block
    	tcb *newBlock = (tcb *) myallocate(sizeof(tcb), __FILE__, __LINE__, LIBRARYREQ);
    	newBlock -> priority = 0;
    	newBlock -> status = THREAD_READY;
    	newBlock -> thread_context = newthread;
	newBlock -> time_ran = 0;

	my_pthread_mutex_lock(thread_count_mtx);	//update to count should be without interrupts
		
    	newBlock -> thread_id = thread_count1;
	*thread = thread_count1;
	thread_count1++;

	my_pthread_mutex_unlock(thread_count_mtx);
    
    	//create node in list for thread context block
	tcb_list *newNode = (tcb_list *) myallocate(sizeof(tcb_list), __FILE__, __LINE__, LIBRARYREQ);

    	if (newNode == NULL) {
    		printf("Error in Malloc");
		errno = EAGAIN;
    		return errno;
    	}
	newNode -> data = newBlock;
	
	//new Tail for list
	if (priority_Queue -> priority0_head == NULL) {
		priority_Queue -> priority0_head = newNode;
		priority_Queue -> priority0_tail = newNode;
	} else {
		priority_Queue -> priority0_tail -> next = newNode;
    	priority_Queue -> priority0_tail = newNode;
	}
    
    	//call yield
    	my_pthread_yield();
    
	return 0;
};

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield() {

        //printf("Thread Yielded\n");
	
	struct itimerval timer;
	getitimer(ITIMER_REAL, &timer);
	
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);		//stops the timer
	
	scheduler(0);
	return 0;
};

/* Makes Scheduling Decisions */
void scheduler(int ranWholeTimeSlice) {

        printf("Scheduler Called\n");
	
	struct itimerval timer;
	struct sigaction sa;
	
	int exited = 0;
	int exitingThreadId;
	
	tcb_list *ptr;
	tcb_list *currentExecutingThread;			//current executing thread
	
	// first run through, running queue head is empty
	if (running_queue_head == NULL) {
		//printf("Running Q empty\n");
		ucontext_t *originalthread = (ucontext_t *) myallocate(sizeof(ucontext_t), __FILE__, __LINE__, LIBRARYREQ);
		my_pthread_t ot = 1;
    	
    		//create new Thread context block
	    	tcb *newBlock = (tcb *) myallocate(sizeof(tcb), __FILE__, __LINE__, LIBRARYREQ);
	    	newBlock -> priority = 0;
		newBlock -> time_ran = 0;
	    	newBlock -> status = THREAD_READY;
	    	newBlock -> thread_context = originalthread;
	    	newBlock -> thread_id = ot;
	    
	    	//create node in list for thread context block
		tcb_list *newNode = (tcb_list *)myallocate(sizeof(tcb_list), __FILE__, __LINE__, LIBRARYREQ);
		newNode -> data = newBlock;
		
		newNode -> next = priority_Queue -> priority0_head;
		priority_Queue -> priority0_head = newNode;
		
		currentExecutingThread = newNode;		//current executing thread is original thread
		
		createRunningQueue();
	} else {
		//printf("Running Q not Empty\n");
		ptr = running_queue_head;

		int aging_flag = 0;
		
		//add running time to thread
		if (ptr -> data -> priority == 2) {
			ptr -> data -> time_ran += 100;
		} else if (ptr -> data -> priority == 1) {
			ptr -> data -> time_ran += 50;
		} else {
			ptr -> data -> time_ran += 25;
		}

		if (ptr -> data -> time_ran >= 375) {		//set priority back to 0
			ptr -> data -> priority = 0;
			ptr -> data -> time_ran = 0;
			aging_flag = 1;
		}
		
		if (ptr -> data -> status == THREAD_EXITED) {		//if thread exited
			exitingThreadId = running_queue_head -> data -> thread_id;		//save id of exiting thread
			running_queue_head = ptr -> next;
			
			mydeallocate(ptr -> data -> thread_context -> uc_stack.ss_sp, __FILE__, __LINE__, LIBRARYREQ);	//free stack pointer
			mydeallocate(ptr -> data, __FILE__, __LINE__, LIBRARYREQ);		//free thread control block
			mydeallocate(ptr, __FILE__, __LINE__, LIBRARYREQ);				//free list node

			//printf("   Thread found to have Exited\n");

			exited = 1;				//indicates that we must use setcontext
			
		} else {	//didn't finish executing
			//printf("   Next Running Selected\n");
			running_queue_head = ptr -> next;
			currentExecutingThread = ptr;				//save reference to current executing thread
			ptr -> data -> status = THREAD_READY;
		
			if (ranWholeTimeSlice && (!aging_flag)) {
				if (ptr -> data -> priority < 2) {
					ptr -> data -> priority = ptr -> data -> priority + 1;		//decrease priority if finished time slice
				}
			}
			
			//add ptr back into a priority queue
			if (ptr -> data -> priority == 2) {
				if (priority_Queue -> priority2_tail == NULL) {
					priority_Queue -> priority2_head = ptr;
					priority_Queue -> priority2_tail = ptr;
				} else {
					priority_Queue -> priority2_tail -> next = ptr;
					priority_Queue -> priority2_tail = ptr;
				}
				ptr -> next = NULL;
			}
			else if (ptr -> data -> priority == 1) {
				if (priority_Queue -> priority1_tail == NULL) {
					priority_Queue -> priority1_head = ptr;
					priority_Queue -> priority1_tail = ptr;
				} else {
					priority_Queue -> priority1_tail -> next = ptr;
					priority_Queue -> priority1_tail = ptr;
				}
				ptr -> next = NULL;
			} else {
				if (priority_Queue -> priority0_tail == NULL) {
					priority_Queue -> priority0_head = ptr;
					priority_Queue -> priority0_tail = ptr;
				} else {
					priority_Queue -> priority0_tail -> next = ptr;
					priority_Queue -> priority0_tail = ptr;
				}
				ptr -> next = NULL;
			}
		
		}
				
		if (running_queue_head == NULL) {
			//printf("Running Q finished execution. Needs to be refilled\n");
			createRunningQueue();
			
		}
	
	}
	
	/* Start a virtual timer. It counts down whenever this process is
	executing. Install timer_handler as the signal handler for SIGVTALRM. */
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &sig_handler;
	sigaction (SIGALRM, &sa, NULL);

	int priority;
	if(running_queue_head != NULL) {		
		priority = running_queue_head -> data -> priority;
	} else {
		priority = 0;
	}
		
	if (priority == 0) {
		/* Configure the timer to expire after 25 msec... */
		// Timer set
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 25000;
		
		timer.it_interval.tv_sec = 0;
		timer.it_interval.tv_usec = 25000;
	} else if (priority == 1) {
		/* Configure the timer to expire after 50 msec... */
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 50000;
		
		timer.it_interval.tv_sec = 0;
		timer.it_interval.tv_usec = 50000;
	} else {
		/* Configure the timer to expire after 100 msec... */
		timer.it_value.tv_sec = 0;
		timer.it_value.tv_usec = 100000;
		
		timer.it_interval.tv_sec = 0;
		timer.it_interval.tv_usec = 100000;
	}

	if(empty_running_queue == 1) {
		//No Threads Left : Scheduler Exiting
		exit(1);
	}
	

	if (exited == 1) {
		//Setting Context
		running_queue_head -> data -> status = THREAD_RUNNING;

		//currThread is in the totalrecall library
		currThread = running_queue_head -> data -> thread_id;

		ucontext_t *context = running_queue_head -> data -> thread_context;	//gets new context to set too
		printf("Swapping to thread: %d\n", running_queue_head -> data -> thread_id);

		protectPages(exitingThreadId, 1);
		setitimer(ITIMER_REAL, &timer, NULL);
		setcontext(context);
	} else {
		//Swapping Context
					
		running_queue_head -> data -> status = THREAD_RUNNING;
		printf("id: %d\n", running_queue_head -> data -> thread_id);
		currThread = running_queue_head -> data -> thread_id;
		

		protectPages(currentExecutingThread -> data -> thread_id, 0);	//protects the page that's about to be swapped out
		setitimer(ITIMER_REAL, &timer, NULL);
		swapcontext(currentExecutingThread -> data -> thread_context, running_queue_head -> data -> thread_context);
	}
	
}

void sig_handler() {
	//TIMER WENT OFF
	
	struct itimerval timer;
	getitimer(ITIMER_REAL, &timer);
	
	timer.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);		//stops the timer
	printf("TIMER WENT OFF\n");
		
	scheduler(1);	

};

void createRunningQueue() {

	//Running Queue Create Started

	int count = 0; // running total of milliseconds
	int max = 300;

	if(priority_Queue -> priority0_head == NULL && priority_Queue -> priority1_head == NULL && priority_Queue -> priority2_head == NULL) {
		//Can't fill Running Queue
		empty_running_queue = 1;
		//return;
	} else {
		//Can fill Running Queue
		empty_running_queue = 0;
	}
	
	tcb_list *p0_temp = priority_Queue -> priority0_head;
	tcb_list *p1_temp = priority_Queue -> priority1_head;
	tcb_list *p2_temp = priority_Queue -> priority2_head;
	
	int p0 = 0, p1 = 0, p2 = 0; // count how many of each priority we're adding

	// run through existing queue and tally each priority
	// try to get 4 - 2 - 1
	
	// try to set 1 priority2 thread
	if (p2_temp != NULL) {
		count += 100;
		p2++;
		p2_temp = p2_temp -> next;
	}
	
	// try to set 2 priority 1 threads
	while (p1_temp != NULL) {
		count += 50;
		p1++;
		p1_temp = p1_temp -> next;
		
		if (p1 == 2) {
			break;
		}
	}
	
	// try to fill in rest with priority 0 threads
	while (p0_temp != NULL && count < max) {
		count += 25;
		p0++;
		p0_temp = p0_temp -> next;
	}
	
	// go back up and try to fill in the blanks if there's room left over
	if (count < max) {
		while (p1_temp != NULL && count < max) {
			printf("c\n");
			if ((count+50) <= max) {
				count += 50;
				p1++;
			}
			p1_temp = p1_temp -> next;
		}
		while (p2_temp != NULL && count < max) {
			printf("d\n");
			if ((count+100) <= max) {
				count += 100;
				p2++;
			}
			p2_temp = p2_temp -> next;
		}
	}
	
	// time to load up the running queue
	// and remove tcb_lists from the priority queues
	// add priority 0s
	while (p0 > 0) {
		if (running_queue_head == NULL) {
			running_queue_head = priority_Queue -> priority0_head;
			running_queue_tail = priority_Queue -> priority0_head;
		} else {
			running_queue_tail -> next = priority_Queue -> priority0_head;
			running_queue_tail = priority_Queue -> priority0_head;
		}
		priority_Queue -> priority0_head = priority_Queue -> priority0_head -> next;
		
		if (priority_Queue -> priority0_head == NULL) {	//if head is null, tail is null
			priority_Queue -> priority0_tail = NULL;
		}
		p0--;
	}
	
	// add priority 1s
	while (p1 > 0) {
		if (running_queue_head == NULL) {
			running_queue_head = priority_Queue -> priority1_head;
			running_queue_tail = priority_Queue -> priority1_head;
		} else {
			running_queue_tail -> next = priority_Queue -> priority1_head;
			running_queue_tail = priority_Queue -> priority1_head;
		}
		priority_Queue -> priority1_head = priority_Queue -> priority1_head -> next;

		if (priority_Queue -> priority1_head == NULL) {	//if head is null, tail is null
			priority_Queue -> priority1_tail = NULL;
		}

		p1--;
	}
	
	// add priority 2s
	while (p2 > 0) {
		if (running_queue_head == NULL) {
			running_queue_head = priority_Queue -> priority2_head;
			running_queue_tail = priority_Queue -> priority2_head;
		} else {
			running_queue_tail -> next = priority_Queue -> priority2_head;
			running_queue_tail = priority_Queue -> priority2_head;
		}
		priority_Queue -> priority2_head = priority_Queue -> priority2_head -> next;

		if (priority_Queue -> priority2_head == NULL) {	//if head is null, tail is null
			priority_Queue -> priority2_tail = NULL;
		}

		p2--;
	}
	
	//Running Queue Creation Ended
}

/* terminate a thread */
void my_pthread_exit(void *value_ptr) {

	//Thread Exited

	tcb_list *ptr = running_queue_head;

	//check to see if any threads are waiting on this one and set done appropriately
	join_var_block *ptr1 = join_LL;

	while (ptr1 != NULL) {
		if (ptr1 -> data -> thread_waiting_for == running_queue_head -> data -> thread_id) {
			ptr1 -> data -> done = 1;
			break;
		}
		ptr1 = ptr1 -> next;	
	}

	//deal with retval
	if (value_ptr != NULL) {
		retval_elem *val = (retval_elem *)myallocate(sizeof(retval_elem), __FILE__, __LINE__, LIBRARYREQ);
		val -> thread_id = ptr -> data -> thread_id;
		val -> retval = value_ptr;

		retval_block *block = (retval_block *)myallocate(sizeof(retval_block), __FILE__, __LINE__, LIBRARYREQ);
		block -> data = val;

		if (retval_LL == NULL) {		//add to return value linked list
			block -> next = NULL;
			retval_LL = block;
		} else {
			block -> next = retval_LL;
			retval_LL = block;
		}
	}
	
	ptr -> data -> status = THREAD_EXITED;
	my_pthread_yield();		//calls yield
	
};

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr) {
	printf("BEGINNING OF JOIN\n");

	join_var *cond_var = (join_var *) myallocate(sizeof(join_var), __FILE__, __LINE__, LIBRARYREQ);
	cond_var -> done = 0;		//if thread we are waiting for is done
	cond_var -> thread_waiting_for = thread;

	join_var_block *block = (join_var_block *) myallocate(sizeof(join_var_block), __FILE__, __LINE__, LIBRARYREQ);
	block -> data = cond_var;

	//my_pthread_t test = thread;
	
	//add it to wait queue
	if (join_LL == NULL) {
		join_LL = block;
		block -> next = NULL;
	} else {
		block -> next = join_LL;
		join_LL = block;
	}


	//check if thread is in priority queues
	int foundT = 0;
	tcb_list *temp0 = priority_Queue -> priority0_head;
	while(temp0 != NULL && foundT == 0){
		if(thread == temp0 -> data -> thread_id) {
			foundT = 1;
		}
		temp0 = temp0 -> next;
	}	

        tcb_list *temp1 = priority_Queue -> priority1_head;
        while(temp1 != NULL && foundT == 0){
                if(thread == temp1 -> data -> thread_id) {
                        foundT = 1;
                }
                temp1 = temp1 -> next;
        }

        tcb_list *temp2 = priority_Queue -> priority2_head;
        while(temp2 != NULL && foundT == 0){
                if(thread == temp2 -> data -> thread_id) {
                        foundT = 1;
                }
                temp2 = temp2 -> next;
        }

        tcb_list *tempR = running_queue_head;
        while(tempR != NULL && foundT == 0){
                if(thread == tempR -> data -> thread_id) {
                        foundT = 1;
                }
                tempR = tempR -> next;
        }

	if (foundT == 1) {	//thread we waited for hasn't exited yet
		//Thread we waiting for hasn't exited yet
		while (cond_var -> done == 0) {
			//Stuck waiting for condition
			my_pthread_yield();
		}
	}
	//Thread we waiting for has exited already

	//free wait queue block
	join_var_block *prev = NULL;
	join_var_block *ptr = join_LL;

	if (ptr == block) {
		join_LL = ptr -> next;
		mydeallocate(ptr -> data, __FILE__, __LINE__, LIBRARYREQ);
		mydeallocate(ptr, __FILE__, __LINE__, LIBRARYREQ);
	} else {
		while (ptr != NULL) {
			if (ptr == block) {
				prev -> next = ptr -> next;	
				mydeallocate(ptr -> data, __FILE__, __LINE__, LIBRARYREQ);
				mydeallocate(ptr, __FILE__, __LINE__, LIBRARYREQ);
			}
			prev = ptr;
			ptr = ptr -> next;
		}
	}

	//check return values for thread we waited for
	if (value_ptr != NULL) {
		retval_block *prev1 = NULL;
		retval_block *ptr1 = retval_LL;

		while (ptr1 != NULL) {
			if (ptr1 -> data -> thread_id == thread) {
				*value_ptr = (ptr1 -> data -> retval);

				if (prev1 == NULL) {
					retval_LL = ptr1 -> next;
				} else {
					prev1 -> next = ptr1 -> next;
				}

				mydeallocate(ptr1 -> data, __FILE__, __LINE__, LIBRARYREQ);
				mydeallocate(ptr1, __FILE__, __LINE__, LIBRARYREQ);
				break;
			}
			prev1 = ptr1;
			ptr1 = ptr1 -> next;
		}
	}

	return 0;
};

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	// 0 indicates that lock is available, 1 that it is held
	if ((*mutex).destroyed == 1) {
		(*mutex).destroyed = 0;
	} else {
		(*mutex).flag = 0;
	}
	return 0;
};

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex) {
	if ((*mutex).destroyed == 1) {
		errno = EINVAL;	//lock has been destroyed so can't lock
		return errno;
	} 
	while (__sync_lock_test_and_set(&mutex -> flag, 1) == 1) {
		my_pthread_yield();
	}
		
	return 0;
};

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex) {
	if ((*mutex).destroyed == 1) {
		errno = EINVAL;	//lock has been destroyed so can't unlock
		return errno;
	} 
	(*mutex).flag = 0;
	return 0;
};

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex) {
	//if (mutex -> flag == 0) {
	if ((*mutex).flag == 0) {
		//mutex is unlocked
		(*mutex).destroyed = 1;
		return 0;
	} else {
		//return error
		errno = EBUSY;
		return errno;
	}
	
};



