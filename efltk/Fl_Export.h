/*
 * $Id: Fl_Export.h 1487 2003-07-04 19:16:34Z laza2000 $
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

#ifndef _FL_EXPORT_H_
#define _FL_EXPORT_H_

/**
 * @file Fl_Export.h
 *
 * The FL_API directive is only used when building DLLs under WIN32
 *
 * @note When compiling library as DLL, you must define FL_SHARED and FL_LIBRARY.
 * @note When compiling application that uses DLL, you must define ONLY FL_SHARED, not FL_LIBRARY !
 *
 */

#if defined(_WIN32) && defined(FL_SHARED) && (defined(_MSC_VER) || defined(__MWERKS__) || defined(__BORLANDC__) || __GNUC__ >= 3)
# ifdef FL_LIBRARY
#  define FL_API __declspec(dllexport)
# else
#  define FL_API __declspec(dllimport)
# endif

#else

# define FL_API

#endif

#endif
