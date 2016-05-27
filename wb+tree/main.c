/* *************************************************** */
// Main for WB-tree (Write Atomic Btree Implemenation //
// Made by Jihye Seo sjh8763@unist.ac.kr //
// Modified by Wookhee Kim okie90@unist.ac.kr //
/* ************************************************** */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>
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
	struct timeval t1, t2;
	int i;
	char *dummy;
	unsigned long *keys;
	long elapsed_time;
	void *ret;
	FILE *fp;
/*
	if ((fp = fopen("../input_file/input_2billion.txt","r")) == NULL)
	{
		puts("error");
		exit(0);
	}
*/
	keys = malloc(sizeof(unsigned long) * 100000100);
	for(i = 0; i < 100000100; i++) {
		keys[i] = i;
	//	fscanf(fp, "%d", &keys[i]);
	}

	dummy = (char *)malloc(15*1024*1024);
	memset(dummy, 0, 15*1024*1024);
	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);
//	flush_buffer((void *)dummy, 15*1024*1024);

	tree *t = initTree();

	gettimeofday(&t1, NULL);
	for(i = 0; i < 100000000; i++)
		Insert(t, keys[i], &keys[i]);
	gettimeofday(&t2, NULL);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000;
	elapsed_time += (t2.tv_usec - t1.tv_usec);

	printf("Bulk load Time = %ld us\n",elapsed_time);
	printf("sizeof(node) = %d\n", sizeof(node));

	memset(dummy, 0, 15*1024*1024);
	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);

	gettimeofday(&t1, NULL);
	for(i = 99999999; i < 100000100; i++)
		Insert(t, keys[i], &keys[i]);
	gettimeofday(&t2, NULL);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000;
	elapsed_time += (t2.tv_usec - t1.tv_usec);
	printf("Insert Time = %ld us\n", elapsed_time);

	memset(dummy, 0, 15*1024*1024);
	clflush_range((void *)dummy, (void *)dummy + 15*1024*1024);
//	flush_buffer((void *)dummy, 15*1024*1024);

	gettimeofday(&t1, NULL);
	for(i = 0; i < 100000100; i++) {
		ret = (void *)Lookup(t, keys[i]);
	//	if (ret == NULL) {
	//		printf("There is no key[%d] = %ld\n", i, keys[i]);
	//		exit(1);
	//	}
		/*
		else {
			printf("Search value = %lu\n", *(unsigned long*)ret);
			sleep(1);
		}
		*/
	}
	gettimeofday(&t2, NULL);
	elapsed_time = (t2.tv_sec - t1.tv_sec) * 1000000;
	elapsed_time += (t2.tv_usec - t1.tv_usec);
	printf("Search Time = %ld us\n", elapsed_time);

	return 0;
}
