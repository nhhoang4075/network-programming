// FileClient - mo hinh SELECT (de giua ky 132)
// Bien dich tren Windows:
//   cl FileClient_select.cpp ws2_32.lib
//   g++ FileClient_select.cpp -lws2_32 -o FileClient
//
// Giao thuc: giong de 247
//   Client gui:  "GET <TenFile>\n"
//   Server tra:  "FAILED\n<ThongBaoLoi>\n\n"
//             hoac "OK\n<FileSize>\n<NoiDungFile>\n\n"
// Mo hinh select: 1 luong duy nhat, socket bat dong bo, dung select() de
// thuc hien tai NHIEU file cung luc tu nhieu server.
//
// Don gian hoa: nhap truoc danh sach (server, ten file), dong rong de bat dau tai.

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT "8888"

enum State { ST_CONNECTING, ST_RECEIVING, ST_DONE };
enum Phase { PH_STATUS, PH_SIZE, PH_CONTENT, PH_FAILMSG };

struct Conn {
    SOCKET s;
    State state;
    Phase phase;
    std::string host;
    std::string fileName;
    std::string buf;     // bo dem tich luy o pha header
    long fileSize;
    long received;
    FILE *fp;
};

void closeConn(Conn *c) {
    if (c->fp) { fclose(c->fp); c->fp = NULL; }
    if (c->s != INVALID_SOCKET) { closesocket(c->s); c->s = INVALID_SOCKET; }
    c->state = ST_DONE;
}

// Phan tich du lieu da nhan duoc trong c->buf (goi sau moi lan recv)
void process(Conn *c) {
    while (true) {
        if (c->phase == PH_STATUS || c->phase == PH_SIZE || c->phase == PH_FAILMSG) {
            size_t pos = c->buf.find('\n');
            if (pos == std::string::npos) return;          // chua du 1 dong, cho them
            std::string line = c->buf.substr(0, pos);
            c->buf.erase(0, pos + 1);

            if (c->phase == PH_STATUS) {
                if (line == "OK")          c->phase = PH_SIZE;
                else if (line == "FAILED") c->phase = PH_FAILMSG;
                else { printf("[%s] Phan hoi sai: %s\n", c->fileName.c_str(), line.c_str()); closeConn(c); return; }
            } else if (c->phase == PH_SIZE) {
                c->fileSize = atol(line.c_str());
                c->received = 0;
                c->fp = fopen(c->fileName.c_str(), "wb");
                if (!c->fp) { printf("[%s] Khong mo duoc file ghi\n", c->fileName.c_str()); closeConn(c); return; }
                c->phase = PH_CONTENT;
            } else { // PH_FAILMSG
                printf("[%s] Server bao loi: %s\n", c->fileName.c_str(), line.c_str());
                closeConn(c);
                return;
            }
        } else { // PH_CONTENT
            long remain = c->fileSize - c->received;
            if (remain <= 0) {
                printf("[%s] Tai xong (%ld byte)\n", c->fileName.c_str(), c->fileSize);
                closeConn(c);
                return;
            }
            long avail = (long)c->buf.size();
            long take = (avail < remain) ? avail : remain;
            if (take > 0) {
                fwrite(c->buf.data(), 1, take, c->fp);
                c->received += take;
                c->buf.erase(0, take);
            }
            if (c->received >= c->fileSize) {
                printf("[%s] Tai xong (%ld byte)\n", c->fileName.c_str(), c->fileSize);
                closeConn(c);
                return;
            }
            if (c->buf.empty()) return;   // het du lieu trong bo dem, cho recv tiep
        }
    }
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    std::vector<Conn *> conns;

    printf("FileClient (select). Nhap danh sach can tai, dong rong de bat dau.\n");
    while (true) {
        char host[256], file[256];
        printf("Dia chi server (rong = xong): ");
        if (!fgets(host, sizeof(host), stdin)) break;
        host[strcspn(host, "\r\n")] = 0;
        if (strlen(host) == 0) break;
        printf("Ten file: ");
        if (!fgets(file, sizeof(file), stdin)) break;
        file[strcspn(file, "\r\n")] = 0;
        if (strlen(file) == 0) break;

        // Phan giai + tao socket bat dong bo + connect
        addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        if (getaddrinfo(host, SERVER_PORT, &hints, &res) != 0) {
            printf("[%s] Khong phan giai duoc %s\n", file, host);
            continue;
        }
        SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        u_long mode = 1;
        ioctlsocket(s, FIONBIO, &mode);            // chuyen sang bat dong bo
        connect(s, res->ai_addr, (int)res->ai_addrlen);  // se tra WSAEWOULDBLOCK
        freeaddrinfo(res);

        Conn *c = new Conn();
        c->s = s; c->state = ST_CONNECTING; c->phase = PH_STATUS;
        c->host = host; c->fileName = file;
        c->fileSize = 0; c->received = 0; c->fp = NULL;
        conns.push_back(c);
    }

    // Vong lap select: chay den khi tat ca ket noi xong
    while (true) {
        fd_set readfds, writefds, exceptfds;
        FD_ZERO(&readfds); FD_ZERO(&writefds); FD_ZERO(&exceptfds);
        int active = 0;
        for (size_t i = 0; i < conns.size(); i++) {
            Conn *c = conns[i];
            if (c->state == ST_CONNECTING) {
                FD_SET(c->s, &writefds);
                FD_SET(c->s, &exceptfds);   // bat su kien connect that bai
                active++;
            } else if (c->state == ST_RECEIVING) {
                FD_SET(c->s, &readfds);
                active++;
            }
        }
        if (active == 0) break;   // xong het

        if (select(0, &readfds, &writefds, &exceptfds, NULL) == SOCKET_ERROR) {
            printf("select loi: %d\n", WSAGetLastError());
            break;
        }

        for (size_t i = 0; i < conns.size(); i++) {
            Conn *c = conns[i];
            if (c->state == ST_DONE) continue;

            // Connect that bai
            if (c->state == ST_CONNECTING && FD_ISSET(c->s, &exceptfds)) {
                printf("[%s] Ket noi that bai\n", c->fileName.c_str());
                closeConn(c);
                continue;
            }
            // Connect thanh cong -> gui yeu cau, chuyen sang nhan
            if (c->state == ST_CONNECTING && FD_ISSET(c->s, &writefds)) {
                std::string req = "GET " + c->fileName + "\n";
                send(c->s, req.c_str(), (int)req.size(), 0);
                c->state = ST_RECEIVING;
                continue;
            }
            // Co du lieu de doc
            if (c->state == ST_RECEIVING && FD_ISSET(c->s, &readfds)) {
                char tmp[4096];
                int ret = recv(c->s, tmp, sizeof(tmp), 0);
                if (ret > 0) {
                    c->buf.append(tmp, ret);
                    process(c);
                } else if (ret == 0) {
                    // server dong
                    if (c->phase == PH_CONTENT && c->received < c->fileSize)
                        printf("[%s] Server dong som, thieu %ld byte\n",
                               c->fileName.c_str(), c->fileSize - c->received);
                    closeConn(c);
                } else {
                    if (WSAGetLastError() != WSAEWOULDBLOCK) closeConn(c);
                }
            }
        }
    }

    for (size_t i = 0; i < conns.size(); i++) delete conns[i];
    WSACleanup();
    return 0;
}
