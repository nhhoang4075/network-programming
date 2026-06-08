// AuthServer - de cuoi ky IT4060-20122-CuoiKy-002
// Bien dich tren Windows:
//   cl AuthServer.cpp ws2_32.lib
//   g++ AuthServer.cpp -lws2_32 -o AuthServer
//
// Nhiem vu:
//   - Tao socket, lang nghe, chap nhan ket noi tu client (moi client 1 luong).
//   - Nhan lenh tu client va xu ly:
//       USER:<ten>  -> co user khong? "No user found" / "OK"
//       PASS:<mk>   -> chua gui USER: "Please send USER first"
//                      mat khau dung: "OK"
//                      mat khau sai:  "Invalid Password"
//   - File data.txt o server, moi dong: <username> <password>

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8888
#define DATA_FILE   "data.txt"

// Tim user trong data.txt. Neu thay, chep mat khau vao outPass va tra ve 1.
int findUser(const char *user, char *outPass, int outPassSize) {
    FILE *fp = fopen(DATA_FILE, "rt");
    if (!fp) return 0;
    char line[512], u[256], p[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (sscanf(line, "%255s %255s", u, p) == 2) {
            if (strcmp(u, user) == 0) {
                strncpy(outPass, p, outPassSize - 1);
                outPass[outPassSize - 1] = 0;
                found = 1;
                break;
            }
        }
    }
    fclose(fp);
    return found;
}

// Doc 1 dong (ket thuc bang '\n') tu socket. Tra ve 1 neu OK, <=0 neu dong/loi.
int recvLine(SOCKET s, char *line, int maxLen) {
    int i = 0;
    char c;
    while (i < maxLen - 1) {
        int ret = recv(s, &c, 1, 0);
        if (ret <= 0) return ret;
        if (c == '\n') break;
        if (c != '\r') line[i++] = c;
    }
    line[i] = 0;
    return 1;
}

void sendMsg(SOCKET s, const char *msg) {
    send(s, msg, (int)strlen(msg), 0);
}

DWORD WINAPI ClientThread(LPVOID param) {
    SOCKET client = (SOCKET)(UINT_PTR)param;

    int hasUser = 0;           // da gui USER hop le chua
    char curUser[256] = "";    // user dang chon
    char curPass[256] = "";    // mat khau cua user do (trong data.txt)

    char line[512];
    while (recvLine(client, line, sizeof(line)) > 0) {
        if (strncmp(line, "USER:", 5) == 0) {
            const char *name = line + 5;
            char pass[256];
            if (findUser(name, pass, sizeof(pass))) {
                strncpy(curUser, name, sizeof(curUser) - 1);
                strncpy(curPass, pass, sizeof(curPass) - 1);
                hasUser = 1;
                sendMsg(client, "OK\n");
            } else {
                sendMsg(client, "No user found\n");
            }
        } else if (strncmp(line, "PASS:", 5) == 0) {
            const char *pass = line + 5;
            if (!hasUser) {
                sendMsg(client, "Please send USER first\n");
            } else if (strcmp(pass, curPass) == 0) {
                sendMsg(client, "OK\n");
            } else {
                sendMsg(client, "Invalid Password\n");
            }
        } else {
            sendMsg(client, "Unknown command\n");
        }
    }

    closesocket(client);
    return 0;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup loi\n");
        return 1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("Loi tao socket: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    if (bind(listenSock, (sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printf("Loi bind: %d\n", WSAGetLastError());
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }
    listen(listenSock, SOMAXCONN);
    printf("AuthServer dang lang nghe o cong %d ...\n", SERVER_PORT);

    while (1) {
        sockaddr_in clientAddr;
        int len = sizeof(clientAddr);
        SOCKET client = accept(listenSock, (sockaddr *)&clientAddr, &len);
        if (client == INVALID_SOCKET) continue;

        printf("Client ket noi: %s:%d\n",
               inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        HANDLE h = CreateThread(NULL, 0, ClientThread, (LPVOID)(UINT_PTR)client, 0, NULL);
        if (h) CloseHandle(h);
        else closesocket(client);
    }

    closesocket(listenSock);
    WSACleanup();
    return 0;
}
