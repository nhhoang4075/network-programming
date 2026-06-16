#!/usr/bin/env bash
#
# Chạy file kiểm thử chính thức (chat_server_test, ELF Linux x86-64) của thầy
# với chat_server của ta — ngay trên macOS Apple Silicon thông qua Docker.
#
#   ./run_official_test.sh
#
# Trên máy Linux x86-64 thật thì KHÔNG cần Docker, chỉ cần:
#   gcc -O2 -o chat_server chat_server.c
#   ./chat_server 9000 &
#   chmod +x chat_server_test && ./chat_server_test     (Enter để chọn mặc định)
#
set -e
cd "$(dirname "$0")"

TEST_URL="http://lebavui.io.vn/chat_server_test"

# 1) Tải file kiểm thử nếu chưa có
if [ ! -f chat_server_test ]; then
  echo ">> Tải $TEST_URL ..."
  curl -fsSL -o chat_server_test "$TEST_URL"
fi
chmod +x chat_server_test

# 2) Dựng image môi trường test (chỉ lần đầu)
if ! docker image inspect chat-test >/dev/null 2>&1; then
  echo ">> Build image chat-test (lần đầu, vài phút) ..."
  docker build --platform linux/amd64 -t chat-test .
fi

# 3) Biên dịch server trong Linux, chạy, rồi chạy file kiểm thử
echo ">> Chạy kiểm thử ..."
docker run --rm --platform linux/amd64 -v "$PWD":/work -w /work chat-test bash -c '
  set -e
  gcc -O2 -o chat_server_linux chat_server.c
  ./chat_server_linux 9000 &
  SRV=$!
  sleep 1
  printf "127.0.0.1\n9000\n" | ./chat_server_test
  RC=$?
  kill $SRV 2>/dev/null || true
  exit $RC
'
