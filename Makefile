CC=gcc
CFLAGS=-pedantic -Wall -std=c11 -Og -ggdb -lSDL3
OBJ=thelife.o main.o

thelife: thelife.c
	$(CC) $(CFLAGS) -o q2 thelife.c
