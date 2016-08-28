#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "PART.h"

int main(void)
{
	struct timespec t1, t2;
	int i, len;
	char *dummy;
	unsigned long *new_value;
	char buf[512];
	unsigned long elapsed_time, total_elapsed_time, input_count = 0;
	void *ret;
	FILE *fp;

	if((fp = fopen("/home/sekwon/Public/input_file/words.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
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
	total_elapsed_time = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		len = strlen(buf);
		buf[len - 1] = '\0';
		clock_gettime(CLOCK_MONOTONIC, &t1);
		art_insert(t, (unsigned char *)buf, len, (void *)buf);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_time += (t2.tv_nsec - t1.tv_nsec);
		total_elapsed_time += elapsed_time;
		input_count++;
	}
	printf("Insertion Time = %lu ns\n", total_elapsed_time);

	/* Check space overhead */
	printf("sizeof(art_node4) = %lu\n", sizeof(art_node4));
	printf("sizeof(art_node16) = %lu\n", sizeof(art_node16));
	printf("sizeof(art_node48) = %lu\n", sizeof(art_node48));
	printf("sizeof(art_node256) = %lu\n", sizeof(art_node256));
	printf("sizeof(art_leaf) = %lu\n", sizeof(art_leaf) + len);
	printf("Total space = %lu byte\n", (node4_count * sizeof(art_node4) + node16_count * sizeof(art_node16) 
				+ node48_count * sizeof(art_node48) + node256_count * sizeof(art_node256) + leaf_count * (sizeof(art_leaf) + len)));
	printf("Space efficiency = %lu\n", (node4_count * sizeof(art_node4) + node16_count * sizeof(art_node16) 
				+ node48_count * sizeof(art_node48) + node256_count * sizeof(art_node256) + leaf_count * (sizeof(art_leaf) + len)) / input_count);
	printf("node4_count = %lu\n", node4_count);
	printf("node16_count = %lu\n", node16_count);
	printf("node48_count = %lu\n", node48_count);
	printf("node256_count = %lu\n", node256_count);
	printf("leaf count = %lu\n", leaf_count);
	printf("clflush count = %lu\n", clflush_count);
	printf("mfence count = %lu\n", mfence_count);

	fseek(fp, 0, SEEK_SET);	

	/* Lookup */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	while (fgets(buf, sizeof(buf), fp)) {
		len = strlen(buf);
		buf[len - 1] = '\0';
		clock_gettime(CLOCK_MONOTONIC, &t1);
		ret = art_search(t, (unsigned char *)buf, len);
		if (ret == NULL) {
			printf("There is no key = %s\n", buf);
			exit(1);
		}
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_time += (t2.tv_nsec - t1.tv_nsec);
		total_elapsed_time += elapsed_time;
	}
	printf("Search Time = %lu ns\n", total_elapsed_time);
	fclose(fp);
#ifdef sekwon
	/* Range scan 0.1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, min, INPUT_NUM / 1000, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 0.1% = %lu ns\n", elapsed_time);

	/* Range scan 1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, min, INPUT_NUM / 100, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 1% = %lu ns\n", elapsed_time);

	/* Range scan 10% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, min, INPUT_NUM / 10, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 10% = %lu ns\n", elapsed_time);

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
#endif
	/* Delete */
//	memset(dummy, 0, 15*1024*1024);
//	flush_buffer((void *)dummy, 15*1024*1024, true);
//	clock_gettime(CLOCK_MONOTONIC, &t1);
//	for (i = 0; i < 100; i++)
//		Delete(t, keys[i]);
//	clock_gettime(CLOCK_MONOTONIC, &t2);
//	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
//	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
//	printf("Delete Time = %lu ns\n", elapsed_time);

//	for (i = 0; i < 50000100; i++)
//		printf("buf[%d] = %lu\n", i, buf[i]);

	return 0;
}
