all: dumpfontx bdf2fontx fontx2png

build:
	mkdir build

dumpfontx: build src/dumpfontx.c src/fontx2.c
	gcc -o build/dumpfontx src/dumpfontx.c src/fontx2.c -I./include

bdf2fontx: build src/bdf2fontx.c
	gcc -Wall -o build/bdf2fontx src/bdf2fontx.c -I./include

fontx2png: build src/fontx2png.c src/fontx2.c
	gcc -o build/fontx2png src/fontx2png.c src/fontx2.c -I./include -lgd

install: dumpfontx bdf2fontx fontx2png
	cp build/* ~/bin/

clean:
	rm -rf build
