//
// Created by xgao on 5/4/20.
//

#ifndef HW4_CLIENTLIST_H
#define HW4_CLIENTLIST_H

// holds the clientfd and other info, not sure what yet
typedef struct Client{
    int clientFd;
} Client;

// linked list structure for client
typedef struct LinkedClient {
    struct LinkedClient* next;
    struct LinkedClient* prev;
    Client* element;
} LinkedClient;

// create a new LinkedList client from file descriptor
LinkedClient* createNewClient(int fd);

// insert a new LinkedClient
void insertClient(LinkedClient* linkedClient);

// removed a LinkedClient
void removeClient(LinkedClient* linkedClient);

// get a LinkedClient by index
LinkedClient* getClientByIndex(int index);

// get linked list size
int getClientListSize(void);

// the root node of the LinkedClient
extern LinkedClient clientList;

#endif //HW4_CLIENTLIST_H
