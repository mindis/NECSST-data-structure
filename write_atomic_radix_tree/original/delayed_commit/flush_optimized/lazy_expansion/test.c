#include <stdio.h>
#include <malloc.h>

int main(void)
{
	int a = 1;
	int *b = &a;
	(*b)++;

	printf("*b = %d\n", *b);
	printf("necsst\n");

	return 0;
}
