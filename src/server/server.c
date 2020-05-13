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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>


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
static void reapChild(void);
static void submitJob(LinkedClient* client, Job* job);
static void listJob(LinkedClient* client);
static void killJob(pid_t pid);

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

// as the name implies
static void reapChild(void){
    while(1){
        siginfo_t infop;
        memset(&infop, 0 , sizeof(infop));
        waitid(P_ALL, -1, &infop, WEXITED | WNOHANG);
        if(!infop.si_pid) break;
        LinkedJob* job = getJob(infop.si_pid);
        job->jobStatus = EXITED;
        char buffer[BUFFER_SIZE];
        sprintf(buffer, "Job %d finished\n", job->pid);
        send(job->client->clientFd, buffer, BUFFER_SIZE, NULL);
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

// starting address is the starting address of the job in the recv buffer.
static Job* deserializeJob(void* startingAddr, int sizeOfJob){
    Job* job = calloc(sizeOfJob, 1);

    int maxMemory;
    memcpy((void*) &maxMemory, startingAddr, 4);
    startingAddr += 4;
    job->maxMemory = maxMemory;

    int maxTime;
    memcpy((void*) &maxTime, startingAddr, 4);
    startingAddr += 4;
    job->maxTime = maxTime;

    int priority;
    memcpy((void*) &priority, startingAddr, 4);
    startingAddr += 4;
    job->priority;

    int envpSize;
    memcpy((void*) &envpSize, startingAddr, 4);
    startingAddr += 4;
    job->envpSize = envpSize;

    int argvSize;
    memcpy((void*) &argvSize, startingAddr, 4);
    startingAddr += 4;
    job->argvSize = argvSize;

    int argc;
    memcpy((void*) &argc, startingAddr, 4);
    startingAddr += 4;
    job->argc = argc;

    memcpy((void*) &(job->envp), startingAddr, envpSize);
    startingAddr += envpSize;

    memcpy((void*) ((&(job->envp)) + envpSize), startingAddr, argvSize);
    return job;

}

static void handleClient(int clientFd){
   LinkedClient* client = getClientByFd(clientFd);
   byte firstByte = '\0';
   recv(clientFd, &firstByte, 1, NULL);
   switch(firstByte){
       case SUBMIT_JOB:{
           int msgSize = 0;
           recv(clientFd, (void*) &msgSize, sizeof(int), NULL);
           byte buffer[msgSize];
           Job* job = deserializeJob(buffer, msgSize);
           submitJob(client, job);
           break;
       }
       case LIST_JOB:{
           listJob(client);
           break;
       }
       case KILL_JOB:{
           int jobPid;
           recv(clientFd, (void*) &jobPid, 4, NULL);
           killJob(jobPid);
           break;
       }

       default:{
           send(clientFd, "command not recognize\n", BUFFER_SIZE, NULL);
           break;
       }
   }
}

static void listJob(LinkedClient* client){
    char buffer[20480]; // will not be enough if clients has too many jobs to print but ... yeah.... whatever.

    int count = 0;
    if(client->element->LinkedJob == NULL) sprintf(buffer, "client has no jobs\n");
    else{
        for(LinkedJob* job = client->element->LinkedJob; job != NULL; job = job->next){
            sprintf((char*)&buffer[count], "Job pid: %d, program name: %s\n", job->pid, (char*) ((&(job->element->envp)) + job->element->envpSize));
            int strLen = strlen(buffer[count]);
            count += strLen + 1;
        }
    }
    send(client->element->clientFd, buffer, 20480, NULL);
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

        // reap finished jobs
        reapChild();

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


static void runJob(Job* job){
    char** envp = calloc(sizeof(void*), 46); // as far as i can tell, there are 46 enviorment varibles on linux. There might be more or less but.... yeah.... whatever
    int i = 0;
    int envpSize = 0;
    char* envpPtr = &(job->envp);
    while(envpSize < job->envpSize){
        int bufferSize = strlen(envpPtr) + 1;
        char* string = calloc(bufferSize, 1);
        strcpy(string, envpPtr); // bad idea because of buffer over flow but .... yeah.. whatever
        envp[i] = string;
        i++;
        envpSize += bufferSize;
        envpPtr += bufferSize;
    }

    char** argv = calloc(sizeof(void*), job->argc);
    int j = 0;
    int argvSize = 0;
    char* argvPtr = (&(job->envp)) + job->envpSize;
    while(argvSize < job->argvSize){
        int bufferSize = strlen(argvPtr) + 1;
        char* string = calloc(bufferSize, 1);
        strcpy(string, argvPtr); // bad idea because of buffer over flow but .... yeah.. whatever
        argv[j] = string;
        j++;
        argvSize += bufferSize;
        argvPtr += bufferSize;
    }
    execve(argv[1], argv, envp);
    exit(1); // failed
}

static void submitJob(LinkedClient* client, Job* job){
    LinkedJob* linkedJob = calloc(sizeof(*linkedJob), 1);
    pid_t pid = fork();

    if(pid < 0){
        perror("fork failed");
        exit(0);
    }

    if(pid > 0){
        struct rlimit rl;
        getrlimit(RLIMIT_AS, &rl);

        // set max memeory
        rl.rlim_cur = job->maxMemory;
        setrlimit(RLIMIT_AS, &rl);

        // set the max cpu time
        getrlimit(RLIMIT_CPU, &rl);
        rl.rlim_cur = job->maxTime;
        setrlimit(RLIMIT_CPU, &rl);

        // set the priority
        getrlimit(RLIMIT_NICE, &rl);
        rl.rlim_cur = job->priority;
        setrlimit(RLIMIT_NICE, &rl);

        runJob(job);
    }

    linkedJob->pid = pid;
    linkedJob->client = client;
    linkedJob->jobStatus = RUNNING;
    linkedJob->element = job;
    if(client->element->LinkedJob == NULL){
        client->element->LinkedJob = linkedJob;
        return;
    }

    LinkedJob* lastLinkedJob = NULL;
    for(lastLinkedJob = client->element->LinkedJob; lastLinkedJob->next != NULL; lastLinkedJob = lastLinkedJob->next);
    lastLinkedJob->next = linkedJob;
    send(client->element->clientFd, "Job successfully started\n", BUFFER_SIZE, NULL);
    return;
}

static void killJob(pid_t pid){
    kill(pid, 9);
}
