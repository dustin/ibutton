# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.6 1999/12/07 10:16:10 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-g -I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o devices.o
PROGS=search

search: mlan.a search.o
	$(CC) -o $@ search.o mlan.a

mlan.a: $(OBJ) mlan.h
	$(AR) rcs $@ $(OBJ)

clean:
	rm -f $(OBJ) mlan.a $(PROGS)
