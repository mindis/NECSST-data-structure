#include <stdio.h>
#include <malloc.h>

typedef struct tag_bit {
	unsigned char a:2;
}tag_bit;

typedef struct test_bit {
	tag_bit sekwon[5];
}test_bit;

int main(void)
{
	unsigned long sor = 11;
	tag_bit *bit = malloc(2 * sizeof(tag_bit));
	bit = (tag_bit *)&sor;

	printf("%d\n", sizeof(long double));
	printf("%d\n", bit[0].a);
	printf("%d\n", bit[1].a);

	return 0;
}
