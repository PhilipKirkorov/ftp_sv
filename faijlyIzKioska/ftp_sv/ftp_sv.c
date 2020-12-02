#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>

#include "ftp_sv.h"
#include "ftp_usr.h"
#include <stdio.h>

ftp_sv* ftp_sv_new() {
    ftp_sv* sv = (ftp_sv*)calloc(1, sizeof(ftp_sv));
    if (sv == NULL)
        return NULL;

    return sv;
}

void sv_stop(ftp_sv* sv) {
    for (int i = 0; i < sv->clientsCount; i++) {
        usr_del(sv->clients[i]);
    }

    if (sv->sock) {
        closesocket(sv->sock);
    }

    free(sv);
}

int sv_start(ftp_sv* sv, short port, const char* addr, const char* hostip, const char * path) {
    size_t len = strlen(path);
    if (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\'))
        len--;

    if (len >= sizeof(sv->path))
        return -1;
    memcpy(sv->path, path, len);
    
    if (!sscanf_s(hostip, "%hhd.%hhd.%hhd.%hhd", &sv->ip[0], &sv->ip[1], &sv->ip[2], &sv->ip[3]))
        return -1;
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    unsigned long NonBlock = 1;
    if (ioctlsocket(sock, FIONBIO, &NonBlock) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = inet_addr(addr);
    sockaddr.sin_port = htons(port);
    if (bind(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        closesocket(sock);
        return -1;
    }

    if (listen(sock, SV_LISTEN) < 0) {
        closesocket(sock);
        return -1;
    }

    sv->sock = sock;

    return 1;
}

void _sv_handle_accept(ftp_sv* sv) {
    SOCKET sock = accept(sv->sock, NULL, NULL);
    if (sock == INVALID_SOCKET)
        return;

    if (sv->clientsCount == SV_MAX_CLIENTS) {
        closesocket(sock);
        return;
    }

    unsigned long NonBlock = 1;
    if (ioctlsocket(sock, FIONBIO, &NonBlock) == SOCKET_ERROR) {
        closesocket(sock);
        return;
    }

    ftp_usr* cl = ftp_usr_new(sock, sv);
    if (cl == NULL) {
        closesocket(sock);
        return;
    }

    sv->clients[sv->clientsCount] = cl;
    sv->clientsCount++;
}

int sv_tick(ftp_sv* sv) {
    if (!sv->sock)
        return -1;

    FD_SET WriteSet;
    FD_SET ReadSet;

    FD_ZERO(&ReadSet);
    FD_ZERO(&WriteSet);

    for (int i = 0; i < sv->clientsCount; i++) {
        usr_select_set(sv->clients[i], &ReadSet, &WriteSet);
    }

    FD_SET(sv->sock, &ReadSet);

    TIMEVAL Timeout;
    Timeout.tv_usec = 0;
    Timeout.tv_sec = 0;

    int res = select(0, &ReadSet, &WriteSet, NULL, &Timeout);
    if (res <= 0) {
        return res;
    }

    for (int i = 0; i < sv->clientsCount; i++) {
        usr_select_check(sv->clients[i], &ReadSet, &WriteSet);
    }

    int d = 0;
    for (int i = 0; i < sv->clientsCount; i++) {
        if (usr_marked_to_del(sv->clients[i])) {
            usr_del(sv->clients[i]);
            d++;
        }
        else if (d) {
            sv->clients[i - d] = sv->clients[i];
        }
    }
    sv->clientsCount -= d;

    if (FD_ISSET(sv->sock, &ReadSet)) {
        _sv_handle_accept(sv);
    }

    return 1;
}
