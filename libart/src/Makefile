BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) art.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)ART art.o main.o -lrt -O3
	rm *.o
