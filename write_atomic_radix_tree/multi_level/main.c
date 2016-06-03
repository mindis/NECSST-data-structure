#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "wm_radix_tree.h"

int main(void)
{
	struct timespec t1, t2;
	int i, j;
	char *dummy;
	unsigned long *keys;
	unsigned long elapsed_time;
	void *ret;
	FILE *fp;
/*
	if((fp = fopen("../../input_file/input_2billion.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}
*/
	keys = malloc(sizeof(unsigned long) * 100000100);

	for (i = 0; i < 100000100; i++) {
		keys[i] = i;
//		fscanf(fp, "%lu", &keys[i]);
	}
/*	
	for (i = 0, j = 0; i < 100000100; i++, j++) {
		keys[i] = j;
		i++;
//		fscanf(fp, "%d", &keys[i]);
	}

	for (i = 1, j = 100000099; i < 100000100; i++, j--) {
		keys[i] = j;
		i++;
	}
*/		

	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024);


	tree *t = initTree();

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000000; i++) {
		if (Insert(t, keys[i], &keys[i]) < 0) {
			printf("Insert error!\n");
			exit(1);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);

	printf("Bulk load Time = %lu ns\n",elapsed_time);
	printf("sizeof(node) = %d\n", sizeof(node));

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 100000000; i < 100000100; i++)
		Insert(t, keys[i], &keys[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insert Time = %lu ns\n", elapsed_time);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000100; i++) {
		ret = Lookup(t, keys[i]);
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		} /*
		else {
			printf("Search value = %lu\n", *(unsigned long *)ret);
		//	sleep(1);
		}*/
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Search Time = %lu ns\n", elapsed_time);
	printf("level descript time = %lu ns\n", level_descript_time);

	return 0;
}
