PREFIX=/usr
BINDIR=$(PREFIX)/bin

CC=gcc -g
INSTALL=ginstall

all:	simpletun
distclean:	clean

clean:
	rm simpletun


install: all
	$(INSTALL) -D simpletun $(DESTDIR)$(BINDIR)/simpletun

macmask:
	$(CC) simpletun.c buffer.c -o simpletun
