#ifndef COMMON_H
#define COMMON_H

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

/**
 * @brief Данные клиента
 */
struct client_data_t {
    int client_fd;                          ///< Дескриптор файла, используется для поиска нужного клиента
    uint16_t client_port;                   ///< Порт клиента
    char client_ip[INET_ADDRSTRLEN];        ///< IP клиента
    char client_name[MAX_NAME_LENGTH];      ///< Имя клиента
};

/**
 * @brief Элемент списка клиентов
 */
struct client_node_t {
    struct client_data_t data;              ///< Данные клиента
    struct client_node_t* next;             ///< Указатель на следующий элемент списка
};

/**
 * @brief Данные чата клиентов
 */
struct chat_t {
    pthread_mutex_t mutex;                  ///< Общий мьютекс
    struct client_node_t* head;             ///< Указатель на первый элемент списка клиентов
    int client_count;                       ///< Количество клиентов
};

/**
 * @brief Данные для передачи в поток клиента
 */
struct pthread_data_t {
    struct chat_t* chat;                    ///< Указатель на данные чата клиентов
    struct client_data_t client_data;       ///< Локаьная копия данных клиента 
};

/**
 * @brief Список команд клиента
 */
enum commands {
    CMD_QUIT,                               ///< Инициализация выхода из чата
    CMD_LIST,                               ///< Запрос списка клиентов
    CMD_MESSAGE,                            ///< Отправка сообщения
};

/**
 * @brief Список типов сообщений для всех клиентов
 */
enum notify_type {
    LEFT,                                   ///< Уведомление о покидании чата
    JOIN                                    ///< Уведомление о присоединениии к чату
};

#endif