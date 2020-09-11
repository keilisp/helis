CC = gcc

helis: helis.c
	$(CC) helis.c -o helis -Wall -Wextra -pedantic -std=c99
