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

enum commands{
    CMD_QUIT,
    CMD_LIST,
    CMD_MESSAGE,
};

enum notify_type {
    LEFT,
    JOIN
};

struct chat_t* chat_init(void) {
    struct chat_t* chat = malloc(sizeof(struct chat_t));

    if (!chat) {
        perror("chat_init: malloc");

        exit(EXIT_FAILURE);
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

void client_add(struct chat_t* chat, struct client_data_t* c_data) {
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

void client_remove(struct chat_t* chat, int client_fd) {
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

ssize_t get_client_name(struct chat_t* chat, struct client_data_t* c_data) {
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

    struct client_node_t* current = chat->head;
    
    while (current) {
        
        if (current->data.client_fd == c_data->client_fd) {
            strncpy(current->data.client_name, c_data->client_name, MAX_NAME_LENGTH - 1);

            current->data.client_name[MAX_NAME_LENGTH - 1] = '\0';

            break;
        }
    
        current = current->next;
    }

    pthread_mutex_unlock(&chat->mutex);

    return count_of_bytes;
}

int client_broadcast(struct chat_t* chat, int sender_fd, char* message, size_t length) {
    pthread_mutex_lock(&chat->mutex);

    struct client_node_t* current = chat->head;
    ssize_t count_of_bytes = 0;
    
    while (current) {
        
        if (current->data.client_fd != sender_fd) {
            count_of_bytes = send(current->data.client_fd, message, length, 0);

            if (count_of_bytes <= 0) {
                perror("client_broadcast: send");

                pthread_mutex_unlock(&chat->mutex);

                return -1;
            }

        }

        current = current->next;
    }

    pthread_mutex_unlock(&chat->mutex);

    return 0;
}

int notify_all_clients(struct chat_t* chat, int sender_fd, char* name, enum notify_type notification) { 
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

int send_client_list(struct chat_t* chat, int fd) {
    pthread_mutex_lock(&chat->mutex);
    
    struct client_node_t* current = chat->head;
    char list_of_clients[BUFFER_SIZE] = {0};
    int offset = 0;

    offset += snprintf(list_of_clients, BUFFER_SIZE, "Online (%d and YOU): ", chat->client_count - 1);
    
    while (current) {
        
        if (current->data.client_fd != fd) {
         
            int written = snprintf(
                list_of_clients + offset,
                BUFFER_SIZE - offset,
                "<%s>, ",
                current->data.client_name
            );

            if (written > 0) {
                offset += written;
            } else {
                break;
            }
            
        }
    
        current = current->next;
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

enum commands command_handler(struct client_data_t* c_data, char* buffer) {
    
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

int handle_client_command(struct chat_t* chat, struct client_data_t* c_data, char* buffer, int* client_cycle) {
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

void* client_handle(void* arg) {
    struct pthread_data_t* p_data = (struct pthread_data_t*) arg;
    struct chat_t* chat = p_data->chat;
    struct client_data_t c_data = p_data->client_data;

    free(p_data);

    char buffer[BUFFER_SIZE] = {0};

    ssize_t count_of_bytes = get_client_name(chat, &c_data);

    if (count_of_bytes < 0) {
        client_remove(chat, c_data.client_fd);

        return NULL;
    }

    if (notify_all_clients(chat, c_data.client_fd, c_data.client_name, JOIN) < 0) {
        client_remove(chat, c_data.client_fd);

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

        if (handle_client_command(chat, &c_data, buffer, &client_cycle) < 0) {
            client_cycle = 0;
        }

    }

    if (notify_all_clients(chat, c_data.client_fd, c_data.client_name, LEFT) < 0) {
        client_remove(chat, c_data.client_fd);

        return NULL;
    }

    client_remove(chat, c_data.client_fd);

    return NULL;
}

int main(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");

        return EXIT_FAILURE;
    }

    int opt = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");

        close(fd);

        return EXIT_FAILURE;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("bind");

        close(fd);

        return EXIT_FAILURE;
    }

    if (listen(fd, MAX_CONNECTION_REQUEST) < 0) {
        perror("listen");

        close(fd);

        return EXIT_FAILURE;
    }

    struct chat_t* chat = chat_init();
    socklen_t client_len = (socklen_t) sizeof(struct sockaddr_in);

    printf("Server is listening...\n");

    while (1) {
        struct client_data_t c_data = {0};
        struct sockaddr_in client_addr = {0};

        c_data.client_fd = accept(fd, (struct sockaddr*) &client_addr, &client_len);

        if (c_data.client_fd < 0) {
            perror("accept");

            continue;
        }

        c_data.client_port = ntohs(client_addr.sin_port);

        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, c_data.client_ip, INET_ADDRSTRLEN);

        client_add(chat, &c_data);

        struct pthread_data_t* pthread_data = malloc(sizeof(struct pthread_data_t));

        if (!pthread_data) {
            perror("main: malloc");

            return EXIT_FAILURE;
        }

        pthread_data->chat = chat;
        pthread_data->client_data = c_data;

        pthread_t pthread;

        if (pthread_create(&pthread, NULL, client_handle, pthread_data) != 0) {
            perror("main: pthread_create");

            break;
        }
        
        pthread_detach(pthread);

        printf("New connection: %s:%d\n", c_data.client_ip, c_data.client_port);
    }

    chat_free(chat);

    close(fd);

    return EXIT_SUCCESS;
}   
