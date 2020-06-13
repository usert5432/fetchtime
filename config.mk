VERSION = 0.1.0

PREFIX  ?= /usr/local
DATADIR ?= $(PREFIX)/share
MANDIR  ?= $(DATADIR)/man
DISTDIR ?= fetchtime-$(VERSION)

CC       ?= gcc
CPPFLAGS += -D_DEFAULT_SOURCE -DVERSION=\"$(VERSION)\"
CFLAGS   ?= -Os
CFLAGS   += -std=c99 -Wpedantic -Wall

