#include <config.h>

/* This is an XPM image file loading framework */
#include <efltk/Fl_Image.h>
#include <efltk/Fl.h>

#include <stdlib.h>
#include <ctype.h>

#include "fl_internal.h"

#define XPM_BYTES_TO_CHECK 10
static bool xpm_is_valid_file(const char *filename)
{
#ifndef VALIDATE_IMAGE_CONTENT
	int len=strlen(filename)-3;
	if(len<1) return false;
	return !strncasecmp(filename+len, "XPM", 3);
#else
    char type[XPM_BYTES_TO_CHECK];
    uint32 pos = ftell(fp);
    fread(type, XPM_BYTES_TO_CHECK, 1, fp);
    fseek(fp, pos, SEEK_SET); //return position in file

    return (memcmp(type, "/* XPM */", 9) == 0);
#endif
}

static bool xpm_is_valid_xpm(const uint8 **stream)
{
    // The header string of an XPMv3 image has the format
    // <width> <height> <ncolors> <cpp> [ <hotspot_x> <hotspot_y> ]
    char *ptr = ((char **)stream)[0];
    if(!ptr) return false;
    int w, h, ncol, cpp;
    if(sscanf(ptr, "%d %d %d %d", &w, &h, &ncol, &cpp) != 4)
        return false;
    return true;
}

static bool xpm_is_valid_mem(const uint8 *stream, uint32 size)
{
	if(size==0) {
		return xpm_is_valid_xpm((const uint8 **)stream);
	} 
    return size>=XPM_BYTES_TO_CHECK && (memcmp(stream, "/* XPM */", 9) == 0);
}

static char *xpm_gets(Fl_IO *xpm_io, char *string, int maxlen)
{
    if(!xpm_io) return 0;

    int i;
    for ( i=0; i<(maxlen-1); ++i ) {
        if(xpm_io->read(&string[i], 1) <= 0 )
        {
            /* EOF or error */
            if(i == 0) {
                /* Hmm, EOF on initial read, return NULL */
                return 0;
            }
            break;
        }
        /* In this case it's okay to use either '\r' or '\n'
         as line separators because blank lines are just
         ignored by the XPM format.
         */
        if( (string[i] == '\n') || (string[i] == '\r') ) {
            break;
        }
    }
    string[i] = '\0';
    return string;
}

/* Hash table to look up colors from pixel strings */
#define STARTING_HASH_SIZE 256

struct hash_entry {
    char *key;
    uint32 color;
    struct hash_entry *next;
};

struct color_hash {
    struct hash_entry **table;
    struct hash_entry *entries; /* array of all entries */
    struct hash_entry *next_free;
    int size;
    int maxnum;
};

static int hash_key(const char *key, int cpp, int size) {
    int hash = 0;
    while ( cpp-- > 0 ) hash = hash * 33 + *key++;
    return hash & (size - 1);
}

static struct color_hash *create_colorhash(int maxnum)
{
    int bytes, s;
    struct color_hash *hash;

    /* we know how many entries we need, so we can allocate
     everything here */
    hash = (color_hash *)malloc(sizeof *hash);
    if(!hash)
        return NULL;

    /* use power-of-2 sized hash table for decoding speed */
    for(s = STARTING_HASH_SIZE; s < maxnum; s <<= 1)
        ;
    hash->size = s;
    hash->maxnum = maxnum;
    bytes = hash->size * sizeof(struct hash_entry **);
    hash->entries = NULL;	/* in case malloc fails */
    hash->table = (hash_entry **)malloc(bytes);
    if(!hash->table)
        return NULL;
    memset(hash->table, 0, bytes);
    hash->entries = (hash_entry *)malloc(maxnum * sizeof(struct hash_entry));
    if(!hash->entries)
        return NULL;
    hash->next_free = hash->entries;
    return hash;
}

static int add_colorhash(struct color_hash *hash,
                         char *key, int cpp, uint32 color)
{
    int index = hash_key(key, cpp, hash->size);
    struct hash_entry *e = hash->next_free++;
    e->color = color;
    e->key = key;
    e->next = hash->table[index];
    hash->table[index] = e;
    return 1;
}

static uint32 get_colorhash(struct color_hash *hash, const char *key, int cpp)
{
    struct hash_entry *entry;
    if(cpp==1) entry = hash->table[*(uint8 *)(key)];
    else entry = hash->table[hash_key(key, cpp, hash->size)];
    while(entry) {
        if(memcmp(key, entry->key, cpp) == 0)
            return entry->color;
        entry = entry->next;
    }
    return 0;		/* garbage in - garbage out */
}

static void free_colorhash(struct color_hash *hash)
{
    if(hash && hash->table) {
        free(hash->table);
        free(hash->entries);
        free(hash);
    }
}

#define ARRAYSIZE(a) (int)(sizeof(a) / sizeof((a)[0]))

/*
 * convert colour spec to RGB (in 0xrrggbb format).
 * return 1 if successful. may scribble on the colorspec buffer.
 */
static int color_to_rgb(char *spec, uint32 *rgb)
{
    if(!strcasecmp("none", spec) || !strcasecmp("#background", spec)) {
        *rgb = 0x00000000;
        return 1;
    }
#ifndef _WIN32
    XColor x;
    fl_open_display();
    if(!XParseColor(fl_display, fl_colormap, spec, &x)) {
        *rgb = 0x00000000;
        return 1;
    }
    int16 r = x.red>>8;
    int16 g = x.green>>8;
    int16 b = x.blue>>8;
    *rgb = (r<<24)|(g<<16)|(b<<8)|255;
    return 1;
#else
    /* poor man's rgb.txt */
    static struct { char *name; uint32 rgb; } known[] = {
        { "none",  0xffffffff },
        { "background",  0xffffffff },
        { "black", 0x000000ff },
        { "white", 0xffffffff },
        { "red",   0xff0000ff },
        { "green", 0x00ff00ff },
        { "blue",  0x0000ffff }
    };

    if(spec[0] == '#') {
        char buf[7];
        ++spec;
        switch(strlen(spec)) {
        case 3:
            buf[0] = buf[1] = spec[0];
            buf[2] = buf[3] = spec[1];
            buf[4] = buf[5] = spec[2];
            break;
        case 6:
            memcpy(buf, spec, 6);
            break;
        case 12:
            buf[0] = spec[0];
            buf[1] = spec[1];
            buf[2] = spec[4];
            buf[3] = spec[5];
            buf[4] = spec[8];
            buf[5] = spec[9];
            break;
        }
        buf[6] = '\0';
        *rgb = (strtol(buf, NULL, 16) << 8) | 255;
        return 1;
    } else {
        for(int i = 0; i < ARRAYSIZE(known); i++)
            if(strcmp(known[i].name, spec)) {
                *rgb = known[i].rgb;
                return 1;
            }
        return 0;
    }
#endif
}

static char *skipspace(char *p) {
    while(isspace((unsigned char)*p)) ++p;
    return p;
}

static char *skipnonspace(char *p) {
    while(!isspace((unsigned char)*p) && *p) ++p;
    return p;
}

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* Load a XPM type image from stream */
static bool xpm_create(Fl_IO *xpm_io, uint8 *&data, Fl_PixelFormat &fmt, int &w, int &h, const char **stream)
{
    bool mem = (stream!=0);
    char line[4096];
    char *here;
    int index;
    int x, y;
    int ncolors, cpp;
    int pixels_len=0;
    char *pixels = NULL;
    int indexed;
    uint8 *dst;
    struct color_hash *colors;
    Fl_Colormap_Color *im_colors = NULL;
    char *keystrings, *nextkey;
    char *error = NULL;

    /* Skip to the first string, which describes the image */
    if(mem) {
        here = (char *)stream[0];
    } else {
        do {
            here = xpm_gets(xpm_io, line, sizeof(line));
            if(!here) {
                fputs("XPM: Premature end of data 1", stderr);
				return false;
			}
            here = skipspace(here);
        } while(*here != '"');
    }

    /*
     * The header string of an XPMv3 image has the format
     *
     * <width> <height> <ncolors> <cpp> [ <hotspot_x> <hotspot_y> ]
     *
     * where the hotspot coords are intended for mouse cursors.
     * Right now we don't use the hotspots but it should be handled
     * one day.
     */
    int offset = mem ? 0 : 1;
    if(sscanf(here + offset, "%d %d %d %d", &w, &h, &ncolors, &cpp) != 4
       || w <= 0 || h <= 0 || ncolors <= 0 || cpp <= 0) {
        fputs("XPM: Invalid format description", stderr);
		return false;
    }

    keystrings = (char *)malloc(ncolors * cpp);
    if(!keystrings) {        
        fputs("XPM: Out of memory", stderr);
    }
    nextkey = keystrings;

    /* Create the new surface */
    if(ncolors <= 256) {
        indexed = 1;
        fmt.realloc(8, 0,0,0,0);
        im_colors = fmt.palette->colors;
        fmt.palette->ncolors = ncolors;
    } else {
        indexed = 0;
        //fmt.realloc(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0);
        fmt.realloc(32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0);
    }

    int pitch = Fl_Renderer::calc_pitch(fmt.bytespp, w);
    data = (uint8*)malloc(h*pitch*sizeof(uint8));

    /* Read the colors */
    colors = create_colorhash(ncolors);
    if(!colors) {
        error = "XPM: Out of memory";
        goto done;
    }

    for(index = 0; index < ncolors; ++index )
    {
        char *key;
        int len;

        if(mem) {
            strncpy(line, stream[index+1], sizeof(line));
            here = line;
        } else {
            do {
                here = xpm_gets(xpm_io, line, sizeof(line));
                if(!here) {
                    error = "XPM: Premature end of data 2";
                    goto done;
                }
                here = skipspace(here);
            } while(*here != '"');
            ++here;
        }
        len = strlen(here);
        if(len < cpp + 7)
            continue;	/* cannot be a valid line */

        key = here;
        key[cpp] = '\0';
        here += cpp + 1;

        /* parse a colour definition */
        for(;;) {
            char nametype;
            char *colname;
            char delim;
            uint32 rgb;

            here = skipspace(here);
            nametype = *here;
            here = skipnonspace(here);
            here = skipspace(here);
            colname = here;
            if(mem) {
                while(*here && !isspace((unsigned char)*here))
                    here++;
            } else {
                while(*here && !isspace((unsigned char)*here) && *here != '"')
                    here++;
                if(!*here) {
                    error = "XPM: Color parse error";
                    goto done;
                }
            }

            if(nametype == 's')
                continue;      /* skip symbolic colour names */

            delim = *here;
            *here = '\0';
            if(delim)
                here++;

            if(!color_to_rgb(colname, &rgb)) {
                continue;
            }

            if(rgb == 0x00000000) {
                if(!indexed) {
                    if(!fmt.Amask) {
                        fmt.realloc(32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
                        fmt.masktype = FL_MASK_ALPHA;
                    }
                    rgb = 0xffffff00;
                } else {
                    fmt.masktype = FL_MASK_COLORKEY;
                    fmt.colorkey = rgb;
                }
            }

            memcpy(nextkey, key, cpp);
            if(indexed) {
                Fl_Colormap_Color *c = im_colors + index;
                c->r = rgb >> 24;
                c->g = rgb >> 16;
                c->b = rgb >> 8;
                c->a = 0;
                if(rgb == 0x00000000) c->a = 1;
                add_colorhash(colors, nextkey, cpp, index);
            } else
                add_colorhash(colors, nextkey, cpp, rgb);
            nextkey += cpp;
            break;
        }
    }

    /* Read the pixels */
    if(!mem) {
        pixels_len = w * cpp;
        pixels = (char *)malloc(MAX(pixels_len + 5, 20));
        if(!pixels) {
            error = "XPM: Out of memory";
            goto done;
        }
    }

    dst = data;
    for (y = 0; y < h; ) {
        char *s;
        if(!mem) {

            // Read line of pixels
            char c;
            do {
                if(xpm_io->read(&c, 1) <= 0) {
                    error = "XPM: Premature end of data 3";
                    goto done;
                }
            } while(c == ' ');
            if(c != '"') {
                /* comment or empty line, skip it */
                while(c != '\n' && c != '\r') {
                    if(xpm_io->read(&c, 1) <= 0) {
                        error = "XPM: Premature end of data 4";
                        goto done;
                    }
                }
                continue;
            }
            if(xpm_io->read(pixels, pixels_len + 3) <= 0) {
                error = "XPM: Premature end of data 5";
                goto done;
            }

        } else {
            // From static table.
            pixels = (char*)stream[y+ncolors+1];
        }

        s = pixels;
        if(indexed) {
            for(x = 0; x < w; x++)
                dst[x] = get_colorhash(colors, s + x * cpp, cpp);
        } else {
            for (x = 0; x < w; x++)
                ((uint32*)dst)[x] = get_colorhash(colors, s + x * cpp, cpp);
        }
        dst += pitch;
        y++;
    }

done:
    if(error) {
        if(data) free(data);
        data = 0;
        fputs(error, stderr);
    }
    if(!mem) free(pixels);
    free(keystrings);
    free_colorhash(colors);

    return (data!=0);
}

static bool xpm_read_file(const char *filename, int quality, uint8 *&data, Fl_PixelFormat &data_format, int &w, int &h)
{
	FILE *fp = fopen(filename, "r");
	if(!fp) return false;

    Fl_IO xpm_io;
    xpm_io.init_io(fp, 0, 0);
    bool ret = xpm_create(&xpm_io, data, data_format, w, h, 0);

	fclose(fp);
	return ret;
}

static bool xpm_read_mem(const uint8 *stream, uint32 size, int quality, uint8 *&data, Fl_PixelFormat &data_format, int &w, int &h)
{
    return xpm_create(0, data, data_format, w, h, (const char**)stream);
}

Fl_Image_IO xpm_reader =
{
    /* GENERAL: */
    "XPM", //name
    "xpm", //filename extension

    /* VALIDATE FUNCTIONS: */
    xpm_is_valid_file, //bool (*is_valid_file)(const char *filename, FILE *fp);
    xpm_is_valid_mem, //bool (*is_valid_mem)(uint8 *stream, uint32 size);

    /* READ FUNCTIONS: */
    xpm_read_file, //bool (*read_file)(FILE *fp, int quality, uint8 *&data, Fl_PixelFormat &format, int &w, int &h);
    xpm_read_mem, //bool (*read_mem)(uint8 *stream, uint32 size, int quality, uint8 *&data, Fl_PixelFormat &format, int &w, int &h);

    /* WRITE FUNCTIONS: */
    NULL, //bool (*write_mem)(uint8 *&stream, int &size, int quality, uint8 *data, Fl_PixelFormat &data_format, int w, int h);
    NULL //bool (*write_file)(FILE *fp, int quality, uint8 *data, Fl_PixelFormat &format, int w, int h);
};
