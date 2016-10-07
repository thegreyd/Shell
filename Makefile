# Makefile for ush
#
# Vincent W. Freeh
# 

CC=gcc
CFLAGS=-g
SRC=shell.c parse.c parse.h
OBJ=shell.o parse.o

shell:	$(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
	\rm $(OBJ) shell