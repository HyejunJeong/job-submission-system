//
// Created by xgao on 5/4/20.
//
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>

#include "../../include/constants.h"

static void create_sock();
static int sock_fd;
struct sockaddr_un server_addr;
void onExitCallBack (int status, void* arg);

int main() {

    // set up the socket
    create_sock();
    char buffer[BUFFER_SIZE];
    strcpy(buffer, "testing");
    int sent_bytes = send(sock_fd, buffer, BUFFER_SIZE, 0);
    return 0;
}

static void create_sock(){
    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char* file_path = strcat((char *)homedir, FILE_NAME);
    strcpy(server_addr.sun_path, file_path);
    connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
}




