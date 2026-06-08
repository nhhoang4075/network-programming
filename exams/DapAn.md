# Đáp án đề thi Lập trình mạng (IT4060)

> Tổng hợp đáp án cho file `LTM_De-Thi.pdf`. Code các câu tự luận nằm trong thư mục `exams/code/`.
> Các chỗ có dấu (⚠) là đáp án cần lưu ý / khác với đáp án đã tô sẵn trong đề.

---

## 1. Ngân hàng câu hỏi (Lương Ánh Hoàng) — 95 câu

Đây là nguồn gốc của mọi mã đề trắc nghiệm. Học thuộc bảng này là đủ.

| Câu | ĐA | Câu | ĐA | Câu | ĐA | Câu | ĐA | Câu | ĐA |
|----|----|----|----|----|----|----|----|----|----|
| 1 | d | 20 | c | 39 | d | 58 | d | 77 | d |
| 2 | b | 21 | d | 40 | c | 59 | a | 78 | a |
| 3 | b | 22 | a | 41 | d | 60 | a | 79 | a |
| 4 | b | 23 | a | 42 | d | 61 | b | 80 | b |
| 5 | d | 24 | d | 43 | d | 62 | a | 81 | c |
| 6 | d | 25 | c | 44 | a | 63 | b | 82 | a |
| 7 | a | 26 | a | 45 | d | 64 | d | 83 | a |
| 8 | d | 27 | c | 46 | b | 65 | d | 84 | b |
| 9 | a | 28 | d | 47 | c | 66 | a | 85 | a |
| 10 | c | 29 | d | 48 | b ⚠ | 67 | c | 86 | c |
| 11 | d | 30 | d | 49 | b | 68 | c | 87 | a |
| 12 | b | 31 | a | 50 | b | 69 | d | 88 | b |
| 13 | c | 32 | d | 51 | d | 70 | b | 89 | d |
| 14 | b | 33 | a | 52 | a | 71 | a | 90 | d |
| 15 | b | 34 | d ⚠ | 53 | d | 72 | b | 91 | b |
| 16 | b | 35 | d | 54 | d | 73 | c | 92 | b |
| 17 | d | 36 | d | 55 | b | 74 | d | 93 | a |
| 18 | b | 37 | d | 56 | c | 75 | d | 94 | b |
| 19 | a | 38 | d | 57 | b | 76 | b | 95 | d |

### Giải thích các câu quan trọng

- **1**: DLL là `ws2_32.dll` → `WS2_32.DLL`.
- **2 / 3 / 30**: header `Winsock2.h`; lib liên kết `WS2_32.LIB`; phân giải tên miền cần thêm `Ws2tcpip.h`.
- **13 / 14**: thêm *driver thiết bị* → tầng **Transport Protocol**; thêm *giao thức mới* → tầng **Provider**.
- **16**: giao thức hướng dòng (TCP) **không** bảo toàn biên thông điệp.
- **18**: `WSADATA` để **nhận** thông tin phiên bản WinSock có trên hệ thống (phiên bản muốn dùng truyền qua tham số `wVersionRequested`).
- **21**: `WSAGetLastError()` + `closesocket(s)` + `WSACleanup()`.
- **26**: dữ liệu >1 byte trong cấu trúc địa chỉ lưu theo **đầu to** (network byte order).
- **27 / 28**: `inet_addr("...")` đổi xâu→IP; `inet_ntoa(addr.sin_addr)` đổi IP→xâu.
- **29**: `htons` đổi cổng host→network (kiểu short).
- **34** ⚠: đáp án đề là **d (cả ba)**, nhưng kỹ thuật *tường lửa KHÔNG làm `bind` thất bại*. Thực tế chỉ b (cổng đã bị chiếm) và c (socket không hợp lệ) gây lỗi.
- **40 / 41**: đồng bộ chặn **chỉ luồng chứa lời gọi**; bất đồng bộ **không chặn** luồng nào.
- **43**: `recv` đồng bộ KHÔNG chặn khi: có dữ liệu (kể cả <1024 byte), đủ 1024 byte, hoặc kết nối đã đóng → cả ba.
- **44 / 45**: 10 kết nối cần **11** socket (1 listen + 10); blocking gửi tập trung 1 luồng cho 100 kết nối cần **101** luồng.
- **46 / 47**: `connect` thành công báo qua **writefds**; dữ liệu OOB báo qua **exceptfds**.
- **48** ⚠: với 100 kết nối, do `FD_SETSIZE = 64` (Winsock) nên cần **2** luồng/2 tập `fd_set`. (Nếu thầy hỏi "select dùng mấy luồng để theo dõi nhiều socket" theo nghĩa chung thì là 1.)
- **52 / 53**: mỗi socket cần 1 `WSAEVENT` → 10 socket = **10**; `WSACreateEvent` tạo event **non-signaled, manual-reset**.
- **55**: socket client kết nối server đăng ký `FD_CONNECT | FD_WRITE | FD_READ | FD_CLOSE`.
- **58 / 59 / 60**: completion routine do **HĐH gọi trong cùng luồng** đã phát yêu cầu I/O; alertable = luồng **đang ngủ & sẵn sàng** chạy callback; đưa về alertable bằng **SleepEx** (`Sleep` thường KHÔNG alertable).
- **61–65**: `CSocket` **dẫn xuất** từ `CAsyncSocket`; CSocket **đồng bộ**, CAsyncSocket **bất đồng bộ**; không dùng object socket chéo luồng (64→d); xử lý sự kiện CAsyncSocket bằng cách **kế thừa + override** (OnReceive, OnConnect...).

### Đoạn code điền chỗ trống (66–95)

**66–72 — Nhận gói Voice/Text (TCP, struct `{char type; int len; char data[65536];}`)**
- 66 `<A>=1` (type là `char`, 1 byte)
- 67 `<B>=4` (len là `int`, 4 byte)
- 68 `<C>=PACKET_TYPE_TEXT` (vì in `"Text:%s"`)
- 69 `<D>=` Phương án khác (đúng phải là `p.data+total`, không có trong lựa chọn)
- 70 `<E>=p.len - total` (số byte còn lại cần nhận)
- 71 `<F>=len` (cộng dồn số byte vừa nhận)
- 72 `<G>=0` (kết thúc chuỗi `\0`)

**73–79 — Nhận UDP + kiểm checksum (XOR)**
- 73 `<A>=sizeof(from)`
- 74 `<B>=total < p.len`
- 75 `<C>=` Cả ba đều sai (đúng phải là `p.data+total`)
- 76 `<D>=p.len - total`
- 77 `<E>=(sockaddr*)&from`
- 78 `<F>=^` (toán tử XOR)
- 79 `<G>=i*2` (mỗi `unsigned short` = 2 byte)

**80–86 — Server quản lý kết nối bằng danh sách liên kết kép**
- 80 `<A>=c`  (lưu kết quả `accept`)
- 81 `<B>=&cAddrLen`
- 82 `<C>=new Connection`
- 83 `<D>=pTmp`  (`pCur->pNext = pTmp`)
- 84 `<E>=pCur`  (`pTmp->pPrev = pCur`)
- 85 `<F>=c`   (`pTmp->s = c`)
- 86 `<G>=pTmp` (tham số cho `ReceiverThread`)

**87–90 — Phân giải tên miền bằng `getaddrinfo`**
- 87 `<A>="8888"` (service là **xâu**)
- 88 `<B>=&info`
- 89 `<C>=pCur->ai_addr`
- 90 `<D>=inet_ntoa(addr.sin_addr)`

**91–95 — HTTP server tách URL (đọc tới `\r\n\r\n`)**
- 91 `<A>=command+len`
- 92 `<B>=len`
- 93 `<C>===` (`if (pos == 0)` → không tìm thấy → Invalid)
- 94 `<D>=command+4` (bỏ `"GET "`)
- 95 `<E>=pos-command-4`

---

## 2. Mã đề trắc nghiệm

### Đề 515 (40 câu)
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|----|----|----|----|----|
| d | b | b | d | b | a | d | a | c | d | b | c | b | b | b | d | b | a | c | d |

| 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 | 40 |
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| a | a | d | c | a | c | d | d | d | d | a | d | d | a | b | d | a | c | a | b |

- Q5: bắt gói tin → **b. Network Monitor** (Cain/Netcat không phải công cụ bắt gói).
- Q20 & Q23: đáp án đúng (WSAGetLastError+closesocket+WSACleanup / socket UDP đúng) **không** có trong lựa chọn → chọn **d (Không phương án nào đúng)**.
- Q31–35 đảo chuỗi: `<A>=len`, `<B>=str[i]`, `<C>=len-i-1`, `<D>=c`, `<E>=s`.
- Q36–40 đọc & gửi file: `<A>=!feof(fp)`, `<B>=buff`, `<C>=fp`, `<D>=s`, `<E>=len`.

### Đề 217 (40 câu)
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 |
|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|----|----|----|----|----|
| d | d | b | b | d | b | b | b | a | a | c | d | c | b | b | b | d | b | d | a |

| 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 | 40 |
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| c | c | a | d | d | c | a | c | a | d | d | a | c | a | b | a | d | d | a | b |

- Q8: bắt gói tin → **b. Wireshark**.
- Q24 (socket UDP) & Q25 (connect code): đáp án đúng không có → **d**.
- Q31–35 đọc & gửi file: `!feof(fp)`, `buff`, `fp`, `s`, `len`.
- Q36–40 đảo chuỗi: `len`, `str[i]`, `len-i-1`, `c`, `s`.

### Đề 247 — Giữa kỳ (30 trắc nghiệm)
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|
| d | b | b | d | c | b | a | b | b | d | a | c | d | b | b |

| 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 |
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| b | b | d | a | a | c | a | c | c | a | d | d | d | d | d |

- **Phần 2 (tự luận):** viết `FileClient` mô hình **blocking** → `exams/code/FileClient_blocking.cpp`.

### Đề 132 — Giữa kỳ (30 trắc nghiệm)
| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|----|----|----|----|----|----|
| d | b | c | b | b | b | b | d | a | b | c | d | d | b | b |

| 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 |
|----|----|----|----|----|----|----|----|----|----|----|----|----|----|----|
| a | b | d | a | a | d | c | a | c | c | a | d | d | d | d |

- **Phần 2 (tự luận):** viết `FileClient` mô hình **select** → `exams/code/FileClient_select.cpp`.

---

## 3. Câu hỏi online (trắc nghiệm trên hệ thống)

| Câu hỏi | Đáp án đúng |
|---------|-------------|
| `fork(); fork();` tạo bao nhiêu tiến trình con? | **3** (2² − 1) |
| Giao thức mail client (Outlook) **gửi** email | **SMTP** |
| pthread: `pthread_create` → `pthread_join` → in | **Child thread.** rồi **Main thread.** ⚠ |
| Giá trị socket lớn nhất `select` chờ được (Linux) | **1023** |
| HTTP upload file lớn lên server | **POST** |
| `pollfd` — trường đăng ký mặt nạ sự kiện | **events** ⚠ |
| Khi nào cần xử lý xung đột luồng (đa chọn) | đọc/ghi **biến toàn cục** + **ghi file** |
| Trình duyệt tải nội dung trang web | **HTTP** |
| Nhận định **sai** về đa tiến trình | "Các tiến trình **dùng chung biến toàn cục**" |
| Outlook **nhận** email từ mail server (đa chọn) | **POP3** + **IMAP4** |
| Phục vụ client song song (đa chọn) | `fork()` + **luồng** |
| `if(fork()) n=n+1; else n=n+2;` (n=1) | **n = 2** và **n = 3** |
| pthread truyền `&n`, hàm sửa biến cục bộ | **10** (không đổi biến gốc) |

⚠ **Hai câu đáp án tô sẵn trong PDF bị SAI:**
1. **pthread + join:** Vì `pthread_join` chờ luồng con xong rồi mới in, kết quả đúng là `Child thread.` → `Main thread.` (PDF tô "Main thread. / Child thread." là sai).
2. **pollfd:** trường đăng ký sự kiện cần thăm dò là `.events` (kernel trả về ở `.revents`). PDF tô "cả events và revents không dùng" là sai.

---

## 4. Câu tự luận (code)

| Đề | Yêu cầu | File |
|----|---------|------|
| Giữa kỳ 247 | FileClient — **blocking** | `code/FileClient_blocking.cpp` |
| Giữa kỳ 132 | FileClient — **select** | `code/FileClient_select.cpp` |
| Cuối kỳ 20122-002 | Server xác thực USER/PASS | `code/AuthServer.cpp` |
| Đề 1 (chat) | ChatClient/Server — **blocking** | `code/ChatClient_blocking.cpp`, `code/ChatServer_blocking.cpp` |
| Đề 2–5 (chat) | select / WSAEventSelect / Overlapped Event / Completion Routine | xem ghi chú trong `code/ChatServer_blocking.cpp` |
