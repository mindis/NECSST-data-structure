BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) art_l.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)ART_L_synthetic_1024M_noflush art_l.o main.o -lrt -O3
	rm *.o
