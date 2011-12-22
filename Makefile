-include config.mak
FIRM_CFLAGS ?= $(shell pkg-config --cflags libfirm)
FIRM_LIBS ?= $(shell pkg-config --libs libfirm)

CFLAGS = -Wall -std=c99 $(FIRM_CFLAGS)
LFLAGS = $(FIRM_LIBS)
TANGLEFLAGS = -t8
BUILDDIR = build

TANGLED_FILES = io.simple example.simple

.PHONY: all documentation clean runtests

all: documentation tutorial libsimple.o $(TANGLED_FILES) runtests

documentation: $(BUILDDIR)/tutorial.html

runtests: runtests.sh libsimple.o tutorial
	./runtests.sh

$(TANGLED_FILES): tutorial.nw
	notangle $(TANGLEFLAGS) -R$@ $< > $@

tutorial.rst: tutorial.nw
	mkdir -p $(BUILDDIR)
	/usr/lib/noweb/markup -t4 < $< | python 2rst.py > $@

$(BUILDDIR)/tutorial.html: tutorial.rst conf.py
	sphinx-build -b html . $(BUILDDIR)

tutorial.c: tutorial.nw
	notangle $(TANGLEFLAGS) $< > $@

debug: tutorial.c
	$(CC) -g3 $< $(LFLAGS) $(LFLAGS) -o $@

tutorial: tutorial.c
	$(CC) $< $(CFLAGS) $(LFLAGS) -o $@

libsimple.o: libsimple.c
	$(CC) -m32 $(CFLAGS) -c $<

clean:
	rm -f tutorial debug libsimple.o
	rm -f tutorial.c $(TANGLED_FILES)
	rm -f tutorial.tex tutorial.pdf tutorial.aux tutorial.log
	rm -f *.s
