PROG      = dwmst
PREFIX	 ?= /usr/local
BINPREFIX = $(PREFIX)/bin

LIBS   = -lX11 -lasound
CFLAGS += -Os -s -pedantic -Wall -std=c99 -D_DEFAULT_SOURCE

$(PROG): dwmst.c
	$(CC) $(CFLAGS) $(LIBS) -o $@ $<

install:
	install -Dm755 $(PROG) $(DESTDIR)$(BINPREFIX)/$(PROG)

uninstall:
	rm -f ${BINPREFIX}/${PROG}

clean:
	rm -f $(PROG)
