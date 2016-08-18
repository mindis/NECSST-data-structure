#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <byteswap.h>
#include "art.h"

//#define INPUT_NUM	65537
#define INPUT_NUM	16000000

int main(void)
{
	struct timespec t1, t2;
	int i, j;
	char *dummy;
	unsigned long *keys, *new_value;
	long double *_keys;
	unsigned long elapsed_time;
	void *ret;
	FILE *fp;
	unsigned long *buf;
	unsigned long max;
	unsigned long min;
	unsigned char temp[8];
	unsigned char _temp[16];

//	printf("sizeof(node) = %d\n", sizeof(node));

	if((fp = fopen("/home/sekwon/Public/input_file/input_random_synthetic_16M.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}

//	_keys = malloc(sizeof(long double) * INPUT_NUM);
	keys = malloc(sizeof(unsigned long) * INPUT_NUM);
	buf = malloc(sizeof(unsigned long) * INPUT_NUM);
	memset(buf, 0, sizeof(unsigned long) * INPUT_NUM);

	for (i = 0; i < INPUT_NUM; i++) {
//		keys[i] = i;
		fscanf(fp, "%lu", &keys[i]);
		keys[i] = __bswap_64(keys[i]);
//		memcpy(temp, &keys[i], 8);

//		for (j = 7; j >= 0; j--) {
//			_temp[(j * 2)] = (temp[j] & LOW_BIT_MASK);
//			_temp[(j * 2) + 1] = (temp[j] >> 4);
//		}
//		memcpy(&_keys[i], _temp, 16);
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
//	flush_buffer(t, sizeof(art_tree), true);
//	flush_buffer(&t, sizeof(art_tree *), true);	// [kh] t -> &t

	/* Insertion */
	dummy = (char *)malloc(15*1024*1024);	// [kh] CPU cache size is 15M?
	memset(dummy, 0, 15*1024*1024);
//	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
/*	
	unsigned char *abc = "abc";
	unsigned char *abd = "abd";
	unsigned char *abcd = "abcd";
	unsigned char *b = "b";
	unsigned char *bbbbbb = "bbbbbb";
	unsigned char *bbbbbbc = "bbbbbbc";
	unsigned char *bbbbc = "bbbbc";

//	unsigned char *abcde = "abcde";

	art_insert(t, abc, sizeof(abc), abc);
	art_insert(t, abd, sizeof(abd), abd);
	art_insert(t, abcd, sizeof(abcd), abcd);
	art_insert(t, b, sizeof(b), b);	
	art_insert(t, bbbbbb, sizeof(bbbbbb), bbbbbb);
	art_insert(t, bbbbbbc, sizeof(bbbbbbc), bbbbbbc);
	art_insert(t, bbbbc, sizeof(bbbbc), bbbbc);
//	art_insert(t, abcde, sizeof(abcde), abcde);
*/	
	/*
	unsigned char *abcdefg = "abcdefg";
	unsigned char *abcdefh = "abcdefh";
	unsigned char *abcdeij = "abcdeij";
	unsigned char *abcdklm = "abcdklm";
	unsigned char *bcdefgh = "bcdefgh";
	unsigned char *bbbbbbb = "bbbbbbb";
	*/
/*
	art_insert(t, abcdefg, sizeof(abcdefg), abcdefg);
	art_insert(t, abcdefh, sizeof(abcdefh), abcdefh);
	art_insert(t, abcdeij, sizeof(abcdeij), abcdeij);
	art_insert(t, abcdklm, sizeof(abcdklm), abcdklm);
	art_insert(t, bcdefgh, sizeof(bcdefgh), bcdefgh);
	art_insert(t, bbbbbbb, sizeof(bbbbbbb), bbbbbbb);
*/

	for(i = 0; i < INPUT_NUM; i++) {
		art_insert(t, (unsigned char *)&keys[i], sizeof(unsigned long), &keys[i]);
	//	art_insert(t, (unsigned char *)&keys[i], sizeof(unsigned long), &keys[i]);
		//	printf("Insert error!\n");
		//	exit(1);
	}

	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insertion Time = %lu ns\n", elapsed_time);

	/* Check space overhead */
//	printf("Node count = %lu\n", node_count);
//	printf("Leaf count = %lu\n", leaf_count);
//	printf("sizeof(art_node256) = %lu\n", sizeof(art_node16));
//	printf("sizeof(art_leaf) = %lu\n", sizeof(art_leaf));
//	printf("Total space = %lu byte\n", (node_count * sizeof(art_node16) + leaf_count * sizeof(art_leaf)));
//	printf("Space efficiency = %lu\n", (node_count * sizeof(art_node16) + leaf_count * sizeof(art_leaf)) / INPUT_NUM);

	/* Lookup */
	memset(dummy, 0, 15*1024*1024);
//	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	/*
	ret = art_search(t, abc, sizeof(abc));
	if (ret == NULL) {
		printf("There is no key[0]\n");
		exit(1);
	}
	ret = art_search(t, abd, sizeof(abd));
	if (ret == NULL) {
		printf("There is no key[1]\n");
		exit(1);
	}
	ret = art_search(t, abcd, sizeof(abcd));
	if (ret == NULL) {
		printf("There is no key[2]\n");
		exit(1);
	}
	ret = art_search(t, b, sizeof(b));
	if (ret == NULL) {
		printf("There is no key[3]\n");
		exit(1);
	}
	ret = art_search(t, bbbbbb, sizeof(bbbbbb));
	if (ret == NULL) {
		printf("There is no key[4]\n");
		exit(1);
	}	
	ret = art_search(t, bbbbbbc, sizeof(bbbbbbc));
	if (ret == NULL) {
		printf("There is no key[5]\n");
		exit(1);
	}
	ret = art_search(t, bbbbc, sizeof(bbbbc));
	if (ret == NULL) {
		printf("There is no key[6]\n");
		exit(1);
	}
*/

/*
	ret = art_search(t, abcdefg, sizeof(abcdefg));
	if (ret == NULL) {
		printf("There is no key[0]\n");
		exit(1);
	}
	ret = art_search(t, abcdefh, sizeof(abcdefh));
	if (ret == NULL) {
		printf("There is no key[1]\n");
		exit(1);
	}
	ret = art_search(t, abcdeij, sizeof(abcdeij));
	if (ret == NULL) {
		printf("There is no key[2]\n");
		exit(1);
	}
	ret = art_search(t, abcdklm, sizeof(abcdklm));
	if (ret == NULL) {
		printf("There is no key[3]\n");
		exit(1);
	}
	ret = art_search(t, bcdefgh, sizeof(bcdefgh));
	if (ret == NULL) {
		printf("There is no key[4]\n");
		exit(1);
	}
	ret = art_search(t, bbbbbbb, sizeof(bbbbbbb));
	if (ret == NULL) {
		printf("There is no key[5]\n");
		exit(1);
	}
	return 0;
*/
	
	for (i = 0; i < INPUT_NUM; i++) {
		ret = art_search(t, (unsigned char *)&keys[i], sizeof(unsigned long));
	//	ret = art_search(t, (unsigned char*)&keys[i], sizeof(unsigned long));
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
#ifdef sekwon
	/* Range scan 0.1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, min, INPUT_NUM / 1000, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 0.1% = %lu ns\n", elapsed_time);

	/* Range scan 1% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, min, INPUT_NUM / 100, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Range scan 1% = %lu ns\n", elapsed_time);

	/* Range scan 10% */
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);
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
	flush_buffer((void *)dummy, 15*1024*1024, true);
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
