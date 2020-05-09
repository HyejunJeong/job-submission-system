#define _GNU_SOURCE
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <fcntl.h>


#include "../../include/constants.h"
#include "../../include/ClientList.h"

static int server_sock_fd;
static struct sockaddr_un domain_sock_addr;
static void create_sock();
static int maxJobs = 10;

void onExitCallBack (void);
void print_usage(char** argv);
static void parse_args(int argc, char** argv);
static void handleConnections();

int main(int argc, char** argv, char** envp) {

    parse_args(argc, argv);

    // registering a call back function on system exit
    // registering a hook on function exit that deletes the .hw4server_control in home dir
    typedef void (*FunctionPtr) (void);
    FunctionPtr functionPtr = &onExitCallBack;
    atexit(functionPtr);

    // set up the unix domain socket
    create_sock();
    // start accepting client, it is just like a network socket
    handleConnections();
    return 0;
}

static void parse_args(int argc, char** argv){
    char c;
    while((c = getopt(argc, argv, "j:")) != -1){
        switch (c) {
            case 'j':
                maxJobs = atoi(optarg);
                break;
            case '?':
                print_usage(argv);
        }
    }
}

static void init_pollfd(struct pollfd fds[]){
    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_sock_fd;
    fds[0].events = POLLIN;
    int count = 1;
    for(LinkedClient* currentClient = clientList.next; currentClient != NULL; currentClient = currentClient->next){
        fds[count].fd = currentClient->element->clientFd;
        fds[count].events = POLLIN;
        count += 1;
    }
}

static void handleClient(int clientFd){
   LinkedClient* client = getClientByFd(clientFd);
   char buffer[BUFFER_SIZE];
   memset(buffer, 0, BUFFER_SIZE);
   recv(clientFd, buffer, BUFFER_SIZE, NULL);
   // dummy handling
   printf("recieve this from client fd %d: %s\n", clientFd, buffer);
}

// client closes its connection
static void closeClient(int clientFd){
    close(clientFd);
    LinkedClient* client = getClientByFd(clientFd);
    removeClient(client);
}

static void handleConnections(){
    // make the server sock none blocking
     fcntl(server_sock_fd, F_SETFL, O_NONBLOCK);
    do {
        int clientFdsNum = getClientListSize();
        struct pollfd fds[clientFdsNum + 1];
        init_pollfd(fds);
        // hangs here until a client socket sends something
        int readyFdNum = poll(fds, clientFdsNum + 1, -1);

        for(int i = 1; i < clientFdsNum + 1; i++){
            // this means this clientfd has sent nothing
            if(fds[i].revents == 0) continue;
            if(fds[i].revents == POLLIN) handleClient(fds[i].fd);
            if(fds[i].revents & (POLLRDHUP | POLLERR | POLLNVAL | POLLRDHUP)) closeClient(fds[i].fd);
        }

        // server fd has a new connection
        if(fds[0].revents != 0){
            do{
                int newClientFd = accept(server_sock_fd, NULL, NULL);
                if(newClientFd < 0){
                    if (errno != EWOULDBLOCK){
                        perror("accept error:");
                        exit(0);
                    }else{
                        break;
                    }
                }
                printf("new client connected with fd %d\n", newClientFd);
                LinkedClient* client = createNewClient(newClientFd);
                insertClient(client);
            }while(true);
        }
    }while(true);
}

void print_usage(char** argv){
    printf("Usage: %s [-t maxjobs]\n", argv[0]);
    exit(0);
}

void onExitCallBack (void){
    printf("deleting the file, please rerun the server\n");
    struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;
    const char* file_path = strcat(homedir, FILE_NAME);
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
