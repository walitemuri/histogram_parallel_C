CFLAGS = -Wall -std=c11

all: histogram

histogram: histogram.c
	gcc $(CFLAGS) histogram.c -o histogram

clean: 
	rm -f *.o histogram