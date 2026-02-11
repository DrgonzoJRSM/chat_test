CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lpthread

SERVER_TARGET = server
CLIENT_TARGET = client

SERVER_SRC_DIR = pthread_server/src
SERVER_HEADER_DIR = pthread_server/headers
CLIENT_SRC_DIR = pthread_client

OBJ_DIR = obj

SERVER_SOURCES = $(wildcard $(SERVER_SRC_DIR)/*.c)
CLIENT_SOURCES = $(wildcard $(CLIENT_SRC_DIR)/*.c)

SERVER_OBJ = $(SERVER_SOURCES:$(SERVER_SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJ = $(CLIENT_SOURCES:$(CLIENT_SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) $(LDFLAGS) -o $@

$(CLIENT_TARGET): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) $(LDFLAGS) -o $@

$(OBJ_DIR)/%.o: $(SERVER_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(SERVER_HEADER_DIR) -c $< -o $@

$(OBJ_DIR)/%.o: $(CLIENT_SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(SERVER_HEADER_DIR) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(SERVER_TARGET) $(CLIENT_TARGET) $(OBJ_DIR)