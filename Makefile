PREFIX=/usr/local
CC=gcc
CFLAGS=-O2 -DNDEBUG
LIBFILES=libchalloc.so
INCFILES=challoc.h
DEVELFLAGS=-DCHALLOC_TEST -g -pedantic -std=c89 -Wall -Wextra -Wno-unused

test: challoc.c challoc.h
	$(CC) $(DEVELFLAGS) -o $@ $< $(LDFLAGS)
	@./test; if [[ "$$?" -eq "0" ]]; then echo "Tests passed."; else echo "Tests failed"; fi

.PHONY: install
install: libchalloc.so
	mkdir -p $(PREFIX)/include/challoc
	mkdir -p $(PREFIX)/lib
	cp -vf $(INCFILES) $(PREFIX)/include/challoc/
	cp -vf $(LIBFILES) $(PREFIX)/lib/

.PHONY: uninstall
uninstall:
	rm -vf $(foreach file,$(INCFILES),$(PREFIX)/include/challoc/$(file))
	rmdir -v $(PREFIX)/include/challoc
	rm -vf $(foreach file,$(LIBFILES),$(PREFIX)/lib/$(file))

libchalloc.so: challoc.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,libchalloc.so -o $@ $< $(LDFLAGS)

challoc.o: challoc.c challoc.h
	$(CC) $(CFLAGS) -fPIC -c -o $@ $< $(LDFLAGS)

check-syntax:
	$(CC) $(DEVELFLAGS) -fsyntax-only $(CHK_SOURCES)

