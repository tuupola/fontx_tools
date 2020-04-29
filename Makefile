all: dumpfontx bdf2fontx

dumpfontx: src/dumpfontx.c
	gcc -o dumpfontx src/dumpfontx.c src/fontx2.c -I./include

bdf2fontx: src/bdf2fontx.c
	gcc -Wall -o bdf2fontx src/bdf2fontx.c -I./include

clean:
	rm dumpfontx bdf2fontx
