#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "common.h"

/**
 * @brief Обработчик клиента чата
 * 
 * Для каждого клиента выделяется свой поток
 * Полностью управляет работой сервера с одним клиентом
 *  
 */
void* clients_handler(void* arg);

#endif