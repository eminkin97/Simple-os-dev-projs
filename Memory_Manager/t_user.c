#include <stdio.h>
#include <stdlib.h>
#include "my_pthread_t.h"

my_pthread_mutex_t mtx;
int count = 0;

void function0() {

    printf("***Hey its me 0***\n");
    my_pthread_exit(NULL);

}

void function1() {

    void (*funct0)() = &function0;
    my_pthread_t lid;
    my_pthread_create(&lid , NULL , (void*)funct0 , NULL);
    printf("***Hello World***\n");
    my_pthread_join(lid , NULL);
    my_pthread_exit(NULL);
}

void function2() {
	if (my_pthread_mutex_lock(&mtx) == -1) {
		printf("ERROR acquiring lock\n");
	}
        printf("***Its me number 2***\n");
        printf("Acquired mutex\n");
	count++;
	my_pthread_mutex_unlock(&mtx);
        my_pthread_exit(NULL);
}

void  * function3(void *arg) {
	int *a = (int *)arg;	
	printf("FUNCTION 3 SAYS: %d\n", *a);
	int *b = (int*)malloc(sizeof(int));
        *b = 69;
        printf("***%d***\n" , *b);
	my_pthread_exit((void*)b);

}

int main() {

    void (*funct)() = &function1;
    void (*funct2)() = &function2;
    void * (*funct3)(void *) = &function3;

    my_pthread_t tid;
    my_pthread_t tid3;

    printf("%x\n", mtx);
    my_pthread_mutex_init(&mtx, NULL);
    printf("%x\n", mtx);
  
 
    printf("b4 Thread 1: %d\n" , tid);

    int a = 2;
    void *b = (void *) &a;

    my_pthread_create(&tid, NULL, (void*)funct, NULL);
    my_pthread_create(&tid3 , NULL , (void*)funct3 , b);

    printf("Thread 1: %d\n" , tid);
    
    my_pthread_join(tid, NULL);
 

    void ** q = (void**)malloc(sizeof(void*));
	
    my_pthread_join(tid3 , q);
    int * f = (int *)*q;

    printf("%d\n" , *f);
    free(f); 
    free(q);

    my_pthread_t z1;
    my_pthread_t z2;
    my_pthread_t z3;
    my_pthread_t z4;
    my_pthread_t z5;
    my_pthread_t z6;
    my_pthread_t z7;
    my_pthread_create(&z1 , NULL , (void*)funct2 , NULL);
    my_pthread_create(&z2 , NULL , (void*)funct2 , NULL);
    my_pthread_create(&z3 , NULL , (void*)funct2 , NULL);
    my_pthread_create(&z4 , NULL , (void*)funct2 , NULL);
    my_pthread_create(&z5 , NULL , (void*)funct2 , NULL);
    my_pthread_mutex_destroy(&mtx);
    my_pthread_create(&z6 , NULL , (void*)funct2 , NULL);
    my_pthread_create(&z7 , NULL , (void*)funct2 , NULL);

    my_pthread_join(z1 , NULL);
    my_pthread_join(z2 , NULL);
    my_pthread_join(z3 , NULL);
    my_pthread_join(z4 , NULL);
    my_pthread_join(z5 , NULL);
    my_pthread_join(z6 , NULL);
    my_pthread_join(z7 , NULL);

    printf("hi\n");
    return 0;
    
}
