# Installation directories.
PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

CFLAGS ?= -Wall -Werror
override CFLAGS += -I../include
override LDFLAGS += -L../src
override LDLIBS += -lsepol

TARGETS=$(patsubst %.c,%,$(sort $(wildcard *.c)))

all: $(TARGETS)

install: all
	-mkdir -p $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGETS) $(DESTDIR)$(BINDIR)

clean:
	-rm -f $(TARGETS) *.o 

indent:
	../../scripts/Lindent $(wildcard *.[ch])

relabel:

