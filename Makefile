# Makefile for ush

CC=gcc
CFLAGS=-g
SRC=ush.c parse.c parse.h
OBJ=ush.o parse.o

ush:	$(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
	\rm $(OBJ) ush
test:
	bash test.sh