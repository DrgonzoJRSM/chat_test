#include "../headers/client_utils.h"

struct client_node_t* search_by_fd(struct chat_t* chat, int fd) {
    struct client_node_t* current = chat->head;

    while (current) {
        
        if (current->data.client_fd == fd) {
            return current;
        }

        current = current->next;
    }

    return NULL;
}

int foreach_client_expect(struct chat_t* chat, int exclude_fd, client_callback callback, void* arg) {
    struct client_node_t* current = chat->head;
    int result = 0;

    while (current) {
        
        if (current->data.client_fd != exclude_fd) {
            result = callback(current, arg);

            if (result < 0) {
                return -1;
            }

        }

        current = current->next;
    }

    return result;
}

int broadcast_callback(struct client_node_t* client, void* arg) {
    struct broadcast_callback_data_t* data = (struct broadcast_callback_data_t*) arg;

    ssize_t count_of_bytes = send(client->data.client_fd, data->message, data->length, 0);

    if (count_of_bytes <= 0) {
        perror("client_broadcast: send");

        return -1;
    }

    return 0;
}

int client_list_callback(struct client_node_t* client, void* arg) {
    struct client_list_callback_data_t* data = (struct client_list_callback_data_t*) arg;

    int offset = *(data->offset);
    
    int written = snprintf(
        data->list_of_clients + offset, 
        BUFFER_SIZE - offset, 
        "<%s>, ", 
        client->data.client_name
    );

    if (written > 0) {
        *(data->offset) = offset + written;
    } else {
        return -1;
    }

    return 0;
}

