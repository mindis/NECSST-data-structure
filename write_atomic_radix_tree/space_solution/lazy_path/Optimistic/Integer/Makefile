BIN_DIR = ./
CFLAGS = -c

all:
	gcc $(CFLAGS) art_lpo.c -lrt -O3
	gcc $(CFLAGS) main.c -lrt -O3
	gcc -o $(BIN_DIR)ART_LPO_synthetic_1024M_noflush art_lpo.o main.o -lrt -O3
	rm *.o
