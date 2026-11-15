CC = gcc
CFLAGS = -Wall -Wextra -Werror
LDLIBS = -ljpeg

TARGETS = mandel mandelmovie

all: $(TARGETS)

mandel: mandel.c jpegrw.c jpegrw.h
	$(CC) $(CFLAGS) mandel.c jpegrw.c -o mandel $(LDLIBS)

mandelmovie: mandelmovie.c
	$(CC) $(CFLAGS) mandelmovie.c -o mandelmovie

clean:
	rm -f $(TARGETS) mandel*.jpg mandel.mpg
