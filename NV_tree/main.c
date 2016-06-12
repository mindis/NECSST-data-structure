#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "NV-tree.h"

int main(void)
{
	struct timespec t1, t2;
	int i;
	char *dummy;
	unsigned long *keys;
	unsigned long elapsed_time;
	void *ret;
	FILE *fp;
	unsigned long *buf;


	if ((fp = fopen("/home/sekwon/Public/input_file/input_200million.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}


	keys = malloc(sizeof(unsigned long) * 100000100);
	buf = malloc(sizeof(unsigned long) * 100000100);
	memset(buf, 0, sizeof(unsigned long) * 100000100);
	for(i = 0; i < 100000100; i++) {
	//	keys[i] = i + 1;
		fscanf(fp, "%lu", &keys[i]);
	}

	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
//	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	tree *t = initTree();

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000100; i++) {
		printf("insert count = %d\n", i);
		if (Insert(t, keys[i], &keys[i]) < 0)
			return 0;
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);

	printf("sizeof(IN) = %d\n", sizeof(IN));
	printf("sizeof(PLN) = %d\n", sizeof(PLN));
	printf("sizeof(LN) = %d\n", sizeof(LN));
	printf("Bulk load Time = %lu ns\n",elapsed_time);

	memset(dummy, 0, 15*1024*1024);
//	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
/*
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 99999999; i < 100000100; i++)
		Insert(t, keys[i], &keys[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insert Time = %lu ns\n", elapsed_time);
*/
	memset(dummy, 0, 15*1024*1024);
//	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000100; i++) {
		ret = (void *)Lookup(t, keys[i]);		
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		}/*
		else {
			printf("Search value = %lu\n", *(unsigned long*)ret);
		//	sleep(1);
		}*/
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Search Time = %lu ns\n", elapsed_time);
/*
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, 0, 100000100, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);

	printf("Range search time = %lu ns\n", elapsed_time);
*/
//	for (i = 0; i < 50000100; i++)
//		printf("buf[%d] = %lu\n", i, buf[i]);
	return 0;
}
