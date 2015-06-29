//
// "$Id: fl_dnd.cpp 508 2002-12-20 20:11:31Z laza2000 $"
//
// Drag & Drop code for the Fast Light Tool Kit (FLTK).
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

#include <config.h>

#ifdef _WIN32
# include "fl_dnd_win32.cpp"
#elif defined(__APPLE__)
# include "fl_dnd_mac.cpp"
#else
# include "fl_dnd_x.cpp"
#endif

//
// End of "$Id: fl_dnd.cpp 508 2002-12-20 20:11:31Z laza2000 $".
//