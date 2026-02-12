#include "../headers/chat_room.h"

struct chat_t* chat_init(void) {
    struct chat_t* chat = malloc(sizeof(struct chat_t));

    if (!chat) {
        perror("chat_init: malloc");

        return NULL;
    }

    chat->head = NULL;
    chat->client_count = 0;

    pthread_mutex_init(&chat->mutex, NULL);

    return chat;
}

void chat_free(struct chat_t* chat) {
    pthread_mutex_lock(&chat->mutex);

    struct client_node_t* current = chat->head;
    struct client_node_t* next;

    while(current) {
        next = current->next;

        close(current->data.client_fd);
        
        free(current);

        current = next;
    }

    pthread_mutex_unlock(&chat->mutex);
    pthread_mutex_destroy(&chat->mutex);

    free(chat);
}

void client_add_to_chat(struct chat_t* chat, struct client_data_t* c_data) {
    pthread_mutex_lock(&chat->mutex);

    struct client_node_t* new_node = malloc(sizeof(struct client_node_t));

    if (!new_node) {
        perror("client_add: malloc");

        exit(EXIT_FAILURE);
    }

    new_node->data = *c_data;
    new_node->next = chat->head;

    chat->head = new_node;
    chat->client_count++;

    printf("Client added: %s:%d [fd: %d]\n",
        c_data->client_ip,
        c_data->client_port,
        c_data->client_fd
    );

    pthread_mutex_unlock(&chat->mutex);
}

void client_remove_from_chat(struct chat_t* chat, int client_fd) {
    pthread_mutex_lock(&chat->mutex);

    struct client_node_t* prev = NULL;
    struct client_node_t* current = chat->head;

    while (current) {

        if (current->data.client_fd == client_fd) {

            if (prev == NULL) {
                chat->head = current->next;
            } else {
                prev->next = current->next;
            }

            printf("Removing client: %s:%d [fd: %d]\n",
                current->data.client_ip,
                current->data.client_port,
                current->data.client_fd
            );

            close(current->data.client_fd);
            
            free(current);

            chat->client_count--;

            break;
        }

        prev = current;
        current = current->next;
    }

    pthread_mutex_unlock(&chat->mutex);
}