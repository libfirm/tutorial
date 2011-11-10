-include config.mak
FIRM_CFLAGS ?= $(shell pkg-config --cflags libfirm)
FIRM_LIBS ?= $(shell pkg-config --libs libfirm)

CFLAGS = -Wall -std=c99 $(FIRM_CFLAGS)
LFLAGS = $(FIRM_LIBS)
TANGLEFLAGS = -t8

TANGLED_FILES = io.simple example.simple

.PHONY: all clean runtests

all: tutorial libsimple tutorial.pdf $(TANGLED_FILES) runtests

runtests: runtests.sh tutorial
	./runtests.sh

$(TANGLED_FILES): tutorial.nw
	notangle ${TANGLEFLAGS} -R$@ $< > $@

tutorial.tex: tutorial.nw
	noweave -delay $< > $@

tutorial.pdf: tutorial.tex
	pdflatex tutorial.tex

tutorial.c: tutorial.nw
	notangle ${TANGLEFLAGS} $< > $@

debug: tutorial.c
	${CC} -g3 $< ${LFLAGS} ${LFLAGS} -o $@

tutorial: tutorial.c
	${CC} $< ${CFLAGS} ${LFLAGS} -o $@

libsimple: libsimple.c
	${CC} ${CFLAGS} -c $<

clean:
	rm -f tutorial debug libsimple.o
	rm -f tutorial.c $(TANGLED_FILES)
	rm -f tutorial.tex tutorial.pdf tutorial.aux tutorial.log
	rm -f *.s
