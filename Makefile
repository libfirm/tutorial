-include config.mak
FIRM_CFLAGS ?= $(shell pkg-config --cflags libfirm)
FIRM_LIBS ?= $(shell pkg-config --libs libfirm)

CFLAGS = -Wall -std=c99 $(FIRM_CFLAGS)
LFLAGS = $(FIRM_LIBS)

TANGLED_FILES = io.kal simple.kal

.PHONY: all clean

all: tutorial libkaleidoscope kal.sh tutorial.pdf $(TANGLED_FILES)
	./kal.sh

%.kal: tutorial.nw
	notangle -R$@ $< > $@

tutorial.tex: tutorial.nw
	noweave -delay $< > $@

tutorial.pdf: tutorial.tex
	pdflatex tutorial.tex

tutorial.c: tutorial.nw
	notangle $< > $@

debug: tutorial.c
	${CC} -g3 $< ${LFLAGS} ${LFLAGS} -o $@

tutorial: tutorial.c
	${CC} $< ${CFLAGS} ${LFLAGS} -o $@

libkaleidoscope: libkaleidoscope.c
	${CC} ${CFLAGS} -c $<

clean: clean.sh
	rm -f tutorial debug libkaleidoscope.o
	rm -f tutorial.c $(TANGLED_FILES)
	rm -f tutorial.tex tutorial.pdf tutorial.aux tutorial.log
	./clean.sh

