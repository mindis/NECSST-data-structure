/* *************************************************** */
// Main for WB-tree (Write Atomic Btree Implemenation //
// Made by Jihye Seo sjh8763@unist.ac.kr //
// Modified by Wookhee Kim okie90@unist.ac.kr //
/* ************************************************** */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include "wbtree.h"
/*
extern long long clftime, mbtime;

int main(int argc, char **argv){
  extern int clflush_cnt;
  struct timeval t1,t2;
  int i,numData,icnt = 0;

  long *keys;
  long key;
  void *ret;
  FILE *f;
  if(argc<2){
    printf("Usage : ./wbtree numData\n");
    exit(0); 
  }
  numData = atoi(argv[1]);
  keys= malloc(sizeof(long )*numData);
 
  // Read integer keys from file
  f = fopen("../../input/input_10M.txt","r+");
  for(i = 0; i<numData; i++){
    fscanf(f,"%d",&keys[i]);  
  }
  fclose(f);


  // Clean caches : Size of L3 cache : 20M
  char *dummy = (char *)malloc(20*1024*1024);
  memset(dummy,0,20*1024*1024);
  clflush_range((void*)dummy,(void*)dummy+20*1024*1024);

  // Initiaalize Tree
  tree *t = initTree();

  // Insert 
  clflush_cnt=0;
  gettimeofday(&t1,NULL);
  for(i = 0; i<numData; i++){
    Insert(t,(long)keys[i], (void*)keys[i]);
  }
  gettimeofday(&t2,NULL);
  long elapsed_time = (t2.tv_sec-t1.tv_sec)*1000000;
  elapsed_time += (t2.tv_usec-t1.tv_usec);

  printf("Insert Time = %ld us\n",elapsed_time);
  printf("clflush_cnt = %d\n",clflush_cnt);
  printf("clftime = %lld\n",clftime);
  printf("mbtime = %lld\n",mbtime);
  printf("sizeof(node) = %d\n",sizeof(node));

  // Clean caches : Size of L3 cache : 20M
  memset(dummy,0,20*1024*1024);
  clflush_range((void*)dummy,(void*)dummy+20*1024*1024);

  // Search 
  gettimeofday(&t1,NULL);
  for(i = 0; i<numData; i++){
    ret = (void *)Lookup(t, keys[i]);
  //  printf("%ld\n",(long)ret);
    //if(ret==NULL){
    //  printf("There is no key[%d] = %ld\n",i,keys[i]);
    //  exit(1);
   // }
  }
  gettimeofday(&t2,NULL);
  elapsed_time = (t2.tv_sec-t1.tv_sec)*1000000;
  elapsed_time += (t2.tv_usec-t1.tv_usec);
  printf("Search Time = %ld us\n",elapsed_time);
}
*/

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
/*
	if ((fp = fopen("/home/sekwon/Public/input_file/input_200million.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}
*/
	keys = malloc(sizeof(unsigned long) * 100000100);
	buf = malloc(sizeof(unsigned long) * 100000100);
	memset(buf, 0, sizeof(unsigned long) * 100000100);
	for(i = 0; i < 100000100; i++) {
		keys[i] = i;
	//	fscanf(fp, "%lu", &keys[i]);
	}

	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	tree *t = initTree();
	flush_buffer(t, 8, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000000; i++)
		Insert(t, keys[i], &keys[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);

	printf("sizeof(node) = %d\n", sizeof(node));
	printf("Bulk load Time = %lu ns\n",elapsed_time);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 99999999; i < 100000100; i++)
		Insert(t, keys[i], &keys[i]);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Insert Time = %lu ns\n", elapsed_time);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	for(i = 0; i < 100000100; i++) {
		ret = (void *)Lookup(t, keys[i]);		
		if (ret == NULL) {
			printf("There is no key[%d] = %lu\n", i, keys[i]);
			exit(1);
		}
		else {
			printf("Search value = %lu\n", *(unsigned long*)ret);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);
	printf("Search Time = %lu ns\n", elapsed_time);

	memset(dummy, 0, 15*1024*1024);
	flush_buffer((void *)dummy, 15*1024*1024, true);

	clock_gettime(CLOCK_MONOTONIC, &t1);
	Range_Lookup(t, 0, 100000100, buf);
	clock_gettime(CLOCK_MONOTONIC, &t2);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000000;
	elapsed_time += (t2.tv_nsec - t1.tv_nsec);

	printf("Range search time = %lu ns\n", elapsed_time);

//	for (i = 0; i < 50000100; i++)
//		printf("buf[%d] = %lu\n", i, buf[i]);

	return 0;
}