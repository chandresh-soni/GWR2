#
# Makefile for the Braille Printer Application
#
# Copyright © 2022 Chandresh Soni
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# POSIX makefile
.POSIX:

# Build silently
.SILENT:

# Version and directories...
VERSION		=	1
prefix		=	$(DESTDIR)/usr/local
includedir	=	$(prefix)/include
bindir		=	$(prefix)/bin
libdir		=	$(prefix)/lib
mandir		=	$(prefix)/share/man
unitdir 	=	`pkg-config --variable=systemdsystemunitdir systemd`


# Compiler/linker options...
CSFLAGS		=	-s "$${CODESIGN_IDENTITY:=-}" --timestamp -o runtime
CFLAGS		=	$(CPPFLAGS) $(OPTIM)
CPPFLAGS	=	'-DVERSION="$(VERSION)"' `pkg-config --cflags cups` `pkg-config --cflags libcupsfilters``pkg-config --cflags pappl` $(OPTIONS)
LDFLAGS		=	$(OPTIM)
LIBS		=	`pkg-config --libs pappl` `pkg-config --libs libcupsfilters` `pkg-config --libs cups` -lm
OPTIM		=	-Os -g




# Targets...
OBJS		=	\
			generic-brf.o \
			brf-printer-app.o
TARGETS		=	\
			brf-printer-app


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
	cp brf-printer-app $(bindir)
	echo "Installing documentation to $(mandir)..."
	mkdir -p $(mandir)/man1
	cp brf-printer-app.1 $(mandir)/man1
	if pkg-config --exists systemd; then \
		echo "Installing systemd service to $(unitdir)..."; \
		mkdir -p $(unitdir); \
		cp brf-printer-app.service $(unitdir); \
	fi

	

brf-printer-app:	$(OBJS)
	echo "Linking $@..."
	$(CC) $(LDFLAGS) -o $@ brf-printer-app.o generic-brf.o $(LIBS)

$(OBJS):	 Makefile

