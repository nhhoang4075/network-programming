/**
 * @file chat_client.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Chat Client cho "Our chat protocol" (slide 262-274).
 *        Giao diện dòng lệnh, dùng select() để vừa nhận tin từ server
 *        vừa đọc bàn phím. Gõ chữ thường = nhắn cả phòng (MSG); dùng
 *        các lệnh /pmsg, /op, /kick, /topic, /quit cho các chức năng khác.
 * @date 2026-06-16
 *
 * Cách dùng:  ./chat_client [host] [port] [nickname]
 *             (mặc định host=127.0.0.1, port=9000; thiếu thì hỏi)
 * Biên dịch:  gcc -O2 -o chat_client chat_client.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT "9000"
#define LINE_BUF     2048
#define IN_BUF       8192

/* Màu ANSI cho dễ nhìn (không bắt buộc) */
#define C_RESET  "\033[0m"
#define C_DIM    "\033[2m"
#define C_CYAN   "\033[36m"
#define C_GREEN  "\033[32m"
#define C_YELLOW "\033[33m"
#define C_RED    "\033[31m"

static char my_nick[64] = "";

/* Kết nối tới host:port, trả về fd hoặc -1. */
static int connect_to(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return -1;
    }
    int fd = -1;
    for (p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

/* Gửi đủ một chuỗi. */
static void send_str(int fd, const char *s) {
    size_t n = strlen(s), off = 0;
    while (off < n) {
        ssize_t k = send(fd, s + off, n - off, 0);
        if (k <= 0) { if (errno == EINTR) continue; return; }
        off += (size_t)k;
    }
}

/* In một thông điệp nhận từ server dưới dạng dễ đọc. */
static void show_server_line(const char *line) {
    /* Mã phản hồi dạng số (1xx/2xx/9xx) */
    if (line[0] >= '0' && line[0] <= '9') {
        if (strncmp(line, "100", 3) == 0)
            printf(C_DIM "  [server] %s" C_RESET "\n", line);
        else
            printf(C_RED "  [server] %s" C_RESET "\n", line);
        return;
    }

    char cmd[16] = "", a[64] = "", rest[1900] = "";
    /* tách: <cmd> <a> <rest...> */
    const char *p = line;
    int i = 0;
    while (*p && *p != ' ' && i < 15) cmd[i++] = *p++;
    cmd[i] = '\0';
    while (*p == ' ') p++;
    i = 0;
    while (*p && *p != ' ' && i < 63) a[i++] = *p++;
    a[i] = '\0';
    while (*p == ' ') p++;
    strncpy(rest, p, sizeof rest - 1);

    if (strcmp(cmd, "MSG") == 0)
        printf(C_CYAN "%s" C_RESET ": %s\n", a, rest);
    else if (strcmp(cmd, "PMSG") == 0)
        printf(C_YELLOW "[riêng] %s" C_RESET ": %s\n", a, rest);
    else if (strcmp(cmd, "JOIN") == 0)
        printf(C_GREEN "* %s đã tham gia phòng" C_RESET "\n", a);
    else if (strcmp(cmd, "QUIT") == 0)
        printf(C_DIM "* %s đã rời phòng" C_RESET "\n", a);
    else if (strcmp(cmd, "OP") == 0)
        printf(C_GREEN "* %s giờ là chủ phòng" C_RESET "\n", a);
    else if (strcmp(cmd, "TOPIC") == 0)
        printf(C_GREEN "* %s đặt chủ đề: %s" C_RESET "\n", a, rest);
    else if (strcmp(cmd, "KICK") == 0) {
        if (strcmp(a, my_nick) == 0)
            printf(C_RED "* Bạn đã bị %s đuổi khỏi phòng" C_RESET "\n", rest);
        else
            printf(C_RED "* %s bị %s đuổi khỏi phòng" C_RESET "\n", a, rest);
    } else
        printf("  %s\n", line);                 /* không rõ -> in thô */
}

static void print_help(void) {
    printf(C_DIM
        "----------------------------------------------------------\n"
        " Gõ nội dung bất kỳ rồi Enter  -> nhắn cho cả phòng (MSG)\n"
        " /pmsg <nick> <nội dung>       -> nhắn riêng\n"
        " /op <nick>                    -> chuyển quyền chủ phòng\n"
        " /kick <nick>                  -> đuổi người (chủ phòng)\n"
        " /topic <chủ đề>               -> đặt chủ đề (chủ phòng)\n"
        " /raw <dòng giao thức>         -> gửi nguyên văn\n"
        " /help                         -> trợ giúp\n"
        " /quit                         -> thoát\n"
        "----------------------------------------------------------"
        C_RESET "\n");
}

/* Đọc 1 dòng từ stdin (bỏ '\n'). Trả 0 nếu EOF. */
static int read_line_stdin(char *buf, size_t cap) {
    if (!fgets(buf, (int)cap, stdin)) return 0;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 1;
}

/* Chuyển dòng người dùng gõ thành lệnh giao thức rồi gửi đi.
 * Trả về 1 nếu là lệnh QUIT (cần thoát). */
static int handle_user_input(int fd, char *line) {
    char out[LINE_BUF + 64];

    if (line[0] != '/') {                        /* văn bản thường -> MSG */
        if (line[0] == '\0') return 0;
        snprintf(out, sizeof out, "MSG %s\n", line);
        send_str(fd, out);
        return 0;
    }

    /* tách lệnh /xxx và phần còn lại */
    char *sp = strchr(line, ' ');
    char verb[16];
    char *arg = (char *)"";
    if (sp) {
        size_t vl = (size_t)(sp - line);
        if (vl >= sizeof verb) vl = sizeof verb - 1;
        memcpy(verb, line, vl); verb[vl] = '\0';
        arg = sp + 1;
        while (*arg == ' ') arg++;
    } else {
        snprintf(verb, sizeof verb, "%s", line);
    }

    if (strcmp(verb, "/quit") == 0) {
        send_str(fd, "QUIT\n");
        return 1;
    } else if (strcmp(verb, "/help") == 0) {
        print_help();
    } else if (strcmp(verb, "/pmsg") == 0) {
        snprintf(out, sizeof out, "PMSG %s\n", arg);
        send_str(fd, out);
    } else if (strcmp(verb, "/op") == 0) {
        snprintf(out, sizeof out, "OP %s\n", arg);
        send_str(fd, out);
    } else if (strcmp(verb, "/kick") == 0) {
        snprintf(out, sizeof out, "KICK %s\n", arg);
        send_str(fd, out);
    } else if (strcmp(verb, "/topic") == 0) {
        snprintf(out, sizeof out, "TOPIC %s\n", arg);
        send_str(fd, out);
    } else if (strcmp(verb, "/raw") == 0) {
        snprintf(out, sizeof out, "%s\n", arg);
        send_str(fd, out);
    } else {
        printf(C_DIM "  (lệnh không rõ, gõ /help)" C_RESET "\n");
    }
    return 0;
}

int main(int argc, char *argv[]) {
    const char *host = (argc > 1) ? argv[1] : DEFAULT_HOST;
    const char *port = (argc > 2) ? argv[2] : DEFAULT_PORT;

    printf("=== Chat Client ===\n");
    int fd = connect_to(host, port);
    if (fd < 0) {
        fprintf(stderr, C_RED "Không kết nối được tới %s:%s\n" C_RESET, host, port);
        return 1;
    }
    printf(C_GREEN "Đã kết nối tới %s:%s" C_RESET "\n", host, port);

    /* ---- Bắt tay JOIN ---- */
    char in[IN_BUF];
    size_t in_len = 0;

    for (;;) {
        char nick[64];
        if (argc > 3 && my_nick[0] == '\0') {
            snprintf(nick, sizeof nick, "%s", argv[3]);   /* dùng nick từ tham số */
        } else {
            printf("Nhập nickname (chỉ chữ thường + số): ");
            fflush(stdout);
            if (!read_line_stdin(nick, sizeof nick)) { close(fd); return 0; }
        }
        char join[128];
        snprintf(join, sizeof join, "JOIN %s\n", nick);
        send_str(fd, join);

        /* đọc 1 dòng phản hồi */
        char resp[256]; size_t rl = 0; resp[0] = '\0';
        while (rl < sizeof resp - 1) {
            char c;
            ssize_t k = recv(fd, &c, 1, 0);
            if (k <= 0) { fprintf(stderr, C_RED "Mất kết nối.\n" C_RESET); close(fd); return 1; }
            if (c == '\n') break;
            if (c != '\r') resp[rl++] = c;
        }
        resp[rl] = '\0';

        if (strncmp(resp, "100", 3) == 0) {
            snprintf(my_nick, sizeof my_nick, "%s", nick);
            printf(C_GREEN "Vào phòng thành công với nick \"%s\"." C_RESET "\n", my_nick);
            break;
        }
        printf(C_RED "JOIN thất bại: %s" C_RESET "\n", resp);
        if (argc > 3) { close(fd); return 1; }    /* nick truyền sẵn mà lỗi -> thoát */
    }

    print_help();
    printf("> "); fflush(stdout);

    /* ---- Vòng lặp chính: select(stdin, socket) ---- */
    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(fd, &rfds);
        int maxfd = (fd > STDIN_FILENO) ? fd : STDIN_FILENO;

        if (select(maxfd + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        /* Dữ liệu từ server */
        if (FD_ISSET(fd, &rfds)) {
            char tmp[4096];
            ssize_t n = recv(fd, tmp, sizeof tmp, 0);
            if (n <= 0) {
                printf("\n" C_RED "Mất kết nối tới server." C_RESET "\n");
                break;
            }
            if (in_len + (size_t)n >= IN_BUF) in_len = 0;
            memcpy(in + in_len, tmp, (size_t)n);
            in_len += (size_t)n;

            size_t start = 0;
            printf("\r\033[K");                     /* xoá prompt hiện tại */
            for (size_t i = 0; i < in_len; i++) {
                if (in[i] == '\n') {
                    size_t end = i;
                    if (end > start && in[end - 1] == '\r') end--;
                    in[end] = '\0';
                    show_server_line(in + start);
                    start = i + 1;
                }
            }
            if (start > 0) {
                memmove(in, in + start, in_len - start);
                in_len -= start;
            }
            printf("> "); fflush(stdout);
        }

        /* Dữ liệu từ bàn phím */
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            char line[LINE_BUF];
            if (!read_line_stdin(line, sizeof line)) {  /* EOF (Ctrl-D) */
                send_str(fd, "QUIT\n");
                break;
            }
            if (handle_user_input(fd, line)) {          /* /quit */
                printf(C_DIM "Đang thoát..." C_RESET "\n");
                break;
            }
            printf("> "); fflush(stdout);
        }
    }

    close(fd);
    return 0;
}
