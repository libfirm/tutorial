.PHONY: all clean

all: frontend libkaleidoscope kal.sh
	./kal.sh

debug:
	gcc -Wall -std=c99 -ggdb $< `pkg-config --cflags --libs libfirm` -o $@

frontend: frontend.c
	gcc -std=c99 $< `pkg-config --cflags --libs libfirm` -o $@

libkaleidoscope: libkaleidoscope.c
	gcc -Wall -c $<

clean: clean.sh
	rm -f frontend libkaleidoscope.o
	./clean.sh

