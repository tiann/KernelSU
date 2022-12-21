# Installation directories.
PREFIX ?= /usr
INCLUDEDIR ?= $(PREFIX)/include
LIBDIR ?= $(PREFIX)/lib
SHLIBDIR ?= /lib
RANLIB ?= ranlib
CILDIR ?= ../cil

VERSION = $(shell cat ../VERSION)
LIBVERSION = 2

LEX = flex
CIL_GENERATED = $(CILDIR)/src/cil_lexer.c

LIBA=libsepol.a 
TARGET=libsepol.so
LIBPC=libsepol.pc
LIBMAP=libsepol.map
LIBSO=$(TARGET).$(LIBVERSION)
OBJS= $(patsubst %.c,%.o,$(sort $(wildcard *.c)))
LOBJS= $(patsubst %.c,%.lo,$(sort $(wildcard *.c)))
CFLAGS ?= -Werror -Wall -W -Wundef -Wshadow -Wmissing-format-attribute -O2 -fno-semantic-interposition

override CFLAGS += -I. -I../include -D_GNU_SOURCE

ifneq ($(DISABLE_CIL),y)
OBJS += $(sort $(patsubst %.c,%.o,$(sort $(wildcard $(CILDIR)/src/*.c)) $(CIL_GENERATED)))
LOBJS += $(sort $(patsubst %.c,%.lo,$(sort $(wildcard $(CILDIR)/src/*.c)) $(CIL_GENERATED)))
override CFLAGS += -I$(CILDIR)/include
endif

# check for reallocarray(3) availability
H := \#
ifeq (yes,$(shell printf '${H}define _GNU_SOURCE\n${H}include <stdlib.h>\nint main(void){void*p=reallocarray(NULL, 1, sizeof(char));return 0;}' | $(CC) -x c -o /dev/null - >/dev/null 2>&1 && echo yes))
override CFLAGS += -DHAVE_REALLOCARRAY
endif

LD_SONAME_FLAGS=-soname,$(LIBSO),--version-script=$(LIBMAP),-z,defs

LN=ln
OS := $(shell uname)
ifeq ($(OS), Darwin)
LD_SONAME_FLAGS=-install_name,$(LIBSO)
LDFLAGS += -undefined dynamic_lookup
LN=gln
endif

all: $(LIBA) $(LIBSO) $(LIBPC)


$(LIBA):  $(OBJS)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(LIBSO): $(LOBJS) $(LIBMAP)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $(LOBJS) -Wl,$(LD_SONAME_FLAGS)
	ln -sf $@ $(TARGET) 

$(LIBPC): $(LIBPC).in ../VERSION
	sed -e 's/@VERSION@/$(VERSION)/; s:@prefix@:$(PREFIX):; s:@libdir@:$(LIBDIR):; s:@includedir@:$(INCLUDEDIR):' < $< > $@

$(LIBMAP): $(LIBMAP).in
ifneq ($(DISABLE_CIL),y)
	cp $< $@
else
	sed -e '/^\s*cil_/d' < $< > $@
endif

ifneq ($(DISABLE_CIL),y)
$(CILDIR)/src/cil_lexer.o: $(CILDIR)/src/cil_lexer.c
	$(CC) $(filter-out -Werror, $(CFLAGS)) -fPIC -c -o $@ $<

$(CILDIR)/src/cil_lexer.lo: $(CILDIR)/src/cil_lexer.c
	$(CC) $(filter-out -Werror, $(CFLAGS)) -fPIC -DSHARED -c -o $@ $<

$(CILDIR)/src/cil_lexer.c: $(CILDIR)/src/cil_lexer.l
	$(LEX) -o $@ $<

endif

%.o:  %.c 
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

%.lo:  %.c
	$(CC) $(CFLAGS) -fPIC -DSHARED -c -o $@ $<

install: all
	test -d $(DESTDIR)$(LIBDIR) || install -m 755 -d $(DESTDIR)$(LIBDIR)
	install -m 644 $(LIBA) $(DESTDIR)$(LIBDIR)
	test -d $(DESTDIR)$(SHLIBDIR) || install -m 755 -d $(DESTDIR)$(SHLIBDIR)
	install -m 755 $(LIBSO) $(DESTDIR)$(SHLIBDIR)
	test -d $(DESTDIR)$(LIBDIR)/pkgconfig || install -m 755 -d $(DESTDIR)$(LIBDIR)/pkgconfig
	install -m 644 $(LIBPC) $(DESTDIR)$(LIBDIR)/pkgconfig
	$(LN) -sf --relative $(DESTDIR)$(SHLIBDIR)/$(LIBSO) $(DESTDIR)$(LIBDIR)/$(TARGET)

relabel:
	/sbin/restorecon $(DESTDIR)$(SHLIBDIR)/$(LIBSO)

clean: 
	-rm -f $(LIBPC) $(LIBMAP) $(OBJS) $(LOBJS) $(LIBA) $(LIBSO) $(TARGET) $(CIL_GENERATED)

indent:
	../../scripts/Lindent $(wildcard *.[ch])

