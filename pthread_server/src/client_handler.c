#include "../headers/client_handler.h"
#include "../headers/chat_room.h"

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

typedef int (*client_callback) (struct client_node_t* client, void* arg);

static int foreach_client_expect(struct chat_t* chat, int exclude_fd, client_callback callback, void* arg) {
    struct client_node_t* current = chat->head;
    int result = 0;

    while (current) {
        
        if (current->data.client_fd != exclude_fd) {
            result = callback(current, arg);

            if (result < 0) {
                return result;
            }

        }

        current = current->next;
    }

    return result;
}

struct broadcast_callback_data_t {
    char* message;
    ssize_t length;
};

int broadcast_callback(struct client_node_t* client, void* arg) {
    struct broadcast_callback_data_t* data = (struct broadcast_callback_data_t*) arg;

    ssize_t count_of_bytes = send(client->data.client_fd, data->message, data->length, 0);

    if (count_of_bytes <= 0) {
        perror("client_broadcast: send");

        return -1;
    }

    return 0;
}

struct client_list_callback_data_t {
    int* offset;
    char* list_of_clients;
};

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

/**
 * @brief Добавляет в данные списка имя клиента
 * 
 * По дескриптору клиента ищет совпадение в списке и вставляет в данные имя
 *  
 * @return ssize_t длину имени в случае успеха, -1 при ошибке
 */
static ssize_t get_client_name(struct chat_t* chat, struct client_data_t* c_data) {
    ssize_t count_of_bytes = recv(c_data->client_fd, c_data->client_name, MAX_NAME_LENGTH - 1, 0);

    c_data->client_name[count_of_bytes] = '\0';

    if (count_of_bytes <= 0) {
            
        if (count_of_bytes == 0) {
            printf("Client %s:%d disconnected\n", c_data->client_ip, c_data->client_port);
        } else {
            perror("getting_name: recv");
        }

        return -1;
    }

    printf("Name for %s:%d - <%s>\n", c_data->client_ip, c_data->client_port, c_data->client_name);

    pthread_mutex_lock(&chat->mutex);

    struct client_node_t* current = search_by_fd(chat, c_data->client_fd);

    if (current) {
        strncpy(current->data.client_name, c_data->client_name, MAX_NAME_LENGTH - 1);

        current->data.client_name[MAX_NAME_LENGTH - 1] = '\0';
    } else {
        pthread_mutex_unlock(&chat->mutex);    
        
        return -1;
    }

    pthread_mutex_unlock(&chat->mutex);

    return count_of_bytes;
}

/**
 * @brief Отправка сообщения всем участникам чата
 * 
 * @return int 0 в случае успеха, -1 при ошибке
 */
static int client_broadcast(struct chat_t* chat, int sender_fd, char* message, size_t length) {
    pthread_mutex_lock(&chat->mutex);

    // struct client_node_t* current = chat->head;
    // ssize_t count_of_bytes = 0;
    
    // while (current) {
        
    //     if (current->data.client_fd != sender_fd) {
    //         count_of_bytes = send(current->data.client_fd, message, length, 0);

    //         if (count_of_bytes <= 0) {
    //             perror("client_broadcast: send");

    //             pthread_mutex_unlock(&chat->mutex);

    //             return -1;
    //         }

    //     }

    //     current = current->next;
    // }

    struct broadcast_callback_data_t data = {
        .message = message,
        .length = length
    };

    if (foreach_client_expect(chat, sender_fd, broadcast_callback, &data) < 0) {
        pthread_mutex_unlock(&chat->mutex);        

        return -1;
    }

    pthread_mutex_unlock(&chat->mutex);

    return 0;
}

/**
 * @brief Уведомление всех учатников чата
 * 
 * Либо о присоединении клиента к чату, либоо о выходе клиента из чата
 * 
 * @return int 0 в случае успеха, -1 при ошибке
 */
static int notify_all_clients(struct chat_t* chat, int sender_fd, char* name, enum notify_type notification) { 
    char message[BUFFER_SIZE] = {0};

    switch (notification) {
        case JOIN:
            snprintf(message, BUFFER_SIZE, "<%s> joined the chat!", name);    

            break;
    
        case LEFT:
            snprintf(message, BUFFER_SIZE, "<%s> left the chat!", name);

            break;
    }

    if (client_broadcast(chat, sender_fd, message, strlen(message)) < 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Отправка клиенту списка всех учатников чата 
 * 
 * Собирает список имен и отправляет клиенту
 * 
 * @return int 0 в случае успеха, -1 при ошибке
 */
static int send_client_list(struct chat_t* chat, int fd) {
    pthread_mutex_lock(&chat->mutex);
    
    // struct client_node_t* current = chat->head;
    char list_of_clients[BUFFER_SIZE] = {0};
    int offset = 0;

    offset += snprintf(list_of_clients, BUFFER_SIZE, "Online (%d and YOU): ", chat->client_count - 1);
    
    // while (current) {
        
    //     if (current->data.client_fd != fd) {
         
    //         int written = snprintf(
    //             list_of_clients + offset,
    //             BUFFER_SIZE - offset,
    //             "<%s>, ",
    //             current->data.client_name
    //         );

    //         if (written > 0) {
    //             offset += written;
    //         } else {
    //             break;
    //         }
            
    //     }
    
    //     current = current->next;
    // }

    struct client_list_callback_data_t data = {
        .offset = &offset,
        .list_of_clients = list_of_clients
    };

    if (foreach_client_expect(chat, fd, client_list_callback, &data) < 0) {
        pthread_mutex_unlock(&chat->mutex);

        return -1;
    }

    if (offset > 0 && list_of_clients[offset - 2] == ',') {
        list_of_clients[offset - 2] = '\0';
    }

    pthread_mutex_unlock(&chat->mutex);

    if (offset == 0) {
        strncpy(list_of_clients, "No other clients online", BUFFER_SIZE - 1);

        offset = strlen(list_of_clients);
    }

    ssize_t count_of_bytes = send(fd, list_of_clients, offset, 0);

    if (count_of_bytes <= 0) {
        perror("send_client_list: send");

        return -1;
    }

    return 0;
}

/**
 * @brief Обработчик команд клиента 
 * 
 * @return enum commands CMD_QUIT - клиент хочет выйти из чата, 
 *                       CMD_LIST - клиент хочет список участников чата
 *                       CMD_MESSAGE - клиент отправил обычное сообщение
 */
static enum commands command_handler(struct client_data_t* c_data, char* buffer) {
    
    if (strcmp(buffer, "!quit") == 0) {
        printf("Client %s:%d <%s> requested disconnect\n", c_data->client_ip, c_data->client_port, c_data->client_name);

        return CMD_QUIT;
    }

    if (strcmp(buffer, "!list") == 0) {
        printf("Client %s:%d requested a list of clients\n", c_data->client_ip, c_data->client_port);

        return CMD_LIST;
    }

    return CMD_MESSAGE;
}

/**
 * @brief Выполнение полученной от клиента команды
 * 
 * @return int 0 в случае успеха, -1 при ошибке
 */
static int executing_clients_command(struct chat_t* chat, struct client_data_t* c_data, char* buffer, int* client_cycle) {
    enum commands cmd = command_handler(c_data, buffer);

    char message[BUFFER_SIZE] = {0};

    switch (cmd) {
        case CMD_QUIT:
            *client_cycle = 0;

            return 0;

        case CMD_LIST:
            
            if (send_client_list(chat, c_data->client_fd)) {
                *client_cycle = 0;
            }

            return 0;

        case CMD_MESSAGE:
            printf("Client <%s> %s:%d: %s\n", c_data->client_name, c_data->client_ip, c_data->client_port, buffer);

            snprintf(message, BUFFER_SIZE, "<%s>: %s", c_data->client_name, buffer);
        
            if (client_broadcast(chat, c_data->client_fd, message, strlen(message))) {
                *client_cycle = 0;
            }

            return 0;
        
        default:
            return -1;

    }
    
}

void* clients_handler(void* arg) {
    struct pthread_data_t* p_data = (struct pthread_data_t*) arg;
    struct chat_t* chat = p_data->chat;
    struct client_data_t c_data = p_data->client_data;

    free(p_data);

    char buffer[BUFFER_SIZE] = {0};

    ssize_t count_of_bytes = get_client_name(chat, &c_data);

    if (count_of_bytes < 0) {
        client_remove_from_chat(chat, c_data.client_fd);

        return NULL;
    }

    if (notify_all_clients(chat, c_data.client_fd, c_data.client_name, JOIN) < 0) {
        client_remove_from_chat(chat, c_data.client_fd);

        return NULL;
    }

    int client_cycle = 1;

    while (client_cycle) {
        memset(buffer, 0, BUFFER_SIZE);

        count_of_bytes = recv(c_data.client_fd, buffer, BUFFER_SIZE - 1, 0);

        if (count_of_bytes <= 0) {
            
            if (count_of_bytes == 0) {
                printf("Client %s:%d disconnected\n", c_data.client_ip, c_data.client_port);

                break;
            } else {
                perror("client_handle: recv");
            }

            continue;
        }

        buffer[count_of_bytes] = '\0';

        if (executing_clients_command(chat, &c_data, buffer, &client_cycle) < 0) {
            client_cycle = 0;
        }

    }

    if (notify_all_clients(chat, c_data.client_fd, c_data.client_name, LEFT) < 0) {
        client_remove_from_chat(chat, c_data.client_fd);

        return NULL;
    }

    client_remove_from_chat(chat, c_data.client_fd);

    return NULL;
}
