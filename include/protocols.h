//
// Created by xgao on 5/9/20.
//

#ifndef HW4_PROTOCOLS_H
#define HW4_PROTOCOLS_H

#include "constants.h"

typedef enum CommandType {
        SUBMIT_JOB = 1,
        LIST_JOB = 2,
        KILL_JOB = 3
} CommandType;

// you will need to malloc this struct to size of struct + envp size + the argv size to accommodate this
typedef struct Job{
    int maxMemory;
    int maxTime;
    int priority;
    int envpSize; // use to extract envp
    int argvSize; // use to extract argv
    int argc; // argc of the job
    byte envp[0]; // envp of the job
    byte argv[0]; // argv of the job
} Job;

// like job will need to allocate size of the Packet + size of the Job
typedef struct Packet{
    byte commandType;
    int msgSize; // This is the job struct size
    Job job;
} Packet;

#endif //HW4_PROTOCOLS_H
