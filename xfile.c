#include "xfile.h"

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

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

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define _getcwd getcwd
#define _chdir chdir
#define _vsnprintf vsnprintf

XFILE_IMPORT int __skt_s;

XFILE_IMPORT char __xf_svr_ipaddr[80];
XFILE_IMPORT int  __xf_svr_port;
XFILE_IMPORT int  __xf_opened_num;
XFILE_IMPORT int  __xf_closed_num;

char *xfile_timestamp(char *tsbuf, int len)
{
    char tmstr[80];    
    struct tm   *newTime;
    time_t      szClock;

    (void)time(&szClock);
    if (NULL == (newTime = localtime(&szClock))) {
        return "";
    }

    (void)sprintf(tmstr, "%s", asctime(newTime));
    tmstr[strlen(tmstr) - 1] = 0;
    (void)strncpy(tsbuf, tmstr, len);
    tsbuf[len - 1] = '\0';

    return tsbuf;
}

static void xfile_dbg(int cmd, char *fmt, ...)
{
    va_list vp;
    char szBuf[XFILE_PRINT_BUFSIZE + 1] = {0};
    char szTs[80];

    va_start(vp, fmt);
    (void)vsprintf(szBuf, fmt, vp);
    va_end(vp);
    (void)printf("%s - %s", xfile_timestamp(szTs, 79), szBuf);
}

static void xfile_settitle()
{
    char title[256] = {0};
    char cwd[MAX_PATH + 2] = {0};

    _getcwd(cwd, MAX_PATH);
    sprintf(title, "XFILE on %s:%d at %s, %u opened %u closed", 
            __xf_svr_ipaddr, __xf_svr_port, cwd, __xf_opened_num, __xf_closed_num);
    printf ("%s", title);
}

int xfile_start(char *szXfHost, unsigned short usPort)
{
    int r;
    int s;
    int addrlen;
    struct sockaddr_in addr_s = {0};
    struct sockaddr_in addr_c = {0};

    __skt_s = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (__skt_s < 0) {
        return XFILE_ERR;
    }

    addr_s.sin_family = AF_INET;
    addr_s.sin_port = htons(usPort);
    addr_s.sin_addr.s_addr= inet_addr(szXfHost);
    //addr_s.sin_addr.s_addr = htonl(INADDR_ANY);

    r = bind(__skt_s, (struct sockaddr *)&addr_s, sizeof(struct sockaddr_in));
    if (r != 0) {
        XFILE_INFO(0, "Can't launch XFILE on %s:%u\r\n", szXfHost, usPort);
        return XFILE_ERR;
    }

    r = listen(__skt_s, SOMAXCONN);
    XFILE_ASSERT(r == 0);

    strcpy(__xf_svr_ipaddr, szXfHost);
    __xf_svr_port = (int)usPort;
    xfile_settitle();

    XFILE_INFO(0, "XFILE is listening on %s:%u\r\n", szXfHost, usPort);
    for (;;) {
        addrlen = sizeof(struct sockaddr_in);
        s = (int)accept (__skt_s, (struct sockaddr *)&addr_c, (socklen_t *)&addrlen);
        if (s <= 0) {
            XFILE_INFO(0, "Error (%d) occurs during accepting new client\r\n", s);
            break;   
        }
    }

    (void)close(__skt_s);
}

#ifdef __XFILE_SVRSIDE__

int main(int argc, char **argv)
{
    printf ("xfile - by zhouyuming, build on %s %s\r\n", __DATE__, __TIME__);
    if (argc < 3) {
        printf ("ERROR: insufficient parameter! start me as 'xfile ip-addr port'\n");
        return 1;
    }

    return xfile_start(argv[1], (unsigned short)atoi(argv[2]));
}

#endif /* __XFILE_SVRSIDE__ */

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* __cplusplus */