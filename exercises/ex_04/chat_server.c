/**
 * @file chat_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Chat Server theo "Our chat protocol" (slide 262-274).
 *        Dùng select() phục vụ nhiều client đồng thời, xử lý lần lượt
 *        các yêu cầu. Giao thức text, theo phiên, không xác thực.
 * @date 2026-06-16
 *
 * Cách dùng:  ./chat_server [port]        (mặc định 9000)
 * Biên dịch:  gcc -O2 -o chat_server chat_server.c
 *
 * Lệnh từ client -> server:
 *   JOIN <nick>            tham gia phòng (nick: [a-z0-9]+)
 *   MSG  <message>         nhắn cho cả phòng
 *   PMSG <nick> <message>  nhắn riêng cho 1 người
 *   OP   <nick>            chuyển quyền chủ phòng (chỉ chủ phòng)
 *   KICK <nick>            đuổi 1 người      (chỉ chủ phòng)
 *   TOPIC <topic>          đặt chủ đề        (chỉ chủ phòng)
 *   QUIT                   thoát phòng
 *
 * Phản hồi (chỉ gửi cho người gửi lệnh):
 *   100 OK                 200 NICKNAME IN USE   201 INVALID NICK NAME
 *   202 UNKNOWN NICKNAME   203 DENIED            999 UNKNOWN ERROR
 *
 * Thông điệp server chủ động gửi (broadcast cho mọi người TRỪ người gửi):
 *   JOIN <nick>            MSG <nick> <message>   PMSG <nick> <message>
 *   OP <nick>              KICK <kicked> <op>     TOPIC <op> <topic>
 *   QUIT <nick>
 *
 * Lưu ý khớp với file kiểm thử chat_server_test:
 *   - Mọi dòng kết thúc bằng đúng '\n' (LF), KHÔNG dùng "\r\n".
 *   - Phản hồi chỉ gửi cho người gửi; broadcast gửi cho mọi người trừ
 *     người gửi (KICK gửi cả cho người bị đuổi).
 *   - Lệnh lỗi không broadcast. Không gửi banner khi client kết nối.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DEFAULT_PORT 9000
#define BACKLOG      16
#define MAX_CLIENTS  256
#define NICK_LEN     64
#define IN_BUF       8192   /* buffer ghép dòng cho mỗi client */
#define OUT_BUF      8192   /* buffer dựng thông điệp gửi đi   */

/* Các mã phản hồi (đã kèm '\n') */
#define RSP_OK            "100 OK\n"
#define RSP_NICK_IN_USE   "200 NICKNAME IN USE\n"
#define RSP_INVALID_NICK  "201 INVALID NICK NAME\n"
#define RSP_UNKNOWN_NICK  "202 UNKNOWN NICKNAME\n"
#define RSP_DENIED        "203 DENIED\n"
#define RSP_ERROR         "999 UNKNOWN ERROR\n"

typedef struct {
    int    fd;              /* -1 nếu slot trống            */
    int    joined;          /* 1 nếu đã JOIN với nick hợp lệ */
    char   nick[NICK_LEN];
    char   in[IN_BUF];      /* dữ liệu chưa đủ 1 dòng        */
    size_t in_len;
} Client;

static Client clients[MAX_CLIENTS];
static int    owner_idx = -1;   /* chỉ số chủ phòng, -1 nếu chưa có */

/* --------------------------------------------------------------------- */
/* Tiện ích                                                              */
/* --------------------------------------------------------------------- */

/* Gửi đủ n byte, bỏ qua lỗi (client có thể đã ngắt). */
static void send_all(int fd, const char *buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        ssize_t k = send(fd, buf + off, n - off, 0);
        if (k <= 0) {
            if (k < 0 && errno == EINTR) continue;
            return;                    /* client lỗi/đóng -> thôi */
        }
        off += (size_t)k;
    }
}

static void send_str(int fd, const char *s) { send_all(fd, s, strlen(s)); }

/* Broadcast tới mọi client đã JOIN, trừ slot except_idx. */
static void broadcast_except(int except_idx, const char *msg) {
    size_t n = strlen(msg);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0 && clients[i].joined && i != except_idx)
            send_all(clients[i].fd, msg, n);
    }
}

/* Nickname hợp lệ: khác rỗng, chỉ gồm chữ thường và chữ số. */
static int valid_nick(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) {
        if (!((*s >= 'a' && *s <= 'z') || (*s >= '0' && *s <= '9')))
            return 0;
    }
    return 1;
}

/* Tìm client đã JOIN theo nick, trả về chỉ số hoặc -1. */
static int find_by_nick(const char *nick) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0 && clients[i].joined &&
            strcmp(clients[i].nick, nick) == 0)
            return i;
    }
    return -1;
}

/* Đóng & giải phóng 1 slot client. Nếu là chủ phòng thì chuyển quyền. */
static void close_client(int idx) {
    if (clients[idx].fd < 0) return;
    close(clients[idx].fd);
    clients[idx].fd     = -1;
    clients[idx].joined = 0;
    clients[idx].in_len = 0;
    clients[idx].nick[0] = '\0';

    if (owner_idx == idx) {           /* chủ phòng rời -> chuyển cho người khác */
        owner_idx = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && clients[i].joined) { owner_idx = i; break; }
        }
    }
}

/* --------------------------------------------------------------------- */
/* Tách tham số                                                          */
/* --------------------------------------------------------------------- */

/* Bỏ khoảng trắng đầu chuỗi, trả về con trỏ mới. */
static char *skip_spaces(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/* Lấy token đầu tiên (đến khoảng trắng) ra `tok`, trả về phần còn lại. */
static char *next_token(char *s, char *tok, size_t cap) {
    s = skip_spaces(s);
    size_t i = 0;
    while (*s && *s != ' ' && *s != '\t') {
        if (i + 1 < cap) tok[i++] = *s;
        s++;
    }
    tok[i] = '\0';
    return skip_spaces(s);
}

/* --------------------------------------------------------------------- */
/* Xử lý một dòng lệnh từ client[idx]                                    */
/* --------------------------------------------------------------------- */
static void handle_line(int idx, char *line) {
    int  fd = clients[idx].fd;
    char cmd[16];
    char *rest = next_token(line, cmd, sizeof cmd);

    if (cmd[0] == '\0') return;                 /* dòng rỗng -> bỏ qua */

    /* ---- JOIN <nick> ---- */
    if (strcmp(cmd, "JOIN") == 0) {
        char nick[NICK_LEN];
        next_token(rest, nick, sizeof nick);

        if (clients[idx].joined) {              /* đã ở trong phòng rồi */
            send_str(fd, RSP_ERROR);
            return;
        }
        if (!valid_nick(nick)) {                /* sai ký tự -> 201   */
            send_str(fd, RSP_INVALID_NICK);
            return;
        }
        if (find_by_nick(nick) >= 0) {          /* trùng tên  -> 200  */
            send_str(fd, RSP_NICK_IN_USE);
            return;
        }
        strncpy(clients[idx].nick, nick, NICK_LEN - 1);
        clients[idx].nick[NICK_LEN - 1] = '\0';
        clients[idx].joined = 1;
        if (owner_idx < 0) owner_idx = idx;     /* người đầu tiên làm chủ phòng */

        send_str(fd, RSP_OK);                     /* phản hồi cho người vào */

        char out[OUT_BUF];                      /* báo cho người khác     */
        snprintf(out, sizeof out, "JOIN %s\n", clients[idx].nick);
        broadcast_except(idx, out);
        return;
    }

    /* Các lệnh dưới đây đều yêu cầu đã JOIN. */
    if (!clients[idx].joined) {
        if (strcmp(cmd, "QUIT") == 0) { send_str(fd, RSP_OK); close_client(idx); }
        else send_str(fd, RSP_ERROR);
        return;
    }

    /* ---- MSG <message> ---- */
    if (strcmp(cmd, "MSG") == 0) {
        send_str(fd, RSP_OK);
        char out[OUT_BUF];
        snprintf(out, sizeof out, "MSG %s %s\n", clients[idx].nick, rest);
        broadcast_except(idx, out);
        return;
    }

    /* ---- PMSG <nick> <message> ---- */
    if (strcmp(cmd, "PMSG") == 0) {
        char target[NICK_LEN];
        char *msg = next_token(rest, target, sizeof target);
        int  t = find_by_nick(target);
        if (t < 0) { send_str(fd, RSP_UNKNOWN_NICK); return; }

        send_str(fd, RSP_OK);
        char out[OUT_BUF];
        snprintf(out, sizeof out, "PMSG %s %s\n", clients[idx].nick, msg);
        send_str(clients[t].fd, out);           /* CHỈ gửi cho người nhận */
        return;
    }

    /* ---- OP <nick> ---- (chỉ chủ phòng) ---- */
    if (strcmp(cmd, "OP") == 0) {
        if (idx != owner_idx) { send_str(fd, RSP_DENIED); return; }
        char target[NICK_LEN];
        next_token(rest, target, sizeof target);
        int t = find_by_nick(target);
        if (t < 0) { send_str(fd, RSP_UNKNOWN_NICK); return; }

        owner_idx = t;                          /* chuyển quyền chủ phòng */
        send_str(fd, RSP_OK);
        char out[OUT_BUF];
        snprintf(out, sizeof out, "OP %s\n", clients[t].nick);
        broadcast_except(idx, out);
        return;
    }

    /* ---- KICK <nick> ---- (chỉ chủ phòng) ---- */
    if (strcmp(cmd, "KICK") == 0) {
        if (idx != owner_idx) { send_str(fd, RSP_DENIED); return; }
        char target[NICK_LEN];
        next_token(rest, target, sizeof target);
        int t = find_by_nick(target);
        if (t < 0) { send_str(fd, RSP_UNKNOWN_NICK); return; }

        send_str(fd, RSP_OK);
        char out[OUT_BUF];                       /* KICK <kicked> <op> */
        snprintf(out, sizeof out, "KICK %s %s\n", clients[t].nick, clients[idx].nick);
        broadcast_except(idx, out);              /* gửi cả cho người bị đuổi */
        close_client(t);                         /* rồi mới ngắt kết nối họ */
        return;
    }

    /* ---- TOPIC <topic> ---- (chỉ chủ phòng) ---- */
    if (strcmp(cmd, "TOPIC") == 0) {
        if (idx != owner_idx) { send_str(fd, RSP_DENIED); return; }
        send_str(fd, RSP_OK);
        char out[OUT_BUF];                       /* TOPIC <op> <topic> */
        snprintf(out, sizeof out, "TOPIC %s %s\n", clients[idx].nick, rest);
        broadcast_except(idx, out);
        return;
    }

    /* ---- QUIT ---- */
    if (strcmp(cmd, "QUIT") == 0) {
        send_str(fd, RSP_OK);
        char out[OUT_BUF];
        snprintf(out, sizeof out, "QUIT %s\n", clients[idx].nick);
        broadcast_except(idx, out);
        close_client(idx);
        return;
    }

    /* Lệnh không hiểu */
    send_str(fd, RSP_ERROR);
}

/* --------------------------------------------------------------------- */
/* Đọc dữ liệu từ client, tách ra từng dòng và xử lý                     */
/* --------------------------------------------------------------------- */
static void on_readable(int idx) {
    char tmp[4096];
    ssize_t n = recv(clients[idx].fd, tmp, sizeof tmp, 0);
    if (n <= 0) {                                /* client ngắt kết nối */
        if (n < 0 && errno == EINTR) return;
        /* coi như QUIT ngầm: báo cho phòng nếu đã JOIN */
        if (clients[idx].joined) {
            char out[OUT_BUF];
            snprintf(out, sizeof out, "QUIT %s\n", clients[idx].nick);
            broadcast_except(idx, out);
        }
        close_client(idx);
        return;
    }

    /* nối vào buffer ghép dòng (tránh tràn) */
    if (clients[idx].in_len + (size_t)n >= IN_BUF) clients[idx].in_len = 0;
    memcpy(clients[idx].in + clients[idx].in_len, tmp, (size_t)n);
    clients[idx].in_len += (size_t)n;

    /* tách từng dòng kết thúc bằng '\n' */
    size_t start = 0;
    for (size_t i = 0; i < clients[idx].in_len; i++) {
        if (clients[idx].in[i] == '\n') {
            size_t end = i;
            if (end > start && clients[idx].in[end - 1] == '\r') end--; /* bỏ CR */
            clients[idx].in[end] = '\0';
            handle_line(idx, clients[idx].in + start);
            start = i + 1;
            if (clients[idx].fd < 0) return;     /* slot đã đóng (QUIT/KICK) */
        }
    }
    /* dồn phần dư về đầu buffer */
    if (start > 0) {
        memmove(clients[idx].in, clients[idx].in + start, clients[idx].in_len - start);
        clients[idx].in_len -= start;
    }
}

/* --------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;

    signal(SIGPIPE, SIG_IGN);                    /* gửi vào socket đã đóng -> EPIPE */
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons((uint16_t)port);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind"); return 1;
    }
    if (listen(listen_fd, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("[chat_server] listening on port %d ...\n", port);
    fflush(stdout);

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(listen_fd, &rfds);
        int maxfd = listen_fd;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd) maxfd = clients[i].fd;
            }
        }

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        /* Có kết nối mới? */
        if (FD_ISSET(listen_fd, &rfds)) {
            struct sockaddr_in cli;
            socklen_t clen = sizeof cli;
            int cfd = accept(listen_fd, (struct sockaddr *)&cli, &clen);
            if (cfd >= 0) {
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (clients[i].fd < 0) { slot = i; break; }
                if (slot < 0) {
                    close(cfd);                  /* hết chỗ */
                } else {
                    clients[slot].fd     = cfd;
                    clients[slot].joined = 0;
                    clients[slot].in_len = 0;
                    clients[slot].nick[0] = '\0';
                    /* KHÔNG gửi banner - client phải nhận phản hồi JOIN đầu tiên */
                }
            }
        }

        /* Client nào có dữ liệu? */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd >= 0 && FD_ISSET(clients[i].fd, &rfds))
                on_readable(i);
        }
    }

    close(listen_fd);
    return 0;
}
