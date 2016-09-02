#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "PART.h"

#define INPUT_NUM	1024000000

int main(void)
{
	struct timespec t1, t2;
	int i, j;
	char *dummy;
	unsigned long *keys, *new_value;
	unsigned long elapsed_time;
	void *ret;
	FILE *fp;
	unsigned long *buf;
	unsigned long max;
	unsigned long min;

	if((fp = fopen("/home/sekwon/Public/input_file/input_random_synthetic_1024M.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}

	keys = malloc(sizeof(unsigned long) * INPUT_NUM);
	buf = malloc(sizeof(unsigned long) * INPUT_NUM);
	memset(buf, 0, sizeof(unsigned long) * INPUT_NUM);

	for (i = 0; i < INPUT_NUM; i++) {
//		keys[i] = i;
		fscanf(fp, "%lu", &keys[i]);
	}

	fclose(fp);

	max = keys[0];
	min = keys[0];
	for (i = 1; i < INPUT_NUM; i++) {
		if (keys[i] > max)
			max = keys[i];
		if (keys[i] < min)
			min = keys[i];
	}

	art_tree *t = malloc(sizeof(art_tree));
	if (art_tree_init(t)) {
		printf("art_tree_init fails!\n");
	}
	flush_buffer_nocount(t, sizeof(art_tree), true);

	/* Insertion */
	dummy = (char *)malloc(15*1024*1024);	// [kh] CPU cache size is 15M?
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < INPUT_NUM; i++) {
		art_insert(t, keys[i], sizeof(unsigned long), &keys[i]);
//		printf("%d\n", i);
	//	art_insert(t, (unsigned char *)&keys[i], sizeof(unsigned long), &keys[i]);
		//	printf("Insert error!\n");
		//	exit(1);
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insertion Time = %lu ns\n", elapsed_time);

	/* Check space overhead */
	printf("sizeof(art_node4) = %lu\n", sizeof(art_node4));
	printf("sizeof(art_node16) = %lu\n", sizeof(art_node16));
	printf("sizeof(art_node48) = %lu\n", sizeof(art_node48));
	printf("sizeof(art_node256) = %lu\n", sizeof(art_node256));
	printf("sizeof(art_leaf) = %lu\n", sizeof(art_leaf));
	printf("Total space = %lu byte\n", (node4_count * sizeof(art_node4) + node16_count * sizeof(art_node16) 
				+ node48_count * sizeof(art_node48) + node256_count * sizeof(art_node256) + leaf_count * sizeof(art_leaf)));
	printf("Space efficiency = %lu\n", (node4_count * sizeof(art_node4) + node16_count * sizeof(art_node16) 
				+ node48_count * sizeof(art_node48) + node256_count * sizeof(art_node256) + leaf_count * sizeof(art_leaf)) / INPUT_NUM);
	printf("node4_count = %lu\n", node4_count);
	printf("node16_count = %lu\n", node16_count);
	printf("node48_count = %lu\n", node48_count);
	printf("node256_count = %lu\n", node256_count);
	printf("leaf count = %lu\n", leaf_count);
	printf("clflush count = %lu\n", clflush_count);
	printf("mfence count = %lu\n", mfence_count);
	printf("path compression count = %lu\n", path_comp_count);

	/* Lookup */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for (i = 0; i < INPUT_NUM; i++) {
		ret = art_search(t, keys[i], sizeof(unsigned long));
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		}
	//	else {
	//		printf("Search value = %lu	count = %lu\n", *(unsigned long *)ret, i);
	//		sleep(1);
	//	}
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);	
	printf("Search Time = %lu ns\n", elapsed_time);

	/* Range scan 0.1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, INPUT_NUM / 1000, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 0.1% = %lu ns\n", elapsed_time);

	/* Range scan 1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, INPUT_NUM / 100, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 1% = %lu ns\n", elapsed_time);

	/* Range scan 10% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, INPUT_NUM / 10, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 10% = %lu ns\n", elapsed_time);
#ifdef sekwon
	/* Update */
	new_value = malloc(sizeof(unsigned long) * INPUT_NUM);
	for (i = 0; i < INPUT_NUM; i++)
		new_value[i] = i;
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for (i = 0; i < INPUT_NUM; i++)
		Update(t, keys[i], &new_value[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);	
	printf("Update Time = %lu ns\n", elapsed_time);

	/* Delete */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for (i = 0; i < 100; i++)
		Delete(t, keys[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Delete Time = %lu ns\n", elapsed_time);

	for (i = 0; i < 50000100; i++)
		printf("buf[%d] = %lu\n", i, buf[i]);
#endif
	return 0;
}
