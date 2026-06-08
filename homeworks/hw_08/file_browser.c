/**
 * @file file_browser.c
 * @author Nguyen Huy Hoang (nhh4075@gmail.com)
 * @brief Bài 2 - Máy chủ HTTP duyệt file/thư mục.
 *        - Yêu cầu thư mục: trả về trang liệt kê các liên kết tới thư mục con
 *          và file (thư mục IN ĐẬM, file IN NGHIÊNG).
 *        - Yêu cầu file: trả về nội dung file kèm Content-Type và Content-Length.
 *          Hỗ trợ văn bản, ảnh, audio, video.
 * @date 2026-06-08
 *
 * Cách dùng: ./file_browser <port> [thư_mục_gốc]   (mặc định thư mục gốc = ".")
 * Biên dịch: gcc -o file_browser file_browser.c
 *
 * Thử nghiệm:  http://localhost:8080/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define exf       exit(EXIT_FAILURE)
#define BACKLOG   16
#define BUF_SIZE  8192
#define LIST_MAX  65536          /* buffer tối đa cho trang liệt kê thư mục */

static char g_root[PATH_MAX];    /* đường dẫn tuyệt đối của thư mục gốc */

/* ----------------------------------------------------------------------------
 * URL-decode: '%XX' -> ký tự, '+' -> dấu cách. (giống Bài 1)
 * -------------------------------------------------------------------------- */
static void url_decode(const char *src, char *dst) {
    while (*src) {
        if (*src == '%' && isxdigit((unsigned char)src[1])
                        && isxdigit((unsigned char)src[2])) {
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

/* ----------------------------------------------------------------------------
 * URL-encode cho thuộc tính href: chỉ giữ lại các ký tự an toàn, còn lại mã hoá
 * thành '%XX'. Giữ nguyên '/' để đường dẫn vẫn hợp lệ. (vd tên file có dấu cách)
 * -------------------------------------------------------------------------- */
static void url_encode(const char *src, char *dst, size_t dst_sz) {
    const char *safe = "-_.~/";
    size_t i = 0;
    while (*src && i + 3 < dst_sz) {
        unsigned char c = (unsigned char)*src;
        if (isalnum(c) || strchr(safe, c)) {
            dst[i++] = c;
        } else {
            i += snprintf(dst + i, dst_sz - i, "%%%02X", c);
        }
        src++;
    }
    dst[i] = '\0';
}

/* ----------------------------------------------------------------------------
 * HTML-escape: chống XSS qua tên file (vd file tên "<script>"). (giống Bài 1)
 * -------------------------------------------------------------------------- */
static void html_escape(const char *src, char *dst, size_t dst_sz) {
    size_t i = 0;
    while (*src && i + 6 < dst_sz) {
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

/* ----------------------------------------------------------------------------
 * Đoán Content-Type từ đuôi file (phần mở rộng). Trả về chuỗi MIME.
 * -------------------------------------------------------------------------- */
static const char *get_mime(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    ext++;                                   /* bỏ qua dấu '.' */

    /* văn bản */
    if (!strcasecmp(ext, "html") || !strcasecmp(ext, "htm")) return "text/html; charset=utf-8";
    if (!strcasecmp(ext, "txt")  || !strcasecmp(ext, "c")   ||
        !strcasecmp(ext, "h")    || !strcasecmp(ext, "md")  ||
        !strcasecmp(ext, "log")  || !strcasecmp(ext, "css") ||
        !strcasecmp(ext, "js"))                               return "text/plain; charset=utf-8";
    /* ảnh */
    if (!strcasecmp(ext, "jpg")  || !strcasecmp(ext, "jpeg")) return "image/jpeg";
    if (!strcasecmp(ext, "png"))                              return "image/png";
    if (!strcasecmp(ext, "gif"))                              return "image/gif";
    if (!strcasecmp(ext, "svg"))                              return "image/svg+xml";
    if (!strcasecmp(ext, "webp"))                             return "image/webp";
    /* audio */
    if (!strcasecmp(ext, "mp3"))                              return "audio/mpeg";
    if (!strcasecmp(ext, "wav"))                              return "audio/wav";
    if (!strcasecmp(ext, "ogg"))                              return "audio/ogg";
    /* video */
    if (!strcasecmp(ext, "mp4"))                              return "video/mp4";
    if (!strcasecmp(ext, "webm"))                             return "video/webm";
    /* khác */
    if (!strcasecmp(ext, "pdf"))                              return "application/pdf";

    return "application/octet-stream";       /* mặc định: tải về */
}

/* ----------------------------------------------------------------------------
 * Gửi status-line + header. Nếu content_length < 0 thì bỏ qua Content-Length
 * (dùng Connection: close để báo kết thúc body).
 * -------------------------------------------------------------------------- */
static void send_header(int client, const char *status,
                        const char *ctype, long content_length) {
    char h[512];
    int n;
    if (content_length >= 0)
        n = snprintf(h, sizeof h,
            "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n\r\n",
            status, ctype, content_length);
    else
        n = snprintf(h, sizeof h,
            "HTTP/1.1 %s\r\n"
            "Content-Type: %s\r\n"
            "Connection: close\r\n\r\n",
            status, ctype);
    send(client, h, n, 0);
}

/* Gửi một trang lỗi đơn giản. */
static void send_error(int client, const char *status) {
    char body[256];
    int n = snprintf(body, sizeof body,
        "<html><body><meta charset='utf-8'><h1>%s</h1></body></html>", status);
    send_header(client, status, "text/html; charset=utf-8", n);
    send(client, body, n, 0);
}

/* ----------------------------------------------------------------------------
 * Liệt kê nội dung thư mục 'fullpath' (đường dẫn trên đĩa) dưới dạng HTML.
 * 'urlpath' là đường dẫn URL hiện tại (luôn kết thúc bằng '/').
 * Thư mục -> IN ĐẬM <b>, file -> IN NGHIÊNG <i>.
 * -------------------------------------------------------------------------- */
static void send_listing(int client, const char *fullpath, const char *urlpath) {
    DIR *dir = opendir(fullpath);
    if (!dir) { send_error(client, "403 Forbidden"); return; }

    char *page = malloc(LIST_MAX);
    if (!page) { closedir(dir); send_error(client, "500 Internal Server Error"); return; }

    /* urlpath do người dùng kiểm soát -> tạo 2 bản an toàn:
         - url_html: html_escape, dùng trong text (<title>/<h1>)
         - url_attr: url_encode,  dùng trong thuộc tính href */
    char url_html[1024 * 6];
    char url_attr[1024 * 3];
    html_escape(urlpath, url_html, sizeof url_html);
    url_encode(urlpath, url_attr, sizeof url_attr);

    size_t len = 0;
    len += snprintf(page + len, LIST_MAX - len,
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<title>Index of %s</title></head><body>"
        "<h1>Index of %s</h1><ul>", url_html, url_html);

    /* Link quay lại thư mục cha (nếu chưa ở gốc) */
    if (strcmp(urlpath, "/") != 0)
        len += snprintf(page + len, LIST_MAX - len,
            "<li><a href='../'><b>../</b></a></li>");

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;                        /* bỏ qua '.' và '..' */

        /* Ghép đường dẫn đầy đủ của mục con để stat() */
        char child[PATH_MAX];
        snprintf(child, sizeof child, "%s/%s", fullpath, ent->d_name);

        struct stat st;
        if (stat(child, &st) < 0) continue;
        int is_dir = S_ISDIR(st.st_mode);

        /* href = urlpath + tên (đã url_encode), thêm '/' nếu là thư mục */
        char enc[PATH_MAX * 3];
        url_encode(ent->d_name, enc, sizeof enc);

        /* tên hiển thị -> html_escape để tránh XSS */
        char disp[PATH_MAX * 6];
        html_escape(ent->d_name, disp, sizeof disp);

        if (is_dir)
            len += snprintf(page + len, LIST_MAX - len,
                "<li><a href='%s%s/'><b>%s/</b></a></li>", url_attr, enc, disp);
        else
            len += snprintf(page + len, LIST_MAX - len,
                "<li><a href='%s%s'><i>%s</i></a> (%lld bytes)</li>",
                url_attr, enc, disp, (long long)st.st_size);

        if (len >= LIST_MAX - 512) break;    /* gần đầy buffer thì dừng */
    }
    closedir(dir);

    len += snprintf(page + len, LIST_MAX - len, "</ul></body></html>");

    send_header(client, "200 OK", "text/html; charset=utf-8", (long)len);
    send(client, page, len, 0);
    free(page);
}

/* ----------------------------------------------------------------------------
 * Gửi nội dung một file: header (Content-Type + Content-Length) rồi gửi byte.
 * Đọc/gửi theo khối nên dùng được cả file nhị phân (ảnh/audio/video).
 * -------------------------------------------------------------------------- */
static void send_file(int client, const char *fullpath, off_t size) {
    int fd = open(fullpath, O_RDONLY);
    if (fd < 0) { send_error(client, "403 Forbidden"); return; }

    send_header(client, "200 OK", get_mime(fullpath), (long)size);

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        ssize_t sent = 0;
        while (sent < n) {                   /* gửi hết khối vừa đọc */
            ssize_t m = send(client, buf + sent, n - sent, 0);
            if (m <= 0) { close(fd); return; }
            sent += m;
        }
    }
    close(fd);
}

/* ----------------------------------------------------------------------------
 * Xử lý 1 kết nối: đọc request, lấy URI, điều hướng tới thư mục/file.
 * -------------------------------------------------------------------------- */
static void handle_client(int client, struct sockaddr_in *cl) {
    char buf[BUF_SIZE];
    int n = read(client, buf, sizeof buf - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    char method[8] = {0}, rawuri[1024] = {0};
    sscanf(buf, "%7s %1023s", method, rawuri);
    printf("[%s:%d] %s %s\n",
           inet_ntoa(cl->sin_addr), ntohs(cl->sin_port), method, rawuri);

    if (strcmp(method, "GET") != 0) {
        send_error(client, "405 Method Not Allowed");
        return;
    }

    /* Bỏ query string (nếu có) rồi URL-decode đường dẫn */
    char *q = strchr(rawuri, '?');
    if (q) *q = '\0';
    char urlpath[1024];
    url_decode(rawuri, urlpath);

    /* Ghép thành đường dẫn trên đĩa: g_root + urlpath */
    char fullpath[PATH_MAX];
    snprintf(fullpath, sizeof fullpath, "%s%s", g_root, urlpath);

    /* Chống path traversal: chuẩn hoá đường dẫn thật, phải nằm trong g_root */
    char resolved[PATH_MAX];
    if (!realpath(fullpath, resolved)) {
        send_error(client, "404 Not Found");
        return;
    }
    size_t rootlen = strlen(g_root);
    /* Phải trùng prefix g_root VÀ ký tự kế tiếp là '/' hoặc hết chuỗi.
       Tránh trường hợp "/root-secret" lọt qua vì có prefix "/root". */
    if (strncmp(resolved, g_root, rootlen) != 0 ||
        (resolved[rootlen] != '/' && resolved[rootlen] != '\0')) {
        send_error(client, "403 Forbidden");   /* đã thoát ra ngoài thư mục gốc */
        return;
    }

    struct stat st;
    if (stat(resolved, &st) < 0) {
        send_error(client, "404 Not Found");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        /* Đảm bảo urlpath kết thúc bằng '/' để href của mục con đúng */
        char dirurl[1024];
        size_t ul = strlen(urlpath);
        if (ul == 0 || urlpath[ul - 1] != '/')
            snprintf(dirurl, sizeof dirurl, "%s/", urlpath);
        else
            snprintf(dirurl, sizeof dirurl, "%s", urlpath);
        send_listing(client, resolved, dirurl);
    } else if (S_ISREG(st.st_mode)) {
        send_file(client, resolved, st.st_size);
    } else {
        send_error(client, "403 Forbidden");   /* không phải file thường/thư mục */
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* Bỏ qua SIGPIPE: khi trình duyệt đóng kết nối sớm (vd tua/đóng video),
       send() sẽ trả về -1 (EPIPE) thay vì giết tiến trình. */
    signal(SIGPIPE, SIG_IGN);

    if (argc < 2 || argc > 3) {
        printf("Usage: %s <port> [thu_muc_goc]\n", argv[0]);
        exf;
    }

    /* Chuẩn hoá thư mục gốc thành đường dẫn tuyệt đối */
    const char *root = (argc == 3) ? argv[2] : ".";
    if (!realpath(root, g_root)) {
        perror("realpath(root) failed");
        exf;
    }

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0) { perror("socket() failed"); exf; }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

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

    printf("File browser listening on port %s, root = %s\n", argv[1], g_root);

    while (1) {
        struct sockaddr_in cl;
        socklen_t cl_len = sizeof cl;
        int client = accept(listener, (struct sockaddr *)&cl, &cl_len);
        if (client < 0) { perror("accept() failed"); continue; }

        handle_client(client, &cl);
        close(client);
    }

    close(listener);
    return 0;
}
