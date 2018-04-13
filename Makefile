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

simpletun: simpletun.c
	$(CC) simpletun.c buffer.c minicomp/minicomp.c minicomp/dc_bytecounter.c -o simpletun -lz -lm -lpthread -std=gnu99
