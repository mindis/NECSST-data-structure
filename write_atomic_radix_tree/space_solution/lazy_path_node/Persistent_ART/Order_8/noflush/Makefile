BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) PART.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)PART_sparse_128M_8 PART.o main.o -lrt -O3
	rm *.o
