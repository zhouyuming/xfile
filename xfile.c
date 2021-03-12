#include <stdio.h>
#if defined(WIN32)
#include <winsock2.h>
#include <time.h>
#include <ws2tcpip.h>
#elif (defined(NSE_PLATFORM_VXWORKS)||defined(__VxWorks__))
#include <socklib.h>
#include <socket.h>
#include <in.h>
#include <ioLib.h>
#include <inetLib.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#ifndef AF_INET
#define AF_INET         2               /* internetwork: UDP, TCP, etc. */
#endif

#ifndef SOCK_STREAM
#define SOCK_STREAM     1               /* stream socket */
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP     0x6         /* TCP protocol */
#endif

#ifndef SOMAXCONN
#define SOMAXCONN 	0x5
#endif

#endif

#ifndef VOID
#define VOID void
#endif

//#include "nse_ipub.h"
#include "xfile.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#ifdef _WIN64
#define XFILE_HTONL(l)  htonll(l) // 64-bit
#define XFILE_NTOHL(l)  ntohll(l) // 64-bit
#else
#define XFILE_HTONL(l)  htonl(l) // 32-bit
#define XFILE_NTOHL(l)  ntohl(l) // 32-bit
#endif



#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#ifndef WIN32
#ifdef __LINUX__
int WSAGetLastError () { return -1; }
int GetLastError () { return errno; }
typedef int NSE_SOCKET;
#endif
#define _getcwd getcwd
#define _chdir chdir
#define _vsnprintf vsnprintf
#else
extern char * _getcwd(char *cwd, int num);
extern int _chdir(char *pwd);
typedef SOCKET NSE_SOCKET;
#endif

/**
 * @brief XFILE_IN_PATCH
 *
 * xfile 通过补丁的方式加载
 */
#ifdef XFILE_IN_PATCH
#define XFILE_IMPORT extern
#else
#define XFILE_IMPORT
#endif

XFILE_IMPORT int __skt_s;
XFILE_IMPORT XFILELINK *__flnk;
XFILE_IMPORT char __xf_ipaddr[80];
XFILE_IMPORT int __xf_port;

XFILE_IMPORT char __xf_svr_ipaddr[80];
XFILE_IMPORT int  __xf_svr_port;
XFILE_IMPORT int  __xf_opened_num;
XFILE_IMPORT int  __xf_closed_num;
XFILE_IMPORT int  __xf_errno;
XFILE_IMPORT XFILE *__xf_opened;

int xfile_getlasterror ()
{
    #ifdef WIN32
    return (int) GetLastError();
    #else
    return errno;
    #endif
}

char *xfile_timestamp (char *tsbuf, int len)
{
    char tmstr[80];    
    struct tm   *newTime;
    time_t      szClock;

    (void) time( &szClock );
    if (NULL == (newTime = localtime(&szClock)))
    {
        return "";
    }

    (VOID) sprintf (tmstr, "%s", asctime (newTime));
    tmstr[strlen(tmstr) - 1] = 0;
    (VOID) strncpy (tsbuf, tmstr, len);
    tsbuf[len - 1] = '\0';

    return tsbuf;
}

static void xfile_dbg (int cmd, char *fmt, ...)
{
    // int n;
    va_list vp;
    char szBuf[XFILE_PRINT_BUFSIZE + 1] = {0};
    char szTs[80];
    va_start (vp, fmt); /*lint !e826 */
    (void) vsprintf (szBuf, fmt, vp);
    va_end (vp);

    (VOID) printf ("%s - %s", xfile_timestamp(szTs, 79), szBuf);    
}

static int xfile_send (XFILELINK *flnk, XFILE_MSG *msg)
{
    int r;
    int ulSend;
    XFILE_MSG msg_s = {0};

    if (NULL == flnk)
    {
        return XFILE_ERR;
    }

    msg_s.f            = (void *)XFILE_HTONL((size_t)msg->f);
    msg_s.ulCmdType    = XFILE_HTONL(msg->ulCmdType);
    msg_s.ulDataBufLen = XFILE_HTONL(msg->ulDataBufLen);
    msg_s.ulRetInfo    = XFILE_HTONL(msg->ulRetInfo);
    msg_s.stDummy.ul[0] = msg->stDummy.ul[0]; // xxx
    msg_s.stDummy.ul[1] = msg->stDummy.ul[1]; // xxx

    if (XFILE_OPEN != msg->ulCmdType)
    {
        msg_s.stDummy.ul[0] = XFILE_HTONL(msg->stDummy.ul[0]);
        msg_s.stDummy.ul[1] = XFILE_HTONL(msg->stDummy.ul[1]);
    }

    /* send ctrl */
    if ((r = send (flnk->skt, (char *)&msg_s, sizeof(XFILE_MSG), 0)) <= 0) // 有用的只有前4个成员变量
    {
        __xf_errno = XFILE_ERR_LINKLOST;
        XFILE_PRNT ("\r\n[XFILE][MSG] fail to send %d bytes, return %d", sizeof(XFILE_MSG), xfile_getlasterror());
        return XFILE_ERR;
    }

    XFILE_ASSURE (r == (int)sizeof(XFILE_MSG), XFILE_ERR, r);

    /* send data */
    if (msg->data) {
        ulSend = 0;

        while (ulSend < (int)msg->ulDataBufLen) {
            if ((r = send (flnk->skt, msg->data + ulSend, (int)(msg->ulDataBufLen - ulSend), 0)) <= 0) {
                __xf_errno = XFILE_ERR_LINKLOST;
                XFILE_PRNT ("\r\n[XFILE][DATA] fail to send bytes (%d of %d), return %d", ulSend, msg->ulDataBufLen, r);
                return XFILE_ERR;
            }
            ulSend += r;
        }

        XFILE_ASSERT (ulSend == msg->ulDataBufLen);
    }

    return XFILE_OK;
}

static int xfile_recv (XFILELINK *flnk, XFILE_MSG *msg)
{
    int r;
    int ulRecv;

    if ((r = recv (flnk->skt, (char *)msg, sizeof(XFILE_MSG), 0)) <= 0)
    {
        __xf_errno = XFILE_ERR_LINKLOST;
        XFILE_PRNT ("\r\n[XFILE][MSG] fail to recv %d bytes, return %d", sizeof(XFILE_MSG), r);
        return XFILE_ERR;
    }
    XFILE_ASSURE (r == (int)sizeof(XFILE_MSG), XFILE_ERR, r);
    
    msg->f            = (void *)XFILE_NTOHL((unsigned long)msg->f);
    msg->ulCmdType    = XFILE_NTOHL(msg->ulCmdType);
    msg->ulDataBufLen = XFILE_NTOHL(msg->ulDataBufLen);
    msg->ulRetInfo    = XFILE_NTOHL(msg->ulRetInfo);

    if (XFILE_OPEN != msg->ulCmdType)
    {
        msg->stDummy.ul[0] = XFILE_NTOHL(msg->stDummy.ul[0]);
        msg->stDummy.ul[1] = XFILE_NTOHL(msg->stDummy.ul[1]);
    }

    if (msg->ulDataBufLen)
    {
        msg->data = XFILE_MALLOC((size_t )msg->ulDataBufLen);
        XFILE_ASSURE (NULL != msg->data, XFILE_ERR, msg->ulDataBufLen);

        ulRecv = 0;
        for (; ulRecv < (int)msg->ulDataBufLen;)
        {
            if ((r = recv (flnk->skt, msg->data + ulRecv, msg->ulDataBufLen - ulRecv, 0)) <= 0)
            {
                __xf_errno = XFILE_ERR_LINKLOST;
                XFILE_FREE(msg->data);
                msg->data = NULL;
                XFILE_PRNT ("\r\n[XFILE][DATA] fail to recv bytes (%d of %d), return %d", ulRecv, msg->ulDataBufLen, r);
                return XFILE_ERR;
            }
            ulRecv += r;
        }

        XFILE_ASSERT (ulRecv == msg->ulDataBufLen);
    }

    return XFILE_OK;
}

// #ifdef WIN32 /* XXX: vxWorks ??? */

///////////////////////////////////////////////////////////////////////////////////////////////
// server side
static void xfile_settitle ()
{
    char title[256] = {0};
    char cwd[MAX_PATH + 2] = {0};

    _getcwd (cwd, MAX_PATH);
    sprintf (title, "XFILE on %s:%d at %s, %u opened %u closed", 
            __xf_svr_ipaddr, __xf_svr_port, cwd, __xf_opened_num, __xf_closed_num);
#ifdef WIN32
    SetConsoleTitle (title);
#else
    printf ("%s", title);
#endif
}

static XFILE *xfile_new ()
{
    XFILE *xf;

    if (NULL == (xf = XFILE_MALLOC(sizeof(XFILE))))
    {
        return NULL;
    }
    xf->next = NULL;
    xf->prev = NULL;

    if (NULL != __xf_opened)
    {
        xf->next = __xf_opened;
        __xf_opened->prev = xf;
    }
    __xf_opened = xf;

    return xf;
}

static XFILE *xfile_find (void *f)
{
    XFILE *xf = __xf_opened;
    while (NULL != xf)
    {
        if (xf->f == f)
        {
            return xf;
        }
        xf = xf->next;
    }
    return NULL;
}

static void xfile_remove (XFILE *xf)
{
    if (__xf_opened == xf)
    {
        __xf_opened = __xf_opened->next;
        if (NULL != __xf_opened && NULL != __xf_opened->next)
        {
            __xf_opened->next->prev = NULL;
        }
        return;
    }

    if (NULL == xf->next)
    {
        xf->prev->next = NULL;
        return;
    }

    xf->prev->next = xf->next;
    xf->next->prev = xf->prev;
}

static void xfile_delete (XFILE *xf)
{
    xfile_remove (xf);
    XFILE_FREE (xf);
}

static int xfile_proc (XFILELINK *flnk, XFILE_MSG *msg)
{
    int ret = XFILE_OK;
    int c;
    int i;
    //unsigned int uiOsRet;
    char *buf = NULL;
    // unsigned long ulFileId;
    char szTmpBuf[1024];
    time_t tm;
    XFILE *xf;

    msg->ulRetInfo = XFILE_OK;
    msg->ulDataBufLen = 0;

    switch (msg->ulCmdType & 0xFF)
    {
        case XFILE_HELLO:
            sprintf (szTmpBuf, "Welcome @%08X@", flnk->skt);
            msg->ulDataBufLen = (unsigned int)strlen(szTmpBuf) + 1;
            msg->data = szTmpBuf;
            break;

        case XFILE_CWD:
            msg->ulRetInfo = (unsigned int) _chdir(msg->data);
            XFILE_INFO (XFILE_CWD, "Change working directory to '%s', code %d\r\n", 
                    msg->data, msg->ulRetInfo);
            xfile_settitle();
            break;

        case XFILE_PWD:
            if ((msg->ulRetInfo = (unsigned int)_getcwd (szTmpBuf, MAX_PATH)))
            {
                msg->ulDataBufLen = (unsigned int)strlen(szTmpBuf) + 1;
                msg->data = szTmpBuf;
            }
            break;

        case XFILE_TELL:
            // msg->ulRetInfo = _tell(msg->f);
            msg->ulRetInfo = ftell(msg->f);
            break;

        case XFILE_SEEK:
            if (NULL == (xf = xfile_find (msg->f)))
            {
                XFILE_INFO (XFILE_READ, "Read fail, file is %p", msg->f);
                break;
            }
            // if (XFILE_PROCESS & xf->mode)
            if ('p' == xf->mode[0])
            {
                xf->filepos = msg->stSeek.lOffset;
            }
            else
            {
                // msg->ulRetInfo = _lseek(xf->f, msg->stSeek.lOffset, msg->stSeek.lOrigin);
                msg->ulRetInfo = fseek(xf->f, msg->stSeek.lOffset, msg->stSeek.lOrigin);
            }
            break;

        case XFILE_FLUSH:
            if (NULL == (xf = xfile_find (msg->f)))
            {
                break;
            }
            if ('p' != xf->mode[0])
            {
                // msg->ulRetInfo = _commit(msg->f);
                msg->ulRetInfo = fflush(msg->f);
            }
            break;

        case XFILE_OPEN:
            // if (XFILE_PROCESS & msg->stOpen.ulMode)
            szTmpBuf[0] = 0;
            if ('p' == msg->stOpen.szMode[0])
            {
                #ifdef WIN32
                msg->ulRetInfo = (unsigned int) OpenProcess (PROCESS_VM_READ, FALSE, (DWORD)atoi(msg->data));
                #else
                msg->ulRetInfo = 1;
                #endif
                if (0 == msg->ulRetInfo)
                {
                    msg->ulRetInfo = XFILE_INVALID_FID;
                }
                strcpy (szTmpBuf, "PROCESS");
            }
            else
            {
                // msg->ulRetInfo = (unsigned int) _open(msg->data, msg->stOpen.ulMode);
                if (0 != (msg->ulRetInfo = (unsigned int) fopen(msg->data, msg->stOpen.szMode)))
                {
                #ifdef WIN32
                    GetFullPathName (msg->data, MAX_PATH, szTmpBuf, NULL);
                #else
                    strcpy (szTmpBuf, msg->data);
                #endif
                }
            }
            XFILE_INFO (XFILE_OPEN, "Open file '%s' with mode '%s', code 0x%08X, err=%d\r\n", 
                    msg->data, msg->stOpen.szMode, msg->ulRetInfo, xfile_getlasterror());
            if (XFILE_INVALID_FID != msg->ulRetInfo)
            {
                msg->ulDataBufLen = (unsigned int)strlen(szTmpBuf) + 1;
                msg->data = szTmpBuf;
                if (NULL == (xf = xfile_new()))
                {
                    return XFILE_ERR;
                }
                xf->f = (void *) msg->ulRetInfo;
                strcpy (xf->mode, msg->stOpen.szMode);
                __xf_opened_num++;
                xfile_settitle();                
            }
            break;

        case XFILE_READ:
            msg->ulDataBufLen = 0;
            msg->data = NULL;
            if (NULL == (buf = XFILE_MALLOC(msg->stRead.ulReadBytes)))
            {
                XFILE_INFO (XFILE_READ, "Read fail, no enough memory (need %u bytes), fid is 0x%08X", msg->stRead.ulReadBytes, msg->f);
                break;
            }
            if (NULL == (xf = xfile_find (msg->f)))
            {
                XFILE_INFO (XFILE_READ, "Read fail, fid is 0x%08X", msg->f);
                break;
            }
            // if (XFILE_PROCESS & xf->mode)
            if ('p' == xf->mode[0])
            {
                /* process read must be paired with seek */
                #ifdef WIN32
                if (0 == ReadProcessMemory ((HANDLE)xf->f, (LPCVOID)xf->filepos, buf, msg->stRead.ulReadBytes, &msg->ulDataBufLen))
                {
                    msg->ulDataBufLen = 0;
                }
                #else
                /* copy memory directly */
                (void *) memcpy (buf, (void *)xf->filepos, msg->stRead.ulReadBytes);
                #endif
            }
            else
            {
                // msg->ulRetInfo = read (msg->f, buf, msg->stRead.ulReadBytes);
                msg->ulRetInfo = (unsigned int)fread (buf, 1, msg->stRead.ulReadBytes, msg->f);
                msg->ulDataBufLen = msg->ulRetInfo;
            }
            //msg->ulDataBufLen = msg->ulRetInfo;
            msg->data = buf;
            XFILE_INFO (XFILE_READ, "Read from file 0x%08X with %u bytes, return %d bytes, err %u\r\n", 
                    msg->f, msg->stRead.ulReadBytes, msg->ulRetInfo, xfile_getlasterror());
            break;

        case XFILE_READLINE:
            msg->ulDataBufLen = 0;
            msg->data = NULL;
            if (NULL == (xf = xfile_find (msg->f)))
            {
                XFILE_INFO (XFILE_READ, "Read fail, fid is 0x%08X", msg->f);
                break;
            }
            if ('p' != xf->mode[0])
            {
                if (feof((FILE*)msg->f))
                {
                    msg->ulDataBufLen = XFILE_ERR;
                    break;
                }
                if (NULL == (buf = XFILE_MALLOC(msg->stRead.ulReadBytes + 1)))
                {
                    XFILE_INFO (XFILE_READ, "Read line fail, no enough memory (need %u bytes), fid is 0x%08X", msg->stRead.ulReadBytes, msg->f);
                    break;
                }

                for (i = 0; i < (int)msg->stRead.ulReadBytes; i++)
                {
                    c = fgetc(msg->f);
                    if (EOF == c || '\n' == c)
                    {
                        break;
                    }
                    buf[i] = c;
                }
                buf[i] = '\0';
                msg->data = buf;
                msg->ulRetInfo = i;
                msg->ulDataBufLen = i;
                XFILE_INFO (XFILE_READ, "Read line from file 0x%08X with %u bytes, return %d\r\n", 
                    msg->f, msg->stRead.ulReadBytes, msg->ulRetInfo);
            }
            break;

        case XFILE_WRITE:
            // msg->ulRetInfo = _write(msg->f, msg->data, msg->stWrite.ulWriteBytes);
            msg->ulRetInfo = (unsigned int)fwrite (msg->data, 1, msg->stWrite.ulWriteBytes, msg->f);
            XFILE_INFO (XFILE_WRITE, "Write to file 0x%08X with %u bytes, return %d\r\n", 
                    msg->f, msg->stWrite.ulWriteBytes, msg->ulRetInfo);
            break;

        case XFILE_EOF:
            // msg->ulRetInfo = _eof(msg->f);
            msg->ulRetInfo = feof((FILE*)msg->f);
            break;

        case XFILE_CLOSE:
            if (NULL != (xf = xfile_find (msg->f)))
            {
                if ('p' == xf->mode[0])
                {
                    #ifdef WIN32
                    CloseHandle ((HANDLE)xf->f);
                    #endif
                    msg->ulRetInfo = 0;
                }
                else
                {
                    msg->ulRetInfo = fclose(xf->f);
                }
                xfile_delete (xf);
            }

            if (0 == msg->ulRetInfo)
            {
                __xf_closed_num++;
                xfile_settitle();
            }
            XFILE_INFO (XFILE_CLOSE, "Close file 0x%08X, code %d\r\n", msg->f, msg->ulRetInfo);
            break;

        case XFILE_SIZE:
            msg->ulRetInfo = 0; // _filelength(msg->fid); TODO
            break;

        case XFILE_TIMESTAMP:
            tm = time (NULL);
            msg->stDummy.ul[1] = (unsigned int)(((unsigned long long)tm) >> 32);
            msg->stDummy.ul[0] = (unsigned int)(tm & 0x00000000FFFFFFFF);
            break;

        default:
            ret = XFILE_ERR;
            break; 
    }
    
    /* send ack */
    if (flnk && !(msg->ulCmdType & XFILE_MSG_NOACK))
    {
        ret = xfile_send(flnk, msg);
    }

    if (NULL != buf)
    {
        XFILE_FREE(buf);
    }

    return ret;
}

static unsigned int XFILE_STDCALL xfile_task (void *lpData)
{
    XFILELINK *flnk;
    XFILE_MSG msg = {0};
    char *data;
    // int t;

    __xf_opened = NULL;
    if (NULL == (flnk = XFILE_MALLOC(sizeof(XFILELINK))))
    {
        return 1;
    }
    flnk->skt = (unsigned int) lpData;

    // t = 4;
    // getsockopt (s, SOL_SOCKET, SO_RCVBUF, (char *)&xf->ulRcvBufLen, &t);
    // getsockopt (s, SOL_SOCKET, SO_SNDBUF, (char *)&xf->ulSndBufLen, &t);
    // printf ("\r\nSO_RCVBUF = %u, SO_RCVBUF = %u", xf->ulSndBufLen, xf->ulRcvBufLen);

    for (;;)
    {
        // data = NULL;
        memset (&msg, 0, sizeof(XFILE_MSG));
        if (XFILE_OK != xfile_recv (flnk, &msg))
        {
            if (msg.data)
            {
                XFILE_FREE(msg.data);
            }
            break;
        }

        /* keep the orignal data ptr */
        data = msg.data;
        xfile_proc (flnk, &msg);

        if (data)
        {
            XFILE_FREE(data);
        }
    }

    XFILE_INFO (0, "Client %08X logout\r\n", flnk->skt);
    XFILE_FREE(flnk);
    return 0;
}

int xfile_start (char *szXfHost, unsigned short usPort)
{
    int r;
    int s;
    unsigned int ulThreadId;
    struct sockaddr_in addr_s = {0};
    struct sockaddr_in addr_c = {0};
    int addrlen;

#ifdef WIN32
    WSADATA wsaData;
    // Initialize Winsock
    r = WSAStartup(MAKEWORD(2,2), &wsaData);
    XFILE_ASSERT (r == 0);
#endif

    if((__skt_s = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        return XFILE_ERR;
    }

    addr_s.sin_family = AF_INET;
    addr_s.sin_port = htons(usPort);
    addr_s.sin_addr.s_addr= inet_addr(szXfHost);
    //addr_s.sin_addr.s_addr = htonl(INADDR_ANY);

    r = bind (__skt_s, (struct sockaddr *)&addr_s, sizeof(struct sockaddr_in));
    if (r != 0)
    {
        XFILE_INFO (0, "Can't launch XFILE on %s:%u\r\n", szXfHost, usPort);
        return XFILE_ERR;
    }

    r = listen (__skt_s, SOMAXCONN);
    XFILE_ASSERT (r == 0);

    strcpy (__xf_svr_ipaddr, szXfHost);
    __xf_svr_port = (int) usPort;
    xfile_settitle();

    XFILE_INFO (0, "XFILE is listening on %s:%u\r\n", szXfHost, usPort);
    for (;;)
    {
        addrlen = sizeof(struct sockaddr_in);
        if ((s = (int)accept (__skt_s, (struct sockaddr *)&addr_c, (socklen_t *)&addrlen)) <= 0)
        // s = accept (__skt_s, 0, 0);
        // if (0 == s)
        {
            XFILE_INFO (0, "Error (%d) occurs during accepting new client\r\n", s);
            break;
        }

        XFILE_INFO (0, "New user comming in: %08X, %08X", s, addr_c.sin_addr.s_addr);
        ulThreadId = 0;
        (void)ulThreadId;
#if defined(WIN32)
        CreateThread (NULL, 0x4000, xfile_task, (void *)s, 0, &ulThreadId);
        printf (" with thread %u\r\n", ulThreadId);
#elif defined(NSE_PLATFORM_LINUX)
        (void)xfile_task;
#else
        (VOID) taskSpawn("tXFILE", 80, 0, 64*1024, (FUNCPTR) xfile_task, s, 0, 0, 0, 0, 0, 0, 0, 0, 0);
#endif
    }

#ifdef WIN32
    (VOID) closesocket (__skt_s);
#else
    (VOID) close (__skt_s);
#endif

    return XFILE_OK;
}
// #endif /* WIN32 */

///////////////////////////////////////////////////////////////////////////////////////////////
// client side
XFILELINK *xfile_link (char* pszIpAddr, int ulPort)
{
    XFILELINK *flnk;
    struct sockaddr_in addr = {0};
    NSE_SOCKET s;
    int r;

    if((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        XFILE_PRNT ("\r\n[XFILE] create socket fail (%d).", s);
        return NULL;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)ulPort);
    addr.sin_addr.s_addr= inet_addr(pszIpAddr);
    if ((r = connect(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) < 0)
    {
        XFILE_PRNT ("\r\n[XFILE] fail to connect target %s:%d (%d).", pszIpAddr, ulPort, r);
        return NULL;
    }

    if (NULL == (flnk = XFILE_MALLOC(sizeof(XFILELINK))))
    {
        return NULL;
    }

    flnk->skt = s;

    __flnk = flnk;
    __xf_port = ulPort;
    strcpy (__xf_ipaddr, pszIpAddr);

    // xfile_synctime();

    return flnk;
}

int xfile_chklink ()
{
    if (NULL == __flnk)
    {
        return XFILE_ERR;
    }

    return XFILE_OK;
}

int xfile_downlink (XFILELINK *flnk)
{
    if (NULL == flnk)
    {
        return XFILE_OK;
    }

#ifdef WIN32
    (void) closesocket (flnk->skt);
#else
    (void) close (flnk->skt);
#endif
    XFILE_FREE (flnk);
    __flnk = NULL;

    return XFILE_OK;
}

static int xfile_exec (XFILELINK *flnk, XFILE_MSG *msg)
{
#if 0 && defined(WIN32)
    if (NULL == flnk)
    {
        /* 如果没有TCP连接，则直接进行文件操作 */
        return xfile_proc (NULL, msg);
    }
#endif
    __xf_errno = 0;
    if (XFILE_OK != xfile_send (flnk, msg))
    {
        return XFILE_ERR;
    }

    if (msg->ulCmdType & XFILE_MSG_NOACK)
    {
        return XFILE_OK;
    }

    if (XFILE_OK != xfile_recv (flnk, msg))
    {
        return XFILE_ERR;
    }

    return XFILE_OK;
}

int xfile_hello (XFILELINK *flnk)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_HELLO;

    if (XFILE_OK != xfile_exec (flnk, &msg))
    {
        return XFILE_ERR;
    }
    
    printf ("\r\nHELLO: %s", msg.data);

    return XFILE_OK;
}

int xfile_chdirEx (XFILELINK *flnk, char *path)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_CWD;
    msg.ulDataBufLen = (int)strlen(path) + 1;
    msg.data = path;

    if (XFILE_OK != xfile_exec (flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;
}

int xfile_chdir (char *path)
{
    if (NULL == __flnk)
    {
        return XFILE_ERR;
    }

    return xfile_chdirEx(__flnk, path);
}


int xfile_pwdEx (XFILELINK *flnk, char *path, int maxlen)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_PWD;

    if (XFILE_OK != xfile_exec (flnk, &msg))
    {
        return XFILE_ERR;
    }

    if (msg.ulRetInfo && msg.data)
    {
        memcpy (path, msg.data, (unsigned int)MIN(maxlen, (int)msg.ulDataBufLen));
        XFILE_FREE(msg.data);
    }

    return XFILE_OK;    
}

//int xfile_filepath (char* file, char *path, int maxlen)
//{
//    XFILE_MSG msg = {0};
//    msg.ulCmdType = XFILE_FULLPATH;
//    msg.data = path;
//    msg.ulDataBufLen = strlen(path) + 1;
//
//    if (XFILE_OK != xfile_exec (__flnk, &msg))
//    {
//        return XFILE_ERR;
//    }
//
//    if (msg.ulRetInfo && msg.data)
//    {
//        memcpy (path, msg.data, (unsigned int)MIN(maxlen, (int)msg.ulDataBufLen));
//        XFILE_FREE(msg.data);
//    }
//
//    return XFILE_OK;    
//}

int xfile_pwd (char *path, int maxlen)
{
    if (NULL == __flnk)
    {
        return XFILE_ERR;
    }

    return xfile_pwdEx(__flnk, path, maxlen);    
}

int xfile_tell (XFILE *xf)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_TELL;
    msg.f = xf->f;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;    
}

int xfile_size (XFILE *xf)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_SIZE;
    msg.f = xf->f;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;    
}

int xfile_seek (XFILE *xf, int offset, int origin)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_SEEK;
    msg.f = xf->f;
    msg.stSeek.lOffset = offset;
    msg.stSeek.lOrigin = origin;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;    
}

int xfile_flush (XFILE *xf)
{
    XFILE_MSG msg = {0};

    if (NULL == xf)
    {
        return XFILE_ERR;
    }

    if (xf->bufpos > 0)
    {
        /* flush buffer */
        msg.ulCmdType = XFILE_WRITE;
        msg.f = xf->f;
        msg.stWrite.ulWriteBytes = xf->bufpos;
        msg.data = xf->buf;
        msg.ulDataBufLen = (int) xf->bufpos;

        if (XFILE_OK != xfile_exec (xf->flnk, &msg))
        {
            return XFILE_ERR;
        }

        xf->bufpos = 0;
    }

    msg.ulCmdType = XFILE_FLUSH;
    msg.f = xf->f;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;    
}

int xfile_eof (XFILE *xf)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_EOF;
    msg.f = xf->f;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    return msg.ulRetInfo;    
}

XFILE *xfile_openEx (XFILELINK *flnk, char *pszFile, char *szModule)
{
    XFILE_MSG msg = {0};
    XFILE *xf;

    msg.ulCmdType = XFILE_OPEN;
    (VOID) strncpy (msg.stOpen.szMode, szModule, 7);
    msg.ulDataBufLen = (size_t)strlen(pszFile) + 1;
    msg.data = pszFile;

    if (XFILE_OK != xfile_exec (flnk, &msg))
    {
        return NULL;
    }

    if (XFILE_INVALID_FID == msg.ulRetInfo)
    {
        __xf_errno = XFILE_ERR_FILE_NOTEXIST;
        return NULL;
    }

    if (NULL == (xf = XFILE_MALLOC(sizeof(XFILE))))
    {
        return NULL;
    }

    (VOID) memset (xf, 0, sizeof(XFILE));
    xf->f = (void *) msg.ulRetInfo;
    xf->flnk = flnk;
    xf->bufpos = 0;
    strncpy (xf->path, msg.data, XFILE_MAX_PATH);
    XFILE_FREE (msg.data);

    return xf;
}

XFILE *xfile_open (char *pszFile, char *szMode)
{
    if (NULL == __flnk)
    {
        return NULL;
    }

    return xfile_openEx (__flnk, pszFile, szMode);
}

int xfile_readEx (XFILE *xf, char *buf, unsigned int count, unsigned int cmdtype)
{
    XFILE_MSG msg = {0};
    msg.ulCmdType = cmdtype;
    msg.f = xf->f;
    msg.stRead.ulReadBytes = count;
    msg.data = NULL;
    msg.ulDataBufLen = 0;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    if (msg.ulDataBufLen > 0 && NULL != msg.data)
    {
        memcpy (buf, msg.data, (unsigned int)msg.ulDataBufLen);
        XFILE_FREE(msg.data);
    }

    return msg.ulDataBufLen;
}

int xfile_read (XFILE *xf, char *buf, unsigned int count)
{
    return xfile_readEx (xf, buf, count, XFILE_READ);
}

int xfile_readline (XFILE *xf, char *buf, unsigned int count)
{
    return xfile_readEx (xf, buf, count, XFILE_READLINE);
}

int xfile_write (XFILE *xf, char *buf, unsigned int count)
{
    int wr;
    unsigned int n;
    XFILE_MSG msg = {0};

    if (NULL == xf || NULL == buf)
    {
        return XFILE_ERR;
    }
    if (0 == count)
    {
        return 0;
    }

    wr = 0;
    do
    {
        /* Write to inner buffer first if possible */
        if (xf->bufpos < XFILE_BUFSIZE)
        {
            n = MIN(count, XFILE_BUFSIZE - xf->bufpos);
            memcpy (&xf->buf[xf->bufpos], buf, n);
            xf->bufpos += n;
            count -= n;
            buf += n;
        }

        XFILE_ASSERT (xf->bufpos <= XFILE_BUFSIZE);

        if (xf->bufpos == XFILE_BUFSIZE)
        {
            /* flush buffer if full */
            msg.ulCmdType    = XFILE_WRITE;
            msg.f          = xf->f;
            msg.data         = xf->buf;
            msg.ulDataBufLen = XFILE_BUFSIZE; //(int)count;
            msg.stWrite.ulWriteBytes = XFILE_BUFSIZE; // count;

            if (XFILE_OK != xfile_exec (xf->flnk, &msg))
            {
                return XFILE_ERR;
            }

            xf->bufpos = 0;
            wr += msg.ulRetInfo;
        }
    } while(count > 0);

    return wr;
}

int xfile_print (XFILE *xf, char *fmt, ...)
{
    //int n;
    va_list vp;
    char szBuf[XFILE_PRINT_BUFSIZE + 1] = {0};
    va_start (vp, fmt); /*lint !e826 */
#ifdef SE5856EMU
    (VOID) vsprintf (szBuf, fmt, vp);
#else
    (VOID) _vsnprintf (szBuf, XFILE_PRINT_BUFSIZE, fmt, vp);
#endif
    va_end (vp);

    return xfile_write(xf, szBuf, (unsigned int)strlen(szBuf));
}

int xfile_close (XFILE *xf)
{
    XFILE_MSG msg = {0};

    if (NULL == xf)
    {
        return XFILE_ERR;
    }

    /* flush the remain */
    (void) xfile_flush (xf);

    msg.ulCmdType = XFILE_CLOSE | XFILE_MSG_NOACK;
    msg.f = xf->f;

    if (XFILE_OK != xfile_exec (xf->flnk, &msg))
    {
        return XFILE_ERR;
    }

    XFILE_FREE(xf);
    return XFILE_OK;
}

int xfile_hosttimestamp (char *buf)
{
    time_t tm;
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_TIMESTAMP;

    if (XFILE_OK != xfile_exec (__flnk, &msg))
    {
        return XFILE_ERR;
    }
    
	//tm = *(time_t *)&msg.stDummy.ul[0];
	tm = ((time_t)(((unsigned long long)msg.stDummy.ul[0] )<< 32)) | (time_t)msg.stDummy.ul[1];
#ifndef WIN32
    tm += (8 * 60 * 60); /* TODO: for vxworks only */
#endif
	//modified by guowang shi
    //(void) sprintf (buf, "%s", asctime(localtime(&tm)));
	(void)sprintf(buf, "%s", "0");
    buf[strlen(buf) - 1] = 0;

    return XFILE_OK;
}

int xfile_synctime()
{
    time_t tm;
    XFILE_MSG msg = {0};
    msg.ulCmdType = XFILE_TIMESTAMP;

    if (XFILE_OK != xfile_exec (__flnk, &msg))
    {
        return XFILE_ERR;
    }
    
    tm = *(time_t *)&msg.stDummy.ul[0];

#ifndef WIN32
    {
        struct timespec ts;
        struct tm *tnow;

        tm += (8 * 60 * 60); /* TODO: for vxworks only */
        (VOID) memset(&ts, 0, sizeof(struct timespec));
        (VOID) memcpy (&ts.tv_sec, &tm, sizeof(time_t));
        clock_settime(0, &ts);

        time(&tm);
        tnow = localtime(&tm);
        printf("\r\nTime on XFILE host is %d/%02d/%02d %02d:%02d:%02d.",
            1900+tnow->tm_year, tnow->tm_mon+1, tnow->tm_mday, tnow->tm_hour, tnow->tm_min, tnow->tm_sec);
    }
#endif
    (VOID) tm;
    return XFILE_OK;
}

int xfile_islinkalive (XFILELINK *flnk)
{
    return XFILE_OK == xfile_chklink() ? 1 : 0;
}

int xfile_errno ()
{
    if (NULL == __flnk)
    {
        return XFILE_ERR_NOT_INITIALIZE;
    }
    return __xf_errno;
}

#ifdef __XFILE_SVRSIDE__
int main (int argc, char **argv)
{
//#ifdef _DEBUG
//    argc = 3;
//    argv[0] = "xfile";
//    argv[1] = "169.254.83.11";
//    argv[2] = "88";
//#endif
    printf ("xfile - by SHANG/62185, build on %s %s\r\n", __DATE__, __TIME__);
    if (argc < 3)
    {
        printf ("ERROR: insufficient parameter! start me as 'xfile ip-addr port'\n");
        return 1;
    }

    return xfile_start(argv[1], (unsigned short) atoi(argv[2]));
}
#endif /* __XFILE_SVRSIDE__ */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */
