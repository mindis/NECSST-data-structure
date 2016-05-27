#include <stdio.h>

int main(void)
{
	int a = 1;
	void *b = &a;
	int c;

	c = *(int *)b;

	printf("c = %d\n", c);
	
	return 0;
}
