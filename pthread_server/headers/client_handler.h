#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "common.h"

ssize_t get_client_name(struct chat_t* chat, struct client_data_t* c_data);
int client_broadcast(struct chat_t* chat, int sender_fd, char* message, size_t length);
int notify_all_clients(struct chat_t* chat, int sender_fd, char* name, enum notify_type notification);
int send_client_list(struct chat_t* chat, int fd);
enum commands command_handler(struct client_data_t* c_data, char* buffer);
int handle_client_command(struct chat_t* chat, struct client_data_t* c_data, char* buffer, int* client_cycle);
void* client_handle(void* arg);

#endif