#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define PORT                2024
#define BUFFER_SIZE         1024
#define MAX_NAME_LENGHT     32

void* recv_handle(void* arg) {
    int* fd = (int*) arg;
    char buffer[BUFFER_SIZE] = {0};
    ssize_t count_of_bytes = 0;
    size_t len = 0;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        count_of_bytes = recv(*fd, buffer, BUFFER_SIZE - 1, 0);

        if (count_of_bytes <= 0) {

            if (count_of_bytes == 0) {
                printf("\nServer disconnected!\n");
            } else {
                perror("recv_handle: recv");
            }

            break;
        }

        len = strlen(buffer);
        
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        
        printf("\r\33[2K%s\n", buffer); 
        printf("You:    ");

        fflush(stdout);
    }

    return NULL;
}

void send_name(int fd) {
    char name[MAX_NAME_LENGHT] = {0};

    printf("Print your name:    ");
        
    fflush(stdout);

    if (!fgets(name, MAX_NAME_LENGHT, stdin)) {
        
        if (feof(stdin)) {
            printf("\nEOF detected. Disconneting...\n");
        } else {
            perror("main: fgets");
        }

        exit(EXIT_FAILURE);
    }

    size_t lenght = strlen(name);

    if (lenght > 0 && name[lenght - 1] == '\n') {
        name[lenght - 1] = '\0';
        lenght--;
    }

    ssize_t count_of_bytes = send(fd, name, lenght + 1, 0);

    if (count_of_bytes <= 0) {
        perror("main: send");

        exit(EXIT_FAILURE);
    }

}

int disconnecting_from_server(int fd, char* buffer) {
    
    if (strcmp(buffer, "!quit") == 0) {
        printf("Disconnecting...\n");

        ssize_t count_of_bytes = send(fd, "!quit", 5, 0);

        if (count_of_bytes <= 0) {
            perror("disconnecting_from_server: send");
        }

        return 1;  
    }

    return 0;
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ip>\n", argv[0]);   

        return EXIT_FAILURE;
    } 

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("main: socket");

        return EXIT_FAILURE;
    } 

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1],  &addr.sin_addr.s_addr) <= 0) {
        fprintf(stderr, "Incorrect address: %s\n", argv[1]);

        close(fd);

        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("main: connect");

        close(fd);

        return EXIT_FAILURE;
    }

    printf("Connected to server! Type messages or '!quit' to exit or '!list' to get list of clients.\n");

    pthread_t pthread = 0;
    
    if (pthread_create(&pthread, NULL, recv_handle, &fd) != 0) {
        perror("main: pthread_create");

        close(fd);

        return EXIT_FAILURE;
    }

    pthread_detach(pthread);

    ssize_t count_of_bytes = 0;
    size_t lenght = 0;
    char buffer[BUFFER_SIZE] = {0};

    send_name(fd);

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);

        printf("You:    ");
        
        fflush(stdout);

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            
            if (feof(stdin)) {
                printf("\nEOF detected. Disconneting...\n");
            } else {
                perror("main: fgets");
            }

            break;
        }

        lenght = strlen(buffer);

        if (lenght > 0 && buffer[lenght - 1] == '\n') {
            buffer[lenght - 1] = '\0';
            lenght--;
        }

        if (lenght == 0) {
            continue;
        }

        if (disconnecting_from_server(fd, buffer)) {
            break;
        }

        count_of_bytes = send(fd, buffer, lenght + 1, 0);

        if (count_of_bytes <= 0) {
            perror("main: send");

            break;
        }

    }

    printf("Shutting down connection...\n ");

    shutdown(fd, SHUT_RDWR);

    close(fd);

    return EXIT_SUCCESS;
}