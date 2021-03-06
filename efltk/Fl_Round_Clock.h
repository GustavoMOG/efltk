/*
 * $Id: Fl_Round_Clock.h 990 2003-03-15 16:09:56Z laza2000 $
 *
 * Extended Fast Light Toolkit (EFLTK)
 * Copyright (C) 2002-2003 by EDE-Team
 * WWW: http://www.sourceforge.net/projects/ede
 *
 * Fast Light Toolkit (FLTK)
 * Copyright (C) 1998-2003 by Bill Spitzak and others.
 * WWW: http://www.fltk.org
 *
 * This library is distributed under the GNU LIBRARY GENERAL PUBLIC LICENSE
 * version 2. See COPYING for details.
 *
 * Author : Mikko Lahteenmaki
 * Email  : mikko@fltk.net
 *
 * Please report all bugs and problems to "efltk-bugs@fltk.net"
 *
 */

#ifndef _FL_ROUND_CLOCK_H_
#define _FL_ROUND_CLOCK_H_

#include "Fl_Clock.h"

/** Fl_Round_Clock */
class Fl_Round_Clock : public Fl_Clock {
public:
    Fl_Round_Clock(int x,int y,int w,int h, const char *l = 0) : Fl_Clock(x,y,w,h,l) { type(ROUND); box(FL_NO_BOX); }
};

#endif
