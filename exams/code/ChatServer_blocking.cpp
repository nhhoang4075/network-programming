// ChatServer - mo hinh BLOCKING (de chat - DE 1, Cau 2)
// Bien dich tren Windows:
//   cl ChatServer_blocking.cpp ws2_32.lib
//   g++ ChatServer_blocking.cpp -lws2_32 -o ChatServer
//
// Giao thuc (cong TCP 8888):
//   struct Packet { int iType; int iLength; char szNickname[32]; char bData[1024]; }
//   iType = 0: dang nhap (client->server), szNickname = nickname dang ky.
//   iType = 1: chat. client->server: szNickname = nickname DICH, bData = cau thoai.
//              Server sua szNickname thanh nickname NGUOI GUI roi chuyen tiep den dich.
//   iType = 2: list. client->server: xin danh sach. server->client: bData chua
//              danh sach nickname, moi nickname chiem dung 32 byte (dem NULL), iLength = n*32.
//   Server chu dong gui lai list (type 2) cho TAT CA client moi khi co client
//   dang nhap / dang xuat.
//
// ===========================================================================
//  GHI CHU cac mo hinh khac (DE 2-5) - cung giao thuc, chi khac cach vao/ra:
//   - DE 2 (select):        1 luong, dat cac SOCKET vao fd_set, goi select() de
//                           biet socket nao co du lieu / co ket noi moi (readfds).
//   - DE 3 (WSAEventSelect): moi SOCKET gan 1 WSAEVENT bang WSAEventSelect(s,ev,
//                           FD_ACCEPT|FD_READ|FD_CLOSE), cho bang WSAWaitForMultipleEvents,
//                           WSAEnumNetworkEvents de biet su kien.
//   - DE 4 (Overlapped Event): moi WSARecv/WSASend kem 1 WSAOVERLAPPED co hEvent,
//                           cho bang WSAWaitForMultipleEvents, WSAGetOverlappedResult.
//   - DE 5 (Completion Routine): WSARecv/WSASend kem completion routine (callback),
//                           luong phai o trang thai alertable (SleepEx/WSAWaitForMultipleEventsEx).
//  Toan bo logic xu ly goi tin (dang nhap, chuyen tiep chat, broadcast list) giu nguyen.
// ===========================================================================

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT  8888
#define MAX_CLIENTS  64
#define NICK_LEN     32
#define DATA_LEN     1024

struct Packet {
    int  iType;
    int  iLength;
    char szNickname[NICK_LEN];
    char bData[DATA_LEN];
};

struct ClientInfo {
    SOCKET s;
    char   nickname[NICK_LEN];
    int    used;
};

ClientInfo       g_clients[MAX_CLIENTS];
CRITICAL_SECTION g_cs;

// --- Gui/nhan tron ven 1 goi tin (blocking) ---
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

// --- Quan ly danh sach client ---
void addClient(SOCKET s, const char *nick) {
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].used) {
            g_clients[i].used = 1;
            g_clients[i].s = s;
            strncpy(g_clients[i].nickname, nick, NICK_LEN - 1);
            g_clients[i].nickname[NICK_LEN - 1] = 0;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
}

void removeClient(SOCKET s) {
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].used && g_clients[i].s == s) {
            g_clients[i].used = 0;
            break;
        }
    }
    LeaveCriticalSection(&g_cs);
}

// Tim nickname nguoi gui theo socket
void getNickBySock(SOCKET s, char *out) {
    EnterCriticalSection(&g_cs);
    out[0] = 0;
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_clients[i].used && g_clients[i].s == s) {
            strncpy(out, g_clients[i].nickname, NICK_LEN);
            break;
        }
    LeaveCriticalSection(&g_cs);
}

// Tim socket dich theo nickname
SOCKET getSockByNick(const char *nick) {
    SOCKET found = INVALID_SOCKET;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_clients[i].used && strncmp(g_clients[i].nickname, nick, NICK_LEN) == 0) {
            found = g_clients[i].s;
            break;
        }
    LeaveCriticalSection(&g_cs);
    return found;
}

// Tao goi tin danh sach (type 2): moi nickname 32 byte, dem NULL
void buildListPacket(Packet *p) {
    memset(p, 0, sizeof(Packet));
    p->iType = 2;
    int count = 0;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].used) {
            // bData[count*32] chua nickname thu (count+1)
            strncpy(p->bData + count * NICK_LEN, g_clients[i].nickname, NICK_LEN);
            count++;
        }
    }
    LeaveCriticalSection(&g_cs);
    p->iLength = count * NICK_LEN;
}

// Gui danh sach hien tai den TAT CA client
void broadcastList() {
    Packet p;
    buildListPacket(&p);

    // Chup nhanh danh sach socket de tranh giu khoa khi gui (blocking)
    SOCKET socks[MAX_CLIENTS];
    int n = 0;
    EnterCriticalSection(&g_cs);
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (g_clients[i].used) socks[n++] = g_clients[i].s;
    LeaveCriticalSection(&g_cs);

    for (int i = 0; i < n; i++) sendPacket(socks[i], &p);
}

DWORD WINAPI ClientThread(LPVOID param) {
    SOCKET client = (SOCKET)(UINT_PTR)param;
    Packet p;

    // 1. Goi dau tien phai la dang nhap (type 0)
    if (recvPacket(client, &p) <= 0 || p.iType != 0) {
        closesocket(client);
        return 0;
    }
    p.szNickname[NICK_LEN - 1] = 0;
    addClient(client, p.szNickname);
    printf("'%s' dang nhap.\n", p.szNickname);

    // Server tu dong gui danh sach cho tat ca (gom ca client vua vao)
    broadcastList();

    // 2. Vong lap xu ly goi tin tu client nay
    while (recvPacket(client, &p) > 0) {
        if (p.iType == 1) {
            // Chat: szNickname = nickname DICH. Doi thanh nickname NGUOI GUI roi chuyen tiep.
            char dest[NICK_LEN];
            strncpy(dest, p.szNickname, NICK_LEN);
            dest[NICK_LEN - 1] = 0;

            SOCKET destSock = getSockByNick(dest);
            if (destSock != INVALID_SOCKET) {
                char sender[NICK_LEN];
                getNickBySock(client, sender);
                memset(p.szNickname, 0, NICK_LEN);
                strncpy(p.szNickname, sender, NICK_LEN - 1);
                sendPacket(destSock, &p);
            }
        } else if (p.iType == 2) {
            // Client xin danh sach -> gui rieng cho client nay
            Packet list;
            buildListPacket(&list);
            sendPacket(client, &list);
        }
    }

    // 3. Client thoat -> go khoi danh sach, cap nhat list cho moi nguoi
    char nick[NICK_LEN];
    getNickBySock(client, nick);
    removeClient(client);
    closesocket(client);
    printf("'%s' dang xuat.\n", nick);
    broadcastList();
    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    InitializeCriticalSection(&g_cs);

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listenSock, (sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("Loi bind: %d\n", WSAGetLastError());
        return 1;
    }
    listen(listenSock, SOMAXCONN);
    printf("ChatServer lang nghe cong %d ...\n", SERVER_PORT);

    while (1) {
        sockaddr_in cAddr;
        int len = sizeof(cAddr);
        SOCKET client = accept(listenSock, (sockaddr *)&cAddr, &len);
        if (client == INVALID_SOCKET) continue;
        HANDLE h = CreateThread(NULL, 0, ClientThread, (LPVOID)(UINT_PTR)client, 0, NULL);
        if (h) CloseHandle(h);
        else closesocket(client);
    }

    DeleteCriticalSection(&g_cs);
    closesocket(listenSock);
    WSACleanup();
    return 0;
}
