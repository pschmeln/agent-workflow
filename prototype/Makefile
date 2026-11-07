CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -I firmware/uno_r4_wifi

.PHONY: test clean

test: build/test_cane build/print_fixture
	./build/test_cane
	./build/print_fixture > build/packets.json
	node --test ../web/logic.test.mjs

build/test_cane: firmware/uno_r4_wifi/cane_logic.c firmware/uno_r4_wifi/cane_logic.h tests/test_cane.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ firmware/uno_r4_wifi/cane_logic.c tests/test_cane.c -lm

build/print_fixture: firmware/uno_r4_wifi/cane_logic.c firmware/uno_r4_wifi/cane_logic.h tests/print_fixture.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ firmware/uno_r4_wifi/cane_logic.c tests/print_fixture.c -lm

clean:
	rm -rf build
