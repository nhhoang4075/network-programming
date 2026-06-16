# Môi trường Linux x86-64 (glibc) để chạy file kiểm thử chat_server_test
# của thầy trên máy macOS Apple Silicon (qua Docker + Rosetta/qemu).
#
#   docker build --platform linux/amd64 -t chat-test .
#
# Xem run_official_test.sh để chạy tự động.
FROM --platform=linux/amd64 debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends gcc libc6-dev \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
