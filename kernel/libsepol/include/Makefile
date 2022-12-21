# Installation directories.
PREFIX ?= /usr
INCDIR = $(PREFIX)/include/sepol
CILDIR ?= ../cil

all:

install: all
	test -d $(DESTDIR)$(INCDIR) || install -m 755 -d $(DESTDIR)$(INCDIR)
	test -d $(DESTDIR)$(INCDIR)/policydb || install -m 755 -d $(DESTDIR)$(INCDIR)/policydb
	test -d $(DESTDIR)$(INCDIR)/cil || install -m 755 -d $(DESTDIR)$(INCDIR)/cil
	install -m 644 $(wildcard sepol/*.h) $(DESTDIR)$(INCDIR)
	install -m 644 $(wildcard sepol/policydb/*.h) $(DESTDIR)$(INCDIR)/policydb
	install -m 644 $(wildcard $(CILDIR)/include/cil/*.h) $(DESTDIR)$(INCDIR)/cil

indent:
	../../scripts/Lindent $(wildcard sepol/*.h)
