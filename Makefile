include config.mk

HEADERS = ntp.h
SOURCES = fetchtime.c
OBJ     = fetchtime.o
BIN     = fetchtime

.PHONY : all options clean dist install uninstall

all: options $(BIN)

options:
	@echo $(BIN) build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "CPPFLAGS = $(CPPFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"

$(OBJ): $(SOURCES) $(HEADERS)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $< -o $@

$(BIN): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

install: all
	@echo Installing executable file to ${DESTDIR}$(PREFIX)/bin
	mkdir -p ${DESTDIR}$(PREFIX)/bin
	cp  $(BIN) ${DESTDIR}$(PREFIX)/bin
	chmod 755 ${DESTDIR}$(PREFIX)/bin/$(BIN)
	@echo Installing manpage to ${DESTDIR}$(MANDIR)
	mkdir -p $(DESTDIR)$(MANDIR)/man1
	sed "s/VERSION/$(VERSION)/g" $(BIN).1 \
		> ${DESTDIR}$(MANDIR)/man1/$(BIN).1
	chmod 0644 ${DESTDIR}$(MANDIR)/man1/$(BIN).1

uninstall:
	@echo removing executable file from ${DESTDIR}$(PREFIX)/bin
	@rm -f ${DESTDIR}$(PREFIX)/bin/$(BIN)

clean:
	@echo Cleaning
	rm -f $(BIN) $(OBJ)
	rm -f $(DISTDIR).tar.gz

dist: clean
	mkdir -p $(DISTDIR)
	cp README.rst LICENSE Makefile config.mk $(DISTDIR)/
	cp $(SOURCES) $(BIN).1 $(HEADERS) $(DISTDIR)/
	tar -c -f $(DISTDIR).tar --remove-files $(DISTDIR)/
	gzip $(DISTDIR).tar


