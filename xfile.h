/*
 * xfile.h
 *
 */

#ifndef __XFILE_H__
#define __XFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#define XFILE_OK                    0
#define XFILE_ERR                   -1
#define XFILE_INVALID_FID           0
#define XFILE_ERR_LINKLOST          -2
#define XFILE_ERR_FILE_NOTEXIST     -3
#define XFILE_ERR_NOT_INITIALIZE    -4

#define XFILE_ASSERT(expr)          {if (!(expr)) {(void)printf("\r\nASSERT FAIL: " #expr ", at %s:%d", __FILE__, __LINE__);}}

#define XFILE_PRINT_BUFSIZE         1023

#define XFILE_INFO xfile_dbg

#ifdef __cplusplus
}
#endif

#endif /* __XFILE_H__ */