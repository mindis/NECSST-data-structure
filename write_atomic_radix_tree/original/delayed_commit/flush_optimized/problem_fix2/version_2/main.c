#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "wart.h"

#define INPUT_NUM	10000000

int main(void)
{
	struct timespec t1, t2;
	int i;
	char *dummy;
	unsigned long *keys, *new_value;
	unsigned long elapsed_time;
	void *ret;
	FILE *fp;
	unsigned long *buf;
	char line[1024];
	FILE *fp2;
	unsigned long nVmSize = 0;
	unsigned long nVmRss = 0;
	unsigned long max;
	unsigned long min;

	printf("sizeof(node) = %d\n", sizeof(node));

	if((fp = fopen("/home/sekwon/Public/input_file/input_sparse_random_10million.txt","r")) == NULL)
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

	for (i = 0; i < INPUT_NUM; i++)
		keys[i] = (keys[i] >> 1);

	max = keys[0];
	min = keys[0];
	for (i = 1; i < INPUT_NUM; i++) {
		if (keys[i] > max)
			max = keys[i];
		if (keys[i] < min)
			min = keys[i];
	}

	printf("max = %lu\n", max);
	printf("min = %lu\n", min);

	tree *t = initTree();
	flush_buffer(t, 8, true);

	/* Insertion */
	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < INPUT_NUM; i++) {
		if (Insert(&t, keys[i], &keys[i]) < 0) {
			printf("Insert error!\n");
			exit(1);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insertion Time = %lu ns\n", elapsed_time);

	/* Check space overhead */
	sprintf(line, "/proc/%d/status", getpid());
	fp2 = fopen(line, "r");
	if (fp2 == NULL)
		return ;

	while (fgets(line, 1024, fp2) != NULL) {
		if (strstr(line, "VmSize")) {
			char tmp[32];
			char size[32];
			sscanf(line, "%s%s", tmp, size);
			nVmSize = atoi(size);
			printf("nVmSize = %lu KB\n", nVmSize);
		}
		else if (strstr(line, "VmRSS")) {
			char tmp[32];
			char size[32];
			sscanf(line, "%s%s", tmp, size);
			nVmRss = atoi(size);
			printf("nVmRss = %lu KB\n", nVmRss);
			break;
		}
	}
	fclose(fp2);

	/* Lookup */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	for (i = 0; i < INPUT_NUM; i++) {
		ret = Lookup(t, keys[i]);	
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

	return 0;
}
