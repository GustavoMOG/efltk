#ifndef __STDLIBX_H__
#define __STDLIBX_H__

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <winsock.h>
#include <errno.h>

#include <tchar.h>
//#include <wceatl.h>
//#include <atlconv.h>
#include <winnetwk.h>

//#define assert(e) 
void abort();

#define _environ (NULL)
char *getenv( const char *varname );
int putenv( const char *envstring );

int _strnicmp(const char *string1,const char *string2,size_t count );

int		_mkdir(const char *);
int		_rmdir(const char *);
int		_access(const char *, int);
int access(const char * s, int i);
int _chdrive(int d);
int _chdir(const char * s);
char *	_mktemp (char *temp);
int		remove(const char *);
int		_chmod( const char *filename, int pmode );
int		rename( const char *oldname, const char *newname );

#define _O_RDONLY       0x0000  /* open for reading only */
#define _O_WRONLY       0x0001  /* open for writing only */
#define _O_RDWR         0x0002  /* open for reading and writing */
#define _O_APPEND       0x0008  /* writes done at eof */

#define _O_CREAT        0x0100  /* create and open file */
#define _O_TRUNC        0x0200  /* open and truncate */
#define _O_EXCL         0x0400  /* open only if file doesn't already exist */

/* O_TEXT files have <cr><lf> sequences translated to <lf> on read()'s,
** and <lf> sequences translated to <cr><lf> on write()'s
*/

#define _O_TEXT         0x4000  /* file mode is text (translated) */
#define _O_BINARY       0x8000  /* file mode is binary (untranslated) */

/* macro to translate the C 2.0 name used to force binary mode for files */

#define _O_RAW  _O_BINARY

/* Open handle inherit bit */

#define _O_NOINHERIT    0x0080  /* child process doesn't inherit file */

/* Temporary file bit - file is deleted when last handle is closed */

#define _O_TEMPORARY    0x0040  /* temporary file bit */

/* temporary access hint */

#define _O_SHORT_LIVED  0x1000  /* temporary storage file, try not to flush */

/* sequential/random access hints */
#define _O_SEQUENTIAL   0x0020  /* file access is primarily sequential */
#define _O_RANDOM       0x0010  /* file access is primarily random */


#define O_RDONLY        _O_RDONLY
#define O_WRONLY        _O_WRONLY
#define O_RDWR          _O_RDWR
#define O_APPEND        _O_APPEND
#define O_CREAT         _O_CREAT
#define O_TRUNC         _O_TRUNC
#define O_EXCL          _O_EXCL
#define O_TEXT          _O_TEXT
#define O_BINARY        _O_BINARY
#define O_RAW           _O_BINARY
#define O_TEMPORARY     _O_TEMPORARY
#define O_NOINHERIT     _O_NOINHERIT
#define O_SEQUENTIAL    _O_SEQUENTIAL
#define O_RANDOM        _O_RANDOM

#define _SH_DENYRW      0x10    /* deny read/write mode */
#define _SH_DENYWR      0x20    /* deny write mode */
#define _SH_DENYRD      0x30    /* deny read mode */
#define _SH_DENYNO      0x40    /* deny none mode */
long	_lseek(int, long, int);

int		_close(int);
int		_read(int, void *, unsigned int);
int		_write(int, const void *, unsigned int);
int		_open( const char *filename, int oflag , int pmode );
int		_chsize( int handle, long size );
int              _sopen(const char *, int, int, ...);

#if _WIN32_WCE < 300
long			strtol( const char *nptr, char **endptr, int base );
unsigned long	strtoul( const char *nptr, char **endptr, int base );
double			strtod( const char *nptr, char **endptr );
#endif
char *			_i64toa (__int64 val,char *buf,int radix);
char *			_ui64toa (unsigned __int64 val,char *buf,int radix);

#ifndef _X86_
__int64			_atoi64(const char *nptr);
#endif

#if _WIN32_WCE < 300
const char *	strrchr(const char *, int);
size_t strspn( const char *string, const char *strCharSet );
#endif

char * getcwd(char *tmp, int size);
wchar_t * _wgetenv(wchar_t *env);
int stricmp(const char *string1, const char *string2);
int strncasecmp(const char *string1, const char *string2,int len);
int strcasecmp(const char *string1, const char *string2);
char * strdup(const char *todup);
char * strerror(int num);

# define floorf(a)	float(floor(float(a)))
# define ceilf(a)	float(ceil(float(a)))
# define fmodf(a,b)	float(fmod(float(a),float(b)))
# define fabsf(a)	float(fabs(float(a)))
# define sinf(a)	float(sin(float(a)))
# define cosf(a)	float(cos(float(a)))
# define tanf(a)	float(tan(float(a)))
# define asinf(a)	float(asin(float(a)))
# define acosf(a)	float(acos(float(a)))
# define atanf(a)	float(atan(float(a)))
# define atan2f(a,b)	float(atan2(float(a),float(b)))
# define expf(a)	float(exp(float(a)))
# define logf(a)	float(log(float(a)))
# define log10f(a)	float(log10(float(a)))
# define sqrtf(a)	float(sqrt(float(a)))

#ifdef __cplusplus
}
#endif

#endif



/*LONG RegQueryValueEx( HKEY hKey, char* lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData );
LONG RegOpenKeyEx( HKEY hKey, const char* lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult );
*/

