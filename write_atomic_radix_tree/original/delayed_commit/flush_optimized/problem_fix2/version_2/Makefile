BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) wart.c -lrt
	gcc $(CFLAGS) main.c -lrt
	gcc -o $(BIN_DIR)wart wart.o main.o -lrt
	rm *.o
