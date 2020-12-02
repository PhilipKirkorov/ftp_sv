#pragma once

#include <winsock2.h>

typedef struct ftp_sv ftp_sv;
typedef struct ftp_tr ftp_tr;

#define CL_MAX_CMD 1024
#define CL_MAX_RESP 1024
#define CL_MAX_PATH 128
#define CL_SEND_BUF 2048

typedef struct ftp_usr {
    SOCKET sock;
    ftp_sv* sv;
    int closing;

    char dir[CL_MAX_PATH];

    char sendBuf[CL_SEND_BUF];
    int sendLen;

    ftp_tr* tr;
} ftp_usr;


int usr_parse_path(ftp_usr* cl, char* arg, char* d);
ftp_usr* ftp_usr_new(SOCKET sock, ftp_sv* sv);
void usr_del(ftp_usr* cl);
void usr_send(ftp_usr* cl, const char* data);
void usr_close(ftp_usr* cl);
int usr_marked_to_del(ftp_usr* cl);
void usr_select_set(ftp_usr* cl, FD_SET* ReadSet, FD_SET* WriteSet);
void usr_select_check(ftp_usr* cl, FD_SET* ReadSet, FD_SET* WriteSet);
