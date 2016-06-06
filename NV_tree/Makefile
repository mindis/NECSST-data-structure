BIN_DIR=./
CFLAGS= -c 

all:
	gcc $(CFLAGS) NV-tree.c -lrt
	gcc $(CFLAGS) main.c -lrt
	gcc -o $(BIN_DIR)NV-tree NV-tree.o main.o -lrt
	rm *.o
