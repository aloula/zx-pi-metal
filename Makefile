CC = cc
CFLAGS = -Wall -Wextra -O2 -g -Isrc
LDFLAGS =
SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)

SRC = src/z80.c src/spectrum.c src/tzx.c
HDR = src/z80.h src/spectrum.h src/tzx.h src/rom.h

all: z80_test spectrum_test zxsdl

z80_test: tests/z80_test.c src/z80.c src/z80.h
	$(CC) $(CFLAGS) -o z80_test tests/z80_test.c src/z80.c $(LDFLAGS)

spectrum_test: tests/spectrum_test.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o spectrum_test tests/spectrum_test.c $(SRC) $(LDFLAGS)

zxsdl: frontends/sdl/zxsdl.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o zxsdl frontends/sdl/zxsdl.c $(SRC) $(SDL_LIBS)

circle_zx:
	$(MAKE) -C frontends/bare-metal CIRCLEHOME=../../circle

test: z80_test spectrum_test
	./z80_test
	./spectrum_test

fulltest: z80_test spectrum_test
	./z80_test
	./z80_test --zexdoc
	./z80_test --zexall
	./spectrum_test

clean:
	rm -rf z80_test spectrum_test zxsdl *.o *.dSYM
	$(MAKE) -C frontends/bare-metal clean

.PHONY: all test fulltest clean circle_zx
