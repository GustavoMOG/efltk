/*
 * $Id: Fl_Light_Button.h 1100 2003-03-31 04:54:06Z parshin $
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

#ifndef _FL_LIGHT_BUTTON_H_
#define _FL_LIGHT_BUTTON_H_

#include "Fl_Check_Button.h"

/** Fl_Light_Button */
class FL_API Fl_Light_Button : public Fl_Check_Button {
public:
    static Fl_Named_Style* default_style;

    /** Creates new light button widget using the given position, size, and label string. */
    Fl_Light_Button(int x,int y,int w,int h,const char *l = 0);

    /** Creates new input widget using the label, size, and alignment. */
    Fl_Light_Button(const char* l = 0,int layout_size=30,Fl_Align layout_al=FL_ALIGN_TOP);

    static void default_glyph(const Fl_Widget*,int,int,int,int,int,Fl_Flags);
};

#endif
