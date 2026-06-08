/**
 * @file calc_server.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Bài 1 - Web thực hiện cộng/trừ/nhân/chia qua HTTP.
 *        Nhận tham số từ cả GET (query string) lẫn POST (body).
 *        Tham số: op (toán tử) + 2 toán hạng a, b.
 * @date 2026-06-08
 *
 * Cách dùng: ./calc_server <port>
 * Biên dịch: gcc -o calc_server calc_server.c
 *
 * Thử nghiệm:
 *   Trình duyệt:  http://localhost:8080/
 *   GET (curl):   curl -v "http://localhost:8080/?op=add&a=3&b=5"
 *   POST (curl):  curl -v -d "op=div&a=10&b=4" http://localhost:8080/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define exf       exit(EXIT_FAILURE)
#define BACKLOG   16
#define BUF_SIZE  8192

/* 
 * URL-decode: trình duyệt mã hoá dấu cách thành '+' hoặc '%20', ký tự đặc biệt
 * thành '%XX'. Hàm này giải mã ngược lại.
 */
static void url_decode(const char *src, char *dst) {
    while (*src) {
        if (
            *src == '%' &&
            isxdigit((unsigned char)src[1]) &&
            isxdigit((unsigned char)src[2])
        ) {
            char hex[3] = { src[1], src[2], '\0' };
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* 
 * Lấy giá trị của một tham số trong chuỗi "op=add&a=3&b=5".
 * Trả về 1 nếu tìm thấy (kết quả đã url_decode vào 'out'), 0 nếu không.
 */
static int get_param(const char *qs, const char *key, char *out, size_t out_sz) {
    size_t klen = strlen(key);
    for (const char *p = qs; p && *p; ) {
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            const char *v   = p + klen + 1;
            const char *end = strchr(v, '&');
            size_t vlen = end ? (size_t)(end - v) : strlen(v);

            char raw[256];
            if (vlen >= sizeof raw) vlen = sizeof raw - 1;
            memcpy(raw, v, vlen);
            raw[vlen] = '\0';

            url_decode(raw, out);
            (void)out_sz;            /* out đủ lớn (>=256) trong phạm vi bài này */
            return 1;
        }
        p = strchr(p, '&');
        if (p) p++;
    }
    return 0;
}

/* 
 * HTML-escape: chống XSS. Đổi các ký tự <, >, &, ", ' thành entity tương ứng
 * trước khi nhúng giá trị do người dùng nhập vào trang HTML trả về.
*/
static void html_escape(const char *src, char *dst, size_t dst_sz) {
    size_t i = 0;
    while (*src && i + 6 < dst_sz) {        /* chừa 6 byte đủ cho "&quot;" */
        const char *ent = NULL;
        switch (*src) {
            case '<':  ent = "&lt;";   break;
            case '>':  ent = "&gt;";   break;
            case '&':  ent = "&amp;";  break;
            case '"':  ent = "&quot;"; break;
            case '\'': ent = "&#39;";  break;
            default:   dst[i++] = *src; src++; continue;
        }
        size_t elen = strlen(ent);
        memcpy(dst + i, ent, elen);
        i += elen;
        src++;
    }
    dst[i] = '\0';
}

/* 
 * Thực hiện phép tính. Trả về:  1 = OK,  0 = toán tử sai,  -1 = chia cho 0.
 */
static int calculate(const char *op, double a, double b, double *res, char *sym) {
    if (!strcmp(op, "add") || !strcmp(op, "+")) {
        *res = a + b; *sym = '+'; return 1;
    }

    if (!strcmp(op, "sub") || !strcmp(op, "-")) {
        *res = a - b; *sym = '-'; return 1;
    }

    if (!strcmp(op, "mul") || !strcmp(op, "*")) {
        *res = a * b; *sym = '*'; return 1;
    }

    if (!strcmp(op, "div") || !strcmp(op, "/")) {
        if (b == 0) return -1;
        *res = a / b; *sym = '/'; return 1;
    }

    return 0;
}

/* 
 * Gửi một HTTP response hoàn chỉnh: status-line + Content-Type + Content-Length
 * + dòng trống + body. (Cấu trúc response: slide tr.239-241)
 */
static void send_html(int client, const char *status, const char *body) {
    char header[512];
    int  hlen = snprintf(header, sizeof header,
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n",
        status, strlen(body));
    send(client, header, hlen, 0);
    send(client, body, strlen(body), 0);
}

/* Trang chủ: form cho người dùng nhập (1 form GET + 1 form POST). */
static void send_form(int client) {
    const char *body =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>Calc Server</title></head><body>"
        "<h1>Máy tính qua HTTP</h1>"

        "<h2>Phương thức GET</h2>"
        "<form method='GET' action='/'>"
        "  a: <input name='a' value='3'> "
        "  <select name='op'>"
        "    <option value='add'>+</option><option value='sub'>-</option>"
        "    <option value='mul'>*</option><option value='div'>/</option>"
        "  </select> "
        "  b: <input name='b' value='5'> "
        "  <button type='submit'>Tính</button>"
        "</form>"

        "<h2>Phương thức POST</h2>"
        "<form method='POST' action='/'>"
        "  a: <input name='a' value='10'> "
        "  <select name='op'>"
        "    <option value='add'>+</option><option value='sub'>-</option>"
        "    <option value='mul'>*</option><option value='div'>/</option>"
        "  </select> "
        "  b: <input name='b' value='4'> "
        "  <button type='submit'>Tính</button>"
        "</form>"
        "</body></html>";
    send_html(client, "200 OK", body);
}

/* Đọc 3 tham số từ 'params', tính toán, và trả về trang kết quả. */
static void handle_calc(int client, const char *method, const char *params) {
    char op[64] = {0}, sa[64] = {0}, sb[64] = {0};

    int has_op = get_param(params, "op", op, sizeof op);
    int has_a  = get_param(params, "a",  sa, sizeof sa);
    int has_b  = get_param(params, "b",  sb, sizeof sb);

    char body[1024];

    if (!has_op || !has_a || !has_b) {
        snprintf(body, sizeof body,
            "<html><body><meta charset='utf-8'><h1>Lỗi 400</h1>"
            "<p>Thiếu tham số. Cần op, a, b.</p>"
            "<a href='/'>Quay lại</a></body></html>");
        send_html(client, "400 Bad Request", body);
        return;
    }

    double a = atof(sa), b = atof(sb), res = 0;
    char sym = '?';
    int rc = calculate(op, a, b, &res, &sym);

    if (rc == 1) {
        snprintf(body, sizeof body,
            "<html><body><meta charset='utf-8'>"
            "<h1>Kết quả (%s)</h1>"
            "<p style='font-size:24px'>%g %c %g = <b>%g</b></p>"
            "<a href='/'>Tính tiếp</a></body></html>",
            method, a, sym, b, res);
        send_html(client, "200 OK", body);
    } else if (rc == -1) {
        snprintf(body, sizeof body,
            "<html><body><meta charset='utf-8'><h1>Lỗi</h1>"
            "<p>Không thể chia cho 0.</p>"
            "<a href='/'>Quay lại</a></body></html>");
        send_html(client, "400 Bad Request", body);
    } else {
        char op_safe[256];
        html_escape(op, op_safe, sizeof op_safe);   /* chống XSS phản chiếu */
        snprintf(body, sizeof body,
            "<html><body><meta charset='utf-8'><h1>Lỗi</h1>"
            "<p>Toán tử không hợp lệ: %s</p>"
            "<a href='/'>Quay lại</a></body></html>", op_safe);
        send_html(client, "400 Bad Request", body);
    }
}

/* Xử lý 1 kết nối: đọc request, phân loại GET/POST, điều hướng. */
static void handle_client(int client, struct sockaddr_in *cl) {
    char buf[BUF_SIZE];
    int  n = read(client, buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    char method[8] = {0}, uri[1024] = {0};
    sscanf(buf, "%7s %1023s", method, uri);
    printf("[%s:%d] %s %s\n",
           inet_ntoa(cl->sin_addr), ntohs(cl->sin_port), method, uri);

    if (strcmp(method, "GET") == 0) {
        char *q = strchr(uri, '?');           /* query nằm sau dấu '?' */
        if (q && q[1])
            handle_calc(client, "GET", q + 1);
        else
            send_form(client);                /* GET '/' không query -> form */

    } else if (strcmp(method, "POST") == 0) {
        /* Body nằm sau dòng trống "\r\n\r\n" */
        char *body = strstr(buf, "\r\n\r\n");
        if (body) {
            body += 4;

            /* Nếu read() đầu tiên chưa lấy hết body, đọc tiếp cho đủ
               Content-Length (form nhỏ thường đã đủ trong 1 lần read). */
            char *cl_hdr = strstr(buf, "Content-Length:");
            if (cl_hdr) {
                int want = atoi(cl_hdr + 15);
                int have = n - (int)(body - buf);
                while (have < want && n < BUF_SIZE - 1) {
                    int m = read(client, buf + n, BUF_SIZE - 1 - n);
                    if (m <= 0) break;
                    n += m; buf[n] = '\0';
                    have += m;
                }
            }
            handle_calc(client, "POST", body);
        } else {
            send_form(client);
        }
    } else {
        send_html(
            client, "405 Method Not Allowed",
            "<html><body><meta charset='utf-8'><h1>405</h1>"
            "<p>Chỉ hỗ trợ GET/POST.</p></body></html>"
        );
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Bỏ qua SIGPIPE: tránh bị giết tiến trình khi client đóng kết nối sớm. */
    signal(SIGPIPE, SIG_IGN);

    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exf;
    }

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) {
        perror("socket() failed"); exf;
    }

    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt) < 0) {
        perror("setsockopt() failed"); exf;
    }

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(atoi(argv[1]));

    if (bind(listener, (struct sockaddr *)&addr, sizeof addr) < 0) {
        perror("bind() failed"); exf;
    }
    if (listen(listener, BACKLOG) < 0) {
        perror("listen() failed"); exf;
    }

    printf("Calc server listening on port %s\n", argv[1]);

    while (1) {
        struct sockaddr_in cl;
        socklen_t cl_len = sizeof cl;
        int client = accept(listener, (struct sockaddr *)&cl, &cl_len);
        if (client < 0) { perror("accept() failed"); continue; }

        handle_client(client, &cl);
        close(client);  // HTTP stateless
    }

    close(listener);
    return 0;
}
