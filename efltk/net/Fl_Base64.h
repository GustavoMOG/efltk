/** 
  * Fl_Base64.h
  * This file contains interface to Fl_Base64 class .
  * 
  * The source below is under LGPL license.
  * Copyright (c) EDE Team. More information: http://ede.sf.net .
  * Authors (sorted by time they worked on this source file):
  * 	Dejan Lekic, dejan�nu6.org
  * Contributors (ie. people that have sent patches, ideas, suggestions):
  *     Mikko Lahteenmaki, mikko�fltk.net
  ****************************************************************************/


#ifndef _FL_BASE64_H_
#define _FL_BASE64_H_

#include "../Fl_String_List.h" 	/* needed for string operations				 */
#include "../Fl_Buffer.h"		/* needed for buffer operations 			 */

/**
 * Fl_Base64 class is responsible for base64 encoding/decoding
 * it works with Fl_Buffer, Fl_String and/or Fl_String_List objects.
 * @see Fl_Buffer, Fl_String, Fl_String_List
 */
class FL_API Fl_Base64
{
public:
	Fl_Base64() { } 	/** Default constructor */
	~Fl_Base64() { } 	/** Default destructor */

	/**
	 * encode() method encodes (base64) given buffer bufSource
	 * to given destination buffer bufDest.
	 *
	 * @param bufDest Fl_Buffer Destination buffer
	 * @param bufSource Fl_Buffer Source buffer
	 * @see encode(Fl_String& strDest, const Fl_Buffer& bufSource)
	 */
	static void encode(Fl_Buffer& bufDest, const Fl_Buffer& bufSource);

	/**
	 * This encode() method encodes (base64) given buffer bufSource
	 * and returns Fl_String object.
	 *
	 * @param strDest Fl_String Destination string
	 * @param bufSource Fl_Buffer* Source buffer	 
	 * @see encode(Fl_Buffer& bufDest, const Fl_Buffer& bufSource)
	 */
	static void encode(Fl_String& strDest, const Fl_Buffer& bufSource);

	/** 
	 * Decodes base64 encoded buffer "string" into buffer "bufDest" 
	 * @param bufDest Fl_Buffer destination buffer
	 * @param bufSource Fl_Buffer source buffer that holds base64 decoded data
	 * @returns length of returned buffer, or -1 on error
	 */
	static int decode(Fl_Buffer &bufDest, const Fl_Buffer &bufSource);

	/** 
	 * Decodes base64 encoded string "sArg" into buffer "bufDest"
	 * @param bufDest Fl_Buffer destination buffer
	 * @param strSource Fl_String source string that holds base64 decoded data
	 * @returns length of returned buffer, or -1 on error
	 */
	static int decode(Fl_Buffer &bufDest, const Fl_String &strSource);
}; /* class Fl_Base64 */

#endif

/***** $Id: Fl_Base64.h 1048 2003-03-27 21:08:20Z dejan $
 *     Project: eFLTK
 ***   This source code is released under GNU LGPL License
 *     Copyright (c) EDE Team, 2000-DWYRT  (DWYRT = Date When You Read This)
 ***** Equinox Desktop Environment, http://ede.sf.net */
