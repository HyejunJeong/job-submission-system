//
// Created by xgao on 5/4/20.
//

#ifndef HW4_SERVER_H
#define HW4_SERVER_H

typedef struct Client{
    int clientFd;
} Client;

typedef struct LinkedClient {
    struct LinkedClient* next;
    struct LinkedClient* prev;
    Client* element;
} LinkedClient;

#endif //HW4_SERVER_H
