#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>

#define PORT                2024
#define BUFFER_SIZE         1024
#define MAX_NAME_LENGTH     32

atomic_int keep_working = 1;

void print_time_prefix(void) {
    time_t mytime = time(NULL);
    struct tm *now = localtime(&mytime);

    printf("[%02d:%02d:%02d]    ", now->tm_hour, now->tm_min, now->tm_sec);
}

void* recv_handle(void* arg) {
    int* thread_fd = (int*) arg;
    int fd = *thread_fd;

    free(thread_fd);

    char buffer[BUFFER_SIZE] = {0};
    ssize_t count_of_bytes = 0;
    size_t len = 0;

    while (atomic_load(&keep_working)) {
        count_of_bytes = recv(fd, buffer, BUFFER_SIZE - 1, 0);

        if (count_of_bytes <= 0) {

            if (count_of_bytes == 0) {
                print_time_prefix();
                printf("Server disconnected!\n");
            } else {
                perror("recv_handle: recv");
            }

            atomic_store(&keep_working, 0);

            break;
        }

        buffer[count_of_bytes] = '\0';
        len = strlen(buffer);

        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len--;
        }

        printf("\r\033[2K");
        print_time_prefix();
        printf("%s\n", buffer);
        print_time_prefix(); 
        printf("You:    ");

        fflush(stdout);
    }

    return NULL;
}

int send_name(int fd) {
    char name[MAX_NAME_LENGTH] = {0};

    print_time_prefix();
    printf("Print your name:    ");
        
    fflush(stdout);

    if (!fgets(name, MAX_NAME_LENGTH, stdin)) {
        
        if (feof(stdin)) {
            printf("\n");
            print_time_prefix();
            printf("EOF detected. Disconneting...\n");
        } else {
            perror("main: fgets");
        }

        return -1;
    }

    size_t length = strlen(name);

    if (length > 0 && name[length - 1] == '\n') {
        name[length - 1] = '\0';
        length--;
    }

    ssize_t count_of_bytes = send(fd, name, length + 1, 0);

    if (count_of_bytes <= 0) {
        perror("main: send");

        return -1;
    }

    return 0;
}

int disconnecting_from_server(int fd, char* buffer) {
    
    if (strcmp(buffer, "!quit") == 0) {

        print_time_prefix();
        printf("Disconnecting...\n");

        ssize_t count_of_bytes = send(fd, "!quit", strlen("!quit"), 0);

        if (count_of_bytes <= 0) {
            perror("disconnecting_from_server: send");
        }

        return 0;  
    }

    return -1;
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

    print_time_prefix();
    printf("Connected to server! Type messages or '!quit' to exit or '!list' to get list of clients.\n");

    ssize_t count_of_bytes = 0;
    size_t length = 0;
    char buffer[BUFFER_SIZE] = {0};

    if (send_name(fd) < 0) {
        close(fd);

        return EXIT_FAILURE;
    }

    pthread_t pthread = 0;

    int* thread_fd = malloc(sizeof(int));

    if (!thread_fd) {
        perror("main: malloc");

        close(fd);

        return EXIT_FAILURE;
    }

    *thread_fd = fd;
    
    if (pthread_create(&pthread, NULL, recv_handle, thread_fd) != 0) {
        perror("main: pthread_create");

        close(fd);

        free(thread_fd);

        return EXIT_FAILURE;
    }

    while (atomic_load(&keep_working)) {
        memset(buffer, 0, BUFFER_SIZE);

        print_time_prefix();
        printf("You:    ");
        
        fflush(stdout);

        if (!fgets(buffer, BUFFER_SIZE, stdin)) {
            
            if (feof(stdin)) {
                printf("\n");
                print_time_prefix();
                printf("EOF detected. Disconneting...\n");
            } else {
                perror("main: fgets");
            }

            break;
        }

        length = strlen(buffer);

        if (length > 0 && buffer[length - 1] == '\n') {
            buffer[length - 1] = '\0';
            length--;
        }

        if (length == 0) {
            continue;
        }

        if (disconnecting_from_server(fd, buffer) == 0) {
            break;
        }

        count_of_bytes = send(fd, buffer, length + 1, 0);

        if (count_of_bytes <= 0) {
            perror("main: send");

            break;
        }

    }

    print_time_prefix();
    printf("Shutting down connection...\n");

    atomic_store(&keep_working, 0);

    pthread_join(pthread, NULL);

    shutdown(fd, SHUT_RDWR);

    close(fd);

    return EXIT_SUCCESS;
}