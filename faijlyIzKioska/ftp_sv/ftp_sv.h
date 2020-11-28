#pragma once

#include <winsock2.h>

#define SV_MAX_CLIENTS 128
#define SV_MAX_ABS_PATH 512
#define SV_LISTEN 10

typedef struct ftp_usr ftp_usr;

typedef struct ftp_sv {
    SOCKET sock;

    ftp_usr* clients[SV_MAX_CLIENTS];
    int clientsCount;

    char ip[4];
    char path[SV_MAX_ABS_PATH];
} ftp_sv;

ftp_sv* ftp_sv_new();
void sv_stop(ftp_sv* sv);
int sv_start(ftp_sv* sv, short port, const char* addr, const char* hostip, const char *path);
int sv_tick(ftp_sv* sv);