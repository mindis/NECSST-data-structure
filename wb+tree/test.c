#include <stdio.h>
#include <stdint.h>

int main(void) {
	char a[8];
	char b[8];
	int i;

	for (i = 0; i < 8; i++)
	{
		b[i] = i;
		a[i] = 8-i;
	}

	*((uint64_t *)a) = *((uint64_t *)b);

	for (i = 0; i < 8; i++)
		printf("a[%d] = %d\n", i, a[i]);

	return 0;
}
