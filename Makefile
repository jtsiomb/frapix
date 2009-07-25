PREFIX=/usr/local
obj = glfps.o
so = libglfps.so

CC = gcc
CFLAGS = -pedantic -Wall -fPIC -g
LDFLAGS =

$(so): $(obj)
	$(CC) -o $@ -shared $(obj) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(obj) $(so)

.PHONY: install
install:
	cp $(so) $(PREFIX)/lib/$(so)
	cp glfps $(PREFIX)/bin/glfps

.PHONY: uninstall
uninstall:
	rm $(PREFIX)/lib/$(so)
	rm $(PREFIX)/bin/glfps
