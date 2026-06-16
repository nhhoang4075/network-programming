# Makefile - Chat Server / Client (Our chat protocol)
CC      = gcc
CFLAGS  = -O2 -Wall -Wextra

all: chat_server chat_client

chat_server: chat_server.c
	$(CC) $(CFLAGS) -o $@ $<

chat_client: chat_client.c
	$(CC) $(CFLAGS) -o $@ $<

# Chạy file kiểm thử chính thức của thầy.
#  - Trên Linux x86-64: build server rồi chạy trực tiếp file test.
#  - Trên macOS/khác:    dùng Docker (xem run_official_test.sh).
test: chat_server
	@if [ "$$(uname -s)-$$(uname -m)" = "Linux-x86_64" ]; then \
		chmod +x chat_server_test; \
		./chat_server 9000 & SRV=$$!; sleep 1; \
		printf "127.0.0.1\n9000\n" | ./chat_server_test; \
		kill $$SRV 2>/dev/null; \
	else \
		./run_official_test.sh; \
	fi

clean:
	rm -f chat_server chat_client chat_server_linux *.o

.PHONY: all test clean
