BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) w_radix_tree.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)WART w_radix_tree.o main.o -lrt -O3
	rm *.o
