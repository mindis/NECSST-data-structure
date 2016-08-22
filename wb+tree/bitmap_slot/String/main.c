#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "wbtree.h"

int main(void)
{
	struct timespec t1, t2;
	int i, len;
	char *dummy;
	unsigned long *new_value;
	char buf[512];
	unsigned long elapsed_time, total_elapsed_time, input_count = 0, search_count = 0;
	void *ret;
	FILE *fp;

	if((fp = fopen("/home/sekwon/Public/input_file/uuid.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}

	tree *t = initTree();

	/* Insertion */
	dummy = (char *)malloc(15*1024*1024);	// [kh] CPU cache size is 15M?
	memset(dummy, 0, 15*1024*1024);
	flush_buffer_nocount((void *)dummy, 15*1024*1024, true);
	total_elapsed_time = 0;
	while (fgets(buf, sizeof(buf), fp)) {
		len = strlen(buf);
		buf[len - 1] = '\0';
		clock_gettime(CLOCK_MONOTONIC, &t1);
		Insert(t, (unsigned char *)buf, len, (void *)buf);
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_time += (t2.tv_nsec - t1.tv_nsec);
		total_elapsed_time += elapsed_time;
		input_count++;
	}
	printf("Insertion Time = %lu ns\n", total_elapsed_time);

	/* Check space overhead */
	printf("Total space = %lu byte\n", node_count * sizeof(node) + input_count * (sizeof(key_item) + len));
	printf("Space efficiency = %lu\n", (node_count * sizeof(node) + input_count * (sizeof(key_item) + len)) / input_count);
	printf("node count = %lu\n", node_count);
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
		ret = Lookup(t, (unsigned char *)buf, len);
		if (ret == NULL) {
			printf("There is no key = %s\n", buf);
			printf("search count = %lu\n", search_count);
			exit(1);
		}
		clock_gettime(CLOCK_MONOTONIC, &t2);
		elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
		elapsed_time += (t2.tv_nsec - t1.tv_nsec);
		total_elapsed_time += elapsed_time;
		search_count++;
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
