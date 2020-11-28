#include <stdio.h>

#include "ftp_usr.h"
#include "ftp_tr.h"
#include "ftp_sv.h"

void _usr_on_cmd(ftp_usr*, char*, char*);

int usr_parse_path(ftp_usr* cl, char* arg, char* d, int dsize) {
    if (arg == NULL) {
        return -1;
    }

    if (arg[0] == '/') {
        d[0] = '/';
        d[1] = '\0';
        arg++;
    }
    else {
        snprintf(d, dsize, cl->dir);
    }

    int len = strlen(arg);
    if (arg[len - 1] == '/' || arg[len - 1] == '\\')
        len--;

    char* ws = arg;
    for (int i = 0; i <= len; i++) {
        if (
            *arg == ':' ||
            *arg == '*' ||
            *arg == '?' ||
            *arg == '"' ||
            *arg == '<' ||
            *arg == '>' ||
            *arg == '|')
        {
            return -1;
        }

        if (*arg == '\\' || *arg == '/' || *arg == '\0') {
            int sz = arg - ws;

            if (sz == 0)
                return -1;


            if (ws[0] == '.' && ws[1] == '.' && sz == 2) {
                int l = strlen(d);
                for (int i = l - 1; i >= 0; i--) {
                    if (d[i] == '/') {
                        d[i == 0 ? 1 : i] = '\0';
                        break;
                    }
                }
            }
            else {
                snprintf(d, dsize, d[1] == '\0' ? "%s%.*s" : "%s/%.*s", d, sz, ws);
            }

            ws = arg + 1;
        }

        arg++;
    }

    return 0;
}

ftp_usr* ftp_usr_new(SOCKET sock, ftp_sv* sv) {
    ftp_usr* cl = (ftp_usr*)malloc(sizeof(ftp_usr));
    if (cl == NULL)
        return NULL;

    memset(cl, 0, sizeof(ftp_usr));
    cl->sock = sock;
    cl->sv = sv;
    cl->dir[0] = '/';

    printf("(client %p) connected\n", cl);

    usr_send(cl, "220 poryadok u fajlov v kioskah bil vjyat");
    return cl;
}

void usr_del(ftp_usr* cl) {
    printf("(client %p) disconnected\n", cl);

    if (cl->tr)
        tr_del(cl->tr);

    closesocket(cl->sock);

    free(cl);
}

void usr_close(ftp_usr* cl) {    
    cl->closing = 1;
}

int usr_marked_to_del(ftp_usr* cl) {
    return cl->closing;
}

void usr_send(ftp_usr* cl, const char* data) {
    printf("(client %p) resp:   %s\n", cl, data);

    int len = strlen(data);

    if (CL_SEND_BUF < cl->sendLen + len + 2) {
        usr_close(cl);
        return;
    }

    char* dest = cl->sendBuf + cl->sendLen;
    memcpy(dest, data, len);
    dest[len] = '\r';
    dest[len + 1] = '\n';
    cl->sendLen += len + 2;
}

void _usr_on_cmd_rw(ftp_usr* cl, char* cmd) {
    printf("(client %p) cmd:    %s\n", cl, cmd);

    char* arg = NULL;

    for (int i = 0; i < strlen(cmd); i++) {
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
            arg = cmd + i + 1;
            break;
        }
    }

    _usr_on_cmd(cl, cmd, arg);
}

void _usr_handle_recv(ftp_usr* cl) {
    char buf[CL_MAX_CMD];

    int res = recv(cl->sock, buf, sizeof(buf), MSG_PEEK);
    if (res == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            usr_close(cl);
        }

        return;
    }

    if (res == 0) {
        usr_close(cl);
        return;
    }

    for (int i = 0; i < res - 1; i++) {
        if (buf[i] = '\r' && buf[i + 1] == '\n') {
            recv(cl->sock, buf, i + 2, 0);
            buf[i] = '\0';
            _usr_on_cmd_rw(cl, buf);
            return;
        }
    }

    if (res == sizeof(buf)) {
        usr_close(cl);
    }
}

void _usr_handle_send(ftp_usr* cl) {
    int res = send(cl->sock, cl->sendBuf, cl->sendLen, 0);
    if (res == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            usr_close(cl);
        }

        return;
    }

    memcpy(cl->sendBuf, cl->sendBuf + res, res);
    cl->sendLen -= res;
}

void usr_select_set(ftp_usr* cl, FD_SET* ReadSet, FD_SET* WriteSet) {
    FD_SET(cl->sock, ReadSet);

    if (cl->sendLen) {
        FD_SET(cl->sock, WriteSet);
    }

    if (cl->tr)
        tr_select_set(cl->tr, ReadSet, WriteSet);
}

void usr_select_check(ftp_usr* cl, FD_SET* ReadSet, FD_SET* WriteSet) {
    if (FD_ISSET(cl->sock, ReadSet)) {
        _usr_handle_recv(cl);
    }

    if (FD_ISSET(cl->sock, WriteSet)) {
        _usr_handle_send(cl);
    }

    if (cl->tr) {
        tr_select_check(cl->tr, ReadSet, WriteSet);
        if (tr_marked_to_del(cl->tr)) {
            tr_del(cl->tr);
            cl->tr = NULL;
        }
    }
}