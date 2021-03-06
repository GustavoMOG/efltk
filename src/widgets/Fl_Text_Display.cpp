//
// "$Id: Fl_Text_Display.cpp 1462 2003-06-18 15:50:38Z laza2000 $"
//
// Copyright Mark Edel.  Permission to distribute under the LGPL for
// the FLTK library granted by Mark Edel.
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
// Please report all bugs and problems to "fltk-bugs@fltk.org".
//
#include <config.h>

#include <efltk/Fl_Window.h>
#include <efltk/x.h>

#include <efltk/Fl.h>
#include <efltk/Fl_Image.h>
#include <efltk/Fl_Text_Buffer.h>
#include <efltk/Fl_Text_Display.h>
#include <efltk/Fl_Style.h>
#include <efltk/fl_utf8.h>
#include <efltk/fl_draw.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#ifndef _WIN32

#if USE_XFT
#include <X11/Xft/Xft.h>
#endif

#endif /* _WIN32 */

#undef min
#undef max

#define max(a, b) ((a)>=(b)?(a):(b))
#define min(a, b) ((a)<=(b)?(a):(b))

// Text area margins.  Left & right margins should be at least 3 so that
// there is some room for the overhanging parts of the cursor!
#define TOP_MARGIN 1
#define BOTTOM_MARGIN 1
#define LEFT_MARGIN 3
#define RIGHT_MARGIN 3

#define NO_HINT -1

/* Masks for text drawing methods.  These are or'd together to form an
   integer which describes what drawing calls to use to draw a string */
#define FILL_MASK 0x100
#define SECONDARY_MASK 0x200
#define PRIMARY_MASK 0x400
#define HIGHLIGHT_MASK 0x800
#define STYLE_LOOKUP_MASK 0xff

/* Maximum displayable line length (how many characters will fit across the
   widest window).  This amount of memory is temporarily allocated from the
   stack in the draw_vline() method for drawing strings */
#define MAX_DISP_LINE_LEN 2048

static int countlines( const char *string );

void Fl_Text_Display::ctor_init()
{
    mMaxsize = 0;
    damage_range1_start = damage_range1_end = -1;
    damage_range2_start = damage_range2_end = -1;
    dragPos = dragType = dragging = 0;

    text_area.x = 0;
    text_area.y = 0;
    text_area.w = 0;
    text_area.h = 0;

    begin();

    mVScrollBar = new Fl_Scrollbar(0,0,scrollbar_width(),0);
    mVScrollBar->callback((Fl_Callback*)v_scrollbar_cb, this);
	mVScrollBar->set_visible();

    mHScrollBar = new Fl_Scrollbar(0,0,0,scrollbar_width());
    mHScrollBar->callback((Fl_Callback*)h_scrollbar_cb, this);
    mHScrollBar->type(Fl_Scrollbar::HORIZONTAL);
	mHScrollBar->set_visible();

    end();
	
	mCursorColor = FL_BLACK;
    mCursorOn = 0;
    mCursorPos = 0;
	mCursorPosOld = -1;
    mCursorToHint = NO_HINT;
    mCursorStyle = NORMAL_CURSOR;
    mCursorPreferredCol = -1;

    mBuffer = 0;
    mFirstChar = 0;
    mLastChar = 0;
    mNBufferLines = 0;
    mTopLineNum = 1;//mTopLineNumHint = 1;
    mAbsTopLineNum = 1; 
    mNeedAbsTopLineNum = 0; 
    mHorizOffset = 0;//mHorizOffsetHint = 0;

    mStyleBuffer = 0;
    mStyleTable = 0;
    mNStyles = 0;
    mNVisibleLines = 1;

    mLineStarts.blocksize(256);
    mLineStarts.append(-1);

    mSuppressResync = 0;
    mNLinesDeleted = 0;
    mModifyingTabDistance = 0;

    mUnfinishedStyle = 0;
    mUnfinishedHighlightCB = 0;
    mHighlightCBArg = 0;

    mLineNumLeft = mLineNumWidth = 0;
    mContinuousWrap = 0;
    mWrapMargin = 0;
    mSuppressResync = mNLinesDeleted = mModifyingTabDistance = 0;
    mMaxFontBound = mMinFontBound = 0;

    mLongestVline = 0;
	mFixedFontWidth = -1;

	scrolldx = scrolldy = 0;
}

// Traditional ctor
Fl_Text_Display::Fl_Text_Display(int X, int Y, int W, int H,  const char* l)
: Fl_Group(X, Y, W, H, l)
{
    ctor_init();
}

// New style ctor
Fl_Text_Display::Fl_Text_Display(const char* l,int layout_size,Fl_Align layout_al,int label_w)
: Fl_Group(l,layout_size,layout_al,label_w) 
{
    ctor_init();
}

/**
 * Free a text display and release its associated memory.  Note, the text
 * BUFFER that the text display displays is a separate entity and is not
 * freed, nor are the style buffer or style table.
 */
Fl_Text_Display::~Fl_Text_Display() 
{   
    delete mVScrollBar;
    delete mHScrollBar;

    if (mBuffer) {
        mBuffer->remove_modify_callback(buffer_modified_cb, this);
        mBuffer->remove_predelete_callback( buffer_predelete_cb, this );
    }    
}

/*
 * Attach a text buffer to display, replacing the current buffer (if any)
 */
void Fl_Text_Display::buffer( Fl_Text_Buffer *buf )
{
    if(!buf) {
        if(mBuffer) {
            mBuffer->remove_modify_callback(buffer_modified_cb, this);
            mBuffer->remove_predelete_callback( buffer_predelete_cb, this);
        }
        mBuffer = 0;
        return;
    }

	/* If the text display is already displaying a buffer, clear it off
     of the display and remove our callback from it */
    if ( mBuffer != 0 ) {
        buffer_modified_cb( 0, 0, mBuffer->length(), 0, 0, this );
        mBuffer->remove_modify_callback( buffer_modified_cb, this );
        mBuffer->remove_predelete_callback( buffer_predelete_cb, this );
    }

	/* Add the buffer to the display, and attach a callback to the buffer for
     receiving modification information when the buffer contents change */
    mBuffer = buf;
    mBuffer->add_modify_callback( buffer_modified_cb, this );
    mBuffer->add_predelete_callback( buffer_predelete_cb, this );

	/* Update the display */
    buffer_modified_cb( 0, buf->length(), 0, 0, 0, this );

    set_font();
}

/**
 * Attach (or remove) highlight information in text display and redisplay.
 * Highlighting information consists of a style buffer which parallels the
 * normal text buffer, but codes font and color information for the display;
 * a style table which translates style buffer codes (indexed by buffer
 * character - 'A') into fonts and colors; and a callback mechanism for
 * as-needed highlighting, triggered by a style buffer entry of
 * "unfinishedStyle".  Style buffer can trigger additional redisplay during
 * a normal buffer modification if the buffer contains a primary Fl_Text_Selection
 * (see extendRangeForStyleMods for more information on this protocol).
 *
 * Style buffers, tables and their associated memory are managed by the caller.
 */
void Fl_Text_Display::highlight_data(Fl_Text_Buffer *styleBuffer,
    Style_Table_Entry *styleTable,
    int nStyles, char unfinishedStyle,
    Unfinished_Style_Cb unfinishedHighlightCB,
	void *cbArg ) 
{
    mStyleBuffer = styleBuffer;
    mStyleTable = styleTable;
    mNStyles = nStyles;
    mUnfinishedStyle = unfinishedStyle;
    mUnfinishedHighlightCB = unfinishedHighlightCB;
    mHighlightCBArg = cbArg;

    /* Call TextDSetFont to combine font information from style table and
     primary font, adjust font-related parameters, and then redisplay */
    set_font();

    relayout(FL_LAYOUT_DAMAGE|FL_LAYOUT_XYWH);
    redraw();
}

/** 
 * Change the (non highlight) font 
 */ 
void Fl_Text_Display::set_font()
{
    int fontWidth, i;

    /* If font size changes, cursor will be redrawn in a new position */
    //blankCursorProtrusions(textD);

    /* If there is a (syntax highlighting) style table in use, find the new
     maximum font height for this text display */
    fl_font(text_font(), text_size());

    mMaxsize = int(fl_height()+leading());
    for (i = 0; i < mNStyles; i++) {
        if(mStyleTable[i].attr == ATTR_IMAGE && mStyleTable[i].image) {
            mMaxsize = max(mMaxsize, mStyleTable[i].image->height());
        } else {
            fl_font(mStyleTable[i].font, mStyleTable[i].size);
            mMaxsize = max(mMaxsize, int(fl_height()+leading()));
        }
    }

	fl_font(text_font(), text_size());

#ifdef _WIN32
    TEXTMETRIC *fontStruct = fl_textmetric(), *styleFont;
    mMaxFontBound = fontStruct->tmMaxCharWidth;
    mMinFontBound = fontStruct->tmAveCharWidth;
    fontWidth = fontStruct->tmAveCharWidth;
    if((fontStruct->tmPitchAndFamily&TMPF_FIXED_PITCH)) {
        fontWidth = -1;
    } else {
        for (i=0; i<mNStyles; i++)
        {
            unsigned int size = mStyleTable[i].size;
            if(text_size()!=size) { fontWidth = -1; break; }
            fl_font(mStyleTable[i].font, mStyleTable[i].size);
            styleFont = fl_textmetric();
            if(styleFont != NULL && (styleFont->tmPitchAndFamily&TMPF_FIXED_PITCH) ) {
                fontWidth = -1;
                break;
            }
        }
    }
#else

#if !USE_XFT

    XFontStruct *fontStruct = (XFontStruct *)fl_xfont(), *styleFont;
    mMaxFontBound = fontStruct->max_bounds.width;
    mMinFontBound = fontStruct->min_bounds.width;
    fontWidth = fontStruct->max_bounds.width;
    if (fontWidth != fontStruct->min_bounds.width) {
        fontWidth = -1;
    } else {
        for (i=0; i<mNStyles; i++)
        {
            unsigned size = mStyleTable[i].size;
            if(text_size()!=size) { fontWidth = -1; break; }
            fl_font(mStyleTable[i].font, mStyleTable[i].size);
            styleFont = (XFontStruct *)fl_xfont();
            if(styleFont != NULL && (styleFont->max_bounds.width != fontWidth || styleFont->max_bounds.width != styleFont->min_bounds.width)) {
                fontWidth = -1;
                break;
            }
        }
    }

#else
    extern XftFont* fl_xftfont();
    XftFont *fontStruct = fl_xftfont(), *styleFont;
    mMaxFontBound = fontStruct->max_advance_width;
    mMinFontBound = fontStruct->max_advance_width;
    fontWidth = fontStruct->max_advance_width;

    for (i=0; i<mNStyles; i++)
    {
        unsigned size = mStyleTable[i].size;
        if(text_size()!=size) { fontWidth = -1; break; }
        fl_font(mStyleTable[i].font, mStyleTable[i].size);
        styleFont = fl_xftfont();
        if(styleFont != NULL && styleFont->max_advance_width != fontWidth) {
            fontWidth = -1;
            break;
        }
    }
    if(fontWidth==0) fontWidth=-1;
#endif /* !USE_XFT */

#endif /* _WIN32 */
    mFixedFontWidth = fontWidth;
}

#if 0
int TextDMinFontWidth(textDisp *textD, Boolean considerStyles) { 
    int fontWidth = textD->fontStruct->max_bounds.width; 
    int i; 

    if (considerStyles) { 
        for (i = 0; i < textD->nStyles; ++i) { 
            int thisWidth = (textD->styleTable[i].font)->min_bounds.width; 
            if (thisWidth < fontWidth) { 
                fontWidth = thisWidth; 
            } 
        } 
    } 
    return(fontWidth); 
} 

int TextDMaxFontWidth(textDisp *textD, Boolean considerStyles) { 
    int fontWidth = textD->fontStruct->max_bounds.width; 
    int i; 

    if (considerStyles) { 
        for (i = 0; i < textD->nStyles; ++i) { 
            int thisWidth = (textD->styleTable[i].font)->max_bounds.width; 
            if (thisWidth > fontWidth) { 
                fontWidth = thisWidth; 
            } 
        } 
    } 
    return(fontWidth); 
} 
#endif

/**
 * Change the size of the displayed text area
 */
void Fl_Text_Display::layout() 
{
    if(!visible_r() || /*!layout_damage() || */!buffer() ) {
        return;
    }
	//printf("Fl_Text_Display::layout() \n");

	int old_vlines, new_vlines;
    int X = 0, Y = 0, W = w(), H = h();
    box()->inset(X, Y, W, H);

    if(W<0 || H<0) {
        Fl_Widget::layout();
        return;
    }

	text_area.x = X+LEFT_MARGIN + mLineNumLeft + mLineNumWidth;
    text_area.w = W-LEFT_MARGIN-RIGHT_MARGIN - mLineNumWidth - mLineNumLeft;
    text_area.y = Y+BOTTOM_MARGIN;
    text_area.h = H-TOP_MARGIN-BOTTOM_MARGIN;

	{		
		if(mContinuousWrap && !mWrapMargin)
				mHScrollBar->clear_visible();
		else	mHScrollBar->set_visible();			
		
		if(scrollbar_align()&FL_ALIGN_LEFT) {
			text_area.x += mVScrollBar->w();
		}
		text_area.w -= mVScrollBar->w();		

		if(mHScrollBar->visible()) {
			if(scrollbar_align()&FL_ALIGN_TOP)
				text_area.y += mHScrollBar->h();
		    text_area.h -= mHScrollBar->h();
		}
		
		// Recalc scrollbar sizes:

        int hor_y = Y+H-mHScrollBar->h();			
        int hor_x = X+mLineNumLeft+mLineNumWidth;

        int ver_y = Y;
        int ver_x = X+W-mVScrollBar->w();

        if(scrollbar_align()&FL_ALIGN_LEFT) {
            ver_x = X;
            hor_x += mVScrollBar->w();
        }
        if(scrollbar_align()&FL_ALIGN_TOP && mHScrollBar->visible()) {
            hor_y = Y;
            ver_y += mHScrollBar->h();
        }

		// Vertical
        mVScrollBar->resize(ver_x, ver_y,                           
						   mVScrollBar->w(),
						   text_area.h+TOP_MARGIN+BOTTOM_MARGIN);

		if(mHScrollBar->visible()) {
			// Horizontal
			mHScrollBar->resize(hor_x, hor_y,
				text_area.w+LEFT_MARGIN+RIGHT_MARGIN,
				mHScrollBar->h());        
		}
	}

	old_vlines = mNVisibleLines;
	new_vlines = (text_area.h / mMaxsize);
	if(new_vlines<0) new_vlines=1;

	if((layout_damage() & FL_LAYOUT_W) && mContinuousWrap && !mWrapMargin) 
	{		
		int oldFirstChar = mFirstChar;		
		mNBufferLines	= count_lines(0, buffer()->length(), true);
        mFirstChar		= line_start(mFirstChar);
	    mTopLineNum		= count_lines(0, mFirstChar, true) + 1;
		absolute_top_line_number(oldFirstChar);
	}

	if(old_vlines != new_vlines) 
	{
        mLineStarts.resize(new_vlines+1);
	    mNVisibleLines = new_vlines;
		calc_line_starts(0, mNVisibleLines);
		calc_last_char();        
	
	} else if(mContinuousWrap && !mWrapMargin) {
        calc_line_starts(0, mNVisibleLines);
	    calc_last_char();
	}

	// everything will fit in the viewport
    if (mNBufferLines < mNVisibleLines && mTopLineNum!=1) {		
		offset_line_starts(1);        
    }
    /*else if(mLineStarts[mNVisibleLines-1] == -1) {
		//If empty lines become visible, there may be an opportunity to display more text by scrolling down		
        int newTop = mTopLineNum-1;
        if(newTop>0) {
			offset_line_starts(newTop);
			redraw();
		}
    }*/	
	else if((new_vlines!=old_vlines || (layout_damage()&FL_LAYOUT_W)) && mTopLineNum + mNVisibleLines > mNBufferLines+2) {
		offset_line_starts(max(1, mNBufferLines - mNVisibleLines + 2));		
	}

	calc_longest_vline();

	// in case horizontal offset is now greater than longest line
    int maxhoffset = max(0, mLongestVline - text_area.w);
    if (mHorizOffset > maxhoffset) {
        mHorizOffset = maxhoffset;
		redraw();
    }

    update_v_scrollbar();	
	update_h_scrollbar();

	// clear the layout flag
    Fl_Widget::layout();
}

/**
 * Refresh a rectangle of the text display.  left and top are in coordinates of
 * the text drawing window
 */
void Fl_Text_Display::draw_text( int left, int top, int width, int height ) 
{
    int fontHeight, firstLine, lastLine, line;

    /* find the line number range of the display */
    fontHeight = mMaxsize;
    firstLine  = ((top - text_area.y - fontHeight + 1) / fontHeight)-1;
    lastLine   = ((top + height - text_area.y) / fontHeight)+1;

	/*
	if(scrolldy < 0) {
		// Hack to get scrolling down working 
		top -= fontHeight*2;
		height += fontHeight*2;
	}
	
	if(top < text_area.y) top = text_area.y;
	if(top+height < text_area.y+text_area.h) height = text_area.h-top;	
	*/
	if(width<1 || height<1) return;

    fl_push_clip(left, top, width, height);

    /* draw the lines */
    for ( line = firstLine; line <= lastLine; line++ ) {
        draw_vline( line, left, left + width, 0, INT_MAX );
    }

    /* draw the line numbers if exposed area includes them */
    if(mLineNumWidth != 0 && left <= mLineNumLeft + mLineNumWidth)
		draw_line_numbers();

    fl_pop_clip();
}

void Fl_Text_Display::redisplay_range(int start, int end)
{
#if HAVE_XUTF8
    start -= find_prev_char(start-1);
    end   += find_next_char(end+1);
#endif
    if (damage_range1_start == -1 && damage_range1_end == -1) {
        damage_range1_start = start;
        damage_range1_end = end;
    } else if ((start >= damage_range1_start && start <= damage_range1_end) ||
            (end >= damage_range1_start && end <= damage_range1_end)) {
        damage_range1_start = min(damage_range1_start, start);
        damage_range1_end = max(damage_range1_end, end);
    } else if (damage_range2_start == -1 && damage_range2_end == -1) {
        damage_range2_start = start;
        damage_range2_end = end;
    } else {
        damage_range2_start = min(damage_range2_start, start);
        damage_range2_end = max(damage_range2_end, end);
    }
    redraw(FL_DAMAGE_SCROLL);
}

/**
 * Refresh all of the text between buffer positions "start" and "end"
 * not including the character at the position "end".
 * If end points beyond the end of the buffer, refresh the whole display
 * after pos, including blank lines which are not technically part of
 * any range of characters.
 */
void Fl_Text_Display::draw_range(int start, int end) 
{
    int i, startLine, lastLine, startIndex, endIndex;

    /* If the range is outside of the displayed text, just return */
    if ( end < mFirstChar || ( start > mLastChar && !empty_vlines() ) )
        return;

    /* Clean up the starting and ending values */
    if ( start < 0 ) start = 0;
    if ( start > mBuffer->length() ) start = mBuffer->length();
    if ( end < 0 ) end = 0;
    if ( end > mBuffer->length() ) end = mBuffer->length();

    /* Get the starting and ending lines */
    if ( start < mFirstChar )
        start = mFirstChar;
    if ( !position_to_line( start, &startLine ) )
        startLine = mNVisibleLines - 1;
    if ( end >= mLastChar ) {
        lastLine = mNVisibleLines - 1;
    } else {
        if ( !position_to_line( end, &lastLine ) ) {
            /* shouldn't happen */
            lastLine = mNVisibleLines - 1;
        }
    }

    /* Get the starting and ending positions within the lines */
    startIndex = mLineStarts[ startLine ] == -1 ? 0 :
        start - mLineStarts[ startLine ];
    if ( end >= mLastChar )
        endIndex = INT_MAX;
    else if ( mLineStarts[ lastLine ] == -1 )
        endIndex = 0;
    else
        endIndex = end - mLineStarts[ lastLine ];

    /* If the starting and ending lines are the same, redisplay the single
     line between "start" and "end" */
    if ( startLine == lastLine ) {
        draw_vline( startLine, 0, INT_MAX, startIndex, endIndex );
        return;
    }

    /* Redisplay the first line from "start" */
    draw_vline( startLine, 0, INT_MAX, startIndex, INT_MAX );

    /* Redisplay the lines in between at their full width */
    for ( i = startLine + 1; i < lastLine; i++ )
        draw_vline( i, 0, INT_MAX, 0, INT_MAX );

    /* Redisplay the last line to "end" */
    draw_vline( lastLine, 0, INT_MAX, 0, endIndex );
}

/**
 * Set the position of the text insertion cursor for text display
 */
void Fl_Text_Display::insert_position( int newPos ) 
{
    /* make sure new position is ok, do nothing if it hasn't changed */
    if ( newPos == mCursorPos )
        return;
    if ( newPos < 0 ) newPos = 0;
    if ( newPos > mBuffer->length() ) newPos = mBuffer->length();

    /* cursor movement cancels vertical cursor motion column */
    mCursorPreferredCol = -1;
	
	mCursorPos = newPos;	
	
	update_h_scrollbar();
	update_v_scrollbar();

	redraw(FL_DAMAGE_VALUE);
}

void Fl_Text_Display::show_cursor(int b) 
{
    mCursorOn = b;
	redraw(FL_DAMAGE_VALUE);
}

void Fl_Text_Display::cursor_style(int style) 
{
    mCursorStyle = style;
    if (mCursorOn) show_cursor();
}

void Fl_Text_Display::wrap_mode(int wrap, int wrapMargin)
{
    mWrapMargin = wrapMargin;
    mContinuousWrap = wrap;

    relayout(FL_LAYOUT_DAMAGE|FL_LAYOUT_XYWH);
    redraw();
}

/**
 * Insert "text" at the current cursor location.  This has the same
 * effect as inserting the text into the buffer using BufInsert and
 * then moving the insert position after the newly inserted text, except
 * that it's optimized to do less redrawing.
 */
void Fl_Text_Display::insert(const char* text) 
{
    int pos = mCursorPos;
    int len = strlen( text );
    mCursorToHint = pos + len;
    mBuffer->insert( pos, text );
    mCursorToHint = NO_HINT;
}

/**
 * Insert "text" (which must not contain newlines), overstriking the current
 * cursor location.
 */
void Fl_Text_Display::overstrike(const char* text) 
{
    int startPos = mCursorPos;
    Fl_Text_Buffer *buf = mBuffer;
    int lineStart = buf->line_start( startPos );
    int textLen = strlen( text );
    int i, p, endPos, indent, startIndent, endIndent;
    const char *c;
    char ch, *paddedText = NULL;

  /* determine how many displayed character positions are covered */
    startIndent = mBuffer->count_displayed_characters( lineStart, startPos );
    indent = startIndent;
    for ( c = text; *c != '\0'; c++ )
        indent += Fl_Text_Buffer::character_width( *c, indent, buf->tab_distance() );
    endIndent = indent;

  /* find which characters to remove, and if necessary generate additional
     padding to make up for removed control characters at the end */
    indent = startIndent;
    for ( p = startPos; ; p++ ) {
        if ( p == buf->length() )
            break;
        ch = buf->character( p );
        if ( ch == '\n' )
            break;
        indent += Fl_Text_Buffer::character_width( ch, indent, buf->tab_distance() );
        if ( indent == endIndent ) {
            p++;
            break;
        } else if ( indent > endIndent ) {
            if ( ch != '\t' ) {
                p++;
                paddedText = new char [ textLen + FL_TEXT_MAX_EXP_CHAR_LEN + 1 ];
                strcpy( paddedText, text );
                for ( i = 0; i < indent - endIndent; i++ )
                    paddedText[ textLen + i ] = ' ';
                paddedText[ textLen + i ] = '\0';
            }
            break;
        }
    }
    endPos = p;

    mCursorToHint = startPos + textLen;
    buf->replace( startPos, endPos, paddedText == NULL ? text : paddedText );
    mCursorToHint = NO_HINT;
    if ( paddedText != NULL )
        delete [] paddedText;
}

/**
 * Translate a buffer text position to the XY location where the top left
 * of the cursor would be positioned to point to that character.  Returns
 * 0 if the position is not displayed because it is VERTICALLY out
 * of view.  If the position is horizontally out of view, returns the
 * X coordinate where the position would be if it were visible.
 */
int Fl_Text_Display::position_to_xy( int pos, int* X, int* Y ) 
{
    int charIndex, lineStartPos, fontHeight, lineLen;
    int visLineNum, charLen, outIndex, xStep, charStyle;
    char expandedChar[ FL_TEXT_MAX_EXP_CHAR_LEN ];

	/* If position is not displayed, return false */
    if (pos < mFirstChar || (pos > mLastChar && !empty_vlines()))
        return 0;

	/* Calculate Y coordinate */
    if (!position_to_line(pos, &visLineNum)) return 0;
    fontHeight = mMaxsize;
    *Y = text_area.y + visLineNum * fontHeight;

	/* Get the text, length, and  buffer position of the line. If the position
     is beyond the end of the buffer and should be at the first position on
     the first empty line, don't try to get or scan the text  */
    lineStartPos = mLineStarts[visLineNum];
    if ( lineStartPos == -1 ) {
        *X = text_area.x - mHorizOffset;
        return 1;
    }
    lineLen = vline_length( visLineNum );
    mBuffer->text_range( m_lineBuffer, lineStartPos, lineStartPos + lineLen );

	/* Step through character positions from the beginning of the line
     to "pos" to calculate the X coordinate */
    xStep = text_area.x - mHorizOffset;
    outIndex = 0;
    for ( charIndex = 0; charIndex < pos - lineStartPos; charIndex++ ) {
        charLen = Fl_Text_Buffer::expand_character( m_lineBuffer[ charIndex ], outIndex, expandedChar,
                mBuffer->tab_distance() );
#if HAVE_XUTF8
        if (charLen > 1 && (m_lineBuffer[ charIndex ] & 0x80)) {
            int i, ii = 0;;
            i = fl_utf_charlen(m_lineBuffer[ charIndex ]);
            while (i > 1) {
                i--;
                ii++;
                expandedChar[ii] = m_lineBuffer[ charIndex + ii];
            }
        }
#endif
        charStyle = position_style( lineStartPos, lineLen, charIndex,
                outIndex );
        xStep += string_width( expandedChar, charLen, charStyle );
        outIndex += charLen;
    }
    *X = xStep;
    return 1;
}

/**
 * In continuous wrap mode, internal line numbers are calculated after 
 * wrapping.  A separate non-wrapped line count is maintained when line 
 * numbering is turned on.  There is some performance cost to maintaining this 
 * line count, so normally absolute line numbers are not tracked if line 
 * numbering is off.  This routine allows callers to specify that they still 
 * want this line count maintained (for use via TextDPosToLineAndCol). 
 * More specifically, this allows the line number reported in the statistics 
 * line to be calibrated in absolute lines, rather than post-wrapped lines. 
 */ 
void Fl_Text_Display::maintain_absolute_top_line_number(int state) 
{ 
    mNeedAbsTopLineNum = state; 
    reset_absolute_top_line_number(); 
} 

/**
 * Returns the absolute (non-wrapped) line number of the first line displayed. 
 * Returns 0 if the absolute top line number is not being maintained. 
 */ 
int Fl_Text_Display::get_absolute_top_line_number() 
{ 
    if (!mContinuousWrap) 
        return mTopLineNum; 
    if (maintaining_absolute_top_line_number()) 
        return mAbsTopLineNum; 
    return 0; 
} 

/**
 * Re-calculate absolute top line number for a change in scroll position. 
 */ 
void Fl_Text_Display::absolute_top_line_number(int oldFirstChar) 
{
    if (maintaining_absolute_top_line_number()) {
        if (mFirstChar < oldFirstChar)
            mAbsTopLineNum -= buffer()->count_lines(mFirstChar, oldFirstChar);
        else
            mAbsTopLineNum += buffer()->count_lines(oldFirstChar, mFirstChar);
    }
}

/**
 * Return true if a separate absolute top line number is being maintained 
 * (for displaying line numbers or showing in the statistics line). 
 */ 
int Fl_Text_Display::maintaining_absolute_top_line_number() 
{ 
    return mContinuousWrap && 
        (mLineNumWidth != 0 || mNeedAbsTopLineNum); 
} 

/**
 * Count lines from the beginning of the buffer to reestablish the 
 * absolute (non-wrapped) top line number.  If mode is not continuous wrap, 
 * or the number is not being maintained, does nothing. 
 */ 
void Fl_Text_Display::reset_absolute_top_line_number() 
{ 
    mAbsTopLineNum = 1; 
    absolute_top_line_number(0); 
} 


/**
 * Find the line number of position "pos".  Note: this only works for
 * displayed lines.  If the line is not displayed, the function returns
 * 0 (without the lineStarts array it could turn in to very long
 * calculation involving scanning large amounts of text in the buffer).
 * If continuous wrap mode is on, returns the absolute line number (as opposed 
 * to the wrapped line number which is used for scrolling). 
 */
int Fl_Text_Display::position_to_linecol( int pos, int* lineNum, int* column ) 
{
    int retVal;

  /* In continuous wrap mode, the absolute (non-wrapped) line count is 
     maintained separately, as needed.  Only return it if we're actually 
     keeping track of it and pos is in the displayed text */ 
    if (mContinuousWrap) { 
        if (!maintaining_absolute_top_line_number() ||  pos < mFirstChar || pos > mLastChar) 
            return 0; 
        *lineNum = mAbsTopLineNum + buffer()->count_lines(mFirstChar, pos); 
        *column = buffer()->count_displayed_characters(buffer()->line_start(pos), pos); 
        return 1; 
    } 

    retVal = position_to_line( pos, lineNum );
    if ( retVal ) {
        *column = mBuffer->count_displayed_characters(
                mLineStarts[ *lineNum ], pos );
        *lineNum += mTopLineNum;
    }
    return retVal;
}

/**
 * Return 1 if position (X, Y) is inside of the primary Fl_Text_Selection
 */
int Fl_Text_Display::in_selection( int X, int Y ) 
{
    int row, column, pos = xy_to_position( X, Y, CHARACTER_POS );
    Fl_Text_Buffer *buf = mBuffer;

    xy_to_rowcol( X, Y, &row, &column, CHARACTER_POS );
    if (range_touches_selection(buf->primary_selection(), mFirstChar, mLastChar))
        column = wrapped_column(row, column);
    return buf->primary_selection()->includes(pos, buf->line_start( pos ), column);
}


/**
 * Correct a column number based on an unconstrained position (as returned by 
 * TextDXYToUnconstrainedPosition) to be relative to the last actual newline 
 * in the buffer before the row and column position given, rather than the 
 * last line start created by line wrapping.  This is an adapter
 * for rectangular selections and code written before continuous wrap mode,
 * which thinks that the unconstrained column is the number of characters
 * from the last newline.  Obviously this is time consuming, because it
 * invloves character re-counting. 
 */
int Fl_Text_Display::wrapped_column(int row, int column) 
{
    int lineStart, dispLineStart;

    if (!mContinuousWrap || row < 0 || row > mNVisibleLines)
        return column;
    dispLineStart = mLineStarts[row];
    if (dispLineStart == -1)
        return column;
    lineStart = buffer()->line_start(dispLineStart);
    return column + buffer()->count_displayed_characters(lineStart, dispLineStart);
}

/**
 * Correct a row number from an unconstrained position (as returned by 
 * TextDXYToUnconstrainedPosition) to a straight number of newlines from the 
 * top line of the display.  Because rectangular selections are based on 
 * newlines, rather than display wrapping, and anywhere a rectangular selection 
 * needs a row, it needs it in terms of un-wrapped lines. 
 */ 
int Fl_Text_Display::wrapped_row(int row) { 
    if (!mContinuousWrap || row < 0 || row > mNVisibleLines)
        return row;
    return buffer()->count_lines(mFirstChar, mLineStarts[row]);
}

/**
 * Scroll the display to bring insertion cursor into view.
 *
 * Note: it would be nice to be able to do this without counting lines twice
 * (scroll() counts them too) and/or to count from the most efficient
 * starting point, but the efficiency of this routine is not as important to
 * the overall performance of the text display.
 */
void Fl_Text_Display::display_insert()
{
    int hOffset, topLine, X, Y;
    hOffset = mHorizOffset;
    topLine = mTopLineNum;

    //int mLastChar = mLineStarts[mNVisibleLines-1];
    if (insert_position() < mFirstChar) {

		//printf("insert_position() < mFirstChar\n");
        topLine -= count_lines(insert_position(), mFirstChar, false);

    } else if (insert_position() > mLastChar && !empty_vlines())
    {
		//printf("insert_position() > mLastChar\n");
        topLine += count_lines(mLastChar -
                (wrap_uses_character(mLastChar) ? 0 : 1),
                insert_position(), false);
    } else if(insert_position() == mLastChar && !empty_vlines() &&
            !wrap_uses_character(mLastChar))
    {
		//printf("insert_position() == mLastChar\n");
        topLine++;
    }	

    if (topLine < 1) {
        fprintf(stderr, "internal consistency check tl1 failed %d %d / %d %d\n", topLine, mTopLineNum, insert_position(), mFirstChar);
        topLine = 1;
    }		

    /* Find the new setting for horizontal offset (this is a bit ungraceful).
     If the line is visible, just use PositionToXY to get the position
     to scroll to, otherwise, do the vertical scrolling first, then the
     horizontal */
    if (!position_to_xy( mCursorPos, &X, &Y ))
    {				
        do_scroll(topLine, hOffset);
        if (!position_to_xy( mCursorPos, &X, &Y ))
            return;   /* Give up, it's not worth it (but why does it fail?) */
    }
	
    if (X+10 > (text_area.x + text_area.w))
        hOffset += X-(text_area.x + text_area.w) + 10;
    else if(X-10 < text_area.x)
        hOffset += X-text_area.x - 10;

	if(hOffset<0) hOffset = 0;

    /* Do the scroll */
    if(topLine != mTopLineNum || hOffset != mHorizOffset)
        scroll(topLine, hOffset);	
}

void Fl_Text_Display::show_insert_position() {
    display_insert();
}

int Fl_Text_Display::find_next_char(int pos)
{
    unsigned ucs;
    char *ptr = buffer()->static_buffer()+pos;
    int len=0;
    char c;
    bool again;

    do {
        again = false;
        while(true) {
            if(pos < 0) return 0;
            c = buffer()->character( pos++ );
            if(!((c & 0x80) && !(c & 0x40))) break;
        }
        if(!len) len = fl_utf_charlen(c);
        int ulen = fl_utf2ucs((uchar*)(ptr+len), len, &ucs);
        if(fl_nonspacing(ucs)) {
            len+=ulen;
            again = true;;
        }
    } while(again);

    return len;
}

int Fl_Text_Display::find_prev_char(int pos)
{
    unsigned ucs;
    char *ptr = buffer()->static_buffer()+pos;
    int len=0;
    char c;
    bool again;

    do {
        again = false;
        while(true) {
            if(pos <= 0) return 0;
            c = buffer()->character( --pos );
            if(!((c & 0x80) && !(c & 0x40))) break;
        }
        if(!len) len = fl_utf_charlen(c);
        int ulen = fl_utf2ucs((uchar*)(ptr-len), len, &ucs);
        if(fl_nonspacing(ucs)) {
            len+=ulen;
            again = true;;
        }
    } while(again);

    return len;
}

/**
 * Cursor movement functions
 */
int Fl_Text_Display::move_right() {
#if HAVE_XUTF8
    insert_position( mCursorPos + find_next_char(insert_position()) );
#else
    if ( mCursorPos >= mBuffer->length() )
        return 0;
    insert_position( mCursorPos + 1 );
#endif
    return 1;
}

int Fl_Text_Display::move_left() {
#if HAVE_XUTF8
    insert_position( mCursorPos - find_prev_char(insert_position()) );
#else
    if ( mCursorPos < 1 )
        return 0;
    insert_position( mCursorPos - 1 );
#endif
    return 1;
}

int Fl_Text_Display::move_up(int lines) 
{
    int lineStartPos, column, prevLineStartPos, newPos, visLineNum;

    /* Find the position of the start of the line. Use the line starts array
     if possible */
    if ( position_to_line( mCursorPos, &visLineNum ) )
        lineStartPos = mLineStarts[ visLineNum ];
    else {
        lineStartPos = line_start( mCursorPos );
        visLineNum = -1;
    }
    if ( lineStartPos == 0 )
        return 0;

    /* Decide what column to move to, if there's a preferred column use that */
    column = mCursorPreferredCol >= 0 ? mCursorPreferredCol :
        mBuffer->count_displayed_characters( lineStartPos, mCursorPos );

    /* count forward from the start of the previous line to reach the column */
    if ( visLineNum != -1 && visLineNum != 0 && lines==1)
        prevLineStartPos = mLineStarts[ visLineNum - 1 ];
    else {
        prevLineStartPos = rewind_lines( lineStartPos, lines );
    }

    newPos = mBuffer->skip_displayed_characters( prevLineStartPos, column );

    if (mContinuousWrap)
        newPos = min(newPos, line_end(prevLineStartPos, true));

    /* move the cursor */
#if HAVE_XUTF8
    //insert_position( (mCursorPos-1) + find_next_char(newPos) );
	insert_position( newPos-1 + find_next_char(newPos) );
#else
    insert_position( newPos );
#endif

    /* if a preferred column wasn't aleady established, establish it */
    mCursorPreferredCol = column;
    return 1;
}

int Fl_Text_Display::move_down(int lines) 
{
    int lineStartPos, column, nextLineStartPos, newPos, visLineNum;

    if ( mCursorPos == mBuffer->length() )
        return 0;

    if ( position_to_line( mCursorPos, &visLineNum ) )
        lineStartPos = mLineStarts[ visLineNum ];
    else {
        lineStartPos = buffer()->line_start( mCursorPos );
        visLineNum = -1;
    }
    column = mCursorPreferredCol >= 0 ? mCursorPreferredCol :
        mBuffer->count_displayed_characters( lineStartPos, mCursorPos );

    nextLineStartPos = skip_lines( lineStartPos, lines, true );

    newPos = mBuffer->skip_displayed_characters( nextLineStartPos, column );

    if(mContinuousWrap)
        newPos = min(newPos, line_end(nextLineStartPos, true));

#if HAVE_XUTF8
    //insert_position( (mCursorPos-1) + find_next_char(newPos) );
	insert_position( newPos-1 + find_next_char(newPos) );
#else
    insert_position( newPos );
#endif

    mCursorPreferredCol = column;
    return 1;
}

/**
 * Same as BufCountLines, but takes in to account wrapping if wrapping is 
 * turned on.  If the caller knows that startPos is at a line start, it 
 * can pass "startPosIsLineStart" as True to make the call more efficient 
 * by avoiding the additional step of scanning back to the last newline. 
 */ 
int Fl_Text_Display::count_lines(int startPos, int endPos, bool startPosIsLineStart)
{ 
    int retLines, retPos, retLineStart, retLineEnd;

    /* If we're not wrapping use simple (and more efficient) BufCountLines */
    if (!mContinuousWrap)
        return buffer()->count_lines(startPos, endPos);

    wrapped_line_counter(buffer(), startPos, endPos, INT_MAX,
						 startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
						 &retLineEnd);
    return retLines;
}

/**
 * Same as BufCountForwardNLines, but takes in to account line breaks when 
 * wrapping is turned on. If the caller knows that startPos is at a line start, 
 * it can pass "startPosIsLineStart" as True to make the call more efficient 
 * by avoiding the additional step of scanning back to the last newline. 
 */ 
int Fl_Text_Display::skip_lines(int startPos, int nLines, bool startPosIsLineStart)
{
    int retLines, retPos, retLineStart, retLineEnd;

    /* if we're not wrapping use more efficient BufCountForwardNLines */
    if (!mContinuousWrap)
        return buffer()->skip_lines(startPos, nLines);

    /* wrappedLineCounter can't handle the 0 lines case */
    if (nLines == 0)
        return startPos;

    /* use the common line counting routine to count forward */
    wrapped_line_counter(buffer(), startPos, buffer()->length(),
					nLines, startPosIsLineStart, 0, &retPos, &retLines, &retLineStart,
					&retLineEnd);
    return retPos;
}

/* *
 * Same as BufEndOfLine, but takes in to account line breaks when wrapping 
 * is turned on.  If the caller knows that startPos is at a line start, it 
 * can pass "startPosIsLineStart" as True to make the call more efficient 
 * by avoiding the additional step of scanning back to the last newline. 
 * 
 * Note that the definition of the end of a line is less clear when continuous 
 * wrap is on.  With continuous wrap off, it's just a pointer to the newline 
 * that ends the line.  When it's on, it's the character beyond the last 
 * DISPLAYABLE character on the line, where a whitespace character which has 
 * been "converted" to a newline for wrapping is not considered displayable. 
 * Also note that, a line can be wrapped at a non-whitespace character if the 
 * line had no whitespace.  In this case, this routine returns a pointer to 
 * the start of the next line.  This is also consistent with the model used by 
 * visLineLength. 
 */ 
int Fl_Text_Display::line_end(int pos, bool startPosIsLineStart) 
{ 
    int retLines, retPos, retLineStart, retLineEnd; 

   /* If we're not wrapping use more efficien BufEndOfLine */ 
    if (!mContinuousWrap) 
        return buffer()->line_end(pos); 

    if (pos == buffer()->length()) 
        return pos; 
    
	wrapped_line_counter(buffer(), pos, buffer()->length(), 1, 
				startPosIsLineStart, 0, &retPos, &retLines, &retLineStart, 
				&retLineEnd); 
    return retLineEnd; 
} 

/**
 * Same as BufStartOfLine, but returns the character after last wrap point 
 * rather than the last newline. 
 */ 
int Fl_Text_Display::line_start(int pos) 
{ 
    int retLines, retPos, retLineStart, retLineEnd; 

   /* If we're not wrapping, use the more efficient BufStartOfLine */ 
    if (!mContinuousWrap) 
        return buffer()->line_start(pos); 

    wrapped_line_counter(buffer(), buffer()->line_start(pos), pos, INT_MAX, true, 0, 
					&retPos, &retLines, &retLineStart, &retLineEnd); 
    return retLineStart; 
} 

/* 
 * Same as BufCountBackwardNLines, but takes in to account line breaks when 
 * wrapping is turned on. 
 */ 
int Fl_Text_Display::rewind_lines(int startPos, int nLines) 
{ 
    Fl_Text_Buffer *buf = buffer(); 
    int pos, lineStart, retLines, retPos, retLineStart, retLineEnd; 

   /* If we're not wrapping, use the more efficient BufCountBackwardNLines */ 
    if (!mContinuousWrap) 
        return buf->rewind_lines(startPos, nLines); 

    pos = startPos; 
    while (true) { 
        lineStart = buf->line_start(pos); 
        wrapped_line_counter(buf, lineStart, pos, INT_MAX, 
            true, 0, &retPos, &retLines, &retLineStart, &retLineEnd); 
        if (retLines > nLines) 
            return skip_lines(lineStart, retLines-nLines, true); 
        nLines -= retLines; 
        pos = lineStart - 1; 
        if (pos < 0) 
            return 0; 
        nLines -= 1; 
    } 
} 

void Fl_Text_Display::next_word() 
{
    int pos = insert_position();
    while ( pos < buffer()->length() && (
                isalnum( buffer()->character( pos ) ) || buffer()->character( pos ) == '_' ) ) {
        pos++;
    }
    while ( pos < buffer()->length() && !( isalnum( buffer()->character( pos ) ) || buffer()->character( pos ) == '_' ) ) {
        pos++;
    }

    insert_position( pos );
}

void Fl_Text_Display::previous_word() 
{
    int pos = insert_position();
    pos--;
    while ( pos && !( isalnum( buffer()->character( pos ) ) || buffer()->character( pos ) == '_' ) ) {
        pos--;
    }
    while ( pos && ( isalnum( buffer()->character( pos ) ) || buffer()->character( pos ) == '_' ) ) {
        pos--;
    }
    if ( !( isalnum( buffer()->character( pos ) ) || buffer()->character( pos ) == '_' ) ) pos++;

    insert_position( pos );
}

/**
 * Callback attached to the text buffer to receive delete information before 
 * the modifications are actually made. 
 */ 
void Fl_Text_Display::buffer_predelete_cb(int pos, int nDeleted, void *cbArg) 
{ 
    Fl_Text_Display *textD = (Fl_Text_Display *)cbArg; 
    if (textD->mContinuousWrap && (textD->mFixedFontWidth == -1 || textD->mModifyingTabDistance)) {
      /* Note: we must perform this measurement, even if there is not a 
      single character deleted; the number of "deleted" lines is the 
      number of visual lines spanned by the real line in which the 
      modification takes place. 
      Also, a modification of the tab distance requires the same 
      kind of calculations in advance, even if the font width is "fixed", 
      because when the width of the tab characters changes, the layout 
      of the text may be completely different. */ 
        textD->measure_deleted_lines(pos, nDeleted); 
    } else {
        textD->mSuppressResync = 0; /* Probably not needed, but just in case */ 
	}
} 

/**
 * Callback attached to the text buffer to receive modification information
 */
void Fl_Text_Display::buffer_modified_cb(int pos, int nInserted, int nDeleted,
										 int nRestyled, const char *deletedText, void *cbArg)
{
	Fl_Text_Display *textD = ( Fl_Text_Display * ) cbArg;
    int origCursorPos = textD->mCursorPos;

    /* buffer modification cancels vertical cursor motion column */
    if ( nInserted != 0 || nDeleted != 0 )
        textD->mCursorPreferredCol = -1;

    /* Update the cursor position */
    if ( textD->mCursorToHint != NO_HINT ) {
        textD->mCursorPos = textD->mCursorToHint;
        textD->mCursorToHint = NO_HINT;
    } else if ( textD->mCursorPos > pos ) {
        if ( textD->mCursorPos < pos + nDeleted )
            textD->mCursorPos = pos;
        else
            textD->mCursorPos += nInserted - nDeleted;
    }

    int scrolled, linesInserted, linesDeleted, startDispPos, endDispPos;    
    Fl_Text_Buffer *buf = textD->mBuffer;
    int oldFirstChar = textD->mFirstChar;
    int wrapModStart, wrapModEnd;

    /* Count the number of lines inserted and deleted, and in the case
     of continuous wrap mode, how much has changed */
    if (textD->mContinuousWrap) {
        textD->find_wrap_range(deletedText, pos, nInserted, nDeleted,
							   &wrapModStart, &wrapModEnd, &linesInserted, &linesDeleted);		
    } else {
        linesInserted	= nInserted == 0 ? 0 : buf->count_lines( pos, pos + nInserted );
        linesDeleted	= nDeleted  == 0 ? 0 : countlines( deletedText );
    }

    /* Update the line starts and topLineNum */
    if ( nInserted != 0 || nDeleted != 0) {
        if (textD->mContinuousWrap) {
            textD->update_line_starts( wrapModStart, wrapModEnd-wrapModStart,
                nDeleted + pos-wrapModStart + (wrapModEnd-(pos+nInserted)),
                linesInserted, linesDeleted, &scrolled );
        } else {

            textD->update_line_starts( pos, nInserted, nDeleted, linesInserted,
									linesDeleted, &scrolled );
        }

    } else
        scrolled = 0;

    /* If we're counting non-wrapped lines as well, maintain the absolute
     (non-wrapped) line number of the text displayed */
    if (textD->maintaining_absolute_top_line_number() && (nInserted != 0 || nDeleted != 0))
    {
        if (pos + nDeleted < oldFirstChar)
            textD->mAbsTopLineNum += buf->count_lines(pos, pos + nInserted) - countlines(deletedText);
        else if (pos < oldFirstChar)
            textD->reset_absolute_top_line_number();
    }

    /* Update the line count for the whole buffer */
    textD->mNBufferLines += linesInserted - linesDeleted;

    // don't need to do anything else if not visible?
    if (!textD->visible_r()) return;

    /* If the changes caused scrolling, re-paint everything and we're done. */
    if ( scrolled ) {
        textD->redraw();
        if ( textD->mStyleBuffer )   /* See comments in extendRangeForStyleMods */
            textD->mStyleBuffer->primary_selection()->selected(0);
        return;
    }

    /* If the changes didn't cause scrolling, decide the range of characters
     that need to be re-painted.  Also if the cursor position moved, be
     sure that the redisplay range covers the old cursor position so the
     old cursor gets erased, and erase the bits of the cursor which extend
     beyond the left and right edges of the text. */
    startDispPos = textD->mContinuousWrap ? wrapModStart : pos;

    if ( origCursorPos == startDispPos && textD->mCursorPos != startDispPos )
        startDispPos = min( startDispPos, origCursorPos - 1 );

    if ( linesInserted == linesDeleted )
    {
        if ( nInserted == 0 && nDeleted == 0 )
            endDispPos = pos + nRestyled;
        else {
            //endDispPos = buf->line_end( pos + nInserted ) + 1;
            endDispPos = textD->mContinuousWrap ? wrapModEnd : buf->line_end( pos + nInserted ) + 1;
			
			/*if(origCursorPos >= startDispPos && (origCursorPos <= endDispPos || endDispPos == buf->length())) {
				textD->clear_cursor();
			}*/
        }
    } else {
        endDispPos = textD->mLastChar + 1;
		/*if (origCursorPos >= pos) {
			textD->clear_cursor();
		}*/
    }

    /* If more than one line is inserted/deleted, a line break may have
       been inserted or removed in between, and the line numbers may
       have changed. If only one line is altered, line numbers cannot
       be affected (the insertion or removal of a line break always
	   results in at least two lines being redrawn). 
	*/
	if((nInserted != 0 || nDeleted != 0) && (linesInserted>0 || linesDeleted>0)) 
	{						
		textD->redraw();
	} else {

		/* If there is a style buffer, check if the modification caused additional
		changes that need to be redisplayed.  (Redisplaying separately would
		cause double-redraw on almost every modification involving styled
		text).  Extend the redraw range to incorporate style changes */
		if ( textD->mStyleBuffer )
			textD->extend_range_for_styles( &startDispPos, &endDispPos );

	    /* Redisplay computed range */
		textD->redisplay_range( startDispPos, endDispPos );
	}

    textD->update_v_scrollbar();
    textD->update_h_scrollbar();	
}

void Fl_Text_Display::calc_longest_vline()
{	
	if(!mContinuousWrap || (mContinuousWrap && mWrapMargin>0)) {
		mLongestVline = 0;
		for (int i = 0; i < mNVisibleLines; i++)
			mLongestVline = max(mLongestVline, measure_vline(i));		
	}
}

/**
 * Find the line number of position "pos" relative to the first line of
 * displayed text. Returns 0 if the line is not displayed.
 */
int Fl_Text_Display::position_to_line( int pos, int *lineNum )
{
    int i;

    if(pos==0) {
        *lineNum = 0;
        return 1;
    }

    if ( pos < mFirstChar )
        return 0;
    if ( pos > mLastChar ) {      
        if ( empty_vlines() ) {
            if ( mLastChar < mBuffer->length() ) {
                if ( !position_to_line( mLastChar, lineNum ) ) {
                    fprintf( stderr, "Consistency check ptvl failed\n" );
                    return 0;
                }
                return ++( *lineNum ) <= mNVisibleLines - 1;
            } else {
                position_to_line( mLastChar - 1, lineNum );
                return 1;
            }
        }
        return 0;
    }

    for ( i = mNVisibleLines - 1; i >= 0; i-- ) {
        if ( mLineStarts[ i ] != -1 && pos >= mLineStarts[ i ] ) {
            *lineNum = i;
            return 1;
        }
    }

    return 0;   /* probably never be reached */
}

/**
 * Draw the text on a single line represented by "visLineNum" (the
 * number of lines down from the top of the display), limited by
 * "leftClip" and "rightClip" window coordinates and "leftCharIndex" and
 * "rightCharIndex" character positions (not including the character at
 * position "rightCharIndex").
 */
void Fl_Text_Display::draw_vline(int visLineNum, int leftClip, int rightClip, int leftCharIndex, int rightCharIndex) 
{
    Fl_Text_Buffer * buf = mBuffer;
    int i, X, Y, startX, charIndex, lineStartPos, lineLen, fontHeight;
    int stdCharWidth, charWidth, startIndex, charStyle, style;
    int charLen, outStartIndex, outIndex; //hasCursor=0, cursorX=0;
    int dispIndexOffset;//, cursorPos = mCursorPos;
    char expandedChar[ FL_TEXT_MAX_EXP_CHAR_LEN ], outStr[ MAX_DISP_LINE_LEN ];
    char *outPtr;

    /* Calculate Y coordinate of the string to draw */
    fontHeight = mMaxsize;
    Y = text_area.y + visLineNum * fontHeight;

    /* Shrink the clipping range to the active display area */
    leftClip	= max( text_area.x-LEFT_MARGIN, leftClip );
    rightClip	= min( rightClip, text_area.x + text_area.w );

    /* If line is not displayed, skip it */
    if ( visLineNum < 0 || visLineNum >= mNVisibleLines ) {
		clear_rect(0, leftClip, Y, rightClip, fontHeight);
		return;
	}

    /* Get the text, length, and  buffer position of the line to display */
    lineStartPos = mLineStarts[ visLineNum ];
    if ( lineStartPos == -1 ) {     
        lineLen = 0;
        m_lineBuffer.data()[0] = 0;
    } else {
        lineLen = vline_length( visLineNum );
        buf->text_range(m_lineBuffer, lineStartPos, lineStartPos + lineLen);
    }

    /* Space beyond the end of the line is still counted in units of characters
     of a standardized character width (this is done mostly because style
     changes based on character position can still occur in this region due
     to rectangular Fl_Text_Selections).  stdCharWidth must be non-zero to prevent a
     potential infinite loop if X does not advance */
    stdCharWidth = mMaxFontBound;//TMPFONTWIDTH; //mFontStruct->max_bounds.width;
    if ( stdCharWidth <= 0 ) {
        fprintf( stderr, "Internal Error, bad font measurement\n" );
        return;
    }

    /* Rectangular Fl_Text_Selections are based on "real" line starts (after a newline
     or start of buffer).  Calculate the difference between the last newline
     position and the line start we're using.  Since scanning back to find a
     newline is expensive, only do so if there's actually a rectangular
     Fl_Text_Selection which needs it */
    if(mContinuousWrap && 
	   (range_touches_selection(buf->primary_selection(), lineStartPos, lineStartPos + lineLen) || 
	   range_touches_selection(buf->secondary_selection(), lineStartPos, lineStartPos + lineLen) ||
       range_touches_selection(buf->highlight_selection(), lineStartPos, lineStartPos + lineLen)))
    {
        dispIndexOffset = buf->count_displayed_characters(buf->line_start(lineStartPos), lineStartPos);
    } else
        dispIndexOffset = 0;

    /* Step through character positions from the beginning of the line (even if
     that's off the left edge of the displayed area) to find the first
     character position that's not clipped, and the X coordinate for drawing
     that character */
    X = text_area.x - mHorizOffset;
    outIndex = 0;
    for ( charIndex = 0; ; charIndex++ )
    {
        charLen = charIndex >= lineLen ? 1 :
            Fl_Text_Buffer::expand_character(m_lineBuffer[ charIndex ], outIndex,
											 expandedChar, buf->tab_distance() );
#if HAVE_XUTF8
        if (charIndex < lineLen && charLen > 1 && (m_lineBuffer[ charIndex ] & 0x80)) {
            int i, ii = 0;;
            i = fl_utf_charlen(m_lineBuffer[ charIndex ]);
            while (i > 1) {
                i--;
                ii++;
                expandedChar[ii] = m_lineBuffer[ charIndex + ii];
            }
        }
#endif
        style = position_style( lineStartPos, lineLen, charIndex,
								outIndex + dispIndexOffset );
        charWidth = charIndex >= lineLen ? stdCharWidth :
            string_width( expandedChar, charLen, style );
        if ( X + charWidth >= leftClip && charIndex >= leftCharIndex ) {
            startIndex = charIndex;
            outStartIndex = outIndex;
            startX = X;
            break;
        }
        X += charWidth;
        outIndex += charLen;
    }		

    /* Scan character positions from the beginning of the clipping range, and
     draw parts whenever the style changes (also note if the cursor is on
     this line, and where it should be drawn to take advantage of the x
     position which we've gone to so much trouble to calculate) */
    outPtr = outStr;
    outIndex = outStartIndex;
    X = startX;
    for ( charIndex = startIndex; charIndex < rightCharIndex; charIndex++ )
    {
        charLen = charIndex >= lineLen ? 1 :
            Fl_Text_Buffer::expand_character( m_lineBuffer[ charIndex ], outIndex, expandedChar,
                buf->tab_distance() );
#if HAVE_XUTF8
        if (charIndex < lineLen && charLen > 1 && (m_lineBuffer[ charIndex ] & 0x80)) {
            int i, ii = 0;;
            i = fl_utf_charlen(m_lineBuffer[ charIndex ]);
            while (i > 1) {
                i--;
                ii++;
                expandedChar[ii] = m_lineBuffer[ charIndex + ii];
            }
        }
#endif
        charStyle = position_style( lineStartPos, lineLen, charIndex,
									outIndex + dispIndexOffset );
        for ( i = 0; i < charLen; i++ )
        {
            if ( i != 0 && charIndex < lineLen && m_lineBuffer[ charIndex ] == '\t' )
                charStyle = position_style( lineStartPos, lineLen,
											charIndex, outIndex + dispIndexOffset );
            if ( charStyle != style ) {
                draw_string( style, startX, Y, X, outStr, outPtr - outStr );
                //printf("cw=%i style=%u startX=%i Y=%i, X=%i\n",charWidth,style, startX, Y, X);
                startX = X;
                outPtr = outStr;
                style = charStyle;
            }
            if ( charIndex < lineLen ) {
                *outPtr = expandedChar[ i ];
                int l = 1;
#if HAVE_XUTF8
                if (*outPtr & 0x80) {
                    l = fl_utf_charlen(*outPtr);
                }
#endif
                charWidth = string_width( &expandedChar[ i ], l, charStyle );
            } else {
                charWidth = stdCharWidth;
                //X += charWidth;
            }
            outPtr++;
            X += charWidth;
            outIndex++;
        }
        if ( outPtr - outStr + FL_TEXT_MAX_EXP_CHAR_LEN >= MAX_DISP_LINE_LEN || X >= rightClip )
            break;
    }

    /* Draw the remaining style segment */
    draw_string( style, startX, Y, X, outStr, outPtr - outStr );
    //printf("> style=%u startX=%i Y=%i, X=%i, %s\n",style, startX, Y, X, outStr);
}

/**
 * Draw a string or blank area according to parameter "style", using the
 * appropriate colors and drawing method for that style, with top left
 * corner at X, y.  If style says to draw text, use "string" as source of
 * characters, and draw "nChars", if style is FILL, erase
 * rectangle where text would have drawn from X to toX and from Y to
 * the maximum Y extent of the current font(s).
 */
void Fl_Text_Display::draw_string( int style, int X, int Y, int toX,
    const char *string, int nChars ) 
{
    Style_Table_Entry * styleRec=0;

    /* Draw blank area rather than text, if that was the request */
    if ( style & FILL_MASK ) {
        clear_rect( style, X, Y, toX - X, mMaxsize );
        return;
    }

    /* Set font, color, and gc depending on style.  For normal text, GCs
     for normal drawing, or drawing within a Fl_Text_Selection or highlight are
     pre-allocated and pre-configured.  For syntax highlighting, GCs are
     configured here, on the fly.
     */

    Fl_Font font = text_font();
    int size = text_size();
    Fl_Color foreground;
    Fl_Color background;

    if ( style & STYLE_LOOKUP_MASK ) {
        styleRec = &mStyleTable[ ( style & STYLE_LOOKUP_MASK ) - 'A' ];
        font = styleRec->font;
        size = styleRec->size;
        foreground = styleRec->color;

        if(style & PRIMARY_MASK) {
            //background = fl_color_average(selection_color(), fl_invert(styleRec->color), 0.2);
            background = selection_color();
            foreground = fl_color_average(foreground, FL_WHITE, 0.3);
        }
        else if(style & HIGHLIGHT_MASK) {
            //background = fl_color_average(highlight_color(), fl_invert(styleRec->color), 0.2);
            background = highlight_color();
            foreground = fl_color_average(foreground, FL_WHITE, 0.3);
        }
        else
            background = color();

        //background = style & PRIMARY_MASK ? selection_color() :
        //style & HIGHLIGHT_MASK ? highlight_color() : color();

        if ( foreground == background )   /* B&W kludge */
            foreground = color();
    } else if ( style & HIGHLIGHT_MASK ) {
        foreground = highlight_label_color();
        background = highlight_color();
    } else if ( style & PRIMARY_MASK ) {
        foreground = selection_text_color();
        background = selection_color();
    } else {
        foreground = text_color();
        background = color();
    }

	fl_color(background);
	fl_rectf( X, Y, toX - X , mMaxsize );

    if(styleRec && styleRec->attr==ATTR_IMAGE && styleRec->image) {
        int iX = X;
        for(int n=0; n<nChars; n++) {
            styleRec->image->draw(iX, Y + mMaxsize - styleRec->image->height(), styleRec->image->width(), mMaxsize, (style&PRIMARY_MASK)?FL_SELECTED:0);
            iX += styleRec->image->width();
        }
    } else {
        fl_color( foreground );
        fl_font( font, size );
        fl_draw( string, nChars, X, Y + mMaxsize - fl_descent());
    }

   // Underline if style is UNDERLINE attr is set 
    if(styleRec && styleRec->attr==ATTR_UNDERLINE)
        fl_line(X, int(Y+mMaxsize-fl_descent()+1), toX-1, int(Y+mMaxsize-fl_descent()+1));
}

/**
 * Clear a rectangle with the appropriate background color for "style"
 */
void Fl_Text_Display::clear_rect(int style, int X, int Y, int width, int height) 
{
	/* A width of zero means "clear to end of window" to XClearArea */
    if ( width == 0 )
        return;

    if ( style & HIGHLIGHT_MASK ) {
        fl_color( highlight_color() );
        fl_rectf( X, Y, width, height );
    } else if ( style & PRIMARY_MASK ) {
        fl_color( selection_color() );
        fl_rectf( X, Y, width, height );
    } else {
        fl_color( color() );
        fl_rectf( X, Y, width, height );
    }
}

/**
 * Clear cursor, if needed 
 */
void Fl_Text_Display::clear_cursor() 
{	
	if(mCursorPosOld>-1) {
		fl_push_clip(text_area.x, text_area.y, text_area.w, text_area.h);
		draw_range(mCursorPosOld-1, mCursorPosOld+1);
		mCursorPosOld = -1;
		fl_pop_clip();
	}	
}

/**
 * Draw cursor, clears old position if needed
 */
void Fl_Text_Display::draw_cursor() 
{
	if(mCursorPos>=0) {
		//printf("drew cursor at pos: %d\n", mCursorPos);		
		draw_cursor(mCursorPos);
		mCursorPosOld = mCursorPos;
	}
}

/**
 * Draw a cursor with top center at X, Y.
 */
void Fl_Text_Display::draw_cursor(int pos) 
{
	int X, Y;
	position_to_xy(pos, &X, &Y);	

    typedef struct {
        int x1, y1, x2, y2;
    } Segment;

    Segment segs[ 5 ];
    int left, right, cursorWidth, midY;  
    int fontWidth = mMinFontBound-1;
    int nSegs = 0;
    int fontHeight = mMaxsize;
    int bot = Y + fontHeight - 1;

    if( X < text_area.x - LEFT_MARGIN || X > text_area.x + text_area.w + LEFT_MARGIN + RIGHT_MARGIN )
        return;

	/* For cursors other than the block, make them around 2/3 of a character
     width, rounded to an even number of pixels so that X will draw an
     odd number centered on the stem at x. */
    cursorWidth = (fontWidth/3) * 2;
    left = X - cursorWidth / 2;
    right = left + cursorWidth;

	/* Create segments and draw cursor */
    if ( mCursorStyle == CARET_CURSOR ) {
        midY = bot - fontHeight / 5;
        segs[ 0 ].x1 = left;	segs[ 0 ].y1 = bot; 
		segs[ 0 ].x2 = X;		segs[ 0 ].y2 = midY;
        segs[ 1 ].x1 = X;		segs[ 1 ].y1 = midY; 
		segs[ 1 ].x2 = right;	segs[ 1 ].y2 = bot;
        segs[ 2 ].x1 = left;	segs[ 2 ].y1 = bot; 
		segs[ 2 ].x2 = X;		segs[ 2 ].y2 = midY - 1;
        segs[ 3 ].x1 = X;		segs[ 3 ].y1 = midY - 1; 
		segs[ 3 ].x2 = right;	segs[ 3 ].y2 = bot;
        nSegs = 4;
    } else if ( mCursorStyle == NORMAL_CURSOR ) {
        
		segs[ 0 ].x1 = left;	segs[ 0 ].y1 = Y; 
		segs[ 0 ].x2 = right;	segs[ 0 ].y2 = Y;
        segs[ 1 ].x1 = X;		segs[ 1 ].y1 = Y; 
		segs[ 1 ].x2 = X;		segs[ 1 ].y2 = bot;
        segs[ 2 ].x1 = left;	segs[ 2 ].y1 = bot; 
		segs[ 2 ].x2 = right;	segs[ 2 ].y2 = bot;
		nSegs = 3;
    } else if ( mCursorStyle == HEAVY_CURSOR ) {
        segs[ 0 ].x1 = X - 1;	segs[ 0 ].y1 = Y; 
		segs[ 0 ].x2 = X - 1;	segs[ 0 ].y2 = bot;
        segs[ 1 ].x1 = X;		segs[ 1 ].y1 = Y; 
		segs[ 1 ].x2 = X;		segs[ 1 ].y2 = bot;
        segs[ 2 ].x1 = X + 1;	segs[ 2 ].y1 = Y; 
		segs[ 2 ].x2 = X + 1;	segs[ 2 ].y2 = bot;
        segs[ 3 ].x1 = left;	segs[ 3 ].y1 = Y; 
		segs[ 3 ].x2 = right;	segs[ 3 ].y2 = Y;
        segs[ 4 ].x1 = left;	segs[ 4 ].y1 = bot; 
		segs[ 4 ].x2 = right;	segs[ 4 ].y2 = bot;
        nSegs = 5;
    } else if ( mCursorStyle == DIM_CURSOR ) {
        midY = Y + fontHeight / 2;
        segs[ 0 ].x1 = X;		segs[ 0 ].y1 = Y; 
		segs[ 0 ].x2 = X;		segs[ 0 ].y2 = Y;
        segs[ 1 ].x1 = X;		segs[ 1 ].y1 = midY; 
		segs[ 1 ].x2 = X;		segs[ 1 ].y2 = midY;
        segs[ 2 ].x1 = X;		segs[ 2 ].y1 = bot; 
		segs[ 2 ].x2 = X;		segs[ 2 ].y2 = bot;
        nSegs = 3;
    } else if ( mCursorStyle == BLOCK_CURSOR ) {
        right = X + fontWidth;
        segs[ 0 ].x1 = X;		segs[ 0 ].y1 = Y; 
		segs[ 0 ].x2 = right;	segs[ 0 ].y2 = Y;
        segs[ 1 ].x1 = right;	segs[ 1 ].y1 = Y; 
		segs[ 1 ].x2 = right;	segs[ 1 ].y2 = bot;
        segs[ 2 ].x1 = right;	segs[ 2 ].y1 = bot; 
		segs[ 2 ].x2 = X;		segs[ 2 ].y2 = bot;
        segs[ 3 ].x1 = X;		segs[ 3 ].y1 = bot; 
		segs[ 3 ].x2 = X;		segs[ 3 ].y2 = Y;
        nSegs = 4;
    }    

	fl_color(mCursorColor);
    for ( int k = 0; k < nSegs; k++ ) {		
        fl_line( segs[ k ].x1, segs[ k ].y1, segs[ k ].x2, segs[ k ].y2 );
    }
}

/**
 * Determine the drawing method to use to draw a specific character from "buf".
 * "lineStartPos" gives the character index where the line begins, "lineIndex",
 * the number of characters past the beginning of the line, and "dispIndex",
 * the number of displayed characters past the beginning of the line.  Passing
 * lineStartPos of -1 returns the drawing style for "no text".
 *
 * Why not just: position_style(pos)?  Because style applies to blank areas
 * of the window beyond the text boundaries, and because this routine must also
 * decide whether a position is inside of a rectangular Fl_Text_Selection, and do
 * so efficiently, without re-counting character positions from the start of the
 * line.
 *
 * Note that style is a somewhat incorrect name, drawing method would
 * be more appropriate.
 */
int Fl_Text_Display::position_style(int lineStartPos, int lineLen, int lineIndex, int dispIndex) 
{
    Fl_Text_Buffer * buf = mBuffer;
    Fl_Text_Buffer *styleBuf = mStyleBuffer;
    int pos, style = 0;

    if ( lineStartPos == -1 || buf == NULL )
        return FILL_MASK;

    pos = lineStartPos + min( lineIndex, lineLen );

    if ( lineIndex >= lineLen )
        style = FILL_MASK;
    else if ( styleBuf != NULL ) {
        style = ( unsigned char ) styleBuf->character( pos );
        if (style == mUnfinishedStyle) {
            // encountered "unfinished" style, trigger parsing
            (mUnfinishedHighlightCB)(this, pos, mHighlightCBArg);
            style = (unsigned char) styleBuf->character( pos);
        }
    }
    if (buf->primary_selection()->includes(pos, lineStartPos, dispIndex))
        style |= PRIMARY_MASK;
    if (buf->highlight_selection()->includes(pos, lineStartPos, dispIndex))
        style |= HIGHLIGHT_MASK;
    if (buf->secondary_selection()->includes(pos, lineStartPos, dispIndex))
        style |= SECONDARY_MASK;
    return style;
}

/**
 * Find the width of a string in the font of a particular style
 */
int Fl_Text_Display::string_width( const char *string, int length, int style ) 
{
    int mask = style & STYLE_LOOKUP_MASK;	    
    if (mask) 
	{
		int si = mask - 'A';
        
		if(si < 0)				si = 0;
        else if(si >= mNStyles) si = mNStyles - 1;

        Style_Table_Entry *style_entry = mStyleTable + si;

        if(style_entry->attr == ATTR_IMAGE && style_entry->image) {
			int iW=0;
            for(int n=0; n<length; n++) {
				iW += style_entry->image->width();
			}
            return iW;
		}

		fl_font(style_entry->font, style_entry->size);
    } else {
        fl_font(text_font(), text_size());
    }

    return (int)fl_width(string, length);
}

/**
 * Translate window coordinates to the nearest (insert cursor or character
 * cell) text position.  The parameter posType specifies how to interpret the
 * position: CURSOR_POS means translate the coordinates to the nearest cursor
 * position, and CHARACTER_POS means return the position of the character
 * closest to (X, Y).
 */
int Fl_Text_Display::xy_to_position(int X, int Y, int posType) 
{
    int charIndex, lineStart, lineLen, fontHeight;
    int charWidth, charLen, charStyle, visLineNum, xStep, outIndex;
    char expandedChar[ FL_TEXT_MAX_EXP_CHAR_LEN ];    

    /* Find the visible line number corresponding to the Y coordinate */
    fontHeight = mMaxsize;
    visLineNum = ( Y - text_area.y ) / fontHeight;
    if ( visLineNum < 0 )
        return mFirstChar;
    if ( visLineNum >= mNVisibleLines )
        visLineNum = mNVisibleLines - 1;

    /* Find the position at the start of the line */
    lineStart = mLineStarts[ visLineNum ];

    /* If the line start was empty, return the last position in the buffer */
    if ( lineStart == -1 )
        return mBuffer->length();

    /* Get the line text and its length */
    lineLen = vline_length( visLineNum );
	mBuffer->text_range(m_lineBuffer, lineStart, lineStart + lineLen);

    /* Step through character positions from the beginning of the line
     to find the character position corresponding to the X coordinate */
    xStep = text_area.x - mHorizOffset;
    outIndex = 0;
    for ( charIndex = 0; charIndex < lineLen; charIndex++ )
    {
        charLen = Fl_Text_Buffer::expand_character(m_lineBuffer[ charIndex ], outIndex, expandedChar, mBuffer->tab_distance());
#if HAVE_XUTF8
        if (charLen > 1 && (m_lineBuffer[ charIndex ] & 0x80)) {
            int i, ii = 0;;
            i = fl_utf_charlen(m_lineBuffer[ charIndex ]);
            while (i > 1) {
                i--;
                ii++;
                expandedChar[ii] = m_lineBuffer[ charIndex + ii];
            }
        }
#endif
        charStyle = position_style( lineStart, lineLen, charIndex, outIndex );
        charWidth = string_width( expandedChar, charLen, charStyle );
        if ( X < xStep + ( posType == CURSOR_POS ? charWidth / 2 : charWidth ) ) {
            return lineStart + charIndex;
        }
        xStep += charWidth;
        outIndex += charLen;
    }

    /* If the X position was beyond the end of the line, return the position
       of the newline at the end of the line */
    return lineStart + lineLen;
}

/**
 * Translate window coordinates to the nearest row and column number for
 * positioning the cursor.  This, of course, makes no sense when the font is
 * proportional, since there are no absolute columns.  The parameter posType
 * specifies how to interpret the position: CURSOR_POS means translate the
 * coordinates to the nearest position between characters, and CHARACTER_POS
 * means translate the position to the nearest character cell.
 */
void Fl_Text_Display::xy_to_rowcol( int X, int Y, int *row, int *column, int posType ) 
{
    int fontHeight = mMaxsize;
    int fontWidth = mMaxFontBound;//TMPFONTWIDTH; //mFontStruct->max_bounds.width;

    /* Find the visible line number corresponding to the Y coordinate */
    *row = ( Y - text_area.y ) / fontHeight;
    if ( *row < 0 ) * row = 0;
    if ( *row >= mNVisibleLines ) * row = mNVisibleLines - 1;
    *column = ( ( X - text_area.x ) + mHorizOffset +
            ( posType == CURSOR_POS ? fontWidth / 2 : 0 ) ) / fontWidth;
    if ( *column < 0 ) * column = 0;
}

/**
 * Offset the line starts array, topLineNum, firstChar and lastChar, for a new
 * vertical scroll position given by newTopLineNum.  If any currently displayed
 * lines will still be visible, salvage the line starts values, otherwise,
 * count lines from the nearest known line start (start or end of buffer, or
 * the closest value in the lineStarts array)
 */
void Fl_Text_Display::offset_line_starts( int newTopLineNum ) 
{
    int oldTopLineNum	= mTopLineNum;
    int oldFirstChar	= mFirstChar;

    int lineDelta	= newTopLineNum - oldTopLineNum;
    int *lineStarts	= (int*)mLineStarts.data();    
    Fl_Text_Buffer *buf = mBuffer;

    /* If there was no offset, nothing needs to be changed */
    if ( lineDelta == 0 )
        return;

    /*{   int i;
    	printf("Scroll, lineDelta %d\n", lineDelta);
    	printf("lineStarts Before: ");
    	for(i=0; i<mNVisibleLines; i++) printf("%d ", lineStarts[i]);
    	printf("\n");
    }*/

    /* Find the new value for firstChar by counting lines from the nearest
     known line start (start or end of buffer, or the closest value in the
     lineStarts array) */
    int lastLineNum = oldTopLineNum + mNVisibleLines - 1;

    if ( newTopLineNum < oldTopLineNum && newTopLineNum < -lineDelta ) {
        mFirstChar = skip_lines( 0, newTopLineNum - 1, true );
		//printf("counting forward %d lines from start\n", newTopLineNum-1);
    } 
	else if ( newTopLineNum < oldTopLineNum ) {
        mFirstChar = rewind_lines( mFirstChar, -lineDelta );
		//printf("counting backward %d lines from firstChar\n", -lineDelta);
    } 
	else if ( newTopLineNum < lastLineNum ) {
        mFirstChar = lineStarts[ newTopLineNum - oldTopLineNum ];
		//printf("taking new start from lineStarts[%d]\n", newTopLineNum - oldTopLineNum);
    }
	else if ( newTopLineNum - lastLineNum < mNBufferLines - newTopLineNum ) {
        mFirstChar = skip_lines( lineStarts[ mNVisibleLines - 1 ], newTopLineNum - lastLineNum, true );
		//printf("counting forward %d lines from start of last line\n", newTopLineNum - lastLineNum); 
    } else {
        mFirstChar = rewind_lines( buf->length(), mNBufferLines - newTopLineNum + 1 );
		//printf("counting backward %d lines from end\n", mNBufferLines - newTopLineNum + 1); 
    }

#if 0
	mFirstChar = rewind_lines( buf->length(), mNBufferLines - newTopLineNum + 1 );
	calc_line_starts( 0, nVisLines );
#else
    /* Fill in the line starts array */
    if ( lineDelta < 0 && -lineDelta < mNVisibleLines ) {
        for ( int i = mNVisibleLines - 1; i >= -lineDelta; i-- )
            lineStarts[ i ] = lineStarts[ i + lineDelta ];
        calc_line_starts( 0, -lineDelta );
    } 
	else if ( lineDelta > 0 && lineDelta < mNVisibleLines ) {
        for ( int i = 0; i < mNVisibleLines - lineDelta; i++ )
			lineStarts[ i ] = lineStarts[ i + lineDelta ];
        calc_line_starts( mNVisibleLines - lineDelta, mNVisibleLines - 1 );
    } 
	else
        calc_line_starts( 0, mNVisibleLines );
#endif

    /* Set lastChar and topLineNum */
    calc_last_char();
    mTopLineNum = newTopLineNum;

    /* If we're numbering lines or being asked to maintain an absolute line
       number, re-calculate the absolute line number */
    absolute_top_line_number(oldFirstChar);

    /*{   int i;
    	printf("lineStarts After: ");
    	for(i=0; i<mNVisibleLines; i++) printf("%d ", lineStarts[i]);
    	printf("\n");
    }*/
}

/**
 * Update the line starts array, topLineNum, firstChar and lastChar for text
 * display "textD" after a modification to the text buffer, given by the
 * position where the change began "pos", and the numbers of characters
 * and lines inserted and deleted.
 */
void Fl_Text_Display::update_line_starts(
						int pos, int charsInserted,
						int charsDeleted, int linesInserted, 
						int linesDeleted, int *scrolled)
{
    int *lineStarts = (int*)mLineStarts.data();
    int i, lineOfPos, lineOfEnd, nVisLines = mNVisibleLines;
    int charDelta = charsInserted - charsDeleted;
    int lineDelta = linesInserted - linesDeleted;

    /* If all of the changes were before the displayed text, the display
     doesn't change, just update the top line num and offset the line
     start entries and first and last characters */
    if ( pos + charsDeleted < mFirstChar ) {
        mTopLineNum += lineDelta;
        //for ( i = 0; i < nVisLines; i++ )
        for ( i = 0; i < nVisLines && lineStarts[i] != -1; i++ )
            lineStarts[ i ] += charDelta;
        mFirstChar += charDelta;
        mLastChar += charDelta;
        *scrolled = 0;
        return;
    }

    /* The change began before the beginning of the displayed text, but
     part or all of the displayed text was deleted */
    if ( pos < mFirstChar ) {
        /* If some text remains in the window, anchor on that  */
        if ( position_to_line( pos + charsDeleted, &lineOfEnd ) &&
                ++lineOfEnd < nVisLines && lineStarts[ lineOfEnd ] != -1 ) {
            mTopLineNum = max( 1, mTopLineNum + lineDelta );

            //mFirstChar = buffer()->rewind_lines( lineStarts[ lineOfEnd ] + charDelta, lineOfEnd );
            mFirstChar = rewind_lines( lineStarts[ lineOfEnd ] + charDelta, lineOfEnd );

            /* Otherwise anchor on original line number and recount everything */
        } else {
            if ( mTopLineNum > mNBufferLines + lineDelta ) {
                mTopLineNum = 1;
                mFirstChar = 0;
            } else
                mFirstChar = skip_lines( 0, mTopLineNum - 1, true );
            //mFirstChar = buffer()->skip_lines( 0, mTopLineNum - 1 );
        }
        calc_line_starts( 0, nVisLines - 1 );
        /* calculate lastChar by finding the end of the last displayed line */
        calc_last_char();
        *scrolled = 1;
        return;
    }

    /* If the change was in the middle of the displayed text (it usually is),
     salvage as much of the line starts array as possible by moving and
     offsetting the entries after the changed area, and re-counting the
     added lines or the lines beyond the salvaged part of the line starts
     array */
    if ( pos <= mLastChar ) {
        /* find line on which the change began */
        position_to_line( pos, &lineOfPos );
        /* salvage line starts after the changed area */
        if ( lineDelta == 0 ) {
            for ( i = lineOfPos + 1; i < nVisLines && lineStarts[ i ] != -1; i++ )
                lineStarts[ i ] += charDelta;
        } else if ( lineDelta > 0 ) {
            for ( i = nVisLines - 1; i >= lineOfPos + lineDelta + 1; i-- )
                lineStarts[ i ] = lineStarts[ i - lineDelta ] +
                    ( lineStarts[ i - lineDelta ] == -1 ? 0 : charDelta );
        } else /* (lineDelta < 0) */ {
            for ( i = max( 0, lineOfPos + 1 ); i < nVisLines + lineDelta; i++ )
                lineStarts[ i ] = lineStarts[ i - lineDelta ] +
                    ( lineStarts[ i - lineDelta ] == -1 ? 0 : charDelta );
        }
        /* fill in the missing line starts */
        if ( linesInserted >= 0 )
            calc_line_starts( lineOfPos + 1, lineOfPos + linesInserted );
        if ( lineDelta < 0 )
            calc_line_starts( nVisLines + lineDelta, nVisLines );
        /* calculate lastChar by finding the end of the last displayed line */
        calc_last_char();
        *scrolled = 0;
        return;
    }

    /* Change was past the end of the displayed text, but displayable by virtue
     of being an insert at the end of the buffer into visible blank lines */
    if ( empty_vlines() ) {
        position_to_line( pos, &lineOfPos );
        calc_line_starts( lineOfPos, lineOfPos + linesInserted );
        calc_last_char();
        *scrolled = 0;
        return;
    }

    /* Change was beyond the end of the buffer and not visible, do nothing */
    *scrolled = 0;
}

/**
 * Scan through the text in the "textD"'s buffer and recalculate the line
 * starts array values beginning at index "startLine" and continuing through
 * (including) "endLine".  It assumes that the line starts entry preceding
 * "startLine" (or mFirstChar if startLine is 0) is good, and re-counts
 * newlines to fill in the requested entries.  Out of range values for
 * "startLine" and "endLine" are acceptable.
 */
void Fl_Text_Display::calc_line_starts( int startLine, int endLine ) 
{
    int startPos, bufLen = mBuffer->length();
    int line, lineEnd, nextLineStart, nVis = mNVisibleLines;
    int *lineStarts = (int*)mLineStarts.data();

	/* Clean up (possibly) messy input parameters */
    if (nVis == 0)			return;
    if (endLine < 0)		endLine = 0;
    if (endLine >= nVis)	endLine = nVis - 1;
    if (startLine < 0)		startLine = 0;
    if (startLine >= nVis)	startLine = nVis - 1;
    if (startLine > endLine)
    	return;

	/* Find the last known good line number -> position mapping */
    if ( startLine == 0 ) {
        lineStarts[ 0 ] = mFirstChar;
        startLine = 1;
    }
    startPos = lineStarts[ startLine - 1 ];

	/* If the starting position is already past the end of the text,
     fill in -1's (means no text on line) and return */
    if ( startPos == -1 ) {
        for ( line = startLine; line <= endLine; line++ )
            lineStarts[ line ] = -1;
        return;
    }

	/* Loop searching for ends of lines and storing the positions of the
     start of the next line in lineStarts */
    for ( line = startLine; line <= endLine; line++ ) 
    {
        find_line_end(startPos, true, &lineEnd, &nextLineStart);
		//nextLineStart = min(buffer()->length(), lineEnd + 1);
        startPos = nextLineStart;     

        if ( startPos >= bufLen ) {
          /* If the buffer ends with a newline or line break, put
           buf->length() in the next line start position (instead of
           a -1 which is the normal marker for an empty line) to
           indicate that the cursor may safely be displayed there */
            if ( line == 0 || ( lineStarts[ line - 1 ] != bufLen &&
                        lineEnd != nextLineStart ) ) {
                lineStarts[ line ] = bufLen;
                line++;
            }
            break;
        }
        lineStarts[ line ] = startPos;    
    }

	/* Set any entries beyond the end of the text to -1 */
    for ( ; line <= endLine; line++ )
        lineStarts[ line ] = -1;	
}

/**
 * Given a Fl_Text_Display with a complete, up-to-date lineStarts array, update
 * the lastChar entry to point to the last buffer position displayed.
 */
void Fl_Text_Display::calc_last_char() 
{
    int i;
    int vis_lines = mNVisibleLines - 1;
    for (i = vis_lines; i >= 0 && mLineStarts[i] == -1; i--) 
		;
    mLastChar = i < 0 ? 0 : line_end(mLineStarts[i], true);
}

void Fl_Text_Display::scroll(int topLineNum, int horizOffset)
{
	do_scroll(topLineNum, horizOffset);

	calc_longest_vline();

    update_v_scrollbar();
    update_h_scrollbar();
}

void Fl_Text_Display::do_scroll(int topLineNum, int horizOffset)
{
	/* Do nothing if scroll position hasn't actually changed or there's no
	   window to draw in yet */
    if(!visible_r() || (mHorizOffset == horizOffset && mTopLineNum == topLineNum)) {
        return;
    }	

	/* Limit the requested scroll position to allowable values */
	if (topLineNum > (mNBufferLines - mNVisibleLines + 2))
		topLineNum = (mNBufferLines - mNVisibleLines + 2);	

	if (topLineNum < 1)
		topLineNum = 1;

	if(horizOffset < 0)
		horizOffset = 0;
	
	if(mHorizOffset != horizOffset) {
		//printf("do_scroll HORIZOFFSET %d %d\n", mHorizOffset, horizOffset);
		int dx = mHorizOffset - horizOffset;
		mHorizOffset = horizOffset; 
		scrolldx += dx; 
	}

	if(mTopLineNum != topLineNum) {
		//printf("do_scroll TOPLINE %d %d\n", mTopLineNum, topLineNum);

		int dy = mTopLineNum - topLineNum;
		scrolldy += (dy*mMaxsize); 

		// If the vertical scroll position has changed, update the line
		// starts array and related counters in the text display
		offset_line_starts(topLineNum);

		calc_longest_vline();
		
		if(!mContinuousWrap) {
			// in case horizontal offset is now greater than longest line
			int maxhoffset = max(0, mLongestVline - text_area.w);
			if (mHorizOffset > maxhoffset) {
				relayout();
			}		
		}

		update_h_scrollbar();
	}

	redraw(FL_DAMAGE_SCROLL);
}

/**
 * Update the minimum, maximum, slider size, page increment, and value
 * for vertical scroll bar.
 */
void Fl_Text_Display::update_v_scrollbar() 
{
	/* The Vert. scroll bar value and slider size directly represent the top
	   line number, and the number of visible lines respectively.  The scroll
	   bar maximum value is chosen to generally represent the size of the whole
	   buffer, with minor adjustments to keep the scroll bar widget happy 
	*/
	
	if(mNBufferLines < mNVisibleLines) {
		mVScrollBar->slider_size(0);
		mVScrollBar->deactivate();
	} else {
		mVScrollBar->value(mTopLineNum, mNVisibleLines, 1, mNBufferLines+1);	
		mVScrollBar->activate();
	}
}

/**
 * Update the minimum, maximum, slider size, page increment, and value
 * for the horizontal scroll bar.
 */
void Fl_Text_Display::update_h_scrollbar() 
{
    int sliderMax = max(mLongestVline, text_area.w + mHorizOffset);
	if(mLongestVline<text_area.w) {
		mHScrollBar->slider_size(0);	
		mHScrollBar->deactivate();
	} else {
		mHScrollBar->activate();
		mHScrollBar->value( mHorizOffset, text_area.w, 0, sliderMax );
	}
}

/**
 * Callbacks for drag or valueChanged on scroll bars
 */
void Fl_Text_Display::v_scrollbar_cb(Fl_Scrollbar* b, Fl_Text_Display* textD) 
{
    if (b->value() == textD->mTopLineNum) return;
    textD->do_scroll(b->value(), textD->mHorizOffset);
}

void Fl_Text_Display::h_scrollbar_cb(Fl_Scrollbar* b, Fl_Text_Display* textD) 
{
    if (b->value() == textD->mHorizOffset) return;
    textD->do_scroll(textD->mTopLineNum, b->value());
}

/*
** Count the number of newlines in a null-terminated text string;
*/
static int countlines( const char *string ) {
    if(!string) return 0;
    const char * c;
    int lineCount = 0;

    for ( c = string; *c != '\0'; c++ )
        if ( *c == '\n' ) lineCount++;
    return lineCount;
}

/*
** Return the width in pixels of the displayed line pointed to by "visLineNum"
*/
#if 0
// Old method from fltk2.0
int Fl_Text_Display::measure_vline( int visLineNum ) {
    int i, width = 0, len, style, lineLen = vline_length( visLineNum );
    int charCount = 0, lineStartPos = mLineStarts[ visLineNum ];
    char expandedChar[ FL_TEXT_MAX_EXP_CHAR_LEN ];

    if ( mStyleBuffer == NULL ) {
        for ( i = 0; i < lineLen; i++ ) {
            len = mBuffer->expand_character( lineStartPos + i,
                    charCount, expandedChar );

            fl_font( text_font(), text_size() );

            width += ( int ) fl_width( expandedChar, len );

            charCount += len;
        }
    } else {
        for ( i = 0; i < lineLen; i++ ) {
            len = mBuffer->expand_character( lineStartPos + i,
                    charCount, expandedChar );
            style = ( unsigned char ) mStyleBuffer->character(
                    lineStartPos + i ) - 'A';

            fl_font( mStyleTable[ style ].font, mStyleTable[ style ].size );

            width += ( int ) fl_width( expandedChar, len );

            charCount += len;
        }
    }
    return width;
}
#else
int Fl_Text_Display::measure_vline( int visLineNum )
{
	if(mLineStarts[ visLineNum ]<0) return 0;

    int i, width = 0, charlen, charCount = 0;
    int lineLen = vline_length( visLineNum );
    int lineStartPos = mLineStarts[ visLineNum ];
    char expandedChar[ FL_TEXT_MAX_EXP_CHAR_LEN ];

    char buffer[4096];
    char *bufptr = buffer;
    int bufpos = 0;

    int last_style = -1, style = -1;

    Fl_Font font = text_font();
    int size = text_size();

    for(i = 0; i < lineLen; i++ )
    {
        charlen = mBuffer->expand_character( lineStartPos + i,
                charCount, expandedChar );

        if(mStyleBuffer) {
            style = ( unsigned char ) mStyleBuffer->character(lineStartPos + i) - 'A';
            if(last_style==-1) last_style=style;
            font = mStyleTable[ style ].font;
            size = mStyleTable[ style ].size;
        }

        if(mStyleBuffer && style!=last_style && (font!=fl_font() || size!=int(fl_size())) ) {
            fl_font(font, size);
            width += int(fl_width( buffer, bufpos ));
            bufpos = 0;
        }

        if( unsigned(bufpos+charlen) >= sizeof(buffer)) {
            fl_font(font, size);
            width += int(fl_width( buffer, bufpos ));
            bufpos = 0;
        }

        if(charlen==1) bufptr[bufpos] = expandedChar[0];
        else strncpy(bufptr+bufpos, expandedChar, charlen);

        charCount += charlen;
        bufpos += charlen;

        last_style = style;
    }

    if(bufpos) {
        fl_font(font, size);
        width += int(fl_width( buffer, bufpos ));
    }
    return width;
}
#endif

/**
 * Return true if there are lines visible with no corresponding buffer text
 */
int Fl_Text_Display::empty_vlines() {
    return mNVisibleLines > 0 && mLineStarts[ mNVisibleLines - 1 ] == -1;
}

/**
 * Return the length of a line (number of displayable characters) by examining
 * entries in the line starts array rather than by scanning for newlines
 */
int Fl_Text_Display::vline_length( int visLineNum ) 
{
    int nextLineStart, lineStartPos = mLineStarts[ visLineNum ];

    if ( lineStartPos == -1 )
        return 0;
    if ( visLineNum + 1 >= mNVisibleLines )
        return mLastChar - lineStartPos;
    nextLineStart = mLineStarts[ visLineNum + 1 ];

    if ( nextLineStart == -1 )
        return mLastChar - lineStartPos;

    if (wrap_uses_character(nextLineStart-1))
        return nextLineStart - 1 - lineStartPos;

    return nextLineStart - lineStartPos;
}

/**
 * When continuous wrap is on, and the user inserts or deletes characters, 
 * wrapping can happen before and beyond the changed position.  This routine 
 * finds the extent of the changes, and counts the deleted and inserted lines 
 * over that range. It also attempts to minimize the size of the range to 
 * what has to be counted and re-displayed, so the results can be useful 
 * both for delimiting where the line starts need to be recalculated, and 
 * for deciding what part of the text to redisplay. 
 */ 
void Fl_Text_Display::find_wrap_range(const char *deletedText, int pos, 
    int nInserted, int nDeleted, int *modRangeStart, int *modRangeEnd, 
    int *linesInserted, int *linesDeleted) 
{
    int length, retPos, retLines, retLineStart, retLineEnd;
    Fl_Text_Buffer *deletedTextBuf, *buf = buffer();
    int nVisLines = mNVisibleLines;
    int *lineStarts = (int*)mLineStarts.data();
    int countFrom, countTo, lineStart, adjLineStart, i;
    int visLineNum = 0, nLines = 0;

    /*
     Determine where to begin searching: either the previous newline, or
     if possible, limit to the start of the (original) previous displayed
     line, using information from the existing line starts array
     */
    if (pos >= mFirstChar && pos <= mLastChar) {
        for (i=nVisLines-1; i>0; i--)
            if (lineStarts[i] != -1 && pos >= lineStarts[i])
                break;
        if (i > 0) {
            countFrom = lineStarts[i-1];
            visLineNum = i-1;
        } else
            countFrom = buf->line_start(pos);
    } else
        countFrom = buf->line_start(pos);

    /*
     Move forward through the (new) text one line at a time, counting
     displayed lines, and looking for either a real newline, or for the
     line starts to re-sync with the original line starts array
     */
    lineStart = countFrom;
    *modRangeStart = countFrom;
    while (true) {

        /* advance to the next line.  If the line ended in a real newline
         or the end of the buffer, that's far enough */
        wrapped_line_counter(buf, lineStart, buf->length(), 1, true, 0,
							&retPos, &retLines, &retLineStart, &retLineEnd);
        if (retPos >= buf->length()) {
            countTo = buf->length();
            *modRangeEnd = countTo;
            if (retPos != retLineEnd)
                nLines++;
            break;
        } else
            lineStart = retPos;
        nLines++;
        if (lineStart > pos + nInserted &&
                buf->character(lineStart-1) == '\n') {
            countTo = lineStart;
            *modRangeEnd = lineStart;
            break;
        }

        /* Don't try to resync in continuous wrap mode with non-fixed font
         sizes; it would result in a chicken-and-egg dependency between
         the calculations for the inserted and the deleted lines.
         If we're in that mode, the number of deleted lines is calculated in
         advance, without resynchronization, so we shouldn't resynchronize
         for the inserted lines either. */
        if (mSuppressResync)
            continue;

        /* check for synchronization with the original line starts array
         before pos, if so, the modified range can begin later */
        if (lineStart <= pos) {
            while (visLineNum<nVisLines && lineStarts[visLineNum] < lineStart)
                visLineNum++;
            if (visLineNum < nVisLines && lineStarts[visLineNum] == lineStart) {
                countFrom = lineStart;
                nLines = 0;
                if (visLineNum+1 < nVisLines && lineStarts[visLineNum+1] != -1)
                    *modRangeStart = min(pos, lineStarts[visLineNum+1]-1);
                else
                    *modRangeStart = countFrom;
            } else
                *modRangeStart = min(*modRangeStart, lineStart-1);
        }

        /* check for synchronization with the original line starts array
         after pos, if so, the modified range can end early */
        else if (lineStart > pos + nInserted) {
            adjLineStart = lineStart - nInserted + nDeleted;
            while (visLineNum<nVisLines && lineStarts[visLineNum]<adjLineStart)
                visLineNum++;
            if (visLineNum < nVisLines && lineStarts[visLineNum] != -1 &&
                    lineStarts[visLineNum] == adjLineStart) {
                countTo = line_end(lineStart, true);
                *modRangeEnd = lineStart;
                break;
            }
        }
    }
    *linesInserted = nLines;


    /* Count deleted lines between countFrom and countTo as the text existed
     before the modification (that is, as if the text between pos and
     pos+nInserted were replaced by "deletedText").  This extra context is
     necessary because wrapping can occur outside of the modified region
     as a result of adding or deleting text in the region. This is done by
     creating a textBuffer containing the deleted text and the necessary
     additional context, and calling the wrappedLineCounter on it.

    NOTE: This must not be done in continuous wrap mode when the font
    width is not fixed. In that case, the calculation would try
    to access style information that is no longer available (deleted
    text), or out of date (updated highlighting), possibly leading
    to completely wrong calculations and/or even crashes eventually.
    (This is not theoretical; it really happened.)

    In that case, the calculation of the number of deleted lines
    has happened before the buffer was modified (only in that case,
    because resynchronization of the line starts is impossible
    in that case, which makes the whole calculation less efficient).
    */
    if (mSuppressResync) {
        *linesDeleted = mNLinesDeleted;
        mSuppressResync = 0;
        return;
    }

    length = (pos-countFrom) + nDeleted +(countTo-(pos+nInserted));
    deletedTextBuf = new Fl_Text_Buffer(length);
	if (pos > countFrom)        
		deletedTextBuf->copy(buffer(), countFrom, pos, 0);
    if (nDeleted != 0)
        deletedTextBuf->insert(pos-countFrom, deletedText);
    if (countTo > pos+nInserted)    
		deletedTextBuf->copy(buffer(), pos+nInserted, countTo, pos-countFrom+nDeleted);
    /* Note that we need to take into account an offset for the style buffer:
     the deletedTextBuf can be out of sync with the style buffer. */
    wrapped_line_counter(deletedTextBuf, 0, length, INT_MAX, true,
						 countFrom, &retPos, &retLines, &retLineStart, 
						 &retLineEnd, false);
    delete deletedTextBuf;
    *linesDeleted = retLines;
    mSuppressResync = 0;
}

/**
 * This is a stripped-down version of the findWrapRange() function above, 
 * intended to be used to calculate the number of "deleted" lines during 
 * a buffer modification. It is called _before_ the modification takes place. 
 * 
 * This function should only be called in continuous wrap mode with a 
 * non-fixed font width. In that case, it is impossible to calculate 
 * the number of deleted lines, because the necessary style information 
 * is no longer available _after_ the modification. In other cases, we 
 * can still perform the calculation afterwards (possibly even more 
 * efficiently). 
 */ 
void Fl_Text_Display::measure_deleted_lines(int pos, int nDeleted) 
{ 
    int retPos, retLines, retLineStart, retLineEnd;
    Fl_Text_Buffer *buf = buffer();
    int nVisLines = mNVisibleLines;
    int *lineStarts = (int*)mLineStarts.data();
    int countFrom, lineStart;
    int visLineNum = 0, nLines = 0, i;
    /*
     Determine where to begin searching: either the previous newline, or
     if possible, limit to the start of the (original) previous displayed
     line, using information from the existing line starts array
     */
    if (pos >= mFirstChar && pos <= mLastChar) {
        for (i=nVisLines-1; i>0; i--)
            if (lineStarts[i] != -1 && pos >= lineStarts[i])
                break;
        if (i > 0) {
            countFrom = lineStarts[i-1];
            visLineNum = i-1;
        } else
            countFrom = buf->line_start(pos);
    } else
        countFrom = buf->line_start(pos);

    /*
      Move forward through the (new) text one line at a time, counting
      displayed lines, and looking for either a real newline, or for the
      line starts to re-sync with the original line starts array
     */
    lineStart = countFrom;
    while (true) {
        /* advance to the next line.  If the line ended in a real newline
         or the end of the buffer, that's far enough */
        wrapped_line_counter(buf, lineStart, buf->length(), 1, true, 0,
            &retPos, &retLines, &retLineStart, &retLineEnd);
        if (retPos >= buf->length()) {
            if (retPos != retLineEnd)
                nLines++;
            break;
        } else
            lineStart = retPos;
        nLines++;
        if (lineStart > pos + nDeleted &&
                buf->character(lineStart-1) == '\n') {
            break;
        }

        /* Unlike in the findWrapRange() function above, we don't try to
         resync with the line starts, because we don't know the length
         of the inserted text yet, nor the updated style information.

         Because of that, we also shouldn't resync with the line starts
         after the modification either, because we must perform the
         calculations for the deleted and inserted lines in the same way.

         This can result in some unnecessary recalculation and redrawing
         overhead, and therefore we should only use this two-phase mode
         of calculation when it's really needed (continuous wrap + variable
         font width). */
    }
    mNLinesDeleted = nLines;
    mSuppressResync = 1;
}

/**
 * Count forward from startPos to either maxPos or maxLines (whichever is 
 * reached first), and return all relevant positions and line count. 
 * The provided textBuffer may differ from the actual text buffer of the 
 * widget. In that case it must be a (partial) copy of the actual text buffer 
 * and the styleBufOffset argument must indicate the starting position of the 
 * copy, to take into account the correct style information. 
 * 
 * Returned values: 
 * 
 *   retPos:        Position where counting ended.  When counting lines, the 
 *                  position returned is the start of the line "maxLines" 
 *                  lines beyond "startPos". 
 *   retLines:      Number of line breaks counted 
 *   retLineStart:  Start of the line where counting ended 
 *   retLineEnd:    End position of the last line traversed 
 */ 
void Fl_Text_Display::wrapped_line_counter(
						Fl_Text_Buffer *buf, int startPos,
						int maxPos, int maxLines, bool startPosIsLineStart, int styleBufOffset,
						int *retPos, int *retLines, int *retLineStart, int *retLineEnd,
						bool countLastLineMissingNewLine)
{
	int lineStart, newLineStart = 0, b, p, colNum, wrapMargin;
    int maxWidth, width, countPixels, i, foundBreak;
    int nLines = 0, tabDist = buffer()->tab_distance();
    unsigned char c;
    
    /* If the font is fixed, or there's a wrap margin set, it's more efficient
       to measure in columns, than to count pixels.  Determine if we can count
       in columns (countPixels == False) or must count pixels (countPixels ==
       True), and set the wrap target for either pixels or columns */
    if (mFixedFontWidth != -1 || mWrapMargin != 0) {
    	countPixels = false;
		wrapMargin =	mWrapMargin != 0 ? mWrapMargin :
            			text_area.w / mFixedFontWidth;
        maxWidth = INT_MAX;
    } else {
    	countPixels = true;
    	wrapMargin = INT_MAX;
    	maxWidth = text_area.w;
    }
    
    /* Find the start of the line if the start pos is not marked as a
       line start. */
    if (startPosIsLineStart)
		lineStart = startPos;
    else
		lineStart = line_start(startPos);
    
    /*
    ** Loop until position exceeds maxPos or line count exceeds maxLines.
    ** (actually, contines beyond maxPos to end of line containing maxPos,
    ** in case later characters cause a word wrap back before maxPos)
    */
    colNum = 0;
    width = 0;
    for (p=lineStart; p<buf->length(); p++) {
    	c = buf->character(p);//BufGetCharacter(buf, p);

    	/* If the character was a newline, count the line and start over,
    	   otherwise, add it to the width and column counts */
    	if (c == '\n') {			
    	    if (p >= maxPos) {
    			*retPos = maxPos;
    			*retLines = nLines;
    			*retLineStart = lineStart;
    			*retLineEnd = maxPos;
    			return;
    	    }
			nLines++;
    	    if (nLines >= maxLines) {
    			*retPos = p + 1;
    			*retLines = nLines;
    			*retLineStart = p + 1;
    			*retLineEnd = p;
    			return;
    	    }
    	    lineStart = p + 1;
    	    colNum = 0;
    	    width = 0;
    	} else {
    	    colNum += Fl_Text_Buffer::character_width(c, colNum, tabDist);
				//buf->character_width(c, colNum, tabDist, nullSubsChar);
    	    if (countPixels)
    	    	width += measure_proportional_character(c, colNum, p+styleBufOffset);				
    	}

    	/* If character exceeded wrap margin, find the break point
    	   and wrap there */
    	if (colNum > wrapMargin || width > maxWidth) {
    	    foundBreak = false;
    	    for (b=p; b>=lineStart; b--) {
    	    	c = buf->character(b);//BufGetCharacter(buf, b);
    	    	if (c == '\t' || c == ' ') {
    	    	    newLineStart = b + 1;
    	    	    if (countPixels) {
    	    	    	colNum = 0;
    	    	    	width = 0;
    	    	    	for (i=b+1; i<p+1; i++) {
    	    	    	    width += measure_proportional_character(buf->character(i), colNum, i+styleBufOffset);
    	    	    	    colNum++;
    	    	    	}
    	    	    } else
    	    	    	colNum = buf->count_displayed_characters(b+1, p+1);
    	    		foundBreak = true;
    	    	    break;
    	    	}
    	    }
    	    if (!foundBreak) { /* no whitespace, just break at margin */
    	    	newLineStart = max(p, lineStart+1);
				colNum = Fl_Text_Buffer::character_width(c, colNum, tabDist);//, nullSubsChar);
    	    	if (countPixels)
   	    	    width = measure_proportional_character(c, colNum, p+styleBufOffset);
    	    }
    	    if (p >= maxPos) {
    			*retPos = maxPos;
    			*retLines = maxPos < newLineStart ? nLines : nLines + 1;
    			*retLineStart = maxPos < newLineStart ? lineStart : newLineStart;
    			*retLineEnd = maxPos;
    			return;
    	    }
    	    nLines++;
    	    if (nLines >= maxLines) {
    			*retPos = foundBreak ? b + 1 : max(p, lineStart+1);
    			*retLines = nLines;
    			*retLineStart = lineStart;
    			*retLineEnd = foundBreak ? b : p;
    			return;
    	    }
    	    lineStart = newLineStart;
    	}
    }

    /* reached end of buffer before reaching pos or line target */
    *retPos = buf->length();
    *retLines = nLines;
    *retLineStart = lineStart;
    *retLineEnd = buf->length();
}

/** 
 * Measure the width in pixels of a character "c" at a particular column 
 * "colNum" and buffer position "pos".  This is for measuring characters in 
 * proportional or mixed-width highlighting fonts. 
 * 
 * A note about proportional and mixed-width fonts: the mixed width and 
 * proportional font code in nedit does not get much use in general editing, 
 * because nedit doesn't allow per-language-mode fonts, and editing programs 
 * in a proportional font is usually a bad idea, so very few users would 
 * choose a proportional font as a default.  There are still probably mixed- 
 * width syntax highlighting cases where things don't redraw properly for 
 * insertion/deletion, though static display and wrapping and resizing 
 * should now be solid because they are now used for online help display. 
 */ 
int Fl_Text_Display::measure_proportional_character(char c, int colNum, int pos) 
{
    int charLen, style;
    char expChar[ FL_TEXT_MAX_EXP_CHAR_LEN ];
    Fl_Text_Buffer *styleBuf = mStyleBuffer;

    charLen = Fl_Text_Buffer::expand_character(c, colNum, expChar, buffer()->tab_distance());
    if (styleBuf == 0) {
        style = 0;
    } else {
        style = (unsigned char)styleBuf->character(pos);
        if (style == mUnfinishedStyle) {
            /* encountered "unfinished" style, trigger parsing */
            (mUnfinishedHighlightCB)(this, pos, mHighlightCBArg);
            style = (unsigned char)styleBuf->character(pos);
        }
    }   
    return string_width(expChar, charLen, style);
}

/**
 * Finds both the end of the current line and the start of the next line.  Why?
 * In continuous wrap mode, if you need to know both, figuring out one from the
 * other can be expensive or error prone.  The problem comes when there's a
 * trailing space or tab just before the end of the buffer.  To translate an
 * end of line value to or from the next lines start value, you need to know
 * whether the trailing space or tab is being used as a line break or just a
 * normal character, and to find that out would otherwise require counting all
 * the way back to the beginning of the line. 
 */ 
void Fl_Text_Display::find_line_end(int startPos, bool startPosIsLineStart, int *lineEnd, int *nextLineStart) 
{ 
    int retLines, retLineStart; 

   /* if we're not wrapping use more efficient BufEndOfLine */ 
    if (!mContinuousWrap) { 
        *lineEnd = buffer()->line_end(startPos); 
        *nextLineStart = min(buffer()->length(), *lineEnd + 1); 
        return; 
    } 

   /* use the wrapped line counter routine to count forward one line */ 
    wrapped_line_counter(buffer(), startPos, buffer()->length(), 
        1, startPosIsLineStart, 0, nextLineStart, &retLines, 
        &retLineStart, lineEnd); 
    return; 
} 

/**
 * Line breaks in continuous wrap mode usually happen at newlines or 
 * whitespace.  This line-terminating character is not included in line 
 * width measurements and has a special status as a non-visible character. 
 * However, lines with no whitespace are wrapped without the benefit of a 
 * line terminating character, and this distinction causes endless trouble 
 * with all of the text display code which was originally written without 
 * continuous wrap mode and always expects to wrap at a newline character. 
 * 
 * Given the position of the end of the line, as returned by TextDEndOfLine 
 * or BufEndOfLine, this returns true if there is a line terminating 
 * character, and false if there's not.  On the last character in the 
 * buffer, this function can't tell for certain whether a trailing space was 
 * used as a wrap point, and just guesses that it wasn't.  So if an exact 
 * accounting is necessary, don't use this function. 
 */ 
int Fl_Text_Display::wrap_uses_character(int lineEndPos) 
{ 
    char c; 

    if (!mContinuousWrap || lineEndPos == buffer()->length()) 
        return 1; 

    c = buffer()->character(lineEndPos); 
    return c == '\n' || ((c == '\t' || c == ' ') && 
            lineEndPos + 1 != buffer()->length()); 
} 

/**
 * Return true if the selection "sel" is rectangular, and touches a 
 * buffer position withing "rangeStart" to "rangeEnd" 
 */ 
int Fl_Text_Display::range_touches_selection(Fl_Text_Selection *sel, int rangeStart, int rangeEnd) 
{ 
    return sel->selected() && sel->rectangular() && sel->end() >= rangeStart && sel->start() <= rangeEnd; 
}

/**
 * Extend the range of a redraw request (from *start to *end) with additional
 * redraw requests resulting from changes to the attached style buffer (which
 * contains auxiliary information for coloring or styling text).
 */
void Fl_Text_Display::extend_range_for_styles( int *start, int *end ) 
{
    Fl_Text_Selection * sel = mStyleBuffer->primary_selection();
    int extended = 0;

  /* The peculiar protocol used here is that modifications to the style
     buffer are marked by selecting them with the buffer's primary Fl_Text_Selection.
     The style buffer is usually modified in response to a modify callback on
     the text buffer BEFORE Fl_Text_Display.c's modify callback, so that it can keep
     the style buffer in step with the text buffer.  The style-update
     callback can't just call for a redraw, because Fl_Text_Display hasn't processed
     the original text changes yet.  Anyhow, to minimize redrawing and to
     avoid the complexity of scheduling redraws later, this simple protocol
     tells the text display's buffer modify callback to extend it's redraw
     range to show the text color/and font changes as well. */
    if ( sel->selected() ) {
        if ( sel->start() < *start ) {
            *start = sel->start();
            extended = 1;
        }
        if ( sel->end() > *end ) {
            *end = sel->end();
            extended = 1;
        }
    }

  /* If the Fl_Text_Selection was extended due to a style change, and some of the
     fonts don't match in spacing, extend redraw area to end of line to
     redraw characters exposed by possible font size changes */
    if ( mFixedFontWidth == -1 && extended )
        * end = mBuffer->line_end( *end ) + 1;
}

void Fl_Text_Display::fl_scroll_cb(void* v,int X, int Y, int W, int H)
{
    ((Fl_Text_Display*)v)->draw_text(X,Y,W,H);
}

// The draw() method.  It tries to minimize what is draw as much as possible.
void Fl_Text_Display::draw() {

    // don't even try if there is no associated text buffer!
    if (!buffer()) { draw_box(); return; }

	fl_color(color());

	// left margin
	fl_rectf(text_area.x-LEFT_MARGIN, text_area.y-TOP_MARGIN,
		LEFT_MARGIN, text_area.h+TOP_MARGIN+BOTTOM_MARGIN);

	// right margin
	fl_rectf(text_area.x+text_area.w, text_area.y-TOP_MARGIN,
		RIGHT_MARGIN, text_area.h+TOP_MARGIN+BOTTOM_MARGIN);

    // draw the non-text, non-scrollbar areas.
    if (damage() & FL_DAMAGE_ALL) {

		// Make sure scrollbars are in sync.
		// - this is fast task
		calc_longest_vline();
		update_h_scrollbar();
		update_v_scrollbar();

        //printf("drawing all\n");

        // top margin
        fl_rectf(text_area.x, text_area.y-TOP_MARGIN,
            text_area.w, TOP_MARGIN);

        // bottom margin
        fl_rectf(text_area.x, text_area.y+text_area.h,
            text_area.w, BOTTOM_MARGIN);

        // draw the box()
        draw_frame();

        // draw that little box in the corner of the scrollbars
        if (mVScrollBar->visible() && mHScrollBar->visible()) {
            fl_color(parent()?parent()->color():color());
            fl_rectf(mVScrollBar->x(), mHScrollBar->y(),
                mVScrollBar->w(), mHScrollBar->h());
        }		
		draw_line_numbers();
	
		mCursorPosOld = -1;

    }	

	// draw all of the text
    if (damage() & FL_DAMAGE_ALL) {
        //puts("drawing all text");

		mCursorPosOld = -1;
		
		int X, Y, W, H;
        if(fl_clip_box(text_area.x, text_area.y, text_area.w, text_area.h, X, Y, W, H)) {
            // Draw text using the intersected clipping box...
            // (this sets the clipping internally)
            draw_text(X, Y, W, H);
        } else {
            // Draw the whole area...			
            draw_text(text_area.x, text_area.y, text_area.w, text_area.h);
        }
    }
	else if((damage() & FL_DAMAGE_SCROLL) && (scrolldx!=0 || scrolldy!=0)) {	
		//puts("scroll lines of text");	
	
		/////////////////////
		// TODO:
		// Fix fl_scroll - bug propably in draw_text!
#if 1
		mCursorPosOld = -1;
		draw_line_numbers();
		
		int X, Y, W, H;
        if(fl_clip_box(text_area.x, text_area.y, text_area.w, text_area.h, X, Y, W, H)) {
            // Draw text using the intersected clipping box...
            // (this sets the clipping internally)
            draw_text(X, Y, W, H);
        } else {
            // Draw the whole area...			
            draw_text(text_area.x, text_area.y, text_area.w, text_area.h);
        }

#else
		if(scrolldy!=0) {
			// Vertical scroll - move linenumbers also
			fl_scroll(text_area.x-LEFT_MARGIN-mLineNumWidth,
					  text_area.y,
					  text_area.w+LEFT_MARGIN+RIGHT_MARGIN+mLineNumWidth,
					  text_area.h, 
					  0, scrolldy, fl_scroll_cb, this);
		} else {
			// Horizontal scroll
			fl_scroll(text_area.x, text_area.y,
					  text_area.w, text_area.h, 
					  scrolldx, 0, fl_scroll_cb, this);	
		}
#endif
	} 
    else if(damage() & FL_DAMAGE_SCROLL) {
        // draw some lines of text
        //puts("draw some lines of text");

        fl_push_clip(text_area.x, text_area.y, text_area.w, text_area.h);
        //printf("drawing text from %d to %d\n", damage_range1_start, damage_range1_end);
        draw_range(damage_range1_start, damage_range1_end);
        if (damage_range2_end != -1) {
            //printf("drawing text from %d to %d\n", damage_range2_start, damage_range2_end);
            draw_range(damage_range2_start, damage_range2_end);
        }
        damage_range1_start = damage_range1_end = -1;
        damage_range2_start = damage_range2_end = -1;
        
		fl_pop_clip();
    }

    // draw the text cursor
    if (damage() & (FL_DAMAGE_ALL | FL_DAMAGE_SCROLL | FL_DAMAGE_VALUE)
		&& !buffer()->primary_selection()->selected() &&
        mCursorOn && Fl::focus() == this ) 
	{
        fl_push_clip(text_area.x-LEFT_MARGIN,
					 text_area.y,
					 text_area.w+LEFT_MARGIN+RIGHT_MARGIN,
					 text_area.h);

		clear_cursor();
		draw_cursor();

        fl_pop_clip();
    }

    // draw the scrollbars
    if (damage() & (FL_DAMAGE_ALL)) {
        mVScrollBar->set_damage(FL_DAMAGE_ALL);
        mHScrollBar->set_damage(FL_DAMAGE_ALL);
    }
    update_child(*mVScrollBar);
    update_child(*mHScrollBar);

	scrolldx = scrolldy = 0;
}

// this processes drag events due to mouse for Fl_Text_Display and
// also drags due to cursor movement with shift held down for
// Fl_Text_Editor
void fl_text_drag_me(int pos, Fl_Text_Display* d)
{
    if (d->dragType == Fl_Text_Display::DRAG_CHAR) {
        if (pos >= d->dragPos) {
            d->buffer()->select(d->dragPos, pos);
        } else {
            d->buffer()->select(pos, d->dragPos);
        }
        d->insert_position(pos);
    } else if (d->dragType == Fl_Text_Display::DRAG_WORD) {
        if (pos >= d->dragPos) {
            d->insert_position(d->word_end(pos));
            d->buffer()->select(d->word_start(d->dragPos), d->word_end(pos));
        } else {
            d->insert_position(d->word_start(pos));
            d->buffer()->select(d->word_start(pos), d->word_end(d->dragPos));
        }
    } else if (d->dragType == Fl_Text_Display::DRAG_LINE) {
        if (pos >= d->dragPos) {
            d->insert_position(d->buffer()->line_end(pos)+1);
            d->buffer()->select(d->buffer()->line_start(d->dragPos),
                d->buffer()->line_end(pos)+1);
        } else {
            d->insert_position(d->buffer()->line_start(pos));
            d->buffer()->select(d->buffer()->line_start(pos),
                d->buffer()->line_end(d->dragPos)+1);
        }
    }
}

int Fl_Text_Display::handle(int event)
{
    if (!buffer()) Fl_Widget::handle(event);

    switch (event)
    {
	case FL_FOCUS:
		show_cursor(mCursorOn); // redraws the cursor
		return 1;

	case FL_UNFOCUS:
		show_cursor(mCursorOn); // redraws the cursor
		return 1;

	case FL_PUSH: {
		// handle clicks in the scrollbars:
        if(!Fl::event_inside(text_area.x,text_area.y,text_area.w,text_area.h)) {
			return Fl_Group::handle(event);
		}

        //take_focus();
        if (Fl::event_state()&FL_SHIFT) return handle(FL_DRAG);
        dragging = 1;
        int pos = xy_to_position(Fl::event_x(), Fl::event_y(), CURSOR_POS);
        dragType = Fl::event_clicks();
        dragPos = pos;

        // See if maybe they are starting to do drag & drop:
        if (!dragType && in_selection(Fl::event_x(), Fl::event_y())) {
			dragType = -1;
            return 1;
		}
        if (dragType == DRAG_CHAR)
			buffer()->unselect();
		else if (dragType == DRAG_WORD)
			buffer()->select(word_start(pos), word_end(pos));
		else if (dragType == DRAG_LINE)
			buffer()->select(buffer()->line_start(pos), buffer()->line_end(pos)+1);

		if (buffer()->primary_selection()->selected())
			insert_position(buffer()->primary_selection()->end());
		else
			insert_position(pos);
        
		show_insert_position();
        return 1;
	}

    case FL_DRAG: {
		if (dragType < 0) {
			// possibly starting drag & drop
            // wait until we are pretty sure they are dragging:
            if (Fl::event_is_click()) return 1;
            dragType = 0;
            // drag:
            char* copy = (char*)buffer()->selection_text();
            if(*copy) {
				Fl::copy(copy, strlen(copy), false);
                free(copy);
                Fl::dnd();
                return 1;
			}
            free(copy);
		}

        int X = Fl::event_x(), Y = Fl::event_y(), pos;
        if (Y < text_area.y) {
			move_up();
            scroll(mTopLineNum - 1, mHorizOffset);
            pos = insert_position();
		} else if (Y >= text_area.y+text_area.h) {
			move_down();
            scroll(mTopLineNum + 1, mHorizOffset);
            pos = insert_position();
		} else {
			pos = xy_to_position(X, Y, CURSOR_POS);
		}
        fl_text_drag_me(pos, this);
        return 1;
	}

    case FL_RELEASE: {
        // handle clicks in the scrollbars:
		if(!Fl::event_inside(text_area.x,text_area.y,text_area.w,text_area.h))
			return Fl_Group::handle(event);

        // if they just clicked inside the selection, put the cursor there
		if (dragType < 0) {
			buffer()->unselect();
            insert_position(dragPos);
            dragType = 0;
		}
        // convert from WORD or LINE selection to CHAR
        if (insert_position() >= dragPos)
			dragPos = buffer()->primary_selection()->start();
		else
			dragPos = buffer()->primary_selection()->end();
		dragType = DRAG_CHAR;

		char* copy = (char*)buffer()->selection_text();
        if(*copy) Fl::copy(copy, strlen(copy), false);
        free(copy);
        return 1;
	}

	case FL_MOUSEWHEEL:
		return mVScrollBar->send(event);
#if 0
        // I shouldn't be using mNVisibleLines or mTopLineNum here in handle()
        // because the values for these might change between now and layout(),
        // but it's OK because I really want the result based on how things
        // were last displayed rather than where they should be displayed next
        // time layout()/draw() happens.
		int lines, sign = (Fl::event_dy() < 0) ? -1 : 1;
        if (abs(Fl::event_dy()) > mNVisibleLines-2) lines = mNVisibleLines-2;
        else lines = abs(Fl::event_dy());
        scroll(mTopLineNum - lines*sign, mHorizOffset);
        return 1;
#endif

	default:
		break;
    }
    return Fl_Widget::handle(event);
}

/**
 * Refresh the line number area.  If clearAll is False, writes only over 
 * the character cell areas.  Setting clearAll to True will clear out any 
 * stray marks outside of the character cell area, which might have been 
 * left from before a resize or font change. 
 */ 
void Fl_Text_Display::draw_line_numbers()
{
    /* Don't draw if mLineNumWidth == 0 (line numbers are hidden), or widget is not yet realized */
    if(mLineNumWidth == 0 || !visible_r())
        return;

    int X = mLineNumLeft+box()->dx();
    int Y = box()->dy();
    int W = mLineNumWidth;
    int H = h()-box()->dh();

    fl_font(text_font(), text_size());

    int y, line, visLine, nCols, lineStart;
    char lineNumString[12];
    int lineHeight = mMaxsize ? mMaxsize : int(fl_height()+leading());
    float charWidth = mMaxFontBound+.5;

	fl_color(button_color());
    button_box()->draw(X,Y,W,H,button_color());
    button_box()->inset(X,Y,W,H);
    
	fl_push_clip(X,Y,W,H);
    Y -= box()->dy();

    /* Draw the line numbers, aligned to the text */
    nCols = min(12, int(mLineNumWidth / charWidth) );
    y = Y+lineHeight;
    line = get_absolute_top_line_number();

    for (visLine=0; visLine < mNVisibleLines; visLine++)
    {
        lineStart = mLineStarts[visLine];
        if(lineStart != -1 && (lineStart==0 || buffer()->character(lineStart-1)=='\n'))
        {
            sprintf(lineNumString, "%d", line);
			fl_color(text_color());           
            fl_draw(lineNumString, W - int(fl_width(lineNumString)), y);
            line++;
        } 
		else if(mContinuousWrap) {
            fl_color(button_color());
            fl_rectf(X, y, W, int(fl_height()+fl_descent()));
            if (visLine == 0)
                line++;
        }
        y += lineHeight;
    }
    fl_pop_clip();
}

void Fl_Text_Display::set_linenumber_area(int left, int width)
{
    mLineNumLeft = left;
    mLineNumWidth = width;

    if(buffer()) reset_absolute_top_line_number();

    relayout();
    redraw();
}

//
// End of "$Id: Fl_Text_Display.cpp 1462 2003-06-18 15:50:38Z laza2000 $".
//
