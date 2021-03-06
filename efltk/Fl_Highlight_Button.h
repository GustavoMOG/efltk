/*
 * $Id: Fl_Highlight_Button.h 1099 2003-03-30 20:40:49Z parshin $
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

#ifndef _FL_HIGHLIGHT_BUTTON_H_
#define _FL_HIGHLIGHT_BUTTON_H_

#include "Fl_Button.h"

/** Fl_Highlight_Button */
class FL_API Fl_Highlight_Button : public Fl_Button {
public:
    static Fl_Named_Style* default_style;

    /** Creates new highlight button widget using the given position, size, and label string. */
    Fl_Highlight_Button(int x,int y,int w,int h,const char *l=0);

    /** Creates new highlight button widget using the label, size, and alignment. */
    Fl_Highlight_Button(const char* l = 0,int layout_size=30,Fl_Align layout_al=FL_ALIGN_TOP);
};

#endif
