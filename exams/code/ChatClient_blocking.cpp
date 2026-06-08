// ChatClient - mo hinh BLOCKING (de chat - DE 1, Cau 1)
// Bien dich tren Windows:
//   cl ChatClient_blocking.cpp ws2_32.lib
//   g++ ChatClient_blocking.cpp -lws2_32 -o ChatClient
//
// Cung giao thuc voi ChatServer_blocking.cpp.
// Lenh nguoi dung:
//   /list                  -> xin danh sach nickname dang online
//   @<nickname> <noi dung> -> gui tin nhan rieng den <nickname>
//   /quit                  -> thoat
//
// Mo hinh blocking: dung 2 luong - 1 luong nhan tu server, 1 luong (main) doc ban phim.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT "8888"
#define NICK_LEN    32
#define DATA_LEN    1024

struct Packet {
    int  iType;
    int  iLength;
    char szNickname[NICK_LEN];
    char bData[DATA_LEN];
};

SOCKET g_sock;
char   g_myNick[NICK_LEN];

int sendPacket(SOCKET s, Packet *p) {
    char *buf = (char *)p;
    int total = 0, n = sizeof(Packet);
    while (total < n) {
        int ret = send(s, buf + total, n - total, 0);
        if (ret <= 0) return ret;
        total += ret;
    }
    return total;
}

int recvPacket(SOCKET s, Packet *p) {
    char *buf = (char *)p;
    int total = 0, n = sizeof(Packet);
    while (total < n) {
        int ret = recv(s, buf + total, n - total, 0);
        if (ret <= 0) return ret;
        total += ret;
    }
    return total;
}

// Luong nhan goi tin tu server va hien thi
DWORD WINAPI ReceiverThread(LPVOID param) {
    Packet p;
    while (recvPacket(g_sock, &p) > 0) {
        if (p.iType == 1) {
            // szNickname = nguoi gui, bData = cau thoai
            p.szNickname[NICK_LEN - 1] = 0;
            printf("\n[%s]: %s\n> ", p.szNickname, p.bData);
        } else if (p.iType == 2) {
            int count = p.iLength / NICK_LEN;
            printf("\n--- Online (%d) ---\n", count);
            for (int i = 0; i < count; i++)
                printf("  %.*s\n", NICK_LEN, p.bData + i * NICK_LEN);
            printf("> ");
        }
        fflush(stdout);
    }
    printf("\nMat ket noi voi server.\n");
    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    char host[256];
    printf("Dia chi server: ");
    fgets(host, sizeof(host), stdin);
    host[strcspn(host, "\r\n")] = 0;

    printf("Nickname: ");
    fgets(g_myNick, sizeof(g_myNick), stdin);
    g_myNick[strcspn(g_myNick, "\r\n")] = 0;

    // Phan giai + ket noi
    addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host, SERVER_PORT, &hints, &res) != 0) {
        printf("Khong phan giai duoc dia chi server\n");
        return 1;
    }
    g_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(g_sock, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("Loi ket noi: %d\n", WSAGetLastError());
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    // Gui goi dang nhap (type 0)
    Packet login;
    memset(&login, 0, sizeof(login));
    login.iType = 0;
    login.iLength = 0;
    strncpy(login.szNickname, g_myNick, NICK_LEN - 1);
    sendPacket(g_sock, &login);

    // Luong nhan
    HANDLE hRecv = CreateThread(NULL, 0, ReceiverThread, NULL, 0, NULL);

    printf("Da ket noi. Lenh: /list | @<nick> <noi dung> | /quit\n> ");
    fflush(stdout);

    // Vong lap doc ban phim
    char line[1200];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\r\n")] = 0;

        if (strcmp(line, "/quit") == 0) {
            break;
        } else if (strcmp(line, "/list") == 0) {
            Packet p;
            memset(&p, 0, sizeof(p));
            p.iType = 2;
            p.iLength = 0;
            sendPacket(g_sock, &p);
        } else if (line[0] == '@') {
            // @<nick> <noi dung>
            char *sp = strchr(line, ' ');
            if (!sp) { printf("Cu phap: @<nick> <noi dung>\n> "); fflush(stdout); continue; }
            *sp = 0;
            char *dest = line + 1;        // bo dau '@'
            char *msg = sp + 1;

            Packet p;
            memset(&p, 0, sizeof(p));
            p.iType = 1;
            strncpy(p.szNickname, dest, NICK_LEN - 1);   // nickname DICH
            strncpy(p.bData, msg, DATA_LEN - 1);
            p.iLength = (int)strlen(p.bData);
            sendPacket(g_sock, &p);
            printf("> "); fflush(stdout);
        } else if (strlen(line) > 0) {
            printf("Lenh khong hieu. Dung: /list | @<nick> <noi dung> | /quit\n> ");
            fflush(stdout);
        } else {
            printf("> "); fflush(stdout);
        }
    }

    closesocket(g_sock);
    WaitForSingleObject(hRecv, 1000);
    CloseHandle(hRecv);
    WSACleanup();
    return 0;
}
