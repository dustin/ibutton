# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.2 1999/09/22 07:49:53 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o

mlan.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)

search: mlan.a search.o
	$(CC) -o $@ search.o mlan.a

clean:
	rm -f $(OBJ) mlan.a
