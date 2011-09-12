CFLAGS = -Wall -std=c99 $(shell pkg-config --cflags --libs libfirm)

.PHONY: all clean

all: frontend libkaleidoscope kal.sh
	./kal.sh

debug: frontend.c
	${CC} -ggdb $< ${CFLAGS} -o $@

frontend: frontend.c
	${CC} $< ${CFLAGS} -o $@

libkaleidoscope: libkaleidoscope.c
	${CC} ${CFLAGS} -c $<

clean: clean.sh
	rm -f frontend debug libkaleidoscope.o
	./clean.sh

