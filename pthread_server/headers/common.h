#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define PORT                    2024
#define MAX_CONNECTION_REQUEST  10
#define BUFFER_SIZE             1024
#define MAX_NAME_LENGTH         32

struct client_data_t {
    int client_fd;
    uint16_t client_port;
    char client_ip[INET_ADDRSTRLEN];
    char client_name[MAX_NAME_LENGTH];
};

struct client_node_t {
    struct client_data_t data;
    struct client_node_t* next;
};

struct chat_t {
    pthread_mutex_t mutex;
    struct client_node_t* head; 
    int client_count;
};

struct pthread_data_t {
    struct chat_t* chat;
    struct client_data_t client_data;
};

enum commands {
    CMD_QUIT,
    CMD_LIST,
    CMD_MESSAGE,
};

enum notify_type {
    LEFT,
    JOIN
};

#endif