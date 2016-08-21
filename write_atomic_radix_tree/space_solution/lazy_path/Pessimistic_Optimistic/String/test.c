#include <stdio.h>
#include <string.h>

int main(void)
{
	char buf[512];
	int len;
	FILE *f = fopen("/home/sekwon/Public/input_file/uuid.txt", "r");
	if (f == NULL) {
		printf("open fail\n");
		return 0;
	}

	while (fgets(buf, sizeof(buf), f)) {
		len = strlen(buf);
		buf[len - 1] = '\0';
		printf("%s\n", buf);
		printf("len = %d\n", len);
	}
	return 0;
}
