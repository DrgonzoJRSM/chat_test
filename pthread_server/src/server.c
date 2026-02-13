#include "../headers/common.h"
#include "../headers/chat_room.h"
#include "../headers/client_handler.h"
#include "../headers/time_prefix.h"

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

    print_time_prefix();
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

        struct pthread_data_t* pthread_data = malloc(sizeof(struct pthread_data_t));

        if (!pthread_data) {
            perror("main: malloc");

            return EXIT_FAILURE;
        }

        pthread_data->chat = chat;
        pthread_data->client_data = c_data;

        pthread_t pthread;

        if (pthread_create(&pthread, NULL, clients_handler, pthread_data) != 0) {
            perror("main: pthread_create");

            break;
        }
        
        pthread_detach(pthread);

        print_time_prefix();
        printf("New connection: %s:%d\n", c_data.client_ip, c_data.client_port);
    }

    chat_free(chat);

    close(fd);

    return EXIT_SUCCESS;
}   
