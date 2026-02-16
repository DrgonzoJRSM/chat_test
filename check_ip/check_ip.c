#define _POSIX_C_SOURCE 200112L 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

void search_ip(char* hostname, struct addrinfo* hint, struct addrinfo** res) {

// Получаем список структур, которые соответсвуют доменному имени
    int status = getaddrinfo(hostname, NULL, hint, res);

    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));

        *res = NULL;
    } else {
        puts("search completed!");
    }
    
}

void print_ip(char* hostname, struct addrinfo* res) {

    if (res == NULL) {
        printf("No IP addresses found for %s\n", hostname);
     
        return;
    }

    struct addrinfo* p;

// Указатель для хранения адреса структуры, нужен для inet_ntop
    void* addr;

    char ip_addr[INET6_ADDRSTRLEN];
    char* ip_standart;

    int count = 0;

    printf("IP addresses for %s:\n", hostname);

// Проходимся по всему списку
    for (p = res; p != NULL; p = p->ai_next) {  
        
        if (p->ai_family == AF_INET) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in *) p->ai_addr;

            addr = &(ipv4->sin_addr);

            ip_standart = "IPV4";
        } else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6 *) p->ai_addr;

            addr = &(ipv6->sin6_addr);

            ip_standart = "IPV6";
        }
        
        if (inet_ntop(p->ai_family, addr, ip_addr, sizeof(ip_addr)) == NULL) {
            perror("inet_ntop error\n");

            continue;
        }

        printf("    %s: %s\n", ip_standart, ip_addr);

        count++;
    }

    if (count == 0) {
        puts("   No IP addresses found :(");
    }

}

int main(void) {
    char hostname[256];
    char input[256];

// Служит для подключения. Можно сказать, служит фильтром
    struct addrinfo hint;

// Хранит в себе результат - связный список структур, которые мы получим  
    struct addrinfo* res;

    memset(&hint, 0, sizeof(hint));

// IPV4 и IPV6
    hint.ai_family = AF_UNSPEC;
    
// TCP
    hint.ai_socktype = SOCK_STREAM;

    while (1) {
        puts("Enter domain name or print quit for ending:");

        if (!fgets(input, sizeof(input), stdin)) {
            perror("Error reading file!");

            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) {
            continue;
        }

        if (strcmp(input, "quit") == 0) {
            break;
        }

        strncpy(hostname, input, sizeof(hostname) - 1);

        hostname[sizeof(hostname) - 1] = '\0';

        if (res) {
            freeaddrinfo(res);

            res = NULL;
        }

        search_ip(hostname, &hint, &res);

        if (res) {
            print_ip(hostname, res);
        }
        
    }

    if (res) {
        freeaddrinfo(res);
    }

    return 0;
}
