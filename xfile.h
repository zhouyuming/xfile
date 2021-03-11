
/*
 * XFILE.H
 *
 */

#ifndef __XFILE_H__
#define __XFILE_H__

#ifdef __cplusplus
    extern "C" {
#endif

// typedef int XFILEID;

#define XFILE_OK                    0
#define XFILE_ERR                   -1
#define XFILE_INVALID_FID           0
#define XFILE_ERR_LINKLOST          -2
#define XFILE_ERR_FILE_NOTEXIST     -3
#define XFILE_ERR_NOT_INITIALIZE    -4

/* Seek method constants */
#define XFILE_SEEK_CUR              1
#define XFILE_SEEK_END              2
#define XFILE_SEEK_SET              0

#ifdef WIN32
#define XFILE_STDCALL __stdcall
#else
#define XFILE_STDCALL
#endif

//#define XFILE_RDONLY                0x0000  /* open for reading only */
//#define XFILE_WRONLY                0x0001  /* open for writing only */
//#define XFILE_RDWR                  0x0002  /* open for reading and writing */
//#define XFILE_APPEND                0x0008  /* writes done at eof */
//
//#define XFILE_CREAT                 0x0100  /* create and open file */
//#define XFILE_TRUNC                 0x0200  /* open and truncate */
//#define XFILE_EXCL                  0x0400  /* open only if file doesn't already exist */
//
///* O_TEXT files have <cr><lf> sequences translated to <lf> on read()'s,
//** and <lf> sequences translated to <cr><lf> on write()'s
//*/
//#define XFILE_TEXT                  0x4000  /* file mode is text (translated) */
//#define XFILE_BINARY                0x8000  /* file mode is binary (untranslated) */
//#define XFILE_PROCESS              0x10000  /* the target file is a process */
        
#define XFILE_MALLOC(size)          malloc(size) /*tbmalloc(size)*/
#define XFILE_FREE(ptr)             free(ptr) /*tbfree(ptr)*/
#define XFILE_ASSERT(expr)          {if (!(expr)) {(VOID)printf ("\r\nASSERT FAIL: " #expr ", at %s:%d", __FILE__, __LINE__);}}
#define XFILE_ASSURE(expr, ret, var) {if (!(expr)) {(VOID)printf ("\r\nASSERT FAIL: " #expr ", at %s:%d (%s=%d)", __FILE__, __LINE__, #var, var); return (ret);}}
#define XFILE_MSG_NOACK             0x80000000
#define XFILE_PRINT_BUFSIZE         1023
#define XFILE_BUFSIZE               (512 * 1024)
#define XFILE_MAX_PATH              1023

typedef enum tagXFILE_MSG_TYPE
{
    XFILE_HELLO = 1,
    XFILE_OPEN,
    XFILE_READ,
    XFILE_WRITE,
    XFILE_SEEK,
    XFILE_TELL,
    XFILE_CLOSE,
    XFILE_EOF,
    XFILE_FLUSH,
    XFILE_SIZE,
    XFILE_PWD,
    XFILE_CWD,
    XFILE_TIMESTAMP,
    XFILE_READLINE
} XFILE_MSG_TYPE;

typedef struct tagXFILE_MSG
{
    size_t ulCmdType;
    union { int fid; void *f; }; /*lint !e658 */
    size_t ulRetInfo;
    size_t ulDataBufLen; /* buffer size in bytes */
    char *data;

    union
    {
        struct {
            size_t ulReadBytes;
            size_t uiOffset;
        } stRead;
        struct {
            size_t ulWriteBytes;
        } stWrite;
        struct {
            // int ulMode;
            char szMode[8];
        } stOpen;
        struct {
            size_t ulPos;
        } stTell;
        struct {
            size_t lOffset;
            size_t lOrigin;
        } stSeek;
        struct {
            size_t ul[2];
        } stDummy;
    } u;

#define stRead  u.stRead
#define stWrite u.stWrite
#define stOpen  u.stOpen
#define stTell  u.stTell
#define stSeek  u.stSeek
#define stDummy u.stDummy
} XFILE_MSG;

typedef struct tagXFILELINK
{
    int skt;
    int ulSndBufLen;
    int ulRcvBufLen;
} XFILELINK;

typedef struct tagXFILE
{
    // int fid;
    union {int fid; void *f;}; /*lint !e658 */
    XFILELINK *flnk;
    char path[XFILE_MAX_PATH + 1];
    char buf[XFILE_BUFSIZE]; /* read and write buffer */
    unsigned int bufpos; /* position in buffer */
    unsigned int filepos;
    // unsigned int mode;
    char mode[8];

    struct tagXFILE *next;
    struct tagXFILE *prev;
} XFILE;

typedef struct tagXFILE_STARTINFO
{
    char *pszSvrHost;
    unsigned int ulPort;
} XFILE_STARTINFO;

#define XFILE_INFO xfile_dbg
#define XFILE_PRNT (VOID) printf

int xfile_start (char *szXfHost, unsigned short usPort);

char *xfile_timestamp (char *tsbuf, int len);
int xfile_hosttimestamp (char *buf);

XFILELINK *xfile_link (char* pszIpAddr, int ulPort);
int xfile_downlink  (XFILELINK *flnk);
XFILE *xfile_openEx (XFILELINK *flnk, char *pszFile, char *szModule);
int xfile_hello     (XFILELINK *flnk);
int xfile_pwdEx     (XFILELINK *flnk, char *path, int maxlen);
int xfile_chdirEx   (XFILELINK *flnk, char *path);
int xfile_synctime  (void);

XFILE *xfile_open   (char *pszFile, char *szMode);
int xfile_pwd       (char *path, int maxlen);
int xfile_chdir     (char *path);

int xfile_read      (XFILE *xf, char *buf, unsigned int count);
int xfile_readline  (XFILE *xf, char *buf, unsigned int count);
int xfile_close     (XFILE *xf);
int xfile_print     (XFILE *xf, char *fmt, ...);
int xfile_write     (XFILE *xf, char *buf, unsigned int count);
int xfile_eof       (XFILE *xf);
int xfile_flush     (XFILE *xf);
int xfile_seek      (XFILE *xf, int offset, int origin);
int xfile_tell      (XFILE *xf);
//void xfile_dbg      (int cmd, char *fmt, ...);
int xfile_errno     (void);
    
#ifdef __cplusplus
    }
#endif

#endif /* __XFILE_H__ */

