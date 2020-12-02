#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#pragma comment(lib, "User32.lib")

#include "ftp_tr.h"
#include "ftp_usr.h"
#include "ftp_sv.h"

ftp_tr* ftp_tr_new(ftp_usr* cl) {
    ftp_tr* tr = (ftp_tr*)calloc(1, sizeof(ftp_tr));
    if (tr == NULL)
        return NULL;

    tr->cl = cl;

    printf("(client %p) data connection openned\n", cl);

    return tr;
}

void tr_del(ftp_tr* tr) {
    printf("(client %p) data connection closed\n", tr->cl);

    if (tr->sendData)
        free(tr->sendData);

    if (tr->fd)
        CloseHandle(tr->fd);

    if (tr->sock)
        closesocket(tr->sock);

    if (tr->pasvSock)
        closesocket(tr->pasvSock);

    free(tr);
}

void tr_close(ftp_tr* tr) {
    tr->closing = 1;
}

int tr_marked_to_del(ftp_tr* tr) {
    return tr->closing;
}

int tr_pasv(ftp_tr* tr) {
    struct sockaddr_in addr;
    int addr_size = sizeof(struct sockaddr_in);
    if (getpeername(tr->cl->sock, (struct sockaddr*)&addr, &addr_size) < 0)
        return -1;
 
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    unsigned long NonBlock = 1;
    if (ioctlsocket(sock, FIONBIO, &NonBlock) == SOCKET_ERROR) {
        closesocket(sock);
        return -1;
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr = addr.sin_addr;
    sockaddr.sin_port = htons(0);
    if (bind(sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        closesocket(sock);
        return -1;
    }

    if (listen(sock, TR_LISTEN) < 0) {
        closesocket(sock);
        return -1;
    }

    struct sockaddr_in	cli_addr;

    int i = sizeof(cli_addr);
    if (getsockname(sock, (struct sockaddr*)&cli_addr, &i) < 0) {
        closesocket(sock);
        return -1;
    }

    int port = ntohs(cli_addr.sin_port);

    tr->pasvSock = sock;

    return port;
}

int tr_send_list(ftp_tr* tr, const char* dir) {
    int msize = 2048;
    int size = 0;
    char* buf = (char*)malloc(msize);
    if (buf == NULL)
        return -1;
    
    char d[MAX_PATH];
    snprintf(d, sizeof(d), "%s/*", dir);

    WIN32_FIND_DATAA FindFileData;
    HANDLE hf = FindFirstFileA(d, &FindFileData);
    if (hf == INVALID_HANDLE_VALUE) {
        free(buf);
        return -1;
    }

    do
    {
        char t[MAX_PATH + 200];

        if (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            snprintf(t, sizeof(t), "d--------- 1 ftp ftp              0 %s\r\n", FindFileData.cFileName);
        }
        else {
            LARGE_INTEGER filesize;
            filesize.LowPart = FindFileData.nFileSizeLow;
            filesize.HighPart = FindFileData.nFileSizeHigh;
            snprintf(t, sizeof(t), "---------- 1 ftp ftp              %lld %s\r\n", filesize.QuadPart, FindFileData.cFileName);
        }


        int len = (int) strlen(t);

        if (size + len >= msize) {
            do {
                msize = msize * 2;
            } while (size + len >= msize);

            void * b = realloc(buf, msize);
            if (b == NULL) {
                free(buf);
                FindClose(hf);
                return -1;
            }
            buf = (char*)b;
        }

        memcpy(buf + size, t, len);
        size += len;
    } while (FindNextFileA(hf, &FindFileData) != 0);
    FindClose(hf);

    buf[size] = '\0';

    tr->sendData = buf;
    tr->sendLen = size;
    tr->action = TrSending;

    return 0;
}

int tr_send_file(ftp_tr* tr, const char* name) {
    HANDLE hFile = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return -1;
    
    DWORD fileSize = GetFileSize(hFile, &fileSize);
    
    char* data = (char*)malloc(fileSize);
    if (data == NULL) {
        CloseHandle(hFile);
        return -1;
    }

    DWORD rad;
    if (FALSE == ReadFile(hFile, data, fileSize, &rad, NULL))
    {
        free(data);
        CloseHandle(hFile);
        return -1;
    }

    CloseHandle(hFile);

    tr->action = TrSending;
    tr->sendData = data;
    tr->sendLen = (unsigned int) fileSize;

    return 0;
}

int tr_recv_file(ftp_tr* tr, const char* name) {
    HANDLE hFile = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL)
        return -1;

    tr->action = TrReading;
    tr->fd = hFile;

    return 0;
}

void _tr_handle_send(ftp_tr* tr) {
    int res = send(tr->sock, tr->sendData + tr->sendSent, tr->sendLen - tr->sendSent, 0);
    if (res == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            tr_close(tr);
        }

        return;
    }

    tr->sendSent += res;
    if (tr->sendSent == tr->sendLen) {
        usr_send(tr->cl, "226 Successfully transferred");
        tr_close(tr);
    }
}

void _tr_handle_recv(ftp_tr* tr) {
    char buf[8192];

    int res = recv(tr->sock, buf, sizeof(buf), 0);
    if (res == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            tr_close(tr);
        }

        return;
    }

    if (res == 0) {
        usr_send(tr->cl, "226 Successfully transferred");
        tr_close(tr);
    }

    DWORD ret;
    WriteFile(tr->fd, buf, res, &ret, NULL);
    if (ret != res)
        tr_close(tr);
     
}

void _tr_handle_accept(ftp_tr* tr) {
    SOCKET sock = accept(tr->pasvSock, NULL, NULL);
    if (sock == INVALID_SOCKET) {
        tr_close(tr);
        return;
    }

    unsigned long NonBlock = 1;
    if (ioctlsocket(sock, FIONBIO, &NonBlock) == SOCKET_ERROR) {
        tr_close(tr);
        closesocket(sock);
        return;
    }

    closesocket(tr->pasvSock);
    tr->pasvSock = 0;

    tr->sock = sock;
}

void tr_select_set(ftp_tr* tr, FD_SET* ReadSet, FD_SET* WriteSet) {
    if (tr->pasvSock) {
        FD_SET(tr->pasvSock, ReadSet);
        return;
    }

    if (tr->action != TrNone)
        FD_SET(tr->sock, tr->action == TrReading ? ReadSet : WriteSet);
}

void tr_select_check(ftp_tr* tr, FD_SET* ReadSet, FD_SET* WriteSet) {
    if (tr->pasvSock) {
        if (FD_ISSET(tr->pasvSock, ReadSet)) {
            _tr_handle_accept(tr);
        }
        return;
    }

    if (tr->action != TrNone && FD_ISSET(tr->sock, tr->action == TrReading ? ReadSet : WriteSet)) {

        if (tr->action == TrSending) {
            _tr_handle_send(tr);
        }
        else if (tr->action == TrReading) {
            _tr_handle_recv(tr);
        }
    }
}
