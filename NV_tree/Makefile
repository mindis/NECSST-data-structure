BIN_DIR=./
CFLAGS= -c 

all:
	gcc $(CFLAGS) NV-tree.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)NV_dense_1024M NV-tree.o main.o -lrt -O3
	rm *.o
