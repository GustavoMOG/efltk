//
// "$Id: Fl_Single_Window.cpp 2 2002-09-03 10:48:27Z laza2000 $"
//
// Single-buffered window for the Fast Light Tool Kit (FLTK).
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

//	A window with a single-buffered context
//
//	This is provided for systems where the base class is double
//	buffered.  You can turn it off using this subclass in case
//	your display looks better without it.

#include <efltk/Fl_Single_Window.h>

//
// End of "$Id: Fl_Single_Window.cpp 2 2002-09-03 10:48:27Z laza2000 $".
//
