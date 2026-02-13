#ifndef CHAT_ROOM_H
#define CHAT_ROOM_H

#include "common.h"

/**
 * @brief Инициализация структуры чата
 */
struct chat_t* chat_init(void);

/**
 * @brief Очистка всех ресурсов
 * 
 */
void chat_free(struct chat_t* chat);

/**
 * @brief Добавление нового клиента в список
 *
 * Выделяет память под элемент списка и ставит в начале списка
 *  
 * @return int 0 в случае успеха, -1 при ошибке 
 */
int client_add_to_chat(struct chat_t* chat, struct client_data_t* c_data);

/**
 * @brief Удаление клиента из списка по его дескриптору
 * 
 */
void client_remove_from_chat(struct chat_t* chat, int client_fd);

#endif