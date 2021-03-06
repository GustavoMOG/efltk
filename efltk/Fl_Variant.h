/*
 * $Id: Fl_Variant.h 1333 2003-05-09 00:18:19Z parshin $
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
 * Author : Alexey Parshin
 * Email  : alexey@fltk.net
 *
 * Please report all bugs and problems to "efltk-bugs@fltk.net"
 *
 */

#ifndef _FL_VARIANT_H_
#define _FL_VARIANT_H_

#include "Fl_Export.h"
#include "Fl_Date_Time.h"

class Fl_Image;

/** FL_VAR_TYPES */
enum FL_VAR_TYPES {
    VAR_NONE     = 0,
    VAR_INT      = 1,
    VAR_FLOAT    = 2,
    VAR_STRING   = 4,
    VAR_TEXT     = 8,
    VAR_BUFFER   = 16,
    VAR_DATE     = 32,
    VAR_DATETIME = 64,
    VAR_IMAGEPTR = 128
};

typedef FL_VAR_TYPES Fl_Variant_Type;

/** Fl_Variant */
class FL_API Fl_Variant {
public:
    static Fl_Variant null_object;

    Fl_Variant() { m_type = VAR_NONE; }
    Fl_Variant(int v);
    Fl_Variant(float v);
    Fl_Variant(const char* v);
    Fl_Variant(const Fl_String& v);
    Fl_Variant(const void *v,int sz);
    Fl_Variant(Fl_Date_Time value);

    ~Fl_Variant() { free_buffers(); }

    bool null() { return (m_type==VAR_NONE); }

    void set_int(int value);
    void set_float(double value);
    void set_string(const char * value,int maxlen=0);
    void set_text(const char * value);
    void set_buffer(const void * value,int sz);
    void set_image_ptr(const Fl_Image * value);
    void set_date(Fl_Date_Time value);
    void set_datetime(Fl_Date_Time value);

    void resize_buffer(int sz);

    // Convert functions
    int as_int() const;
    double as_float() const;
    Fl_String as_string() const;
    bool as_bool() const;
    Fl_Date_Time as_date() const;
    Fl_Date_Time as_datetime() const;
    const Fl_Image *as_image() const;

    /** Direct access to the data - use correctly with type().
      * There is no conversion performed.
      */
    int    get_int() const              { return m_data.intData; }
    double get_float() const            { return m_data.floatData; }
    const char *get_string() const      { return m_data.stringData; }
    const void *get_buffer() const      { return m_data.blobData; }
    const Fl_Image *get_image_ptr() const { return m_data.imagePtr; }
    Fl_Date_Time get_date() const       { return int(m_data.floatData); }
    Fl_Date_Time get_datetime() const   { return m_data.floatData; }

    // Convert operators
    operator int () const;
    operator double () const;
    operator const char * () const;
    operator const Fl_Image * () const;
    operator Fl_Date_Time () const;

    Fl_Variant_Type type() const { return m_type; }
    int size() const { return m_size; }
    void *data() const { return (void *)(variantData *)&m_data; }

    // ASSIGNMENT OPERATORS
    Fl_Variant& operator =(const Fl_Variant &C) {
        if (this == &C) return *this;
        set_data(C);
        return *this;
    }

    Fl_Variant& operator =(const char *s) {
        set_string(s);
        return *this;
    }

    Fl_Variant& operator =(const Fl_String &s) {
        set_string(s.c_str(), s.length());
        return *this;
    }

    Fl_Variant& operator =(int v) {
        set_int(v);
        return *this;
    }

    Fl_Variant& operator =(double v) {
        set_float(v);
        return *this;
    }

    Fl_Variant& operator =(Fl_Date_Time dt) {
        set_datetime(dt);
        return *this;
    }

    Fl_Variant& operator =(const Fl_Image *pm) {
        set_image_ptr(pm);
        return *this;
    }

protected:
    void set_data(const Fl_Variant &C);
    void free_buffers();

private:
    union variantData {
        int       intData;
        double    floatData;
        double    timeData;
        char      *stringData;
        void      *blobData;
        const Fl_Image *imagePtr;
    } m_data;
    int m_size;
    Fl_Variant_Type m_type;
};

#endif
