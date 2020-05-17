#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../../include/constants.h"

#define DELIM   " \t\r\n\v\f"
#define MAXLINE 32
#define RECV_BUFF_SIZE 20480
#define MYDBG       fprintf(stderr, "MYDBG:%s:%s:%d:\n",__FILE__,__func__,__LINE__)

static void create_sock();
void print_buf(unsigned char *buf, int len);
void print_usage();
int submit(unsigned char buffer[BUFFER_SIZE], char **envp);
void get_cmd_type(char *cmd, unsigned char packet[BUFFER_SIZE], char **envp);

static int sock_fd;
struct sockaddr_un server_addr;

void print_usage(){
    printf("Usage: submit, list, kill\n");
    exit(0);
}

// prepare the job buffer upon submit cmd entered, returns joblen
int submit(unsigned char buffer[BUFFER_SIZE], char **envp) {
    int joblen = 0, argvlen = 0, envplen = strlen(*envp) + 1;
    char **argv = NULL;
    int time, mem, prior, i, argc = 0;
    char cmdline[MAXLINE];

    // clear buffer
    memset(buffer, 0 , BUFFER_SIZE);

    // get the command from the user input
    printf("[submit] >> ");
    if (!fgets(cmdline, sizeof(cmdline), stdin))
        printf("Failed to fgets.\n");
    else {
        if (feof(stdin)) {
            exit(0);
        }

        // prepare argv: tokenize user input, attach null char after each command
        argv = (char **) malloc((strlen(cmdline) + 1) * sizeof(char *));
        if(!argv) {
            perror("malloc failed");
            exit(1);
        }
        char *token = strtok(cmdline, DELIM);
        while (token) {
            size_t length = strlen(token) + 1;
            argv[argc] = malloc(length);
            if(!argv[argc]) {
                perror("malloc failed");
                free(argv);
                exit(1);
            }
            memcpy(argv[argc], token, length);

            argc++;

            argvlen += length;
            token = strtok(NULL, DELIM);
        }
        argv[argc] = NULL;

        char temp[MAXLINE];

        // prompt the user to enter and get maxmem, maxtime, priority
        printf("(byte) max_mem_to_consume=");
        if(!fgets(temp, sizeof(temp)-1, stdin))
            printf("Enter max memory to consume in bytes.\n");
        mem = atoi(temp);
        memset(temp, 0 , MAXLINE);

        printf("(sec) max_time_to_run=");
        if(!fgets(temp, sizeof(temp)-1, stdin))
            printf("Enter max time to run in seconds.\n");
        time = atoi(temp);
        memset(temp, 0 , MAXLINE);

        printf("priority=");
        if(!fgets(temp, sizeof(temp)-1, stdin))
            printf("Enter priority.\n");

        prior = atoi(temp);
        memset(temp, 0 , MAXLINE);

        // copy mem, time, priority, envplen, argvlen, argc, envp, argc in order
        memcpy(buffer, &mem, sizeof(int));
        joblen = sizeof(int);

        memcpy(buffer+joblen, &time, sizeof(int));
        joblen += sizeof(int);

        memcpy(buffer+joblen, &prior, sizeof(int));
        joblen += sizeof(int);

        memcpy(buffer+joblen, &envplen, sizeof(int));
        joblen += sizeof(int);

        memcpy(buffer+joblen, &argvlen, sizeof(int));
        joblen += sizeof(int);

        memcpy(buffer+joblen, &argc, sizeof(int));
        joblen += sizeof(int);

        memcpy(buffer+joblen, *envp, strlen(*envp) + 1);
        joblen += strlen(*envp) + 1;

        for (i = 0; i < argc; i++) {
            memcpy(buffer+joblen, argv[i], strlen(argv[i]) + 1);
            joblen += strlen(argv[i]) + 1;
        }
    }

    // free each argv[i]
    for(i = 0; i < argc; i++)
        free(argv[i]);
    // free argv
    free(argv);

    return joblen;
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

// check if user's cmd matches one of four (submit, list, kill, exit),
// parse cmd_type to 1, 2, 3 respectively
// prepare the packet header
void get_cmd_type(char *cmd, unsigned char packet[BUFFER_SIZE], char **envp) {
    byte cmd_type = 0;
    int msglen = 0, joblen = 0;
    unsigned char job[BUFFER_SIZE];

    // cut the trailing line
    cmd[strlen(cmd)-1] = '\0';

    // clear the packet
    memset(packet, 0, BUFFER_SIZE);

    // ignore empty line
    if(cmd[0] == '\0') {
        return;
    }

    if(!strcmp(cmd, "submit") || !strcmp(cmd, "list")
    || !strcmp(cmd, "kill") || !strcmp(cmd, "exit")) {
        if (strcmp(cmd, "submit") == 0) {
            cmd_type = 1;

            joblen = submit(&job[0], envp);

            msglen += 1 + sizeof(int);  // cmd_type + msglen
            msglen += joblen;   // cmd_type + msglen + job

            // set the 1st byte of the packet to cmd_type, 1
            memcpy(packet, &cmd_type, 1);
            // set the 2nd byte to the msglen
            memcpy(packet + 1, &msglen, sizeof(int));
            // set next bytes to the job
            memcpy(packet + 1 + sizeof(int), job, joblen);
        } else if (strcmp(cmd, "list") == 0) {
            cmd_type = 2;
            msglen = 1;

            // set the 1st byte of the packet to cmd_type, 2
            memcpy(packet, &cmd_type, 1);
        } else if (strcmp(cmd, "kill") == 0) {
            cmd_type = 3;
            int jobpid = 0;
            char temp[MAXLINE];

            msglen = 1 + sizeof(int); // one for cmd_type, another for pid

            // prompt the user to enter jobpid to kill
            printf("jobpid=");
            if(!fgets(temp, sizeof(temp)-1, stdin))
                printf("Enter jobpid to kill.\n");
            jobpid = atoi(temp);

            memcpy(packet, &cmd_type, 1);
            memcpy(packet + 1, &jobpid, sizeof(int));
        } else if (strcmp(cmd, "exit") == 0) {
            exit(0);
        }

        send(sock_fd, packet, msglen, 0);

        char response[RECV_BUFF_SIZE];
        memset(response, 0 , RECV_BUFF_SIZE);

        if(recv(sock_fd, response, RECV_BUFF_SIZE, 0) < 0) {
            perror("receiving data");
            exit(1);
        }
        printf("%s", response);

        return;
    }
    else {
        printf("%s\n", "command not found");
    }
    return;
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

int main(int argc, char** argv, char** envp) {
    FILE* null = fopen("/dev/null", "w");
    fprintf(null, "%d%s", argc, argv[0]);
    char cmdline[MAXLINE];
    unsigned char packet[BUFFER_SIZE];

    // set up the socket
    create_sock();

    while(1) {
        printf(">> ");
        if (!fgets(cmdline, sizeof(cmdline), stdin))
            printf("Failed to fgets.\n");
        else {
            if (feof(stdin)) {
                exit(0);
            }

            // prepare the packet
            get_cmd_type(cmdline, &packet[0], envp);
        }
    }
}

