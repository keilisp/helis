OBJS	= helis.o
SOURCE	= helis.c
OUT		= helis
CC		= gcc
FLAGS	= -std=c99 -c -Wall -Wextra -pedantic -std=c99

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT)

gc.o: gc.c
	$(CC) $(FLAGS) gc.c

clean:
	rm -f $(OBJS) $(OUT)

