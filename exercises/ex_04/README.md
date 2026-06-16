# Bài tập lớn — Chat Server / Client (Our chat protocol)

Ứng dụng chat theo giao thức tự định nghĩa mô tả trong slide *Lập trình mạng*
(Chương 4 — Thiết kế giao thức mạng, slide 262–274), tham khảo *“The Definitive
Guide to Linux Network Programming”* — mục **Our chat protocol**.

| Thành phần | File | Vai trò |
|------------|------|---------|
| Chat **Server** | [`chat_server.c`](chat_server.c) | Dùng `select()`, phục vụ nhiều client, xử lý lần lượt các yêu cầu |
| Chat **Client** | [`chat_client.c`](chat_client.c) | Giao diện dòng lệnh, `select()` ghép bàn phím + socket |

- **Sinh viên (server):** Nguyen Huy Hoang — MSSV: **20235336**
- **Bạn cùng nhóm (client):** Tran Duc Bao — MSSV: **20235271**

---

## 1. Đặc tả giao thức

- Thông điệp ở mức **text**, mỗi lệnh/đáp ứng kết thúc bằng `\n` (LF).
- Làm việc theo **phiên**, server xử lý **lần lượt**, không xác thực, không mã hoá.
- Người **JOIN đầu tiên** là **chủ phòng**. Các lệnh `OP/KICK/TOPIC` chỉ chủ
  phòng mới được dùng (người khác nhận `203 DENIED`).

### Lệnh client → server

| Lệnh | Ý nghĩa |
|------|---------|
| `JOIN <nick>` | Tham gia phòng. `nick` chỉ gồm chữ thường + số, không trùng |
| `MSG <message>` | Nhắn cho cả phòng |
| `PMSG <nick> <message>` | Nhắn riêng cho một người |
| `OP <nick>` | Chuyển quyền chủ phòng (chỉ chủ phòng) |
| `KICK <nick>` | Đuổi một người (chỉ chủ phòng) |
| `TOPIC <topic>` | Đặt chủ đề, có thể chứa dấu cách (chỉ chủ phòng) |
| `QUIT` | Thoát phòng |

### Phản hồi server → người gửi lệnh

`100 OK` · `200 NICKNAME IN USE` · `201 INVALID NICK NAME` ·
`202 UNKNOWN NICKNAME` · `203 DENIED` · `999 UNKNOWN ERROR`

### Thông điệp server chủ động gửi (broadcast cho mọi người **trừ** người gửi)

| Thông điệp | Khi nào |
|------------|---------|
| `JOIN <nick>` | Có người tham gia |
| `MSG <nick> <message>` | Có người nhắn cho phòng |
| `PMSG <nick> <message>` | Có tin nhắn riêng (chỉ gửi cho người nhận) |
| `OP <nick>` | Quyền chủ phòng được chuyển |
| `KICK <kicked> <op>` | Có người bị đuổi (gửi cả cho người bị đuổi) |
| `TOPIC <op> <topic>` | Chủ đề thay đổi |
| `QUIT <nick>` | Có người thoát |

---

## 2. Biên dịch & chạy

```bash
make                       # build chat_server và chat_client

./chat_server [port]       # mặc định 9000
./chat_client [host] [port] [nickname]   # mặc định 127.0.0.1 9000, thiếu nick sẽ hỏi
```

Trong client, gõ nội dung bất kỳ rồi Enter để nhắn cho cả phòng; hoặc dùng lệnh tắt:

```
/pmsg <nick> <nội dung>     /op <nick>     /kick <nick>
/topic <chủ đề>             /raw <dòng giao thức thô>     /help     /quit
```

---

## 3. Chạy file kiểm thử của thầy (`chat_server_test`)

File kiểm thử là **ELF Linux x86-64**. Có 2 cách:

### a) Trên máy Linux x86-64 (máy lab) — không cần Docker
```bash
gcc -O2 -o chat_server chat_server.c
./chat_server 9000 &
chmod +x chat_server_test
./chat_server_test          # Enter 2 lần để chọn mặc định 127.0.0.1 / 9000
```

### b) Trên macOS (Apple Silicon) — qua Docker
```bash
./run_official_test.sh      # tự tải file test, dựng image Linux, chạy
```

Kết quả mong đợi: **`Tong so test: 29, that bai: 0`** (toàn bộ `[PASS]`).
Ảnh chụp kết quả lưu kèm trong thư mục này (xem mục screenshots).

> Kiểm thử nhanh trên macOS không cần Docker: `python3 test_protocol.py`
> (mô phỏng đúng 29 phép thử của file test gốc).

---

## 4. Screenshots cần nộp

1. `test_result*.png` — kết quả chạy `chat_server_test` (29/29 PASS).
2. `chat_demo*.png` — server + 2 client trò chuyện (JOIN, MSG, PMSG, OP, TOPIC, KICK, QUIT).
