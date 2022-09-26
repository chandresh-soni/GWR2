#
# Makefile for the HP Printer Application
#
# Copyright © 2020-2022 by Michael R Sweet
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# POSIX makefile
.POSIX:

# Build silently
.SILENT:

# Version and directories...
VERSION		=	1.2
prefix		=	$(DESTDIR)/usr/local
includedir	=	$(prefix)/include
bindir		=	$(prefix)/bin
libdir		=	$(prefix)/lib
mandir		=	$(prefix)/share/man
unitdir 	=	`pkg-config --variable=systemdsystemunitdir systemd`


# Compiler/linker options...
CSFLAGS		=	-s "$${CODESIGN_IDENTITY:=-}" --timestamp -o runtime
CFLAGS		=	$(CPPFLAGS) $(OPTIM)
CPPFLAGS	=	'-DVERSION="$(VERSION)"' `pkg-config --cflags cups` `pkg-config --cflags pappl` $(OPTIONS)
LDFLAGS		=	$(OPTIM)
LIBS		=	`pkg-config --libs pappl` `pkg-config --libs cups` -lm
OPTIM		=	-Os -g
# Uncomment the following line to enable experimental PCL 6 support
#OPTIONS	=	-DWITH_PCL6=1



# Targets...
OBJS		=	\
			generic-brf.o \
			BRF-printer-app.o
TARGETS		=	\
			BRF-printer-app


# General build rules...
.SUFFIXES:	.c .o
.c.o:
	echo "Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<


# Targets...
all:		$(TARGETS)

clean:
	echo "Cleaning all output..."
	rm -f $(TARGETS) $(OBJS)

install:	$(TARGETS)
	echo "Installing program to $(bindir)..."
	mkdir -p $(bindir)
	cp BRF-printer-app $(bindir)
	echo "Installing documentation to $(mandir)..."
	mkdir -p $(mandir)/man1
	cp BRF-printer-app.1 $(mandir)/man1
	

BRF-printer-app:	BRF-printer-app.o
	echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ BRF-printer-app.o $(LIBS)

$(OBJS):	 Makefile

