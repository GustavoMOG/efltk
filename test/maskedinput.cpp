//
//
// Demo program from the fltk documentation.
//
// Copyright 1998-1999 by Bill Spitzak and others.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA.
//
// Please report all bugs and problems to "fltk-bugs@easysw.com".
//

#include <efltk/Fl.h>
#include <efltk/Fl_Window.h>
#include <efltk/Fl_Date_Time.h>
#include <efltk/Fl_Masked_Input.h>
#include <efltk/Fl_Date_Time_Input.h>

int main(int argc, char **argv) {
    Fl_Window *window = new Fl_Window(350,270);

    Fl_Masked_Input *d1 = new Fl_Masked_Input(150,20,150,24,"date (YYYY-MM-DD):");
    d1->mask("2999\\-19\\-39");
    
    Fl_Masked_Input *d2 = new Fl_Masked_Input(150,50,150,24,"date (OS format):");
    d2->mask(Fl_Date_Time::dateInputFormat);
    
    Fl_Masked_Input *t1 = new Fl_Masked_Input(150,80,150,24,"time (HH:MM:SS):");
    t1->mask("29\\:59\\:59");
    
    Fl_Masked_Input *t2 = new Fl_Masked_Input(150,110,150,24,"time (OS format):");
    t2->mask(Fl_Date_Time::timeInputFormat);
    
    Fl_Masked_Input *n = new Fl_Masked_Input(150,140,150,24,"name (Xxxxxxxxxxxxx):");
    n->mask("Lzzzzzzzzzzz");

    Fl_Masked_Input *p = new Fl_Masked_Input(150,170,150,24,"phone :");
    p->mask("\\(999\\)\\-999\\-9999");

    new Fl_Date_Input(150,220,150,24,"Date input:");

    window->end();
    window->show(argc, argv);

    return Fl::run();
}

//
// End of "$Id: maskedinput.cpp 357 2002-11-14 23:23:03Z parshin $".
//
