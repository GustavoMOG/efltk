//
// "$Id: fl_dnd_win32.cpp 6 2002-09-03 11:16:32Z laza2000 $"
//
// Drag & Drop code for the Fast Light Tool Kit (FLTK).
// This is code for dragging *out* of the application. Code for dragging
// into the application is in Fl_win32.cxx
//
// Copyright 1998-2002 by Bill Spitzak and others.
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
// Please report all bugs and problems to "fltk-bugs@fltk.org

// I believe this was written by Matthias Melcher, correct?

#include <efltk/Fl.h>
#include <efltk/Fl_Window.h>
#include <efltk/x.h>

// All of the following code requires GCC 3.x or a non-GNU compiler...
#if !defined(__GNUC__) || __GNUC__ >= 3

#include <ole2.h>
#include <ShellAPI.h>

HANDLE fl_global_selection(int clipboard);

/**
 * this class is needed to allow FLTK apps to be a DnD source
 */
class FLDropSource : public IDropSource
{
    DWORD m_cRefCount;
    public:
        FLDropSource() { m_cRefCount = 0; }
        HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, LPVOID *ppvObject )
        {
            if (IID_IUnknown==riid || IID_IDropSource==riid)
            {
                *ppvObject=this;
                ((LPUNKNOWN)*ppvObject)->AddRef();
                return S_OK;
            }
            *ppvObject = NULL;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() { return ++m_cRefCount; }
        ULONG STDMETHODCALLTYPE Release()
        {
            long nTemp;
            nTemp = --m_cRefCount;
            if(nTemp==0)
                delete this;
            return nTemp;
        }
        STDMETHODIMP GiveFeedback( ulong ) { return DRAGDROP_S_USEDEFAULTCURSORS; }
        STDMETHODIMP QueryContinueDrag( BOOL esc, DWORD keyState )
        {
            if ( esc )
                return DRAGDROP_S_CANCEL;
            if ( !(keyState & MK_LBUTTON) )
                return DRAGDROP_S_DROP;
            return S_OK;
        }
};

/**
 * this is the actual object that FLTK can drop somewhere
 * - the implementation is minimal, but it should work with all decent Win32 drop targets
 */
class FLDataObject : public IDataObject
{
    DWORD m_cRefCount;
    public:
        FLDataObject() { m_cRefCount = 1; }
        HRESULT STDMETHODCALLTYPE QueryInterface( REFIID riid, LPVOID *ppvObject )
        {
            if (IID_IUnknown==riid || IID_IDataObject==riid)
            {
                *ppvObject=this;
                ((LPUNKNOWN)*ppvObject)->AddRef();
                return S_OK;
            }
            *ppvObject = NULL;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() { return ++m_cRefCount; }
        ULONG STDMETHODCALLTYPE Release()
        {
            long nTemp;
            nTemp = --m_cRefCount;
            if(nTemp==0)
                delete this;
            return nTemp;
        }
        // GetData currently allows ASCII text through Global Memory only
        HRESULT STDMETHODCALLTYPE GetData( FORMATETC *pformatetcIn, STGMEDIUM *pmedium )
        {
            if ((pformatetcIn->dwAspect & DVASPECT_CONTENT) &&
                (pformatetcIn->tymed & TYMED_HGLOBAL) &&
                (pformatetcIn->cfFormat == CF_TEXT))
            {
                pmedium->tymed          = TYMED_HGLOBAL;
                pmedium->hGlobal        = fl_global_selection(0);
                pmedium->pUnkForRelease = NULL;
                return S_OK;
            }
            return DV_E_FORMATETC;
        }
        HRESULT STDMETHODCALLTYPE QueryGetData( FORMATETC *pformatetc )
        {
            if ((pformatetc->dwAspect & DVASPECT_CONTENT) &&
                (pformatetc->tymed & TYMED_HGLOBAL) &&
                (pformatetc->cfFormat == CF_TEXT))
                return S_OK;
            return DV_E_FORMATETC;
        }
        // all the following methods are not really needed for a DnD object
        HRESULT STDMETHODCALLTYPE GetDataHere( FORMATETC *pformatetcIn, STGMEDIUM *pmedium) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc( FORMATETC *in, FORMATETC *out) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE SetData( FORMATETC *pformatetc, STGMEDIUM *pmedium, BOOL fRelease) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE EnumFormatEtc( DWORD dir, IEnumFORMATETC **ppenumFormatEtc) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE DAdvise( FORMATETC *pformatetc, DWORD advf,
            IAdviseSink *pAdvSink, DWORD *pdwConnection) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE DUnadvise( DWORD dwConnection) { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE EnumDAdvise( IEnumSTATDATA **ppenumAdvise) { return E_NOTIMPL; }
};

/**
 * drag and drop whatever is in the cut-copy-paste buffer
 * - create a selection first using:
 *     Fl::copy(const char *stuff, int len, 0)
 */
bool Fl::dnd()
{
    DWORD dropEffect;
    ReleaseCapture();

    FLDataObject *fdo = new FLDataObject;
    fdo->AddRef();
    FLDropSource *fds = new FLDropSource;
    fds->AddRef();

    HRESULT ret = DoDragDrop( fdo, fds, DROPEFFECT_MOVE|DROPEFFECT_LINK|DROPEFFECT_COPY, &dropEffect );

    fdo->Release();
    fds->Release();

    // Windows handled the mouse release, remember that:
    pushed_ = 0;
    e_state &= ~FL_BUTTONS;

    return ret==DRAGDROP_S_DROP; // or DD_S_CANCEL
}


#else
bool Fl::dnd()
{
    // Always indicate DnD failed when using GCC < 3...
    return false;
}
#endif                           // !__GNUC__ || __GNUC__ >= 3

//
// End of "$Id: fl_dnd_win32.cpp 6 2002-09-03 11:16:32Z laza2000 $".
//
