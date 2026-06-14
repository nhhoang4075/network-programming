/**
 * @file ftp_client.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Bài tập 04.02 - FTP Client.
 *        Tự cài đặt giao thức FTP (RFC 959) trên socket thô:
 *          1. Đăng nhập server lebavui.io.vn (USER/PASS).
 *          2. Lấy danh sách file (LIST) -> tìm file question_xxxxxx.txt.
 *          3. Tải nội dung file question về (RETR).
 *          4. Tạo file answer_xxxxxx.txt với nội dung đảo ngược.
 *          5. Upload file answer lên server (STOR).
 *        Dùng chế độ passive (PASV) cho kết nối dữ liệu.
 * @date 2026-06-14
 *
 * Biên dịch: gcc -o ftp_client ftp_client.c
 * Cách dùng:
 *   ./ftp_client <mssv> <ngày_sinh>   (vd: ./ftp_client 20235336 15)
 *
 * Quy tắc tài khoản:
 *   username = user_<MSSV>
 *   password = 4 số cuối MSSV + 2 chữ số ngày sinh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define exf       exit(EXIT_FAILURE)
#define BUF_SIZE  8192

/* ---- Server FTP ---- */
#define FTP_HOST  "lebavui.io.vn"
#define FTP_PORT  "21"

/*
 * Mở kết nối TCP tới host:port (port là chuỗi). Trả về socket, hoặc thoát nếu lỗi.
 * Dùng getaddrinfo để phân giải tên miền (lebavui.io.vn) sang địa chỉ IP.
 */
static int tcp_connect(const char *host, const char *port) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;       /* IPv4 cho đơn giản */
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err != 0) {
        fprintf(stderr, "getaddrinfo(%s:%s): %s\n", host, port, gai_strerror(err));
        exf;
    }

    int sock = -1;
    for (p = res; p != NULL; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) break; /* thành công */
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);

    if (sock < 0) {
        fprintf(stderr, "Không kết nối được tới %s:%s\n", host, port);
        exf;
    }
    return sock;
}

/*
 * Đọc một "response" hoàn chỉnh từ control socket.
 * FTP trả về dạng "ddd <text>\r\n", hoặc nhiều dòng:
 *   ddd-<dòng đầu>
 *   ...
 *   ddd <dòng cuối>
 * Hàm đọc tới khi gặp dòng kết thúc đúng định dạng, in ra màn hình và
 * trả về mã trạng thái 3 chữ số (vd 220, 331, 230, 227...).
 */
static int ftp_read_reply(int sock) {
    static char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int code = -1;
    char ch;
    int li = 0;

    for (;;) {
        /* Đọc từng dòng (kết thúc bằng '\n') */
        li = 0;
        while (li < (int)sizeof(line) - 1) {
            ssize_t n = recv(sock, &ch, 1, 0);
            if (n <= 0) {
                fprintf(stderr, "Mất kết nối control.\n");
                exf;
            }
            line[li++] = ch;
            if (ch == '\n') break;
        }
        line[li] = '\0';
        fputs(line, stdout);           /* in lại phản hồi server cho dễ theo dõi */

        /* Một dòng phản hồi hợp lệ bắt đầu bằng 3 chữ số. */
        if (li >= 4 && line[0] >= '0' && line[0] <= '9' &&
            line[1] >= '0' && line[1] <= '9' &&
            line[2] >= '0' && line[2] <= '9') {
            int c = (line[0]-'0')*100 + (line[1]-'0')*10 + (line[2]-'0');
            /* Dòng "ddd " (có dấu cách) là dòng kết thúc; "ddd-" là còn tiếp. */
            if (line[3] == ' ') { code = c; break; }
            if (line[3] == '-') code = c;  /* nhớ mã, đọc tiếp */
        }
    }
    (void)buf;
    return code;
}

/*
 * Gửi một lệnh FTP (tự thêm CRLF) rồi đọc & trả về mã phản hồi.
 * Nếu cmd_log != NULL thì in lệnh ra (ẩn nội dung với PASS để bảo mật).
 */
static int ftp_cmd(int sock, const char *fmt, ...) {
    char cmd[BUF_SIZE];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(cmd, sizeof(cmd) - 3, fmt, ap);
    va_end(ap);

    /* In lệnh gửi đi (giấu mật khẩu) */
    if (strncmp(cmd, "PASS", 4) == 0)
        printf(">>> PASS ****\n");
    else
        printf(">>> %s\n", cmd);

    cmd[len++] = '\r';
    cmd[len++] = '\n';
    cmd[len]   = '\0';

    if (send(sock, cmd, len, 0) != len) {
        perror("send");
        exf;
    }
    return ftp_read_reply(sock);
}

/*
 * Vào passive mode: gửi PASV, server trả về 227 với địa chỉ dạng
 *   227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
 * IP = h1.h2.h3.h4, port = p1*256 + p2.
 * Hàm mở kết nối dữ liệu tới đó và trả về data socket.
 */
static int ftp_pasv(int ctrl) {
    char cmd[] = "PASV\r\n";
    printf(">>> PASV\n");
    send(ctrl, cmd, strlen(cmd), 0);

    /* Đọc dòng phản hồi 227 và lấy phần trong ngoặc */
    char line[BUF_SIZE];
    int li = 0;
    char ch;
    while (li < (int)sizeof(line) - 1) {
        if (recv(ctrl, &ch, 1, 0) <= 0) { fprintf(stderr,"Mất kết nối.\n"); exf; }
        line[li++] = ch;
        if (ch == '\n') break;
    }
    line[li] = '\0';
    fputs(line, stdout);

    char *open = strchr(line, '(');
    if (!open) { fprintf(stderr, "PASV phản hồi sai: %s\n", line); exf; }

    int h1, h2, h3, h4, p1, p2;
    if (sscanf(open + 1, "%d,%d,%d,%d,%d,%d", &h1,&h2,&h3,&h4,&p1,&p2) != 6) {
        fprintf(stderr, "Không phân tích được địa chỉ PASV: %s\n", line); exf;
    }

    char ip[32], port[8];
    snprintf(ip,   sizeof(ip),   "%d.%d.%d.%d", h1, h2, h3, h4);
    snprintf(port, sizeof(port), "%d", p1 * 256 + p2);
    printf("    (data -> %s:%s)\n", ip, port);

    return tcp_connect(ip, port);
}

/*
 * Đọc toàn bộ dữ liệu từ data socket vào buffer cấp phát động.
 * Trả về con trỏ buffer (caller tự free) và độ dài qua *out_len.
 */
static char *read_all(int data_sock, size_t *out_len) {
    size_t cap = BUF_SIZE, len = 0;
    char *buf = malloc(cap);
    if (!buf) { perror("malloc"); exf; }

    ssize_t n;
    char tmp[BUF_SIZE];
    while ((n = recv(data_sock, tmp, sizeof(tmp), 0)) > 0) {
        if (len + n > cap) {
            cap = (len + n) * 2;
            buf = realloc(buf, cap);
            if (!buf) { perror("realloc"); exf; }
        }
        memcpy(buf + len, tmp, n);
        len += n;
    }
    *out_len = len;
    return buf;
}

int main(int argc, char *argv[]) {
    const char *host = FTP_HOST;

    /* Nhận MSSV và ngày sinh qua tham số dòng lệnh (bắt buộc) */
    if (argc != 3) {
        fprintf(stderr, "Cách dùng: %s <mssv> <ngày_sinh>\n", argv[0]);
        fprintf(stderr, "Ví dụ:     %s 20235336 15\n", argv[0]);
        exf;
    }
    const char *mssv = argv[1];
    const char *day  = argv[2];

    size_t mlen = strlen(mssv);
    if (mlen < 4) { fprintf(stderr, "MSSV không hợp lệ: %s\n", mssv); exf; }

    /* username = user_<MSSV>;  password = 4 số cuối MSSV + 2 chữ số ngày sinh */
    char user[64], pass[32];
    snprintf(user, sizeof(user), "user_%s", mssv);
    snprintf(pass, sizeof(pass), "%s%02d", mssv + (mlen - 4), atoi(day));

    /* ---- 1. Kết nối + đăng nhập ---- */
    printf("== Kết nối tới %s:%s ==\n", host, FTP_PORT);
    int ctrl = tcp_connect(host, FTP_PORT);
    ftp_read_reply(ctrl);                       /* 220 welcome */

    if (ftp_cmd(ctrl, "USER %s", user) != 331)  /* cần mật khẩu */
        { fprintf(stderr, "USER thất bại.\n"); exf; }
    if (ftp_cmd(ctrl, "PASS %s", pass) != 230)  /* đăng nhập OK */
        { fprintf(stderr, "Đăng nhập thất bại (sai user/pass?).\n"); exf; }

    ftp_cmd(ctrl, "TYPE I");                     /* chế độ nhị phân */

    /* ---- 2. Lấy danh sách file, tìm question_*.txt ---- */
    /* LIST: hiển thị danh sách tập tin trong thư mục hiện tại (RFC959). */
    printf("\n== Lấy danh sách file (LIST) ==\n");
    int data = ftp_pasv(ctrl);
    if (ftp_cmd(ctrl, "LIST") / 100 != 1)        /* 150/125: đang gửi dữ liệu */
        { fprintf(stderr, "LIST thất bại.\n"); exf; }

    size_t list_len;
    char *list = read_all(data, &list_len);
    close(data);
    ftp_read_reply(ctrl);                         /* 226 hoàn tất */

    /* Tách từng dòng (cách nhau bởi \r\n) và tìm chuỗi "question_".
     * LIST trả về dạng "ls -l" nên tên file nằm cuối dòng; ta dò chuỗi
     * "question_" rồi cắt tới khoảng trắng/CR để lấy đúng tên file. */
    char question[256] = {0};
    char *save = NULL;
    for (char *tok = strtok_r(list, "\r\n", &save);
         tok; tok = strtok_r(NULL, "\r\n", &save)) {
        printf("    %s\n", tok);
        char *q = strstr(tok, "question_");
        if (q) {
            int i = 0;
            while (q[i] && q[i] != ' ' && q[i] != '\t' &&
                   q[i] != '\r' && q[i] != '\n' && i < (int)sizeof(question) - 1) {
                question[i] = q[i];
                i++;
            }
            question[i] = '\0';
        }
    }
    free(list);

    if (question[0] == '\0') {
        fprintf(stderr, "Không tìm thấy file question_xxxxxx.txt trên server.\n");
        exf;
    }
    printf("--> Tìm thấy: %s\n", question);

    /* ---- 3. Tải nội dung file question ---- */
    printf("\n== Tải file %s ==\n", question);
    data = ftp_pasv(ctrl);
    if (ftp_cmd(ctrl, "RETR %s", question) / 100 != 1)
        { fprintf(stderr, "RETR thất bại.\n"); exf; }

    size_t qlen;
    char *qcontent = read_all(data, &qlen);
    close(data);
    ftp_read_reply(ctrl);                         /* 226 */

    /* Lưu lại file question về máy (để đối chiếu) */
    FILE *fq = fopen(question, "wb");
    if (fq) { fwrite(qcontent, 1, qlen, fq); fclose(fq); }
    printf("Nội dung (%zu byte): %.*s\n", qlen, (int)qlen, qcontent);

    /* ---- 4. Tạo file answer với nội dung đảo ngược ---- */
    /* Tên answer = thay "question" bằng "answer" trong tên file. */
    char answer[256];
    snprintf(answer, sizeof(answer), "answer_%s", question + 9); /* bỏ "question_" */

    char *acontent = malloc(qlen ? qlen : 1);
    if (!acontent) { perror("malloc"); exf; }
    for (size_t i = 0; i < qlen; i++)
        acontent[i] = qcontent[qlen - 1 - i];     /* đảo ngược */

    FILE *fa = fopen(answer, "wb");
    if (!fa) { perror("fopen answer"); exf; }
    fwrite(acontent, 1, qlen, fa);
    fclose(fa);
    printf("\n== Tạo %s (đảo ngược) ==\n", answer);
    printf("Nội dung: %.*s\n", (int)qlen, acontent);

    /* ---- 5. Upload file answer lên server ---- */
    printf("\n== Upload %s ==\n", answer);
    data = ftp_pasv(ctrl);
    if (ftp_cmd(ctrl, "STOR %s", answer) / 100 != 1)
        { fprintf(stderr, "STOR thất bại.\n"); exf; }

    /* Gửi toàn bộ nội dung answer qua data socket */
    size_t sent = 0;
    while (sent < qlen) {
        ssize_t n = send(data, acontent + sent, qlen - sent, 0);
        if (n <= 0) { perror("send data"); exf; }
        sent += n;
    }
    close(data);                                  /* đóng để server biết hết dữ liệu */
    ftp_read_reply(ctrl);                          /* 226 upload xong */

    /* ---- Kết thúc ---- */
    ftp_cmd(ctrl, "QUIT");
    close(ctrl);

    free(qcontent);
    free(acontent);
    printf("\n== Hoàn tất! Đã upload %s lên server. ==\n", answer);
    return 0;
}
