#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

typedef struct pointer_array ptr;

struct pointer_array {
	void *entry_ptr[16];
};

int main(void)
{
	int i;
	void *a;
	void *b;
	int c=1;

	/*
	ptr *sekwon = (ptr *)memalign(64,sizeof(ptr));
	memset(sekwon, 0, sizeof(ptr));
	*/
	ptr *prev = (ptr *)malloc(sizeof(ptr));
	printf("sizeof(prev) = %d\n", sizeof(prev));
	printf("sizeof(*prev) = %d\n", sizeof(*prev));
	ptr *next = prev;
	prev = (ptr *)malloc(sizeof(ptr));
	free(next);
/*
	a = &c;
	b = a;

	printf("b = %d\n", *(int*)b);

	for(i = 0; i < 16; i++) {
		if (sekwon->entry_ptr[i] == NULL)
			printf("%d pointer is NULL\n", i);
		else
			printf("%d pointer is not NULL\n", i);
	}
*/
	return 0;
}
