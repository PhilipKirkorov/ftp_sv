#pragma comment (lib, "Ws2_32.lib")
#include <winsock2.h>
#include "ftp_sv/ftp_sv.h"

int main()
{
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        return 0;
    }
    
    ftp_sv* sv = ftp_sv_new();
    sv_start(sv, 1337, "0.0.0.0", "127.0.0.1", "D://Desktop/123/");
    
    while (1) {
        sv_tick(sv);
        Sleep(10);
    }
    
    sv_stop(sv);
}