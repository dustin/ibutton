# Makefile for the mlan library
#
# Copyright (c) 1999  Dustin Sallings <dustin@spy.net>
#
# $Id: Makefile,v 1.7 1999/12/08 04:43:27 dustin Exp $

SHELL=/bin/sh
AR=/usr/bin/ar

CFLAGS=-g -I. -Wall -Werror

OBJ=com.o ds2480.o mlan.o devices.o
PROGS=search lookup
PROGO=search.o lookup.o

all: search lookup

search: mlan.a search.o
	$(CC) -o $@ search.o mlan.a

lookup: mlan.a lookup.o
	$(CC) -o $@ lookup.o mlan.a

mlan.a: $(OBJ) mlan.h
	$(AR) rcs $@ $(OBJ)

clean:
	rm -f $(OBJ) mlan.a $(PROGS) $(PROGO)
