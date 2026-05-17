CC = cc
CFLAGS = -Wall -Wextra -O2 -g -Isrc
LDFLAGS =

SRC = src/z80.c src/spectrum.c src/tzx.c
HDR = src/z80.h src/spectrum.h src/tzx.h src/rom.h src/splash.h

all: z80_test spectrum_test circle_zx

z80_test: tests/z80_test.c src/z80.c src/z80.h
	$(CC) $(CFLAGS) -o z80_test tests/z80_test.c src/z80.c $(LDFLAGS)

spectrum_test: tests/spectrum_test.c $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o spectrum_test tests/spectrum_test.c $(SRC) $(LDFLAGS)

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
	rm -rf z80_test spectrum_test *.o *.dSYM src/splash.h src/keyboard_layout.h
	$(MAKE) -C frontends/bare-metal clean

.PHONY: all test fulltest clean circle_zx
