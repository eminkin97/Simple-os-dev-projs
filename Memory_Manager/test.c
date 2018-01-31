#include <stdlib.h>
#include <stdio.h>
#include "my_pthread_t.h"
#include "totalrecall.h"
#include <string.h>

void function1() {
	printf("hi\n");
    char* c = (char*)malloc(20000);
    strcpy(c,"blahblahblah\n");
	c[19000] = 'a';
    printf("%s",c);
	printf("OMG SUPER IMPT: %c\n", c[19000]);
    
    my_pthread_exit(NULL);
}

int main() {
	char* c = (char*)malloc(80);
	printf("First malloc completed\n");
	c[60]= 'a';
	char* a = (char*)malloc(800);
	printf("%c\n", c[60]);

	void (*funct)() = &function1;
	my_pthread_t tid, tid1, tid2, tid3, tid4, tid5, tid6, tid7, tid8, tid9, tid10;
	my_pthread_create(&tid, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid1, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid2, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid3, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid4, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid5, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid6, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid7, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid8, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid9, NULL, (void*)funct, NULL); 
	my_pthread_create(&tid10, NULL, (void*)funct, NULL); 
	
	my_pthread_join(tid, NULL);
	my_pthread_join(tid1, NULL);
	my_pthread_join(tid2, NULL);
	my_pthread_join(tid3, NULL);
	my_pthread_join(tid4, NULL);
	my_pthread_join(tid5, NULL);
	my_pthread_join(tid6, NULL);
	my_pthread_join(tid7, NULL);
	my_pthread_join(tid8, NULL);
	my_pthread_join(tid9, NULL);
	my_pthread_join(tid10, NULL);
	printf("PRINTING BEFORE THE FREEEEEEEEEE");
	free(c);
	free(a);
	printf("PRINTING AFTER THE FREEEEEEEEEE\n");
	int a1 = 1;
	int b1 = 2;
	int c1 = a1 + b1;
	printf("C: %d\n", c1);
	return 1;
}
