#include <stdio.h>

#include "ftp_usr.h"
#include "ftp_sv.h"
#include "ftp_tr.h"

void _usr_on_cmd(ftp_usr* cl, char* cmd, char* arg) {
    if (strcmp(cmd, "AUTH") == 0) {
        usr_send(cl, "502 Authentication not allowed");
        return;
    }

    if (strcmp(cmd, "USER") == 0) {
        if (arg == NULL) {
            usr_send(cl, "502 nope");
            return;
        }

        char d[CL_MAX_RESP];
        snprintf(d, sizeof(d), "331 Password required for %s", arg);
        usr_send(cl, d);
        return;
    }

    if (strcmp(cmd, "PASS") == 0) {
        usr_send(cl, "230 Logged on");
        return;
    }

    if (strcmp(cmd, "SYST") == 0) {
        usr_send(cl, "215 UNIX emulated by kiosk");
        return;
    }

    if (strcmp(cmd, "FEAT") == 0) {
        usr_send(cl, "211-Features:");
        usr_send(cl, "211 End");
        return;
    }

    if (strcmp(cmd, "PWD") == 0) {
        char d[CL_MAX_RESP];
        snprintf(d, sizeof(d), "257 \"%s\" is current directory.", cl->dir);
        usr_send(cl, d);
        return;
    }

    if (strcmp(cmd, "TYPE") == 0) {
        if (arg[0] != 'I') {
            usr_send(cl, "501 Unsupported type. Supported types are I");
            return;
        }

        usr_send(cl, "200 Type set to I");
        return;
    }

    if (strcmp(cmd, "PASV") == 0) {
        ftp_tr* tr = ftp_tr_new(cl);
        if (tr == NULL) {
            usr_send(cl, "502 nope");
            return;
        }

        int port = tr_pasv(tr);
        if (port < 0) {
            tr_close(tr);
            usr_send(cl, "502 nope");
            return;
        }

        cl->tr = tr;

        char d[CL_MAX_RESP];
        char* ip = cl->sv->ip;
        snprintf(d, sizeof(d), "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", ip[0], ip[1], ip[2], ip[3], port / 256, port % 256);
        usr_send(cl, d);
        return;
    }

    if (strcmp(cmd, "LIST") == 0) {
        if (!cl->tr) {
            usr_send(cl, "502 nope");
            return;
        }

        char d[SV_MAX_ABS_PATH];
        snprintf(d, sizeof(d), "%s%s", cl->sv->path, cl->dir);

        if (tr_send_list(cl->tr, d) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        usr_send(cl, "150 Opening data channel");
        return;
    }

    if (strcmp(cmd, "STOR") == 0) {
        if (!cl->tr) {
            usr_send(cl, "502 nope");
            return;
        }

        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }
        
        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);

        if (tr_recv_file(cl->tr, b) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        usr_send(cl, "150 Opening data channel");
        return;
    }

    if (strcmp(cmd, "RETR") == 0) {
        if (!cl->tr) {
            usr_send(cl, "502 nope");
            return;
        }

        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);
        
        if (tr_send_file(cl->tr, b) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        usr_send(cl, "150 Opening data channel");
        return;
    }

    if (strcmp(cmd, "DELE") == 0) {
        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);

        DeleteFileA(b);

        usr_send(cl, "250 ok.");
        return;
    }

    if (strcmp(cmd, "RMD") == 0) {
        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);

        RemoveDirectoryA(b);

        usr_send(cl, "250 ok.");
        return;
    }

    if (strcmp(cmd, "MKD") == 0) {
        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);

        CreateDirectoryA(b, 0);

        usr_send(cl, "250 ok.");
        return;
    }

    if (strcmp(cmd, "CWD") == 0) {
        char d[CL_MAX_PATH];
        if (usr_parse_path(cl, arg, d, sizeof(d)) < 0) {
            usr_send(cl, "502 nope");
            return;
        }

        char b[SV_MAX_ABS_PATH];
        snprintf(b, sizeof(b), "%s%s", cl->sv->path, d);

        DWORD dwAttrib = GetFileAttributesA(b);
        if (dwAttrib == INVALID_FILE_ATTRIBUTES || !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
            usr_send(cl, "502 nope");
            return;
        }

        snprintf(cl->dir, sizeof(cl->dir), "%s", d);

        usr_send(cl, "250 CWD successful.");
        return;
    }

    if (strcmp(cmd, "CDUP") == 0) {
        usr_parse_path(cl, "..", cl->dir, sizeof(cl->dir));
        usr_send(cl, "250 CWD successful.");
        return;
    }
}