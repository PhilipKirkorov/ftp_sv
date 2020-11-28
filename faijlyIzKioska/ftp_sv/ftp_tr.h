#pragma once

#include <winsock2.h>

typedef struct ftp_sv ftp_sv;
typedef struct ftp_usr ftp_usr;

#define TR_LISTEN 10

typedef struct ftp_tr {
    ftp_usr* cl;
    int closing;
    SOCKET pasvSock;
    SOCKET sock;
    int action;
    char* sendData;
    int sendSent;
    int sendLen;
    FILE* fd;
} ftp_tr;

ftp_tr* ftp_tr_new(ftp_usr* cl);
void tr_close(ftp_tr* tr);
int tr_marked_to_del(ftp_tr* tr);
void tr_del(ftp_tr* tr);
int tr_pasv(ftp_tr* tr);
int tr_send_list(ftp_tr* tr, const char* dir);
int tr_send_file(ftp_tr* tr, const char* name);
int tr_recv_file(ftp_tr* tr, const char* name);
void tr_select_set(ftp_tr* tr, FD_SET* ReadSet, FD_SET* WriteSet);
void tr_select_check(ftp_tr* tr, FD_SET* ReadSet, FD_SET* WriteSet);