# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.3 1999/12/07 05:37:24 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-g -I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o

mlan.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)

search: mlan.a search.o
	$(CC) -o $@ search.o mlan.a

clean:
	rm -f $(OBJ) mlan.a
