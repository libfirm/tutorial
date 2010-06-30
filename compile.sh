gcc -std=c99 -ggdb frontend.c `pkg-config --cflags --libs libfirm` -Wall -W -Wstrict-prototypes -Wmissing-prototypes -o frontend
