#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <pwd.h>

#include "../../include/constants.h";
#include "../../include/server.h";

static int server_sock_fd;
static struct sockaddr_un domain_sock_addr;
static void create_sock();



int main() {

    // set up the unix domain socket
    create_sock();
    // start accepting client, it is just like a network socket
    int newClientFd = accept(server_sock_fd, NULL, NULL);
    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(newClientFd, buffer, BUFFER_SIZE, 0);
    printf(buffer);
    return 0;
}


static void accept_client(){
    do {
        int newClientFd = accept(server_sock_fd, NULL, NULL);
    }while(true);
}


static void create_sock(){
    server_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&domain_sock_addr, 0, sizeof(domain_sock_addr));
    domain_sock_addr.sun_family = AF_UNIX;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char* file_path = strcat(homedir, FILE_NAME);
    strcpy(domain_sock_addr.sun_path, file_path);
    bind(server_sock_fd, (struct sockaddr*)&domain_sock_addr, sizeof(domain_sock_addr));
    listen(server_sock_fd, 10);
}
