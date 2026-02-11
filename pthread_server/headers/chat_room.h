#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include "common.h"

struct chat_t* chat_init(void);
void chat_free(struct chat_t* chat);

void client_add_to_chat(struct chat_t* chat, struct client_data_t* c_data);
void client_remove_from_chat(struct chat_t* chat, int client_fd);

#endif