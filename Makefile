-include config.mak
FIRM_CFLAGS ?= $(shell pkg-config --cflags libfirm)
FIRM_LIBS ?= $(shell pkg-config --libs libfirm)
YCOMP ?= ycomp

CFLAGS = -Wall -W -std=c99 $(FIRM_CFLAGS) -O0 -g3
LFLAGS = $(FIRM_LIBS)
TANGLEFLAGS = -t8
BUILDDIR = build

TANGLED_FILES = io.simple example.simple libruntime.c answer.simple

SIMPLE_FILES = io.simple example.simple answer.simple

.PHONY: all documentation clean runtests

all: documentation tutorial libruntime.o $(TANGLED_FILES) runtests

documentation: $(BUILDDIR)/tutorial.html

runtests: runtests.sh libruntime.o tutorial $(SIMPLE_FILES)
	./runtests.sh

$(TANGLED_FILES): tutorial.nw
	notangle $(TANGLEFLAGS) -R$@ $< > $@

index.rst: tutorial.nw 2rst.py
	mkdir -p $(BUILDDIR)
	/usr/lib/noweb/markup -t < $< | python 2rst.py > $@

$(BUILDDIR)/tutorial.html: tutorial.c index.rst conf.py
	sphinx-build -b html . $(BUILDDIR)

tutorial.c: tutorial.nw
	notangle $(TANGLEFLAGS) $< > $@

debug: tutorial.c
	$(CC) -g3 $< $(LFLAGS) $(LFLAGS) -o $@

tutorial: tutorial.c
	$(CC) $< $(CFLAGS) $(LFLAGS) -o $@

libruntime.o: libruntime.c
	$(CC) $(CFLAGS) -c $<

$(BUILDDIR)/plus.vcg: plus.simple tutorial
	./tutorial -d $< > /dev/null
	mv plus-00.vcg $@

plus.svg: $(BUILDDIR)/plus.vcg
	$(YCOMP) $< --export $@

clean:
	rm -f tutorial debug libsimple.o
	rm -f tutorial.c $(TANGLED_FILES)
	rm -f tutorial.tex tutorial.pdf tutorial.aux tutorial.log
	rm -f *.s
