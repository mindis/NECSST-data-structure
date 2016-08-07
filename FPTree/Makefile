BIN_DIR=./
CFLAGS= -c 

all:
	gcc $(CFLAGS) FPTree.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)FPTree_dense_1024M FPTree.o main.o -lrt -O3
	rm *.o
