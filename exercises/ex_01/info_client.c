#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define exf exit(EXIT_FAILURE)

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client < 0) {
        perror("socket() failed");
        exf;
    }

    // Connect to server at given IP and port
    struct sockaddr_in sv_addr;
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    sv_addr.sin_port = htons(9999);

    if (connect(client, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("connect() failed");
        exf;
    }

    printf("Connected to server: %s:%d\n", inet_ntoa(sv_addr.sin_addr), ntohs(sv_addr.sin_port));

    // Get current working directory name
    char buf[4096];
    char cwd[256];
    int offset = 0;
    int count_offset;

    getcwd(cwd, sizeof(cwd));
    printf("%s\n", cwd);
    
    int len = strlen(cwd) + 1;
    memcpy(buf + offset, cwd, len);
    count_offset = len;
    offset += count_offset + sizeof(int);
    
    DIR *d = opendir(cwd);
    struct dirent *entry;
    int f_count = 0;
    char f_name[256], f_path[512];
    struct stat st;

    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_REG) continue;

        f_count++;
        strcpy(f_name, entry->d_name);

        len = strlen(f_name) + 1;
        memcpy(buf + offset, f_name, len);
        offset += len;

        snprintf(f_path, sizeof(f_path), "%s/%s", cwd, f_name);

        stat(f_path, &st);
        memcpy(buf + offset, &st.st_size, sizeof(off_t));
        offset += sizeof(off_t);

        printf("%s - %lld bytes\n", f_name, st.st_size);
    }

    memcpy(buf + count_offset, &f_count, sizeof(int));

    send(client, buf, offset, 0);

    return 0;
}