# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.4 1999/12/07 08:55:11 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-g -I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o

search: mlan.a search.o
	$(CC) -o $@ search.o mlan.a

mlan.a: $(OBJ) mlan.h
	$(AR) rcs $@ $(OBJ)

clean:
	rm -f $(OBJ) mlan.a
