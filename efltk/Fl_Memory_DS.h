/*
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

#ifndef _FL_MEMORY_DS_H_
#define _FL_MEMORY_DS_H_

#include <efltk/Fl_Data_Fields.h>
#include <efltk/Fl_Data_Source.h>
#include <efltk/Fl_Exception.h>

/** Fl_Memory_DS */
class FL_API Fl_Memory_DS : public Fl_Data_Source  {
protected:
    // ctor, prevent creating the instance of that class
    Fl_Memory_DS() : Fl_Data_Source(0L), m_current(0L), m_currentIndex(-1) { }
public:
    // dtor
    virtual ~Fl_Memory_DS() { close(); }
    virtual void clear();

    // access to the field by name
    virtual const Fl_Variant& operator [] (const char *field_name) const;
    virtual Fl_Variant&       operator [] (const char *field_name);

    virtual const Fl_Data_Field& field (int field_index) const;
    virtual Fl_Data_Field&       field (int field_index);
    virtual void             *user_data() const { return m_current->user_data(); }

    // how many fields do we have in the current record?
    virtual unsigned          field_count() const;
    virtual int               field_index(const char *field_name) const;

     // how many rows do we have ds?
    virtual unsigned          record_count() const;

    // access to the field by number, 0..field_count()-1
    virtual const Fl_Variant& operator [] (int) const;
    virtual Fl_Variant&       operator [] (int);

    virtual bool              read_field(const char *fname,Fl_Variant& fvalue);
    virtual bool              write_field(const char *fname,const Fl_Variant& fvalue);

    // dataset navigation
    virtual bool              open() { fl_throw("Not implemented yet"); }
    virtual bool              close();
    virtual bool              first();
    virtual bool              next();
    virtual bool              prior();
    virtual bool              last();
    virtual bool              find(Fl_Variant position);
    virtual bool              eof() const { return m_eof; }

protected:
    // these methods should be implemented in derived class
    virtual bool load_data() { return true; }
    virtual bool save_data() { return true; }

    Fl_Ptr_List         m_list;
    Fl_Data_Fields     *m_current;
    int                 m_currentIndex;
    bool                m_eof;
};

#endif
