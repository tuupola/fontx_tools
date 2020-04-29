all: dumpfontx

dumpfontx: src/dumpfontx.c
	gcc -o dumpfontx src/dumpfontx.c ../copepod/fontx2.c -I. -I../copepod/include

clean:
	rm dumpfontx