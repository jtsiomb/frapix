PREFIX=/usr/local
src = $(wildcard src/*.c)
obj = $(src:.c=.o)
so = libfrapix.so

CC = gcc
CFLAGS = -pedantic -Wall -fPIC -g
LDFLAGS = -ldl -limago

$(so): $(obj)
	$(CC) -o $@ -shared $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(so)

.PHONY: install
install:
	cp $(so) $(PREFIX)/lib/$(so)
	cp frapix $(PREFIX)/bin/frapix
	ldconfig || true

.PHONY: uninstall
uninstall:
	rm $(PREFIX)/lib/$(so)
	rm $(PREFIX)/bin/frapix
