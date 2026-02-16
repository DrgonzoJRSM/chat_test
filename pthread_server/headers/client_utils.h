#ifndef CLIENT_UTILS_H
#define CLIENT_UTILS_H

#include "common.h"

/**
 * @brief Поиск клиента по дескриптору
 * 
 * @warning Непотокобезопасная, использовать мьютекс
 * 
 * @return struct client_node_t* 
 */
struct client_node_t* search_by_fd(struct chat_t* chat, int fd);

/**
 * @brief Поиск всех клиентов, кроме заданного по дескриптору
 * 
 * Для всех клиентов выполняет callback функцию
 * 
 * @warning Непотокобезопасная, использовать мьютекс
 * 
 * @param exclude_fd Дескриптор, который нужно исключить из поиска
 * @param callback callback функция, которая должна быть применена с данными подходящего клиента
 * @param arg Аргументы для callback функции
 * @return int 0 в случае успеха, -1 при ошибке
 */
int foreach_client_expect(struct chat_t* chat, int exclude_fd, client_callback callback, void* arg);

/**
 * @brief callback, отправляющий клиенту сообщение
 * 
 * @param client Клиент, которому нужно отправить сообщение
 * @param arg Указатель на struct broadcast_callback_data_t
 * @return int 0 в случае успеха, -1 при ошибке 
 */
int broadcast_callback(struct client_node_t* client, void* arg);

/**
 * @brief callback, добавляющий в строку имя клиента
 * 
 * @param client Клиент, чье имя нужно добавить в строку
 * @param arg Указатель на struct client_list_callback_data_t
 * @return int 0 в случае успеха, -1 при ошибке 
 */
int client_list_callback(struct client_node_t* client, void* arg);

#endif