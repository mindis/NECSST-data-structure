#include <stdio.h>
#include <limits.h>

int main(void)
{
	unsigned long max_key;
	unsigned long min_key = 1234657489798456;

	max_key = 0x1UL << 63;
	min_key = ULONG_MAX >> 64;

	printf("max keys = %lu\n", max_key);
	printf("min keys = %lu\n", min_key);

	return 0;
}
