// File:	my_pthread_t.h
// Author:	Yujie REN
// Date:	09/23/2017

// name:
// username of iLab:
// iLab Server: 
#ifndef MY_PTHREAD_T_H
#define MY_PTHREAD_T_H

#define _GNU_SOURCE
#define FIBER_STACK 1024*64		//64KB stack

// thread status codes
#define THREAD_RUNNING 0 
#define THREAD_READY 1
#define THREAD_SUSPENDED 2
#define THREAD_BLOCKED 3 
#define THREAD_EXITED 4
#define THREAD_MISSED_DEADLINE 5

// time quantas for thread execution of each priority level
#define TIME_QUANTA_0 25
#define TIME_QUANTA_1 50
#define TIME_QUANTA_2 100

#define USE_MY_PTHREAD 1


//macros for replacing the standard pthread functions
#define pthread_create(x, y, z, w) my_pthread_create(x, y, z, w)
#define pthread_exit(x) my_pthread_exit(x)
#define pthread_yield() my_pthread_yield()
#define pthread_join(x, y) my_pthread_join(x, y)
#define pthread_mutex_init(x, y) pthread_mutex_init(x, y)
#define pthread_mutex_lock(x) my_pthread_mutex_lock(x)
#define pthread_mutex_unlock(x) my_pthread_mutex_unlock(x)
#define pthread_mutex_destroy(x) my_pthread_mutex_destroy(x)

/* include lib header files that you need here: */
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <string.h>
#include <errno.h>


typedef uint my_pthread_t;
typedef uint my_pthread_attr_t;

uint thread_count1;

#define pthread_t my_pthread_t
#define pthread_attr_t my_pthread_attr_t

typedef struct threadControlBlock {
	/* add something here */
	unsigned int status;
	unsigned int priority;
	unsigned int time_ran;
	ucontext_t *thread_context;
	my_pthread_t thread_id;
	
} tcb; 

/* mutex struct definition */
typedef struct my_pthread_mutex_t {
	
	int flag;
	int destroyed;
	
} my_pthread_mutex_t;

/* define your data structures here: */
typedef struct join_var_t {
	int done;
	void *retval;
	my_pthread_t thread_waiting_for;
} join_var;

typedef struct join_var_block_t {
	join_var *data;
	struct join_var_block_t *next;
} join_var_block;

join_var_block *join_LL;

typedef struct retval_t {
	void *retval;
	my_pthread_t thread_id;
} retval_elem;

typedef struct retval_block_t {
	retval_elem *data;
	struct retval_block_t *next;
} retval_block;

retval_block *retval_LL;	//LL to store retvals when threads exit
// Feel free to add your own auxiliary data structures


ucontext_t return_link;

typedef struct tcb_linked_list {
	
	tcb * data;
	struct tcb_linked_list * next;
	
} tcb_list;

typedef struct multilevel_priority_queue {
	
	tcb_list *priority0_head;
	tcb_list *priority1_head;
	tcb_list *priority2_head;
	
	tcb_list *priority0_tail;
	tcb_list *priority1_tail;
	tcb_list *priority2_tail;
} mpq;

mpq *priority_Queue; 

tcb_list * running_queue_head;
tcb_list * running_queue_tail;

my_pthread_mutex_t *thread_count_mtx;

int empty_running_queue;

/* Function Declarations: */

/* create a new thread */
int my_pthread_create(my_pthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg);

/* give CPU pocession to other user level threads voluntarily */
int my_pthread_yield();

/* Makes Scheduling Decisions */
void scheduler();

void sig_handler();

// create running queue
void createRunningQueue();

// set signal handler
//signal(SI_TIMER, sig_handler);

/* terminate a thread */
void my_pthread_exit(void *value_ptr);

/* wait for thread termination */
int my_pthread_join(my_pthread_t thread, void **value_ptr);

/* initial the mutex lock */
int my_pthread_mutex_init(my_pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int my_pthread_mutex_lock(my_pthread_mutex_t *mutex);

/* release the mutex lock */
int my_pthread_mutex_unlock(my_pthread_mutex_t *mutex);

/* destroy the mutex */
int my_pthread_mutex_destroy(my_pthread_mutex_t *mutex);

#endif
