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
#include <sys/resource.h>
#include <signal.h>

#include "../../include/constants.h"
#include "../../include/ClientList.h"

static int server_sock_fd;
static struct sockaddr_un domain_sock_addr;
static void create_sock();
static int maxJobs = 10;
static int currentJob = 0;

void onExitCallBack (void);
void print_usage(char** argv);
static void parse_args(int argc, char** argv);
static void handleConnections();
static void reapChild(void);
static void submitJob(LinkedClient* client, Job* job);
static void listJob(LinkedClient* client);
static void killJob(int clientFd, pid_t pid);
void print_job(Job* job);

int main(int argc, char** argv) {

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
        if(job == NULL) {
            currentJob -= 1;
            continue;
        }
        job->jobStatus = EXITED;
        currentJob -= 1;
    }
}

static void init_pollfd(struct pollfd fds[], int length){
    memset(fds, 0, length);
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
    job->priority = priority;

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

    void* argvAddr = &job->envp;
    argvAddr += envpSize;
    memcpy(argvAddr, startingAddr, argvSize);
//    print_job(job);
    return job;

}

void print_job(Job* job){
    printf("maxMem: %d, maxTime: %d, maxPriority: %d, envpSize: %d, argvSize: %d, argc: %d\n", job->maxMemory, job->maxTime, job->priority, job->envpSize, job->argvSize, job->argc);

    int i = 0;
    int envpSize = 0;
    char* envpPtr = (char*) &(job->envp);

    while(envpSize < job->envpSize){
        char* debug = (char*) envpPtr;
        int bufferSize = strlen(debug) + 1;
        printf("envp[%d]: %s, size: %d\n", i, debug, bufferSize);
        envpSize += bufferSize;
        envpPtr += bufferSize;
        i++;
    }

    int j = 0;
    int argvSize = 0;
    char* argvPtr = (char*) &(job->envp);
    argvPtr += job->envpSize;
    while(argvSize < job->argvSize){
        char* debug = (char*) argvPtr;
        int bufferSize = strlen(debug) + 1;
        printf("argv[%d]: %s, size: %d\n", j, debug, bufferSize);
        j++;
        argvSize += bufferSize;
        argvPtr += bufferSize;
    }
}

void print_buf(unsigned char *buf, int len) {
    int i;
    unsigned char c;
    for(i = 0; i < len; i ++) {
        c = buf[i];

        if(c == '\0')
            printf("\\0");
        else
            printf("%x", buf[i]);
    }
    printf("\n");
}

static void handleClient(int clientFd){
   LinkedClient* client = getClientByFd(clientFd);
   byte firstByte = '\0';
   recv(clientFd, &firstByte, 1, 0);
   switch(firstByte){
       case SUBMIT_JOB:{
           int msgSize = 0;
           recv(clientFd, (void*) &msgSize, sizeof(int), 0);
           byte buffer[msgSize];
           recv(clientFd, (void*) buffer, msgSize, 0);
           if(currentJob > maxJobs) {
               send(clientFd, "Max jobs reached, please try again later\n", BUFFER_SIZE, 0);
               return;
           }
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
           recv(clientFd, (void*) &jobPid, 4, 0);
           killJob(clientFd, jobPid);
           break;
       }

       default:{
           send(clientFd, "command not recognized\n", BUFFER_SIZE, 0);
           break;
       }
   }
}

static void listJob(LinkedClient* client){
    char buffer[20480]; // will not be enough if clients has too many jobs to print but ... yeah.... whatever.
    memset(buffer, 0 , 20480);

    int count = 0;
    if(client->element->LinkedJob == NULL) {
        sprintf(buffer, "client has no jobs\n");
    }
    else{
        for(LinkedJob* job = client->element->LinkedJob; job != NULL; job = job->next){
            char* programName = (char*)&job->element->envp;
            programName += job->element->envpSize;
            char status[10];
            char* jobStdOut = (job->clientStdOut == NULL) ? calloc(2048, 1) : job->clientStdOut;
            if(job->jobStatus == KILLED) {
                strcpy(status, "Killed");
                if(job->clientStdOut == NULL){
                    strcpy(jobStdOut, "N/A");
                    job->clientStdOut = jobStdOut;
                }
            }else if(job->jobStatus == RUNNING){
                strcpy(status, "running");
                if(job->clientStdOut == NULL){
                    strcpy(jobStdOut, "N/A");
                    job->clientStdOut = jobStdOut;
                }
            }else if(job->jobStatus == EXITED) {
                if(job->clientStdOut == NULL){
                    if(read(job->pipe[0], jobStdOut, 2048) < 0) {
                        perror("read error");
                        exit(0);
                    }
                    job->clientStdOut = jobStdOut;
                }
                strcpy(status, "exited");
            }
            sprintf((char*)&buffer[count], "Job pid: %d, program name: %s, status: %s\nstdout after exited:\n %s\n", job->pid, programName, status, jobStdOut);
            int strLen = strlen(&buffer[count]);
            count += strLen;
        }
    }

    if(count > 0)
        buffer[count + 1] = '\0';
    send(client->element->clientFd, buffer, 20480, 0);
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
        init_pollfd(fds, clientFdsNum + 1);
        // hangs here until a client socket sends something
        poll(fds, clientFdsNum + 1, -1);

        // reap finished jobs
        reapChild();

        for(int i = 1; i < clientFdsNum + 1; i++){
            // this means this clientfd has sent nothing
            if(fds[i].revents == 0) continue;
            if(fds[i].revents == POLLIN) handleClient(fds[i].fd);
            if(fds[i].revents & (POLLHUP | POLLERR | POLLNVAL /*| POLLRDHUP*/)) closeClient(fds[i].fd);
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
    char *homedir = pw->pw_dir;
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
    char** envp = calloc(sizeof(void*), 10); // as far as i can tell, there are 46 enviorment varibles on linux. There might be more or less but.... yeah.... whatever
    int i = 0;
    int envpSize = 0;
    char* envpPtr = (char*) &(job->envp);
    while(envpSize < job->envpSize){
        int bufferSize = strlen(envpPtr) + 1;
        char* string = calloc(bufferSize, 1);
        strcpy(string, envpPtr); // bad idea because of buffer over flow but .... yeah.. whatever
        envp[i] = string;
        i++;
        envpSize += bufferSize;
        envpPtr += bufferSize;
    }

    char** argv = calloc(sizeof(void*), job->argc + 1);
    int j = 0;
    int argvSize = 0;
    char* argvPtr = (char*) &job->envp;
    argvPtr += job->envpSize;
    while(argvSize < job->argvSize){
        int bufferSize = strlen(argvPtr) + 1;
        char* string = calloc(bufferSize, 1);
        strcpy(string, argvPtr); // bad idea because of buffer over flow but .... yeah.. whatever
        argv[j] = string;
        j++;
        argvSize += bufferSize;
        argvPtr += bufferSize;
    }

//    printf("-------------------------- Executing new Job --------------------------\n");
//    for(int i = 0; envp[i] != NULL; i++){
//        printf("envp[%d]: %s\n", i, envp[i]);
//    }
//
//    for(int i = 0; argv[i] != NULL; i++){
//        printf("argv[%d]: %s\n", i, argv[i]);
//    }

    execve(argv[0], argv, envp);
    perror("execvp failed");
    abort();
}

static void submitJob(LinkedClient* client, Job* job){
    LinkedJob* linkedJob = calloc(sizeof(*linkedJob), 1);
    currentJob += 1;
    int pipefd[2];
    if(pipe(pipefd) < 0) {
        perror("pipe failed");
        exit(0);
    }

    pid_t pid = fork();

    if(pid < 0){
        perror("fork failed");
        exit(0);
    }

    if(pid == 0){
        // creating pipe
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
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
        setpriority(PRIO_PROCESS, getpid(), job->priority);

        runJob(job);
    }
    linkedJob->pid = pid;
    linkedJob->client = client->element;
    linkedJob->jobStatus = RUNNING;
    linkedJob->element = job;
    linkedJob->pipe[0] = pipefd[0];
    linkedJob->pipe[1] = pipefd[1];
    close(pipefd[1]);
    if(client->element->LinkedJob == NULL){
        client->element->LinkedJob = linkedJob;
    }else{
        LinkedJob* lastLinkedJob = NULL;
        for(lastLinkedJob = client->element->LinkedJob; lastLinkedJob->next != NULL; lastLinkedJob = lastLinkedJob->next);
        lastLinkedJob->next = linkedJob;
    }


    send(client->element->clientFd, "Job successfully started\n", BUFFER_SIZE, 0);
    return;
}

static void killJob(int clientfd, pid_t pid){
    LinkedJob * job = getJob(pid);
    if(job == NULL) {
        send(clientfd, "No such pid found\n", BUFFER_SIZE, 0);
        return;
    }
    kill(pid, 9);
    job->jobStatus = KILLED;
    send(clientfd, "pid killed\n", BUFFER_SIZE, 0);
}
