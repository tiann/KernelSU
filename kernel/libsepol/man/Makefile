# Installation directories.
LINGUAS ?= ru
PREFIX ?= /usr
MANDIR ?=  $(PREFIX)/share/man
MAN3SUBDIR ?= man3
MAN8SUBDIR ?= man8
MAN3DIR ?= $(MANDIR)/$(MAN3SUBDIR)
MAN8DIR ?= $(MANDIR)/$(MAN8SUBDIR)

all:

install: all
	mkdir -p $(DESTDIR)$(MAN3DIR)
	mkdir -p $(DESTDIR)$(MAN8DIR)
	install -m 644 man3/*.3 $(DESTDIR)$(MAN3DIR)
	install -m 644 man8/*.8 $(DESTDIR)$(MAN8DIR)
	for lang in $(LINGUAS) ; do \
		if [ -e $${lang}/man3 ] ; then \
			mkdir -p $(DESTDIR)$(MANDIR)/$${lang}/$(MAN3SUBDIR) ; \
			install -m 644 $${lang}/man3/*.3 $(DESTDIR)$(MANDIR)/$${lang}/$(MAN3SUBDIR) ; \
		fi ; \
		if [ -e $${lang}/man8 ] ; then \
			mkdir -p $(DESTDIR)$(MANDIR)/$${lang}/$(MAN8SUBDIR) ; \
			install -m 644 $${lang}/man8/*.8 $(DESTDIR)$(MANDIR)/$${lang}/$(MAN8SUBDIR) ; \
		fi ; \
	done
