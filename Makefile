# IceTetris
#
# Works with any reasonably standard make + C compiler.
# Override on the command line if you like:
#   make CC=gcc
#   make CC=clang CFLAGS="-O2 -Wall -Wextra -std=c89 -pedantic"

CC     = cc
CFLAGS = -O2 -Wall

tetris: tetris.c
	$(CC) $(CFLAGS) -o tetris tetris.c

clean:
	rm -f tetris tetris.exe tetris.o

.PHONY: clean
