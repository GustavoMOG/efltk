/*
 * $Id: Fl_Data_Fields.h 1328 2003-05-06 20:57:08Z parshin $
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

#ifndef _FL_DATA_FIELDS_H_
#define _FL_DATA_FIELDS_H_

#include "Fl_Variant.h"
#include "Fl_Ptr_List.h"
#include "Fl_Flags.h"

class Fl_Data_Fields;

/** Fl_Data_Field */
class FL_API Fl_Data_Field {
    friend class Fl_Data_Fields;
public:
    Fl_Data_Field(const char *name);

    Fl_Variant value;

    /** Field attributes are used when we need to convert a field
      * data into something like Fl_ListView. Width defines a width
      * of the column, flags may contain an alignment of data, 
      * precision is used when we deal with floating point numbers.
      * Precision defines # of digits after decimal point. Positive
      * precision means fixed format, negative means a scientific 
      * format.
      * If the visible flag is switched off, the field is not shown 
      * in Fl_ListView and similar classes.
      */
    int         width;
    int         precision;
    Fl_Flags    flags;
    bool        visible;

    const Fl_String &name() const                 { return m_name;           }
    Fl_Variant_Type type() const                  { return value.type();     }

    unsigned buffer_size() const                  { return value.size();     }
    unsigned data_size()   const                  { return m_dataSize;       }
    void data_size(unsigned sz)                   { m_dataSize = sz;         }

    bool is_null() const                          { return m_dataSize == 0;  }

    // convertors
    operator int () const                         { return as_int();         }
    operator double () const                      { return as_float();       }
    operator Fl_String () const                   { return as_string();      }
    operator Fl_Date_Time () const                { return as_datetime();    }
    operator const Fl_Image * () const            { return as_image();       }

    int as_int() const                                    { return value.as_int();   }
    double as_float() const                       { return value.as_float(); }
    Fl_String as_string() const; // converting to a string requires internal formatting..
    bool as_bool() const                                  { return value.as_bool();  }
    Fl_Date_Time as_date() const                      { return value.as_date();  }
    Fl_Date_Time as_datetime() const                  { return value.as_datetime(); }
    const Fl_Image *as_image() const                  { return value.as_image(); }

    // assignments
    Fl_Data_Field& operator = (int v)             { value = v; return *this; }
    Fl_Data_Field& operator = (double v)          { value = v; return *this; }
    Fl_Data_Field& operator = (Fl_String v)       { value = v; return *this; }
    Fl_Data_Field& operator = (Fl_Date_Time v)    { value = v; return *this; }
    Fl_Data_Field& operator = (const char *v)     { value = v; return *this; }
    Fl_Data_Field& operator = (const Fl_Image *v) { value = v; return *this; }

protected:
    Fl_String  m_name;
    unsigned   m_dataSize;
};

/** Fl_Data_Fields */
class FL_API Fl_Data_Fields {
public:
    Fl_Data_Fields() { m_userData = 0; }
    ~Fl_Data_Fields() { clear(); }

    void clear();
    unsigned count() const { return m_list.count(); }
    int  field_index(const char * fname) const;

    Fl_Data_Field& add(const char *fname);
    Fl_Data_Field& add(Fl_Data_Field *fld);

    const Fl_Data_Field& field(unsigned index) const;
    Fl_Data_Field&       field(unsigned index);

    const Fl_Data_Field& field(int index) const;
    Fl_Data_Field&       field(int index);

    const Fl_Data_Field& field(const char *fname) const;
    Fl_Data_Field&       field(const char *fname);

    Fl_Variant&       operator [] (int index);
    const Fl_Variant& operator [] (int index) const;
    Fl_Variant&       operator [] (const char *fname);
    const Fl_Variant& operator [] (const char *fname) const;

    void user_data(void *d) { m_userData = d; } 
    void *user_data() const { return m_userData; } 

private:
    void             *m_userData;
    Fl_Ptr_List       m_list;
    static const Fl_Variant m_fieldNotFound;
};

#endif
