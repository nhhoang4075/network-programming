// FileClient - mo hinh BLOCKING (de giua ky 247)
// Bien dich tren Windows (Visual Studio hoac MinGW):
//   cl FileClient_blocking.cpp ws2_32.lib
//   g++ FileClient_blocking.cpp -lws2_32 -o FileClient
//
// Giao thuc:
//   Client gui:  "GET <TenFile>\n"
//   Server tra:  "FAILED\n<ThongBaoLoi>\n\n"
//             hoac "OK\n<FileSize>\n<NoiDungFile>\n\n"
// Yeu cau: tai duoc nhieu file cung luc, tu nhieu server -> moi file 1 luong.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT "8888"

// Tham so truyen cho moi luong tai file
struct DownloadInfo {
    char host[256];
    char fileName[256];
};

// Doc dung n byte tu socket (blocking). Tra ve so byte doc duoc, <=0 neu loi/dong.
int recvAll(SOCKET s, char *buf, int n) {
    int total = 0;
    while (total < n) {
        int ret = recv(s, buf + total, n - total, 0);
        if (ret <= 0) return ret;   // loi hoac ket noi dong
        total += ret;
    }
    return total;
}

// Doc tung byte cho den khi gap ky tu '\n'. Khong chep '\n' vao line.
// Tra ve 1 neu thanh cong, <=0 neu loi/dong ket noi.
int recvLine(SOCKET s, char *line, int maxLen) {
    int i = 0;
    char c;
    while (i < maxLen - 1) {
        int ret = recv(s, &c, 1, 0);
        if (ret <= 0) return ret;
        if (c == '\n') break;
        line[i++] = c;
    }
    line[i] = 0;
    return 1;
}

DWORD WINAPI DownloadThread(LPVOID param) {
    DownloadInfo *info = (DownloadInfo *)param;

    // 1. Phan giai ten mien / dia chi
    addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(info->host, SERVER_PORT, &hints, &res) != 0) {
        printf("[%s] Khong phan giai duoc dia chi server\n", info->fileName);
        free(info);
        return 1;
    }

    // 2. Tao socket va ket noi
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        printf("[%s] Loi tao socket: %d\n", info->fileName, WSAGetLastError());
        freeaddrinfo(res);
        free(info);
        return 1;
    }
    if (connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        printf("[%s] Loi ket noi: %d\n", info->fileName, WSAGetLastError());
        closesocket(s);
        freeaddrinfo(res);
        free(info);
        return 1;
    }
    freeaddrinfo(res);

    // 3. Gui yeu cau "GET <TenFile>\n"
    char request[512];
    int reqLen = sprintf(request, "GET %s\n", info->fileName);
    if (send(s, request, reqLen, 0) == SOCKET_ERROR) {
        printf("[%s] Loi gui yeu cau: %d\n", info->fileName, WSAGetLastError());
        closesocket(s);
        free(info);
        return 1;
    }

    // 4. Doc dong trang thai dau tien ("OK" hoac "FAILED")
    char status[64];
    if (recvLine(s, status, sizeof(status)) <= 0) {
        printf("[%s] Server dong ket noi\n", info->fileName);
        closesocket(s);
        free(info);
        return 1;
    }

    if (strcmp(status, "FAILED") == 0) {
        char errMsg[1024];
        recvLine(s, errMsg, sizeof(errMsg));   // dong thong bao loi
        printf("[%s] Server bao loi: %s\n", info->fileName, errMsg);
        closesocket(s);
        free(info);
        return 0;
    }

    if (strcmp(status, "OK") != 0) {
        printf("[%s] Phan hoi khong hop le: %s\n", info->fileName, status);
        closesocket(s);
        free(info);
        return 1;
    }

    // 5. Doc kich thuoc file
    char sizeLine[64];
    if (recvLine(s, sizeLine, sizeof(sizeLine)) <= 0) {
        closesocket(s);
        free(info);
        return 1;
    }
    long fileSize = atol(sizeLine);

    // 6. Doc dung fileSize byte noi dung va ghi ra dia
    FILE *fp = fopen(info->fileName, "wb");
    if (!fp) {
        printf("[%s] Khong mo duoc file de ghi\n", info->fileName);
        closesocket(s);
        free(info);
        return 1;
    }

    char buf[4096];
    long remain = fileSize;
    while (remain > 0) {
        int want = (remain < (long)sizeof(buf)) ? (int)remain : (int)sizeof(buf);
        int ret = recv(s, buf, want, 0);
        if (ret <= 0) break;
        fwrite(buf, 1, ret, fp);
        remain -= ret;
    }
    fclose(fp);

    if (remain == 0)
        printf("[%s] Tai xong (%ld byte)\n", info->fileName, fileSize);
    else
        printf("[%s] Tai loi, thieu %ld byte\n", info->fileName, remain);

    closesocket(s);
    free(info);
    return 0;
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("WSAStartup loi\n");
        return 1;
    }

    printf("FileClient (blocking). Nhap rong o ten file de thoat.\n");
    while (1) {
        DownloadInfo *info = (DownloadInfo *)malloc(sizeof(DownloadInfo));

        printf("\nDia chi server: ");
        if (!fgets(info->host, sizeof(info->host), stdin)) { free(info); break; }
        info->host[strcspn(info->host, "\r\n")] = 0;

        printf("Ten file: ");
        if (!fgets(info->fileName, sizeof(info->fileName), stdin)) { free(info); break; }
        info->fileName[strcspn(info->fileName, "\r\n")] = 0;

        if (strlen(info->fileName) == 0) { free(info); break; }

        // Moi yeu cau tai mot luong rieng -> tai nhieu file cung luc
        HANDLE h = CreateThread(NULL, 0, DownloadThread, info, 0, NULL);
        if (h) CloseHandle(h);
        else { free(info); }
    }

    Sleep(2000);   // cho cac luong tai con lai hoan tat (don gian hoa)
    WSACleanup();
    return 0;
}
