# "$Id: Makefile 1526 2005-02-03 15:11:44Z karijes $"
#
# Top-level makefile for the Fast Light Tool Kit (FLTK).
#
# Copyright 1998-1999 by Bill Spitzak and others.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.
#
# Please report all bugs and problems to "fltk-bugs@easysw.com".
#

SHELL=/bin/sh
OS_NAME := $(shell uname -s | sed "s/\//-/" | sed "s/_/-/" | sed "s/-.*//g")

DIRS = src tools themes efltk locale test

GENERATED = makeinclude efltk-config

all: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS)) || exit;\
	done

static: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) static) || exit;\
	done

shared: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) shared) || exit;\
	done

install: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) install) || exit;\
	done

install_static: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) install_static) || exit;\
	done

install_shared: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) install_shared) || exit;\
	done

install_programs: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir; $(MAKE) $(MFLAGS) install_programs) || exit;\
	done

depend: $(GENERATED)
	@for dir in $(DIRS); do\
		if test ! -f $$dir/makedepend; then\
			touch $$dir/makedepend;\
		fi;\
		(cd $$dir;$(MAKE) $(MFLAGS) depend) || exit;\
	done

uninstall: $(GENERATED)
	@for dir in $(DIRS); do\
		(cd $$dir;$(MAKE) $(MFLAGS) uninstall) || exit;\
	done

clean:
	-@ rm -f core *~ *.o *.bck
	@for dir in $(DIRS); do\
		(cd $$dir;$(MAKE) $(MFLAGS) clean) || exit;\
	done

distclean: clean
	rm -f *~ config.* makeinclude efltk-config

ifeq ($(OS_NAME), MINGW32)

configure:

$(GENERATED):

else

$(GENERATED): configure makeinclude.in configh.in efltk-config.in
	./configure

configure: configure.in
	autoconf
	
endif

portable-dist:
	epm -v efltk

native-dist:
	epm -v -f native efltk

#
# End of "$Id: Makefile 1526 2005-02-03 15:11:44Z karijes $".
#
