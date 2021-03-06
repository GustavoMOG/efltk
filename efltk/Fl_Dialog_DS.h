/*
 * $Id: Fl_Dialog_DS.h 1485 2003-07-03 20:37:33Z laza2000 $
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

#ifndef __FL_DIALOG_DS_H__
#define __FL_DIALOG_DS_H__

#include <efltk/Fl_Config.h>
#include <efltk/Fl_Data_Fields.h>
#include <efltk/Fl_Data_Source.h>

/** Fl_Dialog_DS - the special DataSource for Fl_Dialog */
class FL_API Fl_Dialog_DS : public Fl_Data_Source {

protected:
    Fl_Data_Fields            m_fields;

    virtual bool              load_data() { return true; }
    virtual bool              save_data() { return true; }

    bool                      m_widgetsScanned;
    void scan_widgets(Fl_Group *group=NULL);

public:
    /** Constructor */
    Fl_Dialog_DS() : Fl_Data_Source(NULL) { m_widgetsScanned = false; }

    /** access to the field value by name */
    virtual const Fl_Variant& operator [] (const char *field_name) const   { return m_fields[field_name]; }
    virtual Fl_Variant&       operator [] (const char *field_name);

    /** access to the field by index */
    virtual const Fl_Data_Field& field (int field_index) const             { return m_fields.field(field_index); }
    virtual Fl_Data_Field&    field (int field_index)                      { return m_fields.field(field_index); }

    /** row counter */
    virtual unsigned          record_count() const { return 1; }

    /** how many fields do we have in the current record? */
    virtual unsigned          field_count() const;
    virtual int               field_index(const char *field_name) const    { return m_fields.field_index(field_name); }

    /** access to the field by number, 0..field_count()-1 */
    virtual const Fl_Variant& operator [] (int index) const                { return m_fields[index]; }
    virtual Fl_Variant&       operator [] (int index)                      { return m_fields[index]; }
    virtual bool              read_field(const char *fname,Fl_Variant& fvalue) { fvalue = (*this)[fname]; return true; }
    virtual bool              write_field(const char *fname,const Fl_Variant& fvalue) { (*this)[fname] = fvalue; return true; }
};

#endif
