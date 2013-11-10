CFLAGS = -D_FILE_OFFSET_BITS=64

all:
	$(CC) $(CFLAGS) -Wall -lm -o bin/bitmap2pbm src/bitmap2pbm.c

clean:
	rm bin/bitmap2pbm
