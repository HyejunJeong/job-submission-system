#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>

#include "../../include/constants.h"
#include "../../include/ClientList.h"

static int server_sock_fd;
static struct sockaddr_un domain_sock_addr;
static void create_sock();


void onExitCallBack (void);

int main() {

    // registering a call back function on system exit
    // registering a hook on function exit that deletes the .hw4server_control in home dir
    typedef void (*FunctionPtr) (void);
    FunctionPtr functionPtr = &onExitCallBack;
    atexit(functionPtr);

    // set up the unix domain socket
    create_sock();
    // start accepting client, it is just like a network socket
    int newClientFd;
    if((newClientFd = accept(server_sock_fd, NULL, NULL)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int bytes_recieved = recv(newClientFd, buffer, BUFFER_SIZE, 0);
    printf("%s\n", buffer);
    return 0;
}

static void accept_client(){
    do {
        int newClientFd = accept(server_sock_fd, NULL, NULL);
    }while(true);
}

void onExitCallBack (void){
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char* file_path = strcat((char *)homedir, FILE_NAME);
    // unlock the domain socket. This will create a process lock otherwise
    unlink(file_path);
}

static void create_sock(){
    server_sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    memset(&domain_sock_addr, 0, sizeof(domain_sock_addr));
    domain_sock_addr.sun_family = AF_UNIX;
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    char* file_path = strcat((char *) homedir, FILE_NAME);
    strcpy(domain_sock_addr.sun_path, file_path);
    if (bind(server_sock_fd, (struct sockaddr*)&domain_sock_addr, sizeof(domain_sock_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_sock_fd, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
}
