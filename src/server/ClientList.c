//
// Created by xgao on 5/5/20.
//
#include <stddef.h>
#include <stdlib.h>
#include "../../include/ClientList.h"

static int size = 0;
LinkedClient clientList = {NULL, NULL, NULL};

void insertClient(LinkedClient* linkedClient){
    if (clientList.next == NULL){
        clientList.next = linkedClient;
        linkedClient->prev = &clientList;
        size += 1;
        return;
    }

    LinkedClient* lastClient = getClientByIndex(size);
    lastClient->next = linkedClient;
    linkedClient->prev = lastClient;
    size += 1;
    return;
}

int getClientListSize(void){
    return size;
}

void removeClient(LinkedClient* linkedClient){
    LinkedClient* currentClient = clientList.next;
    while (currentClient != NULL){
        if(currentClient == linkedClient) break;
        currentClient = currentClient->next;
    }

    if(currentClient == NULL) return;

    if(currentClient->prev != NULL) currentClient->prev->next = currentClient->next;
    if(currentClient->next != NULL) currentClient->next->prev = currentClient->prev;
    free(currentClient->element);
    free(currentClient);
}

LinkedClient* getClientByIndex(int index){
    LinkedClient* retval = NULL;
    if(size == 0) return retval;
    if(index > size) return retval;
    if(index < 1) return retval;
    retval = clientList.next;
    for(int i = index; i >= 1; i--){
        retval = retval->next;
    }
    return retval;
}

LinkedClient* createNewClient(int fd){
    Client* client = calloc(sizeof(*client), 1);
    LinkedClient* linkedClient = calloc(sizeof(*linkedClient), 1);
    client->clientFd = fd;
    linkedClient->element = client;
    return linkedClient;
}

LinkedClient* getClientByFd(int fd){
    LinkedClient* client = clientList.next;
    while(client != NULL){
        if(client->element->clientFd == fd) return client;
        client = client->next;
    }
    return NULL;
}
