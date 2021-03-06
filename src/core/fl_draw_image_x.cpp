#ifndef _WIN32

#include <efltk/Fl_Renderer.h>
#include <efltk/Fl_Image.h>
#include <efltk/Fl.h>
#include <efltk/fl_draw.h>
#include <efltk/x.h>

#include <string.h>

#include <config.h>

extern void fl_restore_clip();

void Fl_Image::to_screen(int XP, int YP, int WP, int HP, int, int)
{
    int X,Y,W,H;
    fl_clip_box(XP, YP, WP, HP, X, Y, W, H);

    int cx = X-XP, cy = Y-YP;

    if(cx+W > WP) W = WP-cx;
    if(W <= 0) return;

    if(cy+H > HP) H = HP-cy;
    if(H <= 0) return;

    // convert to Xlib coordinates:
    fl_transform(X,Y);

    //printf("TOSCREEN:    %d %d %d %d %d %d\n",X,Y,W,H,cx,cy);
    if(mask) {
        if(id) {
            // both color and mask:
            // I can't figure out how to combine a mask with existing region,
            // so the mask replaces the region instead. This can draw some of
            // the image outside the current clip region if it is not rectangular.

            XSetClipMask(fl_display, fl_gc, (Pixmap)mask);
            XSetClipOrigin(fl_display, fl_gc, X-cx, Y-cy);

            fl_copy_offscreen(X, Y, W, H, (Pixmap)id, cx, cy);

            fl_restore_clip();
            XSetClipOrigin(fl_display, fl_gc, 0, 0);

        } else if(mask) {
            // mask only
            XSetStipple(fl_display, fl_gc, (Pixmap)mask);
            int ox = X-cx; if (ox < 0) ox += width();
            int oy = Y-cy; if (oy < 0) oy += height();
            XSetTSOrigin(fl_display, fl_gc, ox, oy);
            XSetFillStyle(fl_display, fl_gc, FillStippled);
            XFillRectangle(fl_display, fl_window, fl_gc, X, Y, W, H);
            XSetFillStyle(fl_display, fl_gc, FillSolid);
        }
    } else if(id)
    {
        // pix only, no mask
        fl_copy_offscreen(X, Y, W, H, (Pixmap)id, cx, cy);
    } // else { no mask or id, probably an error... }
}

void Fl_Image::to_screen_tiled(int XP, int YP, int WP, int HP, int, int)
{
    // Figure out the smallest rectangle enclosing this and the clip region:
    int X,Y,W,H;
    fl_clip_box(XP, YP, WP, HP, X, Y, W, H);

    if (W <= 0 || H <= 0) return;

    int cx = X-XP, cy = Y-YP;

    if(cx+W > WP) W = WP-cx;
    if(W <= 0) return;

    if(cy+H > HP) H = HP-cy;
    if(H <= 0) return;

    if(mask)
    {
        // Draw tiled with mask.
        // We must put each image to server. I cannot find way to
        // set clipmask for tiling operation..

        fl_push_clip(X, Y, W, H);

        int temp = -cx % width();
        cx = (temp>0 ? width() : 0) - temp;
        temp = -cy % height();
        cy = (temp>0 ? height() : 0) - temp;

        int ccx=cx;
        while (-cy < H) {
            while (-cx < W) {
                to_screen(X-cx, Y-cy, width(), height(), 0,0);
                cx -= width();
            }
            cy -= height();
            cx = ccx;
        }

        fl_pop_clip();

    }
    else if(id) {

        // Draw tiled without mask.
        // We optimize this by making server to do all work :)

        fl_transform(X,Y);

        XGCValues xgcval, xgcsave;
        xgcval.fill_style = FillTiled;
        xgcval.tile = (Pixmap)id;
        xgcval.ts_x_origin = X-cx;
        xgcval.ts_y_origin = Y-cy;

        int gcmask = GCTile|GCFillStyle|GCTileStipXOrigin|GCTileStipYOrigin;

        XGetGCValues(fl_display, fl_gc, gcmask, &xgcsave);
        XChangeGC(fl_display, fl_gc, gcmask, &xgcval);

        XFillRectangle(fl_display, fl_window, fl_gc, X, Y, W, H);

        if( (xgcsave.tile & 0xe0000000) || (xgcsave.fill_style != FillTiled) )
            XChangeGC(fl_display, fl_gc, GCFillStyle|GCTileStipXOrigin|GCTileStipYOrigin, &xgcsave);
        else
            XChangeGC(fl_display, fl_gc, gcmask, &xgcsave);
    }
}

static XImage s_image;	// static image used to pass info to X
static bool _system_inited = false;
static int  _scanline_add  = 0;
static int  _scanline_mask = 0;
Fl_PixelFormat sys_fmt;

Fl_PixelFormat *Fl_Renderer::system_format() {
    Fl_Renderer::system_init();
    return &sys_fmt;
}
bool Fl_Renderer::system_inited() {
    return (_system_inited && fl_display);
}
bool Fl_Renderer::big_endian() {
    Fl_Renderer::system_init();
    return (s_image.byte_order==MSBFirst);
}
bool Fl_Renderer::lil_endian() {
    Fl_Renderer::system_init();
    return (s_image.byte_order==LSBFirst);
}

////////////////////////////////////////////////////////////////
static XPixmapFormatValues *pfvlist=0;
static int num_pfv=0;
XPixmapFormatValues *pfv=0;

void Fl_Renderer::system_init()
{
    if(_system_inited) return;

    fl_open_display();
    fl_xpixel(FL_BLACK); // make sure figure_out_visual in fl_color.cxx is called
    fl_xpixel(FL_WHITE); // also make sure white is allocated

    if(!pfvlist) pfvlist = XListPixmapFormats(fl_display, &num_pfv);
    for(pfv = pfvlist; pfv < pfvlist+num_pfv; pfv++) {
        if(pfv->depth == fl_visual->depth)
            break;
    }

    //XPutImage will handle swapping...
    s_image.byte_order = (WORDS_BIGENDIAN==1) ? MSBFirst : LSBFirst;
    s_image.format = ZPixmap;

#ifdef _DEBUG
    fprintf(stderr, "System Image Format: %d bits per pixel\n", pfv->bits_per_pixel);
    fprintf(stderr, "Display Visual: %d bits per pixel\n", fl_visual->depth);
    fprintf(stderr, "System Image byte-order: %s\n", (s_image.byte_order==LSBFirst)?"Little-endian":"Big-endian");
#endif

    s_image.depth = fl_visual->depth;
    s_image.bits_per_pixel = pfv->bits_per_pixel;

    if(s_image.bits_per_pixel & 7) {
        Fl::fatal("FATAL ERROR! Can't do %d bits per pixel\n", s_image.bits_per_pixel);
    }

    uint n = pfv->scanline_pad/8;
    if (pfv->scanline_pad & 7 || (n&(n-1))) {
        Fl::fatal("Can't do scanline_pad of %d\n", pfv->scanline_pad);
    }

    if (n < 4) n = 4;
    _scanline_add = n-1;
    _scanline_mask = -n;

    sys_fmt.init(pfv->bits_per_pixel,
                 fl_visual->visual->red_mask,
                 fl_visual->visual->green_mask,
                 fl_visual->visual->blue_mask,
                 0);

#if USE_COLORMAP
    if(pfv->bits_per_pixel<=8) {
        extern void copy_palette(Fl_Colormap *map);
        copy_palette(sys_fmt.palette);
    }
#endif

    _system_inited = true;
}

static int _x_err = 0;
static void Tmp_HandleXError(Display * d, XErrorEvent * ev)
{
    d = NULL;
    ev = NULL;
    _x_err=1;
}

uint8 *ximage_to_data(XImage *im, Fl_PixelFormat *desired)
{
    int W=im->width;
    int H=im->height;

    if(!(im->red_mask && im->green_mask && im->blue_mask)) {
        // No masks?, Set default masks
        Visual *visual = fl_visual->visual;
        im->red_mask = visual->red_mask;
        im->green_mask = visual->green_mask;
        im->blue_mask = visual->blue_mask;
    }

    Fl_PixelFormat fmt;
    fmt.init(im->depth, im->red_mask, im->green_mask, im->blue_mask, 0);
    int pitch = Fl_Renderer::calc_pitch(desired->bytespp, W);

    uint32 pixel;
    uint8 *data = new uint8[H*pitch];
    uint8 *ptr;
    uint8 r, g, b;
    int x, y;
    for(y = 0; y < H; y++) {
        ptr = (uint8*)data + (y*pitch);
        for(x = 0; x < W; x++) {
            pixel = XGetPixel(im, x, y);
            fl_rgb_from_pixel(pixel, &fmt, r,g,b);
            fl_assemble_rgb(ptr, desired->bytespp, desired, r, g, b);
            ptr+=desired->bytespp;
        }
    }
    return data;
}

uint8 *Fl_Renderer::data_from_window(Window src, Fl_Rect &rect, Fl_PixelFormat *desired)
{
    // Init renderer
    Fl_Renderer::system_init();

    int x = rect.x();
    int y = rect.y();
    int w = rect.w();
    int h = rect.h();
    int width, height, clipx, clipy;
    int src_x, src_y, src_w, src_h;
    XErrorHandler prev_erh = 0;
    XWindowAttributes   xatt, ratt;

    prev_erh = XSetErrorHandler((XErrorHandler) Tmp_HandleXError);

    XGetWindowAttributes(fl_display, src, &xatt);

    Window dw;
    XGetWindowAttributes(fl_display, xatt.root, &ratt);
    XTranslateCoordinates(fl_display, src, xatt.root, 0, 0, &src_x, &src_y, &dw);
    src_w = xatt.width;
    src_h = xatt.height;
    if((xatt.map_state != IsViewable) && (xatt.backing_store == NotUseful)) {
        XSetErrorHandler((XErrorHandler) prev_erh);
        return 0;
    }

    /* clip to the drawable tree and screen */
    clipx = clipy = 0;
    width = src_w - x;
    height = src_h - y;
    if(width > w)
        width = w;
    if(height > h)
        height = h;

    if((src_x + x + width) > ratt.width)
        width = ratt.width - (src_x + x);
    if((src_y + y + height) > ratt.height)
        height = ratt.height - (src_y + y);

    if(x < 0) {
        clipx = -x;
        width += x;
        x = 0;
    }

    if (y < 0) {
        clipy = -y;
        height += y;
        y = 0;
    }

    if((src_x + x) < 0) {
        clipx -= (src_x + x);
        width += (src_x + x);
        x = -src_x;
    }

    if((src_y + y) < 0) {
        clipy -= (src_y + y);
        height += (src_y + y);
        y = -src_y;
    }

    if((width <= 0) || (height <= 0)) {
        XSetErrorHandler((XErrorHandler) prev_erh);
        return 0;
    }

    w = width;
    h = height;

    //printf("%d %d %d %d\n", x, y, w, h);
    rect.set(x,y,w,h);
    XImage *im = XGetImage(fl_display, src, x, y, w, h, AllPlanes, ZPixmap);
    XSetErrorHandler((XErrorHandler) prev_erh);
    if(!im) return 0;

    uint8 *im_pixels = ximage_to_data(im, desired);
    XDestroyImage(im);
    return im_pixels;
}

uint8 *Fl_Renderer::data_from_pixmap(Pixmap src, Fl_Rect &rect, Fl_PixelFormat *desired)
{
    // Init renderer
    Fl_Renderer::system_init();

    XImage *im = ximage_from_pixmap(src, rect);
    if(!im) return 0;

    uint8 *im_pixels = ximage_to_data(im, desired);
    XDestroyImage(im);
    return im_pixels;
}

XImage *Fl_Renderer::ximage_from_pixmap(Pixmap src, Fl_Rect &rect)
{
    // Init renderer
    Fl_Renderer::system_init();

    int x = rect.x();
    int y = rect.y();
    int w = rect.w();
    int h = rect.h();
    int	width, height, clipx, clipy;
    int	src_x, src_y, src_w, src_h;
    XErrorHandler prev_erh = 0;
    XWindowAttributes   xatt;
    _x_err = 0;
    bool is_pixmap = false;

    prev_erh = XSetErrorHandler((XErrorHandler) Tmp_HandleXError);

    /* lets see if its a pixmap or not */
    XGetWindowAttributes(fl_display, src, &xatt);
    XSync(fl_display, False);
    if (_x_err)
        is_pixmap = true;

    if (is_pixmap) {
        Window dw;
        XGetGeometry(fl_display, src, &dw, &src_x, &src_y,
                     (unsigned int *)&src_w, (unsigned int *)&src_h,
                     (unsigned int *)&src_x, (unsigned int *)&xatt.depth);
        src_x = 0;
        src_y = 0;
    } else {
        XSetErrorHandler((XErrorHandler) prev_erh);
        return 0;
    }

    /* clip to the drawable tree and screen */
    clipx = 0;
    clipy = 0;
    width = src_w - x;
    height = src_h - y;
    if(width > w)
        width = w;
    if(height > h)
        height = h;

    if(x < 0) {
        clipx = -x;
        width += x;
        x = 0;
    }

    if (y < 0) {
        clipy = -y;
        height += y;
        y = 0;
    }

    if((width <= 0) || (height <= 0)) {
        XSetErrorHandler((XErrorHandler) prev_erh);
        return 0;
    }

    w = width;
    h = height;

    rect.set(x,y,w,h);
    XImage *im = XGetImage(fl_display, src, x, y, w, h, AllPlanes, ZPixmap);
    XSetErrorHandler((XErrorHandler) prev_erh);

    if(!im) return 0;
    return im;
}

Window Fl_Renderer::root_window() {
    fl_open_display();
    return RootWindow(fl_display, fl_screen);
}


//////////////////////////////////////////
bool Fl_Renderer::render_to_pixmap(uint8 *src, Fl_Rect *src_rect, Fl_PixelFormat *src_fmt, int src_pitch,
                                   Pixmap dst, Fl_Rect *dst_rect, GC dst_gc, int flags)
{
    // Init renderer
    Fl_Renderer::system_init();

    if(flags&FL_ALIGN_SCALE && src_rect->w()==dst_rect->w() && src_rect->h()==dst_rect->h()) {
        flags = 0;
    }

    if(flags & FL_ALIGN_SCALE)
    {
        s_image.width 	= dst_rect->w();
        s_image.height 	= dst_rect->h();
        s_image.bytes_per_line = ((dst_rect->w() * sys_fmt.bytespp + _scanline_add) & _scanline_mask);

        uint8 *dst_ptr = new uint8[dst_rect->h()*s_image.bytes_per_line];
        if(stretch(src, sys_fmt.bytespp, src_pitch, src_rect,
                   dst_ptr, sys_fmt.bytespp, s_image.bytes_per_line, dst_rect))
        {
            s_image.data = (char *)dst_ptr;
            XPutImage(fl_display, dst, dst_gc, &s_image, 0, 0, dst_rect->x(), dst_rect->y(), dst_rect->w(), dst_rect->h());
        }
        delete []dst_ptr;
    }
    else
    {
        int X=src_rect->x(), Y=src_rect->y();
        int W=src_rect->w(), H=src_rect->h();

        s_image.bytes_per_line = ((W * sys_fmt.bytespp + _scanline_add) & _scanline_mask);
        s_image.width  = W;
        s_image.height = H;

        if(X>0 || Y>0) {
            // We need to draw 1 pixel height lines, since we need to change
            // data buffer pointer to correct place in image buffer...
            for(int y=0; y<H; y++) {
                s_image.data = (char *)src + ((Y+y)*src_pitch)+(X*sys_fmt.bytespp);
                XPutImage(fl_display, dst, dst_gc, &s_image, 0, 0, dst_rect->x(), dst_rect->y()+y, W, 1);
            }
        } else {
            s_image.data = (char *)src;
            s_image.bytes_per_line = ((src_rect->w() * sys_fmt.bytespp + _scanline_add) & _scanline_mask);
            XPutImage(fl_display, dst, dst_gc, &s_image, 0, 0, dst_rect->x(), dst_rect->y(), src_rect->w(), src_rect->h());
        }
    }

    return true;
}

#endif
