# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.1 1999/09/20 02:52:50 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o

mlan.a: $(OBJ)
	$(AR) rcs $@ $(OBJ)

clean:
	rm -f $(OBJ) mlan.a
