#include <stdio.h>
#include <string.h>

#define MAX	"~"

int main(void)
{
	char *a = MAX;
	printf("%s\n", a);
	printf("sizeof(MAX) = %d\n", strlen(MAX));

	return 0;
}
