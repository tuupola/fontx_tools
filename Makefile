all: dumpfontx bdf2fontx

build:
	mkdir build

dumpfontx: build src/dumpfontx.c src/fontx2.c
	gcc -o build/dumpfontx src/dumpfontx.c src/fontx2.c -I./include

bdf2fontx: build src/bdf2fontx.c
	gcc -Wall -o build/bdf2fontx src/bdf2fontx.c -I./include

install: dumpfontx bdf2fontx
	cp build/* ~/bin/

clean:
	rm -rf build
