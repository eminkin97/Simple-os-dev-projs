#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<errno.h>
#include"my_pthread_t.h"
#include"totalrecall.h"

void function3() {

	char * eli = (char*)malloc(6000);
	eli[4] = 'L';
	printf("***ELI[5999]***: %c\n" , eli[4]);
	printf("ELI addr : %d\n" , (char*)eli+4);


	my_pthread_exit(NULL);

}

void function2() {

	int * lots = (int*)malloc(4000000);
	lots[300] = 7;
	printf("*************************lat : %d\n" , lots[300]); 
	int* loter = (int*)malloc(5000000);
	if(loter == NULL) {
		printf("*************************cheese\n");
	}
	free(lots);
	free(loter);
	void (*funct3)() = &function3;
	my_pthread_t tid1;
	my_pthread_create(&tid1 , NULL , (void*)funct3 , NULL);
	char * less = (char*)malloc(sizeof(int) * 18);
	*less = 4;

	less[4] = 'e';
	printf("***less***: %c\n" , less[4]);
	my_pthread_join(tid1 , NULL);
	printf("***************less: %c\n" , less[4]);
	printf("less addr: %d\n" , (char*)less + 4);
	free(less);	
	my_pthread_exit(NULL);

}

void function1() {
	printf("INSIDE FUNCTION 1\n");
	char * test1 = (char*)malloc(10);
	test1[4] = 'd';
	printf("***test1[4]*** : %c\n" , test1[4]);
	void (*funct2)() = &function2;
	my_pthread_t tid0;
	my_pthread_create(&tid0 , NULL , (void*)funct2 , NULL);
	printf("1 : %c\n"  , test1[4]);
	free(test1);
	my_pthread_join(tid0,NULL);
	//printf("test1[4] = %c\n" , test1[4]);
	//printf("addr: %d\n" , (char*)test1+4);

	my_pthread_exit(NULL);

}

void babyfunction() {
	int j = 0;
	char *baby;
	while (j < 200) {
		baby = (char*)malloc(400);
		j++;
	}
	baby[25] = 'h';
	printf("baby: %c\n" , baby[25]);

	my_pthread_exit(NULL);
}

void func_y() {
	char* meow = (char*)malloc(50000);
	int i = 0;
	while (i < 10) {
		meow[3 + 4096 * i] = 'L';
		i++;
	}
	printf("Meow : %c\n" , meow[3]);
	my_pthread_exit(NULL);

}

void func_z() {
	char* meow = (char*)malloc(6553500);
	if (meow == NULL) {
		printf("MEOW WAS NULL\n");
	}
	int i = 0;
	while (i < 1600) {
		meow[3 + 4096 * i] = 'E';
		i++;
	}
	char* meow1 = (char*)malloc(2000);
	*(meow + 6000) = 'c';
	void (*funct2)() = &func_y;
	my_pthread_t tid2;
	my_pthread_create(&tid2 , NULL , (void*)funct2, NULL);
	my_pthread_join(tid2 , NULL);

	*(meow + 5) = 'A';
	printf("MEOW 3 is %c , %d\n", meow[3] , meow+3);
	printf("MEOW 5 is %c\n", meow[5]);

	i = 0;
	while (i < 10) {
		printf("Page %d : %c\n",i, meow[3 + 4096 * i]);
		i++;
	}

	
	i = 0;
	while(i < 10) {
		printf("Page %d : %c\n",i,meow[3+4096*i]);
		char*meowmix = meow - 32;
		printf("meow: %d , mix: %d\n" , meow , meowmix);
		printf("mix + 35: %c\n" , *(meowmix + 35));
		i++;
	}
	
	my_pthread_exit(NULL);

}



int main() {
	printf("*\n*\n*\n*\n*\n*\n*\n*\n*\n");
	void (*funct1)() = &func_z;
	void (*funct2)() = &func_y;

	my_pthread_t tid1, tid2, tid3;

	//char* marsh = (char*)malloc(16);

	my_pthread_create(&tid1 , NULL , (void*)funct1, NULL);
	printf("HELLO TOM -1\n");
	//my_pthread_create(&tid3 , NULL , (void*)funct2, NULL);
	
	printf("HELLO TOM 0\n");
	my_pthread_join(tid1 , NULL);
	printf("HELLO TOM 1\n");
	//my_pthread_join(tid2 , NULL);
	printf("HELLO TOM 2\n");
	//my_pthread_join(tid3 , NULL);

	return 1;

}








